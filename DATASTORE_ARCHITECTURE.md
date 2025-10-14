# Datastore Architecture

## Overview

The SNNFW Datastore provides high-performance persistence for neural network objects with intelligent caching and automatic memory management. It combines an in-memory LRU cache with a RocksDB backing store to handle brain-scale networks that exceed available RAM.

## Key Features

### 1. **LRU Cache (1 Million Objects)**
- Keeps frequently accessed objects in memory
- Automatic eviction of least recently used objects
- Dirty tracking for modified objects
- Thread-safe operations

### 2. **RocksDB Backing Store**
- High-performance key-value store optimized for SSDs
- Persistent storage for all neural objects
- Automatic compression (Snappy)
- Optimized for point lookups

### 3. **JSON Serialization**
- All objects serialize to/from JSON
- Human-readable format for debugging
- Easy to export/import networks
- Type information embedded in JSON

### 4. **Memory Safety**
- Uses `std::shared_ptr` for automatic memory management
- Objects destroyed when no longer referenced
- No memory leaks from cache eviction
- Thread-safe reference counting

### 5. **Type-Safe Retrieval**
- Generic `get(id)` returns `shared_ptr<NeuralObject>`
- Type-specific methods: `getNeuron()`, `getAxon()`, etc.
- Automatic type checking with `dynamic_pointer_cast`

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      APPLICATION CODE                        │
└────────────────────────┬────────────────────────────────────┘
                         │
                         │ get(id) / put(obj) / markDirty(id)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                       DATASTORE                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              LRU CACHE (1M objects)                    │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐            │ │
│  │  │ Neuron   │  │  Axon    │  │ Synapse  │  ...       │ │
│  │  │ ID: 123  │  │ ID: 456  │  │ ID: 789  │            │ │
│  │  │ dirty: Y │  │ dirty: N │  │ dirty: Y │            │ │
│  │  └──────────┘  └──────────┘  └──────────┘            │ │
│  │                                                        │ │
│  │  LRU List: [789] → [123] → [456] → ...               │ │
│  └────────────────────────────────────────────────────────┘ │
│                         │                                    │
│                         │ Cache Miss / Eviction              │
│                         ▼                                    │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              ROCKSDB BACKING STORE                     │ │
│  │                                                        │ │
│  │  Key: "123"  →  Value: {"type":"Neuron", ...}        │ │
│  │  Key: "456"  →  Value: {"type":"Axon", ...}          │ │
│  │  Key: "789"  →  Value: {"type":"Synapse", ...}       │ │
│  │  ...                                                   │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                         │
                         ▼
                    DISK STORAGE
```

## Usage Examples

### Basic Usage

```cpp
#include "snnfw/Datastore.h"
#include "snnfw/NeuralObjectFactory.h"

int main() {
    // Create datastore with 1M object cache
    Datastore datastore("./neural_db", 1000000);
    
    // Create factory for object creation
    NeuralObjectFactory factory;
    
    // Create and store a neuron
    auto neuron = factory.createNeuron(50.0, 0.95, 20);
    datastore.put(neuron);
    
    // Modify the neuron
    neuron->insertSpike(10.0);
    datastore.markDirty(neuron->getId());
    
    // Retrieve the neuron later (from cache or disk)
    auto retrieved = datastore.getNeuron(neuron->getId());
    
    // Datastore automatically flushes dirty objects on destruction
    return 0;
}
```

### Building a Neural Network

```cpp
Datastore datastore("./neural_db");
NeuralObjectFactory factory;

// Create source neuron
auto sourceNeuron = factory.createNeuron(50.0, 0.95, 20);
datastore.put(sourceNeuron);

// Create axon
auto axon = factory.createAxon(sourceNeuron->getId());
sourceNeuron->setAxonId(axon->getId());
datastore.put(axon);
datastore.markDirty(sourceNeuron->getId());

// Create target neuron
auto targetNeuron = factory.createNeuron(50.0, 0.95, 20);
datastore.put(targetNeuron);

// Create dendrite
auto dendrite = factory.createDendrite(targetNeuron->getId());
targetNeuron->addDendrite(dendrite->getId());
datastore.put(dendrite);
datastore.markDirty(targetNeuron->getId());

// Create synapse
auto synapse = factory.createSynapse(axon->getId(), dendrite->getId(), 0.8, 1.5);
axon->addSynapse(synapse->getId());
dendrite->addSynapse(synapse->getId());
datastore.put(synapse);
datastore.markDirty(axon->getId());
datastore.markDirty(dendrite->getId());

// All objects automatically persisted on datastore destruction
```

### Loading an Existing Network

```cpp
Datastore datastore("./neural_db");

// Load neurons by ID
auto neuron1 = datastore.getNeuron(100000000000001);
auto neuron2 = datastore.getNeuron(100000000000002);

// Navigate connections
uint64_t axonId = neuron1->getAxonId();
auto axon = datastore.getAxon(axonId);

const auto& synapseIds = axon->getSynapseIds();
for (uint64_t synapseId : synapseIds) {
    auto synapse = datastore.getSynapse(synapseId);
    auto dendrite = datastore.getDendrite(synapse->getDendriteId());
    auto targetNeuron = datastore.getNeuron(dendrite->getTargetNeuronId());
    
    // Process connection...
}
```

## Cache Behavior

### Cache Hit
1. Object requested via `get(id)`
2. Object found in cache
3. Object moved to front of LRU list (marked as recently used)
4. Return cached object

### Cache Miss
1. Object requested via `get(id)`
2. Object not in cache
3. Load object from RocksDB
4. Deserialize JSON to object
5. If cache full, evict LRU object (flush if dirty)
6. Add object to cache
7. Return object

### Eviction
1. Cache reaches maximum size (1M objects)
2. Identify least recently used object (back of LRU list)
3. If object is dirty, flush to RocksDB
4. Remove object from cache
5. Object destroyed when last `shared_ptr` released

## JSON Serialization Format

Each neural object serializes to JSON with a `type` field for deserialization:

### Neuron
```json
{
  "type": "Neuron",
  "id": 100000000000001,
  "windowSize": 50.0,
  "threshold": 0.95,
  "maxPatterns": 20,
  "axonId": 200000000000001,
  "dendriteIds": [300000000000001, 300000000000002],
  "spikes": [10.0, 20.5, 35.2],
  "referencePatterns": [[10.0, 20.0], [15.0, 25.0]]
}
```

### Axon
```json
{
  "type": "Axon",
  "id": 200000000000001,
  "sourceNeuronId": 100000000000001,
  "synapseIds": [400000000000001, 400000000000002]
}
```

### Dendrite
```json
{
  "type": "Dendrite",
  "id": 300000000000001,
  "targetNeuronId": 100000000000002,
  "synapseIds": [400000000000001]
}
```

### Synapse
```json
{
  "type": "Synapse",
  "id": 400000000000001,
  "axonId": 200000000000001,
  "dendriteId": 300000000000001,
  "weight": 0.8,
  "delay": 1.5
}
```

## Performance Characteristics

### Memory Usage
- **Cache**: ~8 bytes per ID + object size
- **1M Neurons**: ~200-300 MB (depending on spike data)
- **1M Axons**: ~50-100 MB
- **1M Synapses**: ~50 MB
- **Total for 1M mixed objects**: ~200-400 MB

### Throughput
- **Cache Hit**: ~100-500 ns (pointer lookup + LRU update)
- **Cache Miss**: ~10-100 μs (RocksDB read + JSON parse)
- **Write (dirty flush)**: ~10-100 μs (JSON serialize + RocksDB write)
- **Eviction**: ~10-100 μs (flush if dirty + cache removal)

### Scalability
- **Cache Size**: 1,000,000 objects (configurable)
- **Disk Storage**: Limited only by disk space
- **Network Size**: Billions of objects supported
- **Concurrent Access**: Thread-safe with mutex protection

## Thread Safety

All Datastore operations are thread-safe:
- Single mutex protects all cache operations
- RocksDB is thread-safe for concurrent reads/writes
- `shared_ptr` provides thread-safe reference counting

For high-concurrency scenarios, consider:
- Sharding datastore by ID range
- Read-write locks for better read concurrency
- Lock-free cache implementations

## Best Practices

### 1. **Always Mark Dirty**
```cpp
auto neuron = datastore.getNeuron(id);
neuron->insertSpike(10.0);
datastore.markDirty(id);  // IMPORTANT!
```

### 2. **Batch Operations**
```cpp
// Create many objects
for (int i = 0; i < 1000; i++) {
    auto neuron = factory.createNeuron(50.0, 0.95, 20);
    datastore.put(neuron);
}

// Flush periodically for safety
datastore.flushAll();
```

### 3. **Monitor Cache Statistics**
```cpp
uint64_t hits, misses;
datastore.getCacheStats(hits, misses);
double hitRate = 100.0 * hits / (hits + misses);
std::cout << "Cache hit rate: " << hitRate << "%" << std::endl;
```

### 4. **Explicit Flushing for Critical Data**
```cpp
// Modify important neuron
neuron->learnCurrentPattern();
datastore.markDirty(neuron->getId());

// Ensure it's written to disk immediately
datastore.flush(neuron->getId());
```

## Installation Requirements

### RocksDB Installation

**Ubuntu/Debian:**
```bash
sudo apt-get install librocksdb-dev
```

**macOS:**
```bash
brew install rocksdb
```

**From Source:**
```bash
git clone https://github.com/facebook/rocksdb.git
cd rocksdb
make shared_lib
sudo make install
```

## See Also

- [NEURAL_CONNECTIVITY.md](NEURAL_CONNECTIVITY.md) - Neural connection architecture
- [ID_RANGE_SCALE.md](ID_RANGE_SCALE.md) - ID range assignments
- [NEURAL_OBJECT_FACTORY.md](NEURAL_OBJECT_FACTORY.md) - Object creation patterns

