# Real-Time Spike Processing

## Overview

The SpikeProcessor in SNNFW now supports **real-time synchronization**, where each timeslice (1 millisecond of simulation time) is synchronized with 1 millisecond of wall-clock time. This enables real-time neural network simulations that can interact with external systems, sensors, or actuators.

## Key Features

### 1. Real-Time Synchronization
- Each 1ms simulation timeslice corresponds to 1ms of wall-clock time
- Automatic drift correction to maintain synchronization
- Configurable: can be enabled or disabled at runtime

### 2. Configurable Threading
- **Default: 20 delivery threads** for parallel spike delivery
- Configurable at construction time
- Optimized for modern multi-core processors

### 3. Timing Statistics
- Average loop time monitoring
- Maximum loop time tracking
- Drift measurement (how far ahead/behind real-time)
- Periodic logging of performance metrics

### 4. Dual Operating Modes

#### Real-Time Mode (Default)
- Synchronizes with wall-clock time
- Sleeps when ahead of schedule
- Warns when falling behind (>10ms drift)
- Ideal for: Real-time applications, robotics, interactive systems

#### Fast Mode
- Runs as fast as possible
- No synchronization delays
- Can achieve 10-100x real-time speedup
- Ideal for: Batch simulations, training, offline analysis

## API Reference

### Constructor

```cpp
SpikeProcessor(size_t timeSliceCount = 10000, size_t deliveryThreads = 20);
```

**Parameters:**
- `timeSliceCount`: Number of 1ms timeslices to buffer (default: 10000 = 10 seconds)
- `deliveryThreads`: Number of threads for parallel spike delivery (default: 20)

**Example:**
```cpp
// Create processor with 20 threads and 10 second buffer
SpikeProcessor processor(10000, 20);

// Create processor with 50 threads for high-throughput
SpikeProcessor processor(10000, 50);

// Create processor with 4 threads for low-power systems
SpikeProcessor processor(10000, 4);
```

### Real-Time Control

```cpp
void setRealTimeSync(bool enable);
bool isRealTimeSync() const;
```

**Example:**
```cpp
SpikeProcessor processor;

// Enable real-time synchronization (default)
processor.setRealTimeSync(true);
processor.start();

// ... run simulation ...

// Switch to fast mode
processor.stop();
processor.setRealTimeSync(false);
processor.start();
```

### Timing Statistics

```cpp
void getTimingStats(double& avgLoopTime, double& maxLoopTime, double& driftMs) const;
```

**Parameters:**
- `avgLoopTime`: Average time per loop iteration (microseconds)
- `maxLoopTime`: Maximum time for any single loop (microseconds)
- `driftMs`: Current drift from real-time (milliseconds)
  - Negative: ahead of schedule
  - Positive: behind schedule

**Example:**
```cpp
double avgLoop, maxLoop, drift;
processor.getTimingStats(avgLoop, maxLoop, drift);

std::cout << "Average loop: " << avgLoop << " μs" << std::endl;
std::cout << "Max loop: " << maxLoop << " μs" << std::endl;
std::cout << "Drift: " << drift << " ms" << std::endl;
```

## Performance Characteristics

### Real-Time Mode

**Typical Performance:**
- Average loop time: 30-100 μs (depending on spike load)
- Maximum loop time: <1000 μs for most workloads
- Drift: ±1-2 ms over extended periods

**Factors Affecting Performance:**
- Number of spikes per timeslice
- Number of delivery threads
- System load and CPU availability
- Cache efficiency

**Warnings:**
- If loop time exceeds 1000 μs consistently, the system will fall behind
- Warning logged when drift exceeds 10ms
- Consider reducing spike rate or increasing thread count

### Fast Mode

**Typical Performance:**
- Average loop time: 2-10 μs (no synchronization overhead)
- Speedup: 10-100x real-time (depending on workload)
- No drift (not synchronized)

## Usage Examples

### Example 1: Real-Time Robot Control

```cpp
#include "snnfw/SpikeProcessor.h"
#include "snnfw/NeuralObjectFactory.h"

// Create processor with 20 threads for real-time control
SpikeProcessor processor(10000, 20);
processor.setRealTimeSync(true);
processor.start();

// Create neural network for motor control
NeuralObjectFactory factory;
auto motorNeuron = factory.createNeuron(50.0, 0.95, 20);
auto dendrite = factory.createDendrite(motorNeuron->getId());
processor.registerDendrite(dendrite);

// Main control loop
while (running) {
    // Read sensors (happens in real-time)
    double sensorValue = readSensor();
    
    // Schedule spike based on sensor input
    if (sensorValue > threshold) {
        auto spike = std::make_shared<ActionPotential>(
            axonId, dendriteId, 
            processor.getCurrentTime() + 5.0,  // 5ms delay
            sensorValue
        );
        processor.scheduleSpike(spike);
    }
    
    // Motor output happens in real-time as spikes are delivered
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
```

### Example 2: Fast Batch Training

```cpp
// Create processor in fast mode for training
SpikeProcessor processor(10000, 20);
processor.setRealTimeSync(false);  // Fast mode
processor.start();

// Train for 1 hour of simulation time
const double trainingDuration = 3600000.0;  // 1 hour in ms

while (processor.getCurrentTime() < trainingDuration) {
    // Generate training spikes
    generateTrainingSpikes(processor);
    
    // Check progress every simulated second
    if (static_cast<int>(processor.getCurrentTime()) % 1000 == 0) {
        std::cout << "Training progress: " 
                  << (processor.getCurrentTime() / trainingDuration * 100.0)
                  << "%" << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// Training completes in minutes instead of hours!
```

### Example 3: Monitoring and Diagnostics

```cpp
SpikeProcessor processor(10000, 20);
processor.start();

// Monitor performance every second
while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    double avgLoop, maxLoop, drift;
    processor.getTimingStats(avgLoop, maxLoop, drift);
    
    std::cout << "Sim time: " << processor.getCurrentTime() << " ms" << std::endl;
    std::cout << "Avg loop: " << avgLoop << " μs" << std::endl;
    std::cout << "Max loop: " << maxLoop << " μs" << std::endl;
    std::cout << "Drift: " << drift << " ms" << std::endl;
    std::cout << "Pending: " << processor.getPendingSpikeCount() << " spikes" << std::endl;
    
    // Alert if performance degrades
    if (drift > 50.0) {
        std::cerr << "WARNING: System falling behind by " << drift << "ms!" << std::endl;
    }
    
    if (maxLoop > 5000.0) {
        std::cerr << "WARNING: Spike burst detected (" << maxLoop << "μs)" << std::endl;
    }
}
```

## Thread Count Guidelines

### Recommended Thread Counts

| System Type | CPU Cores | Recommended Threads | Use Case |
|-------------|-----------|---------------------|----------|
| Embedded | 2-4 | 4-8 | Low-power, simple networks |
| Desktop | 4-8 | 10-20 | Development, medium networks |
| Workstation | 8-16 | 20-40 | Large networks, real-time |
| Server | 16+ | 40-100 | Massive networks, high throughput |

### Tuning Guidelines

1. **Start with default (20 threads)**
2. **Monitor average loop time**
   - If consistently >500μs: increase threads
   - If consistently <50μs: decrease threads (save resources)
3. **Monitor CPU usage**
   - If <50%: increase threads
   - If >90%: decrease threads or reduce spike rate
4. **Test under peak load**
   - Ensure max loop time stays <1000μs

## Logging and Debugging

### Log Levels

The SpikeProcessor logs at different levels:

- **INFO**: Startup, shutdown, configuration
- **DEBUG**: Periodic timing statistics (every 1000 iterations)
- **WARN**: Drift warnings, scheduling failures
- **ERROR**: Critical failures

### Enable Debug Logging

```cpp
Logger::getInstance().setLevel(spdlog::level::debug);
```

### Periodic Statistics

When debug logging is enabled, statistics are logged every 1000ms:

```
[debug] SpikeProcessor: Sim time: 5000.0ms, Avg loop: 45.2μs, Max loop: 892.0μs, Drift: -0.5ms
```

## Best Practices

### 1. Choose Appropriate Mode
- **Real-time mode**: Interactive systems, robotics, live visualization
- **Fast mode**: Training, batch processing, offline analysis

### 2. Size the Buffer Appropriately
- Default 10000ms (10 seconds) is good for most applications
- Increase for systems with highly variable delays
- Decrease for memory-constrained systems

### 3. Monitor Performance
- Check timing statistics regularly
- Set up alerts for excessive drift
- Log max loop times to detect spike bursts

### 4. Handle Drift Gracefully
- Small drift (<10ms) is normal and self-correcting
- Large drift (>50ms) indicates system overload
- Consider: reducing spike rate, increasing threads, or using fast mode

### 5. Thread Count Optimization
- Start with default (20)
- Tune based on actual workload
- Don't exceed CPU core count by more than 2x

## Troubleshooting

### Problem: System Falling Behind (Positive Drift)

**Symptoms:**
- Drift increasing over time
- Frequent warnings about falling behind

**Solutions:**
1. Increase delivery thread count
2. Reduce spike rate
3. Optimize spike delivery code
4. Switch to fast mode if real-time not required

### Problem: High CPU Usage

**Symptoms:**
- CPU at 100%
- System unresponsive

**Solutions:**
1. Reduce delivery thread count
2. Reduce spike rate
3. Increase sleep time in non-real-time mode

### Problem: Inconsistent Timing

**Symptoms:**
- Large variations in loop time
- Occasional very long loops

**Solutions:**
1. Check for system interrupts
2. Disable CPU frequency scaling
3. Use real-time scheduling (Linux: SCHED_FIFO)
4. Reduce background processes

## Future Enhancements

Planned improvements:

- [ ] Adaptive thread count based on load
- [ ] GPU-accelerated spike delivery
- [ ] Distributed processing across multiple machines
- [ ] Real-time priority scheduling support
- [ ] Hardware timestamping for precise synchronization
- [ ] Integration with real-time operating systems (RTOS)

## References

- Brette, R., et al. (2007). "Simulation of networks of spiking neurons"
- Gewaltig, M. O., & Diesmann, M. (2007). "NEST (NEural Simulation Tool)"
- Morrison, A., et al. (2005). "Advancing the boundaries of high-connectivity network simulation"

