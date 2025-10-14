# NeuralObjectFactory Documentation

## Overview

The `NeuralObjectFactory` is a centralized factory class responsible for creating all `NeuralObject`-derived instances in the SNNFW framework. It provides automatic ID assignment from type-specific ranges, enabling object type identification based solely on the ID value.

## Key Features

- **Automatic ID Assignment**: Each object type gets IDs from a dedicated range
- **Type Identification**: Determine object type from ID alone
- **Thread-Safe**: Can be used safely from multiple threads
- **Statistics Tracking**: Monitor object creation counts
- **Sequential IDs**: IDs are assigned sequentially within each type's range

## ID Range Assignments

Each object type is assigned a range of 100 trillion IDs to support human brain-scale networks:

### Cellular Components

| Object Type | ID Range Start      | ID Range End        | Capacity           |
|-------------|---------------------|---------------------|--------------------|
| **Neuron**  | 100,000,000,000,000 | 199,999,999,999,999 | 100 trillion IDs   |
| **Axon**    | 200,000,000,000,000 | 299,999,999,999,999 | 100 trillion IDs   |
| **Dendrite**| 300,000,000,000,000 | 399,999,999,999,999 | 100 trillion IDs   |
| **Synapse** | 400,000,000,000,000 | 499,999,999,999,999 | 100 trillion IDs   |

### Structural Components (Hierarchical)

| Object Type   | ID Range Start      | ID Range End        | Capacity           |
|---------------|---------------------|---------------------|--------------------|
| **Cluster**   | 500,000,000,000,000 | 599,999,999,999,999 | 100 trillion IDs   |
| **Layer**     | 600,000,000,000,000 | 699,999,999,999,999 | 100 trillion IDs   |
| **Column**    | 700,000,000,000,000 | 799,999,999,999,999 | 100 trillion IDs   |
| **Nucleus**   | 800,000,000,000,000 | 899,999,999,999,999 | 100 trillion IDs   |
| **Region**    | 900,000,000,000,000 | 999,999,999,999,999 | 100 trillion IDs   |
| **Lobe**      | 1,000,000,000,000,000 | 1,099,999,999,999,999 | 100 trillion IDs |
| **Hemisphere**| 1,100,000,000,000,000 | 1,199,999,999,999,999 | 100 trillion IDs |
| **Brain**     | 1,200,000,000,000,000 | 1,299,999,999,999,999 | 100 trillion IDs |

**Biological Context:**
- Human brain: ~86 billion neurons, ~100 trillion synapses
- These ranges provide capacity well beyond biological scale
- Hierarchical structure mirrors biological organization

**Hierarchy:**
```
Brain → Hemisphere → Lobe → Region → Nucleus → Column → Layer → Cluster → Neuron
```

This design allows for:
- **Instant type identification** without lookup tables
- **Biological-scale networks** (100 trillion objects per type)
- **Clear separation** between object types
- **Hierarchical organization** matching brain structure
- **Easy debugging** (IDs clearly indicate object type)

## API Reference

### Creation Methods

#### `createNeuron()`
```cpp
std::shared_ptr<Neuron> createNeuron(
    double windowSizeMs,
    double similarityThreshold,
    size_t maxReferencePatterns = 20);
```
Creates a new Neuron with auto-assigned ID from the Neuron range.

**Parameters:**
- `windowSizeMs`: Size of the rolling time window in milliseconds
- `similarityThreshold`: Threshold for pattern similarity (0.0 to 1.0)
- `maxReferencePatterns`: Maximum number of reference patterns to store

**Returns:** Shared pointer to the newly created Neuron

**Throws:** `std::runtime_error` if ID range is exhausted

---

#### `createAxon()`
```cpp
std::shared_ptr<Axon> createAxon(uint64_t sourceNeuronId);
```
Creates a new Axon with auto-assigned ID from the Axon range.

**Parameters:**
- `sourceNeuronId`: ID of the neuron this axon belongs to

**Returns:** Shared pointer to the newly created Axon

---

#### `createDendrite()`
```cpp
std::shared_ptr<Dendrite> createDendrite(uint64_t targetNeuronId);
```
Creates a new Dendrite with auto-assigned ID from the Dendrite range.

**Parameters:**
- `targetNeuronId`: ID of the neuron this dendrite belongs to

**Returns:** Shared pointer to the newly created Dendrite

---

#### `createSynapse()`
```cpp
std::shared_ptr<Synapse> createSynapse(
    uint64_t axonId,
    uint64_t dendriteId,
    double weight = 1.0,
    double delayMs = 1.0);
```
Creates a new Synapse with auto-assigned ID from the Synapse range.

**Parameters:**
- `axonId`: ID of the presynaptic axon
- `dendriteId`: ID of the postsynaptic dendrite
- `weight`: Synaptic weight (default: 1.0)
- `delayMs`: Synaptic delay in milliseconds (default: 1.0)

**Returns:** Shared pointer to the newly created Synapse

---

#### `createCluster()`
```cpp
std::shared_ptr<Cluster> createCluster();
```
Creates a new Cluster with auto-assigned ID from the Cluster range.

**Returns:** Shared pointer to the newly created Cluster

---

#### `createLayer()`
```cpp
std::shared_ptr<Layer> createLayer();
```
Creates a new Layer with auto-assigned ID from the Layer range.

**Returns:** Shared pointer to the newly created Layer

---

#### `createColumn()`
```cpp
std::shared_ptr<Column> createColumn();
```
Creates a new Column with auto-assigned ID from the Column range.

**Returns:** Shared pointer to the newly created Column

---

#### `createNucleus()`
```cpp
std::shared_ptr<Nucleus> createNucleus();
```
Creates a new Nucleus with auto-assigned ID from the Nucleus range.

**Returns:** Shared pointer to the newly created Nucleus

---

#### `createRegion()`
```cpp
std::shared_ptr<Region> createRegion();
```
Creates a new Region with auto-assigned ID from the Region range.

**Returns:** Shared pointer to the newly created Region

---

#### `createLobe()`
```cpp
std::shared_ptr<Lobe> createLobe();
```
Creates a new Lobe with auto-assigned ID from the Lobe range.

**Returns:** Shared pointer to the newly created Lobe

---

#### `createHemisphere()`
```cpp
std::shared_ptr<Hemisphere> createHemisphere();
```
Creates a new Hemisphere with auto-assigned ID from the Hemisphere range.

**Returns:** Shared pointer to the newly created Hemisphere

---

#### `createBrain()`
```cpp
std::shared_ptr<Brain> createBrain();
```
Creates a new Brain with auto-assigned ID from the Brain range.

**Returns:** Shared pointer to the newly created Brain

---

### Type Identification Methods

#### `getObjectType()`
```cpp
static ObjectType getObjectType(uint64_t id);
```
Determines the object type from an ID value.

**Parameters:**
- `id`: The ID to check

**Returns:** `ObjectType` enum value (NEURON, AXON, DENDRITE, SYNAPSE, CLUSTER, or UNKNOWN)

---

#### `getObjectTypeName()`
```cpp
static const char* getObjectTypeName(ObjectType type);
static const char* getObjectTypeName(uint64_t id);
```
Gets a human-readable string for an object type.

**Parameters:**
- `type`: The ObjectType enum value, OR
- `id`: The ID to check

**Returns:** String representation of the type ("Neuron", "Axon", etc.)

---

### Statistics Methods

#### `getCurrentId()`
```cpp
uint64_t getCurrentId(ObjectType type) const;
```
Gets the current ID counter for a specific object type (the next ID that will be assigned).

**Parameters:**
- `type`: The object type

**Returns:** Current ID counter value

---

#### `getObjectCount()`
```cpp
uint64_t getObjectCount(ObjectType type) const;
```
Gets the number of objects created of a specific type.

**Parameters:**
- `type`: The object type

**Returns:** Number of objects created

---

#### `reset()`
```cpp
void reset();
```
Resets all ID counters to their initial values.

**Warning:** This should only be used for testing or when starting a new simulation. Existing objects will retain their old IDs, which could cause conflicts.

---

## Usage Examples

### Example 1: Creating a Simple Neural Connection

```cpp
#include "snnfw/NeuralObjectFactory.h"

NeuralObjectFactory factory;

// Create two neurons
auto presynapticNeuron = factory.createNeuron(50.0, 0.95, 20);
auto postsynapticNeuron = factory.createNeuron(50.0, 0.95, 20);

// Create axon for presynaptic neuron
auto axon = factory.createAxon(presynapticNeuron->getId());

// Create dendrite for postsynaptic neuron
auto dendrite = factory.createDendrite(postsynapticNeuron->getId());

// Create synapse connecting them
auto synapse = factory.createSynapse(
    axon->getId(), 
    dendrite->getId(), 
    1.5,  // weight
    2.0   // delay in ms
);

std::cout << "Connection: Neuron " << presynapticNeuron->getId()
          << " -> Synapse " << synapse->getId()
          << " -> Neuron " << postsynapticNeuron->getId() << std::endl;
```

### Example 2: Type Identification

```cpp
NeuralObjectFactory factory;

auto neuron = factory.createNeuron(50.0, 0.95, 20);
auto axon = factory.createAxon(neuron->getId());

// Identify type from ID
auto neuronType = NeuralObjectFactory::getObjectType(neuron->getId());
auto axonType = NeuralObjectFactory::getObjectType(axon->getId());

std::cout << "Neuron ID " << neuron->getId() << " is a " 
          << NeuralObjectFactory::getObjectTypeName(neuronType) << std::endl;
std::cout << "Axon ID " << axon->getId() << " is a " 
          << NeuralObjectFactory::getObjectTypeName(axonType) << std::endl;
```

### Example 3: Creating a Network

```cpp
NeuralObjectFactory factory;

// Create 10 neurons
std::vector<std::shared_ptr<Neuron>> neurons;
for (int i = 0; i < 10; ++i) {
    neurons.push_back(factory.createNeuron(50.0, 0.95, 20));
}

// Create axons and dendrites for each neuron
std::vector<std::shared_ptr<Axon>> axons;
std::vector<std::shared_ptr<Dendrite>> dendrites;
for (const auto& neuron : neurons) {
    axons.push_back(factory.createAxon(neuron->getId()));
    dendrites.push_back(factory.createDendrite(neuron->getId()));
}

// Create synapses (fully connected network)
for (const auto& axon : axons) {
    for (const auto& dendrite : dendrites) {
        factory.createSynapse(axon->getId(), dendrite->getId());
    }
}

// Check statistics
std::cout << "Created " 
          << factory.getObjectCount(NeuralObjectFactory::ObjectType::NEURON) 
          << " neurons" << std::endl;
std::cout << "Created " 
          << factory.getObjectCount(NeuralObjectFactory::ObjectType::SYNAPSE) 
          << " synapses" << std::endl;
```

### Example 4: Factory Statistics

```cpp
NeuralObjectFactory factory;

// Create some objects
factory.createNeuron(50.0, 0.95, 20);
factory.createNeuron(50.0, 0.95, 20);
factory.createNeuron(50.0, 0.95, 20);

// Get statistics
uint64_t count = factory.getObjectCount(NeuralObjectFactory::ObjectType::NEURON);
uint64_t nextId = factory.getCurrentId(NeuralObjectFactory::ObjectType::NEURON);

std::cout << "Created " << count << " neurons" << std::endl;
std::cout << "Next neuron ID will be: " << nextId << std::endl;
```

## Thread Safety

The `NeuralObjectFactory` is fully thread-safe. All creation methods and statistics methods use internal mutexes to ensure safe concurrent access.

```cpp
NeuralObjectFactory factory;

// Safe to create objects from multiple threads
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&factory]() {
        for (int j = 0; j < 100; ++j) {
            factory.createNeuron(50.0, 0.95, 20);
        }
    });
}

for (auto& thread : threads) {
    thread.join();
}

// All 1000 neurons will have unique IDs
```

## Design Rationale

### Why Range-Based IDs?

1. **Type Identification**: Instantly determine object type without lookups
2. **Debugging**: IDs clearly indicate what type of object you're dealing with
3. **Scalability**: 100 trillion IDs per type supports human brain-scale networks
4. **Simplicity**: No need for separate type fields or lookup tables

### Why a Factory?

1. **Centralized ID Management**: Single source of truth for ID assignment
2. **Consistency**: Ensures all objects follow the same ID scheme
3. **Thread Safety**: Centralized locking for ID generation
4. **Statistics**: Easy tracking of object creation
5. **Future Extensions**: Easy to add validation, logging, or other features

## Performance Considerations

- **ID Generation**: O(1) - simple counter increment with mutex lock
- **Type Identification**: O(1) - simple range check
- **Memory**: Minimal overhead (5 counters + 1 mutex)
- **Thread Contention**: Minimal - mutex held only during counter increment

## Best Practices

1. **Use a single factory instance** per simulation for consistent ID assignment
2. **Don't manually set IDs** - let the factory manage them
3. **Use type identification** instead of dynamic_cast when possible
4. **Reset only between simulations**, never during active use
5. **Check object counts** to monitor network size and detect leaks

## See Also

- [NeuralObject Base Class](include/snnfw/NeuralObject.h)
- [Neural Infrastructure Documentation](NEURAL_INFRASTRUCTURE.md)
- [Factory Example](examples/factory_example.cpp)

