# NeuroML and HDF5 Import Guide

## Overview

SNNFW now supports importing neuron positions from industry-standard neuroscience formats:

- **NeuroML** - XML-based model description language (NeuroML v2)
- **HDF5** - High-performance binary format (SONATA, NWB, generic)

This enables seamless integration with models from:
- NeuroML-DB (neuroml-db.org)
- Open Source Brain (opensourcebrain.org)
- Allen Brain Atlas (SONATA format)
- Neurodata Without Borders (NWB format)
- NEURON, NEST, Brian2, and other simulators

## Quick Start

### Import from NeuroML

```cpp
#include "snnfw/ModelImporter.h"
#include "snnfw/Datastore.h"

Datastore datastore("./my_db", 1000000);
ModelImporter importer(datastore);

// Import positions from NeuroML file
ImportConfig config;
auto result = importer.importFromNeuroML("model.nml", config);

if (result.success) {
    std::cout << "Imported " << result.positionsSet << " positions" << std::endl;
}
```

### Import from HDF5

```cpp
// Auto-detect dataset path (tries common formats)
auto result = importer.importFromHDF5("positions.h5", config);

// Or specify dataset path explicitly
auto result2 = importer.importFromHDF5("data.h5", config, "/nodes/default/positions");
```

## NeuroML Import

### Supported NeuroML Elements

The importer extracts positions from `<location>` elements within `<instance>` tags:

```xml
<neuroml xmlns="http://www.neuroml.org/schema/neuroml2" id="Network">
    <network id="Net1">
        <population id="Pop0" component="cell_type" size="3">
            <instance id="100000000000000">
                <location x="0.0" y="0.0" z="0.0"/>
            </instance>
            <instance id="100000000000001">
                <location x="10.0" y="5.0" z="2.0"/>
            </instance>
            <instance id="100000000000002">
                <location x="20.0" y="10.0" z="4.0"/>
            </instance>
        </population>
    </network>
</neuroml>
```

### NeuroML Features

- ✅ Parses `<population>` elements
- ✅ Extracts `<instance>` with `<location>` elements
- ✅ Reads x, y, z coordinates
- ✅ Maps instance IDs to neuron IDs
- ✅ Supports multiple populations
- ✅ Position transformation (scale + offset)

### Example: Import from NeuroML-DB

```cpp
// Download a model from NeuroML-DB
// Example: https://neuroml-db.org/model_info?model_id=NMLCL000073

ImportConfig config;
config.positionScale = 0.001f;  // Convert μm to mm if needed

auto result = importer.importFromNeuroML("NMLCL000073.nml", config);

std::cout << "Imported " << result.neuronsImported << " neurons" << std::endl;
std::cout << "Positions set: " << result.positionsSet << std::endl;
```

## HDF5 Import

### Supported HDF5 Formats

The importer auto-detects and supports multiple HDF5 formats:

#### 1. Generic Format

```
/positions          # Nx3 float array (x, y, z)
/neuron_ids         # N uint64 array (optional)
```

#### 2. SONATA Format (Allen Brain Institute)

```
/nodes/default/positions    # Nx3 float array
/nodes/default/neuron_ids   # N uint64 array (optional)
```

Or:
```
/nodes/positions            # Nx3 float array
```

#### 3. NWB Format (Neurodata Without Borders)

```
/processing/ophys/ImageSegmentation/PlaneSegmentation/voxel_mask
```

### HDF5 Features

- ✅ Auto-detects dataset paths
- ✅ Supports multiple HDF5 formats (SONATA, NWB, generic)
- ✅ Reads Nx3 position arrays
- ✅ Optional neuron ID mapping
- ✅ Position transformation (scale + offset)
- ✅ Efficient binary format for large datasets

### Creating HDF5 Files

#### Python (h5py)

```python
import h5py
import numpy as np

# Create HDF5 file
with h5py.File('positions.h5', 'w') as f:
    # Positions (Nx3 array)
    positions = np.array([
        [100.0, 200.0, 50.0],
        [110.0, 205.0, 52.0],
        [120.0, 210.0, 54.0]
    ], dtype=np.float32)
    f.create_dataset('/positions', data=positions)
    
    # Neuron IDs (optional)
    neuron_ids = np.array([
        100000000000000,
        100000000000001,
        100000000000002
    ], dtype=np.uint64)
    f.create_dataset('/neuron_ids', data=neuron_ids)
```

#### MATLAB

```matlab
% Create positions array (Nx3)
positions = [
    100.0, 200.0, 50.0;
    110.0, 205.0, 52.0;
    120.0, 210.0, 54.0
];

% Create HDF5 file
h5create('positions.h5', '/positions', size(positions));
h5write('positions.h5', '/positions', single(positions));

% Optional: Add neuron IDs
neuron_ids = uint64([100000000000000; 100000000000001; 100000000000002]);
h5create('positions.h5', '/neuron_ids', size(neuron_ids));
h5write('positions.h5', '/neuron_ids', neuron_ids);
```

### Example: Import SONATA Dataset

```cpp
// Import from Allen Brain Institute SONATA format
ImportConfig config;
config.positionScale = 1.0f;

// Auto-detect SONATA format
auto result = importer.importFromHDF5("circuit_config.h5", config);

// Or specify path explicitly
auto result2 = importer.importFromHDF5("circuit_config.h5", config, 
                                       "/nodes/default/positions");
```

## Position Transformation

Both NeuroML and HDF5 import support position transformation:

```cpp
ImportConfig config;

// Scale positions (e.g., convert μm to mm)
config.positionScale = 0.001f;

// Offset positions (e.g., align multiple imports)
config.offsetX = 100.0f;
config.offsetY = 50.0f;
config.offsetZ = 25.0f;

auto result = importer.importFromHDF5("positions.h5", config);
```

**Transformation formula:**
```
final_position = (imported_position * scale) + offset
```

**Example:**
- Imported position: (100, 200, 50)
- Scale: 0.001
- Offset: (50, 100, 25)
- Final position: (50.1, 100.2, 25.05)

## Import Result

Both import methods return an `ImportResult` structure:

```cpp
struct ImportResult {
    size_t neuronsImported;     // Number of neurons imported
    size_t positionsSet;        // Number of positions set
    bool success;               // Overall success flag
    std::string errorMessage;   // Error message if failed
    
    // ID mapping (external ID -> SNNFW ID)
    std::map<std::string, uint64_t> neuronIdMap;
};
```

**Usage:**
```cpp
auto result = importer.importFromNeuroML("model.nml", config);

if (result.success) {
    std::cout << "Success!" << std::endl;
    std::cout << "Neurons: " << result.neuronsImported << std::endl;
    std::cout << "Positions: " << result.positionsSet << std::endl;
    
    // Access ID mapping
    for (const auto& [externalId, snnfwId] : result.neuronIdMap) {
        std::cout << "External ID " << externalId 
                 << " -> SNNFW ID " << snnfwId << std::endl;
    }
} else {
    std::cerr << "Error: " << result.errorMessage << std::endl;
}
```

## Complete Example

See `examples/neuroml_hdf5_import_example.cpp` for a complete working example that demonstrates:

1. Creating sample NeuroML and HDF5 files
2. Importing positions from both formats
3. Verifying imported positions
4. Position transformation
5. Using imported positions in visualization

**Run the example:**
```bash
cd build
./neuroml_hdf5_import_example
```

## Integration with Visualization

Imported positions are automatically used by the visualization system:

```cpp
// Import positions
ModelImporter importer(datastore);
importer.importFromHDF5("positions.h5", config);

// Visualization will use stored positions
NetworkDataAdapter adapter(datastore, inspector);
adapter.extractHierarchy(brainId, "Brain");

// Layout engine respects stored positions
LayoutConfig layoutConfig;
layoutConfig.overrideStoredPositions = false;  // Keep imported positions
layoutEngine.computeLayout(adapter, layoutConfig);
```

## Real-World Datasets

### NeuroML-DB Models

Download models from https://neuroml-db.org/

```cpp
// Example: Cerebellar granule cell network
auto result = importer.importFromNeuroML("NMLCL000073.nml", config);
```

### Allen Brain Institute (SONATA)

Download datasets from https://portal.brain-map.org/

```cpp
ImportConfig config;
config.positionScale = 0.001f;  // μm to mm
auto result = importer.importFromHDF5("circuit_config.h5", config);
```

### Neurodata Without Borders (NWB)

Download datasets from https://www.nwb.org/

```cpp
auto result = importer.importFromHDF5("experiment.nwb", config,
                                     "/processing/ophys/ImageSegmentation/PlaneSegmentation/voxel_mask");
```

## Troubleshooting

### NeuroML Import Issues

**Problem:** "No <neuroml> root element found"
- **Solution:** Ensure file is valid NeuroML XML with `<neuroml>` root element

**Problem:** "No positions found in NeuroML file"
- **Solution:** Check that `<instance>` elements have `<location>` children with x, y, z attributes

### HDF5 Import Issues

**Problem:** "No positions dataset found in HDF5 file"
- **Solution:** Specify dataset path explicitly or check file structure with `h5dump` or HDFView

**Problem:** "Positions dataset must be 2D (Nx3)"
- **Solution:** Ensure positions dataset has shape (N, 3) where N is number of neurons

**Problem:** "Failed to open HDF5 file"
- **Solution:** Check file exists and is valid HDF5 format

## Performance

- **NeuroML:** XML parsing is fast for small-medium networks (<100k neurons)
- **HDF5:** Binary format is highly efficient for large networks (>1M neurons)
- **Recommendation:** Use HDF5 for large-scale simulations, NeuroML for model exchange

## See Also

- [Position Storage and Import Documentation](POSITION_STORAGE_AND_IMPORT.md)
- [Visualization Documentation](VISUALIZATION_PHASE4_PROGRESS.md)
- [NeuroML Documentation](https://docs.neuroml.org/)
- [HDF5 Documentation](https://www.hdfgroup.org/solutions/hdf5/)
- [SONATA Format](https://github.com/AllenInstitute/sonata)
- [NWB Format](https://www.nwb.org/)

