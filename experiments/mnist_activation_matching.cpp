/**
 * MNIST Activation Pattern Matching
 *
 * Different approach: Instead of converting Layer 1 activations to spikes,
 * directly match the 64-dimensional activation vector against learned patterns.
 * 
 * This is a hybrid: Layer 1 uses spike-based pattern matching, but Layer 2
 * uses direct vector similarity on the activation patterns.
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

// Configuration
const int GRID_SIZE = 4;
const int REGION_SIZE = 7;
const int NUM_REGIONS = GRID_SIZE * GRID_SIZE;  // 16
const int NUM_ORIENTATIONS = 4;
const int NUM_FEATURES = NUM_REGIONS * NUM_ORIENTATIONS;  // 64
const int TRAIN_PER_DIGIT = 30;
const int TEST_IMAGES = 100;

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

// Extract edge features
std::vector<std::vector<double>> extractEdgeFeatures(const std::vector<uint8_t>& region) {
    std::vector<std::vector<double>> features(NUM_ORIENTATIONS);
    const int size = REGION_SIZE;
    
    for (int r = 1; r < size - 1; ++r) {
        for (int c = 1; c < size - 1; ++c) {
            double left = region[r * size + (c-1)] / 255.0;
            double right = region[r * size + (c+1)] / 255.0;
            double horizEdge = std::abs(left - right);
            if (horizEdge > 0.2) features[0].push_back(horizEdge);
            
            double top = region[(r-1) * size + c] / 255.0;
            double bottom = region[(r+1) * size + c] / 255.0;
            double vertEdge = std::abs(top - bottom);
            if (vertEdge > 0.2) features[1].push_back(vertEdge);
            
            double topLeft = region[(r-1) * size + (c-1)] / 255.0;
            double bottomRight = region[(r+1) * size + (c+1)] / 255.0;
            double diag1Edge = std::abs(topLeft - bottomRight);
            if (diag1Edge > 0.2) features[2].push_back(diag1Edge);
            
            double topRight = region[(r-1) * size + (c+1)] / 255.0;
            double bottomLeft = region[(r+1) * size + (c-1)] / 255.0;
            double diag2Edge = std::abs(topRight - bottomLeft);
            if (diag2Edge > 0.2) features[3].push_back(diag2Edge);
        }
    }
    
    return features;
}

// Convert features to spikes
std::vector<double> featuresToSpikes(const std::vector<double>& features, double duration = 50.0) {
    std::vector<double> spikes;
    for (double feature : features) {
        double spikeTime = duration * (1.0 - feature);
        spikes.push_back(spikeTime);
    }
    std::sort(spikes.begin(), spikes.end());
    return spikes;
}

// Get Layer 1 activation pattern
std::vector<double> getLayer1Activations(
    const MNISTLoader::Image& img,
    const std::vector<std::vector<std::shared_ptr<Neuron>>>& layer1Neurons
) {
    std::vector<double> activations(NUM_FEATURES, 0.0);
    
    int featureIdx = 0;
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            auto region = extractRegion(img, row, col);
            auto edgeFeatures = extractEdgeFeatures(region);
            
            for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
                auto& neuron = layer1Neurons[row * GRID_SIZE + col][orient];
                auto spikes = featuresToSpikes(edgeFeatures[orient]);
                
                for (double spikeTime : spikes) {
                    neuron->insertSpike(spikeTime);
                }
                
                activations[featureIdx] = neuron->getBestSimilarity();
                neuron->clearSpikes();
                featureIdx++;
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
    std::cout << "=== MNIST Activation Pattern Matching ===\n\n";
    
    // Load data
    MNISTLoader trainLoader;
    trainLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
        300
    );
    
    MNISTLoader testLoader;
    testLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-labels-idx1-ubyte",
        TEST_IMAGES
    );
    
    std::cout << "Loaded " << trainLoader.size() << " training images\n";
    std::cout << "Loaded " << testLoader.size() << " test images\n";
    std::cout << "Feature vector size: " << NUM_FEATURES << "\n\n";
    
    // Create Layer 1: Feature detection neurons (shared)
    std::vector<std::vector<std::shared_ptr<Neuron>>> layer1Neurons(NUM_REGIONS);
    for (int region = 0; region < NUM_REGIONS; ++region) {
        for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
            auto neuron = std::make_shared<Neuron>(
                50.0,   // 50ms window
                0.6,    // Threshold
                20,     // Max patterns
                region * NUM_ORIENTATIONS + orient
            );
            layer1Neurons[region].push_back(neuron);
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
                    auto& neuron = layer1Neurons[row * GRID_SIZE + col][orient];
                    auto spikes = featuresToSpikes(edgeFeatures[orient]);
                    
                    if (spikes.empty()) continue;
                    
                    for (double spikeTime : spikes) {
                        neuron->insertSpike(spikeTime);
                    }
                    neuron->learnCurrentPattern();
                    neuron->clearSpikes();
                }
            }
        }
        
        trainCount[label]++;
        
        int totalTrained = 0;
        for (int d = 0; d < 10; ++d) totalTrained += trainCount[d];
        if (totalTrained % 30 == 0) {
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
        if (totalTrained % 30 == 0) {
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
    
    for (size_t i = 0; i < testLoader.size(); ++i) {
        const auto& img = testLoader.getImage(i);
        int trueLabel = img.label;
        
        auto testActivations = getLayer1Activations(img, layer1Neurons);
        
        // Find best matching digit
        int predictedLabel = -1;
        double bestSimilarity = -1.0;
        std::vector<double> avgSimilarities(10, 0.0);
        
        for (int digit = 0; digit < 10; ++digit) {
            double totalSim = 0.0;
            for (const auto& pattern : digitActivationPatterns[digit]) {
                double sim = activationSimilarity(testActivations, pattern);
                totalSim += sim;
            }
            double avgSim = totalSim / digitActivationPatterns[digit].size();
            avgSimilarities[digit] = avgSim;
            
            if (avgSim > bestSimilarity) {
                bestSimilarity = avgSim;
                predictedLabel = digit;
            }
        }
        
        // Debug first few
        if (i < 3) {
            std::cout << "\nTest image " << i << " (true label: " << trueLabel << "):\n";
            std::cout << "  Avg similarities: ";
            for (int d = 0; d < 10; ++d) {
                std::cout << d << ":" << std::fixed << std::setprecision(3) << avgSimilarities[d] << " ";
            }
            std::cout << "\n  Predicted: " << predictedLabel << " (sim=" << bestSimilarity << ")\n";
        }
        
        perDigitTotal[trueLabel]++;
        
        if (predictedLabel == trueLabel) {
            correct++;
            perDigitCorrect[trueLabel]++;
        }
        
        if ((i + 1) % 10 == 0) {
            double accuracy = 100.0 * correct / (i + 1);
            std::cout << "  Tested " << (i + 1) << " images, accuracy: " << std::fixed << std::setprecision(3) << accuracy << "%";
            std::cout.flush();
        }
    }
    
    std::cout << "\n\n=== Results ===\n";
    double overallAccuracy = 100.0 * correct / testLoader.size();
    std::cout << "Overall Accuracy: " << std::fixed << std::setprecision(3) << overallAccuracy << "% ";
    std::cout << "(" << correct << "/" << testLoader.size() << ")\n\n";
    
    std::cout << "Per-Digit Accuracy:\n";
    for (int d = 0; d < 10; ++d) {
        if (perDigitTotal[d] > 0) {
            double acc = 100.0 * perDigitCorrect[d] / perDigitTotal[d];
            std::cout << "  Digit " << d << ": " << std::fixed << std::setprecision(3) << acc << "% ";
            std::cout << "(" << perDigitCorrect[d] << "/" << perDigitTotal[d] << ")\n";
        }
    }
    
    return 0;
}

