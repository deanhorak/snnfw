/**
 * @file playback_viewer.cpp
 * @brief Simple playback viewer for recorded spike activity
 * 
 * Usage:
 *   ./playback_viewer <recording_file.snnr> [--speed <multiplier>] [--loop]
 * 
 * Example:
 *   ./playback_viewer emnist_training.snnr --speed 2.0 --loop
 */

#include "snnfw/RecordingManager.h"
#include "snnfw/Logger.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <map>
#include <iomanip>

using namespace snnfw;

// Simple console-based spike activity display
class ConsoleVisualizer {
public:
    void recordSpike(uint64_t sourceNeuronId, uint64_t targetNeuronId, 
                     uint64_t synapseId, uint64_t timestamp) {
        spikeCount_++;
        lastTimestamp_ = timestamp;
        
        // Track spikes per neuron
        neuronActivity_[sourceNeuronId]++;
        
        // Print progress every 1000 spikes
        if (spikeCount_ % 1000 == 0) {
            printProgress();
        }
    }
    
    void printProgress() {
        std::cout << "\r[Playback] Time: " << std::setw(8) << lastTimestamp_ << " ms  "
                  << "Spikes: " << std::setw(10) << spikeCount_ << "  "
                  << "Active neurons: " << std::setw(6) << neuronActivity_.size()
                  << std::flush;
    }
    
    void printSummary() {
        std::cout << "\n\n=== Playback Summary ===" << std::endl;
        std::cout << "Total spikes: " << spikeCount_ << std::endl;
        std::cout << "Total active neurons: " << neuronActivity_.size() << std::endl;
        std::cout << "Duration: " << lastTimestamp_ << " ms" << std::endl;
        
        // Find most active neurons
        std::vector<std::pair<uint64_t, int>> sorted;
        for (const auto& [neuronId, count] : neuronActivity_) {
            sorted.push_back({neuronId, count});
        }
        std::sort(sorted.begin(), sorted.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::cout << "\nTop 10 most active neurons:" << std::endl;
        for (int i = 0; i < 10 && i < sorted.size(); ++i) {
            std::cout << "  Neuron " << sorted[i].first << ": " 
                      << sorted[i].second << " spikes" << std::endl;
        }
    }
    
private:
    uint64_t spikeCount_ = 0;
    uint64_t lastTimestamp_ = 0;
    std::map<uint64_t, int> neuronActivity_;
};

int main(int argc, char** argv) {
    // Parse command line arguments
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <recording_file.snnr> [--speed <multiplier>] [--loop]" << std::endl;
        std::cerr << "\nExample:" << std::endl;
        std::cerr << "  " << argv[0] << " emnist_training.snnr --speed 2.0 --loop" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    float speed = 1.0f;
    bool loop = false;
    
    // Parse optional arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--speed" && i + 1 < argc) {
            speed = std::stof(argv[++i]);
        } else if (arg == "--loop") {
            loop = true;
        }
    }
    
    std::cout << "=== SNNFW Playback Viewer ===" << std::endl;
    std::cout << "Recording file: " << filename << std::endl;
    std::cout << "Playback speed: " << speed << "x" << std::endl;
    std::cout << "Looping: " << (loop ? "enabled" : "disabled") << std::endl;
    std::cout << std::endl;
    
    // Create recording manager
    RecordingManager recordingManager;
    
    // Load recording
    std::cout << "Loading recording..." << std::endl;
    if (!recordingManager.loadRecording(filename)) {
        std::cerr << "Error: Failed to load recording from " << filename << std::endl;
        return 1;
    }
    
    // Print recording metadata
    const auto& metadata = recordingManager.getMetadata();
    std::cout << "✓ Recording loaded successfully" << std::endl;
    std::cout << "  Total spikes: " << metadata.spikeCount << std::endl;
    std::cout << "  Duration: " << metadata.duration << " ms" << std::endl;
    std::cout << "  Start time: " << metadata.startTime << std::endl;
    std::cout << "  End time: " << metadata.endTime << std::endl;
    std::cout << std::endl;
    
    // Create visualizer
    ConsoleVisualizer visualizer;
    
    // Set up playback callback
    recordingManager.setPlaybackCallback(
        [&visualizer](uint64_t sourceNeuronId, uint64_t targetNeuronId, 
                      uint64_t synapseId, uint64_t timestamp) {
            visualizer.recordSpike(sourceNeuronId, targetNeuronId, synapseId, timestamp);
        }
    );
    
    // Configure playback
    recordingManager.setSpeed(speed);
    recordingManager.setLooping(loop);
    
    // Start playback
    std::cout << "Starting playback..." << std::endl;
    std::cout << "(Press Ctrl+C to stop)" << std::endl;
    std::cout << std::endl;
    recordingManager.play();
    
    // Playback loop
    auto lastTime = std::chrono::steady_clock::now();
    while (recordingManager.getPlaybackState().playing) {
        auto currentTime = std::chrono::steady_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastTime).count();
        lastTime = currentTime;
        
        recordingManager.update(deltaTime);
        
        // Sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Print summary
    visualizer.printSummary();
    
    return 0;
}

