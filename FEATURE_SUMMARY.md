# SNNFW Feature Summary - Named Hierarchical Structures & Experiment Management

## Overview

This document summarizes the new features added to the SNNFW (Spiking Neural Network Framework) for managing named hierarchical neural structures and experiment-specific datastores.

## New Features

### 1. Named Hierarchical Neural Objects

All hierarchical neural objects now support human-readable names for better organization and identification.

#### Supported Objects

- **Nucleus** - Named groups of columns
- **Region** - Named brain regions  
- **Lobe** - Named brain lobes
- **Hemisphere** - Named hemispheres
- **Brain** - Named brain models

#### API Changes

Each hierarchical class now includes:

```cpp
// Constructor with optional name parameter
explicit ClassName(uint64_t id = 0, const std::string& name = "");

// Name getter and setter
void setName(const std::string& name);
const std::string& getName() const;
```

#### Serialization

Names are automatically included in JSON serialization:

```json
{
  "type": "Brain",
  "id": 1200000000000000,
  "name": "Human Visual System Model",
  "hemisphereIds": [...]
}
```

The `fromJson()` method uses `j.value("name", "")` to provide backward compatibility with objects that don't have names.

### 2. ExperimentConfig Class

A new `ExperimentConfig` class provides experiment-specific configuration and datastore management.

#### Key Features

- **Isolated Datastores** - Each experiment has its own RocksDB datastore
- **Directory Management** - Automatic creation and management of experiment directories
- **Custom Paths** - Support for custom subdirectories within experiments
- **Size Tracking** - Query experiment storage size
- **Cleanup** - Safe deletion of entire experiments

#### API

```cpp
class ExperimentConfig {
public:
    ExperimentConfig(const std::string& experimentName, 
                    const std::string& baseDir = "./experiments");
    
    const std::string& getName() const;
    std::string getDatastorePath() const;
    std::string getExperimentDirectory() const;
    std::string getCustomPath(const std::string& subpath) const;
    
    bool createDirectories() const;
    bool exists() const;
    bool deleteExperiment() const;
    size_t getExperimentSize() const;
    
    void setBaseDirectory(const std::string& baseDir);
};
```

#### Directory Structure

```
./experiments/
├── experiment1/
│   └── datastore/
├── experiment2/
│   └── datastore/
└── experiment3/
    └── datastore/
```

## Modified Files

### Header Files

1. **include/snnfw/Nucleus.h**
   - Added `name` member variable
   - Added `setName()` and `getName()` methods
   - Updated constructor to accept optional name
   - Updated `toJson()` to include name
   - Updated `fromJson()` to deserialize name

2. **include/snnfw/Region.h**
   - Same changes as Nucleus

3. **include/snnfw/Lobe.h**
   - Same changes as Nucleus

4. **include/snnfw/Hemisphere.h**
   - Same changes as Nucleus

5. **include/snnfw/Brain.h**
   - Same changes as Nucleus

6. **include/snnfw/ExperimentConfig.h** (NEW)
   - Complete experiment configuration management class

## New Files

1. **include/snnfw/ExperimentConfig.h** - Experiment configuration class
2. **examples/experiment_config_example.cpp** - Comprehensive example demonstrating:
   - Creating named brain structures
   - Persisting to experiment-specific datastores
   - Loading experiments
   - Managing multiple experiments
   - Cleanup operations
3. **EXPERIMENT_MANAGEMENT.md** - Complete documentation for experiment management
4. **FEATURE_SUMMARY.md** - This file

## Usage Examples

### Creating Named Structures

```cpp
NeuralObjectFactory factory;

auto brain = factory.createBrain();
brain->setName("Human Visual System Model");

auto hemisphere = factory.createHemisphere();
hemisphere->setName("Left Hemisphere");

auto lobe = factory.createLobe();
lobe->setName("Occipital Lobe");

auto region = factory.createRegion();
region->setName("Primary Visual Cortex (V1)");

auto nucleus = factory.createNucleus();
nucleus->setName("V1 Layer 4C");
```

### Using Experiment Configuration

```cpp
// Create experiment
ExperimentConfig config("my_experiment");
config.createDirectories();

// Create datastore for this experiment
Datastore datastore(config.getDatastorePath(), 1000000);

// ... work with experiment ...

// Load different experiment
ExperimentConfig config2("another_experiment");
Datastore datastore2(config2.getDatastorePath(), 1000000);
```

### Command-Line Integration

```cpp
int main(int argc, char* argv[]) {
    std::string experimentName = (argc > 1) ? argv[1] : "default";
    
    ExperimentConfig config(experimentName);
    config.createDirectories();
    
    Datastore datastore(config.getDatastorePath(), 1000000);
    
    // ... run experiment ...
}
```

Run with:
```bash
./my_app experiment1
./my_app experiment2
```

## Backward Compatibility

### Serialization

The implementation maintains backward compatibility:

- **Old objects without names**: Will deserialize with empty names
- **New objects with names**: Will serialize and deserialize names correctly
- Uses `j.value("name", "")` to provide default empty string if name field is missing

### Constructor Compatibility

All constructors maintain backward compatibility:

```cpp
// Old style (still works)
auto brain = factory.createBrain();

// New style (with name)
auto brain = std::make_shared<Brain>(id, "My Brain");
```

## Testing

All existing tests continue to pass:

- ✅ NeuronTests
- ✅ ClusterTests  
- ✅ ThreadingTests
- ✅ NeuralComponentsTests
- ✅ SpikeProcessorTests
- ✅ NeuralObjectFactoryTests
- ✅ HierarchicalStructuresTests
- ✅ DatastoreTests

## Examples

Three example programs demonstrate the new features:

1. **datastore_example** - Basic datastore usage
2. **experiment_config_example** - Complete experiment management demonstration
3. **hierarchical_structure_example** - Hierarchical structure creation

Build and run:
```bash
cd build
make experiment_config_example
./experiment_config_example
```

## Benefits

### 1. Improved Organization

- Human-readable names make it easier to identify neural structures
- Names are persisted and loaded automatically
- Better debugging and logging with meaningful names

### 2. Experiment Isolation

- Each experiment has its own datastore
- No risk of data conflicts between experiments
- Easy to switch between experiments

### 3. Scalability

- Support for unlimited experiments
- Each experiment can be managed independently
- Easy cleanup of old experiments

### 4. Flexibility

- Custom base directories for different storage locations
- Custom subdirectories within experiments
- Command-line integration for easy experiment selection

## Future Enhancements

Potential future improvements:

1. **Experiment Metadata** - Store experiment description, creation date, parameters
2. **Experiment Cloning** - Copy experiments for variations
3. **Experiment Comparison** - Tools to compare results across experiments
4. **Name-Based Lookup** - Find objects by name in addition to ID
5. **Experiment Versioning** - Track changes to experiments over time
6. **Experiment Export/Import** - Share experiments between systems

## Documentation

Complete documentation is available in:

- **EXPERIMENT_MANAGEMENT.md** - Comprehensive guide to experiment management
- **DATASTORE_ARCHITECTURE.md** - Datastore implementation details
- **examples/experiment_config_example.cpp** - Working code examples

## Summary

The addition of named hierarchical structures and experiment management provides:

✅ **Better Organization** - Human-readable names for all hierarchical objects  
✅ **Experiment Isolation** - Separate datastores for each experiment  
✅ **Easy Management** - Simple API for creating, loading, and deleting experiments  
✅ **Backward Compatible** - Existing code continues to work  
✅ **Well Tested** - All tests pass  
✅ **Well Documented** - Complete documentation and examples  

These features make SNNFW more suitable for research workflows where multiple experiments need to be managed and compared.

