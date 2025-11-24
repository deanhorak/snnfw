# SNNFW Adapter System

## Overview

The SNNFW Adapter System provides a modular, extensible framework for connecting Spiking Neural Networks (SNNs) to external data sources and actuators. This architecture transforms SNNFW from a specialized MNIST processor into a general-purpose neuromorphic computing framework.

## Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                      BaseAdapter                             │
│  - Configuration management                                  │
│  - Lifecycle control (initialize, shutdown)                  │
│  - Neuron population management                              │
└─────────────────────────────────────────────────────────────┘
                            │
                ┌───────────┴───────────┐
                │                       │
    ┌───────────▼──────────┐  ┌────────▼──────────┐
    │  SensoryAdapter      │  │  MotorAdapter     │
    │  (Input)             │  │  (Output)         │
    │                      │  │                   │
    │  - Feature extraction│  │  - Spike decoding │
    │  - Spike encoding    │  │  - Command gen.   │
    └──────────────────────┘  └───────────────────┘
            │                         │
    ┌───────┼─────────┐       ┌───────┼──────────┐
    │       │         │       │       │          │
┌───▼──┐ ┌──▼───┐ ┌──▼───┐ ┌─▼────┐ ┌▼─────┐ ┌─▼──────┐
│Retina│ │Audio │ │Video │ │Display│ │Motor │ │Actuator│
└──────┘ └──────┘ └──────┘ └───────┘ └──────┘ └────────┘
```

### Adapter Types

#### 1. **SensoryAdapter** (Input)
Converts external data into spike trains for neural processing.

**Key Methods:**
- `extractFeatures()` - Extract features from raw data
- `encodeFeatures()` - Convert features to spike times
- `processData()` - Complete pipeline: data → features → spikes

**Encoding Strategies:**
- **Rate Coding**: Feature intensity → spike timing (stronger = earlier)
- **Temporal Coding**: Feature value → precise spike timing
- **Population Coding**: Feature value → population activity pattern

#### 2. **MotorAdapter** (Output)
Converts spike trains into external actions or commands.

**Key Methods:**
- `decodeSpikes()` - Convert spike patterns to commands
- `executeCommand()` - Execute motor commands
- `update()` - Process neural activity and generate actions

**Decoding Strategies:**
- **Rate Decoding**: Spike rate → command intensity
- **Population Vector**: Population activity → direction/magnitude
- **Winner-Take-All**: Most active neuron → discrete action

## Implemented Adapters

### RetinaAdapter (Sensory)

**Purpose**: Visual processing with edge detection

**Features:**
- 7×7 spatial grid
- 8 orientation filters (0°, 45°, 90°, 135°, 180°, 225°, 270°, 315°)
- 392 neurons (7×7×8)
- Gabor-like edge detection
- Rate coding: edge strength → spike timing

**Configuration:**
```json
{
  "name": "retina",
  "type": "retina",
  "temporalWindow": 100.0,
  "intParams": {
    "grid_width": 7,
    "grid_height": 7,
    "num_orientations": 8
  },
  "doubleParams": {
    "spike_window": 50.0
  }
}
```

**Usage:**
```cpp
#include "snnfw/adapters/RetinaAdapter.h"

// Create adapter
BaseAdapter::Config config;
config.name = "retina";
config.temporalWindow = 100.0;
config.intParams["grid_width"] = 7;
config.intParams["grid_height"] = 7;
config.intParams["num_orientations"] = 8;

auto retina = std::make_shared<RetinaAdapter>(config);
retina->initialize();

// Process image
std::vector<uint8_t> imageData = loadImage("digit.png", 28, 28);
retina->processData(imageData);

// Get activation pattern for classification
auto activations = retina->getActivationPattern();
```

### AudioAdapter (Sensory)

**Purpose**: Audio processing with frequency analysis

**Features:**
- Configurable frequency channels (mel-scale)
- FFT-based spectral analysis
- Rate coding: spectral energy → spike timing
- Temporal windowing with overlap

**Configuration:**
```json
{
  "name": "audio",
  "type": "audio",
  "temporalWindow": 100.0,
  "intParams": {
    "sample_rate": 16000,
    "num_channels": 40,
    "window_size": 512,
    "hop_size": 160
  },
  "doubleParams": {
    "min_frequency": 20.0,
    "max_frequency": 8000.0
  },
  "stringParams": {
    "encoding": "rate"
  }
}
```

**Usage:**
```cpp
#include "snnfw/adapters/AudioAdapter.h"

auto audio = std::make_shared<AudioAdapter>(config);
audio->initialize();

// Process audio samples
std::vector<double> samples = recordAudio(1.0); // 1 second
audio->processData(samples);
```

### DisplayAdapter (Motor)

**Purpose**: Visualize neural activity

**Features:**
- Multiple display modes (raster, heatmap, vector, ASCII)
- Real-time activity visualization
- Population vector decoding
- Configurable update rate

**Configuration:**
```json
{
  "name": "display",
  "type": "display",
  "temporalWindow": 100.0,
  "intParams": {
    "display_width": 80,
    "display_height": 24
  },
  "doubleParams": {
    "update_interval": 50.0
  },
  "stringParams": {
    "mode": "heatmap"
  }
}
```

**Usage:**
```cpp
#include "snnfw/adapters/DisplayAdapter.h"

auto display = std::make_shared<DisplayAdapter>(config);
display->initialize();

// Update display with neural activity
display->update(currentTime);
std::string visualization = display->getDisplayBuffer();
std::cout << visualization << std::endl;
```

## Creating Custom Adapters

### Example: Custom Sensor Adapter

```cpp
#include "snnfw/adapters/SensoryAdapter.h"

class MySensorAdapter : public SensoryAdapter {
public:
    MySensorAdapter(const Config& config) : SensoryAdapter(config) {
        // Initialize custom parameters
        threshold_ = config.getDoubleParam("threshold", 0.5);
    }
    
    bool initialize() override {
        createNeurons();
        // Custom initialization
        return true;
    }
    
    FeatureVector extractFeatures(const std::vector<uint8_t>& data) override {
        FeatureVector features;
        features.timestamp = getCurrentTime();
        
        // Extract custom features
        for (size_t i = 0; i < data.size(); ++i) {
            double normalized = data[i] / 255.0;
            if (normalized > threshold_) {
                features.features.push_back(normalized);
                features.labels.push_back("sensor_" + std::to_string(i));
            }
        }
        
        return features;
    }
    
    SpikePattern encodeFeatures(const FeatureVector& features) override {
        SpikePattern pattern;
        pattern.timestamp = features.timestamp;
        pattern.duration = config_.temporalWindow;
        
        // Encode using rate coding
        pattern.spikeTimes.resize(features.features.size());
        for (size_t i = 0; i < features.features.size(); ++i) {
            double spikeTime = featureToSpikeTime(features.features[i], pattern.duration);
            if (spikeTime >= 0.0) {
                pattern.spikeTimes[i].push_back(spikeTime);
                neurons_[i]->insertSpike(spikeTime);
            }
        }
        
        return pattern;
    }
    
private:
    double threshold_;
};
```

## Configuration System

### Loading from JSON

```cpp
#include "snnfw/ConfigLoader.h"

ConfigLoader loader("config.json");

// Get adapter configurations
auto adapterNames = loader.getAdapterNames();
for (const auto& name : adapterNames) {
    auto config = loader.getAdapterConfig(name);
    // Create adapter based on type
}
```

### Configuration Structure

```json
{
  "adapters": [
    {
      "name": "unique_name",
      "type": "adapter_type",
      "temporalWindow": 100.0,
      "intParams": {
        "param1": 42
      },
      "doubleParams": {
        "param2": 3.14
      },
      "stringParams": {
        "param3": "value"
      }
    }
  ]
}
```

## Best Practices

### 1. Neuron Population Sizing
- Match neuron count to feature dimensionality
- Consider computational resources
- Balance resolution vs. performance

### 2. Temporal Windows
- Align with data sampling rate
- Consider spike timing precision
- Balance memory vs. temporal resolution

### 3. Encoding Strategies
- **Rate coding**: Simple, robust, good for intensity
- **Temporal coding**: Precise, efficient, good for timing
- **Population coding**: Rich representation, good for complex features

### 4. Performance Optimization
- Reuse neuron populations when possible
- Clear spikes after processing
- Use appropriate temporal windows
- Batch process when possible

## Examples

See the `examples/` directory for complete working examples:
- `examples/retina_mnist.cpp` - MNIST digit recognition
- `examples/audio_classification.cpp` - Audio event detection
- `examples/display_visualization.cpp` - Neural activity visualization
- `examples/custom_adapter.cpp` - Creating custom adapters

## API Reference

For detailed API documentation, see:
- `include/snnfw/adapters/BaseAdapter.h`
- `include/snnfw/adapters/SensoryAdapter.h`
- `include/snnfw/adapters/MotorAdapter.h`
- Individual adapter headers in `include/snnfw/adapters/`

