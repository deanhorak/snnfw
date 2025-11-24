#ifndef SNNFW_RECORDING_MANAGER_H
#define SNNFW_RECORDING_MANAGER_H

#include "snnfw/ActivityVisualizer.h"
#include <string>
#include <vector>
#include <cstdint>

namespace snnfw {

/**
 * @brief Metadata for a spike recording
 */
struct RecordingMetadata {
    std::string name;           ///< Recording name
    uint64_t startTime;         ///< Start time (ms)
    uint64_t endTime;           ///< End time (ms)
    uint64_t duration;          ///< Duration (ms)
    size_t spikeCount;          ///< Total number of spikes
    size_t neuronCount;         ///< Number of unique neurons
    std::string timestamp;      ///< ISO 8601 timestamp
};

/**
 * @brief Playback state
 */
struct PlaybackState {
    bool playing;               ///< Currently playing
    bool paused;                ///< Currently paused
    bool looping;               ///< Loop playback
    float speed;                ///< Playback speed multiplier (1.0 = normal)
    uint64_t currentTime;       ///< Current playback time (ms)
    uint64_t startTime;         ///< Playback start time (ms)
    uint64_t endTime;           ///< Playback end time (ms)
    
    PlaybackState() 
        : playing(false), paused(false), looping(false), speed(1.0f),
          currentTime(0), startTime(0), endTime(0) {}
};

/**
 * @brief Manages recording and playback of spike activity
 * 
 * RecordingManager allows you to:
 * - Record spike activity to memory
 * - Save recordings to binary files
 * - Load recordings from files
 * - Play back recordings with speed control
 * - Loop playback
 * 
 * Example usage:
 * ```cpp
 * RecordingManager recorder(activityVisualizer);
 * 
 * // Start recording
 * recorder.startRecording();
 * 
 * // ... simulation runs ...
 * 
 * // Stop and save
 * recorder.stopRecording();
 * recorder.saveRecording("my_recording.snnr");
 * 
 * // Later, load and play back
 * recorder.loadRecording("my_recording.snnr");
 * recorder.setSpeed(2.0f);  // 2x speed
 * recorder.setLooping(true);
 * recorder.play();
 * ```
 */
class RecordingManager {
public:
    /**
     * @brief Constructor
     * @param visualizer Activity visualizer to record from
     */
    explicit RecordingManager(ActivityVisualizer& visualizer);
    
    /**
     * @brief Destructor
     */
    ~RecordingManager();
    
    // Recording control
    
    /**
     * @brief Start recording spike activity
     */
    void startRecording();
    
    /**
     * @brief Stop recording
     */
    void stopRecording();
    
    /**
     * @brief Check if currently recording
     * @return true if recording
     */
    bool isRecording() const { return recording_; }
    
    /**
     * @brief Add a spike to the current recording
     * @param spike Spike event to record
     */
    void recordSpike(const RecordedSpike& spike);
    
    // Save/load
    
    /**
     * @brief Save recording to file
     * @param filename Path to save file
     * @return true if successful
     */
    bool saveRecording(const std::string& filename);
    
    /**
     * @brief Load recording from file
     * @param filename Path to recording file
     * @return true if successful
     */
    bool loadRecording(const std::string& filename);
    
    // Playback control
    
    /**
     * @brief Start playback
     */
    void play();
    
    /**
     * @brief Pause playback
     */
    void pause();
    
    /**
     * @brief Stop playback and reset to beginning
     */
    void stop();
    
    /**
     * @brief Set playback speed
     * @param speed Speed multiplier (1.0 = normal, 2.0 = 2x, 0.5 = half speed)
     */
    void setSpeed(float speed);
    
    /**
     * @brief Enable/disable looping
     * @param loop true to enable looping
     */
    void setLooping(bool loop);
    
    /**
     * @brief Seek to specific time in recording
     * @param time Time to seek to (ms)
     */
    void seek(uint64_t time);
    
    /**
     * @brief Update playback (call every frame)
     * @param deltaTime Time since last update (ms)
     */
    void update(uint64_t deltaTime);
    
    // State access
    
    /**
     * @brief Get playback state
     * @return Current playback state
     */
    const PlaybackState& getPlaybackState() const { return playbackState_; }
    
    /**
     * @brief Get recording metadata
     * @return Recording metadata
     */
    const RecordingMetadata& getMetadata() const { return metadata_; }
    
    /**
     * @brief Get all recorded spikes
     * @return Vector of recorded spikes
     */
    const std::vector<RecordedSpike>& getRecording() const { return spikes_; }
    
    /**
     * @brief Clear current recording
     */
    void clearRecording();
    
private:
    ActivityVisualizer& visualizer_;

    // Recording state
    bool recording_;
    uint64_t recordingStartTime_;
    std::vector<RecordedSpike> spikes_;
    RecordingMetadata metadata_;
    
    // Playback state
    PlaybackState playbackState_;
    size_t playbackIndex_;  // Current index in recording
    
    // Helper methods
    void updateMetadata();
    std::string getCurrentTimestamp();
};

} // namespace snnfw

#endif // SNNFW_RECORDING_MANAGER_H

