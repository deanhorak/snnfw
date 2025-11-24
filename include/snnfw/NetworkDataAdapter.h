#ifndef SNNFW_NETWORK_DATA_ADAPTER_H
#define SNNFW_NETWORK_DATA_ADAPTER_H

#include "snnfw/NeuralObject.h"  // For Position3D
#include <cstdint>
#include <vector>
#include <memory>
#include <map>
#include <string>

namespace snnfw {

// Forward declarations
class Datastore;
class NetworkInspector;
class ActivityMonitor;

/**
 * @brief Visual representation data for a neuron
 */
struct NeuronVisualData {
    uint64_t id;                    ///< Neuron ID
    Position3D position;            ///< 3D position for rendering
    float radius;                   ///< Visual radius (can represent activity)
    float r, g, b, a;              ///< Color (RGBA)
    float activity;                 ///< Current activity level (0.0 to 1.0)
    bool isExcitatory;             ///< True for excitatory, false for inhibitory
    
    // Hierarchical context
    uint64_t clusterId;
    uint64_t layerId;
    uint64_t columnId;
    uint64_t nucleusId;
    uint64_t regionId;
    uint64_t lobeId;
    uint64_t hemisphereId;
    uint64_t brainId;
    
    NeuronVisualData() : id(0), radius(1.0f), r(0.5f), g(0.5f), b(0.5f), a(1.0f),
                         activity(0.0f), isExcitatory(true),
                         clusterId(0), layerId(0), columnId(0), nucleusId(0),
                         regionId(0), lobeId(0), hemisphereId(0), brainId(0) {}
};

/**
 * @brief Visual representation data for a synapse
 */
struct SynapseVisualData {
    uint64_t id;                    ///< Synapse ID
    uint64_t sourceNeuronId;        ///< Source neuron ID
    uint64_t targetNeuronId;        ///< Target neuron ID
    Position3D sourcePos;           ///< Source position (cached for performance)
    Position3D targetPos;           ///< Target position (cached for performance)
    float weight;                   ///< Synaptic weight
    float thickness;                ///< Visual thickness (based on weight)
    float r, g, b, a;              ///< Color (RGBA)
    float activity;                 ///< Recent activity (0.0 to 1.0)
    
    SynapseVisualData() : id(0), sourceNeuronId(0), targetNeuronId(0),
                          weight(1.0f), thickness(1.0f),
                          r(0.5f), g(0.5f), b(0.5f), a(0.5f),
                          activity(0.0f) {}
};

/**
 * @brief Hierarchical group for organizing neurons visually
 */
struct HierarchicalGroup {
    uint64_t id;                    ///< Group ID (cluster, layer, column, etc.)
    std::string typeName;           ///< Type ("Cluster", "Layer", "Column", etc.)
    std::string name;               ///< Human-readable name
    Position3D centerPosition;      ///< Center of this group
    Position3D boundingBoxMin;      ///< Bounding box minimum
    Position3D boundingBoxMax;      ///< Bounding box maximum
    std::vector<uint64_t> neuronIds; ///< Neurons in this group
    std::vector<uint64_t> childGroupIds; ///< Child groups (for hierarchy)
    uint64_t parentGroupId;         ///< Parent group ID
    
    HierarchicalGroup() : id(0), parentGroupId(0) {}
};

/**
 * @brief Statistics for a hierarchical level
 */
struct LevelStatistics {
    std::string levelName;          ///< "Brain", "Hemisphere", "Lobe", etc.
    size_t totalCount;              ///< Total number of objects at this level
    size_t totalNeurons;            ///< Total neurons under this level
    size_t totalSynapses;           ///< Total synapses under this level
    float averageActivity;          ///< Average activity across all neurons
};

/**
 * @class NetworkDataAdapter
 * @brief Adapter to extract and prepare network data for visualization
 * 
 * The NetworkDataAdapter bridges the gap between SNNFW's data structures
 * (Datastore, NetworkInspector, ActivityMonitor) and the visualization system.
 * It extracts network structure, computes spatial layouts, and provides
 * visual representation data optimized for rendering.
 * 
 * Key Features:
 * - Extracts neuron and synapse data from Datastore
 * - Computes hierarchical spatial layouts
 * - Integrates activity data from ActivityMonitor
 * - Provides level-of-detail (LOD) support
 * - Caches visual data for performance
 * - Supports filtering and selection
 * 
 * Usage:
 * ```cpp
 * NetworkDataAdapter adapter(datastore, inspector, activityMonitor);
 * 
 * // Extract network structure
 * adapter.extractNetwork(brainId);
 * 
 * // Get neurons for rendering
 * auto neurons = adapter.getNeurons();
 * for (const auto& neuron : neurons) {
 *     renderer.drawSphere(neuron.position, neuron.radius, neuron.color);
 * }
 * 
 * // Get synapses for rendering
 * auto synapses = adapter.getSynapses();
 * for (const auto& synapse : synapses) {
 *     renderer.drawLine(synapse.sourcePos, synapse.targetPos, synapse.thickness);
 * }
 * ```
 */
class NetworkDataAdapter {
public:
    /**
     * @brief Constructor
     * @param datastore Reference to the datastore
     * @param inspector Reference to the network inspector
     * @param activityMonitor Optional reference to activity monitor (can be nullptr)
     */
    NetworkDataAdapter(Datastore& datastore, 
                      NetworkInspector& inspector,
                      ActivityMonitor* activityMonitor = nullptr);
    
    /**
     * @brief Extract network structure starting from a brain
     * @param brainId ID of the brain to visualize
     * @return True if successful, false otherwise
     */
    bool extractNetwork(uint64_t brainId);
    
    /**
     * @brief Extract a specific hierarchical level
     * @param rootId ID of the root object
     * @param typeName Type of the root ("Hemisphere", "Lobe", etc.)
     * @return True if successful, false otherwise
     */
    bool extractHierarchy(uint64_t rootId, const std::string& typeName);
    
    /**
     * @brief Update activity data from ActivityMonitor
     * @param startTime Start of time window (ms)
     * @param endTime End of time window (ms)
     */
    void updateActivity(double startTime, double endTime);
    
    /**
     * @brief Get all neurons for rendering
     * @return Vector of neuron visual data
     */
    const std::vector<NeuronVisualData>& getNeurons() const { return neurons_; }
    
    /**
     * @brief Get all synapses for rendering
     * @return Vector of synapse visual data
     */
    const std::vector<SynapseVisualData>& getSynapses() const { return synapses_; }
    
    /**
     * @brief Get hierarchical groups
     * @return Vector of hierarchical groups
     */
    const std::vector<HierarchicalGroup>& getGroups() const { return groups_; }
    
    /**
     * @brief Get neurons filtered by hierarchical level
     * @param levelType Type of level ("Cluster", "Layer", "Column", etc.)
     * @param levelId ID of the specific level
     * @return Vector of neuron visual data
     */
    std::vector<NeuronVisualData> getNeuronsByLevel(const std::string& levelType, uint64_t levelId) const;
    
    /**
     * @brief Get synapses connected to a specific neuron
     * @param neuronId ID of the neuron
     * @param includeIncoming Include incoming synapses
     * @param includeOutgoing Include outgoing synapses
     * @return Vector of synapse visual data
     */
    std::vector<SynapseVisualData> getSynapsesByNeuron(uint64_t neuronId, 
                                                        bool includeIncoming = true,
                                                        bool includeOutgoing = true) const;
    
    /**
     * @brief Get statistics for each hierarchical level
     * @return Vector of level statistics
     */
    std::vector<LevelStatistics> getLevelStatistics() const;
    
    /**
     * @brief Set color scheme for neuron types
     * @param excitatoryR Red component for excitatory neurons (0-1)
     * @param excitatoryG Green component for excitatory neurons (0-1)
     * @param excitatoryB Blue component for excitatory neurons (0-1)
     * @param inhibitoryR Red component for inhibitory neurons (0-1)
     * @param inhibitoryG Green component for inhibitory neurons (0-1)
     * @param inhibitoryB Blue component for inhibitory neurons (0-1)
     */
    void setColorScheme(float excitatoryR, float excitatoryG, float excitatoryB,
                       float inhibitoryR, float inhibitoryG, float inhibitoryB);
    
    /**
     * @brief Set activity color mapping
     * @param lowActivityR Red for low activity (0-1)
     * @param lowActivityG Green for low activity (0-1)
     * @param lowActivityB Blue for low activity (0-1)
     * @param highActivityR Red for high activity (0-1)
     * @param highActivityG Green for high activity (0-1)
     * @param highActivityB Blue for high activity (0-1)
     */
    void setActivityColorMap(float lowActivityR, float lowActivityG, float lowActivityB,
                            float highActivityR, float highActivityG, float highActivityB);
    
    /**
     * @brief Update synapse positions based on current neuron positions
     */
    void updateSynapsePositions();

    /**
     * @brief Clear all cached data
     */
    void clearCache();
    
    /**
     * @brief Get total number of neurons
     * @return Neuron count
     */
    size_t getNeuronCount() const { return neurons_.size(); }
    
    /**
     * @brief Get total number of synapses
     * @return Synapse count
     */
    size_t getSynapseCount() const { return synapses_.size(); }
    
    /**
     * @brief Find neuron by ID
     * @param neuronId ID to search for
     * @return Pointer to neuron visual data, or nullptr if not found
     */
    const NeuronVisualData* findNeuron(uint64_t neuronId) const;
    
    /**
     * @brief Find synapse by ID
     * @param synapseId ID to search for
     * @return Pointer to synapse visual data, or nullptr if not found
     */
    const SynapseVisualData* findSynapse(uint64_t synapseId) const;

private:
    // Helper methods for extraction
    void extractNeurons(uint64_t rootId, const std::string& typeName);
    void extractSynapses();
    void buildHierarchicalGroups(uint64_t rootId, const std::string& typeName);
    void computeHierarchicalContext(NeuronVisualData& neuron);
    
    // Helper methods for activity integration
    void updateNeuronActivity(NeuronVisualData& neuron, double startTime, double endTime);
    void updateSynapseActivity(SynapseVisualData& synapse, double startTime, double endTime);
    
    // Helper methods for color mapping
    void applyNeuronColor(NeuronVisualData& neuron);
    void applySynapseColor(SynapseVisualData& synapse);

    // Helper method for hierarchy traversal
    std::string getChildType(const std::string& parentType) const;

    // References to SNNFW components
    Datastore& datastore_;
    NetworkInspector& inspector_;
    ActivityMonitor* activityMonitor_;
    
    // Cached visual data
    std::vector<NeuronVisualData> neurons_;
    std::vector<SynapseVisualData> synapses_;
    std::vector<HierarchicalGroup> groups_;
    
    // Lookup maps for fast access
    std::map<uint64_t, size_t> neuronIndexMap_;  // neuronId -> index in neurons_
    std::map<uint64_t, size_t> synapseIndexMap_; // synapseId -> index in synapses_
    
    // Color scheme
    float excitatoryColor_[3];
    float inhibitoryColor_[3];
    float lowActivityColor_[3];
    float highActivityColor_[3];
};

} // namespace snnfw

#endif // SNNFW_NETWORK_DATA_ADAPTER_H

