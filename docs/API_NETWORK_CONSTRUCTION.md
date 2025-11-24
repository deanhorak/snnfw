# Network Construction API Documentation

**Version:** 1.0  
**Date:** 2025-11-22  
**Purpose:** Comprehensive guide to building neural networks using NetworkBuilder and ConnectivityPattern APIs

---

## Table of Contents

1. [Overview](#overview)
2. [NetworkBuilder API](#networkbuilder-api)
3. [ConnectivityPattern API](#connectivitypattern-api)
4. [ConnectivityBuilder API](#connectivitybuilder-api)
5. [Examples](#examples)
6. [Best Practices](#best-practices)

---

## Overview

The SNNFW framework provides two main APIs for constructing neural networks:

1. **NetworkBuilder** - Fluent API for building hierarchical neural structures
2. **ConnectivityPattern** - Reusable connectivity generators for creating synaptic connections

These APIs work together to simplify the construction of complex, biologically-inspired neural networks.

---

## NetworkBuilder API

### Purpose

NetworkBuilder provides a fluent interface for constructing hierarchical neural networks following the brain's anatomical organization:

```
Brain → Hemisphere → Lobe → Region → Nucleus → Column → Layer → Cluster → Neuron
```

### Class: `NetworkBuilder`

**Header:** `#include "snnfw/NetworkBuilder.h"`

#### Constructor

```cpp
NetworkBuilder(NeuralObjectFactory& factory, Datastore& datastore)
```

**Parameters:**
- `factory` - Reference to NeuralObjectFactory for creating neural objects
- `datastore` - Reference to Datastore for persisting objects

**Example:**
```cpp
NeuralObjectFactory factory;
Datastore datastore("./network_db", 1000000);
NetworkBuilder builder(factory, datastore);
```

#### Configuration Methods

##### Set Default Neuron Parameters

```cpp
NetworkBuilder& setDefaultNeuronParams(
    double threshold,
    double decayFactor,
    int windowSizeMs
)
```

**Parameters:**
- `threshold` - Firing threshold (default: 50.0)
- `decayFactor` - Membrane potential decay factor (default: 0.95)
- `windowSizeMs` - Temporal window size in milliseconds (default: 20)

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.setDefaultNeuronParams(45.0, 0.98, 25);
```

##### Set Default Synapse Parameters

```cpp
NetworkBuilder& setDefaultSynapseParams(
    double weight,
    double delay
)
```

**Parameters:**
- `weight` - Synaptic weight (default: 1.0)
- `delay` - Synaptic delay in milliseconds (default: 1.0)

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.setDefaultSynapseParams(0.8, 1.5);
```

#### Hierarchical Construction Methods

##### Create Brain

```cpp
NetworkBuilder& createBrain(const std::string& name)
```

**Parameters:**
- `name` - Name of the brain (e.g., "HumanBrain", "MouseBrain")

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.createBrain("VisualCortexModel");
```

##### Add Hemisphere

```cpp
NetworkBuilder& addHemisphere(const std::string& name)
```

**Parameters:**
- `name` - Name of the hemisphere (e.g., "Left", "Right")

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.addHemisphere("Left");
```

##### Add Lobe

```cpp
NetworkBuilder& addLobe(const std::string& name)
```

**Parameters:**
- `name` - Name of the lobe (e.g., "Occipital", "Frontal", "Parietal", "Temporal")

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.addLobe("Occipital");
```

##### Add Region

```cpp
NetworkBuilder& addRegion(const std::string& name)
```

**Parameters:**
- `name` - Name of the region (e.g., "V1", "V2", "MT", "PrimaryMotorCortex")

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.addRegion("V1");
```

##### Add Nucleus

```cpp
NetworkBuilder& addNucleus(const std::string& name)
```

**Parameters:**
- `name` - Name of the nucleus

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.addNucleus("VisualProcessingUnit");
```

##### Add Column

```cpp
NetworkBuilder& addColumn(const std::string& name)
```

**Parameters:**
- `name` - Name of the column (e.g., "OrientationColumn_0deg", "ColorColumn_Red")

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.addColumn("OrientationColumn_0deg");
```

##### Add Layer

```cpp
NetworkBuilder& addLayer(const std::string& name)
```

**Parameters:**
- `name` - Name of the layer (e.g., "Layer2/3", "Layer4", "Layer5", "Layer6")

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.addLayer("Layer4");
```

##### Add Cluster

```cpp
NetworkBuilder& addCluster(
    const std::string& name,
    size_t neuronCount,
    double threshold = 0,
    double decayFactor = 0,
    int windowSizeMs = 0
)
```

**Parameters:**
- `name` - Name of the cluster
- `neuronCount` - Number of neurons to create in the cluster
- `threshold` - Neuron firing threshold (0 = use default)
- `decayFactor` - Neuron decay factor (0 = use default)
- `windowSizeMs` - Neuron window size (0 = use default)

**Returns:** Reference to builder for method chaining

**Example:**
```cpp
builder.addCluster("ExcitatoryNeurons", 100, 50.0, 0.95, 20);
```

#### Retrieval Methods

```cpp
std::shared_ptr<Brain> getBrain() const
std::shared_ptr<Hemisphere> getCurrentHemisphere() const
std::shared_ptr<Lobe> getCurrentLobe() const
std::shared_ptr<Region> getCurrentRegion() const
std::shared_ptr<Nucleus> getCurrentNucleus() const
std::shared_ptr<Column> getCurrentColumn() const
std::shared_ptr<Layer> getCurrentLayer() const
std::shared_ptr<Cluster> getCurrentCluster() const
```

**Returns:** Shared pointer to the current object at that hierarchical level

**Example:**
```cpp
auto brain = builder.getBrain();
auto layer = builder.getCurrentLayer();
```

#### Build Method

```cpp
std::shared_ptr<Brain> build()
```

**Returns:** Shared pointer to the constructed Brain object

**Example:**
```cpp
auto brain = builder.build();
```

---

## ConnectivityPattern API

### Purpose

ConnectivityPattern provides reusable generators for creating synaptic connections between neurons with various spatial and probabilistic patterns.

### Base Class: `ConnectivityPattern`

**Header:** `#include "snnfw/ConnectivityPattern.h"`

#### Virtual Methods

```cpp
virtual std::vector<Connection> generateConnections(
    const std::vector<uint64_t>& sourceNeurons,
    const std::vector<uint64_t>& targetNeurons
) = 0
```

**Parameters:**
- `sourceNeurons` - Vector of source neuron IDs
- `targetNeurons` - Vector of target neuron IDs

**Returns:** Vector of Connection objects specifying which neurons to connect

#### Seed Control

```cpp
virtual void setSeed(unsigned int seed)
```

**Parameters:**
- `seed` - Random seed for reproducible connection generation

**Example:**
```cpp
pattern.setSeed(42);  // Reproducible connections
```

### Data Structures

#### Connection

```cpp
struct Connection {
    uint64_t sourceNeuronId;
    uint64_t targetNeuronId;
    double weight;
    double delay;
}
```

#### SpatialPosition

```cpp
struct SpatialPosition {
    double x, y, z;
    double distanceTo(const SpatialPosition& other) const;
}
```

### Pattern Classes

#### 1. RandomSparsePattern

Creates random connections with specified probability.

**Constructor:**
```cpp
RandomSparsePattern(
    double probability,
    double weight,
    double delay
)
```

**Parameters:**
- `probability` - Connection probability (0.0 to 1.0)
- `weight` - Synaptic weight for all connections
- `delay` - Synaptic delay in milliseconds

**Example:**
```cpp
RandomSparsePattern pattern(0.2, 1.0, 1.5);  // 20% connectivity
```

**Use Case:** General sparse connectivity, random networks

#### 2. AllToAllPattern

Creates connections from every source neuron to every target neuron.

**Constructor:**
```cpp
AllToAllPattern(
    double weight,
    double delay
)
```

**Parameters:**
- `weight` - Synaptic weight for all connections
- `delay` - Synaptic delay in milliseconds

**Example:**
```cpp
AllToAllPattern pattern(0.5, 1.0);
```

**Use Case:** Fully connected layers, small networks

#### 3. OneToOnePattern

Creates one-to-one connections: source[i] → target[i].

**Constructor:**
```cpp
OneToOnePattern(
    double weight,
    double delay
)
```

**Parameters:**
- `weight` - Synaptic weight for all connections
- `delay` - Synaptic delay in milliseconds

**Throws:** `std::invalid_argument` if source and target sizes don't match

**Example:**
```cpp
OneToOnePattern pattern(1.0, 1.0);
```

**Use Case:** Direct mappings, identity connections

#### 4. ManyToOnePattern

Creates convergent connections: all sources connect to each target.

**Constructor:**
```cpp
ManyToOnePattern(
    double weight,
    double delay
)
```

**Parameters:**
- `weight` - Synaptic weight for all connections
- `delay` - Synaptic delay in milliseconds

**Example:**
```cpp
ManyToOnePattern pattern(0.3, 2.0);
```

**Use Case:** Convergent architectures, pooling layers

#### 5. DistanceDependentPattern

Creates connections with probability based on spatial distance using Gaussian falloff.

**Constructor:**
```cpp
DistanceDependentPattern(
    double sigma,
    double weight,
    double delay
)
```

**Parameters:**
- `sigma` - Distance scale parameter (larger = longer-range connections)
- `weight` - Synaptic weight for all connections
- `delay` - Synaptic delay in milliseconds

**Connection Probability:** P(d) = exp(-d²/(2σ²))

**Spatial Positions Required:** Must call `setNeuronPositions()` before generating connections

**Methods:**
```cpp
void setNeuronPositions(
    const std::map<uint64_t, SpatialPosition>& positions
)
```

**Example:**
```cpp
DistanceDependentPattern pattern(2.0, 1.0, 1.0);

std::map<uint64_t, SpatialPosition> positions;
positions[neuron1->getId()] = {0.0, 0.0, 0.0};
positions[neuron2->getId()] = {1.0, 1.0, 0.0};
pattern.setNeuronPositions(positions);

auto connections = pattern.generateConnections(sources, targets);
```

**Use Case:** Spatially-organized networks, topographic maps

#### 6. TopographicPattern

Preserves spatial relationships by connecting neurons with similar normalized positions.

**Constructor:**
```cpp
TopographicPattern(
    double neighborhoodSize,
    double weight,
    double delay
)
```

**Parameters:**
- `neighborhoodSize` - Maximum distance for connections (0.0 to 1.0)
- `weight` - Synaptic weight for all connections
- `delay` - Synaptic delay in milliseconds

**Position Mapping:** Neurons are mapped to [0,1] based on their index in the source/target arrays

**Example:**
```cpp
TopographicPattern pattern(0.3, 1.0, 1.0);  // Connect within 30% distance
```

**Use Case:** Retinotopic maps, somatotopic maps, preserving spatial structure

#### 7. SmallWorldPattern

Combines local high-probability connections with sparse long-range connections.

**Constructor:**
```cpp
SmallWorldPattern(
    double localProbability,
    double longRangeProbability,
    double localRadius,
    double weight,
    double delay
)
```

**Parameters:**
- `localProbability` - Connection probability for local neurons (0.0 to 1.0)
- `longRangeProbability` - Connection probability for distant neurons (0.0 to 1.0)
- `localRadius` - Distance threshold for "local" connections
- `weight` - Synaptic weight for all connections
- `delay` - Synaptic delay in milliseconds

**Spatial Positions Required:** Must call `setNeuronPositions()` before generating connections

**Example:**
```cpp
SmallWorldPattern pattern(0.8, 0.05, 2.0, 1.0, 1.0);

std::map<uint64_t, SpatialPosition> positions;
// ... set positions ...
pattern.setNeuronPositions(positions);

auto connections = pattern.generateConnections(sources, targets);
```

**Use Case:** Biologically realistic networks, efficient information propagation

---

## ConnectivityBuilder API

### Purpose

ConnectivityBuilder applies connectivity patterns to create actual synapses, axons, and dendrites in the network.

### Class: `ConnectivityBuilder`

**Header:** `#include "snnfw/ConnectivityBuilder.h"`

#### Constructor

```cpp
ConnectivityBuilder(
    NeuralObjectFactory& factory,
    Datastore& datastore,
    bool autoPersist = true
)
```

**Parameters:**
- `factory` - Reference to NeuralObjectFactory for creating objects
- `datastore` - Reference to Datastore for persisting objects
- `autoPersist` - If true, automatically persist created objects to datastore

**Example:**
```cpp
ConnectivityBuilder builder(factory, datastore, true);
```

#### Connection Methods

##### Connect Using Neuron Objects

```cpp
ConnectivityStats connect(
    const std::vector<std::shared_ptr<Neuron>>& sourceNeurons,
    const std::vector<std::shared_ptr<Neuron>>& targetNeurons,
    ConnectivityPattern& pattern
)
```

**Parameters:**
- `sourceNeurons` - Vector of source neuron objects
- `targetNeurons` - Vector of target neuron objects
- `pattern` - Connectivity pattern to apply

**Returns:** ConnectivityStats with creation statistics

**Example:**
```cpp
std::vector<std::shared_ptr<Neuron>> sources = {...};
std::vector<std::shared_ptr<Neuron>> targets = {...};
RandomSparsePattern pattern(0.2, 1.0, 1.5);

auto stats = builder.connect(sources, targets, pattern);
std::cout << "Created " << stats.synapsesCreated << " synapses\n";
```

##### Connect Using Neuron IDs

```cpp
ConnectivityStats connect(
    const std::vector<uint64_t>& sourceNeuronIds,
    const std::vector<uint64_t>& targetNeuronIds,
    ConnectivityPattern& pattern
)
```

**Parameters:**
- `sourceNeuronIds` - Vector of source neuron IDs
- `targetNeuronIds` - Vector of target neuron IDs
- `pattern` - Connectivity pattern to apply

**Returns:** ConnectivityStats with creation statistics

**Example:**
```cpp
std::vector<uint64_t> sourceIds = {1001, 1002, 1003};
std::vector<uint64_t> targetIds = {2001, 2002};
AllToAllPattern pattern(1.0, 1.0);

auto stats = builder.connect(sourceIds, targetIds, pattern);
```

#### Inspection Methods

```cpp
const std::vector<std::shared_ptr<Synapse>>& getCreatedSynapses() const
const std::vector<std::shared_ptr<Axon>>& getCreatedAxons() const
const std::vector<std::shared_ptr<Dendrite>>& getCreatedDendrites() const
```

**Returns:** Vector of created objects for inspection

**Example:**
```cpp
auto synapses = builder.getCreatedSynapses();
for (const auto& synapse : synapses) {
    std::cout << "Synapse " << synapse->getId()
              << " weight=" << synapse->getWeight() << "\n";
}
```

#### Utility Methods

```cpp
void clearCreatedObjects()
```

**Purpose:** Clears the internal tracking of created objects (does not delete from datastore)

**Example:**
```cpp
builder.clearCreatedObjects();
```

### Data Structures

#### ConnectivityStats

```cpp
struct ConnectivityStats {
    size_t connectionsRequested;  // Number of connections requested by pattern
    size_t synapsesCreated;       // Number of synapses successfully created
    size_t axonsCreated;          // Number of axons created (if neurons didn't have them)
    size_t dendritesCreated;      // Number of dendrites created
    size_t failedConnections;     // Number of connections that failed
}
```

---

## Examples

### Example 1: Simple Hierarchical Network

```cpp
#include "snnfw/NetworkBuilder.h"
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Datastore.h"

NeuralObjectFactory factory;
Datastore datastore("./simple_network", 100000);
NetworkBuilder builder(factory, datastore);

// Set default parameters
builder.setDefaultNeuronParams(50.0, 0.95, 20)
       .setDefaultSynapseParams(1.0, 1.5);

// Build network
auto brain = builder
    .createBrain("SimpleVisualNetwork")
    .addHemisphere("Left")
    .addLobe("Occipital")
    .addRegion("V1")
    .addNucleus("ProcessingUnit")
    .addColumn("Column1")
    .addLayer("Layer4")
    .addCluster("InputNeurons", 100)
    .build();

std::cout << "Created brain with ID: " << brain->getId() << "\n";
```

### Example 2: Multi-Layer Network with Connectivity

```cpp
#include "snnfw/NetworkBuilder.h"
#include "snnfw/ConnectivityPattern.h"
#include "snnfw/ConnectivityBuilder.h"

NeuralObjectFactory factory;
Datastore datastore("./multilayer_network", 100000);
NetworkBuilder netBuilder(factory, datastore);
ConnectivityBuilder connBuilder(factory, datastore);

// Build structure
auto brain = netBuilder
    .createBrain("TwoLayerNetwork")
    .addHemisphere("Left")
    .addLobe("Occipital")
    .addRegion("V1")
    .addNucleus("ProcessingUnit")
    .addColumn("Column1")
    .addLayer("InputLayer")
    .addCluster("InputCluster", 50)
    .build();

// Get input cluster
auto inputCluster = netBuilder.getCurrentCluster();
auto inputNeurons = inputCluster->getNeurons();

// Add output layer
netBuilder
    .addLayer("OutputLayer")
    .addCluster("OutputCluster", 20);

auto outputCluster = netBuilder.getCurrentCluster();
auto outputNeurons = outputCluster->getNeurons();

// Connect layers with 30% sparse connectivity
RandomSparsePattern pattern(0.3, 1.0, 1.5);
pattern.setSeed(42);  // Reproducible

auto stats = connBuilder.connect(inputNeurons, outputNeurons, pattern);

std::cout << "Created " << stats.synapsesCreated << " synapses\n";
std::cout << "Created " << stats.dendritesCreated << " dendrites\n";
```

### Example 3: Topographic Mapping

```cpp
#include "snnfw/ConnectivityPattern.h"
#include "snnfw/ConnectivityBuilder.h"

// Create two layers
std::vector<std::shared_ptr<Neuron>> retinaLayer;
std::vector<std::shared_ptr<Neuron>> cortexLayer;

for (int i = 0; i < 100; ++i) {
    retinaLayer.push_back(factory.createNeuron(50.0, 0.95, 20));
    cortexLayer.push_back(factory.createNeuron(50.0, 0.95, 20));
}

// Create topographic mapping (preserves spatial order)
TopographicPattern pattern(0.2, 1.0, 1.0);  // 20% neighborhood
ConnectivityBuilder builder(factory, datastore);

auto stats = builder.connect(retinaLayer, cortexLayer, pattern);
std::cout << "Topographic connections: " << stats.synapsesCreated << "\n";
```

### Example 4: Distance-Dependent Connectivity

```cpp
#include "snnfw/ConnectivityPattern.h"
#include "snnfw/ConnectivityBuilder.h"

// Create neurons with spatial positions
std::vector<std::shared_ptr<Neuron>> neurons;
std::map<uint64_t, SpatialPosition> positions;

for (int i = 0; i < 50; ++i) {
    auto neuron = factory.createNeuron(50.0, 0.95, 20);
    neurons.push_back(neuron);

    // Arrange in a grid
    positions[neuron->getId()] = {
        static_cast<double>(i % 10),
        static_cast<double>(i / 10),
        0.0
    };
}

// Create distance-dependent connections (Gaussian falloff)
DistanceDependentPattern pattern(2.0, 1.0, 1.0);  // sigma=2.0
pattern.setNeuronPositions(positions);

ConnectivityBuilder builder(factory, datastore);
auto stats = builder.connect(neurons, neurons, pattern);

std::cout << "Distance-dependent connections: " << stats.synapsesCreated << "\n";
```

### Example 5: Small-World Network

```cpp
#include "snnfw/ConnectivityPattern.h"
#include "snnfw/ConnectivityBuilder.h"

// Create neurons with spatial positions
std::vector<std::shared_ptr<Neuron>> neurons;
std::map<uint64_t, SpatialPosition> positions;

for (int i = 0; i < 100; ++i) {
    auto neuron = factory.createNeuron(50.0, 0.95, 20);
    neurons.push_back(neuron);

    // Random positions
    positions[neuron->getId()] = {
        static_cast<double>(rand() % 100) / 10.0,
        static_cast<double>(rand() % 100) / 10.0,
        0.0
    };
}

// Create small-world network
// High local connectivity (80%), sparse long-range (5%)
SmallWorldPattern pattern(0.8, 0.05, 2.0, 1.0, 1.0);
pattern.setNeuronPositions(positions);
pattern.setSeed(42);

ConnectivityBuilder builder(factory, datastore);
auto stats = builder.connect(neurons, neurons, pattern);

std::cout << "Small-world connections: " << stats.synapsesCreated << "\n";
```

### Example 6: Multi-Column Architecture

```cpp
#include "snnfw/NetworkBuilder.h"
#include "snnfw/ConnectivityPattern.h"
#include "snnfw/ConnectivityBuilder.h"

NeuralObjectFactory factory;
Datastore datastore("./multicolumn_network", 1000000);
NetworkBuilder netBuilder(factory, datastore);
ConnectivityBuilder connBuilder(factory, datastore);

// Build V1-like structure with multiple orientation columns
auto brain = netBuilder
    .createBrain("V1Model")
    .addHemisphere("Left")
    .addLobe("Occipital")
    .addRegion("V1")
    .addNucleus("OrientationProcessing");

// Create 8 orientation columns (0°, 22.5°, 45°, ..., 157.5°)
std::vector<std::shared_ptr<Cluster>> columnClusters;

for (int angle = 0; angle < 180; angle += 22) {
    std::string colName = "Column_" + std::to_string(angle) + "deg";

    netBuilder
        .addColumn(colName)
        .addLayer("Layer4")
        .addCluster("Neurons", 50);

    columnClusters.push_back(netBuilder.getCurrentCluster());
}

// Connect columns with lateral inhibition (sparse)
RandomSparsePattern lateralPattern(0.1, -0.5, 1.0);  // Negative weight = inhibition

for (size_t i = 0; i < columnClusters.size(); ++i) {
    for (size_t j = 0; j < columnClusters.size(); ++j) {
        if (i != j) {
            auto sourceNeurons = columnClusters[i]->getNeurons();
            auto targetNeurons = columnClusters[j]->getNeurons();

            connBuilder.connect(sourceNeurons, targetNeurons, lateralPattern);
        }
    }
}

std::cout << "Created multi-column V1 model\n";
```

---

## Best Practices

### 1. Memory Management

**Use Datastore Caching:**
```cpp
// Configure appropriate cache size for your network
Datastore datastore("./network_db", 1000000);  // 1M objects in cache
```

**Flush Periodically:**
```cpp
// After building large structures
builder.build();
datastore.flush();  // Ensure all objects are persisted
```

### 2. Reproducibility

**Set Random Seeds:**
```cpp
pattern.setSeed(42);  // Reproducible connectivity
```

**Document Parameters:**
```cpp
// Good: Clear parameter documentation
builder.setDefaultNeuronParams(
    50.0,   // threshold
    0.95,   // decay factor
    20      // window size (ms)
);

// Bad: Magic numbers
builder.setDefaultNeuronParams(50.0, 0.95, 20);
```

### 3. Performance

**Batch Operations:**
```cpp
// Good: Build entire structure, then connect
auto brain = builder.createBrain("Network")
    .addHemisphere("Left")
    .addLobe("Occipital")
    .addRegion("V1")
    .addColumn("Column1")
    .addLayer("Layer4")
    .addCluster("Neurons", 1000)
    .build();

// Then connect in batches
connBuilder.connect(sources, targets, pattern);
```

**Use Appropriate Patterns:**
```cpp
// For sparse connectivity, use RandomSparsePattern (not AllToAll + pruning)
RandomSparsePattern sparse(0.2, 1.0, 1.0);  // Efficient

// Avoid creating all-to-all then deleting
AllToAllPattern dense(1.0, 1.0);  // Inefficient for sparse networks
```

### 4. Validation

**Check Statistics:**
```cpp
auto stats = connBuilder.connect(sources, targets, pattern);

if (stats.failedConnections > 0) {
    std::cerr << "Warning: " << stats.failedConnections
              << " connections failed\n";
}

std::cout << "Connection density: "
          << (double)stats.synapsesCreated / (sources.size() * targets.size())
          << "\n";
```

**Verify Structure:**
```cpp
auto cluster = netBuilder.getCurrentCluster();
auto neurons = cluster->getNeurons();

std::cout << "Cluster has " << neurons.size() << " neurons\n";
assert(neurons.size() == expectedCount);
```

### 5. Spatial Patterns

**Always Set Positions for Spatial Patterns:**
```cpp
// Required for DistanceDependentPattern and SmallWorldPattern
std::map<uint64_t, SpatialPosition> positions;
for (const auto& neuron : neurons) {
    positions[neuron->getId()] = calculatePosition(neuron);
}
pattern.setNeuronPositions(positions);
```

**Use Appropriate Coordinate Systems:**
```cpp
// Normalized coordinates [0,1] for topographic patterns
SpatialPosition normalized = {x / maxX, y / maxY, z / maxZ};

// Physical coordinates for distance-dependent patterns
SpatialPosition physical = {x_mm, y_mm, z_mm};
```

### 6. Error Handling

**Check for Exceptions:**
```cpp
try {
    // OneToOnePattern requires equal sizes
    OneToOnePattern pattern(1.0, 1.0);
    auto stats = builder.connect(sources, targets, pattern);
} catch (const std::invalid_argument& e) {
    std::cerr << "Connection error: " << e.what() << "\n";
}
```

**Validate Before Building:**
```cpp
if (sources.empty() || targets.empty()) {
    throw std::runtime_error("Cannot connect empty neuron sets");
}
```

### 7. Configuration Management

**Use Configuration Objects:**
```cpp
struct NetworkConfig {
    double neuronThreshold = 50.0;
    double neuronDecay = 0.95;
    int neuronWindow = 20;
    double synapseWeight = 1.0;
    double synapseDelay = 1.5;
    double connectivity = 0.2;
};

NetworkConfig config;
builder.setDefaultNeuronParams(
    config.neuronThreshold,
    config.neuronDecay,
    config.neuronWindow
);
```

**Load from Files:**
```cpp
// Use ConfigLoader for complex configurations
ConfigLoader loader;
auto config = loader.loadFromFile("network_config.json");
```

---

## Configuration Parameters Reference

### Neuron Parameters

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| threshold | double | 50.0 | > 0 | Firing threshold for membrane potential |
| decayFactor | double | 0.95 | 0.0 - 1.0 | Membrane potential decay per timestep |
| windowSizeMs | int | 20 | > 0 | Temporal window for pattern learning (ms) |

### Synapse Parameters

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| weight | double | 1.0 | any | Synaptic strength (negative = inhibitory) |
| delay | double | 1.0 | ≥ 0 | Transmission delay in milliseconds |

### Connectivity Pattern Parameters

#### RandomSparsePattern

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| probability | double | 0.0 - 1.0 | Connection probability |
| weight | double | any | Synaptic weight |
| delay | double | ≥ 0 | Synaptic delay (ms) |

#### DistanceDependentPattern

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| sigma | double | > 0 | Distance scale (larger = longer range) |
| weight | double | any | Synaptic weight |
| delay | double | ≥ 0 | Synaptic delay (ms) |

#### TopographicPattern

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| neighborhoodSize | double | 0.0 - 1.0 | Maximum normalized distance for connections |
| weight | double | any | Synaptic weight |
| delay | double | ≥ 0 | Synaptic delay (ms) |

#### SmallWorldPattern

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| localProbability | double | 0.0 - 1.0 | Connection probability for local neurons |
| longRangeProbability | double | 0.0 - 1.0 | Connection probability for distant neurons |
| localRadius | double | > 0 | Distance threshold for "local" connections |
| weight | double | any | Synaptic weight |
| delay | double | ≥ 0 | Synaptic delay (ms) |

---

## Troubleshooting

### Common Issues

**Issue:** "ID does not exist" error when connecting
```cpp
// Solution: Ensure neurons are persisted to datastore
for (auto& neuron : neurons) {
    datastore.put(neuron);
}
```

**Issue:** OneToOnePattern throws exception
```cpp
// Solution: Ensure source and target have same size
assert(sources.size() == targets.size());
```

**Issue:** DistanceDependentPattern throws "positions not set"
```cpp
// Solution: Set positions before generating connections
pattern.setNeuronPositions(positions);
auto connections = pattern.generateConnections(sources, targets);
```

**Issue:** Low connection density with DistanceDependentPattern
```cpp
// Solution: Increase sigma for longer-range connections
DistanceDependentPattern pattern(5.0, 1.0, 1.0);  // Larger sigma
```

**Issue:** Out of memory when building large networks
```cpp
// Solution: Reduce cache size and flush periodically
Datastore datastore("./network_db", 100000);  // Smaller cache
builder.build();
datastore.flush();
```

---

## See Also

- [Framework Robustness Analysis](../FRAMEWORK_ROBUSTNESS_ANALYSIS.md)
- [Datastore API Documentation](API_DATASTORE.md)
- [SpikeProcessor API Documentation](API_SPIKE_PROCESSOR.md)
- [Examples Directory](../examples/)

---

**Last Updated:** 2025-11-22
**Version:** 1.0
**Maintainer:** SNNFW Development Team


