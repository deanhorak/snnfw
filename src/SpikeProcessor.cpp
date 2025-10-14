#include "snnfw/SpikeProcessor.h"
#include "snnfw/Logger.h"
#include <chrono>
#include <algorithm>

namespace snnfw {

SpikeProcessor::SpikeProcessor(size_t timeSliceCount, size_t deliveryThreads)
    : numTimeSlices(timeSliceCount),
      numDeliveryThreads(deliveryThreads),
      timeStep(1.0),
      running(false),
      stopRequested(false),
      currentTime(0.0),
      currentSliceIndex(0) {
    
    // Initialize event queue with empty vectors for each time slice
    eventQueue.resize(numTimeSlices);
    
    // Create thread pool for spike delivery
    threadPool = std::make_unique<ThreadPool>(numDeliveryThreads);
    
    SNNFW_INFO("SpikeProcessor created: {} time slices, {} delivery threads", 
               numTimeSlices, numDeliveryThreads);
}

SpikeProcessor::~SpikeProcessor() {
    stop();
}

void SpikeProcessor::start() {
    if (running.load()) {
        SNNFW_WARN("SpikeProcessor already running");
        return;
    }

    stopRequested.store(false);
    running.store(true);
    
    // Start the background processing thread
    processingThread = std::thread(&SpikeProcessor::processingLoop, this);
    
    SNNFW_INFO("SpikeProcessor started");
}

void SpikeProcessor::stop() {
    if (!running.load()) {
        return;
    }

    SNNFW_INFO("SpikeProcessor stopping...");
    
    stopRequested.store(true);
    cv.notify_all();
    
    if (processingThread.joinable()) {
        processingThread.join();
    }
    
    running.store(false);
    
    SNNFW_INFO("SpikeProcessor stopped. Final time: {:.3f}ms", currentTime.load());
}

bool SpikeProcessor::scheduleSpike(const std::shared_ptr<ActionPotential>& actionPotential) {
    if (!actionPotential) {
        SNNFW_ERROR("SpikeProcessor: Cannot schedule null action potential");
        return false;
    }

    int sliceIndex = getTimeSliceIndex(actionPotential->getScheduledTime());
    
    if (sliceIndex < 0) {
        SNNFW_WARN("SpikeProcessor: Spike scheduled for time {:.3f}ms is out of range (current: {:.3f}ms, max: {:.3f}ms)",
                   actionPotential->getScheduledTime(),
                   currentTime.load(),
                   currentTime.load() + numTimeSlices * timeStep);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        eventQueue[sliceIndex].push_back(actionPotential);
    }

    SNNFW_TRACE("SpikeProcessor: Scheduled spike for time {:.3f}ms (slice {})",
                actionPotential->getScheduledTime(), sliceIndex);
    
    return true;
}

void SpikeProcessor::registerDendrite(const std::shared_ptr<Dendrite>& dendrite) {
    if (!dendrite) {
        SNNFW_ERROR("SpikeProcessor: Cannot register null dendrite");
        return;
    }

    std::lock_guard<std::mutex> lock(registryMutex);
    dendriteRegistry[dendrite->getId()] = dendrite;
    
    SNNFW_DEBUG("SpikeProcessor: Registered dendrite {} (total: {})",
                dendrite->getId(), dendriteRegistry.size());
}

void SpikeProcessor::unregisterDendrite(uint64_t dendriteId) {
    std::lock_guard<std::mutex> lock(registryMutex);
    auto it = dendriteRegistry.find(dendriteId);
    
    if (it != dendriteRegistry.end()) {
        dendriteRegistry.erase(it);
        SNNFW_DEBUG("SpikeProcessor: Unregistered dendrite {} (remaining: {})",
                    dendriteId, dendriteRegistry.size());
    } else {
        SNNFW_WARN("SpikeProcessor: Dendrite {} not found for unregistration", dendriteId);
    }
}

size_t SpikeProcessor::getPendingSpikeCount() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    size_t total = 0;
    for (const auto& slice : eventQueue) {
        total += slice.size();
    }
    return total;
}

size_t SpikeProcessor::getSpikeCountAtSlice(size_t timeSliceIndex) const {
    if (timeSliceIndex >= numTimeSlices) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(queueMutex);
    return eventQueue[timeSliceIndex].size();
}

int SpikeProcessor::getTimeSliceIndex(double timeMs) const {
    double currentTimeMs = currentTime.load();
    
    // Check if time is in the past
    if (timeMs < currentTimeMs) {
        return -1;
    }
    
    // Calculate relative time from current time
    double relativeTime = timeMs - currentTimeMs;
    
    // Calculate slice index
    size_t relativeSliceIndex = static_cast<size_t>(relativeTime / timeStep);
    
    // Check if it's within our buffer
    if (relativeSliceIndex >= numTimeSlices) {
        return -1;
    }
    
    // Calculate absolute slice index (with wraparound)
    size_t absoluteSliceIndex = (currentSliceIndex + relativeSliceIndex) % numTimeSlices;
    
    return static_cast<int>(absoluteSliceIndex);
}

void SpikeProcessor::processingLoop() {
    SNNFW_INFO("SpikeProcessor: Processing loop started");

    while (!stopRequested.load()) {
        // Deliver spikes for current time slice
        deliverCurrentSlice();

        // Advance time (atomic double doesn't have fetch_add in C++17, so we use store)
        double newTime = currentTime.load() + timeStep;
        currentTime.store(newTime);
        currentSliceIndex = (currentSliceIndex + 1) % numTimeSlices;

        // Sleep for the time step duration (simulating real-time)
        // In a real simulation, you might want to run as fast as possible
        // or synchronize with wall-clock time
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    SNNFW_INFO("SpikeProcessor: Processing loop ended");
}

void SpikeProcessor::deliverCurrentSlice() {
    std::vector<std::shared_ptr<ActionPotential>> spikesToDeliver;
    
    // Extract spikes for current time slice
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        spikesToDeliver = std::move(eventQueue[currentSliceIndex]);
        eventQueue[currentSliceIndex].clear();
    }
    
    if (spikesToDeliver.empty()) {
        return;
    }
    
    SNNFW_TRACE("SpikeProcessor: Delivering {} spikes at time {:.3f}ms",
                spikesToDeliver.size(), currentTime.load());
    
    // Divide spikes evenly among threads
    size_t spikesPerThread = (spikesToDeliver.size() + numDeliveryThreads - 1) / numDeliveryThreads;
    
    std::vector<std::future<void>> deliveryTasks;
    
    for (size_t threadIdx = 0; threadIdx < numDeliveryThreads; ++threadIdx) {
        size_t startIdx = threadIdx * spikesPerThread;
        size_t endIdx = std::min(startIdx + spikesPerThread, spikesToDeliver.size());
        
        if (startIdx >= spikesToDeliver.size()) {
            break;
        }
        
        // Submit delivery task to thread pool
        deliveryTasks.emplace_back(
            threadPool->enqueue([this, &spikesToDeliver, startIdx, endIdx]() {
                for (size_t i = startIdx; i < endIdx; ++i) {
                    const auto& spike = spikesToDeliver[i];
                    
                    // Find the target dendrite
                    std::shared_ptr<Dendrite> dendrite;
                    {
                        std::lock_guard<std::mutex> lock(registryMutex);
                        auto it = dendriteRegistry.find(spike->getDendriteId());
                        if (it != dendriteRegistry.end()) {
                            dendrite = it->second;
                        }
                    }
                    
                    if (dendrite) {
                        dendrite->receiveSpike(spike);
                    } else {
                        SNNFW_WARN("SpikeProcessor: Dendrite {} not found for spike delivery",
                                   spike->getDendriteId());
                    }
                }
            })
        );
    }
    
    // Wait for all delivery tasks to complete
    for (auto& task : deliveryTasks) {
        task.get();
    }
}

} // namespace snnfw

