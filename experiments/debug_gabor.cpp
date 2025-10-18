/**
 * Debug program to test Gabor filter on MNIST images
 * Prints feature values to understand what's going wrong
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/features/GaborOperator.h"
#include "snnfw/features/SobelOperator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

using namespace snnfw;

void printFeatures(const std::string& name, const std::vector<double>& features) {
    std::cout << name << ": [";
    for (size_t i = 0; i < features.size(); ++i) {
        std::cout << std::fixed << std::setprecision(4) << features[i];
        if (i < features.size() - 1) std::cout << ", ";
    }
    std::cout << "]" << std::endl;
}

void printRegion(const std::vector<uint8_t>& region, int regionSize) {
    std::cout << "Region (" << regionSize << "x" << regionSize << "):" << std::endl;
    for (int r = 0; r < regionSize; ++r) {
        for (int c = 0; c < regionSize; ++c) {
            int val = region[r * regionSize + c];
            std::cout << std::setw(4) << val;
        }
        std::cout << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Load MNIST data
    std::string trainImages = "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte";
    std::string trainLabels = "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte";

    MNISTLoader loader;
    if (!loader.load(trainImages, trainLabels, 1000)) {
        std::cerr << "Failed to load MNIST data" << std::endl;
        return 1;
    }

    std::cout << "Loaded " << loader.size() << " images" << std::endl;
    
    // Test parameters
    int gridSize = 7;
    int imageSize = 28;
    int regionSize = imageSize / gridSize; // 4x4
    int numOrientations = 8;
    
    std::cout << "\nRegion size: " << regionSize << "x" << regionSize << std::endl;
    
    // Create Gabor operator with different parameter sets
    std::vector<std::tuple<double, double, int, std::string>> paramSets = {
        {4.0, 2.0, 5, "Original (wavelength=4.0, sigma=2.0, kernel=5)"},
        {2.0, 1.0, 3, "Adjusted (wavelength=2.0, sigma=1.0, kernel=3)"},
        {1.5, 0.8, 3, "Smaller (wavelength=1.5, sigma=0.8, kernel=3)"},
    };
    
    // Create Sobel for comparison
    features::EdgeOperator::Config sobelConfig;
    sobelConfig.name = "sobel";
    sobelConfig.numOrientations = numOrientations;
    sobelConfig.edgeThreshold = 0.0;
    auto sobelOp = std::make_unique<features::SobelOperator>(sobelConfig);
    
    // Test on first image of each digit
    for (int digit = 0; digit < 10; ++digit) {
        // Find first image of this digit
        int imageIdx = -1;
        for (size_t i = 0; i < loader.size(); ++i) {
            if (loader.getImage(i).label == digit) {
                imageIdx = i;
                break;
            }
        }

        if (imageIdx < 0) continue;

        std::cout << "\n========================================" << std::endl;
        std::cout << "DIGIT " << digit << " (image " << imageIdx << ")" << std::endl;
        std::cout << "========================================" << std::endl;

        // Get image
        const auto& image = loader.getImage(imageIdx);

        // Extract center region (3,3) for testing
        int regionRow = 3;
        int regionCol = 3;
        std::vector<uint8_t> region;
        for (int r = 0; r < regionSize; ++r) {
            for (int c = 0; c < regionSize; ++c) {
                int imgR = regionRow * regionSize + r;
                int imgC = regionCol * regionSize + c;
                region.push_back(image.pixels[imgR * imageSize + imgC]);
            }
        }
        
        printRegion(region, regionSize);
        std::cout << std::endl;
        
        // Test Sobel
        auto sobelFeatures = sobelOp->extractEdges(region, regionSize);
        printFeatures("Sobel", sobelFeatures);
        
        // Test each Gabor parameter set
        for (const auto& params : paramSets) {
            double wavelength = std::get<0>(params);
            double sigma = std::get<1>(params);
            int kernelSize = std::get<2>(params);
            std::string desc = std::get<3>(params);
            
            features::EdgeOperator::Config gaborConfig;
            gaborConfig.name = "gabor";
            gaborConfig.numOrientations = numOrientations;
            gaborConfig.edgeThreshold = 0.0;
            gaborConfig.doubleParams["wavelength"] = wavelength;
            gaborConfig.doubleParams["sigma"] = sigma;
            gaborConfig.doubleParams["gamma"] = 0.5;
            gaborConfig.doubleParams["phase_offset"] = 0.0;
            gaborConfig.intParams["kernel_size"] = kernelSize;
            
            auto gaborOp = std::make_unique<features::GaborOperator>(gaborConfig);
            auto gaborFeatures = gaborOp->extractEdges(region, regionSize);
            
            printFeatures(desc, gaborFeatures);
        }
    }
    
    return 0;
}

