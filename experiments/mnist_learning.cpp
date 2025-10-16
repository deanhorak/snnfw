/**
 * @file mnist_learning.cpp
 * @brief MNIST digit recognition using spiking neural network with STDP learning
 * 
 * This experiment demonstrates:
 * - Loading MNIST dataset
 * - Converting images to spike trains (rate coding)
 * - Building a spiking neural network
 * - Training with STDP (Spike-Timing-Dependent Plasticity)
 * - Testing and measuring accuracy
 * 
 * Network Architecture:
 * - Input layer: 784 neurons (28x28 pixels)
 * - Hidden layer: 400 neurons
 * - Output layer: 10 neurons (one per digit)
 * 
 * Encoding:
 * - Pixel intensity -> spike rate (Poisson process)
 * - Brighter pixels -> higher firing rate
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Neuron.h"
#include "snnfw/Axon.h"
#include "snnfw/Dendrite.h"
#include "snnfw/Synapse.h"
#include "snnfw/SpikeProcessor.h"
#include "snnfw/STDPLearning.h"
#include "snnfw/ActionPotential.h"
#include "snnfw/Logger.h"
#include "snnfw/ExperimentConfig.h"
#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>

using namespace snnfw;

// Network configuration
struct NetworkConfig {
    size_t inputSize = 784;      // 28x28 pixels
    size_t hiddenSize = 100;     // Hidden layer neurons (reduced from 400)
    size_t outputSize = 10;      // 10 digits

    double presentationTime = 20.0;  // Time to present each image (ms) - reduced
    double maxSpikeRate = 50.0;      // Maximum firing rate (Hz) - reduced

    // STDP parameters
    double stdpAPlus = 0.005;
    double stdpAMinus = 0.00525;
    double stdpTauPlus = 20.0;
    double stdpTauMinus = 20.0;

    // Training parameters
    size_t numTrainImages = 100;     // Number of training images (reduced for testing)
    size_t numTestImages = 50;       // Number of test images (reduced for testing)
    size_t numEpochs = 1;            // Training epochs
};

class MNISTExperiment {
public:
    MNISTExperiment(const NetworkConfig& config)
        : config(config),
          factory(),
          processor(100000, 20),  // 100 second buffer, 20 threads
          stdp(config.stdpAPlus, config.stdpAMinus,
               config.stdpTauPlus, config.stdpTauMinus),
          experimentConfig("mnist_learning"),
          rng(std::random_device{}()) {

        // Set up experiment directory
        experimentConfig.createDirectories();
    }
    
    bool loadData(const std::string& dataPath) {
        SNNFW_INFO("Loading MNIST dataset from: {}", dataPath);
        
        // Load training data
        if (!trainLoader.load(dataPath + "/train-images-idx3-ubyte",
                             dataPath + "/train-labels-idx1-ubyte",
                             config.numTrainImages)) {
            SNNFW_ERROR("Failed to load training data");
            return false;
        }
        
        // Load test data
        if (!testLoader.load(dataPath + "/t10k-images-idx3-ubyte",
                            dataPath + "/t10k-labels-idx1-ubyte",
                            config.numTestImages)) {
            SNNFW_ERROR("Failed to load test data");
            return false;
        }
        
        SNNFW_INFO("Loaded {} training images, {} test images",
                  trainLoader.size(), testLoader.size());
        
        // Print sample image
        if (trainLoader.size() > 0) {
            SNNFW_INFO("Sample training image:");
            MNISTLoader::printImage(trainLoader.getImage(0));
        }
        
        return true;
    }
    
    void buildNetwork() {
        SNNFW_INFO("Building network: {} -> {} -> {}", 
                  config.inputSize, config.hiddenSize, config.outputSize);
        
        auto startTime = std::chrono::steady_clock::now();
        
        // Create input layer neurons
        inputNeurons.reserve(config.inputSize);
        for (size_t i = 0; i < config.inputSize; ++i) {
            auto neuron = factory.createNeuron(50.0, 0.8, 10);
            auto axon = factory.createAxon(neuron->getId());
            neuron->setAxonId(axon->getId());
            
            inputNeurons.push_back(neuron);
            inputAxons.push_back(axon);
        }
        
        // Create hidden layer neurons
        hiddenNeurons.reserve(config.hiddenSize);
        for (size_t i = 0; i < config.hiddenSize; ++i) {
            auto neuron = factory.createNeuron(50.0, 0.8, 10);
            auto axon = factory.createAxon(neuron->getId());
            auto dendrite = factory.createDendrite(neuron->getId());
            
            neuron->setAxonId(axon->getId());
            neuron->addDendrite(dendrite->getId());
            
            hiddenNeurons.push_back(neuron);
            hiddenAxons.push_back(axon);
            hiddenDendrites.push_back(dendrite);
            
            processor.registerDendrite(dendrite);
        }
        
        // Create output layer neurons
        outputNeurons.reserve(config.outputSize);
        for (size_t i = 0; i < config.outputSize; ++i) {
            auto neuron = factory.createNeuron(50.0, 0.8, 10);
            auto dendrite = factory.createDendrite(neuron->getId());
            
            neuron->addDendrite(dendrite->getId());
            
            outputNeurons.push_back(neuron);
            outputDendrites.push_back(dendrite);
            
            processor.registerDendrite(dendrite);
        }
        
        // Create synapses: input -> hidden
        std::uniform_real_distribution<double> weightDist(0.3, 0.7);
        
        for (size_t h = 0; h < config.hiddenSize; ++h) {
            for (size_t i = 0; i < config.inputSize; ++i) {
                double weight = weightDist(rng);
                auto synapse = factory.createSynapse(
                    inputAxons[i]->getId(),
                    hiddenDendrites[h]->getId(),
                    weight,
                    1.0  // 1ms delay
                );
                
                hiddenDendrites[h]->addSynapse(synapse->getId());
                synapseMap[synapse->getId()] = synapse;
            }
        }
        
        // Create synapses: hidden -> output
        for (size_t o = 0; o < config.outputSize; ++o) {
            for (size_t h = 0; h < config.hiddenSize; ++h) {
                double weight = weightDist(rng);
                auto synapse = factory.createSynapse(
                    hiddenAxons[h]->getId(),
                    outputDendrites[o]->getId(),
                    weight,
                    1.0  // 1ms delay
                );
                
                outputDendrites[o]->addSynapse(synapse->getId());
                synapseMap[synapse->getId()] = synapse;
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        
        SNNFW_INFO("Network built in {}ms", duration.count());
        SNNFW_INFO("Total neurons: {}", 
                  inputNeurons.size() + hiddenNeurons.size() + outputNeurons.size());
        SNNFW_INFO("Total synapses: {}", synapseMap.size());
    }
    
    void train() {
        SNNFW_INFO("Starting training...");
        
        processor.setRealTimeSync(false);  // Fast mode for training
        processor.start();
        
        for (size_t epoch = 0; epoch < config.numEpochs; ++epoch) {
            SNNFW_INFO("Epoch {}/{}", epoch + 1, config.numEpochs);
            
            size_t correct = 0;
            
            for (size_t imgIdx = 0; imgIdx < trainLoader.size(); ++imgIdx) {
                const auto& image = trainLoader.getImage(imgIdx);
                
                // Present image and get prediction
                int predicted = presentImage(image, true);  // true = training mode
                
                if (predicted == image.label) {
                    correct++;
                }
                
                if ((imgIdx + 1) % 10 == 0) {
                    double accuracy = 100.0 * correct / (imgIdx + 1);
                    SNNFW_INFO("  Progress: {}/{} images, Accuracy: {:.2f}%",
                              imgIdx + 1, trainLoader.size(), accuracy);
                }
            }
            
            double finalAccuracy = 100.0 * correct / trainLoader.size();
            SNNFW_INFO("Epoch {} complete. Training accuracy: {:.2f}%",
                      epoch + 1, finalAccuracy);
        }
        
        processor.stop();
        SNNFW_INFO("Training complete");
    }
    
    void test() {
        SNNFW_INFO("Starting testing...");
        
        processor.setRealTimeSync(false);
        processor.start();
        
        size_t correct = 0;
        std::vector<size_t> confusionMatrix(100, 0);  // 10x10 matrix
        
        for (size_t imgIdx = 0; imgIdx < testLoader.size(); ++imgIdx) {
            const auto& image = testLoader.getImage(imgIdx);
            
            int predicted = presentImage(image, false);  // false = testing mode
            
            confusionMatrix[image.label * 10 + predicted]++;
            
            if (predicted == image.label) {
                correct++;
            }
        }
        
        processor.stop();
        
        double accuracy = 100.0 * correct / testLoader.size();
        SNNFW_INFO("Testing complete. Accuracy: {:.2f}% ({}/{})",
                  accuracy, correct, testLoader.size());
        
        // Print confusion matrix
        std::cout << "\nConfusion Matrix:" << std::endl;
        std::cout << "     ";
        for (int i = 0; i < 10; ++i) std::cout << std::setw(4) << i;
        std::cout << std::endl;
        std::cout << "    " << std::string(44, '-') << std::endl;
        
        for (int actual = 0; actual < 10; ++actual) {
            std::cout << std::setw(3) << actual << " |";
            for (int pred = 0; pred < 10; ++pred) {
                std::cout << std::setw(4) << confusionMatrix[actual * 10 + pred];
            }
            std::cout << std::endl;
        }
    }
    
private:
    NetworkConfig config;
    ExperimentConfig experimentConfig;
    NeuralObjectFactory factory;
    SpikeProcessor processor;
    STDPLearning stdp;
    std::mt19937 rng;
    
    MNISTLoader trainLoader;
    MNISTLoader testLoader;
    
    std::vector<std::shared_ptr<Neuron>> inputNeurons;
    std::vector<std::shared_ptr<Neuron>> hiddenNeurons;
    std::vector<std::shared_ptr<Neuron>> outputNeurons;
    
    std::vector<std::shared_ptr<Axon>> inputAxons;
    std::vector<std::shared_ptr<Axon>> hiddenAxons;
    
    std::vector<std::shared_ptr<Dendrite>> hiddenDendrites;
    std::vector<std::shared_ptr<Dendrite>> outputDendrites;
    
    std::map<uint64_t, std::shared_ptr<Synapse>> synapseMap;
    
    /**
     * @brief Present an image to the network and get prediction
     * @param image MNIST image to present
     * @param training Whether this is training mode (apply STDP)
     * @return Predicted digit (0-9)
     */
    int presentImage(const MNISTLoader::Image& image, bool training);
    
    /**
     * @brief Convert pixel intensity to spike train using Poisson process
     */
    std::vector<double> generateSpikeTrainPoisson(double intensity,
                                                   double duration,
                                                   double maxRate);
};

int MNISTExperiment::presentImage(const MNISTLoader::Image& image, bool training) {
    double currentTime = processor.getCurrentTime();
    double startTime = currentTime;
    double endTime = startTime + config.presentationTime;

    // Track spike times for each hidden neuron (for pattern learning)
    std::vector<std::vector<double>> hiddenNeuronSpikes(config.hiddenSize);
    std::vector<std::vector<double>> outputNeuronSpikes(config.outputSize);

    // Generate spike trains for input neurons based on pixel intensities
    for (size_t i = 0; i < config.inputSize; ++i) {
        int row = i / 28;
        int col = i % 28;
        double intensity = image.getNormalizedPixel(row, col);

        // Generate Poisson spike train
        auto spikeTimes = generateSpikeTrainPoisson(intensity,
                                                     config.presentationTime,
                                                     config.maxSpikeRate);

        // For each input spike, propagate to hidden layer neurons
        // Use a simplified approach: only propagate first few spikes to save time
        size_t maxSpikesToPropagate = std::min(spikeTimes.size(), size_t(5));
        for (size_t s = 0; s < maxSpikesToPropagate; ++s) {
            double spikeTime = spikeTimes[s];

            // Propagate to all connected hidden neurons
            for (size_t h = 0; h < config.hiddenSize; ++h) {
                // Find synapse weight (cache this in a better implementation)
                double weight = 0.0;
                for (auto& pair : synapseMap) {
                    if (pair.second->getAxonId() == inputAxons[i]->getId() &&
                        pair.second->getDendriteId() == hiddenDendrites[h]->getId()) {
                        weight = pair.second->getWeight();
                        break;
                    }
                }

                // Probabilistic spike propagation based on weight
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                if (dist(rng) < weight) {
                    // Add spike to hidden neuron's pattern (with synaptic delay)
                    hiddenNeuronSpikes[h].push_back(spikeTime + 1.0);  // 1ms delay
                }
            }
        }
    }

    // Propagate hidden layer spikes to output layer
    for (size_t h = 0; h < config.hiddenSize; ++h) {
        for (double spikeTime : hiddenNeuronSpikes[h]) {
            // Propagate to all connected output neurons
            for (size_t o = 0; o < config.outputSize; ++o) {
                // Find synapse weight
                double weight = 0.0;
                for (auto& pair : synapseMap) {
                    if (pair.second->getAxonId() == hiddenAxons[h]->getId() &&
                        pair.second->getDendriteId() == outputDendrites[o]->getId()) {
                        weight = pair.second->getWeight();
                        break;
                    }
                }

                // Probabilistic spike propagation based on weight
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                if (dist(rng) < weight) {
                    outputNeuronSpikes[o].push_back(spikeTime + 1.0);  // 1ms delay
                }
            }
        }
    }

    // Determine prediction based on which output neuron has most spikes
    int prediction = 0;
    size_t maxSpikes = 0;
    for (size_t o = 0; o < config.outputSize; ++o) {
        if (outputNeuronSpikes[o].size() > maxSpikes) {
            maxSpikes = outputNeuronSpikes[o].size();
            prediction = o;
        }
    }

    // LEARNING: If training, teach the correct output neuron the spike pattern
    if (training) {
        int correctLabel = image.label;

        // Insert the spike pattern into the correct output neuron
        // This is our pattern-based learning mechanism
        for (double spikeTime : outputNeuronSpikes[correctLabel]) {
            outputNeurons[correctLabel]->insertSpike(spikeTime);
        }

        // Learn the current pattern if there were enough spikes
        if (outputNeuronSpikes[correctLabel].size() >= 3) {
            outputNeurons[correctLabel]->learnCurrentPattern();

            SNNFW_TRACE("Learned pattern for digit {} with {} spikes",
                       correctLabel, outputNeuronSpikes[correctLabel].size());
        }

        // Also learn patterns in hidden layer neurons
        // This helps them become feature detectors
        for (size_t h = 0; h < config.hiddenSize; ++h) {
            if (hiddenNeuronSpikes[h].size() >= 3) {
                for (double spikeTime : hiddenNeuronSpikes[h]) {
                    hiddenNeurons[h]->insertSpike(spikeTime);
                }
                hiddenNeurons[h]->learnCurrentPattern();
            }
        }

        // Strengthen synaptic connections that contributed to correct output
        // This is a simplified form of reward-based learning
        if (prediction == correctLabel) {
            // Strengthen connections to the correct output neuron
            for (auto& pair : synapseMap) {
                if (pair.second->getDendriteId() == outputDendrites[correctLabel]->getId()) {
                    double currentWeight = pair.second->getWeight();
                    double newWeight = std::min(1.0, currentWeight + 0.001);  // Small increment
                    pair.second->setWeight(newWeight);
                }
            }
        } else {
            // Weaken connections to the incorrect prediction
            for (auto& pair : synapseMap) {
                if (pair.second->getDendriteId() == outputDendrites[prediction]->getId()) {
                    double currentWeight = pair.second->getWeight();
                    double newWeight = std::max(0.1, currentWeight - 0.001);  // Small decrement
                    pair.second->setWeight(newWeight);
                }
            }
        }
    }

    return prediction;
}

std::vector<double> MNISTExperiment::generateSpikeTrainPoisson(double intensity,
                                                                double duration,
                                                                double maxRate) {
    std::vector<double> spikeTimes;

    if (intensity < 0.01) {
        return spikeTimes;  // No spikes for very low intensity
    }

    // Calculate firing rate based on intensity
    double rate = intensity * maxRate;  // Hz
    double lambda = rate / 1000.0;      // Convert to spikes per ms

    std::exponential_distribution<double> dist(lambda);

    double t = 0.0;
    while (t < duration) {
        t += dist(rng);
        if (t < duration) {
            spikeTimes.push_back(t);
        }
    }

    return spikeTimes;
}

int main(int argc, char** argv) {
    // Initialize logger
    Logger::getInstance().setLevel(spdlog::level::info);
    
    std::cout << "=== MNIST Learning Experiment ===" << std::endl;
    std::cout << std::endl;
    
    // Get data path from command line or use default
    std::string dataPath = "/home/dean/repos/ctm/data/MNIST/raw";
    if (argc > 1) {
        dataPath = argv[1];
    }
    
    // Create experiment
    NetworkConfig config;
    MNISTExperiment experiment(config);
    
    // Load data
    if (!experiment.loadData(dataPath)) {
        return 1;
    }
    
    // Build network
    experiment.buildNetwork();
    
    // Train
    experiment.train();
    
    // Test
    experiment.test();
    
    std::cout << "\n=== Experiment Complete ===" << std::endl;
    
    return 0;
}

