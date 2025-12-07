# SNNFW Project Status - Advanced Visualization Branch

**Branch:** `AdvancedVisualization`
**Date:** 2025-12-07
**Previous Branch:** `AdvancedFeatureDetection`

---

## Executive Summary

The SNNFW (Spiking Neural Network Framework) project has successfully achieved **71.93% accuracy** on EMNIST Letters classification using a biologically-inspired hierarchical V1 architecture with 16 cortical columns (8 orientations × 2 frequencies), 6 layers per column, and ~634K synapses. The framework now includes:

1. **Transparent recording/playback system** - Captures and replays spike activity from experiments
2. **3D spike visualization** - Real-time particle-based spike rendering with cluster spheres
3. **Network structure export** - Binary `.snnw` files with full neural hierarchy
4. **Thread-safe spike processing** - Multi-threaded spike delivery with proper synchronization
5. **Streaming playback** - Efficient playback of large recordings (1M+ spikes)

**Current Focus:** Fixing timing synchronization for recording full experiments in non-real-time mode.

---

## Project Architecture Overview

### Core Neural Model
- **Pattern-Based Neurons:** NOT accumulator-based; neurons learn and match temporal spike patterns
- **Temporal Signatures:** Each neuron fires unique multi-spike patterns (1-10 spikes over 0-100ms)
- **Hierarchical Organization:** Brain → Hemisphere → Lobe → Region → Nucleus → Column → Layer → Cluster → Neuron
- **ID-Based Design:** 64-bit unique IDs for all neural objects
- **Datastore:** RocksDB backing with LRU cache (1M objects), JSON serialization

### Network Architecture (71% Accuracy Configuration)
- **24 Cortical Columns:**
  - 16 Gabor filter columns (8 orientations × 2 frequencies)
  - 6 Center-surround columns (3 scales × 2 types: ON/OFF-center)
  - 2 Specialized detectors (top-loop, gap)
- **6-Layer Canonical Microcircuit:**
  - Layer 1: Modulatory (32 neurons/column)
  - Layer 2/3: Superficial pyramidal (448 neurons/column)
  - Layer 4: Granular input (7×7=49 neurons/column)
  - Layer 5: Deep pyramidal output (80 neurons/column)
  - Layer 6: Corticothalamic feedback (32 neurons/column)
- **Connectivity:**
  - Lateral connections: 4.8M synapses (25% connectivity, ±2 neighbors)
  - Recurrent L2/3: 1.2M synapses (25% connectivity, weight 0.3, delay 2ms)
  - Total: ~6.5M synapses
- **Saccades System:** 4 fixations per image (top, bottom, center, full) @ 100ms each
- **Position Encoding:** Optional spatial context neurons (currently disabled in best config)

### Key Hyperparameters (Best Configuration)
```json
{
  "neuron_window": 500ms,
  "neuron_threshold": 0.93,
  "neuron_max_patterns": 500,
  "num_threads": 24,
  "real_time_sync": false,
  "training_examples_per_letter": 800,
  "test_images": 20800
}
```

---

## Recent Achievements

### 1. Transparent Recording System ✅
**Problem Solved:** Need to record spike activity for analysis and playback without modifying experiment code.

**Solution Implemented:**
- `RecordingManager` with streaming mode writes spikes directly to disk
- Direct recording in `NetworkPropagator::fireNeuron()` before axon/synapse checks
- Fixed-size metadata header (512 bytes) allows in-place updates
- Negative timestamp clamping prevents uint64 overflow
- Binary `.snnr` file format: magic number + version + metadata + spike events

**Results:**
- Successfully recorded 68.6M spikes (2.1GB file) from full 71% experiment
- Successfully recorded 1.2M spikes (38MB file) from test run
- Zero memory overhead (streaming mode)
- Transparent to experiments (enabled via JSON config)

### 2. Playback System ✅
**Problem Solved:** Need to replay recorded spike activity for analysis.

**Solution Implemented:**
- Console playback viewer (`playback_viewer`) with text-based progress
- Graphical playback viewer (`simple_playback_viewer`) with ImGui controls
- Playback speed control (0.1x to 100x, logarithmic slider)
- Looping support
- Activity statistics (total spikes, active neurons, top 10 most active)
- Progress bar showing current time / total duration

**Results:**
- Successfully plays back recordings at variable speeds
- Shows real-time statistics
- ImGui window with interactive controls
- Handles large recordings (68.6M spikes) efficiently

### 3. Network Structure Export Foundation ✅
**Problem Solved:** Playback viewer needs network topology to show 3D visualization.

**Solution Implemented:**
- `NetworkStructureExporter` class for saving/loading network structure
- Binary `.snnw` file format for neurons, synapses, layers
- `NetworkDataAdapter::exportNetworkStructure()` method
- Preserves: Position3D, colors, hierarchical context, connectivity

**Status:** Foundation complete but NOT YET INTEGRATED into experiments or playback viewer.

---

## Current Status: 3D Visualization Working

### What Works ✅
- Playback viewer opens graphical window with ImGui controls
- Network structure (`.snnw` files) loaded and rendered as cluster spheres
- 80 cluster spheres rendered instead of 10K individual neurons (performance optimization)
- Spike particles rendered as bright points during playback
- Speed control from 0.001x to 100x (logarithmic slider)
- Cluster selection and highlighting
- Camera controls (orbit, zoom, pan)
- Streaming playback for large recordings (1M+ spikes)
- Rate limiting (max 1000 spikes/frame) for smooth playback

### Current Issues ⚠️
- **Timing synchronization in non-real-time mode**: When running training without real-time sync, the simulation time advances faster than spike delivery threads can complete, causing "out of range" scheduling errors
- **Fix in progress**: Added `waitForDeliveryThreads()` to synchronize delivery before time advancement, but need to verify it works correctly

### Thread Safety Fixes Applied
- Added `spikesMutex_` to protect `spikes` vector in Neuron class
- Added `incomingSpikesMutex_` to protect `incomingSpikes_` deque
- Updated all methods that access shared data structures to use proper locking
- Copy-under-lock pattern for methods that need to iterate over shared data

---

## Key Files Modified in This Session

### Core Framework
| File | Changes |
|------|---------|
| `include/snnfw/Neuron.h` | Added `spikesMutex_` for thread safety, updated `getSpikes()`/`clearSpikes()` |
| `src/Neuron.cpp` | Thread-safe `insertSpike()`, `learnCurrentPattern()`, `getBestSimilarity()` |
| `include/snnfw/SpikeProcessor.h` | Added `waitForDeliveryThreads()` declaration |
| `src/SpikeProcessor.cpp` | Wait for delivery threads in non-real-time mode |

### Visualization
| File | Changes |
|------|---------|
| `experiments/simple_playback_viewer.cpp` | Cluster sphere rendering, streaming playback, speed control 0.001x-100x |
| `src/LayoutEngine.cpp` | Hierarchical grouped layout algorithm |
| `src/NetworkDataAdapter.cpp` | Network structure extraction with bounding boxes |
| `src/RecordingManager.cpp` | Streaming mode for large recordings |

### Training
| File | Changes |
|------|---------|
| `experiments/emnist_letters_training.cpp` | Recording integration, real-time sync disabled |

---

## Next Steps

1. **Verify timing fix works** - Run 5-example test to confirm no "out of range" errors
2. **Record full 71% experiment** - `--record emnist_full_71pct.snnr --examples 800`
3. **Test playback viewer** - Verify cluster spheres and spike visualization work together
4. **Performance optimization** - Profile non-real-time mode to ensure reasonable execution time
