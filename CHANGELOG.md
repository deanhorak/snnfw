# Changelog

All notable changes to the SNNFW project will be documented in this file.

## [Unreleased]

### Added - 2025-10-14

#### New Base Class: NeuralObject
- Created `NeuralObject` base class for all neural components
- Provides 64-bit unique identifier (`uint64_t id`) for all neural objects
- Virtual destructor for proper polymorphic cleanup
- Protected constructor pattern - only derived classes can instantiate
- Getter and setter methods for ID management

**Files:**
- `include/snnfw/NeuralObject.h`

#### New Class: Cluster
- Created `Cluster` class as a container for managing groups of neurons
- Inherits from `NeuralObject` base class
- Uses `std::shared_ptr<Neuron>` for flexible memory management
- Supports adding, removing, and accessing neurons by index
- Provides cluster information display functionality

**Features:**
- Add neurons to cluster
- Remove neurons by index
- Access neurons by index with bounds checking
- Get all neurons as a vector
- Clear all neurons from cluster
- Query cluster size
- Print cluster information (ID and neuron list)

**Files:**
- `include/snnfw/Cluster.h`
- `src/Cluster.cpp`

#### Updated Class: Neuron
- Refactored `Neuron` class to inherit from `NeuralObject`
- Added `neuronId` parameter to constructor (default: 0)
- Maintains all existing temporal pattern learning functionality
- Now has unique identifier accessible via `getId()`

**Modified Files:**
- `include/snnfw/Neuron.h`
- `src/Neuron.cpp`

#### Comprehensive Unit Tests
- Created 19 unit tests for `Cluster` class
- All 15 existing `Neuron` tests still pass
- Total: 34 unit tests with 100% pass rate

**Test Coverage for Cluster:**
- Constructor with ID and default constructor
- Adding single and multiple neurons
- Null neuron handling
- Getting neurons by valid/invalid index
- Removing neurons by valid/invalid index
- Clearing all neurons
- Getting neurons vector
- Neuron ID preservation
- Inheritance verification
- Print functionality
- Neuron functionality within cluster
- Multiple clusters with different IDs
- Shared neuron ownership

**Files:**
- `tests/test_cluster.cpp`
- Updated `tests/CMakeLists.txt`

#### Examples
- Created `examples/cluster_example.cpp` demonstrating:
  - Creating a cluster with unique ID
  - Creating neurons with unique IDs
  - Adding neurons to cluster
  - Accessing neurons from cluster
  - Testing neuron functionality within cluster
  - Removing neurons from cluster
  - Displaying cluster information

#### Documentation
- Updated `README.md` with:
  - New class descriptions (NeuralObject, Neuron, Cluster)
  - Code examples for basic usage
  - Architecture diagram showing inheritance hierarchy
  - Detailed API documentation for all classes
  
- Created `ARCHITECTURE.md` with:
  - Comprehensive architecture documentation
  - Class hierarchy and relationships
  - Design patterns used
  - Memory management model
  - Future extension plans
  - Performance considerations
  - Testing coverage details

#### Build System
- Updated `CMakeLists.txt` to include `src/Cluster.cpp`
- Updated `tests/CMakeLists.txt` to build and run cluster tests
- All build targets compile successfully

### Technical Details

#### Class Hierarchy
```
NeuralObject (Abstract Base Class)
├── Neuron (Temporal Pattern Learning)
└── Cluster (Neuron Container)
```

#### Memory Management
- Neurons are managed with `std::shared_ptr` for flexible ownership
- Clusters can share neurons (multiple clusters can contain the same neuron)
- Automatic cleanup when last reference is released
- No manual memory management required

#### Backward Compatibility
- All existing Neuron functionality preserved
- Existing tests continue to pass
- Optional `neuronId` parameter maintains backward compatibility
- No breaking changes to existing API

### Build and Test Results

**Build Status:** ✅ Success
- Library: `libsnnfw.a` compiled successfully
- Application: `snnfw_app` compiled successfully
- Tests: All test executables compiled successfully

**Test Status:** ✅ All Passing
- NeuronTests: 15/15 passed
- ClusterTests: 19/19 passed
- Total: 34/34 passed (100%)

**Example Status:** ✅ Working
- `cluster_example` runs successfully
- Demonstrates all cluster functionality
- Shows proper ID management

### Files Added
```
include/snnfw/NeuralObject.h
include/snnfw/Cluster.h
src/Cluster.cpp
tests/test_cluster.cpp
examples/cluster_example.cpp
ARCHITECTURE.md
CHANGELOG.md
```

### Files Modified
```
include/snnfw/Neuron.h
src/Neuron.cpp
CMakeLists.txt
tests/CMakeLists.txt
README.md
```

### Next Steps

The framework now has a solid foundation with:
- ✅ Base class for all neural objects (NeuralObject)
- ✅ Temporal pattern learning neurons (Neuron)
- ✅ Neuron container/grouping (Cluster)
- ✅ Unique identification system
- ✅ Comprehensive unit tests
- ✅ Example code and documentation

**Future Development:**
- [ ] Implement automatic ID generation system
- [ ] Add synaptic connections between neurons
- [ ] Create Network class for higher-level organization
- [ ] Implement learning rules (STDP, etc.)
- [ ] Add serialization/deserialization
- [ ] Performance optimizations
- [ ] Visualization tools

---

## [0.1.0] - Initial Release

### Added
- Basic project structure
- Neuron class with temporal pattern learning
- Pattern recognition using cosine similarity
- Rolling time window for spike management
- Unit tests for Neuron class
- CMake build system
- README and basic documentation

