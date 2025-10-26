/**
 * @file mnist_orientation_columns.cpp
 * @brief MNIST with Biologically-Correct Orientation Columns
 *
 * This experiment implements PROPER orientation columns as found in biological V1.
 * 
 * Key biological insight:
 * - V1 has HUNDREDS of orientation columns, each tuned to a specific orientation
 * - Each column contains neurons that respond to edges at THAT orientation
 * - Different digits have different "orientation signatures"
 *   - Digit "1": mostly vertical (90°)
 *   - Digit "7": horizontal (0°) + diagonal (45°)
 *   - Digit "0": all orientations (circular)
 *
 * Architecture:
 * - 8 orientation columns (0°, 22.5°, 45°, 67.5°, 90°, 112.5°, 135°, 157.5°)
 * - Each column has 64 neurons (8x8 spatial grid)
 * - Total V1: 512 neurons (8 orientations × 64 neurons)
 * - Output: 100 neurons (10 populations × 10 neurons)
 *
 * This should create MUCH more discriminative features than our current approach!
 */

#include "snnfw/MNISTLoader.h"
#include "snnfw/Neuron.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Brain.h"
#include "snnfw/Hemisphere.h"
#include "snnfw/Lobe.h"
#include "snnfw/Region.h"
#include "snnfw/Nucleus.h"
#include "snnfw/Column.h"
#include "snnfw/Layer.h"
#include "snnfw/Cluster.h"
#include "snnfw/SpikeProcessor.h"
#include "snnfw/NetworkPropagator.h"
#include "snnfw/learning/HybridStrategy.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>

using namespace snnfw;
using namespace snnfw::learning;

// Gabor filter for orientation-selective edge detection
std::vector<std::vector<double>> createGaborFilter(double orientation, int size = 7) {
    std::vector<std::vector<double>> filter(size, std::vector<double>(size, 0.0));
    double sigma = 2.0;
    double lambda = 4.0;  // wavelength
    double gamma = 0.5;   // aspect ratio
    
    int center = size / 2;
    double theta = orientation * M_PI / 180.0;  // Convert to radians
    
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - center;
            
            // Rotate coordinates
            double x_theta = dx * cos(theta) + dy * sin(theta);
            double y_theta = -dx * sin(theta) + dy * cos(theta);
            
            // Gabor function
            double gaussian = exp(-(x_theta*x_theta + gamma*gamma*y_theta*y_theta) / (2*sigma*sigma));
            double sinusoid = cos(2*M_PI*x_theta/lambda);
            
            filter[y][x] = gaussian * sinusoid;
        }
    }
    
    return filter;
}

// Apply Gabor filter to image
std::vector<double> applyGaborFilter(const std::vector<uint8_t>& image, 
                                     const std::vector<std::vector<double>>& filter,
                                     int imgWidth, int imgHeight) {
    int filterSize = filter.size();
    int halfSize = filterSize / 2;
    std::vector<double> response(imgWidth * imgHeight, 0.0);
    
    for (int y = halfSize; y < imgHeight - halfSize; ++y) {
        for (int x = halfSize; x < imgWidth - halfSize; ++x) {
            double sum = 0.0;
            
            for (int fy = 0; fy < filterSize; ++fy) {
                for (int fx = 0; fx < filterSize; ++fx) {
                    int imgY = y + fy - halfSize;
                    int imgX = x + fx - halfSize;
                    double pixelValue = image[imgY * imgWidth + imgX] / 255.0;
                    sum += pixelValue * filter[fy][fx];
                }
            }
            
            response[y * imgWidth + x] = std::abs(sum);  // Rectify (take absolute value)
        }
    }
    
    return response;
}

// Spatial pooling: divide response map into grid and pool each region
std::vector<double> spatialPool(const std::vector<double>& response, 
                                int imgWidth, int imgHeight,
                                int gridSize = 8) {
    std::vector<double> pooled(gridSize * gridSize, 0.0);
    int cellWidth = imgWidth / gridSize;
    int cellHeight = imgHeight / gridSize;
    
    for (int gy = 0; gy < gridSize; ++gy) {
        for (int gx = 0; gx < gridSize; ++gx) {
            double maxVal = 0.0;
            
            // Max pooling within this grid cell
            for (int cy = 0; cy < cellHeight; ++cy) {
                for (int cx = 0; cx < cellWidth; ++cx) {
                    int y = gy * cellHeight + cy;
                    int x = gx * cellWidth + cx;
                    if (y < imgHeight && x < imgWidth) {
                        maxVal = std::max(maxVal, response[y * imgWidth + x]);
                    }
                }
            }
            
            pooled[gy * gridSize + gx] = maxVal;
        }
    }
    
    return pooled;
}

// Fire neurons based on activation pattern
void fireNeurons(const std::vector<std::shared_ptr<Neuron>>& neurons,
                const std::vector<double>& activations,
                std::shared_ptr<NetworkPropagator> propagator,
                double currentTime) {
    for (size_t i = 0; i < std::min(neurons.size(), activations.size()); ++i) {
        if (activations[i] > 0.1) {  // Threshold
            // Number of spikes proportional to activation (1-5 spikes)
            int numSpikes = std::min(5, std::max(1, static_cast<int>(activations[i] * 10)));
            
            for (int spike = 0; spike < numSpikes; ++spike) {
                double firingTime = currentTime + (spike * 0.5);
                neurons[i]->insertSpike(firingTime);
            }
            
            neurons[i]->fireAndAcknowledge(currentTime);
            propagator->fireNeuron(neurons[i]->getId(), currentTime);
            neurons[i]->learnCurrentPattern();
        }
    }
}

// Copy spike pattern from source to target neurons
void copySpikePattern(const std::vector<std::shared_ptr<Neuron>>& source,
                     const std::vector<std::shared_ptr<Neuron>>& target) {
    for (const auto& targetNeuron : target) {
        targetNeuron->clearSpikes();
        for (const auto& sourceNeuron : source) {
            for (double spikeTime : sourceNeuron->getSpikes()) {
                targetNeuron->insertSpike(spikeTime);
            }
        }
    }
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
            return 1;
        }

        ConfigLoader configLoader(argv[1]);
        
        // Load MNIST data
        std::cout << "=== Loading MNIST Data ===" << std::endl;
        std::string dataPath = configLoader.get<std::string>("/data/path", "../data/mnist");
        MNISTLoader mnist(dataPath);
        
        if (!mnist.load()) {
            std::cerr << "Failed to load MNIST data" << std::endl;
            return 1;
        }
        
        std::cout << "✓ Loaded " << mnist.getTrainImages().size() << " training images" << std::endl;
        std::cout << "✓ Loaded " << mnist.getTestImages().size() << " test images" << std::endl;
        
        // Create neural object factory
        NeuralObjectFactory factory;
        
        // Create hierarchical structure
        std::cout << "\n=== Creating Hierarchical Structure ===" << std::endl;
        
        auto brain = factory.createBrain();
        brain->setName("Visual Processing Network");
        
        auto hemisphere = factory.createHemisphere();
        hemisphere->setName("Left Hemisphere");
        brain->addHemisphere(hemisphere->getId());
        
        auto occipitalLobe = factory.createLobe();
        occipitalLobe->setName("Occipital Lobe");
        hemisphere->addLobe(occipitalLobe->getId());
        
        auto v1Region = factory.createRegion();
        v1Region->setName("Primary Visual Cortex (V1)");
        occipitalLobe->addRegion(v1Region->getId());
        
        auto v1Nucleus = factory.createNucleus();
        v1Nucleus->setName("V1 Orientation Processing");
        v1Region->addNucleus(v1Nucleus->getId());
        
        std::cout << "✓ Created hierarchical structure" << std::endl;
        
        // Create 8 orientation columns
        std::cout << "\n=== Creating 8 Orientation Columns ===" << std::endl;
        
        const int NUM_ORIENTATIONS = 8;
        const int NEURONS_PER_COLUMN = 64;  // 8x8 spatial grid
        const double ORIENTATION_STEP = 180.0 / NUM_ORIENTATIONS;
        
        std::vector<std::shared_ptr<Column>> orientationColumns;
        std::vector<std::vector<std::shared_ptr<Neuron>>> columnNeurons;
        std::vector<std::vector<std::vector<double>>> gaborFilters;
        
        double neuronWindow = configLoader.get<double>("/neuron/window_size_ms", 200.0);
        double neuronThreshold = configLoader.get<double>("/neuron/similarity_threshold", 0.98);
        int neuronMaxPatterns = configLoader.get<int>("/neuron/max_patterns", 100);
        
        for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
            double orientation = ori * ORIENTATION_STEP;
            
            // Create column
            auto column = factory.createColumn();
            column->setName("Orientation " + std::to_string(static_cast<int>(orientation)) + "°");
            v1Nucleus->addColumn(column->getId());
            orientationColumns.push_back(column);
            
            // Create layer within column
            auto layer = factory.createLayer();
            layer->setName("Layer 4C");
            column->addLayer(layer->getId());
            
            // Create cluster within layer
            auto cluster = factory.createCluster();
            layer->addCluster(cluster->getId());
            
            // Create neurons for this orientation
            std::vector<std::shared_ptr<Neuron>> neurons;
            for (int i = 0; i < NEURONS_PER_COLUMN; ++i) {
                auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
                neurons.push_back(neuron);
                cluster->addNeuron(neuron->getId());
            }
            columnNeurons.push_back(neurons);
            
            // Create Gabor filter for this orientation
            auto filter = createGaborFilter(orientation);
            gaborFilters.push_back(filter);
            
            std::cout << "✓ Created column " << ori << ": " << orientation << "° (" 
                      << neurons.size() << " neurons)" << std::endl;
        }
        
        std::cout << "✓ Total V1 neurons: " << (NUM_ORIENTATIONS * NEURONS_PER_COLUMN) << std::endl;
        
        // Create output layer with population coding
        std::cout << "\n=== Creating Output Layer ===" << std::endl;

        const int NEURONS_PER_DIGIT = 10;
        std::vector<std::vector<std::shared_ptr<Neuron>>> outputPopulations;

        auto outputLayer = factory.createLayer();
        outputLayer->setName("Output Layer");
        v1Nucleus->addColumn(orientationColumns[0]->getId());  // Reuse first column for output
        orientationColumns[0]->addLayer(outputLayer->getId());

        for (int digit = 0; digit < 10; ++digit) {
            auto cluster = factory.createCluster();
            outputLayer->addCluster(cluster->getId());

            std::vector<std::shared_ptr<Neuron>> population;
            for (int i = 0; i < NEURONS_PER_DIGIT; ++i) {
                auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
                population.push_back(neuron);
                cluster->addNeuron(neuron->getId());
            }
            outputPopulations.push_back(population);
        }

        std::cout << "✓ Created output layer: " << (10 * NEURONS_PER_DIGIT) << " neurons" << std::endl;

        // Set similarity metric for all neurons
        std::string similarityMetricStr = configLoader.get<std::string>("/neuron/similarity_metric", "cosine");
        SimilarityMetric metric = SimilarityMetric::COSINE;
        if (similarityMetricStr == "histogram") {
            metric = SimilarityMetric::HISTOGRAM;
        } else if (similarityMetricStr == "euclidean") {
            metric = SimilarityMetric::EUCLIDEAN;
        } else if (similarityMetricStr == "correlation") {
            metric = SimilarityMetric::CORRELATION;
        } else if (similarityMetricStr == "waveform") {
            metric = SimilarityMetric::WAVEFORM;
        }

        for (const auto& neurons : columnNeurons) {
            for (const auto& neuron : neurons) {
                neuron->setSimilarityMetric(metric);
            }
        }
        for (const auto& population : outputPopulations) {
            for (const auto& neuron : population) {
                neuron->setSimilarityMetric(metric);
            }
        }

        std::cout << "✓ Set similarity metric to: " << similarityMetricStr << std::endl;

        // Set learning strategy
        HybridStrategy::Config strategyConfig;
        strategyConfig.pruningThreshold = 0.7;
        strategyConfig.consolidationThreshold = 0.9;
        auto strategy = std::make_shared<HybridStrategy>(strategyConfig);
        for (const auto& neurons : columnNeurons) {
            for (const auto& neuron : neurons) {
                neuron->setPatternUpdateStrategy(strategy);
            }
        }
        for (const auto& population : outputPopulations) {
            for (const auto& neuron : population) {
                neuron->setPatternUpdateStrategy(strategy);
            }
        }

        std::cout << "✓ Set learning strategy: HybridStrategy" << std::endl;

        // Create spike processor and network propagator
        auto spikeProcessor = std::make_shared<SpikeProcessor>();
        spikeProcessor->start();

        auto networkPropagator = std::make_shared<NetworkPropagator>(spikeProcessor);

        // Training
        std::cout << "\n=== Training ===" << std::endl;

        int examplesPerDigit = configLoader.get<int>("/training/examples_per_digit", 500);
        double currentTime = 0.0;

        auto trainStart = std::chrono::high_resolution_clock::now();

        const auto& trainImages = mnist.getTrainImages();
        const auto& trainLabels = mnist.getTrainLabels();

        // Count examples per digit
        std::vector<int> digitCounts(10, 0);

        for (size_t idx = 0; idx < trainImages.size(); ++idx) {
            int label = trainLabels[idx];

            if (digitCounts[label] >= examplesPerDigit) continue;
            digitCounts[label]++;

            const auto& image = trainImages[idx];

            // Process image through each orientation column
            for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
                // Apply Gabor filter
                auto response = applyGaborFilter(image, gaborFilters[ori], 28, 28);

                // Spatial pooling (8x8 grid)
                auto pooled = spatialPool(response, 28, 28, 8);

                // Fire neurons in this orientation column
                fireNeurons(columnNeurons[ori], pooled, networkPropagator, currentTime + ori * 0.1);
            }

            // Collect all V1 spikes
            std::vector<std::shared_ptr<Neuron>> allV1Neurons;
            for (const auto& neurons : columnNeurons) {
                allV1Neurons.insert(allV1Neurons.end(), neurons.begin(), neurons.end());
            }

            // Copy V1 pattern to correct output population
            copySpikePattern(allV1Neurons, outputPopulations[label]);

            // Fire output neurons as teaching signal
            for (const auto& neuron : outputPopulations[label]) {
                neuron->fireAndAcknowledge(currentTime + 2.0);
                networkPropagator->fireNeuron(neuron->getId(), currentTime + 2.0);
                neuron->learnCurrentPattern();
            }

            currentTime += 5.0;

            // Progress
            int totalProcessed = 0;
            for (int c : digitCounts) totalProcessed += c;
            if (totalProcessed % 500 == 0) {
                std::cout << "Trained " << totalProcessed << " images..." << std::endl;
            }

            // Homeostatic plasticity every 100 images
            if (totalProcessed % 100 == 0) {
                for (const auto& neurons : columnNeurons) {
                    for (const auto& neuron : neurons) {
                        neuron->applyHomeostaticPlasticity();
                    }
                }
                for (const auto& population : outputPopulations) {
                    for (const auto& neuron : population) {
                        neuron->applyHomeostaticPlasticity();
                    }
                }
            }

            // Check if done
            bool allDone = true;
            for (int c : digitCounts) {
                if (c < examplesPerDigit) {
                    allDone = false;
                    break;
                }
            }
            if (allDone) break;
        }

        auto trainEnd = std::chrono::high_resolution_clock::now();
        double trainTime = std::chrono::duration<double>(trainEnd - trainStart).count();

        std::cout << "✓ Training complete: " << trainTime << "s" << std::endl;

        // Testing
        std::cout << "\n=== Testing ===" << std::endl;

        int testImages = configLoader.get<int>("/training/test_images", 1000);
        const auto& testImagesData = mnist.getTestImages();
        const auto& testLabelsData = mnist.getTestLabels();

        auto testStart = std::chrono::high_resolution_clock::now();

        int correct = 0;
        std::vector<int> digitCorrect(10, 0);
        std::vector<int> digitTotal(10, 0);

        for (int i = 0; i < std::min(testImages, static_cast<int>(testImagesData.size())); ++i) {
            const auto& image = testImagesData[i];
            int label = testLabelsData[i];

            // Clear all spikes
            for (const auto& neurons : columnNeurons) {
                for (const auto& neuron : neurons) {
                    neuron->clearSpikes();
                }
            }
            for (const auto& population : outputPopulations) {
                for (const auto& neuron : population) {
                    neuron->clearSpikes();
                    neuron->resetInhibition();
                }
            }

            // Process image through orientation columns
            for (int ori = 0; ori < NUM_ORIENTATIONS; ++ori) {
                auto response = applyGaborFilter(image, gaborFilters[ori], 28, 28);
                auto pooled = spatialPool(response, 28, 28, 8);
                fireNeurons(columnNeurons[ori], pooled, networkPropagator, currentTime + ori * 0.1);
            }

            // Collect all V1 spikes
            std::vector<std::shared_ptr<Neuron>> allV1Neurons;
            for (const auto& neurons : columnNeurons) {
                allV1Neurons.insert(allV1Neurons.end(), neurons.begin(), neurons.end());
            }

            // Copy V1 pattern to all output populations
            for (int digit = 0; digit < 10; ++digit) {
                copySpikePattern(allV1Neurons, outputPopulations[digit]);
            }

            // Get population activations
            std::vector<double> populationActivations(10, 0.0);
            for (int digit = 0; digit < 10; ++digit) {
                double totalSimilarity = 0.0;
                for (const auto& neuron : outputPopulations[digit]) {
                    totalSimilarity += neuron->getBestSimilarity();
                }
                populationActivations[digit] = totalSimilarity / NEURONS_PER_DIGIT;
            }

            // Predict digit with highest activation
            int predicted = std::max_element(populationActivations.begin(),
                                            populationActivations.end()) - populationActivations.begin();

            if (predicted == label) {
                correct++;
                digitCorrect[label]++;
            }
            digitTotal[label]++;

            currentTime += 5.0;

            if ((i + 1) % 100 == 0) {
                std::cout << "Tested " << (i + 1) << " images..." << std::endl;
            }
        }

        auto testEnd = std::chrono::high_resolution_clock::now();
        double testTime = std::chrono::duration<double>(testEnd - testStart).count();

        // Print results
        std::cout << "\n=== Results ===" << std::endl;
        std::cout << "Overall Accuracy: " << (100.0 * correct / testImages) << "% ("
                  << correct << "/" << testImages << ")" << std::endl;
        std::cout << "\nPer-Digit Accuracy:" << std::endl;
        for (int digit = 0; digit < 10; ++digit) {
            if (digitTotal[digit] > 0) {
                std::cout << "  Digit " << digit << ": "
                          << (100.0 * digitCorrect[digit] / digitTotal[digit]) << "% ("
                          << digitCorrect[digit] << "/" << digitTotal[digit] << ")" << std::endl;
            }
        }

        std::cout << "\nTiming:" << std::endl;
        std::cout << "  Training: " << trainTime << "s" << std::endl;
        std::cout << "  Testing:  " << testTime << "s" << std::endl;

        spikeProcessor->stop();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

