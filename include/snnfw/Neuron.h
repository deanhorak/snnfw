#ifndef SNNFW_NEURON_H
#define SNNFW_NEURON_H

#include "snnfw/NeuralObject.h"
#include <vector>
#include <cstddef>

namespace snnfw {

/**
 * @brief Neuron class for spiking neural network with temporal pattern learning
 *
 * This class implements a biologically-inspired neuron that learns temporal spike patterns
 * rather than using traditional weight-based learning. Key features:
 *
 * Pattern Learning:
 * - Stores temporal spike patterns (sequences of spike times)
 * - Up to maxReferencePatterns patterns per neuron (default: 20, MNIST uses 100)
 * - When capacity is reached, new patterns are blended into most similar existing pattern
 * - Uses cosine similarity for pattern matching
 *
 * Spike Processing:
 * - Rolling time window maintains recent spikes
 * - Spikes outside the window are automatically removed
 * - Pattern learning via learnCurrentPattern() stores current spike window
 *
 * Similarity Metrics:
 * - Cosine similarity for vector-based pattern matching
 * - Victor-Purpura spike distance for temporal pattern comparison
 * - Histogram-based fuzzy matching for spike train comparison
 *
 * Connectivity:
 * - One axon (output terminal) - stored as axonId
 * - Multiple dendrites (input terminals) - stored as dendriteIds
 *
 * Usage in MNIST Experiments:
 * - 392 neurons (49 regions × 8 orientations)
 * - Each neuron learns edge patterns at specific orientation
 * - Activation vectors used for k-NN classification
 * - Achieves 81.20% accuracy on MNIST digit recognition
 */
class Neuron : public NeuralObject {
public:
    /**
     * @brief Constructor with parameters
     * @param windowSizeMs Size of the rolling time window in milliseconds
     * @param similarityThreshold Threshold for pattern similarity (0.0 to 1.0)
     * @param maxReferencePatterns Maximum number of reference patterns to store
     * @param neuronId Unique identifier for this neuron (default: 0)
     */
    Neuron(double windowSizeMs, double similarityThreshold, size_t maxReferencePatterns = 20, uint64_t neuronId = 0);

    /**
     * @brief Insert a spike with a given timestamp
     * @param spikeTime Timestamp of the spike
     */
    void insertSpike(double spikeTime);

    /**
     * @brief Learn the current spike pattern (either add or blend)
     */
    void learnCurrentPattern();

    /**
     * @brief Print current rolling window of spikes
     */
    void printSpikes() const;

    /**
     * @brief Print all reference patterns
     */
    void printReferencePatterns() const;

    /**
     * @brief Check if current pattern matches any learned pattern
     * @return true if neuron should fire, false otherwise
     */
    bool checkShouldFire() const { return shouldFire(); }

    /**
     * @brief Get the best similarity score between current spikes and learned patterns
     * @return Best similarity score (0.0 to 1.0), or -1.0 if no patterns learned
     */
    double getBestSimilarity() const;

    /**
     * @brief Get the number of learned patterns
     * @return Number of reference patterns stored
     */
    size_t getLearnedPatternCount() const { return referencePatterns.size(); }

    /**
     * @brief Get all learned patterns
     * @return Const reference to vector of learned patterns
     */
    const std::vector<std::vector<double>>& getLearnedPatterns() const { return referencePatterns; }

    /**
     * @brief Get all spikes from the rolling window
     * @return Vector of spike times
     */
    std::vector<double> getSpikes() const { return spikes; }

    /**
     * @brief Clear all spikes from the rolling window
     */
    void clearSpikes() { spikes.clear(); }

    /**
     * @brief Set the axon ID for this neuron
     * @param id ID of the axon
     */
    void setAxonId(uint64_t id) { axonId = id; }

    /**
     * @brief Get the axon ID for this neuron
     * @return ID of the axon (0 if not set)
     */
    uint64_t getAxonId() const { return axonId; }

    /**
     * @brief Add a dendrite to this neuron
     * @param dendriteId ID of the dendrite to add
     */
    void addDendrite(uint64_t dendriteId);

    /**
     * @brief Remove a dendrite from this neuron
     * @param dendriteId ID of the dendrite to remove
     * @return true if dendrite was removed, false if not found
     */
    bool removeDendrite(uint64_t dendriteId);

    /**
     * @brief Get all reference patterns learned by this neuron
     * @return Const reference to vector of reference patterns
     */
    const std::vector<std::vector<double>>& getReferencePatterns() const { return referencePatterns; }

    /**
     * @brief Get all dendrite IDs for this neuron
     * @return Const reference to vector of dendrite IDs
     */
    const std::vector<uint64_t>& getDendriteIds() const { return dendriteIds; }

    /**
     * @brief Get the number of dendrites
     * @return Number of dendrites
     */
    size_t getDendriteCount() const { return dendriteIds.size(); }

    // Serializable interface implementation
    std::string toJson() const override;
    bool fromJson(const std::string& json) override;
    std::string getTypeName() const override { return "Neuron"; }

private:
    std::vector<double> spikes;                          ///< Rolling spike window
    std::vector<std::vector<double>> referencePatterns;  ///< Learned reference patterns
    double windowSize;                                   ///< Size of rolling window in ms
    double threshold;                                    ///< Similarity threshold for firing
    size_t maxPatterns;                                  ///< Maximum number of reference patterns

    uint64_t axonId;                                     ///< ID of the axon for this neuron (0 if not set)
    std::vector<uint64_t> dendriteIds;                   ///< IDs of dendrites connected to this neuron

    /**
     * @brief Remove spikes outside the rolling window
     * @param currentTime Current timestamp
     */
    void removeOldSpikes(double currentTime);

    /**
     * @brief Check if current pattern is similar to any reference pattern
     * @return true if neuron should fire, false otherwise
     */
    bool shouldFire() const;

    /**
     * @brief Find the most similar reference pattern
     * @param newPattern Pattern to compare against
     * @return Index of most similar pattern, or -1 if none found
     */
    int findMostSimilarPattern(const std::vector<double>& newPattern) const;

    /**
     * @brief Blend the new pattern into the target reference pattern
     * @param target Target pattern to blend into
     * @param newPattern New pattern to blend
     * @param alpha Blending factor (0.0 to 1.0)
     */
    void blendPattern(std::vector<double>& target, const std::vector<double>& newPattern, double alpha = 0.2);

    /**
     * @brief Calculate cosine similarity between two equal-length vectors
     * @param a First vector
     * @param b Second vector
     * @return Cosine similarity value (0.0 to 1.0)
     */
    static double cosineSimilarity(const std::vector<double>& a, const std::vector<double>& b);

    /**
     * @brief Compute spike distance between two spike trains
     * @param spikes1 First spike train
     * @param spikes2 Second spike train
     * @return Distance (lower is more similar)
     */
    double spikeDistance(const std::vector<double>& spikes1, const std::vector<double>& spikes2) const;

    /**
     * @brief Convert spike pattern to temporal histogram for fuzzy matching
     * @param pattern Spike times to convert
     * @return Histogram representation
     */
    std::vector<double> spikeToHistogram(const std::vector<double>& pattern) const;

    /**
     * @brief Compute similarity between two histograms
     * @param hist1 First histogram
     * @param hist2 Second histogram
     * @return Similarity score (0.0 to 1.0)
     */
    double histogramSimilarity(const std::vector<double>& hist1, const std::vector<double>& hist2) const;
};

} // namespace snnfw

#endif // SNNFW_NEURON_H
