# Logging Implementation Documentation

## Overview

The SNNFW framework now includes a comprehensive logging facility using **spdlog**, a fast C++ logging library. This replaces all previous `std::cout` calls with structured, level-based logging.

## Library Choice: spdlog

**Why spdlog?**
- **Performance**: One of the fastest C++ logging libraries
- **Header-only option**: Can be used without compilation
- **Thread-safe**: Safe for multi-threaded applications
- **Multiple sinks**: Can log to console, files, and more simultaneously
- **Formatting**: Supports modern formatting with fmt library
- **Widely adopted**: Industry standard with excellent documentation

## Architecture

### Logger Class (Singleton Pattern)

**File:** `include/snnfw/Logger.h`, `src/Logger.cpp`

The `Logger` class provides a singleton interface to spdlog:

```cpp
class Logger {
public:
    static Logger& getInstance();
    void initialize(const std::string& logFileName = "snnfw.log", 
                   spdlog::level::level_enum level = spdlog::level::info);
    void setLevel(spdlog::level::level_enum level);
    std::shared_ptr<spdlog::logger> getLogger();
};
```

**Key Features:**
- Singleton pattern ensures one logger instance across the application
- Dual output: console (colored) and file
- Configurable log levels
- Automatic fallback to console-only if file logging fails

### Log Levels

The logging system supports six severity levels (from least to most severe):

1. **TRACE** - Very detailed information, typically only enabled during development
2. **DEBUG** - Detailed information useful for debugging
3. **INFO** - General informational messages (default level)
4. **WARN** - Warning messages for potentially harmful situations
5. **ERROR** - Error messages for error events
6. **CRITICAL** - Critical messages for very severe error events

### Convenience Macros

Six macros are provided for easy logging:

```cpp
SNNFW_TRACE("Detailed trace: {}", value);
SNNFW_DEBUG("Debug info: {}", value);
SNNFW_INFO("Information: {}", value);
SNNFW_WARN("Warning: {}", value);
SNNFW_ERROR("Error: {}", value);
SNNFW_CRITICAL("Critical: {}", value);
```

**Benefits:**
- Consistent logging interface across the codebase
- Automatic formatting with `{}` placeholders (fmt-style)
- Type-safe (compile-time checking)
- Zero overhead when log level filters out the message

## Implementation Details

### Initialization

The logger must be initialized before use (typically in `main()`):

```cpp
#include "snnfw/Logger.h"
#include <spdlog/spdlog.h>

int main() {
    // Initialize with file name and log level
    snnfw::Logger::getInstance().initialize("snnfw.log", spdlog::level::info);
    
    SNNFW_INFO("Application started");
    // ... rest of application
    return 0;
}
```

**Default Behavior:**
- If not explicitly initialized, creates a console-only logger at INFO level
- Initialization creates both console and file sinks
- Console output is colored for better readability
- File output includes all log levels (controlled by overall level setting)

### Log Format

**Console Format:**
```
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message
```

**File Format:**
```
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message
```

Example:
```
[2025-10-13 21:03:21.459] [info] Logger initialized with file: snnfw.log
[2025-10-13 21:03:21.459] [debug] Neuron 1: Learned new pattern (size=3)
[2025-10-13 21:03:21.459] [warning] This is a warning message
```

### Code Changes

#### Neuron Class

**Before:**
```cpp
std::cout << "Neuron fires a new spike at time: " << spikeTime << std::endl;
std::cout << "Learned new pattern (size=" << spikes.size() << ")." << std::endl;
```

**After:**
```cpp
SNNFW_INFO("Neuron {} fires a new spike at time: {}", getId(), spikeTime);
SNNFW_DEBUG("Neuron {}: Learned new pattern (size={})", getId(), spikes.size());
```

**Benefits:**
- Includes neuron ID for better traceability
- Uses appropriate log levels (INFO for firing, DEBUG for learning)
- Structured formatting
- Can be filtered by log level

#### Cluster Class

**Before:**
```cpp
std::cout << "Cluster ID: " << getId() << std::endl;
std::cout << "Number of neurons: " << neurons.size() << std::endl;
```

**After:**
```cpp
SNNFW_INFO("Cluster ID: {}", getId());
SNNFW_INFO("Number of neurons: {}", neurons.size());
```

## Usage Examples

### Basic Usage

```cpp
#include "snnfw/Logger.h"

// Initialize logger
snnfw::Logger::getInstance().initialize("app.log", spdlog::level::debug);

// Log messages
SNNFW_DEBUG("Starting computation with {} neurons", neuronCount);
SNNFW_INFO("Network initialized successfully");
SNNFW_WARN("Memory usage is high: {} MB", memoryMB);
SNNFW_ERROR("Failed to load configuration file: {}", filename);
```

### Changing Log Level at Runtime

```cpp
// Start with INFO level
snnfw::Logger::getInstance().initialize("app.log", spdlog::level::info);

SNNFW_DEBUG("This will NOT be shown");
SNNFW_INFO("This will be shown");

// Change to DEBUG level
snnfw::Logger::getInstance().setLevel(spdlog::level::debug);

SNNFW_DEBUG("This will NOW be shown");
```

### Production vs Development

```cpp
#ifdef NDEBUG
    // Production: only warnings and errors
    snnfw::Logger::getInstance().initialize("snnfw.log", spdlog::level::warn);
#else
    // Development: all debug information
    snnfw::Logger::getInstance().initialize("snnfw_debug.log", spdlog::level::debug);
#endif
```

## Build System Integration

### CMakeLists.txt Changes

```cmake
# Find or fetch spdlog
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

# Link spdlog to library
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)
```

**Benefits:**
- Automatic download if spdlog not found
- No manual installation required
- Version pinned to v1.12.0 for stability

## Performance Considerations

### Overhead

- **Filtered messages**: Near-zero overhead (compile-time check)
- **Active messages**: ~1-2 microseconds per log call
- **File I/O**: Asynchronous flushing available (not currently enabled)

### Best Practices

1. **Use appropriate levels:**
   - TRACE/DEBUG: Development only
   - INFO: Important events
   - WARN/ERROR/CRITICAL: Always enabled in production

2. **Avoid expensive operations in log calls:**
   ```cpp
   // BAD: Always computes even if DEBUG is disabled
   SNNFW_DEBUG("Result: {}", expensiveComputation());
   
   // GOOD: Check level first
   if (snnfw::Logger::getInstance().getLogger()->should_log(spdlog::level::debug)) {
       SNNFW_DEBUG("Result: {}", expensiveComputation());
   }
   ```

3. **Use formatting instead of string concatenation:**
   ```cpp
   // BAD: Creates temporary strings
   SNNFW_INFO("Neuron " + std::to_string(id) + " fired");
   
   // GOOD: Efficient formatting
   SNNFW_INFO("Neuron {} fired", id);
   ```

## Testing Considerations

### Current Status

The existing unit tests were designed to capture `std::cout` output. With the migration to spdlog, these tests need to be updated.

**Options:**

1. **Remove output checking from tests** - Focus on behavioral testing
2. **Capture spdlog output** - Use custom sinks to capture log messages
3. **Disable logging in tests** - Set log level to OFF during testing

### Recommended Approach

For unit tests, it's better to test behavior rather than log output:

```cpp
// Instead of checking log output
TEST(NeuronTest, LearnPattern) {
    Neuron neuron(50.0, 0.95);
    neuron.insertSpike(10.0);
    neuron.learnCurrentPattern();
    
    // Test behavior, not log output
    EXPECT_EQ(neuron.getReferencePatternCount(), 1);
}
```

## Future Enhancements

### Potential Improvements

1. **Async Logging**: Enable asynchronous logging for better performance
2. **Rotating Files**: Implement log file rotation (size/time based)
3. **Custom Sinks**: Add network logging, database logging, etc.
4. **Per-Component Loggers**: Separate loggers for different subsystems
5. **Log Filtering**: Filter by component, neuron ID, etc.
6. **Performance Metrics**: Log timing information for profiling

### Example: Async Logging

```cpp
void Logger::initialize(const std::string& logFileName, 
                       spdlog::level::level_enum level) {
    // Create async logger
    spdlog::init_thread_pool(8192, 1);
    auto async_file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName);
    logger_ = std::make_shared<spdlog::async_logger>(
        "snnfw", async_file, spdlog::thread_pool());
}
```

## Migration Guide

### For Existing Code

1. **Include the logger header:**
   ```cpp
   #include "snnfw/Logger.h"
   ```

2. **Replace std::cout calls:**
   ```cpp
   // Old
   std::cout << "Message: " << value << std::endl;
   
   // New
   SNNFW_INFO("Message: {}", value);
   ```

3. **Choose appropriate log level:**
   - User-facing information → INFO
   - Debugging details → DEBUG
   - Warnings → WARN
   - Errors → ERROR

4. **Initialize in main():**
   ```cpp
   snnfw::Logger::getInstance().initialize("snnfw.log", spdlog::level::info);
   ```

## Files Modified/Created

### Created:
- `include/snnfw/Logger.h` - Logger class header
- `src/Logger.cpp` - Logger implementation
- `examples/logging_example.cpp` - Logging demonstration
- `LOGGING_IMPLEMENTATION.md` - This document

### Modified:
- `src/Neuron.cpp` - Replaced std::cout with logging macros
- `src/Cluster.cpp` - Replaced std::cout with logging macros
- `src/Main.cpp` - Added logger initialization
- `CMakeLists.txt` - Added spdlog dependency

## Summary

The logging implementation provides:
- ✅ Professional logging infrastructure
- ✅ Multiple severity levels (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)
- ✅ Dual output (console + file)
- ✅ Colored console output
- ✅ Efficient formatting
- ✅ Easy-to-use macros
- ✅ Runtime level adjustment
- ✅ Automatic dependency management via CMake

This foundation enables better debugging, monitoring, and production deployment of the SNNFW framework.

