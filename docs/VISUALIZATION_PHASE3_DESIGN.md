# Visualization Phase 3: Activity Visualization - Design

**Date:** 2025-11-23  
**Status:** Design Phase  
**Dependencies:** Phase 1 (Foundation), Phase 2 (Network Structure)

---

## Overview

Phase 3 adds **real-time activity visualization** to the SNNFW visualization subsystem. This phase focuses on visualizing spike events, activity propagation, temporal patterns, and network dynamics.

---

## Goals

1. **Real-time spike visualization** - Show individual spike events as they occur
2. **Activity heatmaps** - Color neurons based on recent activity levels
3. **Spike propagation** - Animate spikes traveling along synapses
4. **Temporal patterns** - Visualize activity over time
5. **Activity recording** - Record and playback activity sequences
6. **Performance** - Maintain 60 FPS with thousands of spikes per second

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  Application Layer                       │
│              (activity_demo.cpp)                         │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│              ActivityVisualizer                          │
│  - Tracks spike events from ActivityMonitor             │
│  - Manages activity history and decay                   │
│  - Computes neuron activity levels                      │
│  - Generates spike particles for rendering              │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                 SpikeRenderer                            │
│  - Renders spike particles (billboards)                 │
│  - Renders spike trails along synapses                  │
│  - Instanced rendering for performance                  │
│  - Particle system for spike effects                    │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│            Activity Shaders                              │
│  - spike_particle.vert/frag - Spike billboards          │
│  - spike_trail.vert/frag - Propagation trails           │
│  - activity_heatmap.vert/frag - Neuron activity colors  │
└─────────────────────────────────────────────────────────┘
```

---

## Component 1: ActivityVisualizer

### Purpose
Track spike events from ActivityMonitor and prepare activity data for visualization.

### Key Features
- **Spike Event Tracking:** Subscribe to ActivityMonitor for real-time spike events
- **Activity History:** Maintain sliding window of recent activity
- **Activity Decay:** Exponential decay of neuron activity over time
- **Spike Particles:** Generate particle data for spike visualization
- **Activity Levels:** Compute normalized activity levels for heatmaps
- **Temporal Aggregation:** Aggregate activity over time windows

### Data Structures

```cpp
struct SpikeParticle {
    glm::vec3 position;        // Current position
    glm::vec3 velocity;        // Movement direction
    glm::vec4 color;           // RGBA color
    float size;                // Particle size
    float lifetime;            // Time since creation (ms)
    float maxLifetime;         // Total lifetime (ms)
    uint64_t synapseId;        // Associated synapse (0 if none)
    float progress;            // Progress along synapse (0-1)
};

struct NeuronActivity {
    uint64_t neuronId;
    float activityLevel;       // 0.0 - 1.0
    uint64_t lastSpikeTime;    // Timestamp of last spike
    uint32_t spikeCount;       // Spikes in current window
    float decayRate;           // Activity decay rate
};

struct ActivityConfig {
    uint64_t historyDuration;  // How long to keep history (ms)
    float decayRate;           // Activity decay per second
    float spikeParticleSize;   // Size of spike particles
    uint64_t particleLifetime; // Particle lifetime (ms)
    bool showPropagation;      // Show spike propagation
    bool showHeatmap;          // Show activity heatmap
    glm::vec4 spikeColor;      // Color for spike particles
};
```

### Key Methods

```cpp
class ActivityVisualizer {
public:
    ActivityVisualizer(ActivityMonitor& monitor, NetworkDataAdapter& adapter);
    
    // Update activity state
    void update(uint64_t currentTime);
    
    // Get activity data
    const std::vector<SpikeParticle>& getSpikeParticles() const;
    const std::vector<NeuronActivity>& getNeuronActivity() const;
    float getNeuronActivityLevel(uint64_t neuronId) const;
    
    // Configuration
    void setConfig(const ActivityConfig& config);
    const ActivityConfig& getConfig() const;
    
    // Recording and playback
    void startRecording();
    void stopRecording();
    void saveRecording(const std::string& filename);
    void loadRecording(const std::string& filename);
    void playback(uint64_t time);
    
    // Statistics
    uint32_t getTotalSpikes() const;
    float getAverageActivityLevel() const;
    uint32_t getActiveNeuronCount() const;
};
```

---

## Component 2: SpikeRenderer

### Purpose
High-performance rendering of spike particles and propagation effects.

### Key Features
- **Billboard Rendering:** Render spike particles as camera-facing quads
- **Instanced Rendering:** Single draw call for all particles
- **Trail Rendering:** Render spike propagation along synapses
- **Particle Effects:** Glow, fade, and motion blur
- **Depth Sorting:** Sort particles for correct alpha blending
- **Culling:** Frustum and distance culling for performance

### Rendering Techniques

1. **Spike Particles (Billboards)**
   - Camera-facing quads
   - Additive blending for glow effect
   - Size attenuation with distance
   - Color based on neuron type

2. **Spike Trails**
   - Line strips along synapses
   - Animated texture coordinates
   - Fade based on distance from spike
   - Color based on synapse weight

3. **Activity Heatmap**
   - Modify neuron colors based on activity
   - Smooth interpolation between colors
   - Configurable color gradient

### Key Methods

```cpp
class SpikeRenderer {
public:
    SpikeRenderer(ShaderManager& shaderManager);
    
    // Initialization
    bool initialize();
    void cleanup();
    
    // Rendering
    void renderSpikeParticles(
        const std::vector<SpikeParticle>& particles,
        const Camera& camera,
        const RenderConfig& config);
    
    void renderSpikeTrails(
        const std::vector<SpikeParticle>& particles,
        const NetworkDataAdapter& adapter,
        const Camera& camera,
        const RenderConfig& config);
    
    void applyActivityHeatmap(
        const std::vector<NeuronActivity>& activity,
        NetworkDataAdapter& adapter);
    
    // Statistics
    RenderStats getStats() const;
};
```

---

## Component 3: Activity Shaders

### Shader 1: spike_particle.vert/frag

**Purpose:** Render spike particles as billboards

**Vertex Shader:**
- Transform particle position to view space
- Generate billboard quad vertices
- Pass color and lifetime to fragment shader

**Fragment Shader:**
- Circular particle shape with soft edges
- Glow effect based on lifetime
- Additive blending for overlapping particles

### Shader 2: spike_trail.vert/frag

**Purpose:** Render spike propagation trails

**Vertex Shader:**
- Position vertices along synapse path
- Animate texture coordinates based on time
- Pass color and progress to fragment shader

**Fragment Shader:**
- Fade based on distance from spike
- Color gradient along trail
- Alpha blending for transparency

### Shader 3: activity_heatmap.vert/frag

**Purpose:** Color neurons based on activity level

**Vertex Shader:**
- Standard neuron vertex transformation
- Pass activity level to fragment shader

**Fragment Shader:**
- Interpolate between base color and activity color
- Smooth color transitions
- Maintain lighting calculations

---

## Integration with Existing Components

### NetworkGraphRenderer Integration

Add activity visualization to existing network rendering:

```cpp
// In NetworkGraphRenderer::render()
if (config.showActivity) {
    // Apply activity heatmap to neuron colors
    spikeRenderer.applyActivityHeatmap(activity, adapter);
    
    // Render neurons with activity colors
    renderNeurons(adapter, camera, config);
    
    // Render spike particles
    spikeRenderer.renderSpikeParticles(particles, camera, config);
    
    // Render spike trails
    if (config.showPropagation) {
        spikeRenderer.renderSpikeTrails(particles, adapter, camera, config);
    }
}
```

### ActivityMonitor Integration

Subscribe to spike events:

```cpp
// In ActivityVisualizer constructor
monitor.setCallback([this](const SpikeEvent& event) {
    onSpikeEvent(event);
});

// In ActivityVisualizer::onSpikeEvent()
void onSpikeEvent(const SpikeEvent& event) {
    // Update neuron activity
    updateNeuronActivity(event.sourceNeuronId);
    updateNeuronActivity(event.targetNeuronId);
    
    // Create spike particle
    if (config_.showPropagation) {
        createSpikeParticle(event);
    }
}
```

---

## Performance Considerations

### Target Performance
- **60 FPS** with 10,000 neurons
- **1,000 spikes/second** visualization
- **10,000 active particles** maximum

### Optimization Strategies
1. **Instanced Rendering:** Single draw call for all particles
2. **Particle Pooling:** Reuse particle objects
3. **Spatial Culling:** Only render visible particles
4. **LOD System:** Reduce particle count at distance
5. **Batch Updates:** Update activity in batches
6. **GPU Sorting:** Sort particles on GPU if needed

---

## Demo Application

### activity_demo.cpp

**Features:**
- Create network with multiple layers
- Generate synthetic spike activity
- Real-time activity visualization
- Interactive controls for activity parameters
- Recording and playback controls

**UI Controls:**
- Activity decay rate slider
- Particle size slider
- Particle lifetime slider
- Show/hide propagation toggle
- Show/hide heatmap toggle
- Recording controls (start/stop/save/load)
- Playback controls (play/pause/speed)

---

## Implementation Plan

### Step 1: ActivityVisualizer (Est. 400 lines)
- Implement spike event tracking
- Implement activity decay
- Implement particle generation
- Implement activity level computation

### Step 2: SpikeRenderer (Est. 400 lines)
- Implement billboard rendering
- Implement trail rendering
- Implement heatmap application
- Implement instanced rendering

### Step 3: Activity Shaders (Est. 150 lines)
- Create spike_particle shaders
- Create spike_trail shaders
- Create activity_heatmap shaders

### Step 4: Integration (Est. 100 lines)
- Integrate with NetworkGraphRenderer
- Update RenderConfig for activity options
- Add activity controls to ImGui

### Step 5: Demo Application (Est. 300 lines)
- Create activity_demo.cpp
- Generate synthetic activity
- Add recording/playback
- Add UI controls

**Total Estimated Lines:** ~1,350 lines

---

## Success Criteria

- [ ] ActivityVisualizer tracks spike events
- [ ] Activity levels decay over time
- [ ] Spike particles are generated
- [ ] SpikeRenderer renders particles
- [ ] Spike trails show propagation
- [ ] Activity heatmap colors neurons
- [ ] Demo runs at 60 FPS
- [ ] Recording and playback work
- [ ] UI controls are functional
- [ ] Documentation is complete

---

**Phase 3 Design: COMPLETE**  
**Ready for Implementation**

