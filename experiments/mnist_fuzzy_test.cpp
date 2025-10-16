/**
 * MNIST Fuzzy Matching Test - Test temporal binning approach
 *
 * Quick test with 10 training examples per digit, 100 test images
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>

using namespace snnfw;

// Convert MNIST image to spike pattern using rate coding
std::vector<double> imageToSpikePattern(const MNISTLoader::Image& img, double duration = 50.0) {
    std::vector<double> spikes;
    
    for (int row = 0; row < img.rows; ++row) {
        for (int col = 0; col < img.cols; ++col) {
            double intensity = img.getNormalizedPixel(row, col);
            if (intensity > 0.1) {
                double spikeTime = duration * (1.0 - intensity);
                spikes.push_back(spikeTime);
            }
        }
    }
    
    std::sort(spikes.begin(), spikes.end());
    return spikes;
}

int main() {
    std::cout << "=== MNIST Fuzzy Matching Test ===\n\n";
    
    // Load MNIST data
    MNISTLoader trainLoader, testLoader;
    std::string dataPath = "/home/dean/repos/ctm/data/MNIST/raw";
    
    const int TRAIN_PER_DIGIT = 10;
    const int TEST_IMAGES = 100;
    
    if (!trainLoader.load(dataPath + "/train-images-idx3-ubyte",
                          dataPath + "/train-labels-idx1-ubyte", 
                          TRAIN_PER_DIGIT * 10)) {
        std::cerr << "Failed to load training data\n";
        return 1;
    }
    
    if (!testLoader.load(dataPath + "/t10k-images-idx3-ubyte",
                         dataPath + "/t10k-labels-idx1-ubyte",
                         TEST_IMAGES)) {
        std::cerr << "Failed to load test data\n";
        return 1;
    }
    
    std::cout << "Loaded " << trainLoader.size() << " training images\n";
    std::cout << "Loaded " << testLoader.size() << " test images\n\n";
    
    // Create output neurons (one per digit)
    std::vector<std::shared_ptr<Neuron>> outputNeurons;
    for (int i = 0; i < 10; ++i) {
        // windowSize=50ms, threshold=0.7, maxPatterns=20
        auto neuron = std::make_shared<Neuron>(50.0, 0.7, 20, i);
        outputNeurons.push_back(neuron);
    }
    
    std::cout << "=== Training Phase ===\n";
    
    std::vector<int> trainCount(10, 0);
    
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;
        
        // Skip if we already have enough examples for this digit
        if (trainCount[label] >= TRAIN_PER_DIGIT) continue;
        
        // Convert to spike pattern
        auto spikes = imageToSpikePattern(img);
        
        // Insert spikes into the neuron for this digit
        for (double spikeTime : spikes) {
            outputNeurons[label]->insertSpike(spikeTime);
        }
        
        // Learn the pattern
        outputNeurons[label]->learnCurrentPattern();
        
        // Clear spikes for next training example
        outputNeurons[label]->clearSpikes();
        
        trainCount[label]++;
        
        if ((i + 1) % 10 == 0) {
            std::cout << "  Trained " << (i + 1) << " examples\r" << std::flush;
        }
    }
    
    std::cout << "\nTraining complete!\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << "  Digit " << i << ": " << trainCount[i] << " patterns learned\n";
    }
    
    std::cout << "\n=== Testing Phase ===\n";
    
    int correct = 0;
    std::vector<std::vector<int>> confusion(10, std::vector<int>(10, 0));
    
    for (size_t i = 0; i < testLoader.size(); ++i) {
        const auto& img = testLoader.getImage(i);
        int trueLabel = img.label;
        
        // Convert to spike pattern
        auto spikes = imageToSpikePattern(img);
        
        // Test against all neurons - use winner-takes-all based on similarity
        int predictedLabel = -1;
        double bestSimilarity = -1.0;
        std::vector<double> similarities(10, -1.0);

        for (int digit = 0; digit < 10; ++digit) {
            // Insert spikes
            for (double spikeTime : spikes) {
                outputNeurons[digit]->insertSpike(spikeTime);
            }

            // Get similarity score
            double similarity = outputNeurons[digit]->getBestSimilarity();
            similarities[digit] = similarity;
            if (similarity > bestSimilarity) {
                bestSimilarity = similarity;
                predictedLabel = digit;
            }

            outputNeurons[digit]->clearSpikes();
        }

        // Debug: Print similarities for first few test images
        if (i < 3) {
            std::cout << "\nTest image " << i << " (true label: " << trueLabel << "):\n";
            std::cout << "  Similarities: ";
            for (int d = 0; d < 10; ++d) {
                std::cout << d << ":" << std::fixed << std::setprecision(3) << similarities[d] << " ";
            }
            std::cout << "\n  Predicted: " << predictedLabel << " (sim=" << bestSimilarity << ")\n";
        }
        
        // Record result
        if (predictedLabel == trueLabel) {
            correct++;
        }
        confusion[trueLabel][predictedLabel]++;
        
        if ((i + 1) % 10 == 0) {
            std::cout << "  Tested " << (i + 1) << " images, accuracy: " 
                      << (100.0 * correct / (i + 1)) << "%\r" << std::flush;
        }
    }
    
    std::cout << "\n\n=== Results ===\n";
    std::cout << "Overall Accuracy: " << (100.0 * correct / testLoader.size()) 
              << "% (" << correct << "/" << testLoader.size() << ")\n\n";
    
    // Per-digit accuracy
    std::cout << "Per-Digit Accuracy:\n";
    for (int i = 0; i < 10; ++i) {
        int total = 0;
        for (int j = 0; j < 10; ++j) total += confusion[i][j];
        if (total > 0) {
            std::cout << "  Digit " << i << ": " 
                      << (100.0 * confusion[i][i] / total) << "% "
                      << "(" << confusion[i][i] << "/" << total << ")\n";
        }
    }
    
    // Confusion matrix
    std::cout << "\nConfusion Matrix:\n";
    std::cout << "     ";
    for (int i = 0; i < 10; ++i) std::cout << std::setw(4) << i;
    std::cout << "\n";
    
    for (int i = 0; i < 10; ++i) {
        std::cout << std::setw(2) << i << ": ";
        for (int j = 0; j < 10; ++j) {
            std::cout << std::setw(4) << confusion[i][j];
        }
        std::cout << "\n";
    }
    
    return 0;
}

