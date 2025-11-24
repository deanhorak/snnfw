#include "snnfw/NetworkDataAdapter.h"
#include "snnfw/Datastore.h"
#include "snnfw/NetworkInspector.h"
#include "snnfw/ActivityMonitor.h"
#include "snnfw/Neuron.h"
#include "snnfw/Synapse.h"
#include "snnfw/Cluster.h"
#include "snnfw/Layer.h"
#include "snnfw/Column.h"
#include "snnfw/Nucleus.h"
#include "snnfw/Region.h"
#include "snnfw/Lobe.h"
#include "snnfw/Hemisphere.h"
#include "snnfw/Brain.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace snnfw {

// NetworkDataAdapter implementation
NetworkDataAdapter::NetworkDataAdapter(Datastore& datastore, 
                                     NetworkInspector& inspector,
                                     ActivityMonitor* activityMonitor)
    : datastore_(datastore)
    , inspector_(inspector)
    , activityMonitor_(activityMonitor) {
    
    // Default color scheme: blue for excitatory, red for inhibitory
    excitatoryColor_[0] = 0.2f;
    excitatoryColor_[1] = 0.5f;
    excitatoryColor_[2] = 1.0f;
    
    inhibitoryColor_[0] = 1.0f;
    inhibitoryColor_[1] = 0.2f;
    inhibitoryColor_[2] = 0.2f;
    
    // Default activity colors: dark blue for low, bright yellow for high
    lowActivityColor_[0] = 0.1f;
    lowActivityColor_[1] = 0.1f;
    lowActivityColor_[2] = 0.3f;
    
    highActivityColor_[0] = 1.0f;
    highActivityColor_[1] = 1.0f;
    highActivityColor_[2] = 0.2f;
}

bool NetworkDataAdapter::extractNetwork(uint64_t brainId) {
    clearCache();
    
    // Extract from brain level
    return extractHierarchy(brainId, "Brain");
}

bool NetworkDataAdapter::extractHierarchy(uint64_t rootId, const std::string& typeName) {
    clearCache();
    
    // Extract neurons from this hierarchy
    extractNeurons(rootId, typeName);
    
    // Extract synapses connecting these neurons
    extractSynapses();
    
    // Build hierarchical groups
    buildHierarchicalGroups(rootId, typeName);
    
    return neurons_.size() > 0;
}

void NetworkDataAdapter::extractNeurons(uint64_t rootId, const std::string& typeName) {
    // Get hierarchy statistics
    auto stats = inspector_.inspectHierarchy(rootId, typeName, datastore_);

    // Recursively collect all neuron IDs
    std::vector<uint64_t> neuronIds;
    std::function<void(uint64_t, const std::string&)> collectNeurons;

    collectNeurons = [&](uint64_t id, const std::string& type) {
        if (type == "Cluster") {
            // Get cluster and extract neuron IDs
            auto cluster = datastore_.getCluster(id);
            if (cluster) {
                auto clusterNeuronIds = cluster->getNeuronIds();
                neuronIds.insert(neuronIds.end(), clusterNeuronIds.begin(), clusterNeuronIds.end());
            }
        } else {
            // Recurse to children
            auto childStats = inspector_.inspectHierarchy(id, type, datastore_);
            for (uint64_t childId : childStats.childIds) {
                std::string childType = getChildType(type);
                collectNeurons(childId, childType);
            }
        }
    };

    collectNeurons(rootId, typeName);
    
    // Create NeuronVisualData for each neuron
    for (uint64_t neuronId : neuronIds) {
        auto neuron = datastore_.getNeuron(neuronId);
        if (!neuron) continue;

        NeuronVisualData visualData;
        visualData.id = neuronId;
        visualData.radius = 0.5f;  // Default radius, will be updated by layout
        visualData.activity = 0.0f;
        visualData.isExcitatory = true;  // Default, could be determined from neuron type

        // Use stored position if available
        if (neuron->hasPosition()) {
            visualData.position = neuron->getPosition();
        }
        // Otherwise position remains at (0,0,0) and will be set by LayoutEngine

        // Compute hierarchical context
        computeHierarchicalContext(visualData);

        // Apply color
        applyNeuronColor(visualData);

        // Add to collection
        neuronIndexMap_[neuronId] = neurons_.size();
        neurons_.push_back(visualData);
    }
}

void NetworkDataAdapter::extractSynapses() {
    // For each neuron, get its outgoing synapses
    for (const auto& neuron : neurons_) {
        auto neuronObj = datastore_.getNeuron(neuron.id);
        if (!neuronObj) continue;

        uint64_t axonId = neuronObj->getAxonId();
        auto axon = datastore_.getAxon(axonId);
        if (!axon) continue;

        auto synapseIds = axon->getSynapseIds();
        for (uint64_t synapseId : synapseIds) {
            auto synapse = datastore_.getSynapse(synapseId);
            if (!synapse) continue;

            uint64_t dendriteId = synapse->getDendriteId();
            auto dendrite = datastore_.getDendrite(dendriteId);
            if (!dendrite) continue;

            uint64_t targetNeuronId = dendrite->getTargetNeuronId();

            // Check if target neuron is in our visualization
            if (neuronIndexMap_.find(targetNeuronId) == neuronIndexMap_.end()) {
                continue;  // Target not in our set
            }

            SynapseVisualData visualData;
            visualData.id = synapseId;
            visualData.sourceNeuronId = neuron.id;
            visualData.targetNeuronId = targetNeuronId;
            visualData.weight = synapse->getWeight();
            visualData.thickness = 0.1f + std::abs(visualData.weight) * 0.5f;
            visualData.activity = 0.0f;

            // Set positions (will be updated by layout engine)
            visualData.sourcePos = neuron.position;

            // Find target neuron position
            auto targetIt = neuronIndexMap_.find(targetNeuronId);
            if (targetIt != neuronIndexMap_.end()) {
                visualData.targetPos = neurons_[targetIt->second].position;
            }

            // Apply color
            applySynapseColor(visualData);

            // Add to collection
            synapseIndexMap_[synapseId] = synapses_.size();
            synapses_.push_back(visualData);
        }
    }
}

void NetworkDataAdapter::buildHierarchicalGroups(uint64_t rootId, const std::string& typeName) {
    // Build groups recursively
    std::function<void(uint64_t, const std::string&, uint64_t)> buildGroups;
    
    buildGroups = [&](uint64_t id, const std::string& type, uint64_t parentId) {
        auto stats = inspector_.inspectHierarchy(id, type, datastore_);

        HierarchicalGroup group;
        group.id = id;
        group.typeName = type;
        group.name = stats.name;
        group.parentGroupId = parentId;
        
        // Collect neurons in this group
        if (type == "Cluster") {
            auto cluster = datastore_.getCluster(id);
            if (cluster) {
                auto neuronIds = cluster->getNeuronIds();
                for (uint64_t nid : neuronIds) {
                    if (neuronIndexMap_.find(nid) != neuronIndexMap_.end()) {
                        group.neuronIds.push_back(nid);
                    }
                }
            }
        }
        
        // Recurse to children
        for (uint64_t childId : stats.childIds) {
            std::string childType = getChildType(type);
            group.childGroupIds.push_back(childId);
            buildGroups(childId, childType, id);
        }
        
        groups_.push_back(group);
    };
    
    buildGroups(rootId, typeName, 0);
}

void NetworkDataAdapter::computeHierarchicalContext(NeuronVisualData& neuron) {
    // Find the cluster containing this neuron
    for (const auto& group : groups_) {
        if (group.typeName == "Cluster") {
            auto it = std::find(group.neuronIds.begin(), group.neuronIds.end(), neuron.id);
            if (it != group.neuronIds.end()) {
                neuron.clusterId = group.id;
                
                // Find parent layer
                for (const auto& layerGroup : groups_) {
                    if (layerGroup.typeName == "Layer") {
                        auto cit = std::find(layerGroup.childGroupIds.begin(), 
                                           layerGroup.childGroupIds.end(), 
                                           group.id);
                        if (cit != layerGroup.childGroupIds.end()) {
                            neuron.layerId = layerGroup.id;
                            // Continue up the hierarchy...
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
}

void NetworkDataAdapter::updateActivity(double startTime, double endTime) {
    if (!activityMonitor_) return;
    
    // Get activity snapshot
    auto snapshot = activityMonitor_->getActivitySnapshot(startTime, endTime);
    
    // Update neuron activity
    for (auto& neuron : neurons_) {
        updateNeuronActivity(neuron, startTime, endTime);
    }
    
    // Update synapse activity
    for (auto& synapse : synapses_) {
        updateSynapseActivity(synapse, startTime, endTime);
    }
}

void NetworkDataAdapter::updateNeuronActivity(NeuronVisualData& neuron, double startTime, double endTime) {
    if (!activityMonitor_) return;
    
    auto snapshot = activityMonitor_->getActivitySnapshot(startTime, endTime);
    
    // Get spike count for this neuron
    auto it = snapshot.neuronSpikeCount.find(neuron.id);
    if (it != snapshot.neuronSpikeCount.end()) {
        // Normalize activity to 0-1 range (assuming max 100 spikes in window)
        neuron.activity = std::min(1.0f, static_cast<float>(it->second) / 100.0f);
    } else {
        neuron.activity = 0.0f;
    }
    
    // Update color based on activity
    applyNeuronColor(neuron);
}

void NetworkDataAdapter::updateSynapseActivity(SynapseVisualData& synapse, double startTime, double endTime) {
    if (!activityMonitor_) return;
    
    // Get spike events
    auto events = activityMonitor_->getSpikeEvents(startTime, endTime);
    
    // Count spikes through this synapse
    size_t spikeCount = 0;
    for (const auto& event : events) {
        if (event.synapseId == synapse.id) {
            spikeCount++;
        }
    }
    
    // Normalize activity
    synapse.activity = std::min(1.0f, static_cast<float>(spikeCount) / 10.0f);
    
    // Update color
    applySynapseColor(synapse);
}

void NetworkDataAdapter::applyNeuronColor(NeuronVisualData& neuron) {
    // Base color from neuron type
    const float* baseColor = neuron.isExcitatory ? excitatoryColor_ : inhibitoryColor_;
    
    // Mix with activity color
    float activityMix = neuron.activity;
    
    neuron.r = baseColor[0] * (1.0f - activityMix) + highActivityColor_[0] * activityMix;
    neuron.g = baseColor[1] * (1.0f - activityMix) + highActivityColor_[1] * activityMix;
    neuron.b = baseColor[2] * (1.0f - activityMix) + highActivityColor_[2] * activityMix;
    neuron.a = 1.0f;
}

void NetworkDataAdapter::applySynapseColor(SynapseVisualData& synapse) {
    // Color based on weight (positive = green, negative = red)
    if (synapse.weight > 0) {
        synapse.r = 0.2f;
        synapse.g = 0.8f;
        synapse.b = 0.2f;
    } else {
        synapse.r = 0.8f;
        synapse.g = 0.2f;
        synapse.b = 0.2f;
    }
    
    // Mix with activity
    float activityMix = synapse.activity;
    synapse.r = synapse.r * (1.0f - activityMix) + 1.0f * activityMix;
    synapse.g = synapse.g * (1.0f - activityMix) + 1.0f * activityMix;
    synapse.b = synapse.b * (1.0f - activityMix) + 0.2f * activityMix;
    
    // Alpha based on weight magnitude
    synapse.a = 0.3f + std::min(0.7f, std::abs(synapse.weight) * 0.5f);
}

std::vector<NeuronVisualData> NetworkDataAdapter::getNeuronsByLevel(
    const std::string& levelType, uint64_t levelId) const {
    
    std::vector<NeuronVisualData> result;
    
    for (const auto& neuron : neurons_) {
        bool matches = false;
        
        if (levelType == "Cluster" && neuron.clusterId == levelId) matches = true;
        else if (levelType == "Layer" && neuron.layerId == levelId) matches = true;
        else if (levelType == "Column" && neuron.columnId == levelId) matches = true;
        
        if (matches) {
            result.push_back(neuron);
        }
    }
    
    return result;
}

std::vector<SynapseVisualData> NetworkDataAdapter::getSynapsesByNeuron(
    uint64_t neuronId, bool includeIncoming, bool includeOutgoing) const {
    
    std::vector<SynapseVisualData> result;
    
    for (const auto& synapse : synapses_) {
        if (includeOutgoing && synapse.sourceNeuronId == neuronId) {
            result.push_back(synapse);
        }
        if (includeIncoming && synapse.targetNeuronId == neuronId) {
            result.push_back(synapse);
        }
    }
    
    return result;
}

std::vector<LevelStatistics> NetworkDataAdapter::getLevelStatistics() const {
    // TODO: Implement level statistics computation
    return std::vector<LevelStatistics>();
}

void NetworkDataAdapter::setColorScheme(float excitatoryR, float excitatoryG, float excitatoryB,
                                       float inhibitoryR, float inhibitoryG, float inhibitoryB) {
    excitatoryColor_[0] = excitatoryR;
    excitatoryColor_[1] = excitatoryG;
    excitatoryColor_[2] = excitatoryB;
    
    inhibitoryColor_[0] = inhibitoryR;
    inhibitoryColor_[1] = inhibitoryG;
    inhibitoryColor_[2] = inhibitoryB;
}

void NetworkDataAdapter::setActivityColorMap(float lowActivityR, float lowActivityG, float lowActivityB,
                                            float highActivityR, float highActivityG, float highActivityB) {
    lowActivityColor_[0] = lowActivityR;
    lowActivityColor_[1] = lowActivityG;
    lowActivityColor_[2] = lowActivityB;
    
    highActivityColor_[0] = highActivityR;
    highActivityColor_[1] = highActivityG;
    highActivityColor_[2] = highActivityB;
}

void NetworkDataAdapter::updateSynapsePositions() {
    for (auto& synapse : synapses_) {
        // Find source neuron
        auto sourceIt = neuronIndexMap_.find(synapse.sourceNeuronId);
        if (sourceIt != neuronIndexMap_.end()) {
            synapse.sourcePos = neurons_[sourceIt->second].position;
        }

        // Find target neuron
        auto targetIt = neuronIndexMap_.find(synapse.targetNeuronId);
        if (targetIt != neuronIndexMap_.end()) {
            synapse.targetPos = neurons_[targetIt->second].position;
        }
    }
}

void NetworkDataAdapter::clearCache() {
    neurons_.clear();
    synapses_.clear();
    groups_.clear();
    neuronIndexMap_.clear();
    synapseIndexMap_.clear();
}

const NeuronVisualData* NetworkDataAdapter::findNeuron(uint64_t neuronId) const {
    auto it = neuronIndexMap_.find(neuronId);
    if (it != neuronIndexMap_.end()) {
        return &neurons_[it->second];
    }
    return nullptr;
}

const SynapseVisualData* NetworkDataAdapter::findSynapse(uint64_t synapseId) const {
    auto it = synapseIndexMap_.find(synapseId);
    if (it != synapseIndexMap_.end()) {
        return &synapses_[it->second];
    }
    return nullptr;
}

// Helper function to get child type in hierarchy
std::string NetworkDataAdapter::getChildType(const std::string& parentType) const {
    if (parentType == "Brain") return "Hemisphere";
    if (parentType == "Hemisphere") return "Lobe";
    if (parentType == "Lobe") return "Region";
    if (parentType == "Region") return "Nucleus";
    if (parentType == "Nucleus") return "Column";
    if (parentType == "Column") return "Layer";
    if (parentType == "Layer") return "Cluster";
    return "";
}

} // namespace snnfw

