// 3D Playback Viewer for SNNFW Recordings with Network Visualization
// Usage: ./simple_playback_viewer <recording_file.snnr> [--network <network_file.snnw>] [--speed <multiplier>] [--loop]

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
#include "snnfw/NetworkStructureExporter.h"
#include "snnfw/NetworkDataAdapter.h"
#include "snnfw/NetworkInspector.h"
#include "snnfw/Datastore.h"
#include "snnfw/ActivityMonitor.h"
#include "snnfw/ActivityVisualizer.h"
#include "snnfw/ShaderManager.h"
#include "snnfw/NetworkGraphRenderer.h"
#include "snnfw/SpikeRenderer.h"
#include "snnfw/Camera.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

using namespace snnfw;

// Spike particle structure for playback visualization
struct PlaybackSpikeParticle {
    uint64_t neuronId;
    float lifetime;
    float maxLifetime;
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <recording_file.snnr> [--network <network_file.snnw>] [--speed <multiplier>] [--loop]" << std::endl;
        return 1;
    }

    std::string recordingFile = argv[1];
    std::string networkFile = "";
    float playbackSpeed = 1.0f;
    bool looping = false;

    // Parse command-line arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--network" && i + 1 < argc) {
            networkFile = argv[++i];
        } else if (arg == "--speed" && i + 1 < argc) {
            playbackSpeed = std::stof(argv[++i]);
        } else if (arg == "--loop") {
            looping = true;
        }
    }

    // Auto-detect network file if not specified
    if (networkFile.empty()) {
        // Try replacing .snnr with .snnw
        size_t dotPos = recordingFile.find_last_of('.');
        if (dotPos != std::string::npos) {
            networkFile = recordingFile.substr(0, dotPos) + ".snnw";
        }
    }

    std::cout << "3D Playback Viewer with Network Visualization" << std::endl;
    std::cout << "Recording file: " << recordingFile << std::endl;
    std::cout << "Network file: " << networkFile << std::endl;
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
    simConfig.visualizationTitle = "SNNFW 3D Playback Viewer - " + recordingFile;

    // Create recording manager and load the recording
    RecordingManager recordingManager;

    std::cout << "\nLoading recording..." << std::endl;
    // Use streaming playback to avoid loading all spikes into memory
    if (!recordingManager.loadRecording(recordingFile, true)) {
        std::cerr << "Failed to load recording file: " << recordingFile << std::endl;
        return 1;
    }
    std::cout << "✓ Using streaming playback mode (spikes loaded on-demand)" << std::endl;

    const auto& metadata = recordingManager.getMetadata();
    std::cout << "✓ Recording loaded successfully" << std::endl;
    std::cout << "  Total spikes: " << metadata.spikeCount << std::endl;
    std::cout << "  Duration: " << metadata.duration << " ms" << std::endl;
    std::cout << "  Neurons: " << metadata.neuronCount << std::endl;

    // Load network structure if available
    std::vector<NeuronVisualData> networkNeurons;
    std::vector<SynapseVisualData> networkSynapses;
    std::unordered_map<uint64_t, size_t> neuronIndexMap;
    bool hasNetworkStructure = false;

    std::cout << "\nLoading network structure..." << std::endl;
    NetworkStructureExporter exporter;
    if (exporter.load(networkFile)) {
        std::cout << "✓ Network structure loaded successfully" << std::endl;
        const auto& netMetadata = exporter.getMetadata();
        std::cout << "  Network: " << netMetadata.name << std::endl;
        std::cout << "  Neurons: " << netMetadata.neuronCount << std::endl;
        std::cout << "  Synapses: " << netMetadata.synapseCount << std::endl;
        std::cout << "  Layers: " << netMetadata.layerCount << std::endl;

        // Convert ExportedNeuron to NeuronVisualData
        for (const auto& exportedNeuron : exporter.getNeurons()) {
            NeuronVisualData neuron;
            neuron.id = exportedNeuron.id;
            neuron.position = exportedNeuron.position;
            neuron.radius = exportedNeuron.radius;
            neuron.r = exportedNeuron.r;
            neuron.g = exportedNeuron.g;
            neuron.b = exportedNeuron.b;
            neuron.a = exportedNeuron.a;
            neuron.isExcitatory = exportedNeuron.isExcitatory;
            neuron.clusterId = exportedNeuron.clusterId;
            neuron.layerId = exportedNeuron.layerId;
            neuron.columnId = exportedNeuron.columnId;
            neuron.nucleusId = exportedNeuron.nucleusId;
            neuron.regionId = exportedNeuron.regionId;
            neuron.lobeId = exportedNeuron.lobeId;
            neuron.hemisphereId = exportedNeuron.hemisphereId;
            neuron.brainId = exportedNeuron.brainId;
            neuron.activity = 0.0f;

            neuronIndexMap[neuron.id] = networkNeurons.size();
            networkNeurons.push_back(neuron);
        }

        // Convert ExportedSynapse to SynapseVisualData
        for (const auto& exportedSynapse : exporter.getSynapses()) {
            SynapseVisualData synapse;
            synapse.id = exportedSynapse.id;
            synapse.sourceNeuronId = exportedSynapse.sourceNeuronId;
            synapse.targetNeuronId = exportedSynapse.targetNeuronId;

            // Look up neuron positions
            auto sourceIt = neuronIndexMap.find(exportedSynapse.sourceNeuronId);
            auto targetIt = neuronIndexMap.find(exportedSynapse.targetNeuronId);
            if (sourceIt != neuronIndexMap.end() && targetIt != neuronIndexMap.end()) {
                const auto& srcPos = networkNeurons[sourceIt->second].position;
                const auto& tgtPos = networkNeurons[targetIt->second].position;
                synapse.sourcePos = Position3D(srcPos.x, srcPos.y, srcPos.z);
                synapse.targetPos = Position3D(tgtPos.x, tgtPos.y, tgtPos.z);
            } else {
                synapse.sourcePos = Position3D(0.0f, 0.0f, 0.0f);
                synapse.targetPos = Position3D(0.0f, 0.0f, 0.0f);
            }

            synapse.weight = exportedSynapse.weight;
            synapse.thickness = exportedSynapse.thickness;
            synapse.r = exportedSynapse.r;
            synapse.g = exportedSynapse.g;
            synapse.b = exportedSynapse.b;
            synapse.a = exportedSynapse.a;
            synapse.activity = 0.0f;

            networkSynapses.push_back(synapse);
        }

        hasNetworkStructure = true;
        std::cout << "✓ Network structure loaded (" << networkNeurons.size() << " neurons, "
                  << networkSynapses.size() << " synapses)" << std::endl;
    } else {
        std::cout << "⚠ Network structure file not found or failed to load: " << networkFile << std::endl;
        std::cout << "  Continuing with spike playback only (no 3D network visualization)" << std::endl;
    }

    // Create a NetworkDataAdapter for rendering
    // Note: We're creating a temporary datastore just for the adapter interface
    // The actual data will be set directly from the loaded network structure
    Datastore tempDatastore("./temp_playback_db", 1000);
    NetworkInspector tempInspector;
    ActivityMonitor tempActivityMonitor(tempDatastore);
    NetworkDataAdapter networkAdapter(tempDatastore, tempInspector, &tempActivityMonitor);

    // Populate adapter with loaded network data
    if (hasNetworkStructure) {
        networkAdapter.setNeurons(networkNeurons);
        networkAdapter.setSynapses(networkSynapses);

        // Build hierarchical groups from neuron data
        // We need to build ALL levels: Brain, Hemisphere, Lobe, Region, Nucleus, Column, Layer, Cluster

        // Helper to build groups at each level
        auto buildGroupsAtLevel = [&](const std::string& typeName,
                                       std::function<uint64_t(const NeuronVisualData&)> getIdFunc,
                                       std::function<uint64_t(const NeuronVisualData&)> getParentIdFunc) {
            std::map<uint64_t, std::vector<uint64_t>> groupNeurons;
            std::map<uint64_t, Position3D> groupCenters;
            std::map<uint64_t, uint64_t> groupParents;

            for (const auto& neuron : networkNeurons) {
                uint64_t groupId = getIdFunc(neuron);
                if (groupId != 0) {
                    groupNeurons[groupId].push_back(neuron.id);
                    groupParents[groupId] = getParentIdFunc(neuron);

                    // Accumulate positions for center calculation
                    if (groupCenters.find(groupId) == groupCenters.end()) {
                        groupCenters[groupId] = neuron.position;
                    } else {
                        groupCenters[groupId].x += neuron.position.x;
                        groupCenters[groupId].y += neuron.position.y;
                        groupCenters[groupId].z += neuron.position.z;
                    }
                }
            }

            std::vector<HierarchicalGroup> levelGroups;
            for (const auto& [groupId, neuronIds] : groupNeurons) {
                HierarchicalGroup group;
                group.id = groupId;
                group.typeName = typeName;
                group.name = typeName + " " + std::to_string(groupId);
                group.neuronIds = neuronIds;
                group.parentGroupId = groupParents[groupId];

                // Calculate center position
                if (!neuronIds.empty()) {
                    group.centerPosition.x = groupCenters[groupId].x / neuronIds.size();
                    group.centerPosition.y = groupCenters[groupId].y / neuronIds.size();
                    group.centerPosition.z = groupCenters[groupId].z / neuronIds.size();
                }

                // Calculate bounding box
                bool first = true;
                for (uint64_t neuronId : neuronIds) {
                    auto it = neuronIndexMap.find(neuronId);
                    if (it != neuronIndexMap.end()) {
                        const auto& pos = networkNeurons[it->second].position;
                        if (first) {
                            group.boundingBoxMin = pos;
                            group.boundingBoxMax = pos;
                            first = false;
                        } else {
                            group.boundingBoxMin.x = std::min(group.boundingBoxMin.x, pos.x);
                            group.boundingBoxMin.y = std::min(group.boundingBoxMin.y, pos.y);
                            group.boundingBoxMin.z = std::min(group.boundingBoxMin.z, pos.z);
                            group.boundingBoxMax.x = std::max(group.boundingBoxMax.x, pos.x);
                            group.boundingBoxMax.y = std::max(group.boundingBoxMax.y, pos.y);
                            group.boundingBoxMax.z = std::max(group.boundingBoxMax.z, pos.z);
                        }
                    }
                }

                levelGroups.push_back(group);
            }

            return levelGroups;
        };

        // Build groups at all hierarchical levels
        std::vector<HierarchicalGroup> groups;

        // Brain level
        auto brainGroups = buildGroupsAtLevel("Brain",
            [](const NeuronVisualData& n) { return n.brainId; },
            [](const NeuronVisualData& n) { return 0; });
        groups.insert(groups.end(), brainGroups.begin(), brainGroups.end());

        // Hemisphere level
        auto hemisphereGroups = buildGroupsAtLevel("Hemisphere",
            [](const NeuronVisualData& n) { return n.hemisphereId; },
            [](const NeuronVisualData& n) { return n.brainId; });
        groups.insert(groups.end(), hemisphereGroups.begin(), hemisphereGroups.end());

        // Lobe level
        auto lobeGroups = buildGroupsAtLevel("Lobe",
            [](const NeuronVisualData& n) { return n.lobeId; },
            [](const NeuronVisualData& n) { return n.hemisphereId; });
        groups.insert(groups.end(), lobeGroups.begin(), lobeGroups.end());

        // Region level
        auto regionGroups = buildGroupsAtLevel("Region",
            [](const NeuronVisualData& n) { return n.regionId; },
            [](const NeuronVisualData& n) { return n.lobeId; });
        groups.insert(groups.end(), regionGroups.begin(), regionGroups.end());

        // Nucleus level
        auto nucleusGroups = buildGroupsAtLevel("Nucleus",
            [](const NeuronVisualData& n) { return n.nucleusId; },
            [](const NeuronVisualData& n) { return n.regionId; });
        groups.insert(groups.end(), nucleusGroups.begin(), nucleusGroups.end());

        // Column level
        auto columnGroups = buildGroupsAtLevel("Column",
            [](const NeuronVisualData& n) { return n.columnId; },
            [](const NeuronVisualData& n) { return n.nucleusId; });
        groups.insert(groups.end(), columnGroups.begin(), columnGroups.end());

        // Layer level
        auto layerGroups = buildGroupsAtLevel("Layer",
            [](const NeuronVisualData& n) { return n.layerId; },
            [](const NeuronVisualData& n) { return n.columnId; });
        groups.insert(groups.end(), layerGroups.begin(), layerGroups.end());

        // Cluster level
        auto clusterGroups = buildGroupsAtLevel("Cluster",
            [](const NeuronVisualData& n) { return n.clusterId; },
            [](const NeuronVisualData& n) { return n.layerId; });
        groups.insert(groups.end(), clusterGroups.begin(), clusterGroups.end());

        // Set groups in adapter
        networkAdapter.setGroups(groups);

        std::cout << "✓ Network data loaded into adapter for rendering" << std::endl;
        std::cout << "  Built " << groups.size() << " hierarchical groups:" << std::endl;
        std::cout << "    Brain: " << brainGroups.size() << std::endl;
        std::cout << "    Hemisphere: " << hemisphereGroups.size() << std::endl;
        std::cout << "    Lobe: " << lobeGroups.size() << std::endl;
        std::cout << "    Region: " << regionGroups.size() << std::endl;
        std::cout << "    Nucleus: " << nucleusGroups.size() << std::endl;
        std::cout << "    Column: " << columnGroups.size() << std::endl;
        std::cout << "    Layer: " << layerGroups.size() << std::endl;
        std::cout << "    Cluster: " << clusterGroups.size() << std::endl;
    }

    // Build neuron-to-cluster map for spike filtering
    std::unordered_map<uint64_t, uint64_t> neuronToClusterMap;
    if (hasNetworkStructure) {
        const auto& neurons = networkAdapter.getNeurons();
        for (const auto& neuron : neurons) {
            if (neuron.clusterId != 0) {
                neuronToClusterMap[neuron.id] = neuron.clusterId;
            }
        }
        std::cout << "✓ Built neuron-to-cluster map (" << neuronToClusterMap.size() << " neurons)" << std::endl;
    }

    // Create visualization manager
    VisualizationManager vizManager(simConfig);
    vizManager.enableVSync(true);
    vizManager.setBackgroundColor(0.05f, 0.05f, 0.1f);

    // Create camera - start closer for better detail view
    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 20.0f, 50.0f));  // Much closer initial position
    camera.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    camera.setFOV(60.0f);  // Wider FOV for better spatial awareness

    // Initialize renderers for modern OpenGL visualization
    ShaderManager shaderManager;
    NetworkGraphRenderer networkRenderer(shaderManager);
    SpikeRenderer spikeRenderer(shaderManager);

    bool renderersInitialized = false;
    if (hasNetworkStructure) {
        std::cout << "\nInitializing 3D renderers..." << std::endl;
        if (networkRenderer.initialize() && spikeRenderer.initialize()) {
            renderersInitialized = true;
            std::cout << "✓ 3D renderers initialized successfully" << std::endl;
        } else {
            std::cerr << "⚠ Failed to initialize 3D renderers, falling back to basic visualization" << std::endl;
        }
    }

    // Spike particles for visualization
    std::vector<PlaybackSpikeParticle> particles;
    std::unordered_map<uint64_t, uint64_t> neuronActivityCount;
    std::unordered_map<uint64_t, glm::vec3> neuronPositions;

    // Visualization options
    bool showClusterLabels = false;  // Disabled by default for performance with many clusters
    bool showClusterBoundingBoxes = false;
    bool showClusterSpheres = true;  // Show clusters as spheres instead of individual neurons
    bool colorByCluster = true;
    bool colorByLayer = false;
    bool colorByActivity = false;
    float labelScale = 1.0f;
    float clusterSphereScale = 1.0f;  // Scale factor for cluster sphere size
    bool showSynapses = false;  // Disabled by default for performance
    bool showNeurons = false;  // Disabled by default - only show spikes during playback

    // Cluster selection
    int selectedClusterIndex = -1;  // Index into groups vector, -1 = none selected
    std::unordered_set<uint64_t> visibleClusterIds;  // Clusters whose spikes should be shown

    // Helper function to update visible clusters based on selection
    auto updateVisibleClusters = [&]() {
        visibleClusterIds.clear();

        if (selectedClusterIndex < 0 || !hasNetworkStructure) {
            // No selection - show all spikes
            return;
        }

        const auto& groups = networkAdapter.getGroups();
        if (selectedClusterIndex >= static_cast<int>(groups.size())) {
            return;
        }

        const auto& selectedGroup = groups[selectedClusterIndex];
        if (selectedGroup.typeName != "Cluster") {
            return;
        }

        // Add selected cluster
        visibleClusterIds.insert(selectedGroup.id);

        // Find adjacent clusters (same layer)
        uint64_t selectedLayerId = selectedGroup.parentGroupId;
        for (const auto& group : groups) {
            if (group.typeName == "Cluster" && group.parentGroupId == selectedLayerId) {
                visibleClusterIds.insert(group.id);
            }
        }

        std::cout << "Visible clusters updated: " << visibleClusterIds.size() << " clusters" << std::endl;
    };

    // Helper function to build hierarchical path for a group
    auto buildHierarchicalPath = [&](const HierarchicalGroup& group) -> std::vector<std::pair<std::string, std::string>> {
        std::vector<std::pair<std::string, std::string>> path;  // (typeName, name) pairs

        // Build map of group ID to group for quick lookup
        std::unordered_map<uint64_t, const HierarchicalGroup*> groupMap;
        for (const auto& g : networkAdapter.getGroups()) {
            groupMap[g.id] = &g;
        }

        // Traverse up the hierarchy
        const HierarchicalGroup* current = &group;
        while (current != nullptr) {
            path.insert(path.begin(), {current->typeName, current->name});

            // Move to parent
            if (current->parentGroupId != 0) {
                auto it = groupMap.find(current->parentGroupId);
                if (it != groupMap.end()) {
                    current = it->second;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        return path;
    };

    // Get neuron position from network structure or auto-generate
    auto getOrCreateNeuronPosition = [&](uint64_t neuronId) -> glm::vec3 {
        // First check if we have network structure
        if (hasNetworkStructure) {
            auto it = neuronIndexMap.find(neuronId);
            if (it != neuronIndexMap.end()) {
                const auto& pos = networkNeurons[it->second].position;
                return glm::vec3(pos.x, pos.y, pos.z);
            }
        }

        // Fallback: check cached positions
        auto it = neuronPositions.find(neuronId);
        if (it != neuronPositions.end()) {
            return it->second;
        }

        // Auto-generate position in grid layout
        size_t index = neuronPositions.size();
        int gridSize = 50;
        float spacing = 2.0f;

        int x = (index % gridSize) - gridSize / 2;
        int y = ((index / gridSize) % gridSize) - gridSize / 2;
        int z = (index / (gridSize * gridSize)) - 5;

        glm::vec3 pos(x * spacing, y * spacing, z * spacing);
        neuronPositions[neuronId] = pos;
        return pos;
    };

    // Set up playback callback
    const size_t MAX_PARTICLES = 50000; // Limit to prevent memory exhaustion
    const size_t MAX_SPIKES_PER_FRAME = 1000; // Limit spikes processed per frame to prevent lockup
    size_t spikesThisFrame = 0;

    recordingManager.setPlaybackCallback([&](uint64_t sourceNeuronId, uint64_t targetNeuronId,
                                             uint64_t synapseId, uint64_t timestamp) {
        // Track neuron activity
        neuronActivityCount[sourceNeuronId]++;

        // Limit spikes processed per frame to prevent rendering lockup
        if (spikesThisFrame >= MAX_SPIKES_PER_FRAME) {
            return; // Skip this spike - too many this frame
        }

        // Filter by cluster if selection is active - do this BEFORE creating particles
        if (!visibleClusterIds.empty()) {
            auto it = neuronToClusterMap.find(sourceNeuronId);
            if (it == neuronToClusterMap.end() || visibleClusterIds.find(it->second) == visibleClusterIds.end()) {
                return;  // Skip this spike - not in visible clusters
            }
        }

        // Limit particle count to prevent memory exhaustion with large recordings
        if (particles.size() >= MAX_PARTICLES) {
            return; // Skip creating new particles if we're at the limit
        }

        spikesThisFrame++;

        // Create spike particle with 3D position
        PlaybackSpikeParticle particle;
        particle.neuronId = sourceNeuronId;
        particle.position = getOrCreateNeuronPosition(sourceNeuronId);
        particle.lifetime = 0.0f;
        particle.maxLifetime = 100.0f; // 100ms lifetime (reduced from 500ms to limit active particles)
        particle.intensity = 1.0f;

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
    std::cout << "Controls:" << std::endl;
    std::cout << "  Left Mouse Drag: Orbit camera" << std::endl;
    std::cout << "  Right Mouse Drag: Pan camera" << std::endl;
    std::cout << "  Mouse Wheel: Zoom in/out" << std::endl;
    std::cout << "  WASD: Pan camera" << std::endl;
    std::cout << "  Q/E: Zoom in/out" << std::endl;
    std::cout << "  ESC: Exit" << std::endl;

    // Camera control state
    bool mousePressed = false;
    double lastMouseX = 0.0, lastMouseY = 0.0;

    // Store camera pointer for scroll callback (use a static variable to avoid overwriting window user pointer)
    static Camera* g_camera = &camera;
    GLFWwindow* window = vizManager.getWindow();

    // Set up scroll callback for mouse wheel zoom
    // NOTE: Don't use glfwSetWindowUserPointer here - it's already used by VisualizationManager
    glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
        if (g_camera) {
            // Zoom in/out based on scroll direction
            float zoomAmount = static_cast<float>(yoffset) * 10.0f;
            g_camera->zoom(-zoomAmount);  // Negative because scroll up should zoom in
        }
    });

    // Set up render callback
    vizManager.setRenderCallback([&](double deltaTime) {
        GLFWwindow* window = vizManager.getWindow();

        // ====================================================================
        // Camera Controls - Mouse
        // ====================================================================
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // Only handle camera controls if ImGui is not using the mouse
        ImGuiIO& io = ImGui::GetIO();

        // Left mouse button: Orbit camera OR select cluster (on click without drag)
        static bool leftMouseJustPressed = false;
        static double clickStartX = 0, clickStartY = 0;
        static const double clickThreshold = 5.0;  // pixels

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !io.WantCaptureMouse) {
            if (!mousePressed) {
                // Just pressed - record position
                leftMouseJustPressed = true;
                clickStartX = mouseX;
                clickStartY = mouseY;
            } else {
                // Already pressed - check if dragging
                double dragDist = std::sqrt(std::pow(mouseX - clickStartX, 2) + std::pow(mouseY - clickStartY, 2));
                if (dragDist > clickThreshold) {
                    leftMouseJustPressed = false;  // It's a drag, not a click
                }

                // Orbit camera
                float deltaX = static_cast<float>(mouseX - lastMouseX) * 0.01f;
                float deltaY = static_cast<float>(mouseY - lastMouseY) * 0.01f;
                camera.orbit(deltaX, -deltaY);
            }
            mousePressed = true;
        } else {
            // Mouse released
            if (mousePressed && leftMouseJustPressed && hasNetworkStructure) {
                // It was a click (not a drag) - check if we clicked on a cluster label
                double dragDist = std::sqrt(std::pow(mouseX - clickStartX, 2) + std::pow(mouseY - clickStartY, 2));
                if (dragDist <= clickThreshold) {
                    // Check if click is near any cluster label
                    glm::mat4 view = camera.getViewMatrix();
                    glm::mat4 projection = camera.getProjectionMatrix(800.0f / 600.0f);
                    glm::mat4 viewProj = projection * view;

                    int display_w, display_h;
                    glfwGetFramebufferSize(window, &display_w, &display_h);

                    float minDist = 50.0f;  // pixels
                    int closestCluster = -1;

                    const auto& groups = networkAdapter.getGroups();
                    for (size_t i = 0; i < groups.size(); ++i) {
                        const auto& group = groups[i];
                        if (group.typeName == "Cluster" && !group.neuronIds.empty()) {
                            glm::vec3 worldPos(group.centerPosition.x, group.centerPosition.y, group.centerPosition.z);
                            glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);

                            if (clipPos.w > 0.0f) {  // In front of camera
                                glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
                                if (ndc.x >= -1.0f && ndc.x <= 1.0f && ndc.y >= -1.0f && ndc.y <= 1.0f) {
                                    float screenX = (ndc.x + 1.0f) * 0.5f * display_w;
                                    float screenY = (1.0f - ndc.y) * 0.5f * display_h;

                                    float dist = std::sqrt(std::pow(screenX - mouseX, 2) + std::pow(screenY - mouseY, 2));
                                    if (dist < minDist) {
                                        minDist = dist;
                                        closestCluster = static_cast<int>(i);
                                    }
                                }
                            }
                        }
                    }

                    selectedClusterIndex = closestCluster;
                    updateVisibleClusters();  // Update visible clusters when selection changes
                }
            }
            mousePressed = false;
            leftMouseJustPressed = false;
        }

        // Right mouse button: Pan camera
        static bool rightMousePressed = false;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && !io.WantCaptureMouse) {
            if (rightMousePressed) {
                float deltaX = static_cast<float>(mouseX - lastMouseX) * 0.05f;
                float deltaY = static_cast<float>(mouseY - lastMouseY) * 0.05f;
                camera.pan(-deltaX, deltaY);
            }
            rightMousePressed = true;
        } else {
            rightMousePressed = false;
        }

        lastMouseX = mouseX;
        lastMouseY = mouseY;

        // ====================================================================
        // Camera Controls - Keyboard
        // ====================================================================
        float panSpeed = 5.0f * static_cast<float>(deltaTime);
        float zoomSpeed = 100.0f * static_cast<float>(deltaTime);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            camera.pan(0.0f, panSpeed);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            camera.pan(0.0f, -panSpeed);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camera.pan(-panSpeed, 0.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camera.pan(panSpeed, 0.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            camera.zoom(zoomSpeed);
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            camera.zoom(-zoomSpeed);
        }

        // ====================================================================
        // Update Playback
        // ====================================================================
        spikesThisFrame = 0;  // Reset spike counter for this frame
        recordingManager.update(static_cast<uint64_t>(deltaTime * 1000.0));

        // Update particles
        for (auto it = particles.begin(); it != particles.end();) {
            it->lifetime += static_cast<float>(deltaTime * 1000.0);
            it->intensity = 1.0f - (it->lifetime / it->maxLifetime);
            if (it->lifetime >= it->maxLifetime) {
                it = particles.erase(it);
            } else {
                ++it;
            }
        }

        // ====================================================================
        // 3D Rendering with Modern OpenGL
        // ====================================================================
        if (hasNetworkStructure && renderersInitialized) {
            // Clear depth buffer for 3D rendering
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Configure rendering
            RenderConfig renderConfig;
            renderConfig.renderNeurons = showNeurons;
            renderConfig.renderSynapses = showSynapses && (networkSynapses.size() > 0);
            renderConfig.neuronBaseRadius = 0.15f;  // Smaller neurons for better spacing
            renderConfig.neuronUseInstancing = true;
            renderConfig.frustumCulling = true;
            renderConfig.distanceCulling = true;
            renderConfig.maxRenderDistance = 500.0f;
            renderConfig.enableLighting = true;
            renderConfig.lightPosition = glm::vec3(100.0f, 100.0f, 100.0f);

            // Render network structure (neurons and synapses)
            networkRenderer.render(networkAdapter, camera, renderConfig);

            // Prepare cluster spheres if enabled
            std::vector<SpikeParticle> clusterSpheres;
            if (showClusterSpheres) {
                const auto& groups = networkAdapter.getGroups();
                for (const auto& group : groups) {
                    if (group.typeName == "Cluster") {
                        // Calculate cluster sphere position (center of bounding box)
                        glm::vec3 center(
                            (group.boundingBoxMin.x + group.boundingBoxMax.x) * 0.5f,
                            (group.boundingBoxMin.y + group.boundingBoxMax.y) * 0.5f,
                            (group.boundingBoxMin.z + group.boundingBoxMax.z) * 0.5f
                        );

                        // Calculate cluster sphere radius (half the bounding box diagonal)
                        glm::vec3 size(
                            group.boundingBoxMax.x - group.boundingBoxMin.x,
                            group.boundingBoxMax.y - group.boundingBoxMin.y,
                            group.boundingBoxMax.z - group.boundingBoxMin.z
                        );
                        float radius = glm::length(size) * 0.5f * clusterSphereScale;

                        // Color based on cluster ID or selection state
                        glm::vec4 color;
                        if (!visibleClusterIds.empty() && visibleClusterIds.find(group.id) != visibleClusterIds.end()) {
                            // Highlighted color for selected clusters
                            color = glm::vec4(1.0f, 0.8f, 0.2f, 0.4f);  // Yellow/orange, semi-transparent
                        } else {
                            // Default color based on cluster ID
                            float hue = (group.id % 360) / 360.0f;
                            color = glm::vec4(
                                0.5f + 0.5f * std::sin(hue * 6.28f),
                                0.5f + 0.5f * std::sin((hue + 0.33f) * 6.28f),
                                0.5f + 0.5f * std::sin((hue + 0.67f) * 6.28f),
                                0.3f  // Semi-transparent
                            );
                        }

                        // Create sphere particle
                        SpikeParticle sphere;
                        sphere.sourceNeuronId = group.id;
                        sphere.targetNeuronId = 0;
                        sphere.position = center;
                        sphere.velocity = glm::vec3(0.0f);
                        sphere.color = color;
                        sphere.size = radius;
                        sphere.lifetime = 0.0f;
                        sphere.maxLifetime = 1.0f;
                        sphere.synapseId = 0;
                        sphere.progress = 0.0f;

                        clusterSpheres.push_back(sphere);
                    }
                }
            }

            // Convert spike particles to SpikeRenderer format
            // Filter by visible clusters if a cluster is selected
            std::vector<SpikeParticle> spikeParticles;
            for (const auto& p : particles) {
                // Filter by cluster if selection is active
                if (!visibleClusterIds.empty()) {
                    auto it = neuronToClusterMap.find(p.neuronId);
                    if (it == neuronToClusterMap.end() || visibleClusterIds.find(it->second) == visibleClusterIds.end()) {
                        continue;  // Skip this spike - not in visible clusters
                    }
                }

                SpikeParticle sp;
                sp.sourceNeuronId = p.neuronId;
                sp.targetNeuronId = 0; // Not tracked in playback particles
                sp.position = p.position;
                sp.velocity = glm::vec3(0.0f); // Static particles for now
                sp.color = glm::vec4(p.color, p.intensity);
                sp.size = 2.0f * p.intensity;
                sp.lifetime = p.lifetime;
                sp.maxLifetime = p.maxLifetime;
                sp.synapseId = 0; // Not tracked in playback
                sp.progress = 0.0f; // Not used for static particles
                spikeParticles.push_back(sp);
            }

            // Render spike particles
            if (!spikeParticles.empty()) {
                SpikeRenderConfig spikeConfig;
                spikeConfig.enableBlending = true;
                spikeConfig.particleAlpha = 0.8f;
                spikeConfig.glowIntensity = 1.5f;
                spikeConfig.showTrails = false; // Disable trails for now

                // Render cluster spheres first (if enabled)
                if (!clusterSpheres.empty()) {
                    SpikeRenderConfig clusterConfig = spikeConfig;
                    clusterConfig.particleAlpha = 0.3f;  // More transparent for clusters
                    clusterConfig.glowIntensity = 0.5f;  // Less glow for clusters
                    spikeRenderer.renderSpikeParticles(clusterSpheres, camera, clusterConfig);
                }

                // Render spike particles
                spikeRenderer.renderSpikeParticles(spikeParticles, camera, spikeConfig);
            }
        }

        // ====================================================================
        // ImGui UI
        // ====================================================================
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
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
        if (ImGui::SliderFloat("Speed", &speed, 0.001f, 100.0f, "%.3fx", ImGuiSliderFlags_Logarithmic)) {
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
        ImGui::Text("Recording Statistics:");
        ImGui::Text("  Active particles: %zu", particles.size());
        ImGui::Text("  Unique neurons: %zu", neuronActivityCount.size());
        ImGui::Text("  Total spikes: %lu", metadata.spikeCount);

        // Network structure info
        if (hasNetworkStructure) {
            ImGui::Separator();
            ImGui::Text("Network Structure:");
            ImGui::Text("  Neurons: %zu", networkNeurons.size());
            ImGui::Text("  Synapses: %zu", networkSynapses.size());
        }

        // Camera info
        ImGui::Separator();
        ImGui::Text("Camera:");
        glm::vec3 camPos = camera.getPosition();
        glm::vec3 camTarget = camera.getTarget();
        ImGui::Text("  Position: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
        ImGui::Text("  Target: (%.1f, %.1f, %.1f)", camTarget.x, camTarget.y, camTarget.z);
        ImGui::Text("  Distance: %.1f", camera.getDistance());

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

        // Controls help
        ImGui::Separator();
        ImGui::Text("Camera Controls:");
        ImGui::BulletText("Left Mouse Drag: Orbit");
        ImGui::BulletText("Mouse Wheel: Zoom");
        ImGui::BulletText("WASD: Pan");
        ImGui::BulletText("Q/E: Zoom in/out");

        ImGui::End();

        // Visualization Options Window
        if (hasNetworkStructure) {
            ImGui::SetNextWindowPos(ImVec2(470, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
            ImGui::Begin("Visualization Options");

            ImGui::Text("Cluster Identification:");
            ImGui::Separator();

            ImGui::Checkbox("Show Cluster Labels", &showClusterLabels);
            ImGui::Checkbox("Show Bounding Boxes", &showClusterBoundingBoxes);

            ImGui::Separator();
            ImGui::Text("Neuron Coloring:");

            if (ImGui::RadioButton("Color by Cluster", colorByCluster)) {
                colorByCluster = true;
                colorByLayer = false;
                colorByActivity = false;
            }
            if (ImGui::RadioButton("Color by Layer", colorByLayer)) {
                colorByCluster = false;
                colorByLayer = true;
                colorByActivity = false;
            }
            if (ImGui::RadioButton("Color by Activity", colorByActivity)) {
                colorByCluster = false;
                colorByLayer = false;
                colorByActivity = true;
            }

            ImGui::Separator();
            ImGui::Text("Rendering Options:");
            ImGui::Checkbox("Show Cluster Spheres", &showClusterSpheres);
            if (showClusterSpheres) {
                ImGui::SliderFloat("Cluster Sphere Scale", &clusterSphereScale, 0.1f, 3.0f, "%.1f");
            }
            ImGui::Checkbox("Show Neurons", &showNeurons);
            ImGui::Checkbox("Show Synapses", &showSynapses);
            if (showSynapses && networkSynapses.size() > 100000) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                                  "  Warning: %zu synapses may be slow!",
                                  networkSynapses.size());
            }

            ImGui::Separator();
            ImGui::Text("Label Settings:");
            ImGui::SliderFloat("Label Scale", &labelScale, 0.5f, 3.0f, "%.1f");

            ImGui::Separator();
            ImGui::Text("Cluster Information:");
            ImGui::Text("Total Groups: %zu", networkAdapter.getGroups().size());

            // List clusters with neuron counts
            ImGui::BeginChild("ClusterList", ImVec2(0, 200), true);
            for (const auto& group : networkAdapter.getGroups()) {
                if (group.typeName == "Cluster") {
                    ImGui::Text("%s: %zu neurons",
                               group.name.c_str(),
                               group.neuronIds.size());
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("ID: %lu", group.id);
                        ImGui::Text("Center: (%.1f, %.1f, %.1f)",
                                   group.centerPosition.x,
                                   group.centerPosition.y,
                                   group.centerPosition.z);
                        ImGui::EndTooltip();
                    }
                }
            }
            ImGui::EndChild();

            // Selected cluster hierarchical information
            ImGui::Separator();
            ImGui::Text("Selected Cluster:");
            if (selectedClusterIndex >= 0 && selectedClusterIndex < static_cast<int>(networkAdapter.getGroups().size())) {
                const auto& groups = networkAdapter.getGroups();
                const auto& selectedGroup = groups[selectedClusterIndex];

                if (selectedGroup.typeName == "Cluster") {
                    ImGui::Text("  Name: %s", selectedGroup.name.c_str());
                    ImGui::Text("  Neurons: %zu", selectedGroup.neuronIds.size());

                    // Show spike filtering status
                    if (!visibleClusterIds.empty()) {
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                                          "  Spike Filter: Active (%zu clusters visible)",
                                          visibleClusterIds.size());
                    }

                    // Build and display hierarchical path
                    auto path = buildHierarchicalPath(selectedGroup);

                    ImGui::Separator();
                    ImGui::Text("Hierarchical Path:");
                    for (const auto& [typeName, name] : path) {
                        if (!name.empty()) {
                            ImGui::BulletText("%s: %s", typeName.c_str(), name.c_str());
                        } else {
                            ImGui::BulletText("%s: (unnamed)", typeName.c_str());
                        }
                    }

                    if (ImGui::Button("Clear Selection")) {
                        selectedClusterIndex = -1;
                        updateVisibleClusters();  // Update visible clusters when clearing selection
                    }
                }
            } else {
                ImGui::TextWrapped("Click on a cluster label to view its hierarchical information.");
            }

            ImGui::Separator();
            ImGui::TextWrapped("Tip: Zoom in close to see individual neurons and their connections. "
                              "Click cluster labels to see their hierarchical path.");

            ImGui::End();
        }

        // Render cluster labels as 2D overlay (project 3D positions to screen)
        if (hasNetworkStructure && showClusterLabels) {
            // Get view and projection matrices
            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 projection = camera.getProjectionMatrix(800.0f / 600.0f);
            glm::mat4 viewProj = projection * view;

            // Get viewport size
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);

            // Use ImGui overlay for labels
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(display_w, display_h));
            ImGui::Begin("ClusterLabels", nullptr,
                        ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoInputs |
                        ImGuiWindowFlags_NoBackground);

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const auto& groups = networkAdapter.getGroups();
            for (size_t groupIdx = 0; groupIdx < groups.size(); ++groupIdx) {
                const auto& group = groups[groupIdx];
                if (group.typeName == "Cluster" && !group.neuronIds.empty()) {
                    bool isSelected = (selectedClusterIndex >= 0 && groupIdx == static_cast<size_t>(selectedClusterIndex));
                    // Project 3D center position to screen space
                    glm::vec4 clipPos = viewProj * glm::vec4(
                        group.centerPosition.x,
                        group.centerPosition.y,
                        group.centerPosition.z,
                        1.0f
                    );

                    // Perspective divide
                    if (clipPos.w > 0.0f) {  // Only show if in front of camera
                        glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;

                        // Check if in view frustum
                        if (ndcPos.x >= -1.0f && ndcPos.x <= 1.0f &&
                            ndcPos.y >= -1.0f && ndcPos.y <= 1.0f &&
                            ndcPos.z >= -1.0f && ndcPos.z <= 1.0f) {

                            // Convert to screen coordinates
                            float screenX = (ndcPos.x + 1.0f) * 0.5f * display_w;
                            float screenY = (1.0f - ndcPos.y) * 0.5f * display_h;  // Flip Y

                            // Calculate distance-based alpha (fade out when far)
                            float distance = glm::length(glm::vec3(
                                group.centerPosition.x,
                                group.centerPosition.y,
                                group.centerPosition.z
                            ) - camera.getPosition());
                            float alpha = glm::clamp(1.0f - (distance / 200.0f), 0.3f, 1.0f);

                            // Draw label with background
                            ImVec2 textPos(screenX, screenY);
                            std::string label = group.name;
                            ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
                            textSize.x *= labelScale;
                            textSize.y *= labelScale;

                            // Background rectangle (highlight if selected)
                            ImVec2 bgMin(textPos.x - 4, textPos.y - 2);
                            ImVec2 bgMax(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2);

                            if (isSelected) {
                                // Selected: bright cyan background with border
                                drawList->AddRectFilled(bgMin, bgMax,
                                    IM_COL32(0, 150, 200, (int)(220 * alpha)));
                                drawList->AddRect(bgMin, bgMax,
                                    IM_COL32(0, 255, 255, (int)(255 * alpha)), 0.0f, 0, 2.0f);
                            } else {
                                // Normal: dark background
                                drawList->AddRectFilled(bgMin, bgMax,
                                    IM_COL32(0, 0, 0, (int)(180 * alpha)));
                            }

                            // Text (white if selected, yellow if not)
                            ImU32 textColor = isSelected ?
                                IM_COL32(255, 255, 255, (int)(255 * alpha)) :
                                IM_COL32(255, 255, 100, (int)(255 * alpha));

                            drawList->AddText(nullptr, 13.0f * labelScale, textPos,
                                textColor, label.c_str());
                        }
                    }
                }
            }

            ImGui::End();
        }
    });

    // Run visualization
    vizManager.run();

    std::cout << "\n=== Playback Complete ===" << std::endl;
    return 0;
}

