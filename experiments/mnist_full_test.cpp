/**
 * MNIST Full Dataset Test
 * 
 * Train on MNIST training set and test on test set to measure accuracy.
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include "snnfw/ExperimentConfig.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>

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

int main(int argc, char* argv[]) {
    // Parse command line arguments
    size_t trainPerDigit = 100;  // Default: 100 training examples per digit
    size_t testImages = 1000;     // Default: test on 1000 images
    
    if (argc > 1) {
        trainPerDigit = std::atoi(argv[1]);
    }
    if (argc > 2) {
        testImages = std::atoi(argv[2]);
    }
    
    std::cout << "=== MNIST Full Dataset Test ===\n\n";
    std::cout << "Training: " << trainPerDigit << " examples per digit\n";
    std::cout << "Testing: " << testImages << " images\n\n";
    
    // Load training data
    std::cout << "Loading training data...\n";
    MNISTLoader trainLoader;
    trainLoader.load("/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
                     "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
                     0);  // Load all
    std::cout << "Loaded " << trainLoader.size() << " training images\n";
    
    // Load test data
    std::cout << "Loading test data...\n";
    MNISTLoader testLoader;
    testLoader.load("/home/dean/repos/ctm/data/MNIST/raw/t10k-images-idx3-ubyte",
                    "/home/dean/repos/ctm/data/MNIST/raw/t10k-labels-idx1-ubyte",
                    testImages);
    std::cout << "Loaded " << testLoader.size() << " test images\n\n";
    
    // Create 10 output neurons (one per digit)
    // Parameters: windowSize=50ms, threshold=0.7, maxPatterns=20
    std::vector<std::shared_ptr<Neuron>> outputNeurons;
    for (int i = 0; i < 10; ++i) {
        auto neuron = std::make_shared<Neuron>(50.0, 0.7, 20);
        outputNeurons.push_back(neuron);
    }
    
    std::cout << "Created 10 output neurons\n\n";
    
    // TRAINING PHASE
    std::cout << "=== Training Phase ===\n\n";
    
    auto trainStart = std::chrono::steady_clock::now();
    
    std::vector<int> trainCount(10, 0);  // Count training examples per digit
    
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        auto img = trainLoader.getImage(i);
        uint8_t label = img.label;
        
        // Train up to trainPerDigit examples per digit
        if (trainCount[label] < trainPerDigit) {
            auto spikes = imageToSpikePattern(img);
            
            // Insert spikes into the corresponding neuron
            for (double spikeTime : spikes) {
                outputNeurons[label]->insertSpike(spikeTime);
            }
            
            // Learn the pattern
            outputNeurons[label]->learnCurrentPattern();
            
            // Clear spikes for next training example
            outputNeurons[label]->clearSpikes();
            
            trainCount[label]++;
            
            // Progress indicator
            if ((i + 1) % 100 == 0) {
                std::cout << "Processed " << (i + 1) << " images...\r" << std::flush;
            }
        }
        
        // Stop when we have enough examples of each digit
        bool allTrained = true;
        for (int count : trainCount) {
            if (count < trainPerDigit) {
                allTrained = false;
                break;
            }
        }
        if (allTrained) break;
    }
    
    auto trainEnd = std::chrono::steady_clock::now();
    auto trainDuration = std::chrono::duration_cast<std::chrono::milliseconds>(trainEnd - trainStart).count();
    
    std::cout << "\nTraining complete in " << trainDuration << " ms\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << "  Digit " << i << ": " << trainCount[i] << " examples, "
                  << outputNeurons[i]->getLearnedPatternCount() << " patterns learned\n";
    }
    std::cout << "\n";
    
    // TESTING PHASE
    std::cout << "=== Testing Phase ===\n\n";
    
    auto testStart = std::chrono::steady_clock::now();
    
    int correct = 0;
    int total = 0;
    std::vector<std::vector<int>> confusionMatrix(10, std::vector<int>(10, 0));
    
    for (size_t i = 0; i < testLoader.size(); ++i) {
        auto img = testLoader.getImage(i);
        uint8_t trueLabel = img.label;
        
        auto spikes = imageToSpikePattern(img);
        
        // Test against all neurons - use winner-takes-all based on similarity
        int predictedLabel = -1;
        double bestSimilarity = -1.0;

        for (int digit = 0; digit < 10; ++digit) {
            // Insert spikes
            for (double spikeTime : spikes) {
                outputNeurons[digit]->insertSpike(spikeTime);
            }

            // Get similarity score
            double similarity = outputNeurons[digit]->getBestSimilarity();
            if (similarity > bestSimilarity) {
                bestSimilarity = similarity;
                predictedLabel = digit;
            }

            outputNeurons[digit]->clearSpikes();
        }
        
        // Record result
        if (predictedLabel == trueLabel) {
            correct++;
        }
        
        if (predictedLabel >= 0) {
            confusionMatrix[trueLabel][predictedLabel]++;
        }
        
        total++;
        
        // Progress indicator
        if ((i + 1) % 100 == 0) {
            std::cout << "Tested " << (i + 1) << " images... Accuracy: " 
                      << std::fixed << std::setprecision(2)
                      << (100.0 * correct / total) << "%\r" << std::flush;
        }
    }
    
    auto testEnd = std::chrono::steady_clock::now();
    auto testDuration = std::chrono::duration_cast<std::chrono::milliseconds>(testEnd - testStart).count();
    
    std::cout << "\n\nTesting complete in " << testDuration << " ms\n\n";
    
    // RESULTS
    std::cout << "=== Results ===\n\n";
    std::cout << "Accuracy: " << std::fixed << std::setprecision(2)
              << (100.0 * correct / total) << "% (" << correct << "/" << total << ")\n\n";
    
    // Per-digit accuracy
    std::cout << "Per-digit accuracy:\n";
    for (int digit = 0; digit < 10; ++digit) {
        int digitTotal = 0;
        int digitCorrect = confusionMatrix[digit][digit];
        for (int pred = 0; pred < 10; ++pred) {
            digitTotal += confusionMatrix[digit][pred];
        }
        if (digitTotal > 0) {
            std::cout << "  Digit " << digit << ": " 
                      << std::fixed << std::setprecision(1)
                      << (100.0 * digitCorrect / digitTotal) << "% "
                      << "(" << digitCorrect << "/" << digitTotal << ")\n";
        }
    }
    
    std::cout << "\nConfusion Matrix (rows=true, cols=predicted):\n";
    std::cout << "     ";
    for (int i = 0; i < 10; ++i) {
        std::cout << std::setw(5) << i;
    }
    std::cout << "\n";
    
    for (int true_digit = 0; true_digit < 10; ++true_digit) {
        std::cout << std::setw(3) << true_digit << ": ";
        for (int pred_digit = 0; pred_digit < 10; ++pred_digit) {
            std::cout << std::setw(5) << confusionMatrix[true_digit][pred_digit];
        }
        std::cout << "\n";
    }
    
    std::cout << "\n=== Performance ===\n";
    std::cout << "Training time: " << trainDuration << " ms\n";
    std::cout << "Testing time: " << testDuration << " ms\n";
    std::cout << "Training speed: " << (trainPerDigit * 10 * 1000.0 / trainDuration) << " images/sec\n";
    std::cout << "Testing speed: " << (total * 1000.0 / testDuration) << " images/sec\n";
    
    return 0;
}

