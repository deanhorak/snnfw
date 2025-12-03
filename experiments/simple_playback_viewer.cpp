// Simple 3D Playback Viewer for SNNFW Recordings
// Usage: ./simple_playback_viewer <recording_file.snnr> [--speed <multiplier>] [--loop]

#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <vector>
#include <algorithm>

#include "snnfw/RecordingManager.h"
#include "snnfw/SimulationConfig.h"
#include "snnfw/VisualizationManager.h"

#include <imgui.h>
#include <glm/glm.hpp>

using namespace snnfw;

// Simple particle structure for spike visualization
struct SpikeParticle {
    uint64_t neuronId;
    float lifetime;
    float maxLifetime;
    glm::vec3 position;
    glm::vec3 color;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <recording_file.snnr> [--speed <multiplier>] [--loop]" << std::endl;
        return 1;
    }

    std::string recordingFile = argv[1];
    float playbackSpeed = 1.0f;
    bool looping = false;

    // Parse command-line arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--speed" && i + 1 < argc) {
            playbackSpeed = std::stof(argv[++i]);
        } else if (arg == "--loop") {
            looping = true;
        }
    }

    std::cout << "Simple Playback Viewer" << std::endl;
    std::cout << "Recording file: " << recordingFile << std::endl;
    std::cout << "Playback speed: " << playbackSpeed << "x" << std::endl;
    std::cout << "Looping: " << (looping ? "enabled" : "disabled") << std::endl;

    // Create simulation config for playback mode
    SimulationConfig simConfig;
    simConfig.playbackMode = true;
    simConfig.playbackFilename = recordingFile;
    simConfig.playbackSpeed = playbackSpeed;
    simConfig.playbackLooping = looping;
    simConfig.enableVisualization = true;
    simConfig.visualizationWidth = 1920;
    simConfig.visualizationHeight = 1080;
    simConfig.visualizationTitle = "SNNFW Playback Viewer - " + recordingFile;

    // Create recording manager and load the recording
    RecordingManager recordingManager;
    
    std::cout << "\nLoading recording..." << std::endl;
    if (!recordingManager.loadRecording(recordingFile)) {
        std::cerr << "Failed to load recording file: " << recordingFile << std::endl;
        return 1;
    }

    const auto& metadata = recordingManager.getMetadata();
    std::cout << "✓ Recording loaded successfully" << std::endl;
    std::cout << "  Total spikes: " << metadata.spikeCount << std::endl;
    std::cout << "  Duration: " << metadata.duration << " ms" << std::endl;
    std::cout << "  Neurons: " << metadata.neuronCount << std::endl;

    // Create visualization manager
    VisualizationManager vizManager(simConfig);
    vizManager.enableVSync(true);
    vizManager.setBackgroundColor(0.05f, 0.05f, 0.1f);

    // Spike particles for visualization
    std::vector<SpikeParticle> particles;
    std::unordered_map<uint64_t, uint64_t> neuronActivityCount;
    std::unordered_map<uint64_t, glm::vec3> neuronPositions;

    // Auto-generate neuron positions in a grid layout
    auto getOrCreateNeuronPosition = [&](uint64_t neuronId) -> glm::vec3 {
        auto it = neuronPositions.find(neuronId);
        if (it != neuronPositions.end()) {
            return it->second;
        }

        // Create a grid-like layout based on neuron ID
        size_t index = neuronPositions.size();
        int gridSize = 50;  // 50x50 grid per layer
        float spacing = 2.0f;

        int x = (index % gridSize) - gridSize / 2;
        int y = ((index / gridSize) % gridSize) - gridSize / 2;
        int z = (index / (gridSize * gridSize)) - 5;

        glm::vec3 pos(x * spacing, y * spacing, z * spacing);
        neuronPositions[neuronId] = pos;
        return pos;
    };

    // Set up playback callback
    recordingManager.setPlaybackCallback([&](uint64_t sourceNeuronId, uint64_t targetNeuronId,
                                             uint64_t synapseId, uint64_t timestamp) {
        // Track neuron activity
        neuronActivityCount[sourceNeuronId]++;

        // Create spike particle with 3D position
        SpikeParticle particle;
        particle.neuronId = sourceNeuronId;
        particle.position = getOrCreateNeuronPosition(sourceNeuronId);
        particle.lifetime = 0.0f;
        particle.maxLifetime = 500.0f; // 500ms lifetime

        // Color based on neuron ID (deterministic but varied)
        float hue = (sourceNeuronId % 360) / 360.0f;
        particle.color = glm::vec3(
            0.5f + 0.5f * std::sin(hue * 6.28f),
            0.5f + 0.5f * std::sin((hue + 0.33f) * 6.28f),
            0.5f + 0.5f * std::sin((hue + 0.67f) * 6.28f)
        );

        particles.push_back(particle);
    });

    // Start playback
    recordingManager.setSpeed(playbackSpeed);
    recordingManager.setLooping(looping);
    recordingManager.play();

    std::cout << "\nStarting playback..." << std::endl;
    std::cout << "(Close window to exit)" << std::endl;

    // Set up render callback
    vizManager.setRenderCallback([&](double deltaTime) {
        // Update playback
        recordingManager.update(static_cast<uint64_t>(deltaTime * 1000.0));

        // Update particles
        for (auto it = particles.begin(); it != particles.end();) {
            it->lifetime += static_cast<float>(deltaTime * 1000.0);
            if (it->lifetime >= it->maxLifetime) {
                it = particles.erase(it);
            } else {
                ++it;
            }
        }

        // TODO: Add 3D rendering of spike particles using modern OpenGL
        // For now, just show ImGui controls

        // ImGui UI
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Playback Controls");

        // Get playback state
        const auto& state = recordingManager.getPlaybackState();

        // Playback state
        if (ImGui::Button(state.playing ? "Pause" : "Play")) {
            if (state.playing) {
                recordingManager.pause();
            } else {
                recordingManager.play();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            recordingManager.stop();
        }

        // Speed control
        float speed = state.speed;
        if (ImGui::SliderFloat("Speed", &speed, 0.1f, 100.0f, "%.1fx", ImGuiSliderFlags_Logarithmic)) {
            recordingManager.setSpeed(speed);
        }

        // Loop control
        bool loop = state.looping;
        if (ImGui::Checkbox("Loop", &loop)) {
            recordingManager.setLooping(loop);
        }

        // Progress
        uint64_t currentTime = state.currentTime;
        uint64_t duration = metadata.duration;
        float progress = (duration > 0) ? (static_cast<float>(currentTime) / duration) : 0.0f;
        ImGui::ProgressBar(progress, ImVec2(-1, 0),
                          (std::to_string(currentTime) + " / " + std::to_string(duration) + " ms").c_str());

        // Statistics
        ImGui::Separator();
        ImGui::Text("Active particles: %zu", particles.size());
        ImGui::Text("Unique neurons: %zu", neuronActivityCount.size());
        ImGui::Text("Total spikes: %lu", metadata.spikeCount);

        // Top active neurons
        ImGui::Separator();
        ImGui::Text("Top 10 Most Active Neurons:");

        // Sort neurons by activity
        std::vector<std::pair<uint64_t, uint64_t>> sortedActivity(
            neuronActivityCount.begin(), neuronActivityCount.end());
        std::sort(sortedActivity.begin(), sortedActivity.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        for (size_t i = 0; i < std::min(size_t(10), sortedActivity.size()); ++i) {
            ImGui::Text("  Neuron %lu: %lu spikes",
                       sortedActivity[i].first, sortedActivity[i].second);
        }

        ImGui::End();
    });

    // Run visualization
    vizManager.run();

    std::cout << "\n=== Playback Complete ===" << std::endl;
    return 0;
}

