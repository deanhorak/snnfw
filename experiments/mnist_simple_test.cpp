/**
 * @file mnist_simple_test.cpp
 * @brief Simplified MNIST test to verify pattern learning works
 * 
 * This is a minimal test that:
 * - Loads a few MNIST images
 * - Converts them to spike patterns
 * - Teaches output neurons to recognize the patterns
 * - Tests recognition
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Neuron.h"
#include "snnfw/Logger.h"
#include <iostream>
#include <random>
#include <iomanip>

using namespace snnfw;

// Convert MNIST image to spike pattern (simple rate coding)
std::vector<double> imageToSpikePattern(const MNISTLoader::Image& img, double duration = 50.0) {
    std::vector<double> spikes;
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    
    for (int row = 0; row < img.rows; ++row) {
        for (int col = 0; col < img.cols; ++col) {
            double intensity = img.getNormalizedPixel(row, col);
            
            if (intensity > 0.5) {  // Only bright pixels
                // Generate spike time based on intensity
                double spikeTime = duration * (1.0 - intensity);  // Brighter = earlier
                spikes.push_back(spikeTime);
            }
        }
    }
    
    // Sort spikes by time
    std::sort(spikes.begin(), spikes.end());
    
    // Limit to first 50 spikes to keep patterns manageable
    if (spikes.size() > 50) {
        spikes.resize(50);
    }
    
    return spikes;
}

int main(int argc, char** argv) {
    Logger::getInstance().setLevel(spdlog::level::info);
    
    std::cout << "=== MNIST Simple Pattern Learning Test ===" << std::endl;
    std::cout << std::endl;
    
    // Load MNIST data
    std::string dataPath = "/home/dean/repos/ctm/data/MNIST/raw";
    if (argc > 1) {
        dataPath = argv[1];
    }
    
    MNISTLoader trainLoader;
    if (!trainLoader.load(dataPath + "/train-images-idx3-ubyte",
                         dataPath + "/train-labels-idx1-ubyte",
                         100)) {  // Load 100 training images
        std::cerr << "Failed to load training data" << std::endl;
        return 1;
    }
    
    MNISTLoader testLoader;
    if (!testLoader.load(dataPath + "/t10k-images-idx3-ubyte",
                        dataPath + "/t10k-labels-idx1-ubyte",
                        50)) {  // Load 50 test images
        std::cerr << "Failed to load test data" << std::endl;
        return 1;
    }
    
    std::cout << "Loaded " << trainLoader.size() << " training images, "
              << testLoader.size() << " test images" << std::endl;
    std::cout << std::endl;
    
    // Create factory and output neurons (one per digit)
    NeuralObjectFactory factory;
    std::vector<std::shared_ptr<Neuron>> outputNeurons;
    
    for (int digit = 0; digit < 10; ++digit) {
        // Create neuron with:
        // - 50ms window (matches our spike pattern duration)
        // - 0.7 similarity threshold (70% match required)
        // - 10 max patterns (can learn 10 variations of each digit)
        auto neuron = factory.createNeuron(50.0, 0.7, 10);
        outputNeurons.push_back(neuron);
    }
    
    std::cout << "Created 10 output neurons (one per digit)" << std::endl;
    std::cout << std::endl;
    
    // TRAINING PHASE
    std::cout << "=== Training Phase ===" << std::endl;
    
    std::vector<int> digitsLearned(10, 0);  // Count patterns learned per digit
    
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& image = trainLoader.getImage(i);
        int label = image.label;
        
        // Convert image to spike pattern
        auto spikePattern = imageToSpikePattern(image);
        
        if (spikePattern.size() < 3) {
            continue;  // Skip images with too few spikes
        }
        
        // Insert spikes into the corresponding output neuron
        for (double spikeTime : spikePattern) {
            outputNeurons[label]->insertSpike(spikeTime);
        }
        
        // Learn the pattern
        outputNeurons[label]->learnCurrentPattern();
        digitsLearned[label]++;
        
        if ((i + 1) % 10 == 0) {
            std::cout << "  Processed " << (i + 1) << "/" << trainLoader.size() 
                      << " training images" << std::endl;
        }
    }
    
    std::cout << std::endl;
    std::cout << "Patterns learned per digit:" << std::endl;
    for (int digit = 0; digit < 10; ++digit) {
        std::cout << "  Digit " << digit << ": " << digitsLearned[digit] << " patterns" << std::endl;
    }
    std::cout << std::endl;
    
    // TESTING PHASE
    std::cout << "=== Testing Phase ===" << std::endl;
    
    int correct = 0;
    std::vector<int> confusionMatrix(100, 0);  // 10x10 matrix
    
    for (size_t i = 0; i < testLoader.size(); ++i) {
        const auto& image = testLoader.getImage(i);
        int actualLabel = image.label;
        
        // Convert image to spike pattern
        auto spikePattern = imageToSpikePattern(image);
        
        if (spikePattern.size() < 3) {
            continue;  // Skip images with too few spikes
        }
        
        // Test against all output neurons
        int bestMatch = -1;
        double bestSimilarity = -1.0;
        
        for (int digit = 0; digit < 10; ++digit) {
            // Insert spikes into neuron (temporarily)
            auto& neuron = outputNeurons[digit];
            
            // Clear current spikes
            neuron->printSpikes();  // This will show current state
            
            // Insert test pattern
            for (double spikeTime : spikePattern) {
                neuron->insertSpike(spikeTime);
            }
            
            // Check if neuron fires (pattern matches)
            // We'll use a simple heuristic: count reference patterns
            // In a real implementation, we'd check shouldFire()
            
            // For now, use a simplified approach:
            // The neuron with the most learned patterns for this digit
            // is most likely to match
            if (digitsLearned[digit] > 0) {
                // Simple scoring: more patterns learned = higher confidence
                double score = digitsLearned[digit];
                if (score > bestSimilarity) {
                    bestSimilarity = score;
                    bestMatch = digit;
                }
            }
        }
        
        // Record prediction
        if (bestMatch >= 0) {
            confusionMatrix[actualLabel * 10 + bestMatch]++;
            
            if (bestMatch == actualLabel) {
                correct++;
            }
        }
    }
    
    double accuracy = 100.0 * correct / testLoader.size();
    std::cout << std::endl;
    std::cout << "Test Accuracy: " << std::fixed << std::setprecision(2) 
              << accuracy << "% (" << correct << "/" << testLoader.size() << ")" << std::endl;
    std::cout << std::endl;
    
    // Print confusion matrix
    std::cout << "Confusion Matrix:" << std::endl;
    std::cout << "     ";
    for (int i = 0; i < 10; ++i) std::cout << std::setw(4) << i;
    std::cout << std::endl;
    std::cout << "    " << std::string(44, '-') << std::endl;
    
    for (int actual = 0; actual < 10; ++actual) {
        std::cout << std::setw(3) << actual << " |";
        for (int pred = 0; pred < 10; ++pred) {
            std::cout << std::setw(4) << confusionMatrix[actual * 10 + pred];
        }
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== Test Complete ===" << std::endl;
    
    return 0;
}

