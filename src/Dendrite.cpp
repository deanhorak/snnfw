#include "snnfw/Dendrite.h"
#include "snnfw/ActionPotential.h"
#include "snnfw/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

namespace snnfw {

void Dendrite::addSynapse(uint64_t synapseId) {
    // Check if synapse is already connected
    auto it = std::find(synapseIds.begin(), synapseIds.end(), synapseId);
    if (it == synapseIds.end()) {
        synapseIds.push_back(synapseId);
        SNNFW_DEBUG("Dendrite {}: Added synapse {} (total: {})", 
                    getId(), synapseId, synapseIds.size());
    } else {
        SNNFW_WARN("Dendrite {}: Synapse {} already connected", getId(), synapseId);
    }
}

bool Dendrite::removeSynapse(uint64_t synapseId) {
    auto it = std::find(synapseIds.begin(), synapseIds.end(), synapseId);
    if (it != synapseIds.end()) {
        synapseIds.erase(it);
        SNNFW_DEBUG("Dendrite {}: Removed synapse {} (remaining: {})", 
                    getId(), synapseId, synapseIds.size());
        return true;
    }
    SNNFW_WARN("Dendrite {}: Synapse {} not found for removal", getId(), synapseId);
    return false;
}

void Dendrite::receiveSpike(const std::shared_ptr<ActionPotential>& actionPotential) {
    if (!actionPotential) {
        SNNFW_ERROR("Dendrite {}: Received null action potential", getId());
        return;
    }

    SNNFW_DEBUG("Dendrite {} (Neuron {}): Received spike from synapse {} at time {:.3f}ms (amplitude: {:.3f})",
                getId(), 
                targetNeuronId,
                actionPotential->getSynapseId(),
                actionPotential->getScheduledTime(),
                actionPotential->getAmplitude());

    // Here we would typically:
    // 1. Integrate the spike into the neuron's membrane potential
    // 2. Update synaptic efficacy (STDP, etc.)
    // 3. Check if neuron should fire
    //
    // For now, we just log the reception. This will be extended when
    // we integrate with the Neuron class's spike processing.
}

std::string Dendrite::toJson() const {
    json j;
    j["type"] = "Dendrite";
    j["id"] = getId();
    j["targetNeuronId"] = targetNeuronId;
    j["synapseIds"] = synapseIds;
    return j.dump();
}

bool Dendrite::fromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        if (j["type"] != "Dendrite") {
            SNNFW_ERROR("Invalid type in JSON: expected 'Dendrite', got '{}'", j["type"].get<std::string>());
            return false;
        }

        // Deserialize ID from base class
        setId(j["id"]);

        targetNeuronId = j["targetNeuronId"];
        synapseIds = j["synapseIds"].get<std::vector<uint64_t>>();

        return true;
    } catch (const std::exception& e) {
        SNNFW_ERROR("Failed to deserialize Dendrite from JSON: {}", e.what());
        return false;
    }
}

} // namespace snnfw

