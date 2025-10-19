/**
 * @file mnist_learning_strategies.cpp
 * @brief MNIST Digit Recognition with Biologically-Plausible Learning Strategies
 *
 * This experiment tests different pattern learning strategies on MNIST:
 * - AppendStrategy: Baseline (current default behavior)
 * - ReplaceWorstStrategy: Synaptic pruning - replaces least-used patterns
 * - MergeSimilarStrategy: Memory consolidation - merges similar patterns
 *
 * All strategies maintain biological plausibility:
 * - Temporal spike patterns (not weights)
 * - Hebbian learning principles
 * - Local learning (no backpropagation)
 * - Capacity limits (finite synaptic resources)
 *
 * Architecture:
 * - RetinaAdapter: 8×8 grid, Sobel operator, Rate encoding (512 neurons)
 * - Classification: MajorityVoting with k=5
 * - Hyperparameters: Optimized (edge_threshold=0.165)
 * - Training: 5000 examples per digit (50,000 total)
 * - Testing: 10,000 images
 * - Baseline: 94.96% accuracy
 *
 * Usage:
 *   ./mnist_learning_strategies <config_file>
 *   ./mnist_learning_strategies ../configs/mnist_learning_append.json
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/adapters/RetinaAdapter.h"
#include "snnfw/adapters/AdapterFactory.h"
#include "snnfw/classification/ClassificationStrategy.h"
#include "snnfw/classification/MajorityVoting.h"
#include "snnfw/learning/PatternUpdateStrategy.h"
#include "snnfw/learning/AppendStrategy.h"
#include "snnfw/learning/ReplaceWorstStrategy.h"
#include "snnfw/learning/MergeSimilarStrategy.h"
#include "snnfw/learning/HybridStrategy.h"
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
using namespace snnfw::learning;

// Configuration parameters
struct MNISTConfig {
    // Training parameters
    int trainPerDigit;
    int testImages;

    // Learning parameters
    std::string learningStrategy;
    double blendAlpha;
    double mergeWeight;

    // Classification parameters
    int kNeighbors;

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

        // Learning parameters
        cfg.learningStrategy = config.get<std::string>("/learning/strategy", "append");
        cfg.blendAlpha = config.get<double>("/learning/blend_alpha", 0.2);
        cfg.mergeWeight = config.get<double>("/learning/merge_weight", 0.3);

        // Classification parameters
        cfg.kNeighbors = config.get<int>("/classification/k_neighbors", 5);

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
    sample.metadata["width"] = 28.0;
    sample.metadata["height"] = 28.0;

    retina.processData(sample);

    // Get activation pattern
    std::vector<double> activations;
    auto neurons = retina.getNeurons();
    for (const auto& neuron : neurons) {
        double bestSim = neuron->getBestSimilarity();
        activations.push_back(bestSim);
    }

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

        // Create pattern update strategy
        PatternUpdateStrategy::Config strategyConfig;
        strategyConfig.maxPatterns = config.get<size_t>("/neuron/max_patterns", 100);
        strategyConfig.similarityThreshold = config.get<double>("/neuron/similarity_threshold", 0.7);

        std::shared_ptr<PatternUpdateStrategy> strategy;
        std::string strategyName = mnistConfig.learningStrategy;
        std::transform(strategyName.begin(), strategyName.end(), strategyName.begin(), ::tolower);

        if (strategyName == "append") {
            strategy = std::make_shared<AppendStrategy>(strategyConfig);
        } else if (strategyName == "replace_worst" || strategyName == "replaceworst") {
            strategy = std::make_shared<ReplaceWorstStrategy>(strategyConfig);
        } else if (strategyName == "merge_similar" || strategyName == "mergesimilar") {
            strategy = std::make_shared<MergeSimilarStrategy>(strategyConfig);
        } else if (strategyName == "hybrid") {
            // Add hybrid-specific parameters to config
            strategyConfig.doubleParams["merge_threshold"] = config.get<double>("/learning/merge_threshold", 0.85);
            strategyConfig.doubleParams["merge_weight"] = config.get<double>("/learning/merge_weight", 0.3);
            strategyConfig.doubleParams["blend_alpha"] = config.get<double>("/learning/blend_alpha", 0.2);
            strategyConfig.intParams["prune_threshold"] = config.get<int>("/learning/prune_threshold", 2);
            strategy = std::make_shared<HybridStrategy>(strategyConfig);
        } else {
            std::cerr << "Unknown learning strategy: " << mnistConfig.learningStrategy << std::endl;
            return 1;
        }

        std::cout << "Using learning strategy: " << strategy->getName() << std::endl;
        std::cout << std::endl;

        // Set strategy for all neurons
        auto neurons = retina->getNeurons();
        for (auto& neuron : neurons) {
            neuron->setPatternUpdateStrategy(strategy);
        }

        // Organize training data by digit
        std::vector<std::vector<size_t>> digitIndices(10);
        for (size_t i = 0; i < trainLoader.size(); ++i) {
            uint8_t label = trainLoader.getImage(i).label;
            digitIndices[label].push_back(i);
        }

        // Training phase
        std::cout << "=== Training Phase ===" << std::endl;
        auto trainStart = std::chrono::high_resolution_clock::now();

        std::vector<ClassificationStrategy::LabeledPattern> trainingPatterns;

        for (int digit = 0; digit < 10; ++digit) {
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

                // Train neurons on this pattern (using the configured strategy)
                for (auto& neuron : neurons) {
                    neuron->learnCurrentPattern();
                }

                // Store activation pattern for classification
                auto activations = getActivations(*retina, img);
                trainingPatterns.emplace_back(activations, digit);

                // Clear spikes for next image
                for (auto& neuron : neurons) {
                    neuron->clearSpikes();
                }
            }
        }

        auto trainEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> trainDuration = trainEnd - trainStart;

        std::cout << "Training complete. Stored " << trainingPatterns.size() << " patterns." << std::endl;
        std::cout << "Training time: " << trainDuration.count() << " seconds" << std::endl;
        std::cout << std::endl;

        // Create classification strategy
        ClassificationStrategy::Config classConfig;
        classConfig.k = mnistConfig.kNeighbors;
        classConfig.numClasses = 10;
        auto classifier = std::make_shared<MajorityVoting>(classConfig);

        // Testing phase
        std::cout << "=== Testing Phase ===" << std::endl;
        auto testStart = std::chrono::high_resolution_clock::now();

        int correct = 0;
        std::vector<int> digitCorrect(10, 0);
        std::vector<int> digitTotal(10, 0);

        for (int i = 0; i < std::min(mnistConfig.testImages, static_cast<int>(testLoader.size())); ++i) {
            if (i % 1000 == 0) {
                std::cout << "Testing sample " << i << "/" << mnistConfig.testImages << std::endl;
            }

            const auto& img = testLoader.getImage(i);
            auto activations = getActivations(*retina, img);

            // Classify using k-NN
            int predicted = classifier->classify(activations, trainingPatterns, cosineSimilarity);

            if (predicted == img.label) {
                correct++;
                digitCorrect[img.label]++;
            }
            digitTotal[img.label]++;

            // Clear spikes for next image
            for (auto& neuron : neurons) {
                neuron->clearSpikes();
            }
        }

        auto testEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> testDuration = testEnd - testStart;

        // Print results
        std::cout << std::endl;
        std::cout << "=== Results ===" << std::endl;
        std::cout << "Learning Strategy: " << strategy->getName() << std::endl;
        std::cout << "Overall Accuracy: " << std::fixed << std::setprecision(2)
                  << (100.0 * correct / mnistConfig.testImages) << "% ("
                  << correct << "/" << mnistConfig.testImages << ")" << std::endl;
        std::cout << "Testing time: " << testDuration.count() << " seconds" << std::endl;
        std::cout << std::endl;

        std::cout << "Per-digit accuracy:" << std::endl;
        for (int digit = 0; digit < 10; ++digit) {
            std::cout << "  Digit " << digit << ": "
                      << std::fixed << std::setprecision(1)
                      << (100.0 * digitCorrect[digit] / digitTotal[digit]) << "% ("
                      << digitCorrect[digit] << "/" << digitTotal[digit] << ")" << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

