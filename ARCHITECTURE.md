# SNNFW Architecture Documentation

## Overview

SNNFW (Spiking Neural Network Framework) is designed around a hierarchical object-oriented architecture that provides flexibility, extensibility, and efficient management of neural components.

## Class Hierarchy

```
NeuralObject (Abstract Base Class)
├── Neuron (Concrete Implementation)
└── Cluster (Container Class)
```

## Core Classes

### 1. NeuralObject (Base Class)

**File:** `include/snnfw/NeuralObject.h`

**Purpose:** Provides a common interface and unique identification for all neural objects in the framework.

**Key Features:**
- 64-bit unique identifier (`uint64_t id`)
- Virtual destructor for proper polymorphic cleanup
- Protected constructor (only derived classes can instantiate)
- Getter and setter for ID

**Design Rationale:**
- Enables polymorphic behavior across different neural object types
- Provides a consistent way to identify and track objects
- Allows for future extensions (e.g., serialization, network management)

**Interface:**
```cpp
class NeuralObject {
public:
    virtual ~NeuralObject() = default;
    uint64_t getId() const;
    
protected:
    explicit NeuralObject(uint64_t objectId = 0);
    void setId(uint64_t objectId);
    
private:
    uint64_t id;
};
```

---

### 2. Neuron Class

**Files:** 
- `include/snnfw/Neuron.h`
- `src/Neuron.cpp`

**Purpose:** Implements a spiking neuron with temporal pattern learning and recognition capabilities.

**Inheritance:** `class Neuron : public NeuralObject`

**Key Features:**

1. **Temporal Pattern Learning**
   - Learns spike patterns within a rolling time window
   - Stores patterns as reference templates
   - Capacity-limited memory with intelligent blending

2. **Pattern Recognition**
   - Uses cosine similarity to compare patterns
   - Fires when similarity exceeds threshold
   - Only compares patterns of equal length

3. **Rolling Time Window**
   - Maintains only recent spikes
   - Automatically removes old spikes
   - Configurable window size

4. **Adaptive Storage**
   - Maximum pattern capacity
   - Blends new patterns into most similar existing pattern when full
   - Configurable blending factor (alpha = 0.2)

**Constructor:**
```cpp
Neuron(double windowSizeMs, 
       double similarityThreshold, 
       size_t maxReferencePatterns = 20,
       uint64_t neuronId = 0);
```

**Public Methods:**
- `void insertSpike(double spikeTime)` - Add a spike and check for pattern match
- `void learnCurrentPattern()` - Store current spike pattern
- `void printSpikes() const` - Display current spikes
- `void printReferencePatterns() const` - Display learned patterns

**Private Methods:**
- `void removeOldSpikes(double currentTime)` - Maintain rolling window
- `bool shouldFire() const` - Check if current pattern matches any reference
- `int findMostSimilarPattern(const std::vector<double>& newPattern) const`
- `void blendPattern(std::vector<double>& target, const std::vector<double>& newPattern, double alpha)`
- `static double cosineSimilarity(const std::vector<double>& a, const std::vector<double>& b)`

**Data Members:**
- `std::vector<double> spikes` - Current spikes in rolling window
- `std::vector<std::vector<double>> referencePatterns` - Learned patterns
- `double windowSize` - Rolling window size in milliseconds
- `double threshold` - Similarity threshold for firing
- `size_t maxPatterns` - Maximum number of reference patterns

**Algorithm Details:**

*Pattern Matching:*
1. On spike insertion, remove old spikes outside window
2. Check if current spike pattern matches any reference pattern
3. If match found (similarity ≥ threshold), fire

*Pattern Learning:*
1. If under capacity, add pattern directly
2. If at capacity, find most similar existing pattern
3. Blend new pattern into most similar (80% old, 20% new)

*Cosine Similarity:*
```
similarity = (a · b) / (||a|| × ||b||)
```
where a and b are spike time vectors

---

### 3. Cluster Class

**Files:**
- `include/snnfw/Cluster.h`
- `src/Cluster.cpp`

**Purpose:** Container class for managing groups of neurons.

**Inheritance:** `class Cluster : public NeuralObject`

**Key Features:**

1. **Neuron Management**
   - Add/remove neurons dynamically
   - Index-based access
   - Null-safe operations

2. **Shared Ownership**
   - Uses `std::shared_ptr<Neuron>` for memory management
   - Allows neurons to be shared across multiple clusters
   - Automatic cleanup when no longer referenced

3. **Container Interface**
   - Vector-based storage
   - Size queries
   - Bulk operations (clear)

**Constructor:**
```cpp
explicit Cluster(uint64_t clusterId = 0);
```

**Public Methods:**
- `void addNeuron(std::shared_ptr<Neuron> neuron)` - Add neuron to cluster
- `std::shared_ptr<Neuron> getNeuron(size_t index) const` - Get neuron by index
- `const std::vector<std::shared_ptr<Neuron>>& getNeurons() const` - Get all neurons
- `bool removeNeuron(size_t index)` - Remove neuron by index
- `void clear()` - Remove all neurons
- `size_t size() const` - Get number of neurons
- `void printInfo() const` - Display cluster information

**Data Members:**
- `std::vector<std::shared_ptr<Neuron>> neurons` - Vector of neurons

**Design Rationale:**
- Shared pointers allow flexible neuron management
- Index-based access provides simple, efficient retrieval
- Inheriting from NeuralObject allows clusters to be treated as first-class objects

---

## Design Patterns

### 1. Template Method Pattern
The `NeuralObject` base class defines the interface, while derived classes implement specific behavior.

### 2. Strategy Pattern
Different neuron types can implement different firing and learning strategies while maintaining a common interface.

### 3. Composite Pattern (Future)
Clusters can potentially contain other clusters, enabling hierarchical network structures.

---

## Memory Management

### Ownership Model

1. **Neurons:**
   - Created as `std::shared_ptr<Neuron>`
   - Can be owned by multiple clusters
   - Automatically destroyed when last reference is released

2. **Clusters:**
   - Can be created on stack or heap
   - Own shared pointers to neurons
   - Do not exclusively own neurons

3. **Spike Data:**
   - Owned by individual neurons
   - Managed with `std::vector` (automatic memory management)

### Example Ownership Scenario

```cpp
// Create neurons
auto n1 = std::make_shared<Neuron>(50.0, 0.95, 20, 1);
auto n2 = std::make_shared<Neuron>(50.0, 0.95, 20, 2);

// Create clusters
Cluster cluster1(100);
Cluster cluster2(200);

// n1 is in both clusters (shared ownership)
cluster1.addNeuron(n1);
cluster2.addNeuron(n1);

// n2 is only in cluster1
cluster1.addNeuron(n2);

// When cluster1 is destroyed, n1 still exists (cluster2 owns it)
// When cluster2 is destroyed, n1 is finally deleted
// n2 is deleted when cluster1 is destroyed
```

---

## Future Extensions

### Planned Features

1. **ID Management System**
   - Automatic ID generation
   - ID registry for uniqueness guarantees
   - ID-based lookup

2. **Network Class**
   - Higher-level container for clusters
   - Inter-cluster connections
   - Network-wide operations

3. **Synaptic Connections**
   - Connection weights
   - Plasticity rules (STDP, etc.)
   - Connection topology management

4. **Serialization**
   - Save/load network state
   - Export to standard formats
   - Checkpoint/restore functionality

5. **Visualization**
   - Network topology visualization
   - Spike raster plots
   - Pattern visualization

---

## Testing

### Test Coverage

**Neuron Tests:** 15 tests covering:
- Construction and initialization
- Spike insertion and rolling window
- Pattern learning and storage
- Pattern recognition and firing
- Threshold sensitivity
- Edge cases

**Cluster Tests:** 19 tests covering:
- Construction and ID management
- Neuron addition/removal
- Index-based access
- Null safety
- Inheritance verification
- Shared ownership

**Total:** 34 unit tests, 100% pass rate

### Test Files
- `tests/test_neuron.cpp`
- `tests/test_cluster.cpp`

---

## Build System

**Build Tool:** CMake 3.16+

**Compiler Requirements:** C++17

**Dependencies:**
- Google Test (optional, for testing)

**Build Targets:**
- `snnfw` - Static library
- `snnfw_app` - Main application
- `test_neuron` - Neuron unit tests
- `test_cluster` - Cluster unit tests

---

## Performance Considerations

### Current Implementation

1. **Spike Storage:** `std::vector<double>` - O(1) append, O(n) removal
2. **Pattern Matching:** O(m × n × p) where:
   - m = number of reference patterns
   - n = pattern length
   - p = number of spikes in current window
3. **Memory:** O(m × n) for pattern storage

### Optimization Opportunities

1. Use circular buffer for rolling window (avoid O(n) removal)
2. Cache similarity calculations
3. Use approximate nearest neighbor for pattern matching
4. Parallel pattern matching across neurons
5. SIMD optimizations for cosine similarity

---

## Coding Standards

- **Naming:** camelCase for methods, snake_case for variables
- **Documentation:** Doxygen-style comments
- **Namespace:** All code in `snnfw` namespace
- **Headers:** Include guards with `SNNFW_` prefix
- **Const Correctness:** Methods that don't modify state are `const`
- **Memory Safety:** RAII, smart pointers, no raw `new`/`delete`

