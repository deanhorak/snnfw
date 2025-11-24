# SNNFW Visualization Phase 2: Network Structure Visualization - Design

**Date:** 2025-11-23  
**Phase:** 2 (Network Structure Visualization)  
**Status:** Design Complete, Implementation In Progress

---

## Overview

Phase 2 adds 3D network structure visualization to the SNNFW visualization subsystem. This phase enables users to visualize the complete neural hierarchy (Brain → Hemisphere → Lobe → Region → Nucleus → Column → Layer → Cluster → Neuron) with interactive controls and real-time activity integration.

---

## Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                   Visualization Layer                        │
├─────────────────────────────────────────────────────────────┤
│  NetworkGraphRenderer                                        │
│  - Instanced neuron rendering                               │
│  - Batch synapse rendering                                  │
│  - LOD system                                               │
│  - Selection & highlighting                                 │
└────────────┬────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────┐
│  LayoutEngine                                                │
│  - Hierarchical tree layout                                 │
│  - Force-directed layout                                    │
│  - Grid/Circular/Layered layouts                           │
│  - Anatomical layout                                        │
└────────────┬────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────┐
│  NetworkDataAdapter                                          │
│  - Extract network structure                                │
│  - Integrate activity data                                  │
│  - Provide visual representation                            │
└────────────┬────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────┐
│                   SNNFW Core Layer                           │
├─────────────────────────────────────────────────────────────┤
│  Datastore  │  NetworkInspector  │  ActivityMonitor         │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Specifications

### 1. NetworkDataAdapter

**Purpose:** Bridge between SNNFW data structures and visualization system

**Key Features:**
- Extracts neurons and synapses from Datastore
- Computes hierarchical context for each neuron
- Integrates real-time activity from ActivityMonitor
- Provides visual representation data (position, color, size)
- Supports filtering by hierarchical level
- Caches data for performance

**Data Structures:**

```cpp
struct NeuronVisualData {
    uint64_t id;
    Position3D position;
    float radius;
    float r, g, b, a;  // Color
    float activity;     // 0.0 to 1.0
    bool isExcitatory;
    // Hierarchical context
    uint64_t clusterId, layerId, columnId, nucleusId,
             regionId, lobeId, hemisphereId, brainId;
};

struct SynapseVisualData {
    uint64_t id;
    uint64_t sourceNeuronId, targetNeuronId;
    Position3D sourcePos, targetPos;
    float weight, thickness;
    float r, g, b, a;  // Color
    float activity;
};

struct HierarchicalGroup {
    uint64_t id;
    std::string typeName;  // "Cluster", "Layer", etc.
    std::string name;
    Position3D centerPosition;
    Position3D boundingBoxMin, boundingBoxMax;
    std::vector<uint64_t> neuronIds;
    std::vector<uint64_t> childGroupIds;
    uint64_t parentGroupId;
};
```

**API:**

```cpp
NetworkDataAdapter adapter(datastore, inspector, activityMonitor);

// Extract network
adapter.extractNetwork(brainId);
adapter.extractHierarchy(lobeId, "Lobe");

// Update activity
adapter.updateActivity(startTime, endTime);

// Get data
auto neurons = adapter.getNeurons();
auto synapses = adapter.getSynapses();
auto groups = adapter.getGroups();

// Filter
auto clusterNeurons = adapter.getNeuronsByLevel("Cluster", clusterId);
auto neuronSynapses = adapter.getSynapsesByNeuron(neuronId);

// Configure colors
adapter.setColorScheme(0.2f, 0.5f, 1.0f,  // Excitatory (blue)
                      1.0f, 0.2f, 0.2f);  // Inhibitory (red)
```

---

### 2. LayoutEngine

**Purpose:** Compute spatial positions for neurons that preserve hierarchical structure

**Supported Algorithms:**

1. **Hierarchical Tree Layout** (Default)
   - Organizes as tree: Brain at top, neurons at bottom
   - Preserves parent-child relationships
   - Configurable spacing at each level
   - Good for understanding structure

2. **Force-Directed Layout**
   - Physics-based simulation
   - Synapses = springs, neurons = repelling particles
   - Reveals connectivity patterns
   - Iterative convergence

3. **Grid Layout**
   - Regular grid arrangement
   - Simple and predictable
   - Good for small networks

4. **Circular Layout**
   - Radial arrangement
   - Concentric circles for layers
   - Good for symmetric networks

5. **Layered Layout**
   - Sugiyama-style layered graph
   - Minimizes edge crossings
   - Good for feedforward networks

6. **Anatomical Layout**
   - Biologically-inspired positioning
   - Preserves topological relationships
   - Good for realistic networks

**Configuration:**

```cpp
LayoutConfig config;
config.algorithm = LayoutAlgorithm::HIERARCHICAL_TREE;
config.neuronSpacing = 1.0f;
config.clusterSpacing = 5.0f;
config.layerSpacing = 10.0f;
config.columnSpacing = 15.0f;
config.nucleusSpacing = 25.0f;
config.regionSpacing = 40.0f;
config.lobeSpacing = 60.0f;
config.hemisphereSpacing = 100.0f;
config.centerLayout = true;
```

**API:**

```cpp
LayoutEngine layout;
layout.computeLayout(adapter, config);

// Or use specific algorithm
layout.computeHierarchicalTreeLayout(adapter, config);
layout.computeForceDirectedLayout(adapter, config);

// Utilities
Position3D min, max;
layout.getBoundingBox(min, max);
layout.centerLayout(adapter);
layout.normalizePositions(adapter, 100.0f);

// Progress tracking
layout.setProgressCallback([](float progress) {
    std::cout << "Layout: " << (progress * 100) << "%\n";
});
```

---

### 3. NetworkGraphRenderer

**Purpose:** High-performance OpenGL rendering of neural networks

**Key Features:**
- **Instanced Rendering:** Single draw call for all neurons
- **LOD System:** 4 levels (Full, Medium, Low, Minimal)
- **Culling:** Frustum and distance-based
- **Selection:** Highlighting with glow effect
- **Activity Visualization:** Color and size based on activity
- **Hierarchical Groups:** Bounding box rendering

**Rendering Modes:**
- Neurons only
- Synapses only
- Neurons and synapses
- Hierarchical groups

**LOD Levels:**
- **Full:** High-poly spheres (32 segments), curved synapses
- **Medium:** Medium-poly spheres (16 segments), straight lines
- **Low:** Low-poly spheres (8 segments), thin lines
- **Minimal:** Points for neurons, no synapses

**Performance Targets:**
- 1K neurons: 60 FPS, Full LOD
- 10K neurons: 60 FPS, Medium LOD
- 100K neurons: 30 FPS, Low LOD
- 1M neurons: 15 FPS, Minimal LOD

**Configuration:**

```cpp
RenderConfig config;
config.mode = RenderMode::NEURONS_AND_SYNAPSES;
config.lodLevel = LODLevel::FULL;

// Neurons
config.renderNeurons = true;
config.neuronBaseRadius = 0.5f;
config.neuronActivityScale = 0.5f;
config.neuronUseInstancing = true;

// Synapses
config.renderSynapses = true;
config.synapseBaseThickness = 0.1f;
config.synapseCurved = false;

// Culling
config.frustumCulling = true;
config.distanceCulling = true;
config.maxRenderDistance = 1000.0f;

// LOD thresholds
config.lodFullDistance = 50.0f;
config.lodMediumDistance = 100.0f;
config.lodLowDistance = 200.0f;

// Selection
config.enableSelection = true;
config.selectionColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
```

**API:**

```cpp
NetworkGraphRenderer renderer(shaderManager);
renderer.initialize();

// Render
renderer.render(adapter, camera, config);

// Or render components separately
renderer.renderNeurons(adapter, camera, config);
renderer.renderSynapses(adapter, camera, config);
renderer.renderGroupBoundaries(adapter, camera, config);

// Selection
std::set<uint64_t> selected = {neuron1Id, neuron2Id};
renderer.setSelectedNeurons(selected);
renderer.clearSelection();

// Stats
auto stats = renderer.getStats();
std::cout << "Neurons rendered: " << stats.neuronsRendered << "\n";
std::cout << "Draw calls: " << stats.drawCalls << "\n";
std::cout << "Render time: " << stats.renderTimeMs << " ms\n";
```

---

## Shaders

### Neuron Shader (Instanced)

**Vertex Shader (`network_neuron.vert`):**
```glsl
#version 450 core

layout (location = 0) in vec3 aPos;      // Sphere vertex position
layout (location = 1) in vec3 aNormal;   // Sphere vertex normal

// Instance attributes
layout (location = 2) in vec3 aInstancePos;    // Neuron position
layout (location = 3) in vec4 aInstanceColor;  // Neuron color
layout (location = 4) in float aInstanceRadius; // Neuron radius

uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec4 Color;

void main() {
    // Scale and translate sphere to neuron position
    vec3 worldPos = aInstancePos + aPos * aInstanceRadius;
    FragPos = worldPos;
    Normal = aNormal;
    Color = aInstanceColor;
    
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
```

**Fragment Shader (`network_neuron.frag`):**
```glsl
#version 450 core

in vec3 FragPos;
in vec3 Normal;
in vec4 Color;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform bool isSelected;
uniform vec4 selectionColor;
uniform float selectionGlow;

out vec4 FragColor;

void main() {
    // Phong lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    
    vec3 ambient = 0.3 * Color.rgb;
    vec3 diffuse = diff * Color.rgb;
    vec3 specular = 0.5 * spec * vec3(1.0);
    
    vec3 result = ambient + diffuse + specular;
    
    // Selection glow
    if (isSelected) {
        result = mix(result, selectionColor.rgb, selectionGlow);
    }
    
    FragColor = vec4(result, Color.a);
}
```

### Synapse Shader

**Vertex Shader (`network_synapse.vert`):**
```glsl
#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 view;
uniform mat4 projection;

out vec4 Color;

void main() {
    Color = aColor;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
```

**Fragment Shader (`network_synapse.frag`):**
```glsl
#version 450 core

in vec4 Color;
out vec4 FragColor;

void main() {
    FragColor = Color;
}
```

---

## Implementation Plan

### Step 1: Implement NetworkDataAdapter ✓ (Design Complete)
- [x] Design API
- [ ] Implement extraction methods
- [ ] Implement activity integration
- [ ] Implement filtering methods
- [ ] Add color mapping
- [ ] Test with sample network

### Step 2: Implement LayoutEngine ✓ (Design Complete)
- [x] Design API
- [ ] Implement hierarchical tree layout
- [ ] Implement force-directed layout
- [ ] Implement grid layout
- [ ] Implement circular layout
- [ ] Implement utility functions
- [ ] Test layouts with various network sizes

### Step 3: Implement NetworkGraphRenderer ✓ (Design Complete)
- [x] Design API
- [ ] Create shaders
- [ ] Implement instanced neuron rendering
- [ ] Implement synapse rendering
- [ ] Implement LOD system
- [ ] Implement culling
- [ ] Implement selection
- [ ] Test performance

### Step 4: Create Demo Application
- [ ] Build network visualization demo
- [ ] Add ImGui controls for layout selection
- [ ] Add ImGui controls for rendering options
- [ ] Add selection with mouse picking
- [ ] Add camera controls
- [ ] Test with real SNNFW networks

### Step 5: Documentation and Testing
- [ ] Write user guide
- [ ] Create API documentation
- [ ] Write unit tests
- [ ] Performance benchmarks
- [ ] Update main documentation

---

## Success Criteria

Phase 2 is complete when:

- [ ] NetworkDataAdapter extracts network structure correctly
- [ ] LayoutEngine computes hierarchical positions
- [ ] NetworkGraphRenderer renders neurons and synapses
- [ ] Demo application runs at 60 FPS for 1K neurons
- [ ] Selection and highlighting work
- [ ] Activity visualization updates in real-time
- [ ] Documentation is complete
- [ ] All tests pass

---

## Next Steps

1. Implement NetworkDataAdapter (extraction and activity integration)
2. Implement LayoutEngine (hierarchical tree layout first)
3. Create network visualization shaders
4. Implement NetworkGraphRenderer (instanced rendering)
5. Build demo application
6. Test and optimize

---

**Design Status:** Complete ✅  
**Implementation Status:** Ready to begin  
**Estimated Time:** 3-4 days


