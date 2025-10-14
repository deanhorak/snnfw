# Neural Connectivity Architecture

## Overview

This document describes the ID-based connectivity architecture between neurons, axons, dendrites, and synapses in the SNNFW framework. All connections are maintained using 64-bit unsigned integer IDs for memory efficiency and scalability.

## Component Relationships

### Neuron ↔ Axon ↔ Synapse ↔ Dendrite ↔ Neuron

```
┌─────────────┐
│   Neuron    │
│   (Source)  │
└──────┬──────┘
       │ has one
       │ axonId
       ▼
┌─────────────┐
│    Axon     │
│             │
└──────┬──────┘
       │ has many
       │ synapseIds
       ▼
┌─────────────┐
│   Synapse   │
│             │
└──────┬──────┘
       │ connects to
       │ dendriteId
       ▼
┌─────────────┐
│  Dendrite   │
│             │
└──────┬──────┘
       │ belongs to
       │ targetNeuronId
       ▼
┌─────────────┐
│   Neuron    │
│   (Target)  │
└─────────────┘
```

## Detailed Component Descriptions

### 1. Neuron

**Purpose**: The fundamental computational unit that processes spikes and learns patterns.

**Relationships**:
- **Has ONE Axon**: Each neuron has exactly one axon (output terminal)
  - Stored as: `uint64_t axonId`
  - Default: `0` (not set)
  - Methods: `setAxonId()`, `getAxonId()`

- **Has MANY Dendrites**: Each neuron can have multiple dendrites (input terminals)
  - Stored as: `std::vector<uint64_t> dendriteIds`
  - Methods: `addDendrite()`, `removeDendrite()`, `getDendriteIds()`, `getDendriteCount()`

**Example**:
```cpp
NeuralObjectFactory factory;
auto neuron = factory.createNeuron(50.0, 0.95, 20);

// Create and assign axon
auto axon = factory.createAxon(neuron->getId());
neuron->setAxonId(axon->getId());

// Create and add dendrites
auto dendrite1 = factory.createDendrite(neuron->getId());
auto dendrite2 = factory.createDendrite(neuron->getId());
neuron->addDendrite(dendrite1->getId());
neuron->addDendrite(dendrite2->getId());
```

### 2. Axon

**Purpose**: The output terminal of a neuron that transmits action potentials.

**Relationships**:
- **Belongs to ONE Neuron**: Each axon belongs to exactly one source neuron
  - Stored as: `uint64_t sourceNeuronId`
  - Methods: `getSourceNeuronId()`, `setSourceNeuronId()`

- **Connects to MANY Synapses**: Each axon can connect to multiple synapses
  - Stored as: `std::vector<uint64_t> synapseIds`
  - Methods: `addSynapse()`, `removeSynapse()`, `getSynapseIds()`, `getSynapseCount()`

**Example**:
```cpp
NeuralObjectFactory factory;
auto neuron = factory.createNeuron(50.0, 0.95, 20);
auto axon = factory.createAxon(neuron->getId());

// Create synapses
auto synapse1 = factory.createSynapse(axon->getId(), dendrite1->getId(), 0.8, 1.0);
auto synapse2 = factory.createSynapse(axon->getId(), dendrite2->getId(), 0.6, 1.5);

// Add synapses to axon
axon->addSynapse(synapse1->getId());
axon->addSynapse(synapse2->getId());
```

### 3. Synapse

**Purpose**: The connection point between an axon and a dendrite with weight and delay.

**Relationships**:
- **Connects to ONE Axon**: Each synapse connects to exactly one presynaptic axon
  - Stored as: `uint64_t axonId`
  - Method: `getAxonId()`

- **Connects to ONE Dendrite**: Each synapse connects to exactly one postsynaptic dendrite
  - Stored as: `uint64_t dendriteId`
  - Method: `getDendriteId()`

**Properties**:
- `double weight`: Synaptic strength (modulates signal amplitude)
- `double delay`: Transmission delay in milliseconds

**Example**:
```cpp
NeuralObjectFactory factory;

// Create synapse connecting axon to dendrite
auto synapse = factory.createSynapse(
    axonId,      // Presynaptic axon
    dendriteId,  // Postsynaptic dendrite
    0.8,         // Weight
    1.5          // Delay (ms)
);

// Synapse maintains both connections
uint64_t presynapticAxon = synapse->getAxonId();
uint64_t postsynapticDendrite = synapse->getDendriteId();
```

### 4. Dendrite

**Purpose**: The input terminal of a neuron that receives action potentials.

**Relationships**:
- **Belongs to ONE Neuron**: Each dendrite belongs to exactly one target neuron
  - Stored as: `uint64_t targetNeuronId`
  - Methods: `getTargetNeuronId()`, `setTargetNeuronId()`

- **Receives from MANY Synapses**: Each dendrite can receive from multiple synapses
  - Stored as: `std::vector<uint64_t> synapseIds`
  - Methods: `addSynapse()`, `removeSynapse()`, `getSynapseIds()`, `getSynapseCount()`

**Example**:
```cpp
NeuralObjectFactory factory;
auto neuron = factory.createNeuron(50.0, 0.95, 20);
auto dendrite = factory.createDendrite(neuron->getId());

// Create synapses connecting to this dendrite
auto synapse1 = factory.createSynapse(axon1->getId(), dendrite->getId(), 0.8, 1.0);
auto synapse2 = factory.createSynapse(axon2->getId(), dendrite->getId(), 0.6, 1.5);

// Add synapses to dendrite
dendrite->addSynapse(synapse1->getId());
dendrite->addSynapse(synapse2->getId());
```

## Complete Connection Example

Here's a complete example showing how to connect two neurons:

```cpp
#include "snnfw/NeuralObjectFactory.h"
#include <map>

int main() {
    NeuralObjectFactory factory;
    
    // ========================================================================
    // Create Source Neuron with Axon
    // ========================================================================
    auto sourceNeuron = factory.createNeuron(50.0, 0.95, 20);
    auto axon = factory.createAxon(sourceNeuron->getId());
    sourceNeuron->setAxonId(axon->getId());
    
    // ========================================================================
    // Create Target Neuron with Dendrite
    // ========================================================================
    auto targetNeuron = factory.createNeuron(50.0, 0.95, 20);
    auto dendrite = factory.createDendrite(targetNeuron->getId());
    targetNeuron->addDendrite(dendrite->getId());
    
    // ========================================================================
    // Create Synapse Connecting Them
    // ========================================================================
    auto synapse = factory.createSynapse(
        axon->getId(),      // From source neuron's axon
        dendrite->getId(),  // To target neuron's dendrite
        0.8,                // Synaptic weight
        1.5                 // Synaptic delay (ms)
    );
    
    // ========================================================================
    // Register Synapse with Both Axon and Dendrite
    // ========================================================================
    axon->addSynapse(synapse->getId());
    dendrite->addSynapse(synapse->getId());
    
    // ========================================================================
    // Verify Connections
    // ========================================================================
    std::cout << "Source Neuron ID: " << sourceNeuron->getId() << std::endl;
    std::cout << "  Axon ID: " << sourceNeuron->getAxonId() << std::endl;
    std::cout << "  Axon has " << axon->getSynapseCount() << " synapses" << std::endl;
    
    std::cout << "Target Neuron ID: " << targetNeuron->getId() << std::endl;
    std::cout << "  Has " << targetNeuron->getDendriteCount() << " dendrites" << std::endl;
    std::cout << "  Dendrite has " << dendrite->getSynapseCount() << " synapses" << std::endl;
    
    std::cout << "Synapse ID: " << synapse->getId() << std::endl;
    std::cout << "  Connects Axon " << synapse->getAxonId() 
              << " to Dendrite " << synapse->getDendriteId() << std::endl;
    
    return 0;
}
```

## Cardinality Summary

| Component | Relationship | Cardinality | Storage |
|-----------|-------------|-------------|---------|
| Neuron → Axon | has | 1 | `uint64_t axonId` |
| Neuron → Dendrites | has | 0..N | `std::vector<uint64_t> dendriteIds` |
| Axon → Neuron | belongs to | 1 | `uint64_t sourceNeuronId` |
| Axon → Synapses | connects to | 0..N | `std::vector<uint64_t> synapseIds` |
| Dendrite → Neuron | belongs to | 1 | `uint64_t targetNeuronId` |
| Dendrite → Synapses | receives from | 0..N | `std::vector<uint64_t> synapseIds` |
| Synapse → Axon | connects from | 1 | `uint64_t axonId` |
| Synapse → Dendrite | connects to | 1 | `uint64_t dendriteId` |

## Benefits of ID-Based Architecture

1. **Memory Efficiency**: 8-byte IDs vs 16-byte shared pointers
2. **Cache Locality**: Contiguous ID arrays improve performance
3. **Serialization**: Easy to save/load network topology
4. **Decoupling**: Components don't manage each other's lifetimes
5. **Flexibility**: Objects can be stored in separate registries or databases
6. **Scalability**: Supports brain-scale networks with billions of connections
7. **Thread Safety**: IDs can be safely copied across threads

## Navigation Patterns

### Forward Navigation (Source → Target)
```cpp
// From source neuron to target neuron
uint64_t axonId = sourceNeuron->getAxonId();
const auto& synapseIds = axon->getSynapseIds();
for (uint64_t synapseId : synapseIds) {
    uint64_t dendriteId = synapse->getDendriteId();
    uint64_t targetNeuronId = dendrite->getTargetNeuronId();
    // Process target neuron
}
```

### Backward Navigation (Target → Source)
```cpp
// From target neuron to source neurons
const auto& dendriteIds = targetNeuron->getDendriteIds();
for (uint64_t dendriteId : dendriteIds) {
    const auto& synapseIds = dendrite->getSynapseIds();
    for (uint64_t synapseId : synapseIds) {
        uint64_t axonId = synapse->getAxonId();
        uint64_t sourceNeuronId = axon->getSourceNeuronId();
        // Process source neuron
    }
}
```

## See Also

- [ID_RANGE_SCALE.md](ID_RANGE_SCALE.md) - ID range assignments for all object types
- [NEURAL_OBJECT_FACTORY.md](NEURAL_OBJECT_FACTORY.md) - Factory pattern for object creation
- [HIERARCHICAL_STRUCTURE.md](HIERARCHICAL_STRUCTURE.md) - Brain hierarchy organization

