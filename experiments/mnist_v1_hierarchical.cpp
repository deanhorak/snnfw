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

// ============================================================================
// Gabor Filter for Orientation-Selective Receptive Fields
// ============================================================================

/**
 * @brief Create a Gabor filter kernel for orientation selectivity
 * @param orientation Orientation in degrees (0-180)
 * @param size Kernel size (default: 7x7)
 * @return 2D Gabor filter kernel
 *
 * Gabor filters are biologically plausible models of V1 simple cell receptive fields.
 * They respond maximally to edges at a specific orientation.
 */
std::vector<std::vector<double>> createGaborKernel(double orientation, int size = 7) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    double sigma = 2.0;      // Gaussian envelope width
    double lambda = 4.0;     // Wavelength of sinusoid
    double gamma = 0.5;      // Spatial aspect ratio

    int center = size / 2;
    double theta = orientation * M_PI / 180.0;  // Convert to radians

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - center;

            // Rotate coordinates to orientation
            double x_theta = dx * cos(theta) + dy * sin(theta);
            double y_theta = -dx * sin(theta) + dy * cos(theta);

            // Gabor function: Gaussian envelope × sinusoidal grating
            double gaussian = exp(-(x_theta*x_theta + gamma*gamma*y_theta*y_theta) / (2*sigma*sigma));
            double sinusoid = cos(2*M_PI*x_theta/lambda);

            kernel[y][x] = gaussian * sinusoid;
        }
    }

    return kernel;
}

/**
 * @brief Apply Gabor filter to raw image pixels
 * @param imagePixels Raw image pixels (28x28 = 784 values, 0-255)
 * @param gaborKernel Gabor filter kernel
 * @param imgWidth Image width (28 for MNIST)
 * @param imgHeight Image height (28 for MNIST)
 * @param poolSize Pooling size to reduce to 8x8 grid
 * @return Filtered and pooled response map (64 values for 8x8 grid)
 */
std::vector<double> applyGaborFilter(const std::vector<uint8_t>& imagePixels,
                                     const std::vector<std::vector<double>>& gaborKernel,
                                     int imgWidth = 28,
                                     int imgHeight = 28,
                                     int poolSize = 3) {
    int kernelSize = gaborKernel.size();
    int halfKernel = kernelSize / 2;

    // Apply Gabor filter via convolution on full 28x28 image
    std::vector<double> fullResponse(imgWidth * imgHeight, 0.0);

    for (int y = halfKernel; y < imgHeight - halfKernel; ++y) {
        for (int x = halfKernel; x < imgWidth - halfKernel; ++x) {
            double sum = 0.0;

            for (int ky = 0; ky < kernelSize; ++ky) {
                for (int kx = 0; kx < kernelSize; ++kx) {
                    int imgY = y + ky - halfKernel;
                    int imgX = x + kx - halfKernel;
                    double pixelValue = imagePixels[imgY * imgWidth + imgX] / 255.0;  // Normalize to [0,1]
                    sum += pixelValue * gaborKernel[ky][kx];
                }
            }

            fullResponse[y * imgWidth + x] = std::abs(sum);  // Rectify (complex cell behavior)
        }
    }

    // Max pooling to reduce to 8x8 grid
    const int gridSize = 8;
    std::vector<double> pooledResponse(gridSize * gridSize, 0.0);

    for (int gy = 0; gy < gridSize; ++gy) {
        for (int gx = 0; gx < gridSize; ++gx) {
            double maxVal = 0.0;

            // Pool over poolSize×poolSize region
            for (int py = 0; py < poolSize; ++py) {
                for (int px = 0; px < poolSize; ++px) {
                    int imgY = gy * poolSize + py;
                    int imgX = gx * poolSize + px;
                    if (imgY < imgHeight && imgX < imgWidth) {
                        maxVal = std::max(maxVal, fullResponse[imgY * imgWidth + imgX]);
                    }
                }
            }

            pooledResponse[gy * gridSize + gx] = maxVal;
        }
    }

    return pooledResponse;
}

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
        // Check if neuron should fire based on learned patterns
        bool shouldFire = neuron->checkShouldFire();

        if (shouldFire) {
            neuron->fireAndAcknowledge(firingTime);
            propagator->fireNeuron(neuron->getId(), firingTime);
        }
    }
}

// Helper: Copy spike pattern from source layer to target neurons
// This allows output neurons to learn V1 spike patterns during supervised training
void copyLayerSpikePattern(
    const std::vector<std::shared_ptr<Neuron>>& sourceLayer,
    const std::vector<std::shared_ptr<Neuron>>& targetNeurons) {

    // Collect all spike times from source layer
    std::vector<double> layerSpikes;
    for (const auto& sourceNeuron : sourceLayer) {
        auto neuronSpikes = sourceNeuron->getSpikes();
        layerSpikes.insert(layerSpikes.end(), neuronSpikes.begin(), neuronSpikes.end());
    }

    // Sort spikes by time
    std::sort(layerSpikes.begin(), layerSpikes.end());

    // Copy spike pattern to all target neurons
    for (const auto& targetNeuron : targetNeurons) {
        targetNeuron->clearSpikes();  // Clear any existing spikes
        for (double spikeTime : layerSpikes) {
            targetNeuron->insertSpike(spikeTime);
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

// Helper: Apply k-winner-take-all lateral inhibition within a layer
void applyLateralInhibition(
    const std::vector<std::shared_ptr<Neuron>>& neurons,
    int k = 20) {  // Keep top k% active

    if (neurons.empty()) return;

    // Reset inhibition for all neurons
    for (const auto& neuron : neurons) {
        neuron->resetInhibition();
    }

    // Get activations and create index pairs
    std::vector<std::pair<double, size_t>> activationPairs;
    for (size_t i = 0; i < neurons.size(); ++i) {
        double activation = neurons[i]->getBestSimilarity();
        if (activation > 0.0) {
            activationPairs.push_back({activation, i});
        }
    }

    if (activationPairs.empty()) return;

    // Sort by activation (descending)
    std::sort(activationPairs.begin(), activationPairs.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    // Calculate how many winners to keep
    size_t numWinners = std::max(1UL, (activationPairs.size() * k) / 100);

    // Inhibit all non-winners
    double inhibitionStrength = 0.8;  // Strong inhibition
    for (size_t i = numWinners; i < activationPairs.size(); ++i) {
        size_t neuronIdx = activationPairs[i].second;
        neurons[neuronIdx]->applyInhibition(inhibitionStrength);
    }
}

// Helper: Analyze weight distribution of synapses
void analyzeWeightDistribution(
    const std::map<uint64_t, std::shared_ptr<Synapse>>& synapses,
    const std::string& layerName) {

    if (synapses.empty()) return;

    std::vector<double> weights;
    weights.reserve(synapses.size());

    for (const auto& [id, synapse] : synapses) {
        weights.push_back(synapse->getWeight());
    }

    // Calculate statistics
    double sum = 0.0, sumSq = 0.0;
    double minWeight = weights[0], maxWeight = weights[0];

    for (double w : weights) {
        sum += w;
        sumSq += w * w;
        minWeight = std::min(minWeight, w);
        maxWeight = std::max(maxWeight, w);
    }

    double mean = sum / weights.size();
    double variance = (sumSq / weights.size()) - (mean * mean);
    double stddev = std::sqrt(variance);

    // Calculate median
    std::sort(weights.begin(), weights.end());
    double median = weights[weights.size() / 2];

    std::cout << "  " << layerName << " Weight Statistics:" << std::endl;
    std::cout << "    Count: " << weights.size() << std::endl;
    std::cout << "    Mean: " << std::fixed << std::setprecision(4) << mean << std::endl;
    std::cout << "    Std Dev: " << stddev << std::endl;
    std::cout << "    Median: " << median << std::endl;
    std::cout << "    Min: " << minWeight << std::endl;
    std::cout << "    Max: " << maxWeight << std::endl;
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

        // Create 512 V1 hidden neurons organized by orientation columns
        // 8 orientation columns × 64 neurons each (8x8 spatial grid)
        const int NUM_ORIENTATIONS = 8;
        const int NEURONS_PER_ORIENTATION = 64;  // 8x8 spatial grid

        std::vector<std::shared_ptr<Neuron>> v1HiddenNeurons;
        std::vector<std::vector<std::shared_ptr<Neuron>>> v1OrientationColumns(NUM_ORIENTATIONS);

        for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
            for (int i = 0; i < NEURONS_PER_ORIENTATION; ++i) {
                auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
                v1HiddenNeurons.push_back(neuron);
                v1OrientationColumns[ori].push_back(neuron);
                v1HiddenCluster->addNeuron(neuron->getId());
            }
        }

        std::cout << "✓ Created V1 hidden layer: " << v1HiddenNeurons.size() << " neurons" << std::endl;
        std::cout << "  ├─ " << NUM_ORIENTATIONS << " orientation columns" << std::endl;
        std::cout << "  └─ " << NEURONS_PER_ORIENTATION << " neurons per column (8x8 spatial grid)" << std::endl;

        // ========================================================================
        // Create V2 Hidden Layer (256 neurons)
        // ========================================================================
        std::cout << "\n=== Creating V2 Hidden Layer ===" << std::endl;

        auto v2HiddenLayer = factory.createLayer();
        orientationColumn->addLayer(v2HiddenLayer->getId());

        auto v2HiddenCluster = factory.createCluster();
        v2HiddenLayer->addCluster(v2HiddenCluster->getId());

        // Create 256 V2 hidden neurons (half of V1 for progressive abstraction)
        std::vector<std::shared_ptr<Neuron>> v2HiddenNeurons;
        for (int i = 0; i < 256; ++i) {
            auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
            v2HiddenNeurons.push_back(neuron);
            v2HiddenCluster->addNeuron(neuron->getId());
        }

        std::cout << "✓ Created V2 hidden layer: " << v2HiddenNeurons.size() << " neurons" << std::endl;

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
        // Connect V1 to V2 (sparse 30% connectivity)
        // ========================================================================
        std::cout << "\n=== Connecting V1 to V2 Hidden Layer ===" << std::endl;

        int v2Connections = 0;
        double v2Connectivity = 0.30;  // 30% sparse connectivity

        for (const auto& v1Neuron : v1HiddenNeurons) {
            // Create axon for V1 neuron if not exists
            if (v1Neuron->getAxonId() == 0) {
                auto axon = factory.createAxon(v1Neuron->getId());
                v1Neuron->setAxonId(axon->getId());
                allAxons.push_back(axon);
            }

            for (const auto& v2Neuron : v2HiddenNeurons) {
                if (dis2(gen2) < v2Connectivity) {
                    // Create dendrite for V2 neuron
                    auto dendrite = factory.createDendrite(v2Neuron->getId());
                    v2Neuron->addDendrite(dendrite->getId());
                    allDendrites.push_back(dendrite);

                    // Create synapse
                    auto synapse = factory.createSynapse(
                        v1Neuron->getAxonId(),
                        dendrite->getId(),
                        1.0,  // weight
                        1.0   // delay (ms)
                    );
                    allSynapses.push_back(synapse);

                    v2Connections++;
                }
            }
        }

        std::cout << "✓ Connected V1 to V2 hidden layer: " << v2Connections << " synapses" << std::endl;

        // ========================================================================
        // Create Output Layer with Population Coding (10 populations of 10 neurons each)
        // ========================================================================
        std::cout << "\n=== Creating Output Layer with Population Coding ===" << std::endl;

        auto outputLayer = factory.createLayer();
        orientationColumn->addLayer(outputLayer->getId());

        // Create 10 populations (one per digit), each with 10 neurons
        const int NEURONS_PER_DIGIT = 10;
        std::vector<std::vector<std::shared_ptr<Neuron>>> outputPopulations;
        int totalOutputNeurons = 0;

        for (int digit = 0; digit < 10; ++digit) {
            // Create a cluster for this digit's population
            auto digitCluster = factory.createCluster();
            outputLayer->addCluster(digitCluster->getId());

            std::vector<std::shared_ptr<Neuron>> population;
            for (int i = 0; i < NEURONS_PER_DIGIT; ++i) {
                auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
                population.push_back(neuron);
                digitCluster->addNeuron(neuron->getId());
                totalOutputNeurons++;
            }
            outputPopulations.push_back(population);
        }

        std::cout << "✓ Created output layer: " << totalOutputNeurons << " neurons ("
                  << NEURONS_PER_DIGIT << " per digit, 10 digits)" << std::endl;

        // Connect V1 hidden layer to ALL output neurons (50% connectivity)
        std::cout << "\n=== Connecting V1 Hidden Layer to Output Populations ===" << std::endl;

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

            // Connect to all neurons in all populations
            for (const auto& population : outputPopulations) {
                for (const auto& outputNeuron : population) {
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
        }

        std::cout << "✓ Connected V1 to output populations: " << outputConnections << " synapses" << std::endl;

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
        allNeurons.insert(allNeurons.end(), v2HiddenNeurons.begin(), v2HiddenNeurons.end());

        // Flatten output populations into single vector
        for (const auto& population : outputPopulations) {
            allNeurons.insert(allNeurons.end(), population.begin(), population.end());
        }

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

        // Disable real-time sync for maximum speed during training
        spikeProcessor->setRealTimeSync(false);
        std::cout << "✓ Disabled real-time sync (fast mode)" << std::endl;

        // Start the SpikeProcessor background thread
        spikeProcessor->start();
        std::cout << "✓ Started SpikeProcessor background thread" << std::endl;

        // Create learning strategy (if configured)
        std::string learningStrategy = configLoader.get<std::string>("/learning/strategy", "none");

        if (learningStrategy == "hybrid") {
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
            for (const auto& population : outputPopulations) {
                for (const auto& neuron : population) {
                    neuron->setPatternUpdateStrategy(strategy);
                }
            }
            std::cout << "✓ Applied HybridStrategy to all " << allNeurons.size() << " neurons" << std::endl;
        } else {
            std::cout << "✓ Using default pattern learning (no strategy)" << std::endl;
        }

        // Set similarity metric for all neurons
        std::string similarityMetricStr = configLoader.get<std::string>("/neuron/similarity_metric", "cosine");
        SimilarityMetric metric = SimilarityMetric::COSINE;  // Default
        if (similarityMetricStr == "histogram") {
            metric = SimilarityMetric::HISTOGRAM;
        } else if (similarityMetricStr == "euclidean") {
            metric = SimilarityMetric::EUCLIDEAN;
        } else if (similarityMetricStr == "correlation") {
            metric = SimilarityMetric::CORRELATION;
        } else if (similarityMetricStr == "waveform") {
            metric = SimilarityMetric::WAVEFORM;
        }

        for (const auto& neuron : allNeurons) {
            neuron->setSimilarityMetric(metric);
        }
        std::cout << "✓ Set similarity metric to: " << similarityMetricStr << " for all neurons" << std::endl;

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

        // Create Gabor filters for each orientation column
        std::cout << "\n=== Creating Orientation-Selective Gabor Filters ===" << std::endl;
        std::vector<std::vector<std::vector<double>>> gaborKernels;
        for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
            double orientation = ori * (180.0 / NUM_ORIENTATIONS);  // 0°, 22.5°, 45°, 67.5°, 90°, 112.5°, 135°, 157.5°
            auto kernel = createGaborKernel(orientation);
            gaborKernels.push_back(kernel);
            std::cout << "  ✓ Created Gabor filter for orientation " << orientation << "°" << std::endl;
        }

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

        // Analyze initial weight distribution
        std::cout << "\n=== Initial Weight Distribution ===" << std::endl;
        std::map<uint64_t, std::shared_ptr<Synapse>> allSynapsesMap;
        for (const auto& synapse : allSynapses) {
            allSynapsesMap[synapse->getId()] = synapse;
        }
        analyzeWeightDistribution(allSynapsesMap, "All Synapses");

        // Second pass: process images sequentially with spike-based propagation
        auto loopStartTime = std::chrono::high_resolution_clock::now();
        for (size_t idx = 0; idx < trainingIndices.size(); ++idx) {
            auto imageStartTime = std::chrono::high_resolution_clock::now();
            if (idx == 1) std::cout << "  [DEBUG] Starting image 1..." << std::endl;

            size_t i = trainingIndices[idx];
            int label = trainLoader.getImage(i).label;

            if (idx % 100 == 0 || idx <= 2) {
                std::cout << "  Processing training image " << idx << "/" << trainingIndices.size()
                          << " (label=" << label << ")" << std::endl;
            }

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

            // Get current simulation time
            double currentTime = spikeProcessor->getCurrentTime();

            // ============================================================================
            // SIMPLIFIED HIERARCHICAL LEARNING (Fast + Biologically Plausible)
            // ============================================================================

            // STEP 1: Fire retina neurons based on image activations
            fireInputNeurons(retina1->getNeurons(), activation1, networkPropagator, currentTime);
            fireInputNeurons(retina2->getNeurons(), activation2, networkPropagator, currentTime);
            fireInputNeurons(retina3->getNeurons(), activation3, networkPropagator, currentTime);

            // STEP 2: Fire V1 neurons based on ORIENTATION-SELECTIVE GABOR FILTERING
            // Each orientation column responds to edges at a specific orientation
            // This creates orientation-discriminative patterns (like biological V1)

            // Apply Gabor filter for each orientation column directly to raw image
            // Store responses per orientation column
            std::vector<std::vector<std::pair<int, double>>> orientationActivations(NUM_ORIENTATIONS);
            std::vector<double> orientationStrengths(NUM_ORIENTATIONS, 0.0);  // Total response per orientation

            for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
                // Apply Gabor filter for this orientation to raw image pixels
                auto orientationResponse = applyGaborFilter(img.pixels, gaborKernels[ori]);

                // Collect activations for this orientation column
                for (int spatial_idx = 0; spatial_idx < NEURONS_PER_ORIENTATION; ++spatial_idx) {
                    double activation = orientationResponse[spatial_idx];
                    orientationStrengths[ori] += activation;  // Track total response

                    if (activation > 0.05) {  // Lower threshold for Gabor responses
                        int neuron_idx = ori * NEURONS_PER_ORIENTATION + spatial_idx;
                        orientationActivations[ori].push_back({neuron_idx, activation});
                    }
                }

                // Sort this orientation column by activation (highest first)
                std::sort(orientationActivations[ori].begin(), orientationActivations[ori].end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
            }

            // Fire neurons ONLY from the TOP 3 strongest orientation columns
            // This creates highly selective, discriminative patterns!

            // Rank orientations by strength
            std::vector<std::pair<int, double>> orientationRanking;
            for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
                orientationRanking.push_back({ori, orientationStrengths[ori]});
            }
            std::sort(orientationRanking.begin(), orientationRanking.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });

            // Debug: Print orientation strengths for first few images
            if (idx <= 2) {
                std::cout << "  [GABOR DEBUG] Image " << idx << " (label=" << label << ") orientation strengths:" << std::endl;
                for (int rank = 0; rank < NUM_ORIENTATIONS; ++rank) {
                    int ori = orientationRanking[rank].first;
                    double orientation = ori * (180.0 / NUM_ORIENTATIONS);
                    std::cout << "    #" << (rank+1) << ": " << orientation << "°: " << std::fixed << std::setprecision(2)
                              << orientationStrengths[ori] << " (active neurons: " << orientationActivations[ori].size() << ")"
                              << (rank < 3 ? " [FIRE]" : " [skip]") << std::endl;
                }
            }

            // Fire neurons ONLY from orientation columns with STRONG responses
            // Calculate mean and std
            double meanStrength = 0.0;
            for (double s : orientationStrengths) meanStrength += s;
            meanStrength /= NUM_ORIENTATIONS;

            double stdStrength = 0.0;
            for (double s : orientationStrengths) {
                stdStrength += (s - meanStrength) * (s - meanStrength);
            }
            stdStrength = sqrt(stdStrength / NUM_ORIENTATIONS);

            // Only fire from orientations above mean
            double threshold = meanStrength;

            int totalNeuronsFired = 0;
            const int neuronsPerColumn = NEURONS_PER_ORIENTATION / 5;  // 20% of 64 = ~13 neurons per column

            for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
                // Skip weak orientation columns!
                if (orientationStrengths[ori] < threshold) {
                    continue;
                }

                int numToFire = std::min(neuronsPerColumn, static_cast<int>(orientationActivations[ori].size()));

                totalNeuronsFired += numToFire;

                for (int i = 0; i < numToFire; ++i) {
                    int idx_v1 = orientationActivations[ori][i].first;
                    double strength = orientationActivations[ori][i].second;

                    // Each neuron fires its UNIQUE intrinsic temporal signature
                    // The signature is seeded by neuron ID, so each neuron has a different pattern
                    // Firing time is based on orientation column and neuron index for temporal separation
                    double baseTime = currentTime + 2.0 + (ori * 10.0) + (i * 0.5);

                    // Fire the neuron's intrinsic signature pattern
                    v1HiddenNeurons[idx_v1]->fireSignature(baseTime);

                    // Fire and acknowledge for STDP
                    v1HiddenNeurons[idx_v1]->fireAndAcknowledge(baseTime);
                    networkPropagator->fireNeuron(v1HiddenNeurons[idx_v1]->getId(), baseTime);

                    // Learn the firing pattern
                    v1HiddenNeurons[idx_v1]->learnCurrentPattern();
                }
            }

            if (idx <= 2) {
                std::cout << "  Total neurons fired: " << totalNeuronsFired << std::endl;
            }

            // STEP 3: SUPERVISED OUTPUT LAYER LEARNING (population-based classification)
            // IMPORTANT: Each neuron in the population learns from DIFFERENT examples
            // to create diverse "fingerprint" patterns for better discrimination

            // Determine which neuron in the population should learn this example
            // Distribute examples round-robin across all neurons in the population (10 neurons per digit)
            int neuronIndex = idx % outputPopulations[label].size();
            auto& targetNeuron = outputPopulations[label][neuronIndex];

            // Copy V1 spike pattern to ONLY the selected neuron
            copyLayerSpikePattern(v1HiddenNeurons, {targetNeuron});

            // Fire ONLY the selected neuron as a teaching signal
            targetNeuron->fireAndAcknowledge(currentTime + 3.0);
            networkPropagator->fireNeuron(targetNeuron->getId(), currentTime + 3.0);

            // Apply reward-modulated STDP to strengthen V1→output synapses
            networkPropagator->applyRewardModulatedSTDP(targetNeuron->getId(), 1.5);

            // Selected neuron learns the V1 spike pattern
            targetNeuron->learnCurrentPattern();

            // Debug: Print pattern learning info for first training image
            if (idx == 0) {
                std::cout << "\n=== DETAILED PATTERN DEBUG (Training Image 0, Label=" << label << ") ===\n";

                // Check V1 spike pattern
                std::cout << "V1 Spike Pattern:\n";
                int v1SpikeCount = 0;
                double v1MinTime = 1e9, v1MaxTime = -1e9;
                for (const auto& neuron : v1HiddenNeurons) {
                    auto spikes = neuron->getSpikes();
                    v1SpikeCount += spikes.size();
                    for (double t : spikes) {
                        if (t < v1MinTime) v1MinTime = t;
                        if (t > v1MaxTime) v1MaxTime = t;
                    }
                }
                std::cout << "  Total V1 spikes: " << v1SpikeCount << "\n";
                std::cout << "  Time range: " << v1MinTime << "ms to " << v1MaxTime << "ms\n";

                // Check V2 spike pattern
                std::cout << "\nV2 Spike Pattern:\n";
                int v2SpikeCount = 0;
                double v2MinTime = 1e9, v2MaxTime = -1e9;
                for (const auto& neuron : v2HiddenNeurons) {
                    auto spikes = neuron->getSpikes();
                    v2SpikeCount += spikes.size();
                    for (double t : spikes) {
                        if (t < v2MinTime) v2MinTime = t;
                        if (t > v2MaxTime) v2MaxTime = t;
                    }
                }
                std::cout << "  Total V2 spikes: " << v2SpikeCount << "\n";
                std::cout << "  Time range: " << v2MinTime << "ms to " << v2MaxTime << "ms\n";

                // Check output neuron patterns after learning
                std::cout << "\nDigit " << label << " Output Neurons (after learning):\n";
                for (int n = 0; n < 3 && n < outputPopulations[label].size(); ++n) {
                    auto neuron = outputPopulations[label][n];
                    auto currentSpikes = neuron->getSpikes();
                    auto learnedPatterns = neuron->getReferencePatterns();

                    std::cout << "  Neuron " << n << ":\n";
                    std::cout << "    Current spikes: " << currentSpikes.size() << "\n";
                    if (!currentSpikes.empty()) {
                        std::cout << "    Current spike times (first 5): ";
                        for (int s = 0; s < 5 && s < currentSpikes.size(); ++s) {
                            std::cout << currentSpikes[s] << " ";
                        }
                        std::cout << "\n";
                    }
                    std::cout << "    Learned patterns: " << learnedPatterns.size() << "\n";
                    if (!learnedPatterns.empty()) {
                        std::cout << "    Pattern 0 total spikes: " << learnedPatterns[0].getTotalSpikes() << "\n";
                    }
                }
                std::cout << "=== END PATTERN DEBUG ===\n\n";
            }

            // Advance simulation time (5ms per image)
            currentTime += 5.0;

            // Apply homeostatic plasticity every 100 images
            // This helps balance neuron firing rates over time
            if ((idx + 1) % 100 == 0) {
                for (const auto& neuron : v1HiddenNeurons) {
                    neuron->applyHomeostaticPlasticity();
                }
                for (const auto& population : outputPopulations) {
                    for (const auto& neuron : population) {
                        neuron->applyHomeostaticPlasticity();
                    }
                }
            }

            // Periodic memory cleanup every 500 images to prevent memory leaks
            if ((idx + 1) % 500 == 0) {
                for (const auto& neuron : retina1->getNeurons()) {
                    neuron->periodicMemoryCleanup(currentTime);
                }
                for (const auto& neuron : retina2->getNeurons()) {
                    neuron->periodicMemoryCleanup(currentTime);
                }
                for (const auto& neuron : retina3->getNeurons()) {
                    neuron->periodicMemoryCleanup(currentTime);
                }
                for (const auto& neuron : interneurons1) {
                    neuron->periodicMemoryCleanup(currentTime);
                }
                for (const auto& neuron : interneurons2) {
                    neuron->periodicMemoryCleanup(currentTime);
                }
                for (const auto& neuron : interneurons3) {
                    neuron->periodicMemoryCleanup(currentTime);
                }
                for (const auto& neuron : v1HiddenNeurons) {
                    neuron->periodicMemoryCleanup(currentTime);
                }
                for (const auto& population : outputPopulations) {
                    for (const auto& neuron : population) {
                        neuron->periodicMemoryCleanup(currentTime);
                    }
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

        // Analyze final weight distribution
        std::cout << "\n=== Final Weight Distribution ===" << std::endl;
        analyzeWeightDistribution(allSynapsesMap, "All Synapses");

        // Debug: Check output neuron patterns before testing
        std::cout << "\n=== Debug: Output Neuron Patterns ===\n";
        for (int digit = 0; digit < 3; ++digit) {
            int patternCount = outputPopulations[digit][0]->getReferencePatterns().size();
            std::cout << "  Digit " << digit << ": " << patternCount << " patterns learned\n";
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

            // Fire V1 neurons based on ORIENTATION-SELECTIVE GABOR FILTERING (same as training!)
            // Apply Gabor filter for each orientation column directly to raw image
            std::vector<std::vector<std::pair<int, double>>> orientationActivations(NUM_ORIENTATIONS);
            std::vector<double> orientationStrengths(NUM_ORIENTATIONS, 0.0);

            for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
                // Apply Gabor filter for this orientation to raw image pixels
                auto orientationResponse = applyGaborFilter(img.pixels, gaborKernels[ori]);

                // Collect activations for this orientation column
                for (int spatial_idx = 0; spatial_idx < NEURONS_PER_ORIENTATION; ++spatial_idx) {
                    double activation = orientationResponse[spatial_idx];
                    orientationStrengths[ori] += activation;

                    if (activation > 0.05) {
                        int neuron_idx = ori * NEURONS_PER_ORIENTATION + spatial_idx;
                        orientationActivations[ori].push_back({neuron_idx, activation});
                    }
                }

                // Sort this orientation column by activation
                std::sort(orientationActivations[ori].begin(), orientationActivations[ori].end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
            }

            // Rank orientations by strength (same as training!)
            std::vector<std::pair<int, double>> orientationRanking;
            for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
                orientationRanking.push_back({ori, orientationStrengths[ori]});
            }
            std::sort(orientationRanking.begin(), orientationRanking.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });

            // Fire neurons ONLY from orientation columns with STRONG responses (same as training!)
            double meanStrength = 0.0;
            for (double s : orientationStrengths) meanStrength += s;
            meanStrength /= NUM_ORIENTATIONS;

            double stdStrength = 0.0;
            for (double s : orientationStrengths) {
                stdStrength += (s - meanStrength) * (s - meanStrength);
            }
            stdStrength = sqrt(stdStrength / NUM_ORIENTATIONS);
            double threshold = meanStrength;

            const int neuronsPerColumn = NEURONS_PER_ORIENTATION / 5;  // 20% of 64 = ~13 neurons per column

            for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
                if (orientationStrengths[ori] < threshold) {
                    continue;
                }

                int numToFire = std::min(neuronsPerColumn, static_cast<int>(orientationActivations[ori].size()));

                for (int j = 0; j < numToFire; ++j) {
                    int idx_v1 = orientationActivations[ori][j].first;
                    double strength = orientationActivations[ori][j].second;

                    // Each neuron fires its UNIQUE intrinsic temporal signature (same as training)
                    double baseTime = currentTime + 2.0 + (ori * 10.0) + (j * 0.5);

                    // Fire the neuron's intrinsic signature pattern
                    v1HiddenNeurons[idx_v1]->fireSignature(baseTime);

                    v1HiddenNeurons[idx_v1]->fireAndAcknowledge(baseTime);
                    networkPropagator->fireNeuron(v1HiddenNeurons[idx_v1]->getId(), baseTime);
                }
            }

            // Debug: Check V1 spikes for first 10 test images (AFTER firing)
            if (i < 10) {
                int v1SpikeCount = 0;
                int v1ActiveNeurons = 0;
                for (const auto& neuron : v1HiddenNeurons) {
                    int spikes = neuron->getSpikes().size();
                    v1SpikeCount += spikes;
                    if (spikes > 0) v1ActiveNeurons++;
                }
                std::cout << "  [DEBUG TEST] Image " << i << ": V1 spikes=" << v1SpikeCount
                          << ", V1 active neurons=" << v1ActiveNeurons << "/" << v1HiddenNeurons.size() << std::endl;
            }

            // Apply lateral inhibition to V1 hidden layer (sparse coding)
            applyLateralInhibition(v1HiddenNeurons, 20);  // Keep top 20% active

            // Reset inhibition for all output neurons in all populations
            for (const auto& population : outputPopulations) {
                for (const auto& neuron : population) {
                    neuron->resetInhibition();
                }
            }

            // Copy V1 spike pattern to ALL output populations for pattern matching
            // This is the same pattern they learned during training
            for (int digit = 0; digit < 10; ++digit) {
                copyLayerSpikePattern(v1HiddenNeurons, outputPopulations[digit]);
            }

            // Get POPULATION activations (average across each population)
            // Each population will match the current V1 pattern against its learned patterns
            std::vector<double> populationActivations(10, 0.0);
            std::vector<double> populationBestSim(10, 0.0);
            for (int digit = 0; digit < 10; ++digit) {
                double totalActivation = 0.0;
                double bestSim = -1.0;
                for (const auto& neuron : outputPopulations[digit]) {
                    totalActivation += neuron->getActivation();
                    double sim = neuron->getBestSimilarity();
                    if (sim > bestSim) bestSim = sim;
                }
                populationActivations[digit] = totalActivation / outputPopulations[digit].size();
                populationBestSim[digit] = bestSim;
            }

            // Debug: Print detailed pattern information for first test image
            if (i == 0) {
                std::cout << "\n=== DETAILED PATTERN DEBUG (Image 0) ===\n";

                // Check V1 spike pattern
                std::cout << "V1 Spike Pattern:\n";
                int v1SpikeCount = 0;
                double v1MinTime = 1e9, v1MaxTime = -1e9;
                for (const auto& neuron : v1HiddenNeurons) {
                    auto spikes = neuron->getSpikes();
                    v1SpikeCount += spikes.size();
                    for (double t : spikes) {
                        if (t < v1MinTime) v1MinTime = t;
                        if (t > v1MaxTime) v1MaxTime = t;
                    }
                }
                std::cout << "  Total V1 spikes: " << v1SpikeCount << "\n";
                std::cout << "  Time range: " << v1MinTime << "ms to " << v1MaxTime << "ms\n";

                // Check V2 spike pattern
                std::cout << "\nV2 Spike Pattern:\n";
                int v2SpikeCount = 0;
                double v2MinTime = 1e9, v2MaxTime = -1e9;
                for (const auto& neuron : v2HiddenNeurons) {
                    auto spikes = neuron->getSpikes();
                    v2SpikeCount += spikes.size();
                    for (double t : spikes) {
                        if (t < v2MinTime) v2MinTime = t;
                        if (t > v2MaxTime) v2MaxTime = t;
                    }
                }
                std::cout << "  Total V2 spikes: " << v2SpikeCount << "\n";
                std::cout << "  Time range: " << v2MinTime << "ms to " << v2MaxTime << "ms\n";

                // Check output neuron patterns for digit 0
                std::cout << "\nDigit 0 Output Neurons:\n";
                for (int n = 0; n < 3 && n < outputPopulations[0].size(); ++n) {
                    auto neuron = outputPopulations[0][n];
                    auto currentSpikes = neuron->getSpikes();
                    auto learnedPatterns = neuron->getReferencePatterns();

                    std::cout << "  Neuron " << n << ":\n";
                    std::cout << "    Current spikes: " << currentSpikes.size() << "\n";
                    if (!currentSpikes.empty()) {
                        std::cout << "    Current spike times (first 5): ";
                        for (int s = 0; s < 5 && s < currentSpikes.size(); ++s) {
                            std::cout << currentSpikes[s] << " ";
                        }
                        std::cout << "\n";
                    }
                    std::cout << "    Learned patterns: " << learnedPatterns.size() << "\n";
                    if (!learnedPatterns.empty()) {
                        std::cout << "    Pattern 0 total spikes: " << learnedPatterns[0].getTotalSpikes() << "\n";
                    }
                    std::cout << "    Best similarity: " << neuron->getBestSimilarity() << "\n";
                    std::cout << "    Activation: " << neuron->getActivation() << "\n";
                }
                std::cout << "=== END PATTERN DEBUG ===\n\n";
            }

            // Debug: Print best similarities for first 3 test images
            if (i < 3) {
                std::cout << "  [DEBUG SIM] Image " << i << ": ";
                for (int digit = 0; digit < 10; ++digit) {
                    std::cout << "D" << digit << "=" << std::fixed << std::setprecision(3)
                              << populationBestSim[digit] << " ";
                }
                std::cout << std::endl;
            }

            // Debug: Print population activations for first 10 test images
            if (i < 10) {
                std::cout << "  Test image " << i << " (label=" << trueLabel << "): ";
                for (int d = 0; d < 10; ++d) {
                    std::cout << "D" << d << "=" << std::fixed << std::setprecision(3)
                              << populationActivations[d] << " ";
                }
                std::cout << std::endl;
            }

            // Find the winner population (highest similarity, not activation)
            // This avoids the problem where all activations are 1.0 after thresholding
            int predicted = 0;
            double maxSimilarity = populationBestSim[0];
            for (int d = 1; d < 10; ++d) {
                if (populationBestSim[d] > maxSimilarity) {
                    maxSimilarity = populationBestSim[d];
                    predicted = d;
                }
            }

            // Apply winner-take-all inhibition BETWEEN populations
            // Suppress all neurons in non-winner populations
            double inhibitionStrength = 0.5;  // Inhibit by 50% of winner's similarity
            for (int d = 0; d < 10; ++d) {
                if (d != predicted) {
                    for (const auto& neuron : outputPopulations[d]) {
                        neuron->applyInhibition(maxSimilarity * inhibitionStrength);
                    }
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

