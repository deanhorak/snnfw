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
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>

using namespace snnfw;

// Optimized configuration
const int GRID_SIZE = 7;
const int REGION_SIZE = 4;  // 28/7 = 4
const int NUM_REGIONS = GRID_SIZE * GRID_SIZE;  // 49
const int NUM_ORIENTATIONS = 8;
const int NEURONS_PER_FEATURE = 1;  // Back to 1 neuron per feature (5 didn't help)
const int NUM_FEATURES = NUM_REGIONS * NUM_ORIENTATIONS * NEURONS_PER_FEATURE;  // 49 * 8 * 1 = 392
const int TRAIN_PER_DIGIT = 5000;  // Use more of the ~6000 available per digit
const int TEST_IMAGES = 10000;     // Full test set
const double EDGE_THRESHOLD = 0.15;

// Temporal encoding parameters
const double TEMPORAL_WINDOW = 200.0;  // Increased from 50ms to 200ms for better temporal resolution

// k-NN classification parameters
const int K_NEIGHBORS = 5;  // Number of nearest neighbors to consider for voting

// Extract region from image
std::vector<uint8_t> extractRegion(const MNISTLoader::Image& img, int regionRow, int regionCol) {
    std::vector<uint8_t> region;
    int startRow = regionRow * REGION_SIZE;
    int startCol = regionCol * REGION_SIZE;
    
    for (int r = 0; r < REGION_SIZE; ++r) {
        for (int c = 0; c < REGION_SIZE; ++c) {
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
std::vector<std::vector<double>> extractEdgeFeatures(const std::vector<uint8_t>& region) {
    std::vector<std::vector<double>> features(NUM_ORIENTATIONS);
    
    for (int r = 1; r < REGION_SIZE - 1; ++r) {
        for (int c = 1; c < REGION_SIZE - 1; ++c) {
            double center = region[r * REGION_SIZE + c] / 255.0;
            double left = region[r * REGION_SIZE + (c-1)] / 255.0;
            double right = region[r * REGION_SIZE + (c+1)] / 255.0;
            double top = region[(r-1) * REGION_SIZE + c] / 255.0;
            double bottom = region[(r+1) * REGION_SIZE + c] / 255.0;
            double topLeft = region[(r-1) * REGION_SIZE + (c-1)] / 255.0;
            double topRight = region[(r-1) * REGION_SIZE + (c+1)] / 255.0;
            double bottomLeft = region[(r+1) * REGION_SIZE + (c-1)] / 255.0;
            double bottomRight = region[(r+1) * REGION_SIZE + (c+1)] / 255.0;
            
            // 0 degrees (horizontal)
            double edge0 = std::abs(left - right);
            if (edge0 > EDGE_THRESHOLD) features[0].push_back(edge0);
            
            // 90 degrees (vertical)
            double edge90 = std::abs(top - bottom);
            if (edge90 > EDGE_THRESHOLD) features[1].push_back(edge90);
            
            // 45 degrees (diagonal)
            double edge45 = std::abs(topLeft - bottomRight);
            if (edge45 > EDGE_THRESHOLD) features[2].push_back(edge45);
            
            // 135 degrees (diagonal)
            double edge135 = std::abs(topRight - bottomLeft);
            if (edge135 > EDGE_THRESHOLD) features[3].push_back(edge135);
            
            // Intermediate orientations (approximate)
            // 22.5 degrees
            double edge22_5 = std::abs(0.924 * right + 0.383 * bottom - 0.924 * left - 0.383 * top);
            if (edge22_5 > EDGE_THRESHOLD) features[4].push_back(edge22_5);
            
            // 67.5 degrees
            double edge67_5 = std::abs(0.383 * right + 0.924 * bottom - 0.383 * left - 0.924 * top);
            if (edge67_5 > EDGE_THRESHOLD) features[5].push_back(edge67_5);
            
            // 112.5 degrees
            double edge112_5 = std::abs(-0.383 * right + 0.924 * bottom + 0.383 * left - 0.924 * top);
            if (edge112_5 > EDGE_THRESHOLD) features[6].push_back(edge112_5);
            
            // 157.5 degrees
            double edge157_5 = std::abs(-0.924 * right + 0.383 * bottom + 0.924 * left - 0.383 * top);
            if (edge157_5 > EDGE_THRESHOLD) features[7].push_back(edge157_5);
        }
    }
    
    return features;
}

// Convert features to spikes with improved temporal encoding
std::vector<double> featuresToSpikes(const std::vector<double>& features, double duration = TEMPORAL_WINDOW) {
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
    const std::vector<std::vector<std::vector<std::shared_ptr<Neuron>>>>& layer1Neurons
) {
    std::vector<double> activations(NUM_FEATURES, 0.0);

    int featureIdx = 0;
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            auto region = extractRegion(img, row, col);
            auto edgeFeatures = extractEdgeFeatures(region);

            for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
                auto spikes = featuresToSpikes(edgeFeatures[orient]);

                // Get activations from all neurons for this (region, orientation)
                for (int neuronIdx = 0; neuronIdx < NEURONS_PER_FEATURE; ++neuronIdx) {
                    auto& neuron = layer1Neurons[row * GRID_SIZE + col][orient][neuronIdx];

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

int main() {
    std::cout << "=== MNIST Optimized Configuration ===\n\n";
    
    // Load data - FULL DATABASE
    MNISTLoader trainLoader;
    trainLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
        60000  // Full training set
    );

    MNISTLoader testLoader;
    testLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-labels-idx1-ubyte",
        TEST_IMAGES  // Full test set (10000)
    );
    
    std::cout << "Configuration:\n";
    std::cout << "  Grid: " << GRID_SIZE << "x" << GRID_SIZE << " (" << NUM_REGIONS << " regions)\n";
    std::cout << "  Region size: " << REGION_SIZE << "x" << REGION_SIZE << " pixels\n";
    std::cout << "  Orientations: " << NUM_ORIENTATIONS << "\n";
    std::cout << "  Neurons per feature: " << NEURONS_PER_FEATURE << "\n";
    std::cout << "  Total neurons: " << (NUM_REGIONS * NUM_ORIENTATIONS * NEURONS_PER_FEATURE) << "\n";
    std::cout << "  Total features: " << NUM_FEATURES << "\n";
    std::cout << "  Temporal window: " << TEMPORAL_WINDOW << "ms\n";
    std::cout << "  k-NN neighbors: " << K_NEIGHBORS << "\n";
    std::cout << "  Training examples per digit: " << TRAIN_PER_DIGIT << "\n";
    std::cout << "  Edge threshold: " << EDGE_THRESHOLD << "\n\n";

    std::cout << "Loaded " << trainLoader.size() << " training images\n";
    std::cout << "Loaded " << testLoader.size() << " test images\n\n";

    // Create Layer 1: Feature detection neurons (shared)
    // Structure: [region][orientation][neuron_index]
    std::vector<std::vector<std::vector<std::shared_ptr<Neuron>>>> layer1Neurons(NUM_REGIONS);
    int neuronId = 0;
    for (int region = 0; region < NUM_REGIONS; ++region) {
        layer1Neurons[region].resize(NUM_ORIENTATIONS);
        for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
            for (int neuronIdx = 0; neuronIdx < NEURONS_PER_FEATURE; ++neuronIdx) {
                auto neuron = std::make_shared<Neuron>(
                    TEMPORAL_WINDOW,  // 200ms window for better temporal resolution
                    0.6,              // Threshold
                    100,              // Max 100 patterns per neuron
                    neuronId++
                );
                layer1Neurons[region][orient].push_back(neuron);
            }
        }
    }
    
    // Train Layer 1
    std::cout << "=== Training Layer 1 ===\n";
    std::vector<int> trainCount(10, 0);
    
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;
        
        if (trainCount[label] >= TRAIN_PER_DIGIT) continue;
        
        for (int row = 0; row < GRID_SIZE; ++row) {
            for (int col = 0; col < GRID_SIZE; ++col) {
                auto region = extractRegion(img, row, col);
                auto edgeFeatures = extractEdgeFeatures(region);

                for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
                    auto spikes = featuresToSpikes(edgeFeatures[orient]);

                    if (spikes.empty()) continue;

                    // Train all neurons for this (region, orientation)
                    for (int neuronIdx = 0; neuronIdx < NEURONS_PER_FEATURE; ++neuronIdx) {
                        auto& neuron = layer1Neurons[row * GRID_SIZE + col][orient][neuronIdx];

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
            std::cout << "  Trained " << totalTrained << " examples";
            std::cout.flush();
        }
    }
    
    // Store activation patterns for each digit
    std::cout << "\n\n=== Learning Activation Patterns ===\n";
    std::vector<std::vector<std::vector<double>>> digitActivationPatterns(10);
    std::fill(trainCount.begin(), trainCount.end(), 0);
    
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;
        
        if (trainCount[label] >= TRAIN_PER_DIGIT) continue;
        
        auto activations = getLayer1Activations(img, layer1Neurons);
        digitActivationPatterns[label].push_back(activations);
        
        trainCount[label]++;
        
        int totalTrained = 0;
        for (int d = 0; d < 10; ++d) totalTrained += trainCount[d];
        if (totalTrained % 100 == 0) {
            std::cout << "  Learned " << totalTrained << " patterns";
            std::cout.flush();
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
        
        auto testActivations = getLayer1Activations(img, layer1Neurons);

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
        for (int k = 0; k < K_NEIGHBORS && k < similarities.size(); ++k) {
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

