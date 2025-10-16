cmake_minimum_required(VERSION 3.16)

project(snnfw 
    VERSION 1.0.0
    DESCRIPTION "Spiking Neural Network Framework"
    LANGUAGES CXX
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

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

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
FetchContent_MakeAvailable(nlohmann_json)

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

# Source files for library
set(LIB_SOURCES
    src/Neuron.cpp
    src/Cluster.cpp
    src/Logger.cpp
    src/ThreadPool.cpp
    src/Axon.cpp
    src/Dendrite.cpp
    src/Synapse.cpp
    src/SpikeProcessor.cpp
    src/NeuralObjectFactory.cpp
    src/Datastore.cpp
    src/MNISTLoader.cpp
    src/STDPLearning.cpp
)

# Create the library
add_library(${PROJECT_NAME} ${LIB_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        spdlog::spdlog
        Threads::Threads
        nlohmann_json::nlohmann_json
)

# Link RocksDB if found
if(RocksDB_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC RocksDB::rocksdb)
    message(STATUS "Linking RocksDB via target")
elseif(ROCKSDB_LIBRARIES)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${ROCKSDB_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${ROCKSDB_INCLUDE_DIRS})
    message(STATUS "Linking RocksDB manually: ${ROCKSDB_LIBRARIES}")
endif()

# Set target properties
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Set C++ standard
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

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
target_link_libraries(mnist_optimized ${PROJECT_NAME})

add_executable(mnist_pattern_diversity experiments/mnist_pattern_diversity.cpp)
target_link_libraries(mnist_pattern_diversity ${PROJECT_NAME})

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
