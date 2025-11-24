/**
 * @file mnist_hyperparameter_optimization.cpp
 * @brief Hyperparameter optimization for MNIST experiment
 * 
 * Automatically searches for optimal hyperparameters using various strategies:
 * - Grid Search
 * - Random Search
 * - Bayesian Optimization
 * - Genetic Algorithm
 */

#include "snnfw/HyperparameterOptimizer.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/adapters/RetinaAdapter.h"
#include "snnfw/MNISTLoader.h"
#include "snnfw/Logger.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>

using namespace snnfw;
using namespace snnfw::adapters;

// Training pattern for k-NN
struct TrainingPattern {
    std::vector<double> activations;
    int label;
};

// Calculate cosine similarity
double cosineSimilarity(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) return 0.0;
    
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

// k-NN classification
int classifyKNN(const std::vector<double>& testPattern,
                const std::vector<TrainingPattern>& trainingPatterns,
                int k) {
    std::vector<std::pair<double, int>> similarities;
    for (const auto& pattern : trainingPatterns) {
        double sim = cosineSimilarity(testPattern, pattern.activations);
        similarities.push_back({sim, pattern.label});
    }
    
    std::sort(similarities.begin(), similarities.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    std::vector<int> votes(10, 0);
    for (int i = 0; i < k && i < static_cast<int>(similarities.size()); ++i) {
        votes[similarities[i].second]++;
    }
    
    return std::max_element(votes.begin(), votes.end()) - votes.begin();
}

// Create MNIST objective function
ObjectiveFunction createMNISTObjective(const std::string& baseConfigPath,
                                      int trainSamplesPerDigit = 1000,
                                      int testSamples = 1000) {
    
    return [baseConfigPath, trainSamplesPerDigit, testSamples](
        const ParameterConfig& params) -> ExperimentResult {
        
        auto startTime = std::chrono::steady_clock::now();
        
        // Load base configuration
        ConfigLoader loader(baseConfigPath);
        
        // Override with optimization parameters
        for (const auto& [key, value] : params.intParams) {
            // Parse key like "network.grid_size" into section and param
            size_t dotPos = key.find('.');
            if (dotPos != std::string::npos) {
                std::string section = key.substr(0, dotPos);
                std::string param = key.substr(dotPos + 1);
                // Note: ConfigLoader doesn't support runtime modification
                // We'll create adapter config directly
            }
        }
        
        // Create RetinaAdapter with optimized parameters
        BaseAdapter::Config retinaConfig;
        retinaConfig.name = "retina";
        retinaConfig.type = "retina";
        retinaConfig.temporalWindow = params.doubleParams.count("neuron.window_size_ms") ?
            params.doubleParams.at("neuron.window_size_ms") : 200.0;
        
        retinaConfig.intParams["grid_width"] = params.intParams.count("network.grid_size") ?
            params.intParams.at("network.grid_size") : 7;
        retinaConfig.intParams["grid_height"] = retinaConfig.intParams["grid_width"];
        retinaConfig.intParams["num_orientations"] = params.intParams.count("network.num_orientations") ?
            params.intParams.at("network.num_orientations") : 8;
        
        retinaConfig.doubleParams["edge_threshold"] = params.doubleParams.count("network.edge_threshold") ?
            params.doubleParams.at("network.edge_threshold") : 0.15;
        retinaConfig.doubleParams["similarity_threshold"] = params.doubleParams.count("neuron.similarity_threshold") ?
            params.doubleParams.at("neuron.similarity_threshold") : 0.7;
        
        int maxPatterns = params.intParams.count("neuron.max_patterns") ?
            params.intParams.at("neuron.max_patterns") : 100;
        retinaConfig.intParams["max_patterns"] = maxPatterns;
        
        int kNeighbors = params.intParams.count("classification.k_neighbors") ?
            params.intParams.at("classification.k_neighbors") : 5;
        
        // Load MNIST training data (use hardcoded paths for now)
        std::string trainImages = "/home/dean/repos/ctm/data/MNIST/raw/train-images-idx3-ubyte";
        std::string trainLabels = "/home/dean/repos/ctm/data/MNIST/raw/train-labels-idx1-ubyte";
        std::string testImages = "/home/dean/repos/ctm/data/MNIST/raw/t10k-images-idx3-ubyte";
        std::string testLabels = "/home/dean/repos/ctm/data/MNIST/raw/t10k-labels-idx1-ubyte";

        MNISTLoader trainLoader;
        if (!trainLoader.load(trainImages, trainLabels, trainSamplesPerDigit * 10)) {
            SNNFW_ERROR("Failed to load MNIST training data");
            ExperimentResult result;
            result.config = params;
            result.score = 0.0;
            return result;
        }

        MNISTLoader testLoader;
        if (!testLoader.load(testImages, testLabels, testSamples)) {
            SNNFW_ERROR("Failed to load MNIST test data");
            ExperimentResult result;
            result.config = params;
            result.score = 0.0;
            return result;
        }

        // Create adapter AFTER loading data to avoid memory issues
        auto retina = std::make_shared<RetinaAdapter>(retinaConfig);
        if (!retina->initialize()) {
            SNNFW_ERROR("Failed to initialize RetinaAdapter");
            ExperimentResult result;
            result.config = params;
            result.score = 0.0;
            return result;
        }

        // Training phase
        std::vector<TrainingPattern> trainingPatterns;

        for (size_t i = 0; i < trainLoader.size(); ++i) {
            const auto& img = trainLoader.getImage(i);

            SensoryAdapter::DataSample dataSample;
            dataSample.rawData = img.pixels;
            dataSample.timestamp = 0.0;

            retina->processData(dataSample);

            auto neurons = retina->getNeurons();
            for (auto& neuron : neurons) {
                neuron->learnCurrentPattern();
            }

            TrainingPattern pattern;
            pattern.activations = retina->getActivationPattern();
            pattern.label = img.label;
            trainingPatterns.push_back(pattern);

            retina->clearNeuronStates();
        }

        auto trainEndTime = std::chrono::steady_clock::now();
        double trainingTime = std::chrono::duration<double>(trainEndTime - startTime).count();

        // Testing phase
        int correct = 0;
        int total = testLoader.size();

        for (size_t i = 0; i < testLoader.size(); ++i) {
            const auto& img = testLoader.getImage(i);

            SensoryAdapter::DataSample dataSample;
            dataSample.rawData = img.pixels;
            dataSample.timestamp = 0.0;

            retina->processData(dataSample);
            auto activations = retina->getActivationPattern();

            int predictedLabel = classifyKNN(activations, trainingPatterns, kNeighbors);

            if (predictedLabel == static_cast<int>(img.label)) {
                correct++;
            }

            retina->clearNeuronStates();
        }
        
        auto endTime = std::chrono::steady_clock::now();
        double testingTime = std::chrono::duration<double>(endTime - trainEndTime).count();
        
        // Create result
        ExperimentResult result;
        result.config = params;
        result.score = static_cast<double>(correct) / total;
        result.trainingTime = trainingTime;
        result.testingTime = testingTime;
        result.metrics["accuracy"] = result.score;
        result.metrics["correct"] = correct;
        result.metrics["total"] = total;
        result.metrics["num_neurons"] = retina->getNeurons().size();
        result.metrics["training_samples"] = trainingPatterns.size();
        
        SNNFW_INFO("Accuracy: {:.2f}% ({}/{})", result.score * 100.0, correct, total);
        SNNFW_INFO("Training time: {:.2f}s, Testing time: {:.2f}s", 
                   trainingTime, testingTime);
        
        return result;
    };
}

int main(int argc, char* argv[]) {
    std::cout << "=== MNIST Hyperparameter Optimization ===" << std::endl;
    
    // Parse command line arguments
    std::string baseConfig = (argc > 1) ? argv[1] : "../configs/mnist_config.json";
    std::string strategy = (argc > 2) ? argv[2] : "random";
    int maxIterations = (argc > 3) ? std::atoi(argv[3]) : 50;
    int trainSamples = (argc > 4) ? std::atoi(argv[4]) : 1000;
    int testSamples = (argc > 5) ? std::atoi(argv[5]) : 1000;
    
    std::cout << "Base config: " << baseConfig << std::endl;
    std::cout << "Strategy: " << strategy << std::endl;
    std::cout << "Max iterations: " << maxIterations << std::endl;
    std::cout << "Training samples per digit: " << trainSamples << std::endl;
    std::cout << "Test samples: " << testSamples << std::endl;
    std::cout << std::endl;
    
    // Create optimizer
    OptimizationStrategy strategyEnum;
    if (strategy == "grid") {
        strategyEnum = OptimizationStrategy::GRID_SEARCH;
    } else if (strategy == "random") {
        strategyEnum = OptimizationStrategy::RANDOM_SEARCH;
    } else if (strategy == "bayesian") {
        strategyEnum = OptimizationStrategy::BAYESIAN;
    } else if (strategy == "genetic") {
        strategyEnum = OptimizationStrategy::GENETIC_ALGORITHM;
    } else {
        std::cerr << "Unknown strategy: " << strategy << std::endl;
        std::cerr << "Valid strategies: grid, random, bayesian, genetic" << std::endl;
        return 1;
    }
    
    auto optimizer = std::make_shared<HyperparameterOptimizer>(strategyEnum);
    optimizer->setMaxIterations(maxIterations);
    optimizer->setResultsDir("optimization_results");
    optimizer->setSaveIntermediateResults(true);
    
    // Add MNIST-specific parameters
    MNISTOptimizer::addMNISTParameters(*optimizer);
    
    // Set objective function
    optimizer->setObjective(createMNISTObjective(baseConfig, trainSamples, testSamples));
    
    // Run optimization
    std::cout << "Starting optimization..." << std::endl;
    auto best = optimizer->optimize();
    
    // Print results
    std::cout << "\n=== Optimization Complete ===" << std::endl;
    std::cout << "Best accuracy: " << (best.score * 100.0) << "%" << std::endl;
    std::cout << "Training time: " << best.trainingTime << "s" << std::endl;
    std::cout << "Testing time: " << best.testingTime << "s" << std::endl;
    std::cout << "\nBest parameters:" << std::endl;
    
    for (const auto& [key, value] : best.config.intParams) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    for (const auto& [key, value] : best.config.doubleParams) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    for (const auto& [key, value] : best.config.stringParams) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    
    std::cout << "\nResults saved to: optimization_results/" << std::endl;
    
    return 0;
}

