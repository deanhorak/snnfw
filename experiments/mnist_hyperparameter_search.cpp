/**
 * MNIST Hyperparameter Search
 *
 * Systematically test different configurations:
 * 1. Grid size (spatial resolution)
 * 2. Number of orientations (feature richness)
 * 3. Training examples per digit
 * 4. Neuron parameters (window size, threshold, max patterns)
 * 5. Edge detection threshold
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>
#include <string>

using namespace snnfw;

struct Config {
    int gridSize;
    int numOrientations;
    int trainPerDigit;
    double neuronWindowMs;
    double neuronThreshold;
    int neuronMaxPatterns;
    double edgeThreshold;
    
    std::string toString() const {
        return "Grid:" + std::to_string(gridSize) + "x" + std::to_string(gridSize) +
               " Orient:" + std::to_string(numOrientations) +
               " Train:" + std::to_string(trainPerDigit) +
               " Win:" + std::to_string(neuronWindowMs) +
               " Thr:" + std::to_string(neuronThreshold) +
               " MaxPat:" + std::to_string(neuronMaxPatterns) +
               " EdgeThr:" + std::to_string(edgeThreshold);
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

// Extract edge features with configurable orientations
std::vector<std::vector<double>> extractEdgeFeatures(
    const std::vector<uint8_t>& region, 
    int regionSize,
    int numOrientations,
    double edgeThreshold
) {
    std::vector<std::vector<double>> features(numOrientations);
    
    for (int r = 1; r < regionSize - 1; ++r) {
        for (int c = 1; c < regionSize - 1; ++c) {
            double center = region[r * regionSize + c] / 255.0;
            double left = region[r * regionSize + (c-1)] / 255.0;
            double right = region[r * regionSize + (c+1)] / 255.0;
            double top = region[(r-1) * regionSize + c] / 255.0;
            double bottom = region[(r+1) * regionSize + c] / 255.0;
            
            // Horizontal edge (0 degrees)
            double horizEdge = std::abs(left - right);
            if (horizEdge > edgeThreshold) features[0].push_back(horizEdge);
            
            // Vertical edge (90 degrees)
            double vertEdge = std::abs(top - bottom);
            if (vertEdge > edgeThreshold) features[1].push_back(vertEdge);
            
            if (numOrientations >= 4) {
                double topLeft = region[(r-1) * regionSize + (c-1)] / 255.0;
                double bottomRight = region[(r+1) * regionSize + (c+1)] / 255.0;
                double diag1Edge = std::abs(topLeft - bottomRight);
                if (diag1Edge > edgeThreshold) features[2].push_back(diag1Edge);
                
                double topRight = region[(r-1) * regionSize + (c+1)] / 255.0;
                double bottomLeft = region[(r+1) * regionSize + (c-1)] / 255.0;
                double diag2Edge = std::abs(topRight - bottomLeft);
                if (diag2Edge > edgeThreshold) features[3].push_back(diag2Edge);
            }
            
            if (numOrientations >= 8) {
                // Add intermediate orientations (22.5, 67.5, 112.5, 157.5 degrees)
                // Approximate with weighted combinations
                double angle22_5 = std::abs(0.924 * right + 0.383 * bottom - 0.924 * left - 0.383 * top);
                if (angle22_5 > edgeThreshold) features[4].push_back(angle22_5);
                
                double angle67_5 = std::abs(0.383 * right + 0.924 * bottom - 0.383 * left - 0.924 * top);
                if (angle67_5 > edgeThreshold) features[5].push_back(angle67_5);
                
                double angle112_5 = std::abs(-0.383 * right + 0.924 * bottom + 0.383 * left - 0.924 * top);
                if (angle112_5 > edgeThreshold) features[6].push_back(angle112_5);
                
                double angle157_5 = std::abs(-0.924 * right + 0.383 * bottom + 0.924 * left - 0.383 * top);
                if (angle157_5 > edgeThreshold) features[7].push_back(angle157_5);
            }
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

// Cosine similarity
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

// Run experiment with given configuration
double runExperiment(const Config& config, MNISTLoader& trainLoader, MNISTLoader& testLoader, bool verbose = false) {
    int regionSize = 28 / config.gridSize;
    int numRegions = config.gridSize * config.gridSize;
    int numFeatures = numRegions * config.numOrientations;
    
    if (verbose) {
        std::cout << "\n=== Testing Configuration ===\n";
        std::cout << config.toString() << "\n";
        std::cout << "Region size: " << regionSize << "x" << regionSize << "\n";
        std::cout << "Total features: " << numFeatures << "\n";
    }
    
    // Create Layer 1 neurons
    std::vector<std::vector<std::shared_ptr<Neuron>>> layer1Neurons(numRegions);
    for (int region = 0; region < numRegions; ++region) {
        for (int orient = 0; orient < config.numOrientations; ++orient) {
            auto neuron = std::make_shared<Neuron>(
                config.neuronWindowMs,
                config.neuronThreshold,
                config.neuronMaxPatterns,
                region * config.numOrientations + orient
            );
            layer1Neurons[region].push_back(neuron);
        }
    }
    
    // Train Layer 1
    std::vector<int> trainCount(10, 0);
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;
        
        if (trainCount[label] >= config.trainPerDigit) continue;
        
        for (int row = 0; row < config.gridSize; ++row) {
            for (int col = 0; col < config.gridSize; ++col) {
                auto region = extractRegion(img, row, col, regionSize);
                auto edgeFeatures = extractEdgeFeatures(region, regionSize, config.numOrientations, config.edgeThreshold);
                
                for (int orient = 0; orient < config.numOrientations; ++orient) {
                    auto& neuron = layer1Neurons[row * config.gridSize + col][orient];
                    auto spikes = featuresToSpikes(edgeFeatures[orient], config.neuronWindowMs);
                    
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
    }
    
    // Learn activation patterns
    std::vector<std::vector<std::vector<double>>> digitActivationPatterns(10);
    std::fill(trainCount.begin(), trainCount.end(), 0);
    
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        int label = img.label;
        
        if (trainCount[label] >= config.trainPerDigit) continue;
        
        std::vector<double> activations(numFeatures, 0.0);
        int featureIdx = 0;
        
        for (int row = 0; row < config.gridSize; ++row) {
            for (int col = 0; col < config.gridSize; ++col) {
                auto region = extractRegion(img, row, col, regionSize);
                auto edgeFeatures = extractEdgeFeatures(region, regionSize, config.numOrientations, config.edgeThreshold);
                
                for (int orient = 0; orient < config.numOrientations; ++orient) {
                    auto& neuron = layer1Neurons[row * config.gridSize + col][orient];
                    auto spikes = featuresToSpikes(edgeFeatures[orient], config.neuronWindowMs);
                    
                    for (double spikeTime : spikes) {
                        neuron->insertSpike(spikeTime);
                    }
                    
                    activations[featureIdx] = neuron->getBestSimilarity();
                    neuron->clearSpikes();
                    featureIdx++;
                }
            }
        }
        
        digitActivationPatterns[label].push_back(activations);
        trainCount[label]++;
    }
    
    // Test
    int correct = 0;
    for (size_t i = 0; i < testLoader.size(); ++i) {
        const auto& img = testLoader.getImage(i);
        int trueLabel = img.label;
        
        std::vector<double> testActivations(numFeatures, 0.0);
        int featureIdx = 0;
        
        for (int row = 0; row < config.gridSize; ++row) {
            for (int col = 0; col < config.gridSize; ++col) {
                auto region = extractRegion(img, row, col, regionSize);
                auto edgeFeatures = extractEdgeFeatures(region, regionSize, config.numOrientations, config.edgeThreshold);
                
                for (int orient = 0; orient < config.numOrientations; ++orient) {
                    auto& neuron = layer1Neurons[row * config.gridSize + col][orient];
                    auto spikes = featuresToSpikes(edgeFeatures[orient], config.neuronWindowMs);
                    
                    for (double spikeTime : spikes) {
                        neuron->insertSpike(spikeTime);
                    }
                    
                    testActivations[featureIdx] = neuron->getBestSimilarity();
                    neuron->clearSpikes();
                    featureIdx++;
                }
            }
        }
        
        int predictedLabel = -1;
        double bestSimilarity = -1.0;
        
        for (int digit = 0; digit < 10; ++digit) {
            double totalSim = 0.0;
            for (const auto& pattern : digitActivationPatterns[digit]) {
                double sim = activationSimilarity(testActivations, pattern);
                totalSim += sim;
            }
            double avgSim = totalSim / digitActivationPatterns[digit].size();
            
            if (avgSim > bestSimilarity) {
                bestSimilarity = avgSim;
                predictedLabel = digit;
            }
        }
        
        if (predictedLabel == trueLabel) {
            correct++;
        }
    }
    
    double accuracy = 100.0 * correct / testLoader.size();
    
    if (verbose) {
        std::cout << "Accuracy: " << std::fixed << std::setprecision(2) << accuracy << "% ";
        std::cout << "(" << correct << "/" << testLoader.size() << ")\n";
    }
    
    return accuracy;
}

int main() {
    std::cout << "=== MNIST Hyperparameter Search ===\n\n";
    
    // Load data
    MNISTLoader trainLoader;
    trainLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
        1000
    );
    
    MNISTLoader testLoader;
    testLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/t10k-labels-idx1-ubyte",
        200
    );
    
    std::cout << "Loaded " << trainLoader.size() << " training images\n";
    std::cout << "Loaded " << testLoader.size() << " test images\n\n";
    
    // Baseline configuration
    Config baseline = {4, 4, 30, 50.0, 0.6, 20, 0.2};
    
    std::cout << "Baseline: " << baseline.toString() << "\n";
    double baselineAccuracy = runExperiment(baseline, trainLoader, testLoader, true);
    
    std::cout << "\n=== Hyperparameter Sweep ===\n";
    
    // Test different grid sizes
    std::cout << "\n--- Grid Size ---\n";
    for (int gridSize : {2, 4, 7}) {
        Config config = baseline;
        config.gridSize = gridSize;
        std::cout << "Grid " << gridSize << "x" << gridSize << ": ";
        std::cout.flush();
        double acc = runExperiment(config, trainLoader, testLoader, false);
        std::cout << std::fixed << std::setprecision(2) << acc << "%";
        if (acc > baselineAccuracy) std::cout << " ⭐";
        std::cout << "\n";
    }
    
    // Test different orientations
    std::cout << "\n--- Number of Orientations ---\n";
    for (int orientations : {2, 4, 8}) {
        Config config = baseline;
        config.numOrientations = orientations;
        std::cout << orientations << " orientations: ";
        std::cout.flush();
        double acc = runExperiment(config, trainLoader, testLoader, false);
        std::cout << std::fixed << std::setprecision(2) << acc << "%";
        if (acc > baselineAccuracy) std::cout << " ⭐";
        std::cout << "\n";
    }
    
    // Test different training sizes
    std::cout << "\n--- Training Examples per Digit ---\n";
    for (int trainSize : {10, 30, 50, 100}) {
        Config config = baseline;
        config.trainPerDigit = trainSize;
        std::cout << trainSize << " examples: ";
        std::cout.flush();
        double acc = runExperiment(config, trainLoader, testLoader, false);
        std::cout << std::fixed << std::setprecision(2) << acc << "%";
        if (acc > baselineAccuracy) std::cout << " ⭐";
        std::cout << "\n";
    }
    
    // Test different neuron thresholds
    std::cout << "\n--- Neuron Similarity Threshold ---\n";
    for (double threshold : {0.4, 0.5, 0.6, 0.7}) {
        Config config = baseline;
        config.neuronThreshold = threshold;
        std::cout << "Threshold " << threshold << ": ";
        std::cout.flush();
        double acc = runExperiment(config, trainLoader, testLoader, false);
        std::cout << std::fixed << std::setprecision(2) << acc << "%";
        if (acc > baselineAccuracy) std::cout << " ⭐";
        std::cout << "\n";
    }
    
    // Test different edge thresholds
    std::cout << "\n--- Edge Detection Threshold ---\n";
    for (double edgeThresh : {0.1, 0.15, 0.2, 0.25, 0.3}) {
        Config config = baseline;
        config.edgeThreshold = edgeThresh;
        std::cout << "Edge threshold " << edgeThresh << ": ";
        std::cout.flush();
        double acc = runExperiment(config, trainLoader, testLoader, false);
        std::cout << std::fixed << std::setprecision(2) << acc << "%";
        if (acc > baselineAccuracy) std::cout << " ⭐";
        std::cout << "\n";
    }
    
    return 0;
}

