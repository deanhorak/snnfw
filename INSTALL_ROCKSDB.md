# Installing RocksDB

## Overview

RocksDB is a high-performance embedded database for key-value data. It's required for the SNNFW Datastore to provide persistent storage for neural network objects.

## Quick Install

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install librocksdb-dev
```

### macOS
```bash
brew install rocksdb
```

### Fedora/RHEL/CentOS
```bash
sudo dnf install rocksdb-devel
```

## Building from Source

If the package manager version is outdated or unavailable, build from source:

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt-get install -y \
    build-essential \
    libgflags-dev \
    libsnappy-dev \
    zlib1g-dev \
    libbz2-dev \
    liblz4-dev \
    libzstd-dev
```

**macOS:**
```bash
brew install gflags snappy lz4 zstd
```

### Build and Install

```bash
# Clone repository
git clone https://github.com/facebook/rocksdb.git
cd rocksdb

# Build shared library
make shared_lib -j$(nproc)

# Install (requires sudo)
sudo make install-shared

# Update library cache (Linux only)
sudo ldconfig
```

### Custom Installation Path

If you don't have sudo access, install to a custom location:

```bash
# Build
make shared_lib -j$(nproc)

# Install to custom path
PREFIX=$HOME/local make install-shared

# Add to your environment
export LD_LIBRARY_PATH=$HOME/local/lib:$LD_LIBRARY_PATH
export CPLUS_INCLUDE_PATH=$HOME/local/include:$CPLUS_INCLUDE_PATH
```

Then update your CMakeLists.txt:
```cmake
set(CMAKE_PREFIX_PATH "$ENV{HOME}/local" ${CMAKE_PREFIX_PATH})
```

## Verification

Verify RocksDB is installed correctly:

```bash
# Check library
ldconfig -p | grep rocksdb

# Check headers
ls /usr/include/rocksdb/db.h
# or
ls /usr/local/include/rocksdb/db.h
```

## CMake Integration

The SNNFW CMakeLists.txt automatically detects RocksDB:

```cmake
find_package(RocksDB QUIET)
if(NOT RocksDB_FOUND)
    # Try manual detection
    find_path(ROCKSDB_INCLUDE_DIR rocksdb/db.h)
    find_library(ROCKSDB_LIBRARY NAMES rocksdb)
endif()
```

## Troubleshooting

### Library Not Found at Runtime

**Linux:**
```bash
# Add to ~/.bashrc
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Or update ldconfig
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/rocksdb.conf
sudo ldconfig
```

**macOS:**
```bash
# Add to ~/.zshrc or ~/.bash_profile
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
```

### CMake Can't Find RocksDB

```bash
# Set CMAKE_PREFIX_PATH
cmake -DCMAKE_PREFIX_PATH=/usr/local ..

# Or set environment variable
export CMAKE_PREFIX_PATH=/usr/local:$CMAKE_PREFIX_PATH
```

### Compilation Errors

If you get compilation errors, ensure you have all dependencies:

```bash
# Ubuntu/Debian
sudo apt-get install libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev

# macOS
brew install gflags snappy lz4 zstd
```

## Version Requirements

- **Minimum Version**: RocksDB 6.0+
- **Recommended Version**: RocksDB 7.0+ or latest
- **C++ Standard**: C++17 (already required by SNNFW)

## Performance Tuning

For optimal performance, consider these RocksDB build options:

```bash
# Build with optimizations
make shared_lib DEBUG_LEVEL=0 -j$(nproc)

# Build with specific compression
USE_SNAPPY=1 USE_LZ4=1 USE_ZSTD=1 make shared_lib -j$(nproc)
```

## Docker Installation

If using Docker, add to your Dockerfile:

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    librocksdb-dev \
    libspdlog-dev \
    && rm -rf /var/lib/apt/lists/*
```

## Next Steps

After installing RocksDB:

1. Build SNNFW:
   ```bash
   cd /path/to/snnfw
   mkdir -p build && cd build
   cmake ..
   make
   ```

2. Run tests to verify Datastore functionality:
   ```bash
   ctest --output-on-failure
   ```

3. See [DATASTORE_ARCHITECTURE.md](DATASTORE_ARCHITECTURE.md) for usage examples

## Additional Resources

- [RocksDB GitHub](https://github.com/facebook/rocksdb)
- [RocksDB Wiki](https://github.com/facebook/rocksdb/wiki)
- [RocksDB Tuning Guide](https://github.com/facebook/rocksdb/wiki/RocksDB-Tuning-Guide)

