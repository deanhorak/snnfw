# SNNFW Visualization Phase 4: Advanced Features - Design

## Overview

Phase 4 adds advanced visualization features for analyzing temporal patterns, recording/playback, and enhanced interaction capabilities.

**Status:** 🔨 **IN PROGRESS**

**Date:** November 23, 2025

---

## Goals

1. **Temporal Pattern Visualization**
   - Spike raster plots showing neuron firing over time
   - Activity histograms and frequency analysis
   - Temporal correlation visualization

2. **Recording and Playback**
   - Save spike recordings to file
   - Load and replay recordings
   - Playback controls (play/pause, speed, loop)

3. **Pattern Detection**
   - Detect synchronous firing patterns
   - Detect oscillations and rhythms
   - Detect propagation waves

4. **Enhanced Interaction**
   - Click to select neurons
   - Highlight neuron connections
   - Filter by neuron type or activity level
   - Camera bookmarks

---

## Component 1: RasterPlotRenderer

### Purpose
Render spike raster plots showing neuron firing patterns over time.

### Data Structures

```cpp
struct RasterPlotConfig {
    float timeWindow;           // Time window to display (ms)
    float neuronHeight;         // Height of each neuron row (pixels)
    float spikeMarkerSize;      // Size of spike markers
    glm::vec4 spikeColor;       // Color of spike markers
    glm::vec4 backgroundColor;  // Background color
    bool showGrid;              // Show time grid lines
    float gridInterval;         // Grid line interval (ms)
    bool autoScroll;            // Auto-scroll with current time
};

struct RasterPlotData {
    uint64_t neuronId;
    std::vector<uint64_t> spikeTimes;  // Spike timestamps
    int rowIndex;                       // Row in raster plot
};
```

### Class Interface

```cpp
class RasterPlotRenderer {
public:
    RasterPlotRenderer();
    ~RasterPlotRenderer();
    
    // Initialize renderer
    bool initialize();
    
    // Render raster plot
    void render(const std::vector<RasterPlotData>& data,
                uint64_t currentTime,
                const RasterPlotConfig& config);
    
    // Update data
    void addSpike(uint64_t neuronId, uint64_t timestamp);
    void clearHistory();
    
    // Configuration
    void setConfig(const RasterPlotConfig& config);
    const RasterPlotConfig& getConfig() const;
    
    // Cleanup
    void cleanup();
    
private:
    RasterPlotConfig config_;
    GLuint vao_, vbo_;
    std::map<uint64_t, std::vector<uint64_t>> spikeHistory_;
};
```

### Rendering Approach

- Use 2D orthographic projection for raster plot
- Each neuron gets a horizontal row
- Spikes rendered as small circles or vertical lines
- Time axis scrolls horizontally
- Grid lines for time reference

---

## Component 2: ActivityHistogram

### Purpose
Display activity histograms showing spike frequency distribution.

### Data Structures

```cpp
struct HistogramConfig {
    int binCount;               // Number of histogram bins
    float binWidth;             // Width of each bin (ms)
    float maxHeight;            // Maximum bar height (pixels)
    glm::vec4 barColor;         // Color of histogram bars
    glm::vec4 outlineColor;     // Color of bar outlines
    bool showAverage;           // Show average line
    bool showStdDev;            // Show standard deviation
};

struct HistogramData {
    std::vector<float> binValues;  // Spike count per bin
    float average;                  // Average spike rate
    float stdDev;                   // Standard deviation
    float maxValue;                 // Maximum bin value
};
```

### Class Interface

```cpp
class ActivityHistogram {
public:
    ActivityHistogram();
    ~ActivityHistogram();
    
    // Initialize
    bool initialize();
    
    // Render histogram
    void render(const HistogramData& data,
                const HistogramConfig& config);
    
    // Compute histogram from spike data
    HistogramData computeHistogram(
        const std::vector<uint64_t>& spikeTimes,
        uint64_t startTime,
        uint64_t endTime,
        const HistogramConfig& config);
    
    // Configuration
    void setConfig(const HistogramConfig& config);
    
    // Cleanup
    void cleanup();
    
private:
    HistogramConfig config_;
    GLuint vao_, vbo_;
};
```

---

## Component 3: RecordingManager

### Purpose
Manage recording and playback of spike activity.

### Data Structures

```cpp
struct RecordingMetadata {
    std::string name;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t duration;
    size_t spikeCount;
    size_t neuronCount;
    std::string timestamp;      // ISO 8601 timestamp
};

struct PlaybackState {
    bool playing;
    bool paused;
    bool looping;
    float speed;                // Playback speed multiplier
    uint64_t currentTime;
    uint64_t startTime;
    uint64_t endTime;
};
```

### Class Interface

```cpp
class RecordingManager {
public:
    RecordingManager(ActivityVisualizer& visualizer);
    ~RecordingManager();
    
    // Recording control
    void startRecording();
    void stopRecording();
    bool isRecording() const;
    
    // Save/load recordings
    bool saveRecording(const std::string& filename);
    bool loadRecording(const std::string& filename);
    
    // Playback control
    void play();
    void pause();
    void stop();
    void setSpeed(float speed);
    void setLooping(bool loop);
    void seek(uint64_t time);
    
    // State
    const PlaybackState& getPlaybackState() const;
    const RecordingMetadata& getMetadata() const;
    
private:
    ActivityVisualizer& visualizer_;
    PlaybackState playbackState_;
    RecordingMetadata metadata_;
    std::vector<RecordedSpike> recording_;
};
```

### File Format

Binary format for efficient storage:
```
Header:
- Magic number (4 bytes): "SNNR"
- Version (4 bytes): 1
- Metadata length (4 bytes)
- Metadata (JSON string)

Spike Data:
- Spike count (8 bytes)
- For each spike:
  - Timestamp (8 bytes)
  - Source neuron ID (8 bytes)
  - Target neuron ID (8 bytes)
  - Synapse ID (8 bytes)
```

---

## Component 4: PatternDetector

### Purpose
Detect and visualize temporal patterns in spike activity.

### Data Structures

```cpp
struct SynchronyPattern {
    std::vector<uint64_t> neuronIds;  // Neurons firing synchronously
    uint64_t timestamp;                // When pattern occurred
    float synchronyScore;              // 0-1, how synchronous
};

struct OscillationPattern {
    std::vector<uint64_t> neuronIds;  // Neurons oscillating
    float frequency;                   // Oscillation frequency (Hz)
    float amplitude;                   // Oscillation amplitude
    uint64_t startTime;
    uint64_t endTime;
};

struct PropagationWave {
    std::vector<uint64_t> neuronIds;  // Neurons in wave order
    std::vector<uint64_t> timestamps; // Spike times
    float velocity;                    // Wave velocity (units/ms)
    glm::vec3 direction;              // Wave direction
};

struct PatternDetectionConfig {
    // Synchrony detection
    bool detectSynchrony;
    float synchronyWindow;      // Time window for synchrony (ms)
    int minSynchronousNeurons;  // Minimum neurons for pattern
    
    // Oscillation detection
    bool detectOscillations;
    float minFrequency;         // Minimum frequency (Hz)
    float maxFrequency;         // Maximum frequency (Hz)
    
    // Wave detection
    bool detectWaves;
    float waveTimeWindow;       // Time window for wave detection
    int minWaveNeurons;         // Minimum neurons in wave
};
```

### Class Interface

```cpp
class PatternDetector {
public:
    PatternDetector();
    ~PatternDetector();
    
    // Update with new spike data
    void update(const std::vector<RecordedSpike>& spikes,
                uint64_t currentTime);
    
    // Get detected patterns
    const std::vector<SynchronyPattern>& getSynchronyPatterns() const;
    const std::vector<OscillationPattern>& getOscillationPatterns() const;
    const std::vector<PropagationWave>& getPropagationWaves() const;
    
    // Configuration
    void setConfig(const PatternDetectionConfig& config);
    const PatternDetectionConfig& getConfig() const;
    
    // Clear history
    void clearPatterns();
    
private:
    PatternDetectionConfig config_;
    std::vector<SynchronyPattern> synchronyPatterns_;
    std::vector<OscillationPattern> oscillationPatterns_;
    std::vector<PropagationWave> propagationWaves_;
    
    // Detection algorithms
    void detectSynchrony(const std::vector<RecordedSpike>& spikes);
    void detectOscillations(const std::vector<RecordedSpike>& spikes);
    void detectWaves(const std::vector<RecordedSpike>& spikes);
};
```

---

## Component 5: InteractionManager

### Purpose
Handle user interaction with the visualization (selection, highlighting, filtering).

### Data Structures

```cpp
struct SelectionState {
    std::vector<uint64_t> selectedNeurons;
    std::vector<uint64_t> selectedSynapses;
    std::vector<uint64_t> highlightedNeurons;
    std::vector<uint64_t> highlightedSynapses;
};

struct FilterConfig {
    bool filterByType;
    bool showExcitatory;
    bool showInhibitory;
    
    bool filterByActivity;
    float minActivity;
    float maxActivity;
    
    bool filterByConnectivity;
    int minConnections;
    int maxConnections;
};

struct CameraBookmark {
    std::string name;
    glm::vec3 position;
    glm::vec3 target;
    float distance;
};
```

### Class Interface

```cpp
class InteractionManager {
public:
    InteractionManager(Camera& camera, NetworkDataAdapter& adapter);
    ~InteractionManager();
    
    // Selection
    void selectNeuron(uint64_t neuronId);
    void deselectNeuron(uint64_t neuronId);
    void clearSelection();
    void selectConnectedNeurons(uint64_t neuronId, int depth);
    
    // Highlighting
    void highlightNeuron(uint64_t neuronId);
    void highlightSynapse(uint64_t synapseId);
    void clearHighlights();
    
    // Filtering
    void setFilter(const FilterConfig& filter);
    void applyFilter();
    
    // Camera bookmarks
    void saveBookmark(const std::string& name);
    void loadBookmark(const std::string& name);
    const std::vector<CameraBookmark>& getBookmarks() const;
    
    // Mouse picking
    uint64_t pickNeuron(int screenX, int screenY, int screenWidth, int screenHeight);
    
    // State
    const SelectionState& getSelectionState() const;
    
private:
    Camera& camera_;
    NetworkDataAdapter& adapter_;
    SelectionState selectionState_;
    FilterConfig filterConfig_;
    std::vector<CameraBookmark> bookmarks_;
};
```

---

## Implementation Plan

### Priority 1: Recording and Playback (Essential)
1. Implement RecordingManager
2. Add playback controls to UI
3. Test save/load functionality

### Priority 2: Raster Plot (High Value)
1. Implement RasterPlotRenderer
2. Add raster plot window to demo
3. Integrate with ActivityVisualizer

### Priority 3: Interaction (User Experience)
1. Implement InteractionManager
2. Add mouse picking
3. Add selection highlighting
4. Add camera bookmarks

### Priority 4: Pattern Detection (Advanced)
1. Implement PatternDetector
2. Add synchrony detection
3. Add oscillation detection
4. Visualize detected patterns

### Priority 5: Histogram (Nice to Have)
1. Implement ActivityHistogram
2. Add histogram window to demo
3. Show frequency distribution

---

## Estimated Effort

- **RecordingManager:** ~400 lines
- **RasterPlotRenderer:** ~500 lines
- **InteractionManager:** ~600 lines
- **PatternDetector:** ~700 lines
- **ActivityHistogram:** ~300 lines
- **Demo Integration:** ~200 lines
- **Total:** ~2,700 lines

---

## Success Criteria

- ✅ Can record and save spike activity to file
- ✅ Can load and replay recordings with speed control
- ✅ Raster plot displays neuron firing patterns
- ✅ Can select neurons and highlight connections
- ✅ Pattern detector identifies synchronous firing
- ✅ All features integrated into demo application
- ✅ Performance: 60 FPS with recording/playback active

---

## Next Steps

1. Start with RecordingManager (most useful feature)
2. Implement RasterPlotRenderer (high visual impact)
3. Add InteractionManager (improves usability)
4. Implement PatternDetector (advanced analysis)
5. Add ActivityHistogram (final polish)

