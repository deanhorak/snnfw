# Hierarchical Neural Structure Documentation

## Overview

The SNNFW framework provides a complete hierarchical structure for organizing neural networks, mirroring the biological organization of the brain. This hierarchy spans from individual neurons up to complete brain structures.

## Hierarchy Levels

The framework supports 12 levels of organization:

```
Brain
  └─ Hemisphere (Left/Right)
      └─ Lobe (Frontal, Parietal, Temporal, Occipital, etc.)
          └─ Region (Motor Cortex, Visual Cortex, etc.)
              └─ Nucleus (Thalamic nuclei, Basal ganglia, etc.)
                  └─ Column (Cortical columns)
                      └─ Layer (Cortical layers I-VI)
                          └─ Cluster (Groups of neurons)
                              └─ Neuron (Individual neurons)
                                  ├─ Axon (Output)
                                  ├─ Dendrite (Input)
                                  └─ Synapse (Connections)
```

## Structural Components

### 1. Brain
**ID Range:** 1,200,000,000,000,000 - 1,299,999,999,999,999

The top-level container representing a complete brain.

**Contains:** Hemispheres

**Biological Analog:** The entire brain

**Example:**
```cpp
auto brain = factory.createBrain();
brain->addHemisphere(leftHemisphere);
brain->addHemisphere(rightHemisphere);
```

---

### 2. Hemisphere
**ID Range:** 1,100,000,000,000,000 - 1,199,999,999,999,999

Represents a brain hemisphere (typically left and right).

**Contains:** Lobes

**Biological Analog:** Left and right cerebral hemispheres

**Example:**
```cpp
auto leftHemisphere = factory.createHemisphere();
auto rightHemisphere = factory.createHemisphere();
```

---

### 3. Lobe
**ID Range:** 1,000,000,000,000,000 - 1,099,999,999,999,999

Major divisions of the brain.

**Contains:** Regions

**Biological Analog:** Frontal, Parietal, Temporal, Occipital lobes

**Example:**
```cpp
auto frontalLobe = factory.createLobe();
auto parietalLobe = factory.createLobe();
auto temporalLobe = factory.createLobe();
auto occipitalLobe = factory.createLobe();
```

---

### 4. Region
**ID Range:** 900,000,000,000,000 - 999,999,999,999,999

Functional areas within lobes.

**Contains:** Nuclei

**Biological Analog:** Motor cortex, Visual cortex, Auditory cortex, Hippocampus, etc.

**Example:**
```cpp
auto motorCortex = factory.createRegion();
auto visualCortex = factory.createRegion();
auto prefrontalCortex = factory.createRegion();
```

---

### 5. Nucleus
**ID Range:** 800,000,000,000,000 - 899,999,999,999,999

Clusters of neurons with specific functions.

**Contains:** Columns

**Biological Analog:** Thalamic nuclei, Basal ganglia nuclei, Brainstem nuclei

**Example:**
```cpp
auto nucleus = factory.createNucleus();
motorCortex->addNucleus(nucleus);
```

---

### 6. Column
**ID Range:** 700,000,000,000,000 - 799,999,999,999,999

Vertical arrangements of neurons.

**Contains:** Layers

**Biological Analog:** Cortical columns (minicolumns and macrocolumns)

**Example:**
```cpp
auto column = factory.createColumn();
nucleus->addColumn(column);
```

---

### 7. Layer
**ID Range:** 600,000,000,000,000 - 699,999,999,999,999

Horizontal stratifications of neurons.

**Contains:** Clusters

**Biological Analog:** Cortical layers I-VI

**Example:**
```cpp
// Create 6 cortical layers
for (int i = 1; i <= 6; ++i) {
    auto layer = factory.createLayer();
    column->addLayer(layer);
}
```

---

### 8. Cluster
**ID Range:** 500,000,000,000,000 - 599,999,999,999,999

Groups of functionally related neurons.

**Contains:** Neurons

**Biological Analog:** Neuronal assemblies, cell groups

**Example:**
```cpp
auto cluster = factory.createCluster();
layer->addCluster(cluster);
```

---

### 9. Neuron
**ID Range:** 100,000,000,000,000 - 199,999,999,999,999

Individual neurons with temporal pattern recognition.

**Contains:** N/A (cellular level)

**Biological Analog:** Individual neurons

**Example:**
```cpp
auto neuron = factory.createNeuron(50.0, 0.95, 20);
cluster->addNeuron(neuron);
```

---

### 10. Axon
**ID Range:** 200,000,000,000,000 - 299,999,999,999,999

Output structure of a neuron.

**Biological Analog:** Axon terminal

**Example:**
```cpp
auto axon = factory.createAxon(neuron->getId());
```

---

### 11. Dendrite
**ID Range:** 300,000,000,000,000 - 399,999,999,999,999

Input structure of a neuron.

**Biological Analog:** Dendritic tree

**Example:**
```cpp
auto dendrite = factory.createDendrite(neuron->getId());
```

---

### 12. Synapse
**ID Range:** 400,000,000,000,000 - 499,999,999,999,999

Connection between neurons.

**Biological Analog:** Synaptic junction

**Example:**
```cpp
auto synapse = factory.createSynapse(axon->getId(), dendrite->getId(), 1.0, 1.5);
```

---

## Complete Example

Here's a complete example building a simplified brain structure:

```cpp
#include "snnfw/NeuralObjectFactory.h"

NeuralObjectFactory factory;

// Create brain
auto brain = factory.createBrain();

// Create hemispheres
auto leftHemisphere = factory.createHemisphere();
auto rightHemisphere = factory.createHemisphere();
brain->addHemisphere(leftHemisphere->getId());
brain->addHemisphere(rightHemisphere->getId());

// Create lobes in left hemisphere
auto frontalLobe = factory.createLobe();
auto parietalLobe = factory.createLobe();
leftHemisphere->addLobe(frontalLobe->getId());
leftHemisphere->addLobe(parietalLobe->getId());

// Create regions in frontal lobe
auto motorCortex = factory.createRegion();
auto prefrontalCortex = factory.createRegion();
frontalLobe->addRegion(motorCortex->getId());
frontalLobe->addRegion(prefrontalCortex->getId());

// Create nuclei in motor cortex
auto nucleus = factory.createNucleus();
motorCortex->addNucleus(nucleus->getId());

// Create columns in nucleus
for (int i = 0; i < 10; ++i) {
    auto column = factory.createColumn();
    nucleus->addColumn(column->getId());

    // Create 6 layers per column (like cortical layers I-VI)
    for (int j = 1; j <= 6; ++j) {
        auto layer = factory.createLayer();
        column->addLayer(layer->getId());

        // Create clusters in each layer
        for (int k = 0; k < 5; ++k) {
            auto cluster = factory.createCluster();
            layer->addCluster(cluster->getId());

            // Create neurons in each cluster
            for (int n = 0; n < 100; ++n) {
                auto neuron = factory.createNeuron(50.0, 0.95, 20);
                cluster->addNeuron(neuron);
            }
        }
    }
}

// This creates:
// - 1 brain
// - 2 hemispheres
// - 2 lobes (in left hemisphere)
// - 2 regions
// - 1 nucleus
// - 10 columns
// - 60 layers (6 per column)
// - 300 clusters (5 per layer)
// - 30,000 neurons (100 per cluster)
```

## Biological Correspondence

### Human Brain Structure

| Level | Human Brain Example | Typical Count |
|-------|---------------------|---------------|
| Brain | Entire brain | 1 |
| Hemisphere | Left/Right | 2 |
| Lobe | Frontal, Parietal, Temporal, Occipital | 4-5 per hemisphere |
| Region | Motor cortex, V1, Hippocampus | Dozens |
| Nucleus | Thalamic nuclei, Basal ganglia | Hundreds |
| Column | Cortical columns | Millions |
| Layer | Cortical layers I-VI | 6 in neocortex |
| Cluster | Neuronal assemblies | Variable |
| Neuron | Individual neurons | ~86 billion |

### Cortical Organization

The framework particularly supports cortical organization:

```
Cortex (Region)
  └─ Cortical Columns (Column)
      └─ Cortical Layers (Layer)
          ├─ Layer I (Molecular layer)
          ├─ Layer II (External granular layer)
          ├─ Layer III (External pyramidal layer)
          ├─ Layer IV (Internal granular layer)
          ├─ Layer V (Internal pyramidal layer)
          └─ Layer VI (Multiform layer)
```

## Design Principles

### 1. Hierarchical Containment
Each level contains the level below it, creating a tree structure.

### 2. Flexible Organization
Not all levels need to be used. You can create:
- Simple networks: Just Clusters and Neurons
- Medium networks: Layers, Clusters, and Neurons
- Complex networks: Full hierarchy from Brain to Neurons

### 3. Biological Fidelity
The hierarchy mirrors actual brain organization, making it intuitive for neuroscientists.

### 4. Scalability
Each level can contain unlimited children (up to memory limits).

## Use Cases

### Research Networks
Use Clusters and Neurons for small-scale experiments.

### Cortical Simulations
Use Columns, Layers, Clusters, and Neurons to model cortical circuits.

### Whole-Brain Models
Use the complete hierarchy to model entire brain structures.

### Comparative Neuroscience
Model different brain structures (insect, rodent, primate, human) using appropriate levels.

## API Summary

All structural classes provide consistent methods for ID-based storage:

```cpp
// Adding children (by ID)
void add<ChildType>(uint64_t childId);

// Getting child IDs
uint64_t get<ChildType>Id(size_t index) const;

// Removing children (by ID)
bool remove<ChildType>(uint64_t childId);

// Querying
size_t size() const;
void clear();
const std::vector<uint64_t>& get<ChildType>Ids() const;
```

**Example:**
```cpp
// Create objects
auto brain = factory.createBrain();
auto hemisphere = factory.createHemisphere();

// Add by ID
brain->addHemisphere(hemisphere->getId());

// Get ID by index
uint64_t hId = brain->getHemisphereId(0);

// Get all IDs
const auto& ids = brain->getHemisphereIds();

// Remove by ID
brain->removeHemisphere(hId);
```

## See Also

- [NeuralObjectFactory Documentation](NEURAL_OBJECT_FACTORY.md)
- [ID Range Scale Reference](ID_RANGE_SCALE.md)
- [Hierarchical Structure Example](examples/hierarchical_structure_example.cpp)

