# Recording and Playback Framework Usage Guide

## Overview

The SNNFW framework now supports transparent recording and playback of spike activity with minimal impact on experiment code. Recording and visualization can be enabled/disabled independently for maximum flexibility.

## Four Operational Modes

### 1. Performance Mode (Default)
**Configuration:** `recording=false`, `visualization=false`
- **Use case:** Maximum performance training/testing
- **Overhead:** Zero - no recording or visualization
- **Speed:** Fastest possible execution

### 2. Recording Mode
**Configuration:** `recording=true`, `visualization=false`
- **Use case:** Record training sessions for later analysis
- **Overhead:** Minimal - only spike event recording
- **Speed:** Near-maximum (no visualization overhead)
- **Output:** `.snnr` recording file

### 3. Visualization Mode
**Configuration:** `recording=false`, `visualization=true`
- **Use case:** Live debugging and observation
- **Overhead:** Moderate - OpenGL rendering
- **Speed:** Real-time synchronized (1ms = 1ms)

### 4. Debug Mode
**Configuration:** `recording=true`, `visualization=true`
- **Use case:** Full debugging with live view and recording
- **Overhead:** Maximum - both recording and rendering
- **Speed:** Real-time synchronized
- **Output:** Live visualization + `.snnr` recording file

## Quick Start Examples

### Example 1: Performance Mode (No Recording, No Visualization)

```cpp
#include "snnfw/SimulationConfig.h"
#include "snnfw/ActivityMonitor.h"
#include "snnfw/SpikeProcessor.h"

// Default configuration - performance mode
SimulationConfig config;
// config.enableRecording = false;      // default
// config.enableVisualization = false;  // default
config.realTimeSync = false;  // Disable for maximum speed

// Create components
Datastore datastore("./network_db", 1000000);
ActivityMonitor activityMonitor(datastore, config);
SpikeProcessor spikeProcessor(datastore, config.realTimeSync, config.spikeProcessorThreads);

// Run experiment at maximum speed
activityMonitor.startMonitoring();
spikeProcessor.start();

// ... your training/testing code ...

spikeProcessor.stop();
activityMonitor.stopMonitoring();
```

### Example 2: Recording Mode (Record Without Visualization)

```cpp
#include "snnfw/SimulationConfig.h"
#include "snnfw/ActivityMonitor.h"
#include "snnfw/RecordingManager.h"

// Enable recording, disable visualization
SimulationConfig config;
config.enableRecording = true;
config.recordingFilename = "training_session.snnr";
config.enableVisualization = false;
config.realTimeSync = false;  // Fast mode

// Create components
Datastore datastore("./network_db", 1000000);
ActivityMonitor activityMonitor(datastore);

// Create recording manager and attach to activity monitor
RecordingManager recordingManager;
activityMonitor.setRecordingManager(&recordingManager);

// Start monitoring (automatically starts recording)
activityMonitor.startMonitoring();
SpikeProcessor spikeProcessor(datastore, false, 20);
spikeProcessor.start();

// ... your training/testing code ...

// Stop and save recording
spikeProcessor.stop();
activityMonitor.saveRecording(config.recordingFilename);
activityMonitor.stopMonitoring();

std::cout << "Recording saved to " << config.recordingFilename << std::endl;
```

### Example 3: Visualization Mode (Live View Without Recording)

```cpp
#include "snnfw/SimulationConfig.h"
#include "snnfw/VisualizationManager.h"

// Enable visualization, disable recording
SimulationConfig config;
config.enableRecording = false;
config.enableVisualization = true;
config.visualizationWidth = 1920;
config.visualizationHeight = 1080;
config.realTimeSync = true;  // Required for smooth visualization

// Create components
Datastore datastore("./network_db", 1000000);
ActivityMonitor activityMonitor(datastore);
SpikeProcessor spikeProcessor(datastore, true, 20);

// Create visualization
VisualizationManager vizManager(config.visualizationWidth, 
                                config.visualizationHeight,
                                config.visualizationTitle);

// ... set up network data adapter, camera, renderer ...

// Run with live visualization
activityMonitor.startMonitoring();
spikeProcessor.start();
vizManager.run();  // Blocks until window closed
```

### Example 4: Debug Mode (Recording + Visualization)

```cpp
// Enable both recording and visualization
SimulationConfig config;
config.enableRecording = true;
config.recordingFilename = "debug_session.snnr";
config.enableVisualization = true;
config.realTimeSync = true;

// Create components
Datastore datastore("./network_db", 1000000);
ActivityMonitor activityMonitor(datastore);
RecordingManager recordingManager;
activityMonitor.setRecordingManager(&recordingManager);

// Create visualization
VisualizationManager vizManager(config.visualizationWidth,
                                config.visualizationHeight,
                                "Debug Mode");

// ... set up visualization ...

// Run with both features
activityMonitor.startMonitoring();
SpikeProcessor spikeProcessor(datastore, true, 20);
spikeProcessor.start();
vizManager.run();

// Save recording when done
activityMonitor.saveRecording(config.recordingFilename);
```

### Example 5: Playback Mode (Replay Recorded Session)

```cpp
#include "snnfw/RecordingManager.h"
#include "snnfw/VisualizationManager.h"

// Configure playback mode
SimulationConfig config;
config.playbackMode = true;
config.playbackFilename = "training_session.snnr";
config.playbackSpeed = 1.0f;  // Normal speed
config.playbackLooping = false;

// Create visualization for playback
VisualizationManager vizManager(1920, 1080, "Playback Mode");

// Load recording
RecordingManager recordingManager;
if (!recordingManager.loadRecording(config.playbackFilename)) {
    std::cerr << "Failed to load recording!" << std::endl;
    return 1;
}

// Set up playback callback to update visualization
recordingManager.setPlaybackCallback([&](uint64_t sourceId, uint64_t targetId,
                                         uint64_t synapseId, uint64_t timestamp) {
    // Update visualization with spike event
    // This will be called for each spike during playback
});

// Set up ImGui controls in visualization update callback
vizManager.setUpdateCallback([&](double deltaTime) {
    // Update playback
    recordingManager.update(static_cast<uint64_t>(deltaTime * 1000.0));

    // ImGui playback controls
    ImGui::Begin("Playback Controls");

    auto& state = recordingManager.getPlaybackState();

    // Play/Pause button
    if (state.playing && !state.paused) {
        if (ImGui::Button("Pause")) {
            recordingManager.pause();
        }
    } else {
        if (ImGui::Button("Play")) {
            recordingManager.play();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        recordingManager.stop();
    }

    // Speed control
    float speed = state.speed;
    if (ImGui::SliderFloat("Speed", &speed, 0.1f, 10.0f)) {
        recordingManager.setSpeed(speed);
    }

    // Timeline scrubber
    uint64_t currentTime = state.currentTime;
    if (ImGui::SliderScalar("Time", ImGuiDataType_U64, &currentTime,
                            &state.startTime, &state.endTime)) {
        recordingManager.seek(currentTime);
    }

    // Looping checkbox
    bool looping = state.looping;
    if (ImGui::Checkbox("Loop", &looping)) {
        recordingManager.setLooping(looping);
    }

    // Display metadata
    auto& metadata = recordingManager.getMetadata();
    ImGui::Text("Duration: %llu ms", metadata.duration);
    ImGui::Text("Spikes: %zu", metadata.spikeCount);
    ImGui::Text("Neurons: %zu", metadata.neuronCount);

    ImGui::End();
});

// Run playback visualization
recordingManager.play();
vizManager.run();
```

## Integration with Existing Experiments

### Minimal Changes Required

The framework is designed to require minimal changes to existing experiment code:

**Before (no recording/visualization):**
```cpp
Datastore datastore("./db", 1000000);
ActivityMonitor monitor(datastore);
SpikeProcessor processor(datastore, false, 20);

// ... experiment code ...
```

**After (with optional recording/visualization):**
```cpp
SimulationConfig config;
config.enableRecording = true;  // or false
config.enableVisualization = true;  // or false

Datastore datastore("./db", 1000000);
ActivityMonitor monitor(datastore, config);  // Pass config
SpikeProcessor processor(datastore, config.realTimeSync, config.spikeProcessorThreads);

// Optional: Set up recording if enabled
RecordingManager* recordingMgr = nullptr;
if (config.enableRecording) {
    recordingMgr = new RecordingManager();
    monitor.setRecordingManager(recordingMgr);
}

// ... experiment code (unchanged) ...

// Save recording if enabled
if (recordingMgr) {
    monitor.saveRecording(config.recordingFilename);
    delete recordingMgr;
}
```

## Command-Line Integration

Recommended pattern for experiment executables:

```cpp
int main(int argc, char** argv) {
    SimulationConfig config;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--record" && i + 1 < argc) {
            config.enableRecording = true;
            config.recordingFilename = argv[++i];
        }
        else if (arg == "--visualize") {
            config.enableVisualization = true;
        }
        else if (arg == "--playback" && i + 1 < argc) {
            config.playbackMode = true;
            config.playbackFilename = argv[++i];
        }
        else if (arg == "--no-realtime") {
            config.realTimeSync = false;
        }
    }

    // Validate configuration
    if (!config.validate()) {
        std::cerr << "Invalid configuration!" << std::endl;
        return 1;
    }

    // Run experiment with configuration
    // ...
}
```

**Usage examples:**
```bash
# Performance mode (fastest)
./my_experiment --no-realtime

# Record training session
./my_experiment --record training.snnr --no-realtime

# Live visualization
./my_experiment --visualize

# Record + visualize
./my_experiment --record debug.snnr --visualize

# Playback recorded session
./my_experiment --playback training.snnr
```

## Performance Characteristics

| Mode | Recording | Visualization | Real-Time Sync | Relative Speed |
|------|-----------|---------------|----------------|----------------|
| Performance | ❌ | ❌ | ❌ | 100% (fastest) |
| Recording | ✅ | ❌ | ❌ | ~95% |
| Visualization | ❌ | ✅ | ✅ | ~10-20% (real-time) |
| Debug | ✅ | ✅ | ✅ | ~10-20% (real-time) |
| Playback | N/A | ✅ | ✅ | Variable (0.1x - 10x) |

## File Format

Recording files (`.snnr`) are binary files containing:
- Magic number: `SNNR`
- Version: `1`
- Metadata (JSON): name, timestamps, spike count, neuron count
- Spike events: timestamp, source neuron ID, target neuron ID, synapse ID

Files are portable and can be shared between systems.

## Next Steps

1. Implement playback mode in VisualizationManager
2. Add ImGui playback controls
3. Update experiment code to use SimulationConfig
4. Test all four modes



