/**
 * @file mnist_v1_hierarchical.cpp
 * @brief MNIST with Anatomically-Correct V1 Hierarchical Structure
 *
 * This experiment implements a biologically-inspired hierarchical visual processing
 * network based on the anatomical pathway from Occipital Lobe to V1 (Primary Visual Cortex).
 *
 * Hierarchical Structure:
 * Brain → Hemisphere → Occipital Lobe → V1 Region → Nucleus → Column → Layer 4C
 *
 * Architecture (Phase 2 - Multi-Cluster):
 * - Three input clusters with 512 neurons each (8×8 grid, 8 orientations)
 * - Each cluster receives different convolution of visual input:
 *   - Cluster 1: Sobel threshold=0.165 (baseline)
 *   - Cluster 2: Sobel threshold=0.10 (more sensitive, finer edges)
 *   - Cluster 3: Sobel threshold=0.25 (less sensitive, strong edges only)
 * - HybridStrategy learning
 * - MajorityVoting classification (k=5)
 * - Total: 1536 neurons (3 × 512)
 *
 * Usage:
 *   ./mnist_v1_hierarchical <config_file>
 *   ./mnist_v1_hierarchical ../configs/mnist_v1_hierarchical.json
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Brain.h"
#include "snnfw/Hemisphere.h"
#include "snnfw/Lobe.h"
#include "snnfw/Region.h"
#include "snnfw/Nucleus.h"
#include "snnfw/Column.h"
#include "snnfw/Layer.h"
#include "snnfw/Cluster.h"
#include "snnfw/adapters/RetinaAdapter.h"
#include "snnfw/classification/MajorityVoting.h"
#include "snnfw/learning/HybridStrategy.h"
#include "snnfw/SpikeProcessor.h"
#include "snnfw/NetworkPropagator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#ifdef _OPENMP
#include <omp.h>
#endif

using namespace snnfw;
using namespace snnfw::adapters;
using namespace snnfw::classification;
using namespace snnfw::learning;

// Configuration
struct V1Config {
    int trainPerDigit;
    int testImages;
    int kNeighbors;
    std::string trainImagesPath;
    std::string trainLabelsPath;
    std::string testImagesPath;
    std::string testLabelsPath;

    static V1Config fromConfigLoader(const ConfigLoader& config) {
        V1Config cfg;
        cfg.trainPerDigit = config.get<int>("/training/examples_per_digit", 5000);
        cfg.testImages = config.get<int>("/training/test_images", 10000);
        cfg.kNeighbors = config.get<int>("/classification/k_neighbors", 5);
        cfg.trainImagesPath = config.getRequired<std::string>("/data/train_images");
        cfg.trainLabelsPath = config.getRequired<std::string>("/data/train_labels");
        cfg.testImagesPath = config.getRequired<std::string>("/data/test_images");
        cfg.testLabelsPath = config.getRequired<std::string>("/data/test_labels");
        return cfg;
    }
};

// Cosine similarity
double cosineSimilarity(const std::vector<double>& a, const std::vector<double>& b) {
    double dot = 0.0, normA = 0.0, normB = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    if (normA == 0.0 || normB == 0.0) return 0.0;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}

// Helper: Fire input neurons based on retina activations
void fireInputNeurons(
    const std::vector<std::shared_ptr<Neuron>>& neurons,
    const std::vector<double>& activations,
    std::shared_ptr<NetworkPropagator> propagator,
    double currentTime) {

    for (size_t i = 0; i < neurons.size() && i < activations.size(); ++i) {
        if (activations[i] > 0.1) {  // Threshold for firing
            // Fire neuron with timing based on activation strength
            // Higher activation = earlier spike
            double firingTime = currentTime + (1.0 - activations[i]) * 10.0;  // 0-10ms delay
            propagator->fireNeuron(neurons[i]->getId(), firingTime);
        }
    }
}

// Helper: Check if any neurons in a layer should fire and schedule their spikes
void processLayerFiring(
    const std::vector<std::shared_ptr<Neuron>>& neurons,
    std::shared_ptr<NetworkPropagator> propagator,
    double firingTime) {

    for (const auto& neuron : neurons) {
        // Check if neuron should fire based on pattern matching
        if (neuron->checkShouldFire()) {
            // Schedule the neuron to fire at the specified future time
            // The SpikeProcessor will deliver the spike at the correct time
            neuron->fireAndAcknowledge(firingTime);
            propagator->fireNeuron(neuron->getId(), firingTime);
        }
    }
}

// Helper: Get activation vector from a layer of neurons
std::vector<double> getLayerActivations(
    const std::vector<std::shared_ptr<Neuron>>& neurons) {

    std::vector<double> activations;
    activations.reserve(neurons.size());

    for (const auto& neuron : neurons) {
        // Get best similarity to learned patterns
        double activation = neuron->getBestSimilarity();
        activations.push_back(activation >= 0 ? activation : 0.0);
    }

    return activations;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    try {
        // Load configuration
        std::cout << "=== MNIST V1 Hierarchical Architecture ===" << std::endl;
        std::cout << "Loading configuration from: " << argv[1] << std::endl;
        ConfigLoader configLoader(argv[1]);
        V1Config config = V1Config::fromConfigLoader(configLoader);

        // Create hierarchical structure
        std::cout << "\n=== Building Hierarchical Structure ===" << std::endl;
        NeuralObjectFactory factory;

        auto brain = factory.createBrain();
        brain->setName("Visual Processing Network");
        std::cout << "✓ Created Brain: " << brain->getName() << std::endl;

        auto hemisphere = factory.createHemisphere();
        hemisphere->setName("Left Hemisphere");
        brain->addHemisphere(hemisphere->getId());
        std::cout << "✓ Created Hemisphere: " << hemisphere->getName() << std::endl;

        auto occipitalLobe = factory.createLobe();
        occipitalLobe->setName("Occipital Lobe");
        hemisphere->addLobe(occipitalLobe->getId());
        std::cout << "✓ Created Lobe: " << occipitalLobe->getName() << std::endl;

        auto v1Region = factory.createRegion();
        v1Region->setName("Primary Visual Cortex (V1)");
        occipitalLobe->addRegion(v1Region->getId());
        std::cout << "✓ Created Region: " << v1Region->getName() << std::endl;

        auto v1Nucleus = factory.createNucleus();
        v1Nucleus->setName("V1 Input Processing Nucleus");
        v1Region->addNucleus(v1Nucleus->getId());
        std::cout << "✓ Created Nucleus: " << v1Nucleus->getName() << std::endl;

        auto orientationColumn = factory.createColumn();
        v1Nucleus->addColumn(orientationColumn->getId());
        std::cout << "✓ Created Column (ID: " << orientationColumn->getId() << ")" << std::endl;

        auto layer4C = factory.createLayer();
        orientationColumn->addLayer(layer4C->getId());
        std::cout << "✓ Created Layer 4C (ID: " << layer4C->getId() << ")" << std::endl;

        // Create 3 input clusters with different convolutions
        std::cout << "\n=== Creating 3 Input Clusters with Different Convolutions ===" << std::endl;

        // Cluster 1: Sobel threshold=0.165 (baseline)
        auto inputCluster1 = factory.createCluster();
        layer4C->addCluster(inputCluster1->getId());
        std::cout << "✓ Created Cluster 1 (ID: " << inputCluster1->getId() << ")" << std::endl;

        // Cluster 2: Sobel threshold=0.10 (more sensitive)
        auto inputCluster2 = factory.createCluster();
        layer4C->addCluster(inputCluster2->getId());
        std::cout << "✓ Created Cluster 2 (ID: " << inputCluster2->getId() << ")" << std::endl;

        // Cluster 3: Sobel threshold=0.25 (less sensitive)
        auto inputCluster3 = factory.createCluster();
        layer4C->addCluster(inputCluster3->getId());
        std::cout << "✓ Created Cluster 3 (ID: " << inputCluster3->getId() << ")" << std::endl;

        // Create 3 RetinaAdapters with different edge thresholds
        std::cout << "\n=== Creating 3 RetinaAdapters ===" << std::endl;

        // RetinaAdapter 1: threshold=0.165 (baseline)
        auto retinaConfig1 = configLoader.getAdapterConfig("retina");
        retinaConfig1.doubleParams["edge_threshold"] = 0.165;
        auto retina1 = std::make_shared<RetinaAdapter>(retinaConfig1);
        retina1->initialize();
        std::cout << "✓ RetinaAdapter 1: " << retina1->getNeurons().size() << " neurons, threshold=0.165" << std::endl;

        // RetinaAdapter 2: threshold=0.10 (more sensitive)
        auto retinaConfig2 = configLoader.getAdapterConfig("retina");
        retinaConfig2.doubleParams["edge_threshold"] = 0.10;
        auto retina2 = std::make_shared<RetinaAdapter>(retinaConfig2);
        retina2->initialize();
        std::cout << "✓ RetinaAdapter 2: " << retina2->getNeurons().size() << " neurons, threshold=0.10" << std::endl;

        // RetinaAdapter 3: threshold=0.25 (less sensitive)
        auto retinaConfig3 = configLoader.getAdapterConfig("retina");
        retinaConfig3.doubleParams["edge_threshold"] = 0.25;
        auto retina3 = std::make_shared<RetinaAdapter>(retinaConfig3);
        retina3->initialize();
        std::cout << "✓ RetinaAdapter 3: " << retina3->getNeurons().size() << " neurons, threshold=0.25" << std::endl;

        // Add neurons to clusters
        for (const auto& neuron : retina1->getNeurons()) {
            inputCluster1->addNeuron(neuron->getId());
        }
        for (const auto& neuron : retina2->getNeurons()) {
            inputCluster2->addNeuron(neuron->getId());
        }
        for (const auto& neuron : retina3->getNeurons()) {
            inputCluster3->addNeuron(neuron->getId());
        }
        std::cout << "✓ Added neurons to clusters (512 neurons each)" << std::endl;
        std::cout << "  Total neurons: " << (inputCluster1->size() + inputCluster2->size() + inputCluster3->size()) << std::endl;

        // ========================================================================
        // Create Interneuron Columns (3 columns, 128 neurons each)
        // ========================================================================
        std::cout << "\n=== Creating Interneuron Columns ===" << std::endl;

        // Create 3 interneuron clusters (one between each pair of adjacent input clusters)
        auto interneuronCluster1 = factory.createCluster();  // Between input clusters 1 and 2
        auto interneuronCluster2 = factory.createCluster();  // Between input clusters 2 and 3
        auto interneuronCluster3 = factory.createCluster();  // Between input clusters 3 and 1 (ring)
        layer4C->addCluster(interneuronCluster1->getId());
        layer4C->addCluster(interneuronCluster2->getId());
        layer4C->addCluster(interneuronCluster3->getId());

        // Create interneurons (128 per column, same parameters as input neurons)
        std::vector<std::shared_ptr<Neuron>> interneurons1, interneurons2, interneurons3;
        double neuronWindow = configLoader.get<double>("/neuron/window_size_ms", 200.0);
        double neuronThreshold = configLoader.get<double>("/neuron/similarity_threshold", 0.7);
        int neuronMaxPatterns = configLoader.get<int>("/neuron/max_patterns", 100);

        for (int i = 0; i < 128; ++i) {
            auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
            interneurons1.push_back(neuron);
            interneuronCluster1->addNeuron(neuron->getId());
        }
        for (int i = 0; i < 128; ++i) {
            auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
            interneurons2.push_back(neuron);
            interneuronCluster2->addNeuron(neuron->getId());
        }
        for (int i = 0; i < 128; ++i) {
            auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
            interneurons3.push_back(neuron);
            interneuronCluster3->addNeuron(neuron->getId());
        }

        std::cout << "✓ Created 3 interneuron columns (128 neurons each)" << std::endl;
        std::cout << "  Total interneurons: " << (interneurons1.size() + interneurons2.size() + interneurons3.size()) << std::endl;

        // ========================================================================
        // Create Sparse Connections Between Adjacent Clusters (50% connectivity)
        // ========================================================================
        std::cout << "\n=== Creating Sparse Horizontal Connections ===" << std::endl;

        // Storage for all created axons, synapses, and dendrites
        std::vector<std::shared_ptr<Axon>> allAxons;
        std::vector<std::shared_ptr<Synapse>> allSynapses;
        std::vector<std::shared_ptr<Dendrite>> allDendrites;

        // Helper lambda to create sparse bidirectional connections
        auto createSparseConnections = [&factory, &allAxons, &allSynapses, &allDendrites](
            const std::vector<std::shared_ptr<Neuron>>& sourceNeurons,
            const std::vector<std::shared_ptr<Neuron>>& interneurons,
            const std::vector<std::shared_ptr<Neuron>>& targetNeurons,
            double connectivity) {

            int connectionCount = 0;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.0, 1.0);

            // Connect source cluster → interneurons (50% sparse)
            for (const auto& sourceNeuron : sourceNeurons) {
                // Create axon for source neuron if not exists
                if (sourceNeuron->getAxonId() == 0) {
                    auto axon = factory.createAxon(sourceNeuron->getId());
                    sourceNeuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                }

                for (const auto& interneuron : interneurons) {
                    if (dis(gen) < connectivity) {
                        // Create dendrite for interneuron
                        auto dendrite = factory.createDendrite(interneuron->getId());
                        interneuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        // Create synapse connecting them
                        auto synapse = factory.createSynapse(
                            sourceNeuron->getAxonId(),
                            dendrite->getId(),
                            1.0,  // weight
                            1.0   // delay (ms)
                        );
                        allSynapses.push_back(synapse);

                        connectionCount++;
                    }
                }
            }

            // Connect interneurons → target cluster (50% sparse)
            for (const auto& interneuron : interneurons) {
                // Create axon for interneuron if not exists
                if (interneuron->getAxonId() == 0) {
                    auto axon = factory.createAxon(interneuron->getId());
                    interneuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                }

                for (const auto& targetNeuron : targetNeurons) {
                    if (dis(gen) < connectivity) {
                        // Create dendrite for target neuron
                        auto dendrite = factory.createDendrite(targetNeuron->getId());
                        targetNeuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        // Create synapse connecting them
                        auto synapse = factory.createSynapse(
                            interneuron->getAxonId(),
                            dendrite->getId(),
                            1.0,  // weight
                            1.0   // delay (ms)
                        );
                        allSynapses.push_back(synapse);

                        connectionCount++;
                    }
                }
            }

            return connectionCount;
        };

        // Create connections: Cluster1 ↔ Interneurons1 ↔ Cluster2
        int conn1 = createSparseConnections(retina1->getNeurons(), interneurons1, retina2->getNeurons(), 0.5);
        std::cout << "✓ Connected Cluster 1 ↔ Interneurons 1 ↔ Cluster 2: " << conn1 << " synapses" << std::endl;

        // Create connections: Cluster2 ↔ Interneurons2 ↔ Cluster3
        int conn2 = createSparseConnections(retina2->getNeurons(), interneurons2, retina3->getNeurons(), 0.5);
        std::cout << "✓ Connected Cluster 2 ↔ Interneurons 2 ↔ Cluster 3: " << conn2 << " synapses" << std::endl;

        // Create connections: Cluster3 ↔ Interneurons3 ↔ Cluster1 (ring topology)
        int conn3 = createSparseConnections(retina3->getNeurons(), interneurons3, retina1->getNeurons(), 0.5);
        std::cout << "✓ Connected Cluster 3 ↔ Interneurons 3 ↔ Cluster 1: " << conn3 << " synapses" << std::endl;
        std::cout << "  Total synapses: " << (conn1 + conn2 + conn3) << std::endl;

        // ========================================================================
        // Create V1 Hidden Layer (512 neurons)
        // ========================================================================
        std::cout << "\n=== Creating V1 Hidden Layer ===" << std::endl;

        auto v1HiddenLayer = factory.createLayer();
        orientationColumn->addLayer(v1HiddenLayer->getId());

        auto v1HiddenCluster = factory.createCluster();
        v1HiddenLayer->addCluster(v1HiddenCluster->getId());

        // Create 512 V1 hidden neurons
        std::vector<std::shared_ptr<Neuron>> v1HiddenNeurons;
        for (int i = 0; i < 512; ++i) {
            auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
            v1HiddenNeurons.push_back(neuron);
            v1HiddenCluster->addNeuron(neuron->getId());
        }

        std::cout << "✓ Created V1 hidden layer: " << v1HiddenNeurons.size() << " neurons" << std::endl;

        // Connect all input sources to V1 hidden layer (sparse 25% connectivity)
        std::cout << "\n=== Connecting Input Sources to V1 Hidden Layer ===" << std::endl;

        std::random_device rd2;
        std::mt19937 gen2(rd2());
        std::uniform_real_distribution<> dis2(0.0, 1.0);

        int v1Connections = 0;
        double v1Connectivity = 0.25;  // 25% sparse connectivity

        // Collect all source neurons (input clusters + interneurons)
        std::vector<std::shared_ptr<Neuron>> allSourceNeurons;
        allSourceNeurons.insert(allSourceNeurons.end(), retina1->getNeurons().begin(), retina1->getNeurons().end());
        allSourceNeurons.insert(allSourceNeurons.end(), retina2->getNeurons().begin(), retina2->getNeurons().end());
        allSourceNeurons.insert(allSourceNeurons.end(), retina3->getNeurons().begin(), retina3->getNeurons().end());
        allSourceNeurons.insert(allSourceNeurons.end(), interneurons1.begin(), interneurons1.end());
        allSourceNeurons.insert(allSourceNeurons.end(), interneurons2.begin(), interneurons2.end());
        allSourceNeurons.insert(allSourceNeurons.end(), interneurons3.begin(), interneurons3.end());

        std::cout << "  Total source neurons: " << allSourceNeurons.size() << " (1536 input + 384 interneurons)" << std::endl;

        // Create sparse connections from all sources to V1 hidden neurons
        for (const auto& sourceNeuron : allSourceNeurons) {
            // Create axon for source neuron if not exists
            if (sourceNeuron->getAxonId() == 0) {
                auto axon = factory.createAxon(sourceNeuron->getId());
                sourceNeuron->setAxonId(axon->getId());
                allAxons.push_back(axon);
            }

            for (const auto& v1Neuron : v1HiddenNeurons) {
                if (dis2(gen2) < v1Connectivity) {
                    // Create dendrite for V1 neuron
                    auto dendrite = factory.createDendrite(v1Neuron->getId());
                    v1Neuron->addDendrite(dendrite->getId());
                    allDendrites.push_back(dendrite);

                    // Create synapse
                    auto synapse = factory.createSynapse(
                        sourceNeuron->getAxonId(),
                        dendrite->getId(),
                        1.0,  // weight
                        1.0   // delay (ms)
                    );
                    allSynapses.push_back(synapse);

                    v1Connections++;
                }
            }
        }

        std::cout << "✓ Connected sources to V1 hidden layer: " << v1Connections << " synapses" << std::endl;

        // ========================================================================
        // Create Output Layer (10 neurons for digit classification)
        // ========================================================================
        std::cout << "\n=== Creating Output Layer ===" << std::endl;

        auto outputLayer = factory.createLayer();
        orientationColumn->addLayer(outputLayer->getId());

        auto outputCluster = factory.createCluster();
        outputLayer->addCluster(outputCluster->getId());

        // Create 10 output neurons (one per digit)
        std::vector<std::shared_ptr<Neuron>> outputNeurons;
        for (int i = 0; i < 10; ++i) {
            auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
            outputNeurons.push_back(neuron);
            outputCluster->addNeuron(neuron->getId());
        }

        std::cout << "✓ Created output layer: " << outputNeurons.size() << " neurons (one per digit)" << std::endl;

        // Connect V1 hidden layer to output neurons (50% connectivity)
        std::cout << "\n=== Connecting V1 Hidden Layer to Output ===" << std::endl;

        std::random_device rd3;
        std::mt19937 gen3(rd3());
        std::uniform_real_distribution<> dis3(0.0, 1.0);

        int outputConnections = 0;
        double outputConnectivity = 0.5;  // 50% connectivity

        for (const auto& v1Neuron : v1HiddenNeurons) {
            // Create axon for V1 neuron if not exists
            if (v1Neuron->getAxonId() == 0) {
                auto axon = factory.createAxon(v1Neuron->getId());
                v1Neuron->setAxonId(axon->getId());
                allAxons.push_back(axon);
            }

            for (const auto& outputNeuron : outputNeurons) {
                if (dis3(gen3) < outputConnectivity) {
                    // Create dendrite for output neuron
                    auto dendrite = factory.createDendrite(outputNeuron->getId());
                    outputNeuron->addDendrite(dendrite->getId());
                    allDendrites.push_back(dendrite);

                    // Create synapse
                    auto synapse = factory.createSynapse(
                        v1Neuron->getAxonId(),
                        dendrite->getId(),
                        1.0,  // weight
                        1.0   // delay (ms)
                    );
                    allSynapses.push_back(synapse);

                    outputConnections++;
                }
            }
        }

        std::cout << "✓ Connected V1 to output layer: " << outputConnections << " synapses" << std::endl;

        // Print network summary
        std::cout << "\n=== Network Architecture Summary ===" << std::endl;
        std::cout << "  Input Layer:       1536 neurons (3 clusters × 512)" << std::endl;
        std::cout << "  Interneurons:       384 neurons (3 columns × 128)" << std::endl;
        std::cout << "  V1 Hidden Layer:    512 neurons" << std::endl;
        std::cout << "  Output Layer:        10 neurons" << std::endl;
        std::cout << "  Total Neurons:     2442 neurons" << std::endl;
        std::cout << "  Total Synapses:    " << (conn1 + conn2 + conn3 + v1Connections + outputConnections) << " synapses" << std::endl;

        // ========================================================================
        // Create SpikeProcessor and NetworkPropagator for spike-based propagation
        // ========================================================================
        std::cout << "\n=== Initializing Spike-Based Propagation System ===" << std::endl;

        // Create SpikeProcessor with configurable buffer and thread count
        int bufferSize = configLoader.get<int>("/spike_processor/buffer_size", 10000);
        int numThreads = configLoader.get<int>("/spike_processor/num_threads", 20);
        auto spikeProcessor = std::make_shared<SpikeProcessor>(bufferSize, numThreads);
        std::cout << "✓ Created SpikeProcessor: " << bufferSize << " time slices ("
                  << bufferSize << "ms buffer), " << numThreads << " delivery threads" << std::endl;

        // Create NetworkPropagator
        auto networkPropagator = std::make_shared<NetworkPropagator>(spikeProcessor);

        // Set STDP parameters
        double stdpAPlus = configLoader.get<double>("/stdp/a_plus", 0.01);
        double stdpAMinus = configLoader.get<double>("/stdp/a_minus", 0.012);
        double stdpTauPlus = configLoader.get<double>("/stdp/tau_plus", 20.0);
        double stdpTauMinus = configLoader.get<double>("/stdp/tau_minus", 20.0);
        networkPropagator->setSTDPParameters(stdpAPlus, stdpAMinus, stdpTauPlus, stdpTauMinus);
        std::cout << "✓ Created NetworkPropagator with STDP (A+=" << stdpAPlus
                  << ", A-=" << stdpAMinus << ", τ+=" << stdpTauPlus << ", τ-=" << stdpTauMinus << ")" << std::endl;

        // Register all neurons with NetworkPropagator
        std::cout << "\n=== Registering Neural Objects ===" << std::endl;

        // Collect all neurons
        std::vector<std::shared_ptr<Neuron>> allNeurons;
        allNeurons.insert(allNeurons.end(), retina1->getNeurons().begin(), retina1->getNeurons().end());
        allNeurons.insert(allNeurons.end(), retina2->getNeurons().begin(), retina2->getNeurons().end());
        allNeurons.insert(allNeurons.end(), retina3->getNeurons().begin(), retina3->getNeurons().end());
        allNeurons.insert(allNeurons.end(), interneurons1.begin(), interneurons1.end());
        allNeurons.insert(allNeurons.end(), interneurons2.begin(), interneurons2.end());
        allNeurons.insert(allNeurons.end(), interneurons3.begin(), interneurons3.end());
        allNeurons.insert(allNeurons.end(), v1HiddenNeurons.begin(), v1HiddenNeurons.end());
        allNeurons.insert(allNeurons.end(), outputNeurons.begin(), outputNeurons.end());

        // Register neurons and set NetworkPropagator reference
        for (const auto& neuron : allNeurons) {
            networkPropagator->registerNeuron(neuron);
            neuron->setNetworkPropagator(networkPropagator);
        }
        std::cout << "✓ Registered " << allNeurons.size() << " neurons" << std::endl;

        // Register all axons
        for (const auto& axon : allAxons) {
            networkPropagator->registerAxon(axon);
        }
        std::cout << "✓ Registered " << allAxons.size() << " axons" << std::endl;

        // Register all synapses
        for (const auto& synapse : allSynapses) {
            networkPropagator->registerSynapse(synapse);
        }
        std::cout << "✓ Registered " << allSynapses.size() << " synapses" << std::endl;

        // Register all dendrites
        for (const auto& dendrite : allDendrites) {
            networkPropagator->registerDendrite(dendrite);
            dendrite->setNetworkPropagator(networkPropagator);
            spikeProcessor->registerDendrite(dendrite);
        }
        std::cout << "✓ Registered " << allDendrites.size() << " dendrites" << std::endl;

        // Start the SpikeProcessor background thread
        spikeProcessor->start();
        std::cout << "✓ Started SpikeProcessor background thread" << std::endl;

        // Create learning strategy
        PatternUpdateStrategy::Config strategyConfig;
        strategyConfig.maxPatterns = configLoader.get<int>("/neuron/max_patterns", 100);
        strategyConfig.similarityThreshold = configLoader.get<double>("/neuron/similarity_threshold", 0.7);
        strategyConfig.doubleParams["merge_threshold"] = configLoader.get<double>("/learning/merge_threshold", 0.85);
        strategyConfig.doubleParams["merge_weight"] = configLoader.get<double>("/learning/merge_weight", 0.3);
        strategyConfig.doubleParams["blend_alpha"] = configLoader.get<double>("/learning/blend_alpha", 0.2);
        strategyConfig.intParams["prune_threshold"] = configLoader.get<int>("/learning/prune_threshold", 2);

        auto strategy = std::make_shared<HybridStrategy>(strategyConfig);

        // Set strategy for all neurons in the network
        for (const auto& neuron : retina1->getNeurons()) {
            neuron->setPatternUpdateStrategy(strategy);
        }
        for (const auto& neuron : retina2->getNeurons()) {
            neuron->setPatternUpdateStrategy(strategy);
        }
        for (const auto& neuron : retina3->getNeurons()) {
            neuron->setPatternUpdateStrategy(strategy);
        }
        for (const auto& neuron : interneurons1) {
            neuron->setPatternUpdateStrategy(strategy);
        }
        for (const auto& neuron : interneurons2) {
            neuron->setPatternUpdateStrategy(strategy);
        }
        for (const auto& neuron : interneurons3) {
            neuron->setPatternUpdateStrategy(strategy);
        }
        for (const auto& neuron : v1HiddenNeurons) {
            neuron->setPatternUpdateStrategy(strategy);
        }
        for (const auto& neuron : outputNeurons) {
            neuron->setPatternUpdateStrategy(strategy);
        }
        std::cout << "✓ Applied HybridStrategy to all 2442 neurons" << std::endl;

        // Load MNIST data
        std::cout << "\n=== Loading MNIST Data ===" << std::endl;
        MNISTLoader trainLoader;
        MNISTLoader testLoader;

        if (!trainLoader.load(config.trainImagesPath, config.trainLabelsPath)) {
            std::cerr << "Failed to load training data" << std::endl;
            return 1;
        }
        if (!testLoader.load(config.testImagesPath, config.testLabelsPath)) {
            std::cerr << "Failed to load test data" << std::endl;
            return 1;
        }

        std::cout << "✓ Loaded " << trainLoader.size() << " training images" << std::endl;
        std::cout << "✓ Loaded " << testLoader.size() << " test images" << std::endl;

        // Training
        std::cout << "\n=== Training Phase (Spike-Based with STDP) ===" << std::endl;
        auto trainStart = std::chrono::steady_clock::now();

        // First pass: collect indices of images to train on
        std::vector<size_t> trainingIndices;
        std::vector<int> trainCount(10, 0);
        for (size_t i = 0; i < trainLoader.size(); ++i) {
            int label = trainLoader.getImage(i).label;
            if (trainCount[label] < config.trainPerDigit) {
                trainingIndices.push_back(i);
                trainCount[label]++;
            }
        }

        std::cout << "  Selected " << trainingIndices.size() << " training images" << std::endl;
        std::cout << "  Using spike-based forward propagation with STDP learning" << std::endl;

        // Second pass: process images sequentially with spike-based propagation
        for (size_t idx = 0; idx < trainingIndices.size(); ++idx) {
            size_t i = trainingIndices[idx];
            int label = trainLoader.getImage(i).label;

            // Convert MNISTLoader::Image to RetinaAdapter::Image
            RetinaAdapter::Image img;
            img.pixels = trainLoader.getImage(i).pixels;
            img.rows = trainLoader.getImage(i).rows;
            img.cols = trainLoader.getImage(i).cols;

            // Clear all spike buffers before processing new image
            for (const auto& neuron : allNeurons) {
                neuron->clearSpikes();
            }

            // Process through all 3 retinas to get activations
            auto activation1 = retina1->processImage(img);
            auto activation2 = retina2->processImage(img);
            auto activation3 = retina3->processImage(img);

            // Schedule all spikes into the future using the circular event queue
            // The SpikeProcessor will automatically deliver them at the correct times
            double currentTime = spikeProcessor->getCurrentTime();

            // Schedule input neuron spikes (0-10ms from now based on activation)
            fireInputNeurons(retina1->getNeurons(), activation1, networkPropagator, currentTime);
            fireInputNeurons(retina2->getNeurons(), activation2, networkPropagator, currentTime);
            fireInputNeurons(retina3->getNeurons(), activation3, networkPropagator, currentTime);

            // Schedule intermediate layer processing (10ms after input)
            processLayerFiring(interneurons1, networkPropagator, currentTime + 10.0);
            processLayerFiring(interneurons2, networkPropagator, currentTime + 10.0);
            processLayerFiring(interneurons3, networkPropagator, currentTime + 10.0);

            // Schedule V1 hidden layer processing (20ms after input)
            processLayerFiring(v1HiddenNeurons, networkPropagator, currentTime + 20.0);

            // Schedule supervised teaching signal (50ms after input)
            // This fires the correct output neuron for STDP learning
            outputNeurons[label]->fireAndAcknowledge(currentTime + 50.0);
            networkPropagator->fireNeuron(outputNeurons[label]->getId(), currentTime + 50.0);

            // Wait for all scheduled events to complete (60ms total)
            std::this_thread::sleep_for(std::chrono::milliseconds(60));

            // Learn patterns in all neurons that received spikes
            for (const auto& neuron : retina1->getNeurons()) {
                if (neuron->getSpikes().size() > 0) {
                    neuron->learnCurrentPattern();
                }
            }
            for (const auto& neuron : retina2->getNeurons()) {
                if (neuron->getSpikes().size() > 0) {
                    neuron->learnCurrentPattern();
                }
            }
            for (const auto& neuron : retina3->getNeurons()) {
                if (neuron->getSpikes().size() > 0) {
                    neuron->learnCurrentPattern();
                }
            }
            for (const auto& neuron : interneurons1) {
                if (neuron->getSpikes().size() > 0) {
                    neuron->learnCurrentPattern();
                }
            }
            for (const auto& neuron : interneurons2) {
                if (neuron->getSpikes().size() > 0) {
                    neuron->learnCurrentPattern();
                }
            }
            for (const auto& neuron : interneurons3) {
                if (neuron->getSpikes().size() > 0) {
                    neuron->learnCurrentPattern();
                }
            }
            for (const auto& neuron : v1HiddenNeurons) {
                if (neuron->getSpikes().size() > 0) {
                    neuron->learnCurrentPattern();
                }
            }
            for (const auto& neuron : outputNeurons) {
                if (neuron->getSpikes().size() > 0) {
                    neuron->learnCurrentPattern();
                }
            }

            if ((idx + 1) % 1000 == 0) {
                std::cout << "  Processed " << (idx + 1) << " images..." << std::endl;
            }
        }

        auto trainEnd = std::chrono::steady_clock::now();
        double trainTime = std::chrono::duration<double>(trainEnd - trainStart).count();
        
        std::cout << "✓ Training complete in " << std::fixed << std::setprecision(1) 
                  << trainTime << "s" << std::endl;
        for (int d = 0; d < 10; ++d) {
            std::cout << "  Digit " << d << ": " << trainCount[d] << " patterns" << std::endl;
        }

        // Testing
        std::cout << "\n=== Testing Phase (Spike-Based Classification) ===" << std::endl;
        std::cout << "  Using output layer activations for classification" << std::endl;
        auto testStart = std::chrono::steady_clock::now();

        int correct = 0;
        std::vector<int> perDigitCorrect(10, 0);
        std::vector<int> perDigitTotal(10, 0);

        size_t numTestImages = std::min((size_t)config.testImages, testLoader.size());

        // Testing is sequential for now (spike propagation is not thread-safe yet)
        for (size_t i = 0; i < numTestImages; ++i) {
            int trueLabel = testLoader.getImage(i).label;

            // Convert MNISTLoader::Image to RetinaAdapter::Image
            RetinaAdapter::Image img;
            img.pixels = testLoader.getImage(i).pixels;
            img.rows = testLoader.getImage(i).rows;
            img.cols = testLoader.getImage(i).cols;

            // Clear all spike buffers before processing new image
            for (const auto& neuron : allNeurons) {
                neuron->clearSpikes();
            }

            // Process through all 3 retinas to get activations
            auto activation1 = retina1->processImage(img);
            auto activation2 = retina2->processImage(img);
            auto activation3 = retina3->processImage(img);

            // Schedule all spikes into the future using the circular event queue
            double currentTime = spikeProcessor->getCurrentTime();

            // Schedule input neuron spikes (0-10ms from now based on activation)
            fireInputNeurons(retina1->getNeurons(), activation1, networkPropagator, currentTime);
            fireInputNeurons(retina2->getNeurons(), activation2, networkPropagator, currentTime);
            fireInputNeurons(retina3->getNeurons(), activation3, networkPropagator, currentTime);

            // Schedule intermediate layer processing (10ms after input)
            processLayerFiring(interneurons1, networkPropagator, currentTime + 10.0);
            processLayerFiring(interneurons2, networkPropagator, currentTime + 10.0);
            processLayerFiring(interneurons3, networkPropagator, currentTime + 10.0);

            // Schedule V1 hidden layer processing (20ms after input)
            processLayerFiring(v1HiddenNeurons, networkPropagator, currentTime + 20.0);

            // Wait for all scheduled events to complete (30ms total for inference)
            std::this_thread::sleep_for(std::chrono::milliseconds(30));

            // Get activations from output neurons
            auto outputActivations = getLayerActivations(outputNeurons);

            // Classify based on which output neuron has highest activation
            int predicted = 0;
            double maxActivation = outputActivations[0];
            for (int d = 1; d < 10; ++d) {
                if (outputActivations[d] > maxActivation) {
                    maxActivation = outputActivations[d];
                    predicted = d;
                }
            }

            if (predicted == trueLabel) {
                correct++;
                perDigitCorrect[trueLabel]++;
            }
            perDigitTotal[trueLabel]++;

            if ((i + 1) % 100 == 0) {
                std::cout << "  Tested " << (i + 1) << " images..." << std::endl;
            }
        }

        auto testEnd = std::chrono::steady_clock::now();
        double testTime = std::chrono::duration<double>(testEnd - testStart).count();

        // Results
        std::cout << "\n=== Results ===" << std::endl;
        std::cout << "Overall Accuracy: " << std::fixed << std::setprecision(2)
                  << (100.0 * correct / config.testImages) << "% "
                  << "(" << correct << "/" << config.testImages << ")" << std::endl;
        
        std::cout << "\nPer-Digit Accuracy:" << std::endl;
        for (int d = 0; d < 10; ++d) {
            double acc = perDigitTotal[d] > 0 ? 100.0 * perDigitCorrect[d] / perDigitTotal[d] : 0.0;
            std::cout << "  Digit " << d << ": " << std::setw(5) << std::fixed << std::setprecision(1)
                      << acc << "% (" << perDigitCorrect[d] << "/" << perDigitTotal[d] << ")" << std::endl;
        }

        std::cout << "\nTiming:" << std::endl;
        std::cout << "  Training: " << std::fixed << std::setprecision(1) << trainTime << "s" << std::endl;
        std::cout << "  Testing:  " << std::fixed << std::setprecision(1) << testTime << "s" << std::endl;

        std::cout << "\n=== Hierarchical Structure Summary ===" << std::endl;
        std::cout << "Brain: " << brain->getName() << std::endl;
        std::cout << "  └─ Hemisphere: " << hemisphere->getName() << std::endl;
        std::cout << "      └─ Lobe: " << occipitalLobe->getName() << std::endl;
        std::cout << "          └─ Region: " << v1Region->getName() << std::endl;
        std::cout << "              └─ Nucleus: " << v1Nucleus->getName() << std::endl;
        std::cout << "                  └─ Column: Orientation Column (ID: " << orientationColumn->getId() << ")" << std::endl;
        std::cout << "                      └─ Layer: Layer 4C (ID: " << layer4C->getId() << ")" << std::endl;
        std::cout << "                          ├─ Cluster 1: Sobel threshold=0.165 (ID: " << inputCluster1->getId()
                  << ", " << inputCluster1->size() << " neurons)" << std::endl;
        std::cout << "                          ├─ Cluster 2: Sobel threshold=0.10 (ID: " << inputCluster2->getId()
                  << ", " << inputCluster2->size() << " neurons)" << std::endl;
        std::cout << "                          └─ Cluster 3: Sobel threshold=0.25 (ID: " << inputCluster3->getId()
                  << ", " << inputCluster3->size() << " neurons)" << std::endl;
        std::cout << "\nTotal neurons: " << (inputCluster1->size() + inputCluster2->size() + inputCluster3->size()) << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

