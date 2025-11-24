# SNNFW Quick Reference Guide

**Version:** 1.0  
**Date:** 2025-11-22

---

## Essential Includes

```cpp
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Datastore.h"
#include "snnfw/SpikeProcessor.h"
#include "snnfw/NetworkBuilder.h"
#include "snnfw/ConnectivityPattern.h"
#include "snnfw/ConnectivityBuilder.h"
```

---

## Quick Start

### 1. Initialize Framework

```cpp
// Create factory and datastore
NeuralObjectFactory factory;
Datastore datastore("./network_db", 1000000);  // 1M object cache

// Create spike processor
SpikeProcessor processor(datastore, true, 20);  // Real-time, 20 threads
```

### 2. Build Network

```cpp
NetworkBuilder builder(factory, datastore);

auto brain = builder
    .createBrain("MyNetwork")
    .addHemisphere("Left")
    .addLobe("Occipital")
    .addRegion("V1")
    .addColumn("Column1")
    .addLayer("Layer4")
    .addCluster("Neurons", 100)  // 100 neurons
    .build();
```

### 3. Create Connections

```cpp
ConnectivityBuilder connBuilder(factory, datastore);

auto layer1Neurons = layer1->getNeurons();
auto layer2Neurons = layer2->getNeurons();

RandomSparsePattern pattern(0.2, 1.0, 1.5);  // 20% connectivity
auto stats = connBuilder.connect(layer1Neurons, layer2Neurons, pattern);
```

### 4. Process Spikes

```cpp
processor.start();

// Add input spikes
processor.addSpike(neuronId, currentTime);

// Run for 1 second
std::this_thread::sleep_for(std::chrono::seconds(1));

processor.stop();
```

---

## Common Patterns

### Create Neurons

```cpp
// Standard neuron
auto neuron = factory.createNeuron(50.0, 0.95, 20);

// Excitable neuron (low threshold)
auto excitable = factory.createNeuron(30.0, 0.95, 20);

// Selective neuron (high threshold)
auto selective = factory.createNeuron(75.0, 0.95, 20);
```

### Create Synapses

```cpp
// Excitatory synapse
auto excitatory = factory.createSynapse(axonId, dendriteId, 1.0, 1.5);

// Inhibitory synapse
auto inhibitory = factory.createSynapse(axonId, dendriteId, -1.0, 1.5);

// Strong synapse
auto strong = factory.createSynapse(axonId, dendriteId, 2.5, 1.0);
```

### Connectivity Patterns

```cpp
// Random sparse (20% connectivity)
RandomSparsePattern sparse(0.2, 1.0, 1.5);

// All-to-all
AllToAllPattern dense(1.0, 1.0);

// One-to-one
OneToOnePattern oneToOne(1.0, 1.0);

// Distance-dependent
DistanceDependentPattern distance(2.0, 1.0, 1.0);
distance.setNeuronPositions(positions);

// Topographic
TopographicPattern topo(0.3, 1.0, 1.0);

// Small-world
SmallWorldPattern smallWorld(0.8, 0.05, 2.0, 1.0, 1.0);
smallWorld.setNeuronPositions(positions);
```

---

## Default Parameters

### Neuron
- **Threshold:** 50.0
- **Decay Factor:** 0.95
- **Window Size:** 20ms

### Synapse
- **Weight:** 1.0
- **Delay:** 1.0ms

### Datastore
- **Cache Size:** 1,000,000 objects

### SpikeProcessor
- **Real-time:** true
- **Threads:** 20

---

## Parameter Ranges

| Parameter | Min | Typical | Max | Unit |
|-----------|-----|---------|-----|------|
| Neuron Threshold | 1.0 | 50.0 | 1000.0 | - |
| Neuron Decay | 0.0 | 0.95 | 1.0 | - |
| Neuron Window | 5 | 20 | 100 | ms |
| Synapse Weight | -10.0 | 1.0 | 10.0 | - |
| Synapse Delay | 0.0 | 1.5 | 100.0 | ms |
| Cache Size | 1K | 1M | 10M | objects |
| Thread Count | 1 | 20 | 100 | threads |

---

## Performance Tips

### Memory
```cpp
// Small network
Datastore datastore("./db", 50000);

// Large network
Datastore datastore("./db", 5000000);

// Flush periodically
datastore.flush();
```

### CPU
```cpp
// Auto-detect cores
int threads = std::thread::hardware_concurrency();
SpikeProcessor processor(datastore, true, threads);

// Fast mode (training)
SpikeProcessor processor(datastore, false, 50);
```

### Network
```cpp
// Use sparse connectivity
RandomSparsePattern pattern(0.1, 1.0, 1.0);  // 10%

// Avoid dense patterns for large networks
// AllToAllPattern pattern(1.0, 1.0);  // Don't use
```

---

## Monitoring

### Cache Statistics
```cpp
uint64_t hits, misses;
datastore.getCacheStats(hits, misses);
double hitRate = (double)hits / (hits + misses);
std::cout << "Hit rate: " << (hitRate * 100) << "%\n";
```

### Processor Statistics
```cpp
auto stats = processor.getStats();
std::cout << "Avg loop: " << stats.avgLoopTimeMs << "ms\n";
std::cout << "Max loop: " << stats.maxLoopTimeMs << "ms\n";
std::cout << "Drift: " << stats.timeDriftMs << "ms\n";
```

### Connection Statistics
```cpp
auto stats = connBuilder.connect(sources, targets, pattern);
std::cout << "Synapses: " << stats.synapsesCreated << "\n";
std::cout << "Density: " << (double)stats.synapsesCreated / 
             (sources.size() * targets.size()) << "\n";
```

---

## Common Issues

### "ID does not exist"
```cpp
// Solution: Persist neurons to datastore
datastore.put(neuron);
```

### Low cache hit rate
```cpp
// Solution: Increase cache size
Datastore datastore("./db", 5000000);  // Larger cache
```

### Slow processing
```cpp
// Solution: Increase threads or use fast mode
SpikeProcessor processor(datastore, false, 50);
```

### No neurons firing
```cpp
// Solution: Lower threshold or increase weights
auto neuron = factory.createNeuron(30.0, 0.95, 20);  // Lower threshold
auto synapse = factory.createSynapse(axonId, dendriteId, 2.0, 1.0);  // Higher weight
```

### Runaway firing
```cpp
// Solution: Increase threshold or add inhibition
auto neuron = factory.createNeuron(75.0, 0.95, 20);  // Higher threshold
auto inhibitory = factory.createSynapse(axonId, dendriteId, -1.5, 1.0);  // Inhibition
```

---

## Code Templates

### Minimal Network
```cpp
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Datastore.h"
#include "snnfw/NetworkBuilder.h"

int main() {
    NeuralObjectFactory factory;
    Datastore datastore("./network", 100000);
    NetworkBuilder builder(factory, datastore);
    
    auto brain = builder
        .createBrain("MinimalNetwork")
        .addHemisphere("Left")
        .addLobe("Cortex")
        .addRegion("Area1")
        .addColumn("Column1")
        .addLayer("Layer1")
        .addCluster("Neurons", 10)
        .build();
    
    std::cout << "Created network with " 
              << builder.getCurrentCluster()->getNeurons().size() 
              << " neurons\n";
    
    return 0;
}
```

### Two-Layer Network
```cpp
#include "snnfw/NetworkBuilder.h"
#include "snnfw/ConnectivityBuilder.h"
#include "snnfw/ConnectivityPattern.h"

int main() {
    NeuralObjectFactory factory;
    Datastore datastore("./network", 100000);
    NetworkBuilder netBuilder(factory, datastore);
    ConnectivityBuilder connBuilder(factory, datastore);
    
    // Build structure
    auto brain = netBuilder
        .createBrain("TwoLayerNetwork")
        .addHemisphere("Left")
        .addLobe("Cortex")
        .addRegion("Area1")
        .addColumn("Column1")
        .addLayer("InputLayer")
        .addCluster("Input", 50)
        .build();
    
    auto inputNeurons = netBuilder.getCurrentCluster()->getNeurons();
    
    netBuilder
        .addLayer("OutputLayer")
        .addCluster("Output", 20);
    
    auto outputNeurons = netBuilder.getCurrentCluster()->getNeurons();
    
    // Connect layers
    RandomSparsePattern pattern(0.3, 1.0, 1.5);
    auto stats = connBuilder.connect(inputNeurons, outputNeurons, pattern);
    
    std::cout << "Created " << stats.synapsesCreated << " connections\n";
    
    return 0;
}
```

### Spike Processing
```cpp
#include "snnfw/SpikeProcessor.h"
#include "snnfw/Datastore.h"

int main() {
    Datastore datastore("./network", 100000);
    SpikeProcessor processor(datastore, true, 20);
    
    // Start processor
    processor.start();
    
    // Add input spikes
    for (int t = 0; t < 1000; ++t) {
        processor.addSpike(inputNeuronId, t);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Stop processor
    processor.stop();
    
    // Print statistics
    auto stats = processor.getStats();
    std::cout << "Avg loop time: " << stats.avgLoopTimeMs << "ms\n";
    
    return 0;
}
```

---

## See Also

- [API Documentation](API_NETWORK_CONSTRUCTION.md)
- [Configuration Guide](CONFIGURATION_GUIDE.md)
- [Framework Analysis](../FRAMEWORK_ROBUSTNESS_ANALYSIS.md)
- [Examples](../examples/)

---

**Last Updated:** 2025-11-22  
**Version:** 1.0


