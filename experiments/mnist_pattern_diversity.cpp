/**
 * MNIST Pattern Diversity Analysis
 * 
 * Investigates whether neurons are learning unique patterns or just repeating the same ones.
 * Key questions:
 * 1. How many unique patterns does each neuron learn?
 * 2. Are patterns being blended too aggressively?
 * 3. Do we need more neurons per region/orientation?
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>
#include <set>

using namespace snnfw;

const int GRID_SIZE = 7;
const int REGION_SIZE = 4;
const int NUM_REGIONS = GRID_SIZE * GRID_SIZE;
const int NUM_ORIENTATIONS = 8;
const int NUM_FEATURES = NUM_REGIONS * NUM_ORIENTATIONS;
const int TRAIN_IMAGES = 1000;  // Analyze first 1000 images
const double EDGE_THRESHOLD = 0.15;

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
    
    for (int r = 1; r < REGION_SIZE - 1; ++r) {
        for (int c = 1; c < REGION_SIZE - 1; ++c) {
            double left = region[r * REGION_SIZE + (c-1)] / 255.0;
            double right = region[r * REGION_SIZE + (c+1)] / 255.0;
            double top = region[(r-1) * REGION_SIZE + c] / 255.0;
            double bottom = region[(r+1) * REGION_SIZE + c] / 255.0;
            double topLeft = region[(r-1) * REGION_SIZE + (c-1)] / 255.0;
            double topRight = region[(r-1) * REGION_SIZE + (c+1)] / 255.0;
            double bottomLeft = region[(r+1) * REGION_SIZE + (c-1)] / 255.0;
            double bottomRight = region[(r+1) * REGION_SIZE + (c+1)] / 255.0;
            
            double edge0 = std::abs(left - right);
            if (edge0 > EDGE_THRESHOLD) features[0].push_back(edge0);
            
            double edge90 = std::abs(top - bottom);
            if (edge90 > EDGE_THRESHOLD) features[1].push_back(edge90);
            
            double edge45 = std::abs(topLeft - bottomRight);
            if (edge45 > EDGE_THRESHOLD) features[2].push_back(edge45);
            
            double edge135 = std::abs(topRight - bottomLeft);
            if (edge135 > EDGE_THRESHOLD) features[3].push_back(edge135);
            
            double edge22_5 = std::abs(0.924 * right + 0.383 * bottom - 0.924 * left - 0.383 * top);
            if (edge22_5 > EDGE_THRESHOLD) features[4].push_back(edge22_5);
            
            double edge67_5 = std::abs(0.383 * right + 0.924 * bottom - 0.383 * left - 0.924 * top);
            if (edge67_5 > EDGE_THRESHOLD) features[5].push_back(edge67_5);
            
            double edge112_5 = std::abs(-0.383 * right + 0.924 * bottom + 0.383 * left - 0.924 * top);
            if (edge112_5 > EDGE_THRESHOLD) features[6].push_back(edge112_5);
            
            double edge157_5 = std::abs(-0.924 * right + 0.383 * bottom + 0.924 * left - 0.383 * top);
            if (edge157_5 > EDGE_THRESHOLD) features[7].push_back(edge157_5);
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

// Compute similarity between two spike patterns
double spikePatternSimilarity(const std::vector<double>& s1, const std::vector<double>& s2) {
    if (s1.empty() || s2.empty()) return 0.0;
    
    const double q = 0.5;
    double totalCost = 0.0;
    std::vector<bool> matched2(s2.size(), false);
    
    for (double spike1 : s1) {
        double minDist = std::numeric_limits<double>::max();
        int bestMatch = -1;
        
        for (size_t j = 0; j < s2.size(); ++j) {
            if (!matched2[j]) {
                double dist = std::abs(spike1 - s2[j]);
                if (dist < minDist) {
                    minDist = dist;
                    bestMatch = static_cast<int>(j);
                }
            }
        }
        
        if (bestMatch >= 0) {
            double shiftCost = q * minDist;
            double deleteInsertCost = 2.0;
            
            if (shiftCost < deleteInsertCost) {
                totalCost += shiftCost;
                matched2[bestMatch] = true;
            } else {
                totalCost += deleteInsertCost;
            }
        } else {
            totalCost += 1.0;
        }
    }
    
    for (bool matched : matched2) {
        if (!matched) totalCost += 1.0;
    }
    
    // Convert distance to similarity
    return 1.0 / (1.0 + totalCost);
}

int main() {
    std::cout << "=== MNIST Pattern Diversity Analysis ===\n\n";
    
    // Load data
    MNISTLoader trainLoader;
    trainLoader.load(
        "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
        "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
        TRAIN_IMAGES
    );
    
    std::cout << "Loaded " << trainLoader.size() << " training images\n\n";
    
    // Create Layer 1 neurons
    std::vector<std::vector<std::shared_ptr<Neuron>>> layer1Neurons(NUM_REGIONS);
    for (int region = 0; region < NUM_REGIONS; ++region) {
        for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
            auto neuron = std::make_shared<Neuron>(
                50.0,   // 50ms window
                0.6,    // Threshold
                100,    // Max 100 patterns
                region * NUM_ORIENTATIONS + orient
            );
            layer1Neurons[region].push_back(neuron);
        }
    }
    
    // Track all spike patterns seen by each neuron
    std::vector<std::vector<std::vector<std::vector<double>>>> allPatternsSeen(NUM_REGIONS);
    for (int region = 0; region < NUM_REGIONS; ++region) {
        allPatternsSeen[region].resize(NUM_ORIENTATIONS);
    }
    
    // Train and collect patterns
    std::cout << "=== Training and Collecting Patterns ===\n";
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        const auto& img = trainLoader.getImage(i);
        
        for (int row = 0; row < GRID_SIZE; ++row) {
            for (int col = 0; col < GRID_SIZE; ++col) {
                int regionIdx = row * GRID_SIZE + col;
                auto region = extractRegion(img, row, col);
                auto edgeFeatures = extractEdgeFeatures(region);
                
                for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
                    auto& neuron = layer1Neurons[regionIdx][orient];
                    auto spikes = featuresToSpikes(edgeFeatures[orient]);
                    
                    if (spikes.empty()) continue;
                    
                    // Store the pattern we're about to learn
                    allPatternsSeen[regionIdx][orient].push_back(spikes);
                    
                    // Learn it
                    for (double spikeTime : spikes) {
                        neuron->insertSpike(spikeTime);
                    }
                    neuron->learnCurrentPattern();
                    neuron->clearSpikes();
                }
            }
        }
        
        if ((i + 1) % 100 == 0) {
            std::cout << "  Processed " << (i + 1) << " images\n";
        }
    }
    
    // Analyze pattern diversity
    std::cout << "\n=== Pattern Diversity Analysis ===\n\n";
    
    // Sample a few neurons to analyze in detail
    std::vector<std::pair<int, int>> sampleNeurons = {
        {24, 0},  // Center region, orientation 0
        {24, 1},  // Center region, orientation 1
        {0, 0},   // Top-left corner
        {48, 0},  // Bottom-right corner
    };
    
    for (const auto& [regionIdx, orient] : sampleNeurons) {
        const auto& neuron = layer1Neurons[regionIdx][orient];
        const auto& patterns = allPatternsSeen[regionIdx][orient];
        
        std::cout << "Neuron [Region " << regionIdx << ", Orient " << orient << "]:\n";
        std::cout << "  Total patterns seen: " << patterns.size() << "\n";
        std::cout << "  Patterns stored in neuron: " << neuron->getReferencePatterns().size() << "\n";
        
        // Count empty patterns
        int emptyCount = 0;
        for (const auto& p : patterns) {
            if (p.empty()) emptyCount++;
        }
        std::cout << "  Empty patterns: " << emptyCount << " (" 
                  << (100.0 * emptyCount / patterns.size()) << "%)\n";
        
        // Analyze pattern sizes
        if (!patterns.empty()) {
            std::vector<size_t> sizes;
            for (const auto& p : patterns) {
                if (!p.empty()) sizes.push_back(p.size());
            }
            if (!sizes.empty()) {
                std::sort(sizes.begin(), sizes.end());
                std::cout << "  Pattern sizes: min=" << sizes.front() 
                          << ", median=" << sizes[sizes.size()/2]
                          << ", max=" << sizes.back() << "\n";
            }
        }
        
        // Compute pairwise similarities between stored patterns
        const auto& storedPatterns = neuron->getReferencePatterns();
        if (storedPatterns.size() >= 2) {
            std::vector<double> similarities;
            for (size_t i = 0; i < storedPatterns.size(); ++i) {
                for (size_t j = i + 1; j < storedPatterns.size(); ++j) {
                    double sim = spikePatternSimilarity(storedPatterns[i], storedPatterns[j]);
                    similarities.push_back(sim);
                }
            }
            
            if (!similarities.empty()) {
                std::sort(similarities.begin(), similarities.end());
                std::cout << "  Stored pattern similarities: min=" << std::fixed << std::setprecision(3)
                          << similarities.front()
                          << ", median=" << similarities[similarities.size()/2]
                          << ", max=" << similarities.back() << "\n";
            }
        }
        
        std::cout << "\n";
    }
    
    // Overall statistics
    std::cout << "=== Overall Statistics ===\n\n";
    
    int totalNeurons = 0;
    int neuronsWithPatterns = 0;
    int totalPatternsStored = 0;
    int totalPatternsMaxedOut = 0;
    
    for (int region = 0; region < NUM_REGIONS; ++region) {
        for (int orient = 0; orient < NUM_ORIENTATIONS; ++orient) {
            totalNeurons++;
            const auto& neuron = layer1Neurons[region][orient];
            int numPatterns = neuron->getReferencePatterns().size();
            
            if (numPatterns > 0) neuronsWithPatterns++;
            totalPatternsStored += numPatterns;
            if (numPatterns >= 100) totalPatternsMaxedOut++;
        }
    }
    
    std::cout << "Total neurons: " << totalNeurons << "\n";
    std::cout << "Neurons with patterns: " << neuronsWithPatterns 
              << " (" << (100.0 * neuronsWithPatterns / totalNeurons) << "%)\n";
    std::cout << "Average patterns per neuron: " 
              << (totalPatternsStored / (double)totalNeurons) << "\n";
    std::cout << "Neurons maxed out (100 patterns): " << totalPatternsMaxedOut
              << " (" << (100.0 * totalPatternsMaxedOut / totalNeurons) << "%)\n";
    
    return 0;
}

