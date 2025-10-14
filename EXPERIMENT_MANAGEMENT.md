# Experiment Management in SNNFW

## Overview

SNNFW provides a comprehensive experiment management system that allows you to:

1. **Create named hierarchical neural structures** - All hierarchical objects (Nucleus, Region, Lobe, Hemisphere, Brain) can be assigned human-readable names
2. **Manage experiment-specific datastores** - Each experiment has its own isolated persistent storage
3. **Switch between experiments** - Easily work with multiple experiments without data conflicts

## Named Hierarchical Structures

### Supported Named Objects

The following hierarchical neural objects support naming:

- **Nucleus** - Named groups of columns (e.g., "V1 Layer 4C", "Thalamic Nucleus")
- **Region** - Named brain regions (e.g., "Primary Visual Cortex (V1)", "Motor Cortex")
- **Lobe** - Named brain lobes (e.g., "Occipital Lobe", "Frontal Lobe")
- **Hemisphere** - Named hemispheres (e.g., "Left Hemisphere", "Right Hemisphere")
- **Brain** - Named brain models (e.g., "Human Visual System Model", "Rodent Motor Control")

### Using Named Objects

```cpp
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Brain.h"
#include "snnfw/Hemisphere.h"
#include "snnfw/Lobe.h"

NeuralObjectFactory factory;

// Create a brain with a name
auto brain = factory.createBrain();
brain->setName("Human Visual System Model");

// Create named hemispheres
auto leftHemisphere = factory.createHemisphere();
leftHemisphere->setName("Left Hemisphere");

auto rightHemisphere = factory.createHemisphere();
rightHemisphere->setName("Right Hemisphere");

// Add hemispheres to brain
brain->addHemisphere(leftHemisphere->getId());
brain->addHemisphere(rightHemisphere->getId());

// Create named lobes
auto occipitalLobe = factory.createLobe();
occipitalLobe->setName("Occipital Lobe");
leftHemisphere->addLobe(occipitalLobe->getId());

// Create named regions
auto v1Region = factory.createRegion();
v1Region->setName("Primary Visual Cortex (V1)");
occipitalLobe->addRegion(v1Region->getId());

// Create named nuclei
auto nucleus = factory.createNucleus();
nucleus->setName("V1 Layer 4C");
v1Region->addNucleus(nucleus->getId());

// Access names
std::cout << "Brain: " << brain->getName() << std::endl;
std::cout << "Hemisphere: " << leftHemisphere->getName() << std::endl;
std::cout << "Lobe: " << occipitalLobe->getName() << std::endl;
std::cout << "Region: " << v1Region->getName() << std::endl;
std::cout << "Nucleus: " << nucleus->getName() << std::endl;
```

### Serialization

Names are automatically included in JSON serialization:

```json
{
  "type": "Brain",
  "id": 1200000000000000,
  "name": "Human Visual System Model",
  "hemisphereIds": [1100000000000000, 1100000000000001]
}
```

## Experiment Configuration

### ExperimentConfig Class

The `ExperimentConfig` class manages experiment-specific settings and datastore paths.

```cpp
#include "snnfw/ExperimentConfig.h"

// Create an experiment configuration
ExperimentConfig config("my_experiment");

// Get experiment information
std::string name = config.getName();                    // "my_experiment"
std::string expDir = config.getExperimentDirectory();   // "./experiments/my_experiment"
std::string dbPath = config.getDatastorePath();         // "./experiments/my_experiment/datastore"

// Create the experiment directory structure
config.createDirectories();

// Check if experiment exists
if (config.exists()) {
    std::cout << "Experiment exists!" << std::endl;
}

// Get experiment size
size_t size = config.getExperimentSize();
std::cout << "Experiment size: " << size << " bytes" << std::endl;

// Delete experiment (WARNING: Permanent!)
config.deleteExperiment();
```

### Custom Base Directory

By default, experiments are stored in `./experiments/`. You can customize this:

```cpp
// Use a custom base directory
ExperimentConfig config("my_experiment", "/data/neural_experiments");

// Or change it later
config.setBaseDirectory("/data/neural_experiments");
```

### Custom Paths

You can get custom paths within the experiment directory:

```cpp
ExperimentConfig config("my_experiment");

// Get custom paths
std::string logPath = config.getCustomPath("logs");
std::string checkpointPath = config.getCustomPath("checkpoints");
std::string resultsPath = config.getCustomPath("results");
```

## Complete Workflow Example

### 1. Creating an Experiment

```cpp
#include "snnfw/ExperimentConfig.h"
#include "snnfw/Datastore.h"
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Brain.h"

// Create experiment configuration
ExperimentConfig config("visual_cortex_study");
config.createDirectories();

// Create datastore for this experiment
Datastore datastore(config.getDatastorePath(), 1000000);

// Register factories (see datastore documentation)
registerFactories(datastore);

// Create neural structures
NeuralObjectFactory factory;
auto brain = factory.createBrain();
brain->setName("Human Visual System Model");

// ... create more structures ...

// Store in datastore
datastore.put(brain);
datastore.markDirty(brain->getId());
datastore.flushAll();
```

### 2. Loading an Experiment

```cpp
// Load existing experiment
ExperimentConfig config("visual_cortex_study");

if (!config.exists()) {
    std::cerr << "Experiment does not exist!" << std::endl;
    return;
}

// Create datastore
Datastore datastore(config.getDatastorePath(), 1000000);
registerFactories(datastore);

// Load brain
uint64_t brainId = 1200000000000000ULL;  // Known brain ID
auto brain = std::dynamic_pointer_cast<Brain>(datastore.get(brainId));

if (brain) {
    std::cout << "Loaded brain: " << brain->getName() << std::endl;
    
    // Navigate the hierarchy
    for (uint64_t hemisphereId : brain->getHemisphereIds()) {
        auto hemisphere = std::dynamic_pointer_cast<Hemisphere>(datastore.get(hemisphereId));
        std::cout << "  Hemisphere: " << hemisphere->getName() << std::endl;
        
        for (uint64_t lobeId : hemisphere->getLobeIds()) {
            auto lobe = std::dynamic_pointer_cast<Lobe>(datastore.get(lobeId));
            std::cout << "    Lobe: " << lobe->getName() << std::endl;
        }
    }
}
```

### 3. Managing Multiple Experiments

```cpp
// Create multiple experiments
std::vector<std::string> experiments = {
    "motor_cortex_study",
    "visual_processing",
    "language_networks"
};

for (const auto& name : experiments) {
    ExperimentConfig config(name);
    config.createDirectories();
    
    // Each experiment has its own isolated datastore
    Datastore datastore(config.getDatastorePath(), 1000000);
    
    // ... create experiment-specific structures ...
}

// Switch between experiments
ExperimentConfig currentExperiment("motor_cortex_study");
Datastore datastore(currentExperiment.getDatastorePath(), 1000000);
```

### 4. Command-Line Integration

You can integrate experiment selection into your main application:

```cpp
int main(int argc, char* argv[]) {
    // Parse command-line arguments
    std::string experimentName = "default_experiment";
    
    if (argc > 1) {
        experimentName = argv[1];
    }
    
    // Create or load experiment
    ExperimentConfig config(experimentName);
    
    if (!config.exists()) {
        std::cout << "Creating new experiment: " << experimentName << std::endl;
        config.createDirectories();
    } else {
        std::cout << "Loading existing experiment: " << experimentName << std::endl;
    }
    
    // Create datastore
    Datastore datastore(config.getDatastorePath(), 1000000);
    registerFactories(datastore);
    
    // ... run your experiment ...
    
    return 0;
}
```

Run with:
```bash
./my_app motor_cortex_study
./my_app visual_processing
./my_app language_networks
```

## Directory Structure

When using ExperimentConfig, your directory structure will look like:

```
./experiments/
├── motor_cortex_study/
│   ├── datastore/          # RocksDB datastore
│   ├── logs/               # Custom: application logs
│   ├── checkpoints/        # Custom: model checkpoints
│   └── results/            # Custom: experiment results
├── visual_processing/
│   └── datastore/
└── language_networks/
    └── datastore/
```

## Best Practices

1. **Use descriptive experiment names** - Make it easy to identify experiments later
2. **Use descriptive object names** - Name your brain structures meaningfully
3. **One experiment per study** - Keep different research questions in separate experiments
4. **Regular backups** - Back up experiment directories before major changes
5. **Clean up old experiments** - Use `deleteExperiment()` to remove obsolete data
6. **Document your experiments** - Keep notes about what each experiment contains

## API Reference

### Named Object Methods

All hierarchical objects (Nucleus, Region, Lobe, Hemisphere, Brain) support:

- `void setName(const std::string& name)` - Set the object's name
- `const std::string& getName() const` - Get the object's name

### ExperimentConfig Methods

- `ExperimentConfig(const std::string& experimentName, const std::string& baseDir = "./experiments")` - Constructor
- `const std::string& getName() const` - Get experiment name
- `std::string getDatastorePath() const` - Get datastore path
- `std::string getExperimentDirectory() const` - Get experiment directory
- `bool createDirectories() const` - Create experiment directories
- `bool exists() const` - Check if experiment exists
- `bool deleteExperiment() const` - Delete experiment (permanent!)
- `size_t getExperimentSize() const` - Get experiment size in bytes
- `std::string getCustomPath(const std::string& subpath) const` - Get custom path within experiment

## Examples

See the following example programs:

- `examples/experiment_config_example.cpp` - Complete demonstration of experiment management
- `examples/datastore_example.cpp` - Basic datastore usage

Build and run:
```bash
cd build
make experiment_config_example
./experiment_config_example
```

