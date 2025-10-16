/**
 * MNIST Spatial SDR Test - Use spatial structure with region-based SDRs
 *
 * Approach:
 * 1. Divide 28x28 image into 4x4 grid (16 regions of 7x7 pixels each)
 * 2. Each region generates its own spike pattern
 * 3. Each output neuron has 16 "receptive field" neurons (one per region)
 * 4. Match based on similarity across all regions (spatial SDR overlap)
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
const int NUM_REGIONS = GRID_SIZE * GRID_SIZE;  // 16 regions total
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
            }
        }
    }
    
    return region;
}

// Convert region pixels to spike pattern using rate coding
std::vector<double> regionToSpikePattern(const std::vector<uint8_t>& region, double duration = 50.0) {
    std::vector<double> spikes;
    
    for (size_t i = 0; i < region.size(); ++i) {
        double intensity = region[i] / 255.0;
        
        // Only generate spikes for pixels above threshold (sparse coding)
        if (intensity > 0.1) {
            // Brighter pixels spike earlier
            double spikeTime = duration * (1.0 - intensity);
            spikes.push_back(spikeTime);
        }
    }
    
    // Sort spikes by time
    std::sort(spikes.begin(), spikes.end());
    
    return spikes;
}

// Convert entire image to spatial SDR (16 spike patterns, one per region)
std::vector<std::vector<double>> imageToSpatialSDR(const MNISTLoader::Image& img) {
    std::vector<std::vector<double>> spatialSDR;
    
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            auto region = extractRegion(img, row, col);
            auto spikes = regionToSpikePattern(region);
            spatialSDR.push_back(spikes);
        }
    }
    
    return spatialSDR;
}

int main() {
    std::cout << "=== MNIST Spatial SDR Test ===\n\n";

    // Load MNIST data
    MNISTLoader trainLoader;
    trainLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
        100  // Load first 100 for training
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
    std::cout << "Region size: " << REGION_SIZE << "x" << REGION_SIZE << " pixels\n\n";
    
    // Create neurons: 10 digits × 16 regions = 160 neurons total
    // Each digit has 16 neurons (one per spatial region)
    std::vector<std::vector<std::shared_ptr<Neuron>>> digitNeurons(10);
    
    for (int digit = 0; digit < 10; ++digit) {
        for (int region = 0; region < NUM_REGIONS; ++region) {
            auto neuron = std::make_shared<Neuron>(
                50.0,   // 50ms window
                0.6,    // Lower threshold for more flexibility
                20,     // Max 20 patterns per region
                digit * NUM_REGIONS + region  // Unique ID
            );
            digitNeurons[digit].push_back(neuron);
        }
    }
    
    // Training phase
    std::cout << "=== Training Phase ===\n";
    std::vector<int> trainCount(10, 0);

    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;

        if (trainCount[label] >= TRAIN_PER_DIGIT) continue;

        // Convert image to spatial SDR (16 spike patterns)
        auto spatialSDR = imageToSpatialSDR(img);

        // Debug: Print spike counts for first image
        if (i == 0) {
            std::cout << "First image (label " << label << ") spike counts per region:\n";
            for (int r = 0; r < NUM_REGIONS; ++r) {
                std::cout << "  Region " << r << ": " << spatialSDR[r].size() << " spikes\n";
            }
        }

        // Train each region's neuron
        for (int region = 0; region < NUM_REGIONS; ++region) {
            auto& neuron = digitNeurons[label][region];

            // Skip empty regions (no spikes)
            if (spatialSDR[region].empty()) {
                continue;
            }

            // Insert spikes for this region
            for (double spikeTime : spatialSDR[region]) {
                neuron->insertSpike(spikeTime);
            }

            // Learn the pattern
            neuron->learnCurrentPattern();
            neuron->clearSpikes();
        }
        
        trainCount[label]++;
        
        if ((trainCount[0] + trainCount[1] + trainCount[2] + trainCount[3] + trainCount[4] +
             trainCount[5] + trainCount[6] + trainCount[7] + trainCount[8] + trainCount[9]) % 10 == 0) {
            std::cout << "  Trained " << (trainCount[0] + trainCount[1] + trainCount[2] + trainCount[3] + trainCount[4] +
                                          trainCount[5] + trainCount[6] + trainCount[7] + trainCount[8] + trainCount[9]) << " examples";
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
        
        // Convert to spatial SDR
        auto spatialSDR = imageToSpatialSDR(img);
        
        // Test against all digits - compute average similarity across all regions
        int predictedLabel = -1;
        double bestAvgSimilarity = -1.0;
        std::vector<double> avgSimilarities(10, 0.0);
        
        for (int digit = 0; digit < 10; ++digit) {
            double totalSimilarity = 0.0;
            int activeRegions = 0;
            
            // Test each region
            for (int region = 0; region < NUM_REGIONS; ++region) {
                auto& neuron = digitNeurons[digit][region];
                
                // Insert spikes for this region
                for (double spikeTime : spatialSDR[region]) {
                    neuron->insertSpike(spikeTime);
                }
                
                // Get similarity score
                double similarity = neuron->getBestSimilarity();
                
                // Only count regions with meaningful activity
                if (similarity > 0.0) {
                    totalSimilarity += similarity;
                    activeRegions++;
                }
                
                neuron->clearSpikes();
            }
            
            // Average similarity across active regions
            double avgSimilarity = (activeRegions > 0) ? (totalSimilarity / activeRegions) : 0.0;
            avgSimilarities[digit] = avgSimilarity;
            
            if (avgSimilarity > bestAvgSimilarity) {
                bestAvgSimilarity = avgSimilarity;
                predictedLabel = digit;
            }
        }
        
        // Debug: Print first few test images
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
    
    std::cout << "\nConfusion Matrix:\n";
    std::cout << "        0   1   2   3   4   5   6   7   8   9\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << " " << i << ":";
        for (int j = 0; j < 10; ++j) {
            std::cout << std::setw(4) << confusionMatrix[i][j];
        }
        std::cout << "\n";
    }
    
    return 0;
}

