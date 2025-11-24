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
      currentSliceIndex(0),
      realTimeSync(true),
      totalLoopTime(0.0),
      maxLoopTime(0.0),
      loopCount(0),
      accumulatedDrift(0.0) {

    // Initialize event queue with empty vectors for each time slice
    eventQueue.resize(numTimeSlices);

    // Create thread pool for spike delivery
    threadPool = std::make_unique<ThreadPool>(numDeliveryThreads);

    SNNFW_INFO("SpikeProcessor created: {} time slices, {} delivery threads, real-time sync: {}",
               numTimeSlices, numDeliveryThreads, realTimeSync);
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

    // Reset timing statistics
    {
        std::lock_guard<std::mutex> lock(statsMutex);
        totalLoopTime = 0.0;
        maxLoopTime = 0.0;
        loopCount = 0;
        accumulatedDrift = 0.0;
    }

    // Record wall-clock start time
    startWallTime = std::chrono::steady_clock::now();

    // Start the background processing thread
    processingThread = std::thread(&SpikeProcessor::processingLoop, this);

    SNNFW_INFO("SpikeProcessor started (real-time sync: {})", realTimeSync);
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

    auto loopStartTime = std::chrono::steady_clock::now();

    while (!stopRequested.load()) {
        auto iterationStart = std::chrono::steady_clock::now();

        // Deliver spikes for current time slice
        deliverCurrentSlice();

        // Advance time (atomic double doesn't have fetch_add in C++17, so we use store)
        double newTime = currentTime.load() + timeStep;
        currentTime.store(newTime);
        currentSliceIndex = (currentSliceIndex + 1) % numTimeSlices;

        auto iterationEnd = std::chrono::steady_clock::now();

        // Calculate how long this iteration took
        auto iterationDuration = std::chrono::duration_cast<std::chrono::microseconds>(
            iterationEnd - iterationStart);
        double iterationTimeUs = static_cast<double>(iterationDuration.count());

        // Update timing statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex);
            totalLoopTime += iterationTimeUs;
            maxLoopTime = std::max(maxLoopTime, iterationTimeUs);
            loopCount++;
        }

        if (realTimeSync) {
            // Real-time synchronization: each timeslice should take exactly 1ms of wall-clock time
            // Calculate expected wall-clock time for this simulation time
            double expectedWallTimeMs = currentTime.load();
            auto expectedWallTime = startWallTime +
                std::chrono::microseconds(static_cast<int64_t>(expectedWallTimeMs * 1000.0));

            auto now = std::chrono::steady_clock::now();

            // Calculate drift
            auto drift = std::chrono::duration_cast<std::chrono::microseconds>(now - expectedWallTime);
            double driftMs = static_cast<double>(drift.count()) / 1000.0;

            {
                std::lock_guard<std::mutex> lock(statsMutex);
                accumulatedDrift = driftMs;
            }

            // If we're ahead of schedule, sleep to maintain real-time sync
            if (drift.count() < 0) {
                // We're ahead - sleep for the remaining time
                auto sleepDuration = std::chrono::microseconds(-drift.count());
                std::this_thread::sleep_for(sleepDuration);
            } else if (driftMs > 10.0) {
                // We're falling behind by more than 10ms - log a warning
                SNNFW_WARN("SpikeProcessor: Falling behind real-time by {:.2f}ms at simulation time {:.1f}ms",
                          driftMs, currentTime.load());
            }

            // Log timing info periodically (every 1000 iterations = 1 second of sim time)
            if (loopCount % 1000 == 0) {
                double avgLoopTime = totalLoopTime / loopCount;
                SNNFW_DEBUG("SpikeProcessor: Sim time: {:.1f}ms, Avg loop: {:.1f}μs, Max loop: {:.1f}μs, Drift: {:.2f}ms",
                           currentTime.load(), avgLoopTime, maxLoopTime, driftMs);
            }
        } else {
            // Non-real-time mode: run as fast as possible
            // Just a tiny sleep to prevent CPU spinning if there's no work
            if (iterationTimeUs < 10.0) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    }

    SNNFW_INFO("SpikeProcessor: Processing loop ended at simulation time {:.3f}ms",
               currentTime.load());

    // Log final statistics
    double avgLoopTime, maxLoop, drift;
    getTimingStats(avgLoopTime, maxLoop, drift);
    SNNFW_INFO("SpikeProcessor: Final stats - Avg loop: {:.1f}μs, Max loop: {:.1f}μs, Final drift: {:.2f}ms",
               avgLoopTime, maxLoop, drift);
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

void SpikeProcessor::getTimingStats(double& avgLoopTime, double& maxLoop, double& driftMs) const {
    std::lock_guard<std::mutex> lock(statsMutex);

    if (loopCount > 0) {
        avgLoopTime = totalLoopTime / loopCount;
    } else {
        avgLoopTime = 0.0;
    }

    maxLoop = maxLoopTime;
    driftMs = accumulatedDrift;
}

} // namespace snnfw

