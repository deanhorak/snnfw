/**
 * MNIST Edge Feature Test - Use edge/orientation features like V1 neurons
 *
 * Approach:
 * 1. Apply simple edge detection (horizontal, vertical, diagonal)
 * 2. Each orientation creates its own spike pattern
 * 3. Combine spatial SDRs with feature-based encoding
 * 4. More biologically plausible - mimics V1 simple cells
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
const int GRID_SIZE = 4;           // 4x4 grid of regions
const int REGION_SIZE = 7;         // Each region is 7x7 pixels
const int NUM_REGIONS = GRID_SIZE * GRID_SIZE;  // 16 regions
const int NUM_ORIENTATIONS = 4;    // 4 edge orientations (horizontal, vertical, 2 diagonals)
const int TRAIN_PER_DIGIT = 10;
const int TEST_IMAGES = 100;

// Extract a region from the image
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

// Apply simple edge detection to a region
// Returns 4 feature maps (one per orientation)
std::vector<std::vector<double>> extractEdgeFeatures(const std::vector<uint8_t>& region) {
    std::vector<std::vector<double>> features(NUM_ORIENTATIONS);
    
    // Convert to 7x7 grid
    const int size = REGION_SIZE;
    
    for (int r = 1; r < size - 1; ++r) {
        for (int c = 1; c < size - 1; ++c) {
            int idx = r * size + c;
            double center = region[idx] / 255.0;
            
            // Horizontal edge (left - right)
            double left = region[r * size + (c-1)] / 255.0;
            double right = region[r * size + (c+1)] / 255.0;
            double horizEdge = std::abs(left - right);
            if (horizEdge > 0.2) features[0].push_back(horizEdge);
            
            // Vertical edge (top - bottom)
            double top = region[(r-1) * size + c] / 255.0;
            double bottom = region[(r+1) * size + c] / 255.0;
            double vertEdge = std::abs(top - bottom);
            if (vertEdge > 0.2) features[1].push_back(vertEdge);
            
            // Diagonal edge (top-left to bottom-right)
            double topLeft = region[(r-1) * size + (c-1)] / 255.0;
            double bottomRight = region[(r+1) * size + (c+1)] / 255.0;
            double diag1Edge = std::abs(topLeft - bottomRight);
            if (diag1Edge > 0.2) features[2].push_back(diag1Edge);
            
            // Diagonal edge (top-right to bottom-left)
            double topRight = region[(r-1) * size + (c+1)] / 255.0;
            double bottomLeft = region[(r+1) * size + (c-1)] / 255.0;
            double diag2Edge = std::abs(topRight - bottomLeft);
            if (diag2Edge > 0.2) features[3].push_back(diag2Edge);
        }
    }
    
    return features;
}

// Convert feature values to spike pattern
std::vector<double> featuresToSpikePattern(const std::vector<double>& features, double duration = 50.0) {
    std::vector<double> spikes;
    
    for (double feature : features) {
        // Stronger features spike earlier
        double spikeTime = duration * (1.0 - feature);
        spikes.push_back(spikeTime);
    }
    
    // Sort spikes by time
    std::sort(spikes.begin(), spikes.end());
    
    return spikes;
}

// Convert entire image to feature-based spatial SDR
// Returns: 16 regions × 4 orientations = 64 spike patterns
std::vector<std::vector<std::vector<double>>> imageToFeatureSDR(const MNISTLoader::Image& img) {
    std::vector<std::vector<std::vector<double>>> featureSDR;
    
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            auto region = extractRegion(img, row, col);
            auto edgeFeatures = extractEdgeFeatures(region);
            
            // Convert each orientation's features to spikes
            std::vector<std::vector<double>> regionSpikes;
            for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
                auto spikes = featuresToSpikePattern(edgeFeatures[orient]);
                regionSpikes.push_back(spikes);
            }
            
            featureSDR.push_back(regionSpikes);
        }
    }
    
    return featureSDR;
}

int main() {
    std::cout << "=== MNIST Edge Feature Test ===\n\n";
    
    // Load MNIST data
    MNISTLoader trainLoader;
    trainLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
        100
    );
    
    MNISTLoader testLoader;
    testLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-labels-idx1-ubyte",
        TEST_IMAGES
    );
    
    std::cout << "Loaded " << trainLoader.size() << " training images\n";
    std::cout << "Loaded " << testLoader.size() << " test images\n";
    std::cout << "Grid: " << GRID_SIZE << "x" << GRID_SIZE << " = " << NUM_REGIONS << " regions\n";
    std::cout << "Orientations: " << NUM_ORIENTATIONS << " (H, V, D1, D2)\n";
    std::cout << "Total neurons per digit: " << NUM_REGIONS * NUM_ORIENTATIONS << "\n\n";
    
    // Create neurons: 10 digits × 16 regions × 4 orientations = 640 neurons total
    std::vector<std::vector<std::vector<std::shared_ptr<Neuron>>>> digitNeurons(10);
    
    for (int digit = 0; digit < 10; ++digit) {
        digitNeurons[digit].resize(NUM_REGIONS);
        for (int region = 0; region < NUM_REGIONS; ++region) {
            for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
                auto neuron = std::make_shared<Neuron>(
                    50.0,   // 50ms window
                    0.6,    // Threshold
                    20,     // Max patterns
                    digit * NUM_REGIONS * NUM_ORIENTATIONS + region * NUM_ORIENTATIONS + orient
                );
                digitNeurons[digit][region].push_back(neuron);
            }
        }
    }
    
    // Training phase
    std::cout << "=== Training Phase ===\n";
    std::vector<int> trainCount(10, 0);
    
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;
        
        if (trainCount[label] >= TRAIN_PER_DIGIT) continue;
        
        // Convert image to feature-based SDR
        auto featureSDR = imageToFeatureSDR(img);
        
        // Train each region's orientation-selective neurons
        for (int region = 0; region < NUM_REGIONS; ++region) {
            for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
                auto& neuron = digitNeurons[label][region][orient];
                
                // Skip empty patterns
                if (featureSDR[region][orient].empty()) {
                    continue;
                }
                
                // Insert spikes
                for (double spikeTime : featureSDR[region][orient]) {
                    neuron->insertSpike(spikeTime);
                }
                
                // Learn the pattern
                neuron->learnCurrentPattern();
                neuron->clearSpikes();
            }
        }
        
        trainCount[label]++;
        
        int totalTrained = 0;
        for (int d = 0; d < 10; ++d) totalTrained += trainCount[d];
        if (totalTrained % 10 == 0) {
            std::cout << "  Trained " << totalTrained << " examples";
            std::cout.flush();
        }
    }
    
    std::cout << "\nTraining complete!\n";
    for (int d = 0; d < 10; ++d) {
        std::cout << "  Digit " << d << ": " << trainCount[d] << " patterns learned\n";
    }
    
    // Testing phase
    std::cout << "\n=== Testing Phase ===\n";
    
    int correct = 0;
    std::vector<int> perDigitCorrect(10, 0);
    std::vector<int> perDigitTotal(10, 0);
    std::vector<std::vector<int>> confusionMatrix(10, std::vector<int>(10, 0));
    
    for (size_t i = 0; i < testLoader.size(); ++i) {
        const auto& img = testLoader.getImage(i);
        int trueLabel = img.label;
        
        // Convert to feature SDR
        auto featureSDR = imageToFeatureSDR(img);
        
        // Test against all digits
        int predictedLabel = -1;
        double bestAvgSimilarity = -1.0;
        std::vector<double> avgSimilarities(10, 0.0);
        
        for (int digit = 0; digit < 10; ++digit) {
            double totalSimilarity = 0.0;
            int activeNeurons = 0;
            
            // Test each region and orientation
            for (int region = 0; region < NUM_REGIONS; ++region) {
                for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
                    auto& neuron = digitNeurons[digit][region][orient];
                    
                    // Insert spikes
                    for (double spikeTime : featureSDR[region][orient]) {
                        neuron->insertSpike(spikeTime);
                    }
                    
                    // Get similarity
                    double similarity = neuron->getBestSimilarity();
                    if (similarity > 0.0) {
                        totalSimilarity += similarity;
                        activeNeurons++;
                    }
                    
                    neuron->clearSpikes();
                }
            }
            
            // Average similarity
            double avgSimilarity = (activeNeurons > 0) ? (totalSimilarity / activeNeurons) : 0.0;
            avgSimilarities[digit] = avgSimilarity;
            
            if (avgSimilarity > bestAvgSimilarity) {
                bestAvgSimilarity = avgSimilarity;
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
            std::cout << "\n  Predicted: " << predictedLabel << " (sim=" << bestAvgSimilarity << ")\n";
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
    
    return 0;
}

