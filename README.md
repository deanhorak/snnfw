# SNNFW - Spiking Neural Network Framework

A high-performance C++ framework for building and simulating biologically-inspired spiking neural networks with persistent storage and experiment management.

## 🌟 Features

### Core Neural Components
- **Neurons** - Leaky Integrate-and-Fire (LIF) model with temporal pattern learning
- **Axons** - Output terminals for neurons with synapse management
- **Dendrites** - Input terminals for neurons with synapse connections
- **Synapses** - Weighted connections between axons and dendrites with configurable delays
- **Action Potentials** - Spike events with timing and amplitude
- **Spike Processor** - Multi-threaded spike event processing

### Hierarchical Brain Structure
Biologically-accurate hierarchical organization with **named objects**:
- **Brain** - Top-level brain model
- **Hemisphere** - Left/right brain hemispheres
- **Lobe** - Brain lobes (frontal, parietal, occipital, temporal)
- **Region** - Functional brain regions
- **Nucleus** - Groups of neural columns
- **Column** - Cortical columns
- **Layer** - Cortical layers
- **Cluster** - Groups of neurons

### High-Performance Persistence
- **LRU Cache** - 1 million object in-memory cache
- **RocksDB Backend** - High-performance persistent storage
- **JSON Serialization** - All objects serializable to/from JSON
- **Automatic Memory Management** - Smart pointer-based lifecycle
- **Dirty Tracking** - Efficient write-back on cache eviction

### Experiment Management
- **Isolated Datastores** - Each experiment has its own persistent storage
- **Named Structures** - Human-readable names for all hierarchical objects
- **Directory Management** - Automatic creation and organization
- **Multiple Experiments** - Easy switching between different studies
- **Size Tracking** - Monitor storage usage per experiment

### Configuration System
- **JSON Configuration** - Externalized hyperparameters for reproducible experiments
- **SONATA Format** - Industry-standard network description using HDF5
- **ConfigLoader** - Type-safe configuration loading with defaults
- **SONATANetworkBuilder** - Build networks from SONATA files
- **Multiple Configurations** - Easy parameter sweeps and ablation studies

### Advanced Features
- **64-bit Unique IDs** - Support for brain-scale networks (100 trillion objects per type)
- **ID-Based Storage** - Memory-efficient ID references instead of pointers
- **Factory Pattern** - Centralized object creation with automatic ID assignment
- **Multithreading** - ThreadPool, ThreadSafe<T>, ThreadSafeRW<T>, AtomicCounter
- **Fast Logging** - spdlog-based logging with configurable levels
- **Comprehensive Tests** - 202 unit tests with 100% pass rate

## 🚀 Quick Start

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.14 or higher
- RocksDB 6.0 or higher

### Installation

#### Ubuntu/Debian
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake librocksdb-dev

# Clone repository
git clone https://github.com/deanhorak/snnfw.git
cd snnfw

# Build
mkdir build && cd build
cmake ..
make -j4

# Run tests
ctest --output-on-failure
```

#### macOS
```bash
# Install dependencies
brew install cmake rocksdb

# Clone and build
git clone https://github.com/deanhorak/snnfw.git
cd snnfw
mkdir build && cd build
cmake ..
make -j4
ctest --output-on-failure
```

### Basic Usage

```cpp
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/ExperimentConfig.h"
#include "snnfw/Datastore.h"
#include "snnfw/Brain.h"

using namespace snnfw;

int main() {
    // Create experiment configuration
    ExperimentConfig config("my_experiment");
    config.createDirectories();

    // Create datastore
    Datastore datastore(config.getDatastorePath(), 1000000);

    // Create neural objects
    NeuralObjectFactory factory;

    // Create a brain with named structures
    auto brain = factory.createBrain();
    brain->setName("My Neural Network");

    auto hemisphere = factory.createHemisphere();
    hemisphere->setName("Left Hemisphere");
    brain->addHemisphere(hemisphere->getId());

    // Create neurons
    auto neuron1 = factory.createNeuron(50.0, 0.95, 20);
    auto neuron2 = factory.createNeuron(50.0, 0.95, 20);

    // Create connectivity
    auto axon = factory.createAxon(neuron1->getId());
    neuron1->setAxonId(axon->getId());

    auto dendrite = factory.createDendrite(neuron2->getId());
    neuron2->addDendrite(dendrite->getId());

    auto synapse = factory.createSynapse(
        axon->getId(),
        dendrite->getId(),
        0.8,  // weight
        1.5   // delay (ms)
    );

    // Store in datastore
    datastore.put(brain);
    datastore.put(hemisphere);
    datastore.put(neuron1);
    datastore.put(neuron2);

    datastore.markDirty(brain->getId());
    datastore.flushAll();

    return 0;
}
```

## Core Components

### NeuralObjectFactory

The `NeuralObjectFactory` is the central component for creating all neural objects with automatic ID management:

- **Automatic ID Assignment**: Assigns unique IDs from type-specific ranges (100 trillion IDs per type)
- **Type Identification**: Object type can be determined from ID alone
- **Thread-Safe**: Safe for concurrent use from multiple threads
- **Object Counting**: Tracks number of objects created per type
- **Reset Capability**: Can reset all counters for testing

**ID Ranges:**
- Neurons: 100,000,000,000,000 - 199,999,999,999,999
- Axons: 200,000,000,000,000 - 299,999,999,999,999
- Dendrites: 300,000,000,000,000 - 399,999,999,999,999
- Synapses: 400,000,000,000,000 - 499,999,999,999,999
- Clusters: 500,000,000,000,000 - 599,999,999,999,999
- Layers: 600,000,000,000,000 - 699,999,999,999,999
- Columns: 700,000,000,000,000 - 799,999,999,999,999
- Nuclei: 800,000,000,000,000 - 899,999,999,999,999
- Regions: 900,000,000,000,000 - 999,999,999,999,999
- Lobes: 1,000,000,000,000,000 - 1,099,999,999,999,999
- Hemispheres: 1,100,000,000,000,000 - 1,199,999,999,999,999
- Brains: 1,200,000,000,000,000 - 1,299,999,999,999,999

### Hierarchical Structure Classes

All hierarchical classes use **ID-based storage** for memory efficiency:

- **Brain**: Top-level container, stores Hemisphere IDs
- **Hemisphere**: Stores Lobe IDs (left/right brain hemispheres)
- **Lobe**: Stores Region IDs (frontal, parietal, temporal, occipital)
- **Region**: Stores Nucleus IDs (functional brain areas)
- **Nucleus**: Stores Column IDs (clusters of neurons)
- **Column**: Stores Layer IDs (cortical columns)
- **Layer**: Stores Cluster IDs (cortical layers I-VI)
- **Cluster**: Stores Neuron objects (groups of neurons)

### Cellular Components

- **Neuron**: Spiking neuron with temporal pattern learning
  - Learns spike patterns in rolling time window
  - Recognizes patterns using cosine similarity
  - Adaptive memory with pattern blending

- **Axon**: Output structure of a neuron
  - Links to source neuron ID

- **Dendrite**: Input structure of a neuron
  - Links to target neuron ID

- **Synapse**: Connection between neurons
  - Configurable weight and transmission delay
  - Links axon and dendrite IDs

### Event Processing

- **ActionPotential**: Time-based event representing a spike
  - Scheduled delivery time
  - Source and target neuron IDs

- **SpikeProcessor**: Background thread for event processing
  - Time-sliced event queue
  - Parallel spike delivery
  - Configurable time step

## Architecture

The framework uses an object-oriented design with two main inheritance hierarchies:

### Physical Components (NeuralObject)
```
NeuralObject (base class with 64-bit ID)
├── Neuron (temporal pattern learning)
├── Axon (output structure)
├── Dendrite (input structure)
├── Synapse (connection)
├── Cluster (neuron container)
├── Layer (cluster container, ID-based)
├── Column (layer container, ID-based)
├── Nucleus (column container, ID-based)
├── Region (nucleus container, ID-based)
├── Lobe (region container, ID-based)
├── Hemisphere (lobe container, ID-based)
└── Brain (hemisphere container, ID-based)
```

### Time-Based Events (EventObject)
```
EventObject (base class with scheduled time)
└── ActionPotential (spike event)
```

**Key Design Principles:**
- **ID-Based Storage**: Hierarchical containers store child IDs (not objects) for memory efficiency
- **Type Identification**: Object type determinable from ID alone (no RTTI needed)
- **Factory Pattern**: Centralized object creation with automatic ID management
- **Thread Safety**: Mutex-protected ID generation and event processing
- **Scalability**: 100 trillion IDs per type supports brain-scale simulations

## 📚 Documentation

### Core Framework
- **[EXPERIMENT_MANAGEMENT.md](EXPERIMENT_MANAGEMENT.md)** - Complete guide to experiment management
- **[DATASTORE_ARCHITECTURE.md](DATASTORE_ARCHITECTURE.md)** - Datastore implementation details
- **[NEURAL_CONNECTIVITY.md](NEURAL_CONNECTIVITY.md)** - Neural connectivity architecture
- **[FEATURE_SUMMARY.md](FEATURE_SUMMARY.md)** - Recent feature additions
- **[INSTALL_ROCKSDB.md](INSTALL_ROCKSDB.md)** - RocksDB installation guide

### Configuration System
- **[docs/CONFIGURATION_SYSTEM.md](docs/CONFIGURATION_SYSTEM.md)** - Complete guide to configuration system
- **[configs/README.md](configs/README.md)** - Available configurations and usage

### Experiments
- **[MNIST_EXPERIMENTS.md](MNIST_EXPERIMENTS.md)** - MNIST digit recognition experiments (81.20% accuracy)

## 🎯 Examples

The `examples/` directory contains working demonstrations:

- **datastore_example.cpp** - Basic datastore usage
- **experiment_config_example.cpp** - Experiment management
- **hierarchical_structure_example.cpp** - Building brain hierarchies
- **factory_example.cpp** - Using the NeuralObjectFactory
- **neural_network_example.cpp** - Complete neural network with spike processing
- **realtime_spike_example.cpp** - Real-time spike processing demonstration

Build and run examples:
```bash
cd build
make experiment_config_example
./experiment_config_example
```

## 🧠 MNIST Digit Recognition

The `experiments/` directory contains MNIST digit recognition experiments demonstrating spike-based pattern matching:

**Current Best Result: 81.20% accuracy** using:
- Edge-based feature detection (8 orientations)
- Spike-based pattern learning
- k-Nearest Neighbors classification (k=5)
- 7×7 spatial grid (49 regions)
- Configurable via JSON and SONATA format

See **[MNIST_EXPERIMENTS.md](MNIST_EXPERIMENTS.md)** for complete details.

### Quick Start

```bash
cd build
make mnist_optimized -j4

# Run with default configuration (81.20% accuracy)
LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH SPDLOG_LEVEL=error ./mnist_optimized ../configs/mnist_config.json

# Run with fast configuration (quick testing)
./mnist_optimized ../configs/mnist_config_fast.json

# Run with different k value
./mnist_optimized ../configs/mnist_config_k3.json
```

### Available Configurations

- **`mnist_config.json`** - Default (81.20% accuracy, 7×7 grid, k=5)
- **`mnist_config_5x5.json`** - Smaller grid (faster, ~75-78% accuracy)
- **`mnist_config_k3.json`** - k=3 neighbors
- **`mnist_config_k10.json`** - k=10 neighbors
- **`mnist_config_fast.json`** - Quick testing (1000 examples/digit)
- **`mnist_config_no_sonata.json`** - Manual network construction

See **[configs/README.md](configs/README.md)** for detailed configuration documentation.

## 🧪 Testing

The framework includes comprehensive tests:

```bash
cd build
ctest --output-on-failure
```

**Test Suites:**
- NeuronTests - Neuron functionality
- ClusterTests - Cluster management
- ThreadingTests - Multithreading utilities
- NeuralComponentsTests - Axons, dendrites, synapses
- SpikeProcessorTests - Spike event processing
- NeuralObjectFactoryTests - Factory pattern
- HierarchicalStructuresTests - Brain hierarchy
- DatastoreTests - Persistence layer

**Current Status:** ✅ 100% tests passing (202 tests)

## 🏗️ Project Structure

```
snnfw/
├── include/snnfw/          # Public headers
│   ├── NeuralObject.h      # Base class
│   ├── Neuron.h            # Neuron implementation
│   ├── Axon.h              # Axon implementation
│   ├── Dendrite.h          # Dendrite implementation
│   ├── Synapse.h           # Synapse implementation
│   ├── Brain.h             # Brain hierarchy
│   ├── Datastore.h         # Persistence layer
│   ├── ExperimentConfig.h  # Experiment management
│   └── ...
├── src/                    # Implementation files
├── tests/                  # Unit tests
├── examples/               # Example programs
├── build/                  # Build directory (generated)
└── CMakeLists.txt          # Build configuration
```

## 📄 License

[Add your license here]

## 👥 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📧 Contact

Dean Horak - dean.horak@gmail.com

Project Link: [https://github.com/deanhorak/snnfw](https://github.com/deanhorak/snnfw)

## 🙏 Acknowledgments

- **spdlog** - Fast C++ logging library
- **nlohmann/json** - JSON for Modern C++
- **RocksDB** - Persistent key-value store
- **Google Test** - C++ testing framework
