/**
 * @file mnist_with_adapters.cpp
 * @brief MNIST Digit Recognition using Adapter System
 *
 * This is a refactored version of mnist_optimized.cpp that uses the new adapter system.
 * It demonstrates how to use RetinaAdapter for visual processing instead of inline
 * edge detection and spike encoding.
 *
 * Architecture:
 * - RetinaAdapter: Handles visual processing (7×7 grid, 8 orientations, 392 neurons)
 * - k-NN Classification: Same as original (k=5, cosine similarity)
 * - Expected Accuracy: 81.20% (same as original)
 *
 * Usage:
 *   ./mnist_with_adapters <config_file>
 *   ./mnist_with_adapters ../configs/mnist_config_with_adapters.json
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/adapters/RetinaAdapter.h"
#include "snnfw/adapters/AdapterFactory.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>

using namespace snnfw;
using namespace snnfw::adapters;

// Configuration parameters
struct MNISTConfig {
    // Training parameters
    int trainPerDigit;
    int testImages;

    // Classification parameters
    std::string classificationMethod;
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

        // Classification parameters
        cfg.classificationMethod = config.get<std::string>("/classification/method", "knn");
        cfg.kNeighbors = config.get<int>("/classification/k_neighbors", 5);

        // Data paths
        cfg.trainImagesPath = config.getRequired<std::string>("/data/train_images");
        cfg.trainLabelsPath = config.getRequired<std::string>("/data/train_labels");
        cfg.testImagesPath = config.getRequired<std::string>("/data/test_images");
        cfg.testLabelsPath = config.getRequired<std::string>("/data/test_labels");

        return cfg;
    }
};

// Training pattern with label
struct TrainingPattern {
    std::vector<double> activations;
    int label;
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

// k-NN classification
int classifyKNN(const std::vector<double>& testActivations,
                const std::vector<TrainingPattern>& trainingPatterns,
                int k) {
    // Calculate similarities to all training patterns
    std::vector<std::pair<double, int>> similarities;
    for (const auto& pattern : trainingPatterns) {
        double sim = cosineSimilarity(testActivations, pattern.activations);
        similarities.push_back({sim, pattern.label});
    }

    // Sort by similarity (descending)
    std::sort(similarities.begin(), similarities.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    // Vote among k nearest neighbors
    std::vector<int> votes(10, 0);
    for (int i = 0; i < k && i < static_cast<int>(similarities.size()); ++i) {
        votes[similarities[i].second]++;
    }

    // Return label with most votes
    return std::max_element(votes.begin(), votes.end()) - votes.begin();
}

// Get activation pattern from retina adapter
std::vector<double> getActivations(RetinaAdapter& retina, const MNISTLoader::Image& img) {
    // Process image through retina adapter
    SensoryAdapter::DataSample sample;
    sample.rawData = img.pixels;
    sample.timestamp = 0.0;
    
    // Add metadata
    sample.metadata["width"] = 28.0;
    sample.metadata["height"] = 28.0;
    
    // Process through adapter pipeline
    retina.processData(sample);
    
    // Get activation pattern
    return retina.getActivationPattern();
}

// Train retina adapter on images
void trainRetina(RetinaAdapter& retina,
                 const std::vector<MNISTLoader::Image>& images,
                 int startIdx, int count) {
    std::cout << "Training retina adapter on " << count << " images..." << std::endl;
    
    for (int i = 0; i < count && (startIdx + i) < static_cast<int>(images.size()); ++i) {
        const auto& img = images[startIdx + i];
        
        // Process image
        SensoryAdapter::DataSample sample;
        sample.rawData = img.pixels;
        sample.timestamp = static_cast<double>(i);
        sample.metadata["width"] = 28.0;
        sample.metadata["height"] = 28.0;
        
        // Process and learn
        retina.processData(sample);
        
        // Train neurons (learn current patterns)
        auto neurons = retina.getNeurons();
        for (auto& neuron : neurons) {
            neuron->learnCurrentPattern();
            neuron->clearSpikes();
        }
        
        // Clear adapter state for next image
        retina.clearNeuronStates();
        
        if ((i + 1) % 1000 == 0) {
            std::cout << "  Trained on " << (i + 1) << " images" << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ../configs/mnist_config_with_adapters.json" << std::endl;
        return 1;
    }

    try {
        // Load configuration
        std::cout << "Loading configuration from: " << argv[1] << std::endl;
        ConfigLoader configLoader(argv[1]);
        MNISTConfig config = MNISTConfig::fromConfigLoader(configLoader);

        // Create retina adapter from configuration
        std::cout << "Creating retina adapter..." << std::endl;
        auto retinaConfig = configLoader.getAdapterConfig("retina");
        auto retina = std::make_shared<RetinaAdapter>(retinaConfig);
        retina->initialize();
        
        std::cout << "Retina adapter created with:" << std::endl;
        std::cout << "  Grid size: " << retina->getIntParam("grid_size", 7) << std::endl;
        std::cout << "  Orientations: " << retina->getIntParam("num_orientations", 8) << std::endl;
        std::cout << "  Total neurons: " << retina->getNeurons().size() << std::endl;

        // Load MNIST data
        std::cout << "\nLoading MNIST data..." << std::endl;
        MNISTLoader trainLoader;
        trainLoader.load(config.trainImagesPath, config.trainLabelsPath, 60000);

        MNISTLoader testLoader;
        testLoader.load(config.testImagesPath, config.testLabelsPath, config.testImages);

        std::cout << "Loaded " << trainLoader.size() << " training images" << std::endl;
        std::cout << "Loaded " << testLoader.size() << " test images" << std::endl;

        // Train retina neurons and build activation patterns (combined phase)
        std::cout << "\n=== Training Phase ===" << std::endl;
        std::cout << "Training retina neurons and building patterns..." << std::endl;

        std::vector<TrainingPattern> trainingPatterns;
        std::vector<int> trainCount(10, 0);

        const auto& trainImages = trainLoader.getImages();
        for (size_t i = 0; i < trainImages.size(); ++i) {
            int label = trainImages[i].label;

            if (trainCount[label] >= config.trainPerDigit) continue;

            // Process image through retina
            SensoryAdapter::DataSample sample;
            sample.rawData = trainImages[i].pixels;
            sample.timestamp = static_cast<double>(i);
            sample.metadata["width"] = 28.0;
            sample.metadata["height"] = 28.0;

            retina->processData(sample);

            // Train neurons on this pattern
            auto neurons = retina->getNeurons();
            for (auto& neuron : neurons) {
                neuron->learnCurrentPattern();
            }

            // Get activation pattern for k-NN (before clearing spikes!)
            TrainingPattern pattern;
            pattern.activations = retina->getActivationPattern();
            pattern.label = label;
            trainingPatterns.push_back(pattern);

            // Now clear spikes for next image
            retina->clearNeuronStates();

            trainCount[label]++;

            int totalTrained = 0;
            for (int d = 0; d < 10; ++d) totalTrained += trainCount[d];
            if (totalTrained % 100 == 0) {
                std::cout << "\r  Trained " << totalTrained << " patterns" << std::flush;
            }
        }

        std::cout << "\n✓ Training complete! Built " << trainingPatterns.size() << " patterns\n";

        // Test phase
        std::cout << "\n=== Testing Phase ===" << std::endl;
        int correct = 0;
        std::vector<std::vector<int>> confusionMatrix(10, std::vector<int>(10, 0));

        const auto& testImages = testLoader.getImages();
        for (int i = 0; i < config.testImages && i < static_cast<int>(testImages.size()); ++i) {
            auto activations = getActivations(*retina, testImages[i]);
            int predicted = classifyKNN(activations, trainingPatterns, config.kNeighbors);
            int actual = testImages[i].label;

            confusionMatrix[actual][predicted]++;
            if (predicted == actual) {
                correct++;
            }

            if ((i + 1) % 1000 == 0) {
                double accuracy = 100.0 * correct / (i + 1);
                std::cout << "Tested " << (i + 1) << " images, accuracy: "
                          << std::fixed << std::setprecision(2) << accuracy << "%" << std::endl;
            }
        }

        // Print results
        std::cout << "\n=== Results ===" << std::endl;
        double accuracy = 100.0 * correct / config.testImages;
        std::cout << "Overall Accuracy: " << std::fixed << std::setprecision(2)
                  << accuracy << "% (" << correct << "/" << config.testImages << ")" << std::endl;

        // Print per-digit accuracy
        std::cout << "\nPer-Digit Accuracy:" << std::endl;
        for (int digit = 0; digit < 10; ++digit) {
            int total = 0;
            for (int j = 0; j < 10; ++j) {
                total += confusionMatrix[digit][j];
            }
            if (total > 0) {
                double digitAccuracy = 100.0 * confusionMatrix[digit][digit] / total;
                std::cout << "  Digit " << digit << ": " << std::fixed << std::setprecision(1)
                          << digitAccuracy << "% (" << confusionMatrix[digit][digit] << "/" << total << ")" << std::endl;
            }
        }

        // Cleanup
        retina->shutdown();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

