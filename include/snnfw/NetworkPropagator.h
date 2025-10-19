#ifndef SNNFW_NETWORK_PROPAGATOR_H
#define SNNFW_NETWORK_PROPAGATOR_H

#include "snnfw/Neuron.h"
#include "snnfw/Axon.h"
#include "snnfw/Synapse.h"
#include "snnfw/Dendrite.h"
#include "snnfw/SpikeProcessor.h"
#include <memory>
#include <map>
#include <vector>
#include <mutex>

namespace snnfw {

/**
 * @brief NetworkPropagator manages forward propagation of spikes through a multi-layer network
 *
 * This class coordinates the propagation of action potentials through explicit synaptic
 * connections in a biologically plausible manner. It maintains registries of all neural
 * objects and provides methods for:
 * - Registering neurons, axons, synapses, and dendrites
 * - Propagating spikes from source neurons through their axons and synapses
 * - Delivering spikes to target neurons via dendrites
 * - Computing layer activations based on spike patterns
 *
 * Architecture:
 * - Maintains registries for all neural objects (neurons, axons, synapses, dendrites)
 * - Uses SpikeProcessor for temporal spike delivery with delays
 * - Supports layer-by-layer forward propagation
 * - Thread-safe for concurrent access
 *
 * Biological Motivation:
 * In biological neural networks, action potentials propagate from the soma through
 * the axon, across synapses (with delays and weight modulation), and into dendrites
 * of downstream neurons. This class simulates that process explicitly.
 */
class NetworkPropagator {
public:
    /**
     * @brief Constructor
     * @param spikeProcessor Shared pointer to the spike processor for temporal delivery
     */
    explicit NetworkPropagator(std::shared_ptr<SpikeProcessor> spikeProcessor);

    /**
     * @brief Register a neuron with the propagator
     * @param neuron Shared pointer to the neuron
     */
    void registerNeuron(const std::shared_ptr<Neuron>& neuron);

    /**
     * @brief Register an axon with the propagator
     * @param axon Shared pointer to the axon
     */
    void registerAxon(const std::shared_ptr<Axon>& axon);

    /**
     * @brief Register a synapse with the propagator
     * @param synapse Shared pointer to the synapse
     */
    void registerSynapse(const std::shared_ptr<Synapse>& synapse);

    /**
     * @brief Register a dendrite with the propagator
     * @param dendrite Shared pointer to the dendrite
     */
    void registerDendrite(const std::shared_ptr<Dendrite>& dendrite);

    /**
     * @brief Fire a neuron and propagate spikes through its axon
     * 
     * When a neuron fires, this method:
     * 1. Gets the neuron's axon
     * 2. For each synapse connected to the axon:
     *    - Creates an ActionPotential with appropriate delay and weight
     *    - Schedules it for delivery via SpikeProcessor
     *
     * @param neuronId ID of the neuron that is firing
     * @param firingTime Time when the neuron fires (in ms)
     * @return Number of spikes scheduled, or -1 if neuron/axon not found
     */
    int fireNeuron(uint64_t neuronId, double firingTime);

    /**
     * @brief Deliver a spike to a target neuron via dendrite
     * 
     * This is called by the dendrite when it receives an action potential.
     * It inserts the spike into the target neuron's spike buffer.
     *
     * @param neuronId ID of the target neuron
     * @param spikeTime Time of the spike (in ms)
     * @param amplitude Amplitude of the spike (modulated by synaptic weight)
     * @return true if delivered successfully, false if neuron not found
     */
    bool deliverSpikeToNeuron(uint64_t neuronId, double spikeTime, double amplitude);

    /**
     * @brief Get a neuron by ID
     * @param neuronId ID of the neuron
     * @return Shared pointer to the neuron, or nullptr if not found
     */
    std::shared_ptr<Neuron> getNeuron(uint64_t neuronId) const;

    /**
     * @brief Get an axon by ID
     * @param axonId ID of the axon
     * @return Shared pointer to the axon, or nullptr if not found
     */
    std::shared_ptr<Axon> getAxon(uint64_t axonId) const;

    /**
     * @brief Get a synapse by ID
     * @param synapseId ID of the synapse
     * @return Shared pointer to the synapse, or nullptr if not found
     */
    std::shared_ptr<Synapse> getSynapse(uint64_t synapseId) const;

    /**
     * @brief Get a dendrite by ID
     * @param dendriteId ID of the dendrite
     * @return Shared pointer to the dendrite, or nullptr if not found
     */
    std::shared_ptr<Dendrite> getDendrite(uint64_t dendriteId) const;

    /**
     * @brief Compute activation vector for a layer of neurons
     * 
     * For each neuron in the layer, computes the best similarity between
     * its current spike pattern and its learned reference patterns.
     *
     * @param neuronIds Vector of neuron IDs in the layer
     * @return Vector of activation values (one per neuron)
     */
    std::vector<double> computeLayerActivation(const std::vector<uint64_t>& neuronIds) const;

    /**
     * @brief Clear all spike buffers in registered neurons
     */
    void clearAllSpikes();

    /**
     * @brief Get the number of registered neurons
     * @return Number of neurons in the registry
     */
    size_t getNeuronCount() const;

    /**
     * @brief Get the number of registered synapses
     * @return Number of synapses in the registry
     */
    size_t getSynapseCount() const;

private:
    // Spike processor for temporal delivery
    std::shared_ptr<SpikeProcessor> spikeProcessor_;

    // Registries for neural objects
    std::map<uint64_t, std::shared_ptr<Neuron>> neuronRegistry_;
    std::map<uint64_t, std::shared_ptr<Axon>> axonRegistry_;
    std::map<uint64_t, std::shared_ptr<Synapse>> synapseRegistry_;
    std::map<uint64_t, std::shared_ptr<Dendrite>> dendriteRegistry_;

    // Mutexes for thread-safe access
    mutable std::mutex neuronMutex_;
    mutable std::mutex axonMutex_;
    mutable std::mutex synapseMutex_;
    mutable std::mutex dendriteMutex_;
};

} // namespace snnfw

#endif // SNNFW_NETWORK_PROPAGATOR_H

