# SNNFW Configuration Guide

**Version:** 1.0  
**Date:** 2025-11-22  
**Purpose:** Comprehensive guide to configuring SNNFW framework components

---

## Table of Contents

1. [Overview](#overview)
2. [Datastore Configuration](#datastore-configuration)
3. [SpikeProcessor Configuration](#spikeprocessor-configuration)
4. [Neuron Parameters](#neuron-parameters)
5. [Synapse Parameters](#synapse-parameters)
6. [Learning Parameters](#learning-parameters)
7. [Performance Tuning](#performance-tuning)
8. [Best Practices](#best-practices)

---

## Overview

The SNNFW framework provides extensive configuration options for:
- **Datastore** - Persistence and caching
- **SpikeProcessor** - Real-time spike processing
- **Neurons** - Firing dynamics and temporal learning
- **Synapses** - Connection strength and plasticity
- **Learning** - STDP and homeostatic mechanisms

All parameters have sensible defaults but can be tuned for specific applications.

---

## Datastore Configuration

### Constructor Parameters

```cpp
Datastore(const std::string& dbPath, size_t cacheSize)
```

#### dbPath (string)

**Purpose:** Path to RocksDB database directory

**Default:** None (required parameter)

**Recommendations:**
- Use absolute paths for production
- Use temporary directories for testing
- Ensure sufficient disk space (10-100GB for large networks)

**Examples:**
```cpp
// Production
Datastore datastore("/data/neural_networks/v1_model", 1000000);

// Testing
Datastore datastore("/tmp/test_network", 10000);

// Development
Datastore datastore("./network_db", 100000);
```

#### cacheSize (size_t)

**Purpose:** Maximum number of objects to keep in LRU cache

**Default:** None (required parameter)

**Range:** 1,000 - 10,000,000

**Memory Impact:** ~1KB per cached object (approximate)

**Recommendations:**
- **Small networks (<10K neurons):** 10,000 - 100,000
- **Medium networks (10K-1M neurons):** 100,000 - 1,000,000
- **Large networks (>1M neurons):** 1,000,000 - 10,000,000

**Trade-offs:**
- Larger cache = faster access, more memory
- Smaller cache = less memory, more disk I/O

**Examples:**
```cpp
// Small network (low memory)
Datastore datastore("./network_db", 10000);

// Medium network (balanced)
Datastore datastore("./network_db", 500000);

// Large network (high performance)
Datastore datastore("./network_db", 5000000);
```

### Cache Statistics

Monitor cache performance:

```cpp
uint64_t hits, misses;
datastore.getCacheStats(hits, misses);

double hitRate = (double)hits / (hits + misses);
std::cout << "Cache hit rate: " << (hitRate * 100) << "%\n";
```

**Target Hit Rates:**
- **>90%** - Excellent (cache size appropriate)
- **70-90%** - Good (consider increasing cache)
- **<70%** - Poor (increase cache size or reduce working set)

---

## SpikeProcessor Configuration

### Constructor Parameters

```cpp
SpikeProcessor(
    Datastore& datastore,
    bool realTime = true,
    int numThreads = 20
)
```

#### realTime (bool)

**Purpose:** Enable real-time synchronization (1ms timeslice = 1ms wall-clock)

**Default:** true

**Options:**
- `true` - Real-time mode (synchronized with wall clock)
- `false` - Fast mode (process as fast as possible)

**Use Cases:**
- **Real-time mode:** Interactive applications, robotics, real-time control
- **Fast mode:** Batch processing, training, offline analysis

**Examples:**
```cpp
// Real-time mode (for interactive applications)
SpikeProcessor processor(datastore, true, 20);

// Fast mode (for training)
SpikeProcessor processor(datastore, false, 20);
```

#### numThreads (int)

**Purpose:** Number of worker threads for parallel spike processing

**Default:** 20

**Range:** 1 - 100 (practical limit: number of CPU cores × 2)

**Recommendations:**
- **Small networks (<10K neurons):** 4 - 10 threads
- **Medium networks (10K-100K neurons):** 10 - 20 threads
- **Large networks (>100K neurons):** 20 - 50 threads

**Trade-offs:**
- More threads = higher throughput, more overhead
- Fewer threads = lower overhead, may bottleneck on large networks

**CPU Utilization:**
```cpp
// Conservative (50% CPU)
int numThreads = std::thread::hardware_concurrency() / 2;

// Balanced (100% CPU)
int numThreads = std::thread::hardware_concurrency();

// Aggressive (200% CPU with hyperthreading)
int numThreads = std::thread::hardware_concurrency() * 2;
```

**Examples:**
```cpp
// Small network (4 threads)
SpikeProcessor processor(datastore, true, 4);

// Medium network (20 threads)
SpikeProcessor processor(datastore, true, 20);

// Large network (50 threads)
SpikeProcessor processor(datastore, true, 50);

// Auto-detect (use all cores)
SpikeProcessor processor(datastore, true, std::thread::hardware_concurrency());
```

### Performance Monitoring

```cpp
processor.start();
// ... run simulation ...
processor.stop();

auto stats = processor.getStats();
std::cout << "Avg loop time: " << stats.avgLoopTimeMs << " ms\n";
std::cout << "Max loop time: " << stats.maxLoopTimeMs << " ms\n";
std::cout << "Time drift: " << stats.timeDriftMs << " ms\n";
```

**Target Metrics (Real-time Mode):**
- **Avg loop time:** <1ms (meeting real-time constraint)
- **Max loop time:** <5ms (occasional spikes acceptable)
- **Time drift:** <100ms over 1 hour (good synchronization)

---

## Neuron Parameters

### Constructor Parameters

```cpp
Neuron(double threshold, double decayFactor, int windowSizeMs)
```

#### threshold (double)

**Purpose:** Membrane potential threshold for firing

**Default:** 50.0

**Range:** 1.0 - 1000.0 (typical: 20.0 - 100.0)

**Biological Context:** Corresponds to ~-55mV firing threshold in biological neurons

**Effects:**
- **Lower threshold:** More excitable, fires more frequently
- **Higher threshold:** Less excitable, fires less frequently

**Recommendations:**
- **Excitatory neurons:** 40.0 - 60.0
- **Inhibitory neurons:** 30.0 - 50.0
- **Input neurons:** 20.0 - 40.0 (more sensitive)
- **Output neurons:** 50.0 - 80.0 (more selective)

**Examples:**
```cpp
// Highly excitable input neuron
auto inputNeuron = factory.createNeuron(25.0, 0.95, 20);

// Standard excitatory neuron
auto excitatoryNeuron = factory.createNeuron(50.0, 0.95, 20);

// Selective output neuron
auto outputNeuron = factory.createNeuron(75.0, 0.95, 20);
```

#### decayFactor (double)

**Purpose:** Membrane potential decay rate per millisecond

**Default:** 0.95

**Range:** 0.0 - 1.0 (typical: 0.90 - 0.99)

**Biological Context:** Corresponds to membrane time constant (τ)

**Effects:**
- **Higher decay (closer to 1.0):** Longer integration window, more temporal summation
- **Lower decay (closer to 0.0):** Shorter integration window, less temporal summation

**Time Constant Relationship:**
- decay = 0.90 → τ ≈ 10ms
- decay = 0.95 → τ ≈ 20ms
- decay = 0.99 → τ ≈ 100ms

**Recommendations:**
- **Fast dynamics:** 0.85 - 0.92 (τ = 5-15ms)
- **Standard dynamics:** 0.93 - 0.96 (τ = 15-30ms)
- **Slow dynamics:** 0.97 - 0.99 (τ = 30-100ms)

**Examples:**
```cpp
// Fast-responding neuron (short integration)
auto fastNeuron = factory.createNeuron(50.0, 0.90, 20);

// Standard neuron (medium integration)
auto standardNeuron = factory.createNeuron(50.0, 0.95, 20);

// Slow-integrating neuron (long integration)
auto slowNeuron = factory.createNeuron(50.0, 0.98, 20);
```

#### windowSizeMs (int)

**Purpose:** Temporal window size for pattern learning (milliseconds)

**Default:** 20

**Range:** 5 - 100 (typical: 10 - 50)

**Effects:**
- **Larger window:** Learn longer temporal patterns, more memory
- **Smaller window:** Learn shorter patterns, less memory

**Memory Impact:** ~8 bytes per millisecond per neuron

**Recommendations:**
- **Fast patterns (visual flicker):** 10 - 20ms
- **Standard patterns (speech phonemes):** 20 - 50ms
- **Slow patterns (motor sequences):** 50 - 100ms

**Examples:**
```cpp
// Short-term pattern learning
auto shortTermNeuron = factory.createNeuron(50.0, 0.95, 10);

// Medium-term pattern learning
auto mediumTermNeuron = factory.createNeuron(50.0, 0.95, 30);

// Long-term pattern learning
auto longTermNeuron = factory.createNeuron(50.0, 0.95, 100);
```

---

## Synapse Parameters

### Constructor Parameters

```cpp
Synapse(uint64_t axonId, uint64_t dendriteId, double weight, double delay)
```

#### weight (double)

**Purpose:** Synaptic strength (positive = excitatory, negative = inhibitory)

**Default:** 1.0

**Range:** -10.0 to +10.0 (typical: -2.0 to +2.0)

**Effects:**
- **Positive weight:** Excitatory (increases membrane potential)
- **Negative weight:** Inhibitory (decreases membrane potential)
- **Magnitude:** Strength of effect

**Recommendations:**
- **Weak connections:** 0.1 - 0.5
- **Standard connections:** 0.5 - 1.5
- **Strong connections:** 1.5 - 3.0
- **Inhibitory connections:** -0.5 to -2.0

**Examples:**
```cpp
// Weak excitatory synapse
auto weakSynapse = factory.createSynapse(axonId, dendriteId, 0.3, 1.0);

// Standard excitatory synapse
auto standardSynapse = factory.createSynapse(axonId, dendriteId, 1.0, 1.0);

// Strong excitatory synapse
auto strongSynapse = factory.createSynapse(axonId, dendriteId, 2.0, 1.0);

// Inhibitory synapse
auto inhibitorySynapse = factory.createSynapse(axonId, dendriteId, -1.0, 1.0);
```

#### delay (double)

**Purpose:** Synaptic transmission delay (milliseconds)

**Default:** 1.0

**Range:** 0.0 - 100.0 (typical: 0.5 - 10.0)

**Biological Context:** Axonal conduction delay + synaptic transmission delay

**Effects:**
- **Shorter delay:** Faster signal propagation
- **Longer delay:** Slower signal propagation, enables temporal patterns

**Recommendations:**
- **Local connections:** 0.5 - 2.0ms
- **Medium-range connections:** 2.0 - 5.0ms
- **Long-range connections:** 5.0 - 20.0ms

**Examples:**
```cpp
// Fast local connection
auto localSynapse = factory.createSynapse(axonId, dendriteId, 1.0, 0.5);

// Standard connection
auto standardSynapse = factory.createSynapse(axonId, dendriteId, 1.0, 1.5);

// Long-range connection
auto longRangeSynapse = factory.createSynapse(axonId, dendriteId, 1.0, 10.0);
```

---

## Learning Parameters

### STDP (Spike-Timing-Dependent Plasticity)

STDP parameters are configured in the Neuron class:

```cpp
// STDP is automatically applied during spike processing
// Parameters are hardcoded in Neuron::applySTDP()
```

**Current STDP Parameters:**
- **A_plus:** 0.01 (LTP amplitude)
- **A_minus:** 0.01 (LTD amplitude)
- **tau_plus:** 20ms (LTP time constant)
- **tau_minus:** 20ms (LTD time constant)

**Effects:**
- **Pre-before-post (Δt > 0):** Potentiation (weight increase)
- **Post-before-pre (Δt < 0):** Depression (weight decrease)

### Homeostatic Plasticity

Configured via `setTargetFiringRate()`:

```cpp
neuron->setTargetFiringRate(10.0);  // Target: 10 Hz
```

**Purpose:** Maintain stable firing rates through weight scaling

**Default:** 10.0 Hz

**Range:** 1.0 - 100.0 Hz (typical: 5.0 - 20.0 Hz)

**Recommendations:**
- **Sparse coding:** 1.0 - 5.0 Hz
- **Standard coding:** 5.0 - 15.0 Hz
- **Dense coding:** 15.0 - 30.0 Hz

---

## Performance Tuning

### Memory Optimization

#### Reduce Cache Size

```cpp
// For memory-constrained systems
Datastore datastore("./network_db", 50000);  // 50K objects (~50MB)
```

#### Flush Periodically

```cpp
// Flush dirty objects to disk
datastore.flush();

// Clear cache statistics
datastore.clearCacheStats();
```

#### Monitor Memory Usage

```cpp
// Check cache statistics
uint64_t hits, misses;
datastore.getCacheStats(hits, misses);

double hitRate = (double)hits / (hits + misses);
if (hitRate < 0.7) {
    std::cout << "Warning: Low cache hit rate (" << hitRate << ")\n";
    std::cout << "Consider increasing cache size\n";
}
```

### CPU Optimization

#### Thread Pool Sizing

```cpp
// Auto-detect optimal thread count
int optimalThreads = std::thread::hardware_concurrency();

// For CPU-bound workloads
SpikeProcessor processor(datastore, true, optimalThreads);

// For I/O-bound workloads (can use more threads)
SpikeProcessor processor(datastore, true, optimalThreads * 2);
```

#### Real-time vs Fast Mode

```cpp
// Training (fast mode)
SpikeProcessor trainingProcessor(datastore, false, 20);

// Inference (real-time mode)
SpikeProcessor inferenceProcessor(datastore, true, 10);
```

### Network Optimization

#### Sparse Connectivity

```cpp
// Use sparse patterns for large networks
RandomSparsePattern pattern(0.1, 1.0, 1.0);  // 10% connectivity

// Avoid dense patterns
// AllToAllPattern pattern(1.0, 1.0);  // Don't use for large networks
```

#### Batch Processing

```cpp
// Process spikes in batches
std::vector<Spike> spikeBatch;
for (const auto& spike : allSpikes) {
    spikeBatch.push_back(spike);

    if (spikeBatch.size() >= 1000) {
        processor.processSpikes(spikeBatch);
        spikeBatch.clear();
    }
}
```

---

## Best Practices

### 1. Start with Defaults

```cpp
// Use default parameters initially
auto neuron = factory.createNeuron(50.0, 0.95, 20);
auto synapse = factory.createSynapse(axonId, dendriteId, 1.0, 1.0);
```

### 2. Tune Incrementally

```cpp
// Adjust one parameter at a time
// Test 1: Baseline
auto neuron1 = factory.createNeuron(50.0, 0.95, 20);

// Test 2: Lower threshold
auto neuron2 = factory.createNeuron(40.0, 0.95, 20);

// Test 3: Higher decay
auto neuron3 = factory.createNeuron(50.0, 0.98, 20);
```

### 3. Monitor Performance

```cpp
// Track key metrics
auto stats = processor.getStats();
std::cout << "Avg loop time: " << stats.avgLoopTimeMs << " ms\n";

uint64_t hits, misses;
datastore.getCacheStats(hits, misses);
std::cout << "Cache hit rate: " << ((double)hits / (hits + misses)) << "\n";
```

### 4. Document Configuration

```cpp
// Create configuration struct
struct NetworkConfig {
    // Neuron parameters
    double neuronThreshold = 50.0;
    double neuronDecay = 0.95;
    int neuronWindow = 20;

    // Synapse parameters
    double synapseWeight = 1.0;
    double synapseDelay = 1.5;

    // Datastore parameters
    size_t cacheSize = 1000000;

    // Processor parameters
    bool realTime = true;
    int numThreads = 20;

    void print() const {
        std::cout << "Network Configuration:\n";
        std::cout << "  Neuron: threshold=" << neuronThreshold
                  << ", decay=" << neuronDecay
                  << ", window=" << neuronWindow << "ms\n";
        std::cout << "  Synapse: weight=" << synapseWeight
                  << ", delay=" << synapseDelay << "ms\n";
        std::cout << "  Datastore: cache=" << cacheSize << " objects\n";
        std::cout << "  Processor: realTime=" << realTime
                  << ", threads=" << numThreads << "\n";
    }
};
```

### 5. Use Configuration Files

```cpp
// Save configuration to JSON
nlohmann::json config;
config["neuron"]["threshold"] = 50.0;
config["neuron"]["decay"] = 0.95;
config["neuron"]["window"] = 20;
config["synapse"]["weight"] = 1.0;
config["synapse"]["delay"] = 1.5;

std::ofstream file("network_config.json");
file << config.dump(4);
```

### 6. Validate Parameters

```cpp
void validateNeuronParams(double threshold, double decay, int window) {
    if (threshold <= 0) {
        throw std::invalid_argument("Threshold must be positive");
    }
    if (decay < 0 || decay > 1) {
        throw std::invalid_argument("Decay must be in [0, 1]");
    }
    if (window <= 0) {
        throw std::invalid_argument("Window must be positive");
    }
}
```

### 7. Profile Before Optimizing

```cpp
// Use PerformanceProfiler to identify bottlenecks
PerformanceProfiler profiler(datastore);
profiler.startProfiling();

// Run simulation
processor.start();
// ... simulation code ...
processor.stop();

profiler.stopProfiling();
auto report = profiler.generateReport();
std::cout << report << "\n";
```

---

## Configuration Examples

### Example 1: Small Interactive Network

```cpp
// Configuration for small real-time network
NetworkConfig config;
config.neuronThreshold = 45.0;      // Slightly more excitable
config.neuronDecay = 0.95;          // Standard decay
config.neuronWindow = 20;           // Standard window
config.synapseWeight = 1.0;         // Standard weight
config.synapseDelay = 1.0;          // Fast transmission
config.cacheSize = 50000;           // Small cache (50K objects)
config.realTime = true;             // Real-time mode
config.numThreads = 4;              // Few threads

Datastore datastore("./interactive_network", config.cacheSize);
SpikeProcessor processor(datastore, config.realTime, config.numThreads);
```

### Example 2: Large Training Network

```cpp
// Configuration for large-scale training
NetworkConfig config;
config.neuronThreshold = 50.0;      // Standard threshold
config.neuronDecay = 0.95;          // Standard decay
config.neuronWindow = 30;           // Longer patterns
config.synapseWeight = 1.0;         // Standard weight
config.synapseDelay = 1.5;          // Standard delay
config.cacheSize = 5000000;         // Large cache (5M objects)
config.realTime = false;            // Fast mode
config.numThreads = 50;             // Many threads

Datastore datastore("./training_network", config.cacheSize);
SpikeProcessor processor(datastore, config.realTime, config.numThreads);
```

### Example 3: Biologically Realistic Network

```cpp
// Configuration for biological realism
NetworkConfig config;
config.neuronThreshold = 55.0;      // Realistic threshold
config.neuronDecay = 0.98;          // Slow decay (τ ≈ 50ms)
config.neuronWindow = 50;           // Long temporal window
config.synapseWeight = 0.8;         // Moderate weight
config.synapseDelay = 2.0;          // Realistic delay
config.cacheSize = 1000000;         // Medium cache
config.realTime = true;             // Real-time mode
config.numThreads = 20;             // Balanced threads

Datastore datastore("./biological_network", config.cacheSize);
SpikeProcessor processor(datastore, config.realTime, config.numThreads);
```

### Example 4: Sparse Coding Network

```cpp
// Configuration for sparse coding
NetworkConfig config;
config.neuronThreshold = 75.0;      // High threshold (selective)
config.neuronDecay = 0.92;          // Fast decay
config.neuronWindow = 15;           // Short window
config.synapseWeight = 2.0;         // Strong weights
config.synapseDelay = 1.0;          // Fast transmission
config.cacheSize = 500000;          // Medium cache
config.realTime = false;            // Fast mode
config.numThreads = 30;             // Many threads

Datastore datastore("./sparse_network", config.cacheSize);
SpikeProcessor processor(datastore, config.realTime, config.numThreads);

// Use sparse connectivity
RandomSparsePattern pattern(0.05, 2.0, 1.0);  // 5% connectivity, strong weights
```

---

## Troubleshooting

### Performance Issues

**Symptom:** Slow spike processing (avg loop time > 1ms in real-time mode)

**Solutions:**
1. Increase thread count
2. Reduce network size
3. Use sparse connectivity
4. Increase cache size
5. Switch to fast mode for training

**Symptom:** High memory usage

**Solutions:**
1. Reduce cache size
2. Flush datastore periodically
3. Use smaller temporal windows
4. Reduce network size

**Symptom:** Low cache hit rate (<70%)

**Solutions:**
1. Increase cache size
2. Reduce working set (process fewer neurons at once)
3. Improve spatial/temporal locality

### Stability Issues

**Symptom:** Runaway firing (neurons fire continuously)

**Solutions:**
1. Increase firing threshold
2. Add inhibitory connections
3. Enable homeostatic plasticity
4. Reduce synaptic weights

**Symptom:** Silent network (no neurons fire)

**Solutions:**
1. Decrease firing threshold
2. Increase synaptic weights
3. Check input connectivity
4. Verify input spike generation

### Learning Issues

**Symptom:** No learning (weights don't change)

**Solutions:**
1. Verify STDP is enabled
2. Check spike timing (need pre-post correlations)
3. Increase learning rate (A_plus, A_minus)
4. Ensure sufficient training time

**Symptom:** Unstable learning (weights explode/vanish)

**Solutions:**
1. Enable homeostatic plasticity
2. Reduce learning rate
3. Add weight bounds
4. Use weight normalization

---

## See Also

- [Network Construction API](API_NETWORK_CONSTRUCTION.md)
- [Framework Robustness Analysis](../FRAMEWORK_ROBUSTNESS_ANALYSIS.md)
- [Examples Directory](../examples/)

---

**Last Updated:** 2025-11-22
**Version:** 1.0
**Maintainer:** SNNFW Development Team


