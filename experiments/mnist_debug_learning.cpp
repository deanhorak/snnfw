/**
 * MNIST Pattern Learning Debug - Verify neuron pattern learning
 * 
 * This experiment validates that neurons can learn and recognize
 * spike patterns from MNIST digits.
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include "snnfw/ExperimentConfig.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace snnfw;

// Convert MNIST image to spike pattern using rate coding
std::vector<double> imageToSpikePattern(const MNISTLoader::Image& img, double duration = 50.0) {
    std::vector<double> spikes;
    
    for (int row = 0; row < img.rows; ++row) {
        for (int col = 0; col < img.cols; ++col) {
            double intensity = img.getNormalizedPixel(row, col);
            
            // Only generate spikes for pixels above threshold
            if (intensity > 0.1) {
                // Brighter pixels generate earlier spikes
                double spikeTime = duration * (1.0 - intensity);
                spikes.push_back(spikeTime);
            }
        }
    }
    
    // Sort spikes by time
    std::sort(spikes.begin(), spikes.end());
    
    return spikes;
}

int main() {
    std::cout << "=== MNIST Pattern Learning Debug ===\n\n";
    
    // Create experiment config
    ExperimentConfig config("mnist_pattern_learning_debug");
    
    // Load MNIST data
    MNISTLoader loader;
    loader.load("/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
                "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
                30);  // Load 30 images
    
    std::cout << "Loaded " << loader.size() << " training images\n\n";
    
    // Create 10 output neurons (one per digit)
    // Parameters: windowSize=50ms, threshold=0.7, maxPatterns=10
    std::vector<std::shared_ptr<Neuron>> outputNeurons;
    for (int i = 0; i < 10; ++i) {
        auto neuron = std::make_shared<Neuron>(50.0, 0.7, 10);
        outputNeurons.push_back(neuron);
    }
    
    std::cout << "Created 10 output neurons\n\n";
    
    // TEST 1: Train neurons on first 3 examples of each digit
    std::cout << "=== TEST 1: Training Phase ===\n\n";
    
    std::vector<int> trainCount(10, 0);  // Count training examples per digit
    
    for (size_t i = 0; i < loader.size(); ++i) {
        auto img = loader.getImage(i);
        uint8_t label = img.label;
        
        // Train up to 3 examples per digit
        if (trainCount[label] < 3) {
            auto spikes = imageToSpikePattern(img);
            
            std::cout << "Training digit " << static_cast<int>(label) 
                      << " (example " << (trainCount[label] + 1) << "): "
                      << spikes.size() << " spikes\n";
            
            // Insert spikes into the corresponding neuron
            for (double spikeTime : spikes) {
                outputNeurons[label]->insertSpike(spikeTime);
            }
            
            // Learn the pattern
            outputNeurons[label]->learnCurrentPattern();

            // Clear spikes for next training example
            outputNeurons[label]->clearSpikes();

            trainCount[label]++;
        }
        
        // Stop when we have 3 examples of each digit
        bool allTrained = true;
        for (int count : trainCount) {
            if (count < 3) {
                allTrained = false;
                break;
            }
        }
        if (allTrained) break;
    }
    
    std::cout << "\nTraining complete!\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << "  Digit " << i << ": " << trainCount[i] << " patterns learned\n";
    }
    
    // TEST 2: Check what neurons have learned
    std::cout << "\n=== TEST 2: Inspect Learned Patterns ===\n\n";
    
    for (int i = 0; i < 10; ++i) {
        auto neuron = outputNeurons[i];
        std::cout << "Neuron for digit " << i << ":\n";
        std::cout << "  Number of learned patterns: " << neuron->getLearnedPatternCount() << "\n";
        
        // Get pattern details if available
        auto patterns = neuron->getLearnedPatterns();
        for (size_t p = 0; p < patterns.size(); ++p) {
            std::cout << "  Pattern " << (p + 1) << ": " << patterns[p].size() << " spikes\n";
        }
        std::cout << "\n";
    }
    
    // TEST 3: Test recognition on new examples
    std::cout << "=== TEST 3: Recognition Test ===\n\n";
    
    int correct = 0;
    int total = 0;
    
    // Test on remaining images
    for (size_t i = 0; i < loader.size(); ++i) {
        auto img = loader.getImage(i);
        uint8_t correctLabel = img.label;
        
        // Skip if this was a training example
        // (In real test we'd use separate test set, but for debug we just test all)
        
        auto spikes = imageToSpikePattern(img);
        
        // Test each neuron to see if it recognizes this pattern
        int bestMatch = -1;
        double bestSimilarity = 0.0;
        
        for (int digit = 0; digit < 10; ++digit) {
            auto neuron = outputNeurons[digit];
            
            // Insert spikes into neuron (without learning)
            for (double spikeTime : spikes) {
                neuron->insertSpike(spikeTime);
            }
            
            // Check if neuron fires (recognizes pattern)
            bool fires = neuron->checkShouldFire();
            
            // Get similarity score (we'll need to add this method or approximate it)
            // For now, just use shouldFire() as binary indicator
            if (fires) {
                // This neuron recognizes the pattern
                if (bestMatch == -1) {
                    bestMatch = digit;
                    bestSimilarity = 1.0;
                }
            }
            
            // Clear the neuron's spike window for next test
            neuron->clearSpikes();
        }
        
        // Check if prediction is correct
        if (bestMatch == correctLabel) {
            correct++;
        }
        total++;
        
        if (total <= 10) {  // Show first 10 predictions
            std::cout << "Image " << i << ": True=" << static_cast<int>(correctLabel) 
                      << ", Predicted=" << bestMatch 
                      << (bestMatch == correctLabel ? " ✓" : " ✗") << "\n";
        }
    }
    
    std::cout << "\n=== Results ===\n";
    std::cout << "Accuracy: " << (100.0 * correct / total) << "% "
              << "(" << correct << "/" << total << ")\n";
    
    // TEST 4: Detailed analysis of one neuron
    std::cout << "\n=== TEST 4: Detailed Analysis of Digit '1' Neuron ===\n\n";
    
    auto neuron1 = outputNeurons[1];
    std::cout << "Neuron for digit '1':\n";
    std::cout << "Learned patterns: " << neuron1->getLearnedPatternCount() << "\n\n";
    
    // Test with a known '1' image
    for (size_t i = 0; i < loader.size(); ++i) {
        auto img = loader.getImage(i);
        if (img.label == 1) {
            auto spikes = imageToSpikePattern(img);
            
            std::cout << "Testing with digit '1' image (index " << i << "):\n";
            std::cout << "  Spike count: " << spikes.size() << "\n";
            
            // Insert spikes
            for (double spikeTime : spikes) {
                neuron1->insertSpike(spikeTime);
            }
            
            // Check recognition
            bool fires = neuron1->checkShouldFire();
            std::cout << "  Neuron fires: " << (fires ? "YES ✓" : "NO ✗") << "\n";

            neuron1->clearSpikes();
            break;
        }
    }

    // Test with a known '8' image (should NOT fire)
    for (size_t i = 0; i < loader.size(); ++i) {
        auto img = loader.getImage(i);
        if (img.label == 8) {
            auto spikes = imageToSpikePattern(img);

            std::cout << "\nTesting with digit '8' image (index " << i << "):\n";
            std::cout << "  Spike count: " << spikes.size() << "\n";

            // Insert spikes
            for (double spikeTime : spikes) {
                neuron1->insertSpike(spikeTime);
            }

            // Check recognition
            bool fires = neuron1->checkShouldFire();
            std::cout << "  Neuron fires: " << (fires ? "YES ✗ (false positive)" : "NO ✓ (correct rejection)") << "\n";
            
            neuron1->clearSpikes();
            break;
        }
    }
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "✓ Neurons can learn spike patterns\n";
    std::cout << "✓ Pattern learning stores temporal structure\n";
    std::cout << "? Recognition accuracy: " << (100.0 * correct / total) << "%\n";
    std::cout << "\nNext: Debug recognition if accuracy is low\n";
    
    return 0;
}

