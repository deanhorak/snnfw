#include "snnfw/NetworkPropagator.h"
#include "snnfw/ActionPotential.h"
#include "snnfw/Logger.h"
#include <algorithm>

namespace snnfw {

NetworkPropagator::NetworkPropagator(std::shared_ptr<SpikeProcessor> spikeProcessor)
    : spikeProcessor_(spikeProcessor) {
    if (!spikeProcessor_) {
        SNNFW_ERROR("NetworkPropagator: SpikeProcessor cannot be null");
        throw std::invalid_argument("SpikeProcessor cannot be null");
    }
    SNNFW_INFO("NetworkPropagator: Initialized");
}

void NetworkPropagator::registerNeuron(const std::shared_ptr<Neuron>& neuron) {
    if (!neuron) {
        SNNFW_ERROR("NetworkPropagator: Cannot register null neuron");
        return;
    }

    std::lock_guard<std::mutex> lock(neuronMutex_);
    neuronRegistry_[neuron->getId()] = neuron;
    SNNFW_DEBUG("NetworkPropagator: Registered neuron {}", neuron->getId());
}

void NetworkPropagator::registerAxon(const std::shared_ptr<Axon>& axon) {
    if (!axon) {
        SNNFW_ERROR("NetworkPropagator: Cannot register null axon");
        return;
    }

    std::lock_guard<std::mutex> lock(axonMutex_);
    axonRegistry_[axon->getId()] = axon;
    SNNFW_DEBUG("NetworkPropagator: Registered axon {}", axon->getId());
}

void NetworkPropagator::registerSynapse(const std::shared_ptr<Synapse>& synapse) {
    if (!synapse) {
        SNNFW_ERROR("NetworkPropagator: Cannot register null synapse");
        return;
    }

    std::lock_guard<std::mutex> lock(synapseMutex_);
    synapseRegistry_[synapse->getId()] = synapse;
    SNNFW_DEBUG("NetworkPropagator: Registered synapse {}", synapse->getId());
}

void NetworkPropagator::registerDendrite(const std::shared_ptr<Dendrite>& dendrite) {
    if (!dendrite) {
        SNNFW_ERROR("NetworkPropagator: Cannot register null dendrite");
        return;
    }

    std::lock_guard<std::mutex> lock(dendriteMutex_);
    dendriteRegistry_[dendrite->getId()] = dendrite;
    
    // Also register with SpikeProcessor for delivery
    spikeProcessor_->registerDendrite(dendrite);
    
    SNNFW_DEBUG("NetworkPropagator: Registered dendrite {}", dendrite->getId());
}

int NetworkPropagator::fireNeuron(uint64_t neuronId, double firingTime) {
    // Get the neuron
    std::shared_ptr<Neuron> neuron;
    {
        std::lock_guard<std::mutex> lock(neuronMutex_);
        auto it = neuronRegistry_.find(neuronId);
        if (it == neuronRegistry_.end()) {
            SNNFW_WARN("NetworkPropagator: Neuron {} not found", neuronId);
            return -1;
        }
        neuron = it->second;
    }

    // Get the neuron's axon
    uint64_t axonId = neuron->getAxonId();
    if (axonId == 0) {
        SNNFW_DEBUG("NetworkPropagator: Neuron {} has no axon", neuronId);
        return 0;
    }

    std::shared_ptr<Axon> axon;
    {
        std::lock_guard<std::mutex> lock(axonMutex_);
        auto it = axonRegistry_.find(axonId);
        if (it == axonRegistry_.end()) {
            SNNFW_WARN("NetworkPropagator: Axon {} not found for neuron {}", axonId, neuronId);
            return -1;
        }
        axon = it->second;
    }

    // Get all synapses connected to this axon
    const auto& synapseIds = axon->getSynapseIds();
    if (synapseIds.empty()) {
        SNNFW_DEBUG("NetworkPropagator: Axon {} has no synapses", axonId);
        return 0;
    }

    int spikesScheduled = 0;

    // For each synapse, create and schedule an action potential
    for (uint64_t synapseId : synapseIds) {
        std::shared_ptr<Synapse> synapse;
        {
            std::lock_guard<std::mutex> lock(synapseMutex_);
            auto it = synapseRegistry_.find(synapseId);
            if (it == synapseRegistry_.end()) {
                SNNFW_WARN("NetworkPropagator: Synapse {} not found", synapseId);
                continue;
            }
            synapse = it->second;
        }

        // Create action potential with synaptic delay and weight
        double arrivalTime = firingTime + synapse->getDelay();
        double amplitude = synapse->getWeight();

        auto actionPotential = std::make_shared<ActionPotential>(
            synapseId,
            synapse->getDendriteId(),
            arrivalTime,
            amplitude
        );

        // Schedule for delivery
        if (spikeProcessor_->scheduleSpike(actionPotential)) {
            spikesScheduled++;
            SNNFW_TRACE("NetworkPropagator: Scheduled spike from neuron {} via synapse {} to dendrite {} at time {:.3f}ms",
                       neuronId, synapseId, synapse->getDendriteId(), arrivalTime);
        } else {
            SNNFW_WARN("NetworkPropagator: Failed to schedule spike from neuron {} via synapse {}",
                      neuronId, synapseId);
        }
    }

    SNNFW_DEBUG("NetworkPropagator: Neuron {} fired at {:.3f}ms, scheduled {} spikes",
               neuronId, firingTime, spikesScheduled);

    return spikesScheduled;
}

bool NetworkPropagator::deliverSpikeToNeuron(uint64_t neuronId, double spikeTime, double amplitude) {
    std::shared_ptr<Neuron> neuron;
    {
        std::lock_guard<std::mutex> lock(neuronMutex_);
        auto it = neuronRegistry_.find(neuronId);
        if (it == neuronRegistry_.end()) {
            SNNFW_WARN("NetworkPropagator: Cannot deliver spike to neuron {} - not found", neuronId);
            return false;
        }
        neuron = it->second;
    }

    // Insert spike into neuron's buffer
    // Note: amplitude could be used to modulate the spike, but for now we just insert the spike time
    neuron->insertSpike(spikeTime);
    
    SNNFW_TRACE("NetworkPropagator: Delivered spike to neuron {} at time {:.3f}ms (amplitude: {:.3f})",
               neuronId, spikeTime, amplitude);

    return true;
}

std::shared_ptr<Neuron> NetworkPropagator::getNeuron(uint64_t neuronId) const {
    std::lock_guard<std::mutex> lock(neuronMutex_);
    auto it = neuronRegistry_.find(neuronId);
    return (it != neuronRegistry_.end()) ? it->second : nullptr;
}

std::shared_ptr<Axon> NetworkPropagator::getAxon(uint64_t axonId) const {
    std::lock_guard<std::mutex> lock(axonMutex_);
    auto it = axonRegistry_.find(axonId);
    return (it != axonRegistry_.end()) ? it->second : nullptr;
}

std::shared_ptr<Synapse> NetworkPropagator::getSynapse(uint64_t synapseId) const {
    std::lock_guard<std::mutex> lock(synapseMutex_);
    auto it = synapseRegistry_.find(synapseId);
    return (it != synapseRegistry_.end()) ? it->second : nullptr;
}

std::shared_ptr<Dendrite> NetworkPropagator::getDendrite(uint64_t dendriteId) const {
    std::lock_guard<std::mutex> lock(dendriteMutex_);
    auto it = dendriteRegistry_.find(dendriteId);
    return (it != dendriteRegistry_.end()) ? it->second : nullptr;
}

std::vector<double> NetworkPropagator::computeLayerActivation(const std::vector<uint64_t>& neuronIds) const {
    std::vector<double> activations;
    activations.reserve(neuronIds.size());

    for (uint64_t neuronId : neuronIds) {
        std::shared_ptr<Neuron> neuron;
        {
            std::lock_guard<std::mutex> lock(neuronMutex_);
            auto it = neuronRegistry_.find(neuronId);
            if (it != neuronRegistry_.end()) {
                neuron = it->second;
            }
        }

        if (neuron) {
            // Get the best similarity between current spikes and learned patterns
            double activation = neuron->getBestSimilarity();
            activations.push_back(activation);
        } else {
            SNNFW_WARN("NetworkPropagator: Neuron {} not found when computing activation", neuronId);
            activations.push_back(0.0);
        }
    }

    return activations;
}

void NetworkPropagator::clearAllSpikes() {
    std::lock_guard<std::mutex> lock(neuronMutex_);
    for (auto& pair : neuronRegistry_) {
        pair.second->clearSpikes();
    }
    SNNFW_DEBUG("NetworkPropagator: Cleared all spike buffers");
}

size_t NetworkPropagator::getNeuronCount() const {
    std::lock_guard<std::mutex> lock(neuronMutex_);
    return neuronRegistry_.size();
}

size_t NetworkPropagator::getSynapseCount() const {
    std::lock_guard<std::mutex> lock(synapseMutex_);
    return synapseRegistry_.size();
}

} // namespace snnfw

