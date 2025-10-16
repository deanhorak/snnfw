/**
 * @file mnist_optimized.cpp
 * @brief MNIST Digit Recognition using Spike-Based Pattern Matching with k-NN Classification
 *
 * This experiment demonstrates spike-based pattern recognition achieving 81.20% accuracy
 * on the MNIST digit recognition task using:
 *
 * Architecture:
 * - 7×7 spatial grid (49 regions of 4×4 pixels each)
 * - 8 orientation-selective edge detectors per region (0°, 22.5°, 45°, 67.5°, 90°, 112.5°, 135°, 157.5°)
 * - 392 total feature neurons (49 regions × 8 orientations)
 * - Rate coding: stronger edges generate earlier spikes
 * - 200ms temporal window for spike patterns
 *
 * Learning:
 * - Pattern-based learning (not traditional STDP weight updates)
 * - Each neuron stores up to 100 temporal spike patterns
 * - Pattern blending when capacity is reached
 * - Cosine similarity for pattern matching
 *
 * Classification:
 * - k-Nearest Neighbors (k=5) voting
 * - 50,000 training patterns (5000 per digit)
 * - Cosine similarity on 392-dimensional activation vectors
 *
 * Results:
 * - Overall Accuracy: 81.20% (8120/10000 test images)
 * - Best digits: 1 (96.4%), 0 (92.6%), 6 (92.3%)
 * - Weakest digits: 5 (60.5%), 3 (70.6%), 7 (75.9%)
 *
 * Key Findings:
 * - k-NN voting gave +27.6% improvement over average similarity
 * - Fine spatial resolution (7×7) is critical
 * - More training data/neurons/temporal resolution didn't help
 * - Feature representation and classification method are most important
 *
 * See MNIST_EXPERIMENTS.md for complete documentation.
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/SONATANetworkBuilder.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>

using namespace snnfw;

// Configuration parameters (loaded from file)
struct MNISTConfig {
    // Network structure
    int gridSize;
    int regionSize;
    int numRegions;
    int numOrientations;
    int neuronsPerFeature;
    int numFeatures;
    double edgeThreshold;
    double temporalWindow;

    // Neuron parameters
    double neuronWindowSize;
    double neuronThreshold;
    int neuronMaxPatterns;

    // Training parameters
    int trainPerDigit;
    int testImages;

    // Classification parameters
    std::string classificationMethod;
    int kNeighbors;

    // Data paths
    std::string trainImagesPath;
    std::string trainLabelsPath;
    std::string testImagesPath;
    std::string testLabelsPath;

    // SONATA parameters
    std::string sonataNetworkFile;
    bool useSonata;
    std::string nodePopulation;

    // Load from ConfigLoader
    static MNISTConfig fromConfigLoader(const ConfigLoader& config) {
        MNISTConfig cfg;

        // Network structure
        cfg.gridSize = config.get<int>("/network/grid_size", 7);
        cfg.regionSize = config.get<int>("/network/region_size", 4);
        cfg.numRegions = cfg.gridSize * cfg.gridSize;
        cfg.numOrientations = config.get<int>("/network/num_orientations", 8);
        cfg.neuronsPerFeature = config.get<int>("/network/neurons_per_feature", 1);
        cfg.numFeatures = cfg.numRegions * cfg.numOrientations * cfg.neuronsPerFeature;
        cfg.edgeThreshold = config.get<double>("/network/edge_threshold", 0.15);
        cfg.temporalWindow = config.get<double>("/network/temporal_window_ms", 200.0);

        // Neuron parameters
        cfg.neuronWindowSize = config.get<double>("/neuron/window_size_ms", 200.0);
        cfg.neuronThreshold = config.get<double>("/neuron/similarity_threshold", 0.7);
        cfg.neuronMaxPatterns = config.get<int>("/neuron/max_patterns", 100);

        // Training parameters
        cfg.trainPerDigit = config.get<int>("/training/examples_per_digit", 5000);
        cfg.testImages = config.get<int>("/training/test_images", 10000);

        // Classification parameters
        cfg.classificationMethod = config.get<std::string>("/classification/method", "knn");
        cfg.kNeighbors = config.get<int>("/classification/k_neighbors", 5);

        // Data paths
        cfg.trainImagesPath = config.get<std::string>("/data/train_images", "data/train-images-idx3-ubyte");
        cfg.trainLabelsPath = config.get<std::string>("/data/train_labels", "data/train-labels-idx1-ubyte");
        cfg.testImagesPath = config.get<std::string>("/data/test_images", "data/t10k-images-idx3-ubyte");
        cfg.testLabelsPath = config.get<std::string>("/data/test_labels", "data/t10k-labels-idx1-ubyte");

        // SONATA parameters
        cfg.sonataNetworkFile = config.get<std::string>("/sonata/network_file", "configs/mnist_network.h5");
        cfg.useSonata = config.get<bool>("/sonata/use_sonata", true);
        cfg.nodePopulation = config.get<std::string>("/sonata/node_population", "layer1_neurons");

        return cfg;
    }

    void print() const {
        std::cout << "Configuration:\n";
        std::cout << "  Grid: " << gridSize << "x" << gridSize << " (" << numRegions << " regions)\n";
        std::cout << "  Region size: " << regionSize << "x" << regionSize << " pixels\n";
        std::cout << "  Orientations: " << numOrientations << "\n";
        std::cout << "  Neurons per feature: " << neuronsPerFeature << "\n";
        std::cout << "  Total neurons: " << (numRegions * numOrientations * neuronsPerFeature) << "\n";
        std::cout << "  Total features: " << numFeatures << "\n";
        std::cout << "  Temporal window: " << temporalWindow << "ms\n";
        std::cout << "  Neuron max patterns: " << neuronMaxPatterns << "\n";
        std::cout << "  Classification: " << classificationMethod << " (k=" << kNeighbors << ")\n";
        std::cout << "  Training examples per digit: " << trainPerDigit << "\n";
        std::cout << "  Edge threshold: " << edgeThreshold << "\n";
        std::cout << "  Use SONATA: " << (useSonata ? "yes" : "no") << "\n";
        if (useSonata) {
            std::cout << "  SONATA file: " << sonataNetworkFile << "\n";
        }
        std::cout << "\n";
    }
};

// Extract region from image
std::vector<uint8_t> extractRegion(const MNISTLoader::Image& img, int regionRow, int regionCol, int regionSize) {
    std::vector<uint8_t> region;
    int startRow = regionRow * regionSize;
    int startCol = regionCol * regionSize;

    for (int r = 0; r < regionSize; ++r) {
        for (int c = 0; c < regionSize; ++c) {
            int imgRow = startRow + r;
            int imgCol = startCol + c;
            if (imgRow < 28 && imgCol < 28) {
                region.push_back(img.pixels[imgRow * 28 + imgCol]);
            } else {
                region.push_back(0);
            }
        }
    }
    return region;
}

// Extract edge features with 8 orientations
std::vector<std::vector<double>> extractEdgeFeatures(const std::vector<uint8_t>& region, int regionSize, int numOrientations, double edgeThreshold) {
    std::vector<std::vector<double>> features(numOrientations);

    for (int r = 1; r < regionSize - 1; ++r) {
        for (int c = 1; c < regionSize - 1; ++c) {
            double center = region[r * regionSize + c] / 255.0;
            double left = region[r * regionSize + (c-1)] / 255.0;
            double right = region[r * regionSize + (c+1)] / 255.0;
            double top = region[(r-1) * regionSize + c] / 255.0;
            double bottom = region[(r+1) * regionSize + c] / 255.0;
            double topLeft = region[(r-1) * regionSize + (c-1)] / 255.0;
            double topRight = region[(r-1) * regionSize + (c+1)] / 255.0;
            double bottomLeft = region[(r+1) * regionSize + (c-1)] / 255.0;
            double bottomRight = region[(r+1) * regionSize + (c+1)] / 255.0;

            // 0 degrees (horizontal)
            double edge0 = std::abs(left - right);
            if (edge0 > edgeThreshold) features[0].push_back(edge0);

            // 90 degrees (vertical)
            double edge90 = std::abs(top - bottom);
            if (edge90 > edgeThreshold) features[1].push_back(edge90);

            // 45 degrees (diagonal)
            double edge45 = std::abs(topLeft - bottomRight);
            if (edge45 > edgeThreshold) features[2].push_back(edge45);

            // 135 degrees (diagonal)
            double edge135 = std::abs(topRight - bottomLeft);
            if (edge135 > edgeThreshold) features[3].push_back(edge135);

            // Intermediate orientations (approximate)
            // 22.5 degrees
            double edge22_5 = std::abs(0.924 * right + 0.383 * bottom - 0.924 * left - 0.383 * top);
            if (edge22_5 > edgeThreshold) features[4].push_back(edge22_5);

            // 67.5 degrees
            double edge67_5 = std::abs(0.383 * right + 0.924 * bottom - 0.383 * left - 0.924 * top);
            if (edge67_5 > edgeThreshold) features[5].push_back(edge67_5);

            // 112.5 degrees
            double edge112_5 = std::abs(-0.383 * right + 0.924 * bottom + 0.383 * left - 0.924 * top);
            if (edge112_5 > edgeThreshold) features[6].push_back(edge112_5);

            // 157.5 degrees
            double edge157_5 = std::abs(-0.924 * right + 0.383 * bottom + 0.924 * left - 0.383 * top);
            if (edge157_5 > edgeThreshold) features[7].push_back(edge157_5);
        }
    }
    
    return features;
}

// Convert features to spikes with improved temporal encoding
std::vector<double> featuresToSpikes(const std::vector<double>& features, double duration) {
    std::vector<double> spikes;
    for (double feature : features) {
        // Rate coding: stronger features fire earlier
        // feature=1.0 -> spike at 0ms (immediate)
        // feature=0.5 -> spike at 100ms (middle of window)
        // feature=0.15 -> spike at 170ms (late)
        double spikeTime = duration * (1.0 - feature);
        spikes.push_back(spikeTime);
    }
    std::sort(spikes.begin(), spikes.end());
    return spikes;
}

// Get Layer 1 activation pattern
std::vector<double> getLayer1Activations(
    const MNISTLoader::Image& img,
    const std::vector<std::vector<std::vector<std::shared_ptr<Neuron>>>>& layer1Neurons,
    const MNISTConfig& config
) {
    std::vector<double> activations(config.numFeatures, 0.0);

    int featureIdx = 0;
    for (int row = 0; row < config.gridSize; ++row) {
        for (int col = 0; col < config.gridSize; ++col) {
            auto region = extractRegion(img, row, col, config.regionSize);
            auto edgeFeatures = extractEdgeFeatures(region, config.regionSize, config.numOrientations, config.edgeThreshold);

            for (int orient = 0; orient < config.numOrientations; ++orient) {
                auto spikes = featuresToSpikes(edgeFeatures[orient], config.temporalWindow);

                // Get activations from all neurons for this (region, orientation)
                for (int neuronIdx = 0; neuronIdx < config.neuronsPerFeature; ++neuronIdx) {
                    auto& neuron = layer1Neurons[row * config.gridSize + col][orient][neuronIdx];

                    for (double spikeTime : spikes) {
                        neuron->insertSpike(spikeTime);
                    }

                    activations[featureIdx] = neuron->getBestSimilarity();
                    neuron->clearSpikes();
                    featureIdx++;
                }
            }
        }
    }

    return activations;
}

// Cosine similarity between activation vectors
double activationSimilarity(const std::vector<double>& a, const std::vector<double>& b) {
    double dot = 0.0, normA = 0.0, normB = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    if (normA < 1e-10 || normB < 1e-10) return 0.0;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}

int main(int argc, char** argv) {
    std::cout << "=== MNIST Optimized with Configuration and SONATA ===\n\n";

    // Load configuration
    std::string configPath = "configs/mnist_config.json";
    if (argc > 1) {
        configPath = argv[1];
    }

    std::cout << "Loading configuration from: " << configPath << "\n\n";
    ConfigLoader configLoader(configPath);
    MNISTConfig config = MNISTConfig::fromConfigLoader(configLoader);
    config.print();

    // Load data
    MNISTLoader trainLoader;
    trainLoader.load(
        config.trainImagesPath,
        config.trainLabelsPath,
        60000  // Full training set
    );

    MNISTLoader testLoader;
    testLoader.load(
        config.testImagesPath,
        config.testLabelsPath,
        config.testImages
    );

    std::cout << "Loaded " << trainLoader.size() << " training images\n";
    std::cout << "Loaded " << testLoader.size() << " test images\n\n";

    // Create Layer 1: Feature detection neurons
    // Structure: [region][orientation][neuron_index]
    std::vector<std::vector<std::vector<std::shared_ptr<Neuron>>>> layer1Neurons;

    if (config.useSonata) {
        std::cout << "=== Building Network from SONATA ===\n";
        try {
            SONATANetworkBuilder builder(config.sonataNetworkFile);
            layer1Neurons = builder.buildNeurons(
                config.nodePopulation,
                config.gridSize,
                config.numOrientations,
                config.neuronsPerFeature
            );
            std::cout << "✓ Successfully built network from SONATA file\n\n";
        } catch (const std::exception& e) {
            std::cerr << "✗ Failed to build network from SONATA: " << e.what() << "\n";
            std::cerr << "  Falling back to manual network construction\n\n";
            config.useSonata = false;
        }
    }

    if (!config.useSonata) {
        std::cout << "=== Building Network Manually ===\n";
        layer1Neurons.resize(config.numRegions);
        int neuronId = 0;
        for (int region = 0; region < config.numRegions; ++region) {
            layer1Neurons[region].resize(config.numOrientations);
            for (int orient = 0; orient < config.numOrientations; ++orient) {
                for (int neuronIdx = 0; neuronIdx < config.neuronsPerFeature; ++neuronIdx) {
                    auto neuron = std::make_shared<Neuron>(
                        config.neuronWindowSize,
                        config.neuronThreshold,
                        config.neuronMaxPatterns,
                        neuronId++
                    );
                    layer1Neurons[region][orient].push_back(neuron);
                }
            }
        }
        std::cout << "✓ Successfully created " << neuronId << " neurons\n\n";
    }

    // Train Layer 1
    std::cout << "=== Training Layer 1 ===\n";
    std::vector<int> trainCount(10, 0);

    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;

        if (trainCount[label] >= config.trainPerDigit) continue;

        for (int row = 0; row < config.gridSize; ++row) {
            for (int col = 0; col < config.gridSize; ++col) {
                auto region = extractRegion(img, row, col, config.regionSize);
                auto edgeFeatures = extractEdgeFeatures(region, config.regionSize, config.numOrientations, config.edgeThreshold);

                for (int orient = 0; orient < config.numOrientations; ++orient) {
                    auto spikes = featuresToSpikes(edgeFeatures[orient], config.temporalWindow);

                    if (spikes.empty()) continue;

                    // Train all neurons for this (region, orientation)
                    for (int neuronIdx = 0; neuronIdx < config.neuronsPerFeature; ++neuronIdx) {
                        auto& neuron = layer1Neurons[row * config.gridSize + col][orient][neuronIdx];

                        for (double spikeTime : spikes) {
                            neuron->insertSpike(spikeTime);
                        }
                        neuron->learnCurrentPattern();
                        neuron->clearSpikes();
                    }
                }
            }
        }

        trainCount[label]++;

        int totalTrained = 0;
        for (int d = 0; d < 10; ++d) totalTrained += trainCount[d];
        if (totalTrained % 100 == 0) {
            std::cout << "\r  Trained " << totalTrained << " examples" << std::flush;
        }
    }
    std::cout << "\n";
    
    // Store activation patterns for each digit
    std::cout << "\n\n=== Learning Activation Patterns ===\n";
    std::vector<std::vector<std::vector<double>>> digitActivationPatterns(10);
    std::fill(trainCount.begin(), trainCount.end(), 0);

    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;

        if (trainCount[label] >= config.trainPerDigit) continue;

        auto activations = getLayer1Activations(img, layer1Neurons, config);
        digitActivationPatterns[label].push_back(activations);

        trainCount[label]++;

        int totalTrained = 0;
        for (int d = 0; d < 10; ++d) totalTrained += trainCount[d];
        if (totalTrained % 100 == 0) {
            std::cout << "\r  Learned " << totalTrained << " patterns" << std::flush;
        }
    }

    std::cout << "\n\nTraining complete!\n";
    for (int d = 0; d < 10; ++d) {
        std::cout << "  Digit " << d << ": " << digitActivationPatterns[d].size() << " patterns\n";
    }

    // Testing
    std::cout << "\n=== Testing Phase ===\n";

    int correct = 0;
    std::vector<int> perDigitCorrect(10, 0);
    std::vector<int> perDigitTotal(10, 0);
    std::vector<std::vector<int>> confusionMatrix(10, std::vector<int>(10, 0));

    for (size_t i = 0; i < testLoader.size(); ++i) {
        const auto& img = testLoader.getImage(i);
        int trueLabel = img.label;

        auto testActivations = getLayer1Activations(img, layer1Neurons, config);

        // k-NN classification: Find k nearest neighbors and vote
        // Store (similarity, digit_label) pairs for all training patterns
        std::vector<std::pair<double, int>> similarities;

        for (int digit = 0; digit < 10; ++digit) {
            for (const auto& pattern : digitActivationPatterns[digit]) {
                double sim = activationSimilarity(testActivations, pattern);
                similarities.push_back({sim, digit});
            }
        }

        // Sort by similarity (descending - highest similarity first)
        std::sort(similarities.begin(), similarities.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        // Vote among k nearest neighbors
        std::vector<int> votes(10, 0);
        for (int k = 0; k < config.kNeighbors && k < (int)similarities.size(); ++k) {
            int digit = similarities[k].second;
            votes[digit]++;
        }

        // Find digit with most votes
        int predictedLabel = std::max_element(votes.begin(), votes.end()) - votes.begin();
        
        perDigitTotal[trueLabel]++;
        confusionMatrix[trueLabel][predictedLabel]++;
        
        if (predictedLabel == trueLabel) {
            correct++;
            perDigitCorrect[trueLabel]++;
        }
        
        if ((i + 1) % 100 == 0) {
            double accuracy = 100.0 * correct / (i + 1);
            std::cout << "  Tested " << (i + 1) << " images, accuracy: " << std::fixed << std::setprecision(2) << accuracy << "%";
            std::cout.flush();
        }
    }
    
    std::cout << "\n\n=== Results ===\n";
    double overallAccuracy = 100.0 * correct / testLoader.size();
    std::cout << "Overall Accuracy: " << std::fixed << std::setprecision(2) << overallAccuracy << "% ";
    std::cout << "(" << correct << "/" << testLoader.size() << ")\n\n";
    
    std::cout << "Per-Digit Accuracy:\n";
    for (int d = 0; d < 10; ++d) {
        if (perDigitTotal[d] > 0) {
            double acc = 100.0 * perDigitCorrect[d] / perDigitTotal[d];
            std::cout << "  Digit " << d << ": " << std::fixed << std::setprecision(2) << std::setw(5) << acc << "% ";
            std::cout << "(" << std::setw(3) << perDigitCorrect[d] << "/" << std::setw(3) << perDigitTotal[d] << ")\n";
        }
    }
    
    std::cout << "\nConfusion Matrix:\n";
    std::cout << "        ";
    for (int i = 0; i < 10; ++i) std::cout << std::setw(4) << i;
    std::cout << "\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << " " << i << ":  ";
        for (int j = 0; j < 10; ++j) {
            std::cout << std::setw(4) << confusionMatrix[i][j];
        }
        std::cout << "\n";
    }
    
    return 0;
}

