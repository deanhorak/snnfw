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

### 🔌 Adapter System (NEW!)
Modular input/output framework for connecting SNNs to external data sources and actuators:
- **SensoryAdapters** - Convert external data (images, audio, sensors) into spike trains
- **MotorAdapters** - Convert spike trains into external actions (displays, motors, actuators)
- **RetinaAdapter** - Visual processing with edge detection (7×7 grid, 8 orientations, 392 neurons)
- **AudioAdapter** - Audio processing with FFT and mel-scale frequency channels
- **DisplayAdapter** - Neural activity visualization (raster, heatmap, vector, ASCII modes)
- **Custom Adapters** - Easy creation of domain-specific adapters
- **Encoding Strategies** - Rate coding, temporal coding, population coding
- **Decoding Strategies** - Rate decoding, population vector, winner-take-all

### 🎯 Hyperparameter Optimization (NEW!)
Automated hyperparameter tuning with multiple optimization strategies:
- **Grid Search** - Exhaustive search over parameter grid
- **Random Search** - Random sampling of parameter space (recommended)
- **Bayesian Optimization** - Model-based optimization with Gaussian Process
- **Genetic Algorithm** - Evolutionary optimization with crossover and mutation
- **General-Purpose** - Works with any experiment, not just MNIST
- **Result Tracking** - Automatic saving of all intermediate results
- **Resume Capability** - Continue interrupted optimization runs
- **JSON Configuration** - Easy parameter space specification

### 🎨 3D Visualization System (NEW!)
Real-time OpenGL-based visualization of network structure and activity:
- **Network Structure** - 3D visualization of hierarchical brain organization
- **Activity Visualization** - Real-time spike events with particle systems
- **Layout Algorithms** - Force-directed, hierarchical, and grid layouts
- **Interactive Camera** - Mouse/keyboard controls with pan/zoom/rotate
- **Recording & Playback** - Save and replay network activity
- **Raster Plots** - Traditional neuroscience spike visualization
- **Pattern Detection** - Visual highlighting of detected patterns
- **ImGui Interface** - Interactive controls and statistics

### 📥 Model Import & Position Storage (NEW!)
Import neuron positions from industry-standard neuroscience formats:
- **NeuroML** - XML-based model description language (NeuroML v2)
- **HDF5** - High-performance binary format (SONATA, NWB, generic)
- **CSV** - Simple comma-separated position files
- **SWC** - NEURON morphology files
- **NEST JSON** - NEST simulator export format
- **Position Storage** - 3D coordinates stored in all neural objects
- **Position Transformation** - Scale and offset for coordinate system conversion
- **Hybrid Layouts** - Mix imported positions with computed layouts
- **Visualization Integration** - Imported positions automatically used in 3D view

### Advanced Features
- **64-bit Unique IDs** - Support for brain-scale networks (100 trillion objects per type)
- **ID-Based Storage** - Memory-efficient ID references instead of pointers
- **Factory Pattern** - Centralized object creation with automatic ID assignment
- **Multithreading** - ThreadPool, ThreadSafe<T>, ThreadSafeRW<T>, AtomicCounter
- **Fast Logging** - spdlog-based logging with configurable levels
- **Comprehensive Tests** - 121+ unit tests with 97.6% pass rate

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

### Adapter System Usage

```cpp
#include "snnfw/adapters/RetinaAdapter.h"
#include "snnfw/ConfigLoader.h"

using namespace snnfw;
using namespace snnfw::adapters;

// Load configuration
ConfigLoader loader("../configs/mnist_config_with_adapters.json");
auto retinaConfig = loader.getAdapterConfig("retina");

// Create retina adapter
auto retina = std::make_shared<RetinaAdapter>(retinaConfig);
retina->initialize();

// Process image
SensoryAdapter::DataSample sample;
sample.rawData = imagePixels;  // 28x28 grayscale image
sample.timestamp = 0.0;

retina->processData(sample);

// Get activation pattern for classification
auto activations = retina->getActivationPattern();

// Train neurons
for (auto& neuron : retina->getNeurons()) {
    neuron->learnCurrentPattern();
}
```

### Hyperparameter Optimization Usage

```cpp
#include "snnfw/HyperparameterOptimizer.h"

using namespace snnfw;

// Create optimizer
auto optimizer = std::make_shared<HyperparameterOptimizer>(
    OptimizationStrategy::RANDOM_SEARCH);

// Add parameters to optimize
ParameterSpec gridSize;
gridSize.name = "network.grid_size";
gridSize.type = ParameterSpec::Type::INTEGER;
gridSize.minValue = 5;
gridSize.maxValue = 9;
gridSize.step = 1;
optimizer->addParameter(gridSize);

// Set objective function
optimizer->setObjective([](const ParameterConfig& params) -> ExperimentResult {
    double accuracy = runExperiment(params);
    ExperimentResult result;
    result.config = params;
    result.score = accuracy;
    return result;
});

// Run optimization
optimizer->setMaxIterations(50);
auto best = optimizer->optimize();

std::cout << "Best accuracy: " << best.score << std::endl;
```

Or use the command-line tool:
```bash
# Random search with 50 iterations
./mnist_hyperparameter_optimization ../configs/mnist_config.json random 50 1000 1000

# Arguments: <config> <strategy> <iterations> <train_samples> <test_samples>
# Strategies: grid, random, bayesian, genetic
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
├── Neuron (temporal pattern learning, dendrite container, ID-based)
├── Axon (output structure, ID-based)
├── Dendrite (input structure, ID-based)
├── Synapse (connection, ID-based)
├── Cluster (neuron container, ID-based)
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

### Adapter System
- **[docs/ADAPTER_SYSTEM.md](docs/ADAPTER_SYSTEM.md)** - Complete adapter system guide
- **[docs/ADAPTER_IMPLEMENTATION_SUMMARY.md](docs/ADAPTER_IMPLEMENTATION_SUMMARY.md)** - Implementation summary

### Hyperparameter Optimization
- **[docs/HYPERPARAMETER_OPTIMIZATION.md](docs/HYPERPARAMETER_OPTIMIZATION.md)** - Complete optimization guide
- **[docs/HYPERPARAMETER_OPTIMIZATION_SUMMARY.md](docs/HYPERPARAMETER_OPTIMIZATION_SUMMARY.md)** - Implementation summary

### Experiments
- **[MNIST_EXPERIMENTS.md](MNIST_EXPERIMENTS.md)** - MNIST digit recognition experiments (94.96% accuracy with optimized hyperparameters)
- **[MNIST_V1_MULTICOLUMN.md](MNIST_V1_MULTICOLUMN.md)** - Multi-column V1 architecture with canonical cortical microcircuits (69.56% accuracy)

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

## 🔌 Adapter System

The Adapter System provides a modular framework for connecting SNNs to external data sources and actuators. This transforms SNNFW from a specialized framework into a general-purpose neuromorphic computing platform.

### Quick Start with Adapters

```cpp
#include "snnfw/adapters/RetinaAdapter.h"

// Create and configure adapter
BaseAdapter::Config config;
config.name = "retina";
config.temporalWindow = 100.0;
config.intParams["grid_width"] = 7;
config.intParams["grid_height"] = 7;
config.intParams["num_orientations"] = 8;

auto retina = std::make_shared<RetinaAdapter>(config);
retina->initialize();

// Process image data
std::vector<uint8_t> imageData = loadImage("digit.png", 28, 28);
retina->processData(imageData);

// Get neural activation pattern
auto activations = retina->getActivationPattern();
```

### Available Adapters

- **RetinaAdapter** - Visual processing with edge detection (94.96% MNIST accuracy)
- **AudioAdapter** - Audio processing with FFT and mel-scale channels
- **DisplayAdapter** - Neural activity visualization (raster, heatmap, vector, ASCII)
- **Custom Adapters** - Easy to create domain-specific adapters

### Examples

```bash
cd build

# MNIST with RetinaAdapter (94.96% accuracy with optimized hyperparameters)
make mnist_classification_strategies -j4
./mnist_classification_strategies ../configs/mnist_8x8_optimized.json

# Display visualization
make display_visualization -j4
./display_visualization

# Custom temperature sensor adapter
make custom_adapter -j4
./custom_adapter
```

See **[docs/ADAPTER_SYSTEM.md](docs/ADAPTER_SYSTEM.md)** for complete documentation.

## 🧠 MNIST Digit Recognition

The `experiments/` directory contains MNIST digit recognition experiments demonstrating spike-based pattern matching:

### Approach 1: RetinaAdapter with k-NN Classification

**Current Best Result: 94.96% accuracy** using:
- RetinaAdapter with Sobel edge detection (8 orientations)
- Spike-based pattern learning
- k-Nearest Neighbors classification (k=5)
- **8×8 spatial grid (512 neurons)** - Optimized for higher spatial resolution
- **Optimized edge_threshold: 0.165** - Fine-tuned from 0.15
- **Pluggable classification strategies** (MajorityVoting, WeightedDistance, WeightedSimilarity)
- Configurable via JSON

See **[MNIST_EXPERIMENTS.md](MNIST_EXPERIMENTS.md)** for complete details.

### Approach 2: Multi-Column V1 Architecture with STDP Learning

**Result: 69.56% accuracy** using:
- **12 cortical columns** with orientation selectivity (0°, 15°, 30°, ..., 165°)
- **6 layers per column** (canonical cortical microcircuit: L1, L2/3, L4, L5, L6)
- **3,840 neurons total** with ~195,000 synapses
- **Gabor filtering** for orientation-selective edge detection (9×9 kernels)
- **Explicit Axon/Synapse/Dendrite connectivity**
- **STDP learning** with reward modulation
- **Population-based classification** (20 neurons per digit)
- Biologically plausible spiking neural network

See **[MNIST_V1_MULTICOLUMN.md](MNIST_V1_MULTICOLUMN.md)** for complete details.

## 🔤 EMNIST Letters Classification

The `experiments/` directory also contains EMNIST letters (A-Z) classification experiments demonstrating advanced multi-column architecture with saccades:

### Multi-Column Architecture with Saccades and Optimization

**Current Best Result: 71.93% accuracy** using:
- **24 cortical columns** with diverse feature detectors:
  - 16 orientation columns (8 orientations × 2 frequencies)
  - 6 center-surround columns (3 scales × 2 types)
  - 2 specialized detectors (top-loop, gap)
- **6 layers per column** (L1, L2/3, L4, L5, L6) with optimized neuron counts
- **16,542 neurons total** with ~4.9M synapses
- **Saccade-based sequential spatial attention** (4 fixations: TOP, BOTTOM, CENTER, FULL)
- **Temporal integration** (500ms window spanning all fixations)
- **Optimized hyperparameters** via 80-trial Optuna optimization:
  - Layer 2/3: 448 neurons (integration layer)
  - Layer 5: 80 neurons (output layer)
  - 15 neurons per letter (390 output neurons total)
  - Gabor threshold: 0.24 (high selectivity)
  - Similarity threshold: 0.93 (selective pattern matching)
  - Recurrent weight: 0.3 (balanced feedback)
- **Population-based classification** with spike-based pattern learning
- **+4.53% improvement** over baseline through systematic optimization

**Key Achievements:**
- ✅ Saccades implementation: +1.86% improvement
- ✅ 50-trial optimization: +2.65% improvement (71.91% accuracy)
- ✅ Focused 30-trial optimization: +0.02% improvement (71.93% accuracy)
- ✅ Stable and reproducible results (verified)

See **[FINAL_OPTIMIZATION_REPORT.md](FINAL_OPTIMIZATION_REPORT.md)** for complete optimization details.

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

**Optimized Configurations (with Adapter System):**
- **`mnist_8x8_majority.json`** - **Best: 94.76% accuracy** (8×8 grid, 512 neurons, Majority Voting)
- **`mnist_8x8_weighted_distance.json`** - 94.76% accuracy (8×8 grid, Distance-Weighted k-NN)
- **`mnist_8x8_weighted_similarity.json`** - 94.76% accuracy (8×8 grid, Similarity-Weighted k-NN)
- **`mnist_sobel_rate_8x8.json`** - 94.63% accuracy (8×8 grid, 512 neurons)
- **`mnist_sobel_rate_9x9.json`** - 94.61% accuracy (9×9 grid, 648 neurons)
- **`mnist_sobel_rate.json`** - 92.71% accuracy (7×7 grid, 392 neurons)
- **`mnist_sobel_rate_5x5.json`** - 89.90% accuracy (5×5 grid, 200 neurons)
- **`mnist_sobel_rate_4x4.json`** - 83.20% accuracy (4×4 grid, 128 neurons)

**Legacy Configurations:**
- **`mnist_config.json`** - 81.20% accuracy (7×7 grid, inline implementation)
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
