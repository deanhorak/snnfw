# SNNFW Configuration System

## Overview

The SNNFW framework now supports **externalized configuration** and **SONATA network format** for flexible, reproducible experiments. This allows you to:

- **Change hyperparameters** without recompiling
- **Version control** experiment configurations
- **Share configurations** with collaborators
- **Use industry-standard SONATA format** for network structure
- **Easily run parameter sweeps** and ablation studies

## Architecture

The configuration system consists of three main components:

### 1. ConfigLoader (`include/snnfw/ConfigLoader.h`)

A type-safe JSON configuration loader that provides:

- **Type-safe access** to configuration values with `get<T>(path, default)`
- **Required parameters** with `getRequired<T>(path)` that throw if missing
- **Nested access** using JSON pointer syntax (e.g., `/network/grid_size`)
- **Section extraction** with `getSection(path)` for grouped parameters
- **Existence checking** with `has(path)`

### 2. SONATANetworkBuilder (`include/snnfw/SONATANetworkBuilder.h`)

Builds neural networks from SONATA HDF5 files:

- **Reads node populations** with neuron properties
- **Reads edge populations** with synapse connectivity
- **Extracts metadata** from SONATA files
- **Creates SNNFW neurons** with parameters from SONATA attributes

### 3. JSON Configuration Files

Human-readable JSON files that define:

- **Network structure** (grid size, orientations, neurons per feature)
- **Neuron parameters** (temporal window, similarity threshold, max patterns)
- **Training parameters** (examples per digit, test images)
- **Classification parameters** (method, k-neighbors)
- **Data paths** (training/test images and labels)
- **SONATA settings** (network file, population names, enable/disable)

## Configuration File Format

### Example: `configs/mnist_config.json`

```json
{
  "network": {
    "grid_size": 7,
    "region_size": 4,
    "num_orientations": 8,
    "neurons_per_feature": 1,
    "edge_threshold": 0.15
  },
  "neuron": {
    "window_size_ms": 200.0,
    "similarity_threshold": 0.7,
    "max_patterns": 100
  },
  "training": {
    "examples_per_digit": 5000,
    "test_images": 10000,
    "spike_duration_ms": 100.0
  },
  "classification": {
    "method": "knn",
    "k_neighbors": 5
  },
  "data": {
    "train_images": "/path/to/train-images-idx3-ubyte",
    "train_labels": "/path/to/train-labels-idx1-ubyte",
    "test_images": "/path/to/t10k-images-idx3-ubyte",
    "test_labels": "/path/to/t10k-labels-idx1-ubyte"
  },
  "sonata": {
    "network_file": "../configs/mnist_network.h5",
    "use_sonata": true,
    "node_population": "layer1_neurons",
    "edge_population": "layer1_synapses"
  }
}
```

### Configuration Sections

#### `network`
- **`grid_size`**: Number of regions per dimension (7 = 7×7 grid = 49 regions)
- **`region_size`**: Size of each region in pixels (4 = 4×4 pixels)
- **`num_orientations`**: Number of edge orientations to detect (8 = 0°, 22.5°, 45°, ...)
- **`neurons_per_feature`**: Neurons per feature detector (typically 1)
- **`edge_threshold`**: Threshold for edge detection (0.0-1.0)

#### `neuron`
- **`window_size_ms`**: Temporal window for spike pattern matching (milliseconds)
- **`similarity_threshold`**: Cosine similarity threshold for pattern matching (0.0-1.0)
- **`max_patterns`**: Maximum patterns each neuron can store

#### `training`
- **`examples_per_digit`**: Training examples per digit class (5000 = 50,000 total)
- **`test_images`**: Number of test images to evaluate (10,000 = full test set)
- **`spike_duration_ms`**: Duration of spike encoding window (milliseconds)

#### `classification`
- **`method`**: Classification method (`"knn"` or `"voting"`)
- **`k_neighbors`**: Number of neighbors for k-NN classification

#### `data`
- **`train_images`**: Path to MNIST training images file
- **`train_labels`**: Path to MNIST training labels file
- **`test_images`**: Path to MNIST test images file
- **`test_labels`**: Path to MNIST test labels file

#### `sonata`
- **`network_file`**: Path to SONATA HDF5 network file
- **`use_sonata`**: Enable/disable SONATA network loading (`true`/`false`)
- **`node_population`**: Name of node population in SONATA file
- **`edge_population`**: Name of edge population in SONATA file

## SONATA Network Format

### What is SONATA?

**SONATA** (Scalable Open Network Architecture TemplAte) is an industry-standard format for describing large-scale neural network models, developed by the Blue Brain Project.

### SONATA File Structure

SONATA uses **HDF5** (Hierarchical Data Format) to store:

- **Nodes** (neurons) with properties like position, type, parameters
- **Edges** (synapses) with connectivity and weights
- **Metadata** describing the network structure

### Generating SONATA Networks

Use the provided Python script to generate SONATA network files:

```bash
python3 scripts/generate_mnist_sonata.py
```

This creates `configs/mnist_network.h5` with:
- **392 neurons** (7×7 grid × 8 orientations × 1 neuron per feature)
- **Node properties**: `window_size_ms`, `similarity_threshold`, `max_patterns`, `region_row`, `region_col`, `orientation`, `neuron_index`
- **Metadata**: `grid_size`, `num_orientations`, `neurons_per_feature`

### SONATA Node Attributes

Each neuron in the SONATA file has these attributes:

| Attribute | Type | Description |
|-----------|------|-------------|
| `window_size_ms` | double | Temporal window for pattern matching (ms) |
| `similarity_threshold` | double | Cosine similarity threshold (0.0-1.0) |
| `max_patterns` | uint64 | Maximum patterns to store |
| `region_row` | int32 | Row index in grid (0-6 for 7×7 grid) |
| `region_col` | int32 | Column index in grid (0-6 for 7×7 grid) |
| `orientation` | int32 | Edge orientation index (0-7 for 8 orientations) |
| `neuron_index` | int32 | Neuron index within feature (0 for single neuron) |

## Using the Configuration System

### Basic Usage

```cpp
#include "snnfw/ConfigLoader.h"

// Load configuration
ConfigLoader config("configs/mnist_config.json");

// Get values with defaults
int gridSize = config.get<int>("/network/grid_size", 5);
double threshold = config.get<double>("/neuron/similarity_threshold", 0.7);

// Get required values (throws if missing)
std::string trainImages = config.getRequired<std::string>("/data/train_images");

// Check if value exists
if (config.has("/sonata/use_sonata")) {
    bool useSonata = config.get<bool>("/sonata/use_sonata", false);
}
```

### Loading SONATA Networks

```cpp
#include "snnfw/SONATANetworkBuilder.h"

// Initialize builder
SONATANetworkBuilder builder("configs/mnist_network.h5");

// Build neurons from SONATA file
auto neurons = builder.buildNeurons(
    "layer1_neurons",  // population name
    7,                 // grid size
    8,                 // num orientations
    1                  // neurons per feature
);

// Get metadata
auto metadata = builder.getNeuronMetadata("layer1_neurons");
std::cout << "Node count: " << metadata["node_count"] << std::endl;
```

### Running Experiments with Custom Configurations

```bash
# Use default configuration
./mnist_optimized

# Use custom configuration
./mnist_optimized path/to/custom_config.json
```

## Creating Custom Configurations

### Example: Smaller Grid (5×5)

```json
{
  "network": {
    "grid_size": 5,
    "region_size": 5,
    "num_orientations": 8,
    "neurons_per_feature": 1,
    "edge_threshold": 0.15
  },
  ...
}
```

Don't forget to regenerate the SONATA file:

```bash
# Edit scripts/generate_mnist_sonata.py to use grid_size=5
python3 scripts/generate_mnist_sonata.py
```

### Example: Different k-NN Parameter

```json
{
  "classification": {
    "method": "knn",
    "k_neighbors": 3
  },
  ...
}
```

### Example: Disable SONATA (Manual Network Construction)

```json
{
  "sonata": {
    "use_sonata": false
  },
  ...
}
```

## Benefits

### 1. **Reproducibility**
- Configuration files can be versioned with git
- Exact experiment parameters are documented
- Easy to reproduce results from papers

### 2. **Flexibility**
- Change parameters without recompiling
- Quick iteration on hyperparameters
- Easy A/B testing

### 3. **Collaboration**
- Share configurations with team members
- Standard format (JSON + SONATA)
- Clear documentation of experiment setup

### 4. **Experimentation**
- Run parameter sweeps easily
- Ablation studies with minimal code changes
- Compare different network architectures

## Advanced Topics

### Custom SONATA Generation

Modify `scripts/generate_mnist_sonata.py` to create custom network structures:

```python
# Example: Add custom neuron properties
node_type_id = np.zeros(num_neurons, dtype=np.int32)
node_type_id[::2] = 0  # Even neurons: type 0
node_type_id[1::2] = 1  # Odd neurons: type 1

nodes.create_dataset('node_type_id', data=node_type_id)
```

### Fallback to Manual Construction

If SONATA loading fails, the system automatically falls back to manual network construction:

```cpp
if (config.useSonata) {
    try {
        neurons = builder.buildNeurons(...);
    } catch (const std::exception& e) {
        std::cerr << "SONATA failed, using manual construction" << std::endl;
        neurons = buildManualNetwork(...);
    }
}
```

## Troubleshooting

### "Failed to open config file"
- Check that the path is correct relative to your working directory
- Use absolute paths if needed

### "Unable to open file configs/mnist_network.h5"
- Run `python3 scripts/generate_mnist_sonata.py` to generate the file
- Check that the path in the config matches the actual file location

### "Failed to open image file"
- Update the `data` section paths to point to your MNIST dataset
- Use absolute paths to avoid working directory issues

## See Also

- [MNIST Experiments Documentation](../MNIST_EXPERIMENTS.md)
- [SONATA Format Specification](https://github.com/AllenInstitute/sonata)
- [libsonata Documentation](https://github.com/BlueBrain/libsonata)

