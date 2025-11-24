/**
 * @file test_emnist_loader.cpp
 * @brief Quick test to verify EMNIST Letters dataset loading
 */

#include "snnfw/EMNISTLoader.h"
#include "snnfw/Logger.h"
#include <iostream>
#include <map>

using namespace snnfw;

int main(int argc, char* argv[]) {
    // Initialize logger
    Logger::getInstance().initialize("emnist_test.log", spdlog::level::info);

    std::cout << "=== EMNIST Letters Dataset Loader Test ===" << std::endl;
    std::cout << std::endl;
    
    // Paths to EMNIST letters dataset
    std::string trainImagesPath = "/home/dean/repos/ctm/data/raw/emnist-letters-train-images-idx3-ubyte";
    std::string trainLabelsPath = "/home/dean/repos/ctm/data/raw/emnist-letters-train-labels-idx1-ubyte";
    std::string testImagesPath = "/home/dean/repos/ctm/data/raw/emnist-letters-test-images-idx3-ubyte";
    std::string testLabelsPath = "/home/dean/repos/ctm/data/raw/emnist-letters-test-labels-idx1-ubyte";
    
    // Load training set (first 1000 images for quick test)
    std::cout << "Loading training set..." << std::endl;
    EMNISTLoader trainLoader(EMNISTLoader::Variant::LETTERS);
    if (!trainLoader.load(trainImagesPath, trainLabelsPath, 1000)) {
        std::cerr << "Failed to load training set!" << std::endl;
        return 1;
    }
    
    std::cout << "  Loaded " << trainLoader.size() << " training images" << std::endl;
    std::cout << "  Number of classes: " << trainLoader.getNumClasses() << std::endl;
    std::cout << "  Variant: " << trainLoader.getVariantName() << std::endl;
    std::cout << std::endl;
    
    // Load test set (first 100 images for quick test)
    std::cout << "Loading test set..." << std::endl;
    EMNISTLoader testLoader(EMNISTLoader::Variant::LETTERS);
    if (!testLoader.load(testImagesPath, testLabelsPath, 100)) {
        std::cerr << "Failed to load test set!" << std::endl;
        return 1;
    }
    
    std::cout << "  Loaded " << testLoader.size() << " test images" << std::endl;
    std::cout << std::endl;
    
    // Analyze label distribution in training set
    std::cout << "=== Training Set Label Distribution ===" << std::endl;
    std::map<uint8_t, int> labelCounts;
    for (size_t i = 0; i < trainLoader.size(); ++i) {
        uint8_t label = trainLoader.getImage(i).label;
        labelCounts[label]++;
    }
    
    std::cout << "Label | Char | Count" << std::endl;
    std::cout << "------|------|------" << std::endl;
    for (const auto& [label, count] : labelCounts) {
        char ch = (label >= 1 && label <= 26) ? ('A' + label - 1) : '?';
        std::cout << "  " << static_cast<int>(label) << "   |  " << ch << "   | " << count << std::endl;
    }
    std::cout << std::endl;
    
    // Display a few sample images
    std::cout << "=== Sample Images ===" << std::endl;
    std::cout << std::endl;
    
    for (int i = 0; i < std::min(3, static_cast<int>(testLoader.size())); ++i) {
        std::cout << "Image " << i << ":" << std::endl;
        EMNISTLoader::printImage(testLoader.getImage(i), 128);
        std::cout << std::endl;
    }
    
    // Verify image dimensions
    const auto& img = trainLoader.getImage(0);
    std::cout << "=== Image Properties ===" << std::endl;
    std::cout << "  Dimensions: " << img.rows << "x" << img.cols << std::endl;
    std::cout << "  Pixel count: " << img.pixels.size() << std::endl;
    std::cout << "  Sample pixel values (first 10): ";
    for (int i = 0; i < 10 && i < static_cast<int>(img.pixels.size()); ++i) {
        std::cout << static_cast<int>(img.pixels[i]) << " ";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    
    std::cout << "✅ EMNIST Letters dataset loaded successfully!" << std::endl;
    std::cout << "   Ready to create EMNIST experiments." << std::endl;
    
    return 0;
}

