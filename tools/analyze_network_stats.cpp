/**
 * @file analyze_network_stats.cpp
 * @brief Analyze network connectivity and spike generation statistics
 * 
 * This tool analyzes the MNIST hierarchical network to determine:
 * - Average and range of synapses per neuron (fan-out)
 * - Temporal separation of spikes (synaptic delays)
 * - Expected spike counts per neuron firing
 */

#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Neuron.h"
#include "snnfw/Axon.h"
#include "snnfw/Synapse.h"
#include "snnfw/Dendrite.h"
#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <numeric>
#include <iomanip>

struct NetworkStats {
    std::vector<int> fanOutCounts;  // Number of synapses per neuron
    std::vector<double> delays;      // Synaptic delays
    
    void addNeuronFanOut(int count) {
        fanOutCounts.push_back(count);
    }
    
    void addDelay(double delay) {
        delays.push_back(delay);
    }
    
    void print() const {
        std::cout << "\n=== Network Connectivity Statistics ===" << std::endl;
        
        // Fan-out statistics
        if (!fanOutCounts.empty()) {
            int total = std::accumulate(fanOutCounts.begin(), fanOutCounts.end(), 0);
            double avg = static_cast<double>(total) / fanOutCounts.size();
            int minFanOut = *std::min_element(fanOutCounts.begin(), fanOutCounts.end());
            int maxFanOut = *std::max_element(fanOutCounts.begin(), fanOutCounts.end());
            
            std::cout << "\nFan-Out (synapses per neuron):" << std::endl;
            std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg << std::endl;
            std::cout << "  Range: " << minFanOut << " - " << maxFanOut << std::endl;
            std::cout << "  Total neurons analyzed: " << fanOutCounts.size() << std::endl;
            std::cout << "  Total synapses: " << total << std::endl;
        }
        
        // Delay statistics
        if (!delays.empty()) {
            double totalDelay = std::accumulate(delays.begin(), delays.end(), 0.0);
            double avgDelay = totalDelay / delays.size();
            double minDelay = *std::min_element(delays.begin(), delays.end());
            double maxDelay = *std::max_element(delays.begin(), delays.end());
            
            std::cout << "\nSynaptic Delays:" << std::endl;
            std::cout << "  Average: " << std::fixed << std::setprecision(3) << avgDelay << " ms" << std::endl;
            std::cout << "  Range: " << minDelay << " - " << maxDelay << " ms" << std::endl;
            std::cout << "  Temporal spread: " << (maxDelay - minDelay) << " ms" << std::endl;
        }
        
        std::cout << "\n=== Spike Generation Analysis ===" << std::endl;
        if (!fanOutCounts.empty()) {
            int total = std::accumulate(fanOutCounts.begin(), fanOutCounts.end(), 0);
            double avg = static_cast<double>(total) / fanOutCounts.size();
            int minFanOut = *std::min_element(fanOutCounts.begin(), fanOutCounts.end());
            int maxFanOut = *std::max_element(fanOutCounts.begin(), fanOutCounts.end());
            
            std::cout << "\nWhen a single neuron fires:" << std::endl;
            std::cout << "  Average spikes generated: " << std::fixed << std::setprecision(2) << avg << std::endl;
            std::cout << "  Minimum spikes: " << minFanOut << std::endl;
            std::cout << "  Maximum spikes: " << maxFanOut << std::endl;
            
            if (!delays.empty()) {
                double minDelay = *std::min_element(delays.begin(), delays.end());
                double maxDelay = *std::max_element(delays.begin(), delays.end());
                std::cout << "\nTemporal distribution of generated spikes:" << std::endl;
                std::cout << "  All spikes arrive within: " << (maxDelay - minDelay) << " ms window" << std::endl;
                std::cout << "  Earliest arrival: +" << minDelay << " ms after firing" << std::endl;
                std::cout << "  Latest arrival: +" << maxDelay << " ms after firing" << std::endl;
            }
        }
    }
};

// Simulate the MNIST hierarchical network connectivity
NetworkStats analyzeHierarchicalNetwork() {
    NetworkStats stats;
    NeuralObjectFactory factory;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    std::cout << "Simulating MNIST Hierarchical Network..." << std::endl;
    
    // Layer 1: Retina neurons (3 clusters × 512 neurons = 1536)
    std::vector<std::shared_ptr<Neuron>> retinaNeurons;
    for (int i = 0; i < 1536; ++i) {
        auto neuron = factory.createNeuron(200.0, 0.7, 20);
        auto axon = factory.createAxon(neuron->getId());
        neuron->setAxonId(axon->getId());
        retinaNeurons.push_back(neuron);
    }
    
    // Layer 2: Interneurons (3 clusters × 128 neurons = 384)
    std::vector<std::shared_ptr<Neuron>> interneurons;
    for (int i = 0; i < 384; ++i) {
        auto neuron = factory.createNeuron(200.0, 0.7, 20);
        auto axon = factory.createAxon(neuron->getId());
        neuron->setAxonId(axon->getId());
        interneurons.push_back(neuron);
    }
    
    // Layer 3: V1 Hidden neurons (512)
    std::vector<std::shared_ptr<Neuron>> v1Neurons;
    for (int i = 0; i < 512; ++i) {
        auto neuron = factory.createNeuron(200.0, 0.7, 20);
        auto axon = factory.createAxon(neuron->getId());
        neuron->setAxonId(axon->getId());
        v1Neurons.push_back(neuron);
    }
    
    // Layer 4: Output neurons (10)
    std::vector<std::shared_ptr<Neuron>> outputNeurons;
    for (int i = 0; i < 10; ++i) {
        auto neuron = factory.createNeuron(200.0, 0.7, 20);
        auto axon = factory.createAxon(neuron->getId());
        neuron->setAxonId(axon->getId());
        outputNeurons.push_back(neuron);
    }
    
    std::cout << "Created neurons: 1536 retina + 384 interneurons + 512 V1 + 10 output" << std::endl;
    
    // Connections: Retina → Interneurons (50% connectivity, bidirectional)
    std::cout << "Creating retina → interneuron connections (50% sparse)..." << std::endl;
    int retinaToInterneuronSynapses = 0;
    for (size_t i = 0; i < retinaNeurons.size(); ++i) {
        int synapseCount = 0;
        for (size_t j = 0; j < interneurons.size(); ++j) {
            if (dis(gen) < 0.5) {
                stats.addDelay(1.0);  // 1ms delay
                synapseCount++;
                retinaToInterneuronSynapses++;
            }
        }
        stats.addNeuronFanOut(synapseCount);
    }
    
    // Connections: Interneurons → Retina (reverse, 50% connectivity)
    std::cout << "Creating interneuron → retina connections (50% sparse)..." << std::endl;
    for (size_t i = 0; i < interneurons.size(); ++i) {
        int synapseCount = 0;
        for (size_t j = 0; j < retinaNeurons.size(); ++j) {
            if (dis(gen) < 0.5) {
                stats.addDelay(1.0);
                synapseCount++;
            }
        }
        stats.addNeuronFanOut(synapseCount);
    }
    
    // Connections: All sources (retina + interneurons) → V1 (25% connectivity)
    std::cout << "Creating (retina + interneurons) → V1 connections (25% sparse)..." << std::endl;
    std::vector<std::shared_ptr<Neuron>> allSources;
    allSources.insert(allSources.end(), retinaNeurons.begin(), retinaNeurons.end());
    allSources.insert(allSources.end(), interneurons.begin(), interneurons.end());
    
    for (size_t i = 0; i < allSources.size(); ++i) {
        int synapseCount = 0;
        for (size_t j = 0; j < v1Neurons.size(); ++j) {
            if (dis(gen) < 0.25) {
                stats.addDelay(1.0);
                synapseCount++;
            }
        }
        stats.addNeuronFanOut(synapseCount);
    }
    
    // Connections: V1 → Output (50% connectivity)
    std::cout << "Creating V1 → output connections (50% sparse)..." << std::endl;
    for (size_t i = 0; i < v1Neurons.size(); ++i) {
        int synapseCount = 0;
        for (size_t j = 0; j < outputNeurons.size(); ++j) {
            if (dis(gen) < 0.5) {
                stats.addDelay(1.0);
                synapseCount++;
            }
        }
        stats.addNeuronFanOut(synapseCount);
    }
    
    std::cout << "Network simulation complete!" << std::endl;
    
    return stats;
}

int main() {
    std::cout << "=== MNIST Hierarchical Network Analysis ===" << std::endl;
    std::cout << "Analyzing spike generation and temporal patterns..." << std::endl;
    
    NetworkStats stats = analyzeHierarchicalNetwork();
    stats.print();
    
    std::cout << "\n=== Interpretation ===" << std::endl;
    std::cout << "When a neuron fires, it generates action potentials for each of its" << std::endl;
    std::cout << "outgoing synapses. These spikes are scheduled into the circular event" << std::endl;
    std::cout << "queue at future time slices based on synaptic delays." << std::endl;
    std::cout << "\nWith 1ms uniform delays, all spikes from a single neuron arrive at" << std::endl;
    std::cout << "their target dendrites within the same 1ms time slice." << std::endl;
    
    return 0;
}

