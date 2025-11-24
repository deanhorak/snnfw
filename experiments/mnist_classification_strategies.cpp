/**
 * @file mnist_classification_strategies.cpp
 * @brief MNIST Digit Recognition with Pluggable Classification Strategies
 *
 * This experiment tests different classification strategies on MNIST:
 * - MajorityVoting: Baseline k-NN with equal votes (current: 94.63%)
 * - WeightedDistance: Distance-weighted k-NN (expected: +0.5-1.5%)
 * - WeightedSimilarity: Similarity-weighted k-NN (expected: +0.5-1.5%)
 *
 * Architecture:
 * - RetinaAdapter: 8×8 grid, Sobel operator, Rate encoding (512 neurons)
 * - Classification: Pluggable strategies with k=5
 * - Training: 5000 examples per digit (50,000 total)
 * - Testing: 10,000 images
 *
 * Usage:
 *   ./mnist_classification_strategies <config_file>
 *   ./mnist_classification_strategies ../configs/mnist_sobel_rate_8x8.json
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/adapters/RetinaAdapter.h"
#include "snnfw/adapters/AdapterFactory.h"
#include "snnfw/classification/ClassificationStrategy.h"
#include "snnfw/classification/MajorityVoting.h"
#include "snnfw/classification/WeightedDistance.h"
#include "snnfw/classification/WeightedSimilarity.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>
#include <chrono>

using namespace snnfw;
using namespace snnfw::adapters;
using namespace snnfw::classification;

// Configuration parameters
struct MNISTConfig {
    // Training parameters
    int trainPerDigit;
    int testImages;

    // Classification parameters
    std::string classificationStrategy;
    int kNeighbors;
    double distanceExponent;

    // Data paths
    std::string trainImagesPath;
    std::string trainLabelsPath;
    std::string testImagesPath;
    std::string testLabelsPath;

    // Load from ConfigLoader
    static MNISTConfig fromConfigLoader(const ConfigLoader& config) {
        MNISTConfig cfg;

        // Training parameters
        cfg.trainPerDigit = config.get<int>("/training/examples_per_digit", 5000);
        cfg.testImages = config.get<int>("/training/test_images", 10000);

        // Classification parameters
        cfg.classificationStrategy = config.get<std::string>("/classification/strategy", "majority");
        cfg.kNeighbors = config.get<int>("/classification/k_neighbors", 5);
        cfg.distanceExponent = config.get<double>("/classification/distance_exponent", 2.0);

        // Data paths
        cfg.trainImagesPath = config.getRequired<std::string>("/data/train_images");
        cfg.trainLabelsPath = config.getRequired<std::string>("/data/train_labels");
        cfg.testImagesPath = config.getRequired<std::string>("/data/test_images");
        cfg.testLabelsPath = config.getRequired<std::string>("/data/test_labels");

        return cfg;
    }
};

// Cosine similarity between two vectors
double cosineSimilarity(const std::vector<double>& a, const std::vector<double>& b) {
    double dotProduct = 0.0;
    double normA = 0.0;
    double normB = 0.0;

    for (size_t i = 0; i < a.size(); ++i) {
        dotProduct += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }

    if (normA == 0.0 || normB == 0.0) return 0.0;
    return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
}

// Get activation pattern from retina adapter
std::vector<double> getActivations(RetinaAdapter& retina, const MNISTLoader::Image& img) {
    SensoryAdapter::DataSample sample;
    sample.rawData = img.pixels;
    sample.timestamp = 0.0;

    retina.processData(sample);
    auto activations = retina.getActivationPattern();
    retina.clearNeuronStates();

    return activations;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    try {
        // Load configuration
        ConfigLoader config(argv[1]);
        auto mnistConfig = MNISTConfig::fromConfigLoader(config);

        std::cout << "=== MNIST Classification Strategy Comparison ===" << std::endl;
        std::cout << "Configuration: " << argv[1] << std::endl;
        std::cout << "Training: " << mnistConfig.trainPerDigit << " examples per digit" << std::endl;
        std::cout << "Testing: " << mnistConfig.testImages << " images" << std::endl;
        std::cout << "k-NN: k=" << mnistConfig.kNeighbors << std::endl;
        std::cout << "Strategy: " << mnistConfig.classificationStrategy << std::endl;
        std::cout << "Distance Exponent: " << mnistConfig.distanceExponent << std::endl;
        std::cout << std::endl;

        // Load MNIST data
        std::cout << "Loading MNIST training data..." << std::endl;
        MNISTLoader trainLoader;
        trainLoader.load(mnistConfig.trainImagesPath, mnistConfig.trainLabelsPath, 60000);

        std::cout << "Loading MNIST test data..." << std::endl;
        MNISTLoader testLoader;
        testLoader.load(mnistConfig.testImagesPath, mnistConfig.testLabelsPath, 10000);

        // Create retina adapter from config
        std::cout << "Creating RetinaAdapter..." << std::endl;
        auto adapterConfig = config.getAdapterConfig("retina");
        auto retina = std::make_shared<RetinaAdapter>(adapterConfig);

        retina->initialize();
        std::cout << "RetinaAdapter initialized with " << retina->getNeurons().size() << " neurons" << std::endl;
        std::cout << std::endl;

        // Create classification strategy
        ClassificationStrategy::Config strategyConfig;
        strategyConfig.k = mnistConfig.kNeighbors;
        strategyConfig.numClasses = 10;
        strategyConfig.distanceExponent = mnistConfig.distanceExponent;

        std::unique_ptr<ClassificationStrategy> strategy;
        if (mnistConfig.classificationStrategy == "majority" || mnistConfig.classificationStrategy == "majority_voting") {
            strategy = std::make_unique<MajorityVoting>(strategyConfig);
        } else if (mnistConfig.classificationStrategy == "weighted_distance") {
            strategy = std::make_unique<WeightedDistance>(strategyConfig);
        } else if (mnistConfig.classificationStrategy == "weighted_similarity") {
            strategy = std::make_unique<WeightedSimilarity>(strategyConfig);
        } else {
            std::cerr << "Error: Unknown classification strategy: " << mnistConfig.classificationStrategy << std::endl;
            return 1;
        }

        std::cout << "Using classification strategy: " << strategy->getName() << std::endl;
        std::cout << std::endl;

        // Training phase
        std::cout << "=== Training Phase ===" << std::endl;
        std::vector<ClassificationStrategy::LabeledPattern> trainingPatterns;

        auto trainStart = std::chrono::high_resolution_clock::now();

        // Organize training images by digit
        std::vector<std::vector<size_t>> digitIndices(10);
        for (size_t i = 0; i < trainLoader.size(); ++i) {
            int label = trainLoader.getImage(i).label;
            digitIndices[label].push_back(i);
        }

        for (int digit = 0; digit <= 9; ++digit) {
            std::cout << "Training digit " << digit << "..." << std::endl;

            int count = std::min(mnistConfig.trainPerDigit, static_cast<int>(digitIndices[digit].size()));
            for (int i = 0; i < count; ++i) {
                size_t idx = digitIndices[digit][i];
                const auto& img = trainLoader.getImage(idx);

                // Process image through retina
                SensoryAdapter::DataSample sample;
                sample.rawData = img.pixels;
                sample.timestamp = static_cast<double>(idx);
                sample.metadata["width"] = 28.0;
                sample.metadata["height"] = 28.0;

                retina->processData(sample);

                // Train neurons on this pattern
                auto neurons = retina->getNeurons();
                for (auto& neuron : neurons) {
                    neuron->learnCurrentPattern();
                }

                // Get activation pattern for k-NN (before clearing spikes!)
                auto activations = retina->getActivationPattern();
                trainingPatterns.emplace_back(activations, digit);

                // Clear spikes for next image
                retina->clearNeuronStates();
            }
        }

        auto trainEnd = std::chrono::high_resolution_clock::now();
        auto trainDuration = std::chrono::duration_cast<std::chrono::milliseconds>(trainEnd - trainStart);

        std::cout << "Training complete. Stored " << trainingPatterns.size() << " patterns." << std::endl;
        std::cout << "Training time: " << trainDuration.count() / 1000.0 << " seconds" << std::endl;
        std::cout << std::endl;

        // Testing phase
        std::cout << "=== Testing Phase ===" << std::endl;
        int correct = 0;
        int total = std::min(mnistConfig.testImages, static_cast<int>(testLoader.size()));
        std::vector<int> perDigitCorrect(10, 0);
        std::vector<int> perDigitTotal(10, 0);

        auto testStart = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < total; ++i) {
            if (i % 1000 == 0) {
                std::cout << "Testing sample " << i << "/" << total << std::endl;
            }

            const auto& img = testLoader.getImage(i);
            int trueLabel = img.label;

            // Get activation pattern
            auto activations = getActivations(*retina, img);

            // Classify using strategy
            int predictedLabel = strategy->classify(activations, trainingPatterns, cosineSimilarity);

            if (predictedLabel == trueLabel) {
                correct++;
                perDigitCorrect[trueLabel]++;
            }
            perDigitTotal[trueLabel]++;
        }

        auto testEnd = std::chrono::high_resolution_clock::now();
        auto testDuration = std::chrono::duration_cast<std::chrono::milliseconds>(testEnd - testStart);

        // Print results
        std::cout << std::endl;
        std::cout << "=== Results ===" << std::endl;
        std::cout << "Strategy: " << strategy->getName() << std::endl;
        std::cout << "Overall Accuracy: " << (100.0 * correct / total) << "% (" << correct << "/" << total << ")" << std::endl;
        std::cout << "Testing time: " << testDuration.count() / 1000.0 << " seconds" << std::endl;
        std::cout << std::endl;

        std::cout << "Per-digit accuracy:" << std::endl;
        for (int digit = 0; digit <= 9; ++digit) {
            double accuracy = (perDigitTotal[digit] > 0) ?
                (100.0 * perDigitCorrect[digit] / perDigitTotal[digit]) : 0.0;
            std::cout << "  Digit " << digit << ": " << std::fixed << std::setprecision(1)
                     << accuracy << "% (" << perDigitCorrect[digit] << "/" << perDigitTotal[digit] << ")" << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

