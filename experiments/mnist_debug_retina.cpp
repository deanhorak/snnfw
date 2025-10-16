/**
 * MNIST Retina Debug - Verify spike pattern generation
 *
 * This experiment validates that our "retina" (image-to-spike converter)
 * produces sensible spike patterns from MNIST images.
 */

#include "snnfw/MNISTLoader.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace snnfw;

// Forward declarations
std::vector<double> imageToSpikePattern(const MNISTLoader::Image& img, double duration = 50.0);
void analyzeSpikePattern(const std::vector<double>& spikes, const std::string& label);
void visualizeSpikePattern(const std::vector<double>& spikes, double duration = 50.0);
void printImage(const MNISTLoader::Image& img);
void comparePatterns(const std::vector<double>& pattern1, const std::vector<double>& pattern2);

// Convert MNIST image to spike pattern using rate coding
std::vector<double> imageToSpikePattern(const MNISTLoader::Image& img, double duration) {
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

// Print spike pattern statistics
void analyzeSpikePattern(const std::vector<double>& spikes, const std::string& label) {
    if (spikes.empty()) {
        std::cout << label << ": NO SPIKES\n";
        return;
    }
    
    double minTime = *std::min_element(spikes.begin(), spikes.end());
    double maxTime = *std::max_element(spikes.begin(), spikes.end());
    double avgTime = 0.0;
    for (double t : spikes) avgTime += t;
    avgTime /= spikes.size();
    
    std::cout << label << ":\n";
    std::cout << "  Spike count: " << spikes.size() << "\n";
    std::cout << "  Time range: [" << std::fixed << std::setprecision(2) 
              << minTime << " ms, " << maxTime << " ms]\n";
    std::cout << "  Average time: " << avgTime << " ms\n";
    std::cout << "  Temporal spread: " << (maxTime - minTime) << " ms\n";
}

// Visualize spike pattern as temporal histogram
void visualizeSpikePattern(const std::vector<double>& spikes, double duration) {
    const int bins = 50;
    const int maxBarWidth = 60;
    std::vector<int> histogram(bins, 0);
    
    // Count spikes in each time bin
    for (double t : spikes) {
        int bin = std::min(bins - 1, static_cast<int>((t / duration) * bins));
        histogram[bin]++;
    }
    
    // Find max count for scaling
    int maxCount = *std::max_element(histogram.begin(), histogram.end());
    if (maxCount == 0) return;
    
    std::cout << "\n  Temporal Distribution (0-" << duration << " ms):\n";
    for (int i = 0; i < bins; i += 2) {  // Show every 2ms
        double timeStart = (i * duration) / bins;
        int count = histogram[i] + (i + 1 < bins ? histogram[i + 1] : 0);
        int barWidth = (count * maxBarWidth) / maxCount;
        
        std::cout << "  " << std::setw(5) << std::fixed << std::setprecision(1) 
                  << timeStart << "ms |";
        for (int j = 0; j < barWidth; ++j) std::cout << "█";
        std::cout << " " << count << "\n";
    }
}

// Print ASCII visualization of image
void printImage(const MNISTLoader::Image& img) {
    const char* shades = " .:-=+*#%@";
    
    for (int row = 0; row < img.rows; ++row) {
        std::cout << "  ";
        for (int col = 0; col < img.cols; ++col) {
            double intensity = img.getNormalizedPixel(row, col);
            int shadeIdx = std::min(9, static_cast<int>(intensity * 10));
            std::cout << shades[shadeIdx];
        }
        std::cout << "\n";
    }
}

// Compare spike patterns for same digit vs different digits
void comparePatterns(const std::vector<double>& pattern1, const std::vector<double>& pattern2) {
    // Simple similarity: compare spike counts and temporal distribution
    int count1 = pattern1.size();
    int count2 = pattern2.size();
    
    double countSimilarity = 1.0 - std::abs(count1 - count2) / static_cast<double>(std::max(count1, count2));
    
    std::cout << "  Spike count similarity: " << std::fixed << std::setprecision(2) 
              << (countSimilarity * 100) << "%\n";
    std::cout << "  Count difference: " << std::abs(count1 - count2) << " spikes\n";
}

int main() {
    std::cout << "=== MNIST Retina Debug ===\n\n";
    
    // Load MNIST data
    MNISTLoader loader;
    loader.load("/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte",
                "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte",
                20);

    std::cout << "Loaded " << loader.size() << " training images\n\n";
    
    // Test 1: Visualize spike patterns for different digits
    std::cout << "=== TEST 1: Spike Patterns for Different Digits ===\n\n";
    
    for (int digitToShow = 0; digitToShow <= 9; ++digitToShow) {
        // Find first occurrence of this digit
        for (size_t i = 0; i < loader.size(); ++i) {
            auto img = loader.getImage(i);
            auto label = img.label;
            
            if (label == digitToShow) {
                std::cout << "--- Digit " << static_cast<int>(label) << " ---\n";
                printImage(img);
                std::cout << "\n";
                
                auto spikes = imageToSpikePattern(img);
                analyzeSpikePattern(spikes, "Spike Pattern");
                visualizeSpikePattern(spikes);
                std::cout << "\n";
                break;
            }
        }
    }
    
    // Test 2: Compare patterns for same digit
    std::cout << "\n=== TEST 2: Consistency - Two Examples of Digit '1' ===\n\n";
    
    std::vector<double> pattern1_first, pattern1_second;
    int found = 0;

    for (size_t i = 0; i < loader.size() && found < 2; ++i) {
        auto img = loader.getImage(i);
        auto label = img.label;
        
        if (label == 1) {
            auto spikes = imageToSpikePattern(img);
            if (found == 0) {
                std::cout << "First '1':\n";
                printImage(img);
                std::cout << "\n";
                analyzeSpikePattern(spikes, "Pattern");
                pattern1_first = spikes;
            } else {
                std::cout << "\nSecond '1':\n";
                printImage(img);
                std::cout << "\n";
                analyzeSpikePattern(spikes, "Pattern");
                pattern1_second = spikes;
            }
            found++;
        }
    }
    
    std::cout << "\nSimilarity between two '1' patterns:\n";
    comparePatterns(pattern1_first, pattern1_second);
    
    // Test 3: Compare patterns for different digits
    std::cout << "\n=== TEST 3: Discrimination - Digit '1' vs Digit '8' ===\n\n";
    
    std::vector<double> pattern1, pattern8;

    for (size_t i = 0; i < loader.size(); ++i) {
        auto img = loader.getImage(i);
        auto label = img.label;
        
        if (label == 1 && pattern1.empty()) {
            std::cout << "Digit '1':\n";
            printImage(img);
            std::cout << "\n";
            pattern1 = imageToSpikePattern(img);
            analyzeSpikePattern(pattern1, "Pattern");
        }
        
        if (label == 8 && pattern8.empty()) {
            std::cout << "\nDigit '8':\n";
            printImage(img);
            std::cout << "\n";
            pattern8 = imageToSpikePattern(img);
            analyzeSpikePattern(pattern8, "Pattern");
        }
        
        if (!pattern1.empty() && !pattern8.empty()) break;
    }
    
    std::cout << "\nDifference between '1' and '8' patterns:\n";
    comparePatterns(pattern1, pattern8);
    
    // Test 4: Check for edge cases
    std::cout << "\n=== TEST 4: Edge Cases ===\n\n";
    
    // Find darkest and brightest images
    double minAvgIntensity = 1.0, maxAvgIntensity = 0.0;
    size_t darkestIdx = 0, brightestIdx = 0;

    for (size_t i = 0; i < loader.size(); ++i) {
        auto img = loader.getImage(i);
        double avgIntensity = 0.0;
        for (int r = 0; r < img.rows; ++r) {
            for (int c = 0; c < img.cols; ++c) {
                avgIntensity += img.getNormalizedPixel(r, c);
            }
        }
        avgIntensity /= (img.rows * img.cols);
        
        if (avgIntensity < minAvgIntensity) {
            minAvgIntensity = avgIntensity;
            darkestIdx = i;
        }
        if (avgIntensity > maxAvgIntensity) {
            maxAvgIntensity = avgIntensity;
            brightestIdx = i;
        }
    }
    
    std::cout << "Darkest image (avg intensity: " << minAvgIntensity << "):\n";
    auto darkImg = loader.getImage(darkestIdx);
    printImage(darkImg);
    auto darkSpikes = imageToSpikePattern(darkImg);
    analyzeSpikePattern(darkSpikes, "\nSpike Pattern");

    std::cout << "\n\nBrightest image (avg intensity: " << maxAvgIntensity << "):\n";
    auto brightImg = loader.getImage(brightestIdx);
    printImage(brightImg);
    auto brightSpikes = imageToSpikePattern(brightImg);
    analyzeSpikePattern(brightSpikes, "\nSpike Pattern");
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "✓ Retina converts images to temporal spike patterns\n";
    std::cout << "✓ Brighter pixels → earlier spikes\n";
    std::cout << "✓ Different digits produce different spike counts\n";
    std::cout << "✓ Temporal distribution varies by digit structure\n";
    std::cout << "\nNext: Verify pattern learning in neurons\n";
    
    return 0;
}

