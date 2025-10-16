/**
 * MNIST Two-Layer Network - Hierarchical feature learning
 *
 * Architecture:
 * - Layer 1 (Feature Layer): 16 regions × 4 orientations = 64 feature detectors
 * - Layer 2 (Integration Layer): 10 output neurons (one per digit)
 * 
 * Training:
 * - Layer 1 learns edge patterns in each region/orientation
 * - Layer 2 learns which Layer 1 activation patterns correspond to each digit
 * 
 * Testing:
 * - Layer 1 produces activation pattern (similarity scores)
 * - Layer 2 matches this activation pattern against learned digit patterns
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
const int TRAIN_PER_DIGIT = 20;  // More training examples for better learning
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

// Extract edge features from region
std::vector<std::vector<double>> extractEdgeFeatures(const std::vector<uint8_t>& region) {
    std::vector<std::vector<double>> features(NUM_ORIENTATIONS);
    const int size = REGION_SIZE;
    
    for (int r = 1; r < size - 1; ++r) {
        for (int c = 1; c < size - 1; ++c) {
            int idx = r * size + c;
            
            // Horizontal edge
            double left = region[r * size + (c-1)] / 255.0;
            double right = region[r * size + (c+1)] / 255.0;
            double horizEdge = std::abs(left - right);
            if (horizEdge > 0.2) features[0].push_back(horizEdge);
            
            // Vertical edge
            double top = region[(r-1) * size + c] / 255.0;
            double bottom = region[(r+1) * size + c] / 255.0;
            double vertEdge = std::abs(top - bottom);
            if (vertEdge > 0.2) features[1].push_back(vertEdge);
            
            // Diagonal edges
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

// Convert features to spike pattern
std::vector<double> featuresToSpikes(const std::vector<double>& features, double duration = 50.0) {
    std::vector<double> spikes;
    for (double feature : features) {
        double spikeTime = duration * (1.0 - feature);
        spikes.push_back(spikeTime);
    }
    std::sort(spikes.begin(), spikes.end());
    return spikes;
}

// Get Layer 1 activation pattern for an image
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
                
                // Insert spikes and get similarity
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

// Convert activation pattern to spike pattern for Layer 2
// Higher activations → earlier spikes
std::vector<double> activationsToSpikes(const std::vector<double>& activations, double duration = 50.0) {
    std::vector<double> spikes;
    
    for (size_t i = 0; i < activations.size(); ++i) {
        double activation = activations[i];
        
        // Only create spikes for significant activations
        if (activation > 0.3) {
            // Higher activation → earlier spike
            double spikeTime = duration * (1.0 - activation);
            spikes.push_back(spikeTime);
        }
    }
    
    std::sort(spikes.begin(), spikes.end());
    return spikes;
}

int main() {
    std::cout << "=== MNIST Two-Layer Network ===\n\n";
    
    // Load data
    MNISTLoader trainLoader;
    trainLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
        200
    );
    
    MNISTLoader testLoader;
    testLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-labels-idx1-ubyte",
        TEST_IMAGES
    );
    
    std::cout << "Loaded " << trainLoader.size() << " training images\n";
    std::cout << "Loaded " << testLoader.size() << " test images\n";
    std::cout << "Architecture:\n";
    std::cout << "  Layer 1: " << NUM_FEATURES << " feature neurons (" << NUM_REGIONS << " regions × " << NUM_ORIENTATIONS << " orientations)\n";
    std::cout << "  Layer 2: 10 integration neurons (one per digit)\n\n";
    
    // Create Layer 1: Feature detection neurons (shared across all digits)
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
    
    // Create Layer 2: Integration neurons (one per digit)
    std::vector<std::shared_ptr<Neuron>> layer2Neurons(10);
    for (int digit = 0; digit < 10; ++digit) {
        layer2Neurons[digit] = std::make_shared<Neuron>(
            50.0,   // 50ms window
            0.65,   // Slightly higher threshold for integration layer
            30,     // More patterns for integration
            1000 + digit
        );
    }
    
    // === TRAINING ===
    std::cout << "=== Training Phase ===\n";
    std::vector<int> trainCount(10, 0);
    
    // First pass: Train Layer 1 feature detectors
    std::cout << "Training Layer 1 (feature detectors)...\n";
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;
        
        if (trainCount[label] >= TRAIN_PER_DIGIT) continue;
        
        // Extract features and train Layer 1
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
        if (totalTrained % 20 == 0) {
            std::cout << "  Trained " << totalTrained << " examples";
            std::cout.flush();
        }
    }
    
    std::cout << "\n\nTraining Layer 2 (integration neurons)...\n";
    std::fill(trainCount.begin(), trainCount.end(), 0);
    
    // Second pass: Train Layer 2 integration neurons
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;
        
        if (trainCount[label] >= TRAIN_PER_DIGIT) continue;
        
        // Get Layer 1 activations
        auto activations = getLayer1Activations(img, layer1Neurons);
        
        // Convert to spikes for Layer 2
        auto layer2Spikes = activationsToSpikes(activations);
        
        if (layer2Spikes.empty()) continue;
        
        // Train the corresponding digit neuron
        auto& neuron = layer2Neurons[label];
        for (double spikeTime : layer2Spikes) {
            neuron->insertSpike(spikeTime);
        }
        neuron->learnCurrentPattern();
        neuron->clearSpikes();
        
        trainCount[label]++;
        
        int totalTrained = 0;
        for (int d = 0; d < 10; ++d) totalTrained += trainCount[d];
        if (totalTrained % 20 == 0) {
            std::cout << "  Trained " << totalTrained << " examples";
            std::cout.flush();
        }
    }
    
    std::cout << "\n\nTraining complete!\n";
    for (int d = 0; d < 10; ++d) {
        std::cout << "  Digit " << d << ": " << trainCount[d] << " patterns learned\n";
    }

    // === TESTING ===
    std::cout << "\n=== Testing Phase ===\n";

    int correct = 0;
    std::vector<int> perDigitCorrect(10, 0);
    std::vector<int> perDigitTotal(10, 0);
    std::vector<std::vector<int>> confusionMatrix(10, std::vector<int>(10, 0));

    for (size_t i = 0; i < testLoader.size(); ++i) {
        const auto& img = testLoader.getImage(i);
        int trueLabel = img.label;

        // Get Layer 1 activations
        auto activations = getLayer1Activations(img, layer1Neurons);

        // Convert to spikes for Layer 2
        auto layer2Spikes = activationsToSpikes(activations);

        // Test against all Layer 2 neurons
        int predictedLabel = -1;
        double bestSimilarity = -1.0;
        std::vector<double> similarities(10, 0.0);

        for (int digit = 0; digit < 10; ++digit) {
            auto& neuron = layer2Neurons[digit];

            // Insert spikes
            for (double spikeTime : layer2Spikes) {
                neuron->insertSpike(spikeTime);
            }

            // Get similarity
            double similarity = neuron->getBestSimilarity();
            similarities[digit] = similarity;

            if (similarity > bestSimilarity) {
                bestSimilarity = similarity;
                predictedLabel = digit;
            }

            neuron->clearSpikes();
        }

        // Debug first few
        if (i < 3) {
            std::cout << "\nTest image " << i << " (true label: " << trueLabel << "):\n";
            std::cout << "  Layer 2 similarities: ";
            for (int d = 0; d < 10; ++d) {
                std::cout << d << ":" << std::fixed << std::setprecision(3) << similarities[d] << " ";
            }
            std::cout << "\n  Predicted: " << predictedLabel << " (sim=" << bestSimilarity << ")\n";
        }

        // Record results
        perDigitTotal[trueLabel]++;
        confusionMatrix[trueLabel][predictedLabel]++;

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

    // Print results
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

    std::cout << "\nConfusion Matrix:\n";
    std::cout << "        ";
    for (int d = 0; d < 10; ++d) std::cout << std::setw(4) << d;
    std::cout << "\n";
    for (int true_d = 0; true_d < 10; ++true_d) {
        std::cout << " " << true_d << ":  ";
        for (int pred_d = 0; pred_d < 10; ++pred_d) {
            std::cout << std::setw(4) << confusionMatrix[true_d][pred_d];
        }
        std::cout << "\n";
    }

    return 0;
}

