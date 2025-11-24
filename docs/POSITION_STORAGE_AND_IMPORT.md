# 3D Position Storage and Model Import

## Overview

SNNFW now supports storing 3D spatial coordinates for all neural objects and importing positions from external neural simulation frameworks like NEURON and NEST. This enables:

1. **Anatomically realistic visualizations** - Import real neuron positions from morphology files
2. **Hybrid layouts** - Combine imported positions with computed layouts
3. **Position persistence** - All positions are automatically saved to the datastore
4. **Model interoperability** - Import models from NEURON, NEST, and NeuroML

## Features Implemented

### 1. Position Storage in NeuralObject Base Class

All neural objects (Neuron, Cluster, Layer, Column, Nucleus, Region, Lobe, Hemisphere, Brain) can now store 3D positions.

**Position3D Structure:**
```cpp
struct Position3D {
    float x, y, z;
    
    // Distance calculation
    float distanceTo(const Position3D& other) const;
    
    // Operators: +, -, *, ==, !=
};
```

**NeuralObject Methods:**
```cpp
bool hasPosition() const;                          // Check if position is set
const Position3D& getPosition() const;             // Get position
void setPosition(const Position3D& pos);           // Set position
void setPosition(float x, float y, float z);       // Set position (convenience)
void clearPosition();                              // Clear position
```

### 2. Automatic Serialization

Positions are automatically serialized to JSON and persisted to the datastore:

```json
{
  "id": 100000000000000,
  "position": {
    "x": 10.5,
    "y": 20.3,
    "z": 15.7
  },
  ...
}
```

**Backward Compatibility:** Position field is optional - objects without positions work normally.

### 3. Model Import Utilities

The `ModelImporter` class provides methods to import positions from various formats:

```cpp
#include "snnfw/ModelImporter.h"

Datastore datastore("./my_db", 1000000);
ModelImporter importer(datastore);

// Import from CSV
ImportConfig config;
config.positionScale = 1.0f;
config.offsetX = 0.0f;
config.offsetY = 0.0f;
config.offsetZ = 0.0f;

auto result = importer.importFromCSV("positions.csv", config);
if (result.success) {
    std::cout << "Imported " << result.positionsSet << " positions" << std::endl;
}
```

**Supported Formats:**

| Format | Method | Description |
|--------|--------|-------------|
| CSV | `importFromCSV()` | Simple comma-separated format: `neuron_id,x,y,z` |
| NEURON SWC | `importFromSWC()` | NEURON morphology format: `n T x y z R P` |
| NEST JSON | `importFromNEST()` | NEST JSON format with position arrays |
| NeuroML | `importFromNeuroML()` | NeuroML XML format with `<location>` elements |
| HDF5 | `importFromHDF5()` | HDF5 format (SONATA, NWB, generic) |

### 4. Position Transformation

Import positions with scaling and offset transformations:

```cpp
ImportConfig config;
config.positionScale = 2.0f;    // Scale by 2x
config.offsetX = 100.0f;        // Offset by 100 in X
config.offsetY = 50.0f;         // Offset by 50 in Y
config.offsetZ = 25.0f;         // Offset by 25 in Z

importer.importFromCSV("positions.csv", config);
```

**Transformation Formula:**
```
final_position = (imported_position * scale) + offset
```

### 5. Visualization Integration

NetworkDataAdapter automatically uses stored positions when available:

```cpp
// In NetworkDataAdapter::extractNeurons()
if (neuron->hasPosition()) {
    visualData.position = neuron->getPosition();  // Use stored position
} else {
    // Position will be computed by LayoutEngine
    visualData.position = Position3D(0, 0, 0);
}
```

**LayoutEngine Configuration:**
```cpp
LayoutConfig config;
config.overrideStoredPositions = false;  // Respect stored positions (default)
// config.overrideStoredPositions = true;  // Force recompute all positions

layoutEngine.computeLayout(adapter, config);
```

## Usage Examples

### Example 1: Create Neurons with Positions

```cpp
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Datastore.h"

Datastore datastore("./my_db", 10000);
NeuralObjectFactory factory;

// Create neuron with position
auto neuron = factory.createNeuron(100.0, 0.8, 10);
neuron->setPosition(10.5f, 20.3f, 15.7f);
datastore.put(neuron);

// Position is automatically persisted
```

### Example 2: Import from CSV

**positions.csv:**
```csv
neuron_id,x,y,z
100000000000000,0.0,0.0,0.0
100000000000001,10.0,0.0,0.0
100000000000002,20.0,0.0,0.0
```

**Code:**
```cpp
ModelImporter importer(datastore);
ImportConfig config;
auto result = importer.importFromCSV("positions.csv", config);

std::cout << "Imported: " << result.positionsSet << " positions" << std::endl;
```

### Example 3: Import from NEURON SWC

**morphology.swc:**
```
# NEURON morphology file
1 1 0.0 0.0 0.0 1.0 -1
2 3 10.0 0.0 0.0 0.5 1
3 3 20.0 5.0 0.0 0.5 2
```

**Code:**
```cpp
auto result = importer.importFromSWC("morphology.swc", config);
```

### Example 4: Import from NEST JSON

**nest_positions.json:**
```json
{
  "neurons": [
    {"id": 100000000000000, "position": [0.0, 0.0, 0.0]},
    {"id": 100000000000001, "position": [10.0, 0.0, 0.0]},
    {"id": 100000000000002, "position": [20.0, 0.0, 0.0]}
  ]
}
```

**Code:**
```cpp
auto result = importer.importFromNEST("nest_positions.json", config);
```

### Example 5: Import from HDF5 (Generic Format)

**Code:**
```cpp
ImportConfig config;
config.positionScale = 0.001f;  // Convert μm to mm
config.offsetX = 50.0f;
config.offsetY = 100.0f;
config.offsetZ = 25.0f;

// Auto-detect dataset path
auto result = importer.importFromHDF5("positions.h5", config);

// Or specify dataset path explicitly
auto result2 = importer.importFromHDF5("data.h5", config, "/nodes/default/positions");
```

**HDF5 File Structure (Generic):**
```
/positions          # Nx3 float array (required)
/neuron_ids         # N uint64 array (optional)
```

**HDF5 File Structure (SONATA):**
```
/nodes/default/positions    # Nx3 float array
/nodes/default/neuron_ids   # N uint64 array (optional)
```

**HDF5 File Structure (NWB):**
```
/processing/ophys/ImageSegmentation/PlaneSegmentation/voxel_mask
```

### Example 6: Export Positions to CSV

```cpp
std::vector<uint64_t> neuronIds = {100000000000000, 100000000000001, 100000000000002};
importer.exportToCSV("exported_positions.csv", neuronIds);
```

### Example 7: Hybrid Layout (Imported + Computed)

```cpp
// Import positions for some neurons
importer.importFromCSV("partial_positions.csv", config);

// Compute positions for neurons without stored positions
LayoutConfig layoutConfig;
layoutConfig.overrideStoredPositions = false;  // Don't override imported positions
layoutEngine.computeGridLayout(adapter, layoutConfig);

// Result: Neurons with imported positions keep them,
//         neurons without positions get computed positions
```

## File Formats

### CSV Format

```csv
neuron_id,x,y,z
<id>,<x>,<y>,<z>
...
```

- Header line is optional (detected automatically)
- Comments start with `#`
- Empty lines are ignored

### NEURON SWC Format

```
n T x y z R P
```

- `n`: Point number (used as neuron ID)
- `T`: Type (1=soma, 2=axon, 3=dendrite, etc.)
- `x, y, z`: 3D coordinates
- `R`: Radius
- `P`: Parent point number

### NEST JSON Format

```json
{
  "neurons": [
    {
      "id": <neuron_id>,
      "position": [<x>, <y>, <z>]
    },
    ...
  ]
}
```

### HDF5 Format

**Generic Format:**
```
/positions          # Dataset: Nx3 float array (x, y, z coordinates)
/neuron_ids         # Dataset: N uint64 array (optional, defaults to sequential IDs)
```

**SONATA Format:**
```
/nodes/<population>/positions    # Dataset: Nx3 float array
/nodes/<population>/neuron_ids   # Dataset: N uint64 array (optional)
```

**NWB (Neurodata Without Borders) Format:**
```
/processing/ophys/ImageSegmentation/PlaneSegmentation/voxel_mask
```

**Creating HDF5 Files:**

You can create HDF5 files using Python (h5py), MATLAB, or C++:

```python
import h5py
import numpy as np

# Create HDF5 file
with h5py.File('positions.h5', 'w') as f:
    # Create positions dataset (5 neurons x 3 coordinates)
    positions = np.array([
        [100.0, 200.0, 50.0],
        [110.0, 205.0, 52.0],
        [120.0, 210.0, 54.0],
        [130.0, 215.0, 56.0],
        [140.0, 220.0, 58.0]
    ], dtype=np.float32)
    f.create_dataset('/positions', data=positions)

    # Create neuron IDs dataset (optional)
    neuron_ids = np.array([100000000000000, 100000000000001,
                          100000000000002, 100000000000003,
                          100000000000004], dtype=np.uint64)
    f.create_dataset('/neuron_ids', data=neuron_ids)
```

## API Reference

### ModelImporter Class

```cpp
class ModelImporter {
public:
    ModelImporter(Datastore& datastore);
    
    // Import methods
    ImportResult importFromCSV(const std::string& filename, const ImportConfig& config);
    ImportResult importFromSWC(const std::string& filename, const ImportConfig& config);
    ImportResult importFromNEST(const std::string& filename, const ImportConfig& config);
    ImportResult importFromNeuroML(const std::string& filename, const ImportConfig& config);
    ImportResult importFromHDF5(const std::string& filename, const ImportConfig& config,
                                const std::string& datasetPath = "");

    // Direct position manipulation
    bool setNeuronPosition(uint64_t neuronId, const Position3D& position);
    bool setNeuronPosition(uint64_t neuronId, float x, float y, float z);
    bool getNeuronPosition(uint64_t neuronId, Position3D& position) const;
    
    // Export
    bool exportToCSV(const std::string& filename, const std::vector<uint64_t>& neuronIds) const;
    
    // Get last import result
    const ImportResult& getLastResult() const;
};
```

### ImportConfig Structure

```cpp
struct ImportConfig {
    bool importPositions = true;
    float positionScale = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float offsetZ = 0.0f;
};
```

### ImportResult Structure

```cpp
struct ImportResult {
    bool success = false;
    size_t neuronsImported = 0;
    size_t positionsSet = 0;
    std::string errorMessage;
    std::unordered_map<std::string, uint64_t> neuronIdMap;
};
```

## Running the Examples

### CSV Import Example

```bash
cd build
./position_import_example
```

**Output:**
```
=== SNNFW Position Import/Export Example ===

Part 1: Creating neurons with positions...
  ✓ Created 10 neurons with positions

Part 2: Exporting positions to CSV...
  ✓ Exported positions to neuron_positions.csv

Part 3: Clearing positions and re-importing...
  ✓ Imported 10 positions from CSV

Part 4: Verifying imported positions...
  ✓ Verified 10/10 positions

Part 5: Demonstrating position transformation...
  ✓ Imported with transformation

=== Summary ===
✓ Demonstrated position storage in NeuralObject base class
✓ Demonstrated CSV export/import functionality
✓ Demonstrated position transformation (scale + offset)
✓ All positions are persisted to datastore
```

### NeuroML and HDF5 Import Example

```bash
cd build
./neuroml_hdf5_import_example
```

**Output:**
```
=== SNNFW NeuroML and HDF5 Import Example ===

Part 1: Creating sample import files...
  ✓ Created sample NeuroML file: sample_network.nml
  ✓ Created sample HDF5 file: sample_positions.h5

Part 2: Creating neurons for NeuroML import...
  ✓ Created 5 neurons

Part 3: Importing positions from NeuroML...
  ✓ Successfully imported from NeuroML
    - Positions set: 5
    - Neurons imported: 5

Part 4: Verifying NeuroML positions...
  ✓ Verified 5/5 NeuroML positions

Part 5: Creating neurons for HDF5 import...
  ✓ Created 5 neurons

Part 6: Importing positions from HDF5...
  ✓ Successfully imported from HDF5
    - Positions set: 5
    - Neurons imported: 5

Part 7: Verifying HDF5 positions (with transformation)...
  ✓ Verified 5/5 HDF5 positions

=== Summary ===
✓ NeuroML import: 5 positions
✓ HDF5 import: 5 positions
✓ Total positions imported: 10
```

## Implementation Details

### Files Modified

- `include/snnfw/NeuralObject.h` - Added Position3D and position fields
- `src/Neuron.cpp` - Updated serialization
- `src/Cluster.cpp` - Updated serialization
- `include/snnfw/Layer.h` - Updated serialization
- `include/snnfw/Column.h` - Updated serialization
- `include/snnfw/Nucleus.h` - Updated serialization
- `include/snnfw/Region.h` - Updated serialization
- `include/snnfw/Lobe.h` - Updated serialization
- `include/snnfw/Hemisphere.h` - Updated serialization
- `include/snnfw/Brain.h` - Updated serialization
- `src/NetworkDataAdapter.cpp` - Use stored positions
- `include/snnfw/LayoutEngine.h` - Added overrideStoredPositions flag
- `src/LayoutEngine.cpp` - Respect stored positions

### Files Created

- `include/snnfw/ModelImporter.h` - Import utility header (with HDF5 support)
- `src/ModelImporter.cpp` - Import utility implementation (NeuroML + HDF5)
- `examples/position_import_example.cpp` - CSV import example
- `examples/neuroml_hdf5_import_example.cpp` - NeuroML and HDF5 import example
- `docs/POSITION_STORAGE_AND_IMPORT.md` - This documentation

## Future Enhancements

1. **Batch Import** - Parallel import for large datasets
2. **Position Validation** - Check for overlapping neurons, out-of-bounds positions
3. **Coordinate Systems** - Support different coordinate system conventions
4. **Morphology Import** - Import full neuron morphologies, not just soma positions
5. **SONATA Connectivity** - Import synaptic connections from SONATA format
6. **NWB Timeseries** - Import spike times and other timeseries data from NWB files

## See Also

- [Visualization Documentation](VISUALIZATION_PHASE4_PROGRESS.md)
- [Datastore Documentation](../README.md#datastore)
- [Network Builder Documentation](../README.md#network-builder)

