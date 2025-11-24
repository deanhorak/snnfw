#include "snnfw/RecordingManager.h"
#include "snnfw/ActivityVisualizer.h"
#include <fstream>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <set>

namespace snnfw {

RecordingManager::RecordingManager(ActivityVisualizer& visualizer)
    : visualizer_(visualizer)
    , recording_(false)
    , recordingStartTime_(0)
    , playbackIndex_(0)
{
}

RecordingManager::~RecordingManager() {
}

void RecordingManager::startRecording() {
    if (recording_) return;

    recording_ = true;
    recordingStartTime_ = 0;  // Will be set on first spike
    spikes_.clear();
    
    metadata_.name = "Recording";
    metadata_.startTime = 0;
    metadata_.endTime = 0;
    metadata_.duration = 0;
    metadata_.spikeCount = 0;
    metadata_.neuronCount = 0;
    metadata_.timestamp = getCurrentTimestamp();
}

void RecordingManager::stopRecording() {
    if (!recording_) return;
    
    recording_ = false;
    updateMetadata();
}

void RecordingManager::recordSpike(const RecordedSpike& spike) {
    if (!recording_) return;

    // Set start time on first spike
    if (spikes_.empty()) {
        recordingStartTime_ = spike.timestamp;
        metadata_.startTime = spike.timestamp;
    }

    spikes_.push_back(spike);
    metadata_.endTime = spike.timestamp;
}

bool RecordingManager::saveRecording(const std::string& filename) {
    if (spikes_.empty()) {
        return false;
    }
    
    updateMetadata();
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write magic number
    const char magic[4] = {'S', 'N', 'N', 'R'};
    file.write(magic, 4);
    
    // Write version
    uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Write metadata as JSON-like string
    std::stringstream metaStream;
    metaStream << "{"
               << "\"name\":\"" << metadata_.name << "\","
               << "\"startTime\":" << metadata_.startTime << ","
               << "\"endTime\":" << metadata_.endTime << ","
               << "\"duration\":" << metadata_.duration << ","
               << "\"spikeCount\":" << metadata_.spikeCount << ","
               << "\"neuronCount\":" << metadata_.neuronCount << ","
               << "\"timestamp\":\"" << metadata_.timestamp << "\""
               << "}";
    
    std::string metaStr = metaStream.str();
    uint32_t metaLength = static_cast<uint32_t>(metaStr.length());
    file.write(reinterpret_cast<const char*>(&metaLength), sizeof(metaLength));
    file.write(metaStr.c_str(), metaLength);

    // Write spike count
    uint64_t spikeCount = spikes_.size();
    file.write(reinterpret_cast<const char*>(&spikeCount), sizeof(spikeCount));

    // Write spikes
    for (const auto& spike : spikes_) {
        file.write(reinterpret_cast<const char*>(&spike.timestamp), sizeof(spike.timestamp));
        file.write(reinterpret_cast<const char*>(&spike.sourceNeuronId), sizeof(spike.sourceNeuronId));
        file.write(reinterpret_cast<const char*>(&spike.targetNeuronId), sizeof(spike.targetNeuronId));
        file.write(reinterpret_cast<const char*>(&spike.synapseId), sizeof(spike.synapseId));
    }
    
    file.close();
    return true;
}

bool RecordingManager::loadRecording(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read and verify magic number
    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "SNNR", 4) != 0) {
        return false;
    }
    
    // Read version
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        return false;  // Unsupported version
    }
    
    // Read metadata
    uint32_t metaLength;
    file.read(reinterpret_cast<char*>(&metaLength), sizeof(metaLength));
    
    std::vector<char> metaBuffer(metaLength + 1);
    file.read(metaBuffer.data(), metaLength);
    metaBuffer[metaLength] = '\0';
    
    // Parse metadata (simple parsing, not full JSON)
    std::string metaStr(metaBuffer.data());
    // For now, just extract the values we need
    // In production, use a proper JSON parser
    
    // Read spike count
    uint64_t spikeCount;
    file.read(reinterpret_cast<char*>(&spikeCount), sizeof(spikeCount));

    // Read spikes
    spikes_.clear();
    spikes_.reserve(spikeCount);

    for (uint64_t i = 0; i < spikeCount; ++i) {
        RecordedSpike spike;
        file.read(reinterpret_cast<char*>(&spike.timestamp), sizeof(spike.timestamp));
        file.read(reinterpret_cast<char*>(&spike.sourceNeuronId), sizeof(spike.sourceNeuronId));
        file.read(reinterpret_cast<char*>(&spike.targetNeuronId), sizeof(spike.targetNeuronId));
        file.read(reinterpret_cast<char*>(&spike.synapseId), sizeof(spike.synapseId));
        spikes_.push_back(spike);
    }
    
    file.close();
    
    // Update metadata
    updateMetadata();
    
    // Reset playback state
    playbackState_.startTime = metadata_.startTime;
    playbackState_.endTime = metadata_.endTime;
    playbackState_.currentTime = metadata_.startTime;
    playbackIndex_ = 0;
    
    return true;
}

void RecordingManager::play() {
    if (spikes_.empty()) return;

    playbackState_.playing = true;
    playbackState_.paused = false;
}

void RecordingManager::pause() {
    playbackState_.paused = true;
}

void RecordingManager::stop() {
    playbackState_.playing = false;
    playbackState_.paused = false;
    playbackState_.currentTime = playbackState_.startTime;
    playbackIndex_ = 0;
}

void RecordingManager::setSpeed(float speed) {
    playbackState_.speed = std::max(0.1f, std::min(10.0f, speed));
}

void RecordingManager::setLooping(bool loop) {
    playbackState_.looping = loop;
}

void RecordingManager::seek(uint64_t time) {
    playbackState_.currentTime = std::max(playbackState_.startTime,
                                          std::min(playbackState_.endTime, time));

    // Find corresponding index in recording
    playbackIndex_ = 0;
    for (size_t i = 0; i < spikes_.size(); ++i) {
        if (spikes_[i].timestamp >= playbackState_.currentTime) {
            playbackIndex_ = i;
            break;
        }
    }
}

void RecordingManager::update(uint64_t deltaTime) {
    if (!playbackState_.playing || playbackState_.paused || spikes_.empty()) {
        return;
    }

    // Advance playback time
    uint64_t scaledDelta = static_cast<uint64_t>(deltaTime * playbackState_.speed);
    playbackState_.currentTime += scaledDelta;

    // Play spikes that occurred during this time step
    while (playbackIndex_ < spikes_.size() &&
           spikes_[playbackIndex_].timestamp <= playbackState_.currentTime) {

        const auto& spike = spikes_[playbackIndex_];
        visualizer_.recordSpike(spike.sourceNeuronId, spike.targetNeuronId,
                               spike.synapseId, spike.timestamp);
        playbackIndex_++;
    }
    
    // Check if we've reached the end
    if (playbackState_.currentTime >= playbackState_.endTime) {
        if (playbackState_.looping) {
            // Loop back to start
            playbackState_.currentTime = playbackState_.startTime;
            playbackIndex_ = 0;
        } else {
            // Stop playback
            stop();
        }
    }
}

void RecordingManager::clearRecording() {
    spikes_.clear();
    playbackIndex_ = 0;
    playbackState_ = PlaybackState();
    metadata_ = RecordingMetadata();
}

void RecordingManager::updateMetadata() {
    metadata_.spikeCount = spikes_.size();

    if (!spikes_.empty()) {
        metadata_.startTime = spikes_.front().timestamp;
        metadata_.endTime = spikes_.back().timestamp;
        metadata_.duration = metadata_.endTime - metadata_.startTime;

        // Count unique neurons
        std::set<uint64_t> uniqueNeurons;
        for (const auto& spike : spikes_) {
            uniqueNeurons.insert(spike.sourceNeuronId);
            uniqueNeurons.insert(spike.targetNeuronId);
        }
        metadata_.neuronCount = uniqueNeurons.size();
    }
}

std::string RecordingManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

} // namespace snnfw

