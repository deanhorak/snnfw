# Hyperparameter Optimization

## Overview

SNNFW includes a general-purpose hyperparameter optimization framework that can automatically search for optimal parameters for any experiment. The framework supports multiple optimization strategies and is designed to be model-agnostic.

## Features

- **Multiple Optimization Strategies**:
  - Grid Search: Exhaustive search over parameter grid
  - Random Search: Random sampling of parameter space
  - Bayesian Optimization: Model-based optimization with Gaussian Process
  - Genetic Algorithm: Evolutionary optimization

- **General-Purpose Design**: Can optimize any model or experiment
- **Automatic Result Tracking**: Saves all intermediate results
- **Resume Capability**: Can resume interrupted optimization runs
- **Parallel Execution**: Supports multi-worker parallel evaluation (future)
- **JSON Configuration**: Easy parameter specification

## Quick Start

### 1. MNIST Optimization Example

```bash
# Build the optimizer
cd build
cmake ..
make mnist_hyperparameter_optimization

# Run with random search (default)
./mnist_hyperparameter_optimization ../configs/mnist_config.json random 50 1000 1000

# Arguments:
# 1. Base config file
# 2. Strategy: grid, random, bayesian, genetic
# 3. Max iterations
# 4. Training samples per digit
# 5. Test samples
```

### 2. Optimization Strategies

#### Grid Search
Exhaustively evaluates all combinations of parameters.

```bash
./mnist_hyperparameter_optimization ../configs/mnist_config.json grid 100
```

**Pros**: Guaranteed to find best combination in search space
**Cons**: Exponentially expensive with number of parameters

#### Random Search
Randomly samples parameter configurations.

```bash
./mnist_hyperparameter_optimization ../configs/mnist_config.json random 50
```

**Pros**: Often finds good solutions quickly, scales well
**Cons**: No guarantee of finding optimal solution

#### Bayesian Optimization
Uses Gaussian Process to model objective function and intelligently sample.

```bash
./mnist_hyperparameter_optimization ../configs/mnist_config.json bayesian 50
```

**Pros**: Sample-efficient, good for expensive evaluations
**Cons**: More complex, requires initial exploration phase

#### Genetic Algorithm
Evolves population of configurations using crossover and mutation.

```bash
./mnist_hyperparameter_optimization ../configs/mnist_config.json genetic 100
```

**Pros**: Good for complex search spaces, naturally parallel
**Cons**: Requires tuning of GA parameters (mutation rate, population size)

## Using the Optimizer in Your Code

### Basic Usage

```cpp
#include "snnfw/HyperparameterOptimizer.h"

using namespace snnfw;

// 1. Create optimizer
auto optimizer = std::make_shared<HyperparameterOptimizer>(
    OptimizationStrategy::RANDOM_SEARCH);

// 2. Add parameters to optimize
ParameterSpec gridSize;
gridSize.name = "network.grid_size";
gridSize.type = ParameterSpec::Type::INTEGER;
gridSize.minValue = 5;
gridSize.maxValue = 9;
gridSize.step = 1;
optimizer->addParameter(gridSize);

ParameterSpec threshold;
threshold.name = "neuron.similarity_threshold";
threshold.type = ParameterSpec::Type::DOUBLE;
threshold.minValue = 0.5;
threshold.maxValue = 0.9;
threshold.step = 0.1;
optimizer->addParameter(threshold);

// 3. Define objective function
optimizer->setObjective([](const ParameterConfig& params) -> ExperimentResult {
    // Run your experiment with these parameters
    double accuracy = runExperiment(params);
    
    ExperimentResult result;
    result.config = params;
    result.score = accuracy;  // Higher is better
    return result;
});

// 4. Run optimization
optimizer->setMaxIterations(50);
auto best = optimizer->optimize();

// 5. Use best parameters
std::cout << "Best accuracy: " << best.score << std::endl;
```

### Parameter Types

#### Integer Parameters
```cpp
ParameterSpec param;
param.name = "network.grid_size";
param.type = ParameterSpec::Type::INTEGER;
param.minValue = 5;
param.maxValue = 10;
param.step = 1;  // Will try: 5, 6, 7, 8, 9, 10
```

#### Double Parameters
```cpp
ParameterSpec param;
param.name = "neuron.threshold";
param.type = ParameterSpec::Type::DOUBLE;
param.minValue = 0.1;
param.maxValue = 0.9;
param.step = 0.1;  // Will try: 0.1, 0.2, ..., 0.9
```

#### Categorical Parameters
```cpp
ParameterSpec param;
param.name = "classification.method";
param.type = ParameterSpec::Type::CATEGORICAL;
param.categories = {"knn", "svm", "random_forest"};
```

### Accessing Parameters in Objective Function

```cpp
optimizer->setObjective([](const ParameterConfig& params) -> ExperimentResult {
    // Integer parameters
    int gridSize = params.intParams.at("network.grid_size");
    
    // Double parameters
    double threshold = params.doubleParams.at("neuron.threshold");
    
    // String parameters
    std::string method = params.stringParams.at("classification.method");
    
    // Run experiment...
    double score = runExperiment(gridSize, threshold, method);
    
    ExperimentResult result;
    result.config = params;
    result.score = score;
    result.metrics["accuracy"] = score;
    result.metrics["grid_size"] = gridSize;
    return result;
});
```

## MNIST-Specific Helper

For MNIST experiments, use the `MNISTOptimizer` helper:

```cpp
#include "snnfw/HyperparameterOptimizer.h"

// Create MNIST optimizer with default parameters
auto optimizer = MNISTOptimizer::create(
    "../configs/mnist_config.json",
    "optimization_results");

// Automatically adds common MNIST parameters:
// - network.grid_size
// - network.num_orientations
// - network.edge_threshold
// - neuron.similarity_threshold
// - neuron.max_patterns
// - neuron.window_size_ms
// - classification.k_neighbors

optimizer->setMaxIterations(50);
auto best = optimizer->optimize();
```

## Results

### Result Structure

Each experiment result contains:
- `config`: Parameter configuration used
- `score`: Objective score (higher is better)
- `trainingTime`: Training time in seconds
- `testingTime`: Testing time in seconds
- `metrics`: Additional metrics (accuracy, loss, etc.)
- `timestamp`: When experiment was run

### Saving and Loading Results

```cpp
// Save results
optimizer->saveResults("results.json");

// Load results
optimizer->loadResults("results.json");

// Resume optimization from saved results
auto best = optimizer->resume("results.json");
```

### Result Files

Results are saved in the specified directory:
```
optimization_results/
├── final_results.json          # All results
├── result_20250117_143022_001.json  # Intermediate result 1
├── result_20250117_143045_123.json  # Intermediate result 2
└── ...
```

### Analyzing Results

```python
import json
import pandas as pd

# Load results
with open('optimization_results/final_results.json') as f:
    data = json.load(f)

# Convert to DataFrame
results = pd.DataFrame([
    {
        **r['config']['intParams'],
        **r['config']['doubleParams'],
        'score': r['score'],
        'training_time': r['trainingTime']
    }
    for r in data['results']
])

# Find best
best = results.loc[results['score'].idxmax()]
print(f"Best accuracy: {best['score']:.4f}")
print(f"Best parameters: {best.to_dict()}")

# Plot parameter importance
import matplotlib.pyplot as plt
for param in ['network.grid_size', 'neuron.similarity_threshold']:
    plt.figure()
    plt.scatter(results[param], results['score'])
    plt.xlabel(param)
    plt.ylabel('Accuracy')
    plt.title(f'Impact of {param}')
    plt.show()
```

## Best Practices

### 1. Start with Random Search
Random search is often the best starting point:
- Fast to run
- Good results with 50-100 iterations
- Easy to parallelize

### 2. Use Coarse-to-Fine Strategy
1. First pass: Wide parameter ranges, coarse steps
2. Second pass: Narrow ranges around best results, fine steps

```cpp
// First pass: Coarse
ParameterSpec threshold;
threshold.minValue = 0.1;
threshold.maxValue = 0.9;
threshold.step = 0.2;  // Coarse: 0.1, 0.3, 0.5, 0.7, 0.9

// Second pass: Fine (after finding best ~0.7)
threshold.minValue = 0.6;
threshold.maxValue = 0.8;
threshold.step = 0.05;  // Fine: 0.60, 0.65, 0.70, 0.75, 0.80
```

### 3. Use Smaller Datasets for Optimization
Train on subset of data for faster iteration:
```bash
# Fast optimization (1000 samples)
./mnist_hyperparameter_optimization config.json random 50 1000 1000

# Final validation (full dataset)
./mnist_with_adapters config_optimized.json
```

### 4. Monitor Intermediate Results
Check intermediate results to:
- Verify optimization is progressing
- Detect if search space is too narrow/wide
- Stop early if good solution found

### 5. Save Everything
Always enable intermediate result saving:
```cpp
optimizer->setSaveIntermediateResults(true);
```

This allows:
- Resuming interrupted runs
- Analyzing optimization trajectory
- Debugging issues

## Advanced Topics

### Custom Optimization Strategy

You can implement custom strategies by extending the optimizer:

```cpp
class CustomOptimizer : public HyperparameterOptimizer {
protected:
    ExperimentResult customStrategy() override {
        // Your custom optimization logic
        for (int i = 0; i < maxIterations_; ++i) {
            auto config = generateCustomConfig();
            auto result = objective_(config);
            results_.push_back(result);
        }
        return getBestResult();
    }
};
```

### Multi-Objective Optimization

Optimize for multiple objectives (accuracy, speed, memory):

```cpp
optimizer->setObjective([](const ParameterConfig& params) -> ExperimentResult {
    auto [accuracy, speed, memory] = runExperiment(params);
    
    // Weighted combination
    double score = 0.7 * accuracy + 0.2 * (1.0 / speed) + 0.1 * (1.0 / memory);
    
    ExperimentResult result;
    result.config = params;
    result.score = score;
    result.metrics["accuracy"] = accuracy;
    result.metrics["speed"] = speed;
    result.metrics["memory"] = memory;
    return result;
});
```

### Constraint Handling

Add constraints to parameter combinations:

```cpp
optimizer->setObjective([](const ParameterConfig& params) -> ExperimentResult {
    int gridSize = params.intParams.at("network.grid_size");
    int numOrientations = params.intParams.at("network.num_orientations");
    
    // Constraint: total neurons must be < 1000
    int totalNeurons = gridSize * gridSize * numOrientations;
    if (totalNeurons > 1000) {
        ExperimentResult result;
        result.config = params;
        result.score = 0.0;  // Invalid configuration
        return result;
    }
    
    // Valid configuration - run experiment
    return runExperiment(params);
});
```

## Troubleshooting

### Optimization Not Improving
- Increase max iterations
- Widen parameter ranges
- Try different strategy (e.g., Bayesian instead of Random)
- Check if objective function is working correctly

### Out of Memory
- Reduce training/test sample sizes
- Reduce parameter ranges (fewer neurons, smaller grids)
- Use grid search with coarser steps

### Slow Optimization
- Reduce training/test sample sizes
- Use random search instead of grid search
- Enable parallel workers (future feature)

## References

- Bergstra, J., & Bengio, Y. (2012). Random search for hyper-parameter optimization. JMLR.
- Snoek, J., Larochelle, H., & Adams, R. P. (2012). Practical Bayesian optimization. NeurIPS.
- Eiben, A. E., & Smith, J. E. (2015). Introduction to evolutionary computing. Springer.

