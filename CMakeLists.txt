cmake_minimum_required(VERSION 3.16)

project(snnfw
    VERSION 1.0.0
    DESCRIPTION "Spiking Neural Network Framework"
    LANGUAGES C CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find threading library
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find OpenMP for parallelization
find_package(OpenMP REQUIRED)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")

# Include directories
include_directories(include)

# Find spdlog
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    message(STATUS "spdlog not found via find_package, attempting FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# Fetch nlohmann/json for serialization
message(STATUS "Fetching nlohmann/json...")
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
# Make nlohmann_json available first so libsonata can find it
FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
    FetchContent_Populate(nlohmann_json)
    add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR})
endif()

# Set nlohmann_json_DIR so libsonata can find it
set(nlohmann_json_DIR ${nlohmann_json_BINARY_DIR})

# Fetch HighFive (HDF5 C++ wrapper) - required by libsonata
message(STATUS "Fetching HighFive...")
option(HIGHFIVE_USE_BOOST "Enable Boost support" OFF)
option(HIGHFIVE_USE_EIGEN "Enable Eigen support" OFF)
option(HIGHFIVE_BUILD_DOCS "Build documentation" OFF)
option(HIGHFIVE_EXAMPLES "Build examples" OFF)
option(HIGHFIVE_UNIT_TESTS "Build unit tests" OFF)
FetchContent_Declare(
    HighFive
    GIT_REPOSITORY https://github.com/BlueBrain/HighFive.git
    GIT_TAG v2.9.0
)
# Make HighFive available first so libsonata can find it
FetchContent_GetProperties(HighFive)
if(NOT highfive_POPULATED)
    FetchContent_Populate(HighFive)
    add_subdirectory(${highfive_SOURCE_DIR} ${highfive_BINARY_DIR})
endif()

# Set HighFive_DIR so libsonata can find it
set(HighFive_DIR ${highfive_BINARY_DIR})

# Fetch libsonata for SONATA format support
message(STATUS "Fetching libsonata...")
option(SONATA_TESTS "Build libsonata tests" OFF)
option(SONATA_PYTHON "Build libsonata Python bindings" OFF)
option(EXTLIB_FROM_SUBMODULES "Use git submodules for external libraries" OFF)

# Temporarily disable warnings-as-errors for libsonata
set(CMAKE_CXX_FLAGS_BACKUP "${CMAKE_CXX_FLAGS}")
string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

FetchContent_Declare(
    libsonata
    GIT_REPOSITORY https://github.com/BlueBrain/libsonata.git
    GIT_TAG v0.1.27
)
FetchContent_MakeAvailable(libsonata)

# Restore original flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_BACKUP}")

# Find RocksDB for persistent storage
message(STATUS "Looking for RocksDB...")
find_package(RocksDB QUIET CONFIG)
if(RocksDB_FOUND)
    message(STATUS "Found RocksDB via find_package")
else()
    message(STATUS "RocksDB not found via find_package, attempting to find system library")
    # Try to find RocksDB manually
    find_path(ROCKSDB_INCLUDE_DIR rocksdb/db.h PATHS /usr/include /usr/local/include)
    find_library(ROCKSDB_LIBRARY NAMES rocksdb PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

    if(ROCKSDB_INCLUDE_DIR AND ROCKSDB_LIBRARY)
        message(STATUS "Found RocksDB library: ${ROCKSDB_LIBRARY}")
        message(STATUS "Found RocksDB include: ${ROCKSDB_INCLUDE_DIR}")
        set(ROCKSDB_LIBRARIES ${ROCKSDB_LIBRARY})
        set(ROCKSDB_INCLUDE_DIRS ${ROCKSDB_INCLUDE_DIR})
        set(ROCKSDB_FOUND TRUE)
    else()
        message(WARNING "RocksDB not found. Please install it:")
        message(WARNING "  Ubuntu/Debian: sudo apt-get install librocksdb-dev")
        message(WARNING "  macOS: brew install rocksdb")
        message(WARNING "  Or build from source: https://github.com/facebook/rocksdb")
        set(ROCKSDB_FOUND FALSE)
    endif()
endif()

# Visualization dependencies
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/third_party/glfw)
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/third_party/glad)
set(GLM_DIR ${CMAKE_SOURCE_DIR}/third_party/glm)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

# GLFW
if(EXISTS ${GLFW_DIR})
    message(STATUS "Found GLFW in third_party")
    option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
    option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
    option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
    add_subdirectory(${GLFW_DIR})
else()
    message(WARNING "GLFW not found in third_party - visualization will not be available")
endif()

# GLAD
if(EXISTS ${GLAD_DIR})
    message(STATUS "Found GLAD in third_party")
    add_library(glad STATIC ${GLAD_DIR}/src/glad.c)
    target_include_directories(glad PUBLIC ${GLAD_DIR}/include)
else()
    message(WARNING "GLAD not found in third_party - visualization will not be available")
endif()

# ImGui
if(EXISTS ${IMGUI_DIR})
    message(STATUS "Found ImGui in third_party")
    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(imgui PUBLIC glfw glad)
else()
    message(WARNING "ImGui not found in third_party - visualization will not be available")
endif()

# Source files for library
set(LIB_SOURCES
    src/Neuron.cpp
    src/BinaryPattern.cpp
    src/Cluster.cpp
    src/Logger.cpp
    src/ThreadPool.cpp
    src/Axon.cpp
    src/Dendrite.cpp
    src/Synapse.cpp
    src/SpikeProcessor.cpp
    src/NetworkPropagator.cpp
    src/NeuralObjectFactory.cpp
    src/Datastore.cpp
    src/MNISTLoader.cpp
    src/EMNISTLoader.cpp
    src/STDPLearning.cpp
    src/HyperparameterOptimizer.cpp
    src/ConfigOptimizer.cpp
    src/adapters/RetinaAdapter.cpp
    src/adapters/DisplayAdapter.cpp
    src/adapters/AudioAdapter.cpp
    src/encoding/RateEncoder.cpp
    src/encoding/TemporalEncoder.cpp
    src/encoding/PopulationEncoder.cpp
    src/encoding/EncodingStrategyFactory.cpp
    src/features/SobelOperator.cpp
    src/features/GaborOperator.cpp
    src/features/DoGOperator.cpp
    src/features/EdgeOperatorFactory.cpp
    src/classification/MajorityVoting.cpp
    src/classification/WeightedDistance.cpp
    src/classification/WeightedSimilarity.cpp
    src/classification/ClassificationStrategyFactory.cpp
    src/learning/AppendStrategy.cpp
    src/learning/ReplaceWorstStrategy.cpp
    src/learning/MergeSimilarStrategy.cpp
    src/learning/HybridStrategy.cpp
    src/learning/PatternUpdateStrategyFactory.cpp
    src/NetworkInspector.cpp
    src/ActivityMonitor.cpp
    src/PerformanceProfiler.cpp
    src/NetworkValidator.cpp
    src/NetworkBuilder.cpp
    src/ConnectivityPattern.cpp
    src/ConnectivityBuilder.cpp
    src/ModelImporter.cpp
)

# Visualization sources (optional)
if(EXISTS ${GLFW_DIR} AND EXISTS ${GLAD_DIR} AND EXISTS ${IMGUI_DIR})
    list(APPEND LIB_SOURCES
        src/Camera.cpp
        src/ShaderManager.cpp
        src/GeometryRenderer.cpp
        src/VisualizationManager.cpp
        src/NetworkDataAdapter.cpp
        src/LayoutEngine.cpp
        src/NetworkGraphRenderer.cpp
        src/ActivityVisualizer.cpp
        src/SpikeRenderer.cpp
        src/RecordingManager.cpp
        src/RasterPlotRenderer.cpp
        src/InteractionManager.cpp
        src/PatternDetector.cpp
        src/ActivityHistogram.cpp
    )
    set(VISUALIZATION_ENABLED TRUE)
    message(STATUS "Visualization support ENABLED")
else()
    set(VISUALIZATION_ENABLED FALSE)
    message(STATUS "Visualization support DISABLED (missing dependencies)")
endif()

# Create the library
add_library(${PROJECT_NAME} ${LIB_SOURCES})

# Find HDF5
find_package(HDF5 COMPONENTS C)
if(HDF5_FOUND)
    message(STATUS "HDF5 found: ${HDF5_VERSION}")
else()
    message(WARNING "HDF5 not found - HDF5 import will not be available")
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        spdlog::spdlog
        Threads::Threads
        nlohmann_json::nlohmann_json
)

# Link HDF5 if found
if(HDF5_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${HDF5_LIBRARIES} curl)
    target_include_directories(${PROJECT_NAME} PUBLIC ${HDF5_INCLUDE_DIRS})
    message(STATUS "Linking HDF5: ${HDF5_LIBRARIES}")
endif()

# Link RocksDB if found
if(RocksDB_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC RocksDB::rocksdb)
    message(STATUS "Linking RocksDB via target")
elseif(ROCKSDB_LIBRARIES)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${ROCKSDB_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${ROCKSDB_INCLUDE_DIRS})
    message(STATUS "Linking RocksDB manually: ${ROCKSDB_LIBRARIES}")
endif()

# Link visualization libraries if enabled
if(VISUALIZATION_ENABLED)
    target_link_libraries(${PROJECT_NAME} PUBLIC glfw glad imgui)
    target_include_directories(${PROJECT_NAME} PUBLIC ${GLM_DIR})
    message(STATUS "Linking visualization libraries")
endif()

# Set target properties
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Set C++ standard
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# Link OpenMP
target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)

# Main executable
add_executable(${PROJECT_NAME}_app src/Main.cpp)
target_link_libraries(${PROJECT_NAME}_app ${PROJECT_NAME})

# Example executables
add_executable(factory_example examples/factory_example.cpp)
target_link_libraries(factory_example ${PROJECT_NAME})

add_executable(hierarchical_structure_example examples/hierarchical_structure_example.cpp)
target_link_libraries(hierarchical_structure_example ${PROJECT_NAME})

add_executable(datastore_example examples/datastore_example.cpp)
target_link_libraries(datastore_example ${PROJECT_NAME})

add_executable(experiment_config_example examples/experiment_config_example.cpp)
target_link_libraries(experiment_config_example ${PROJECT_NAME})

add_executable(realtime_spike_example examples/realtime_spike_example.cpp)
target_link_libraries(realtime_spike_example ${PROJECT_NAME})

add_executable(position_import_example examples/position_import_example.cpp)
target_link_libraries(position_import_example ${PROJECT_NAME})

add_executable(neuroml_hdf5_import_example examples/neuroml_hdf5_import_example.cpp)
target_link_libraries(neuroml_hdf5_import_example ${PROJECT_NAME})

# Visualization demos (if enabled)
if(VISUALIZATION_ENABLED)
    add_executable(visualization_demo examples/visualization_demo.cpp)
    target_link_libraries(visualization_demo ${PROJECT_NAME})
    message(STATUS "Added visualization_demo executable")

    add_executable(network_visualization_demo examples/network_visualization_demo.cpp)
    target_link_libraries(network_visualization_demo ${PROJECT_NAME})
    message(STATUS "Added network_visualization_demo executable")

    add_executable(activity_demo examples/activity_demo.cpp)
    target_link_libraries(activity_demo ${PROJECT_NAME})
    message(STATUS "Added activity_demo executable")
endif()

# Experiments
add_executable(mnist_learning experiments/mnist_learning.cpp)
target_link_libraries(mnist_learning ${PROJECT_NAME})

add_executable(mnist_simple_test experiments/mnist_simple_test.cpp)
target_link_libraries(mnist_simple_test ${PROJECT_NAME})

add_executable(mnist_debug_retina experiments/mnist_debug_retina.cpp)
target_link_libraries(mnist_debug_retina ${PROJECT_NAME})

add_executable(mnist_debug_learning experiments/mnist_debug_learning.cpp)
target_link_libraries(mnist_debug_learning ${PROJECT_NAME})

add_executable(mnist_full_test experiments/mnist_full_test.cpp)
target_link_libraries(mnist_full_test ${PROJECT_NAME})

add_executable(mnist_visualize_patterns experiments/mnist_visualize_patterns.cpp)
target_link_libraries(mnist_visualize_patterns ${PROJECT_NAME})

add_executable(mnist_fuzzy_test experiments/mnist_fuzzy_test.cpp)
target_link_libraries(mnist_fuzzy_test ${PROJECT_NAME})

add_executable(mnist_spatial_sdr experiments/mnist_spatial_sdr.cpp)
target_link_libraries(mnist_spatial_sdr ${PROJECT_NAME})

add_executable(mnist_edge_features experiments/mnist_edge_features.cpp)
target_link_libraries(mnist_edge_features ${PROJECT_NAME})

add_executable(mnist_two_layer experiments/mnist_two_layer.cpp)
target_link_libraries(mnist_two_layer ${PROJECT_NAME})

add_executable(mnist_activation_matching experiments/mnist_activation_matching.cpp)
target_link_libraries(mnist_activation_matching ${PROJECT_NAME})

add_executable(mnist_hyperparameter_search experiments/mnist_hyperparameter_search.cpp)
target_link_libraries(mnist_hyperparameter_search ${PROJECT_NAME})

add_executable(mnist_optimized experiments/mnist_optimized.cpp)
find_package(CURL REQUIRED)
target_link_libraries(mnist_optimized ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl)
target_include_directories(mnist_optimized PRIVATE
    ${highfive_SOURCE_DIR}/include
    ${libsonata_SOURCE_DIR}/include
    ${HDF5_INCLUDE_DIRS}
)

add_executable(mnist_pattern_diversity experiments/mnist_pattern_diversity.cpp)
target_link_libraries(mnist_pattern_diversity ${PROJECT_NAME})

add_executable(mnist_with_adapters experiments/mnist_with_adapters.cpp)
find_package(CURL REQUIRED)
target_link_libraries(mnist_with_adapters ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl)
target_include_directories(mnist_with_adapters PRIVATE
    ${highfive_SOURCE_DIR}/include
    ${libsonata_SOURCE_DIR}/include
)

add_executable(debug_gabor experiments/debug_gabor.cpp)
target_link_libraries(debug_gabor ${PROJECT_NAME})

add_executable(mnist_hyperparameter_optimization experiments/mnist_hyperparameter_optimization.cpp)
target_link_libraries(mnist_hyperparameter_optimization ${PROJECT_NAME})

add_executable(mnist_classification_strategies experiments/mnist_classification_strategies.cpp)
find_package(CURL REQUIRED)
target_link_libraries(mnist_classification_strategies ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl)
target_include_directories(mnist_classification_strategies PRIVATE
    ${highfive_SOURCE_DIR}/include
    ${libsonata_SOURCE_DIR}/include
)

add_executable(mnist_learning_strategies experiments/mnist_learning_strategies.cpp)
target_link_libraries(mnist_learning_strategies ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl)
target_include_directories(mnist_learning_strategies PRIVATE
    ${highfive_SOURCE_DIR}/include
    ${libsonata_SOURCE_DIR}/include
)

add_executable(mnist_v1_hierarchical experiments/mnist_v1_hierarchical.cpp)
target_link_libraries(mnist_v1_hierarchical ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl OpenMP::OpenMP_CXX)
target_include_directories(mnist_v1_hierarchical PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/libsonata/include
)

add_executable(mnist_v1_multicolumn experiments/mnist_v1_multicolumn.cpp)
target_link_libraries(mnist_v1_multicolumn ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl OpenMP::OpenMP_CXX)
target_include_directories(mnist_v1_multicolumn PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/libsonata/include
)

add_executable(mnist_v1_optimized experiments/mnist_v1_optimized.cpp)
target_link_libraries(mnist_v1_optimized ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl OpenMP::OpenMP_CXX)
target_include_directories(mnist_v1_optimized PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/libsonata/include
)

add_executable(mnist_v1_24col experiments/mnist_v1_24col.cpp)
target_link_libraries(mnist_v1_24col ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl OpenMP::OpenMP_CXX)
target_include_directories(mnist_v1_24col PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/libsonata/include
)

add_executable(mnist_v1_32col experiments/mnist_v1_32col.cpp)
target_link_libraries(mnist_v1_32col ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl OpenMP::OpenMP_CXX)
target_include_directories(mnist_v1_32col PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/libsonata/include
)

add_executable(mnist_orientation_columns experiments/mnist_orientation_columns.cpp)
target_link_libraries(mnist_orientation_columns ${PROJECT_NAME} sonata::sonata_shared HighFive CURL::libcurl OpenMP::OpenMP_CXX)
target_include_directories(mnist_orientation_columns PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/libsonata/include
)

# EMNIST experiments
add_executable(test_emnist_loader experiments/test_emnist_loader.cpp)
target_link_libraries(test_emnist_loader ${PROJECT_NAME})

add_executable(emnist_letters_v1 experiments/emnist_letters_v1.cpp)
target_link_libraries(emnist_letters_v1 ${PROJECT_NAME})
target_link_libraries(emnist_letters_v1 CURL::libcurl)

add_executable(emnist_letters_optimize experiments/emnist_letters_optimize.cpp)
target_link_libraries(emnist_letters_optimize ${PROJECT_NAME})

# Analysis tools
add_executable(analyze_network_stats tools/analyze_network_stats.cpp)
target_link_libraries(analyze_network_stats ${PROJECT_NAME})

# Testing (optional)
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation (simplified - no export for now due to spdlog dependency)
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)

# Saccades debug test
add_executable(test_saccades_debug experiments/test_saccades_debug.cpp)
target_link_libraries(test_saccades_debug ${PROJECT_NAME})
