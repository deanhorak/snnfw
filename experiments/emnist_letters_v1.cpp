/**
 * @file emnist_letters_v1.cpp
 * @brief EMNIST Letters classification using multi-column hierarchical V1 architecture
 *
 * Architecture:
 * - 16 cortical columns (orientation-selective + center-surround + specialized)
 * - Each column has 6 layers following canonical cortical microcircuit:
 *   - Layer 1: Apical dendrites, modulatory inputs
 *   - Layer 2/3: Superficial pyramidal neurons, lateral connections
 *   - Layer 4: Granular input layer (receives thalamic/sensory input)
 *   - Layer 5: Deep pyramidal neurons, output layer
 *   - Layer 6: Corticothalamic feedback neurons
 *
 * Connectivity pattern (canonical microcircuit):
 *   Input → Layer 4 → Layer 2/3 → Layer 5 → Layer 6 → (feedback to Layer 4)
 *   Layer 1 receives modulatory/contextual input from higher areas
 *
 * Dataset: EMNIST Letters (26 classes: A-Z)
 * - Training: 124,800 images (26 letters × ~4,800 each)
 * - Testing: 20,800 images (26 letters × 800 each)
 */

#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <random>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <future>
#include <thread>

#include "snnfw/NeuralObjectFactory.h"
#include "snnfw/Brain.h"
#include "snnfw/Hemisphere.h"
#include "snnfw/Lobe.h"
#include "snnfw/Region.h"
#include "snnfw/Nucleus.h"
#include "snnfw/Column.h"
#include "snnfw/Layer.h"
#include "snnfw/Cluster.h"
#include "snnfw/Neuron.h"
#include "snnfw/Axon.h"
#include "snnfw/Synapse.h"
#include "snnfw/Dendrite.h"
#include "snnfw/NetworkPropagator.h"
#include "snnfw/SpikeProcessor.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/EMNISTLoader.h"

using namespace snnfw;

// Configuration structure
struct MultiColumnConfig {
    // Neuron parameters
    double neuronWindow;
    double neuronThreshold;
    int neuronMaxPatterns;

    // Spike processor parameters
    int numThreads;

    // Training parameters
    int trainingExamplesPerLetter;
    int testImages;

    // Data paths
    std::string trainImagesPath;
    std::string trainLabelsPath;
    std::string testImagesPath;
    std::string testLabelsPath;

    // Architecture parameters - Column counts
    int numOrientations;
    int numFrequencies;
    int numCenterSurroundScales;
    int numCenterSurroundTypes;
    int numBlobScales;
    int numBlobTypes;
    int numSpecializedDetectors;

    // Architecture parameters - Layer sizes
    int layer1Neurons;
    int layer23Neurons;
    int layer4Size;  // Grid size (e.g., 8 means 8x8 = 64 neurons)
    int layer5Neurons;
    int layer6Neurons;

    // Architecture parameters - Connectivity
    double lateralConnectivity;
    int neighborRange;
    double recurrentConnectivity;
    double recurrentWeight;
    double recurrentDelay;

    // Architecture parameters - Gabor filters
    double freqLow;
    double freqHigh;
    double gaborThreshold;

    // Architecture parameters - Center-surround
    std::vector<std::pair<double, double>> centerSurroundParams;  // (centerSigma, surroundSigma) pairs

    // Architecture parameters - Blob detectors
    std::vector<double> blobSigmas;

    // Output layer parameters
    int neuronsPerClass;

    // Saccade/spatial attention parameters
    struct FixationRegion {
        std::string name;
        int rowStart;
        int rowEnd;
        int colStart;
        int colEnd;
    };

    bool saccadesEnabled;
    int numFixations;
    double fixationDurationMs;
    std::vector<FixationRegion> fixationRegions;

    // Position encoding parameters
    bool positionFeedbackEnabled;
    int positionNeuronsPerFixation;

    static MultiColumnConfig fromConfigLoader(const ConfigLoader& loader) {
        MultiColumnConfig config;

        // Neuron parameters
        config.neuronWindow = loader.get<double>("/neuron/window_size_ms", 200.0);
        config.neuronThreshold = loader.get<double>("/neuron/similarity_threshold", 0.90);
        config.neuronMaxPatterns = loader.get<int>("/neuron/max_patterns", 100);

        // Spike processor parameters
        config.numThreads = loader.get<int>("/spike_processor/num_threads", 20);

        // Training parameters
        config.trainingExamplesPerLetter = loader.get<int>("/training/examples_per_letter", 800);
        config.testImages = loader.get<int>("/training/test_images", 20800);

        // Data paths
        config.trainImagesPath = loader.getRequired<std::string>("/data/train_images");
        config.trainLabelsPath = loader.getRequired<std::string>("/data/train_labels");
        config.testImagesPath = loader.getRequired<std::string>("/data/test_images");
        config.testLabelsPath = loader.getRequired<std::string>("/data/test_labels");

        // Architecture parameters - Column counts
        config.numOrientations = loader.get<int>("/architecture/columns/num_orientations", 4);
        config.numFrequencies = loader.get<int>("/architecture/columns/num_frequencies", 2);
        config.numCenterSurroundScales = loader.get<int>("/architecture/columns/num_center_surround_scales", 2);
        config.numCenterSurroundTypes = loader.get<int>("/architecture/columns/num_center_surround_types", 2);
        config.numBlobScales = loader.get<int>("/architecture/columns/num_blob_scales", 0);
        config.numBlobTypes = loader.get<int>("/architecture/columns/num_blob_types", 0);
        config.numSpecializedDetectors = loader.get<int>("/architecture/columns/num_specialized_detectors", 4);

        // Architecture parameters - Layer sizes
        config.layer1Neurons = loader.get<int>("/architecture/layers/layer1_neurons", 32);
        config.layer23Neurons = loader.get<int>("/architecture/layers/layer23_neurons", 256);
        config.layer4Size = loader.get<int>("/architecture/layers/layer4_size", 8);
        config.layer5Neurons = loader.get<int>("/architecture/layers/layer5_neurons", 64);
        config.layer6Neurons = loader.get<int>("/architecture/layers/layer6_neurons", 32);

        // Architecture parameters - Connectivity
        config.lateralConnectivity = loader.get<double>("/architecture/connectivity/lateral_connectivity", 0.20);
        config.neighborRange = loader.get<int>("/architecture/connectivity/neighbor_range", 2);
        config.recurrentConnectivity = loader.get<double>("/architecture/connectivity/recurrent_connectivity", 0.15);
        config.recurrentWeight = loader.get<double>("/architecture/connectivity/recurrent_weight", 0.4);
        config.recurrentDelay = loader.get<double>("/architecture/connectivity/recurrent_delay", 2.0);

        // Architecture parameters - Gabor filters
        config.freqLow = loader.get<double>("/architecture/gabor/freq_low", 8.0);
        config.freqHigh = loader.get<double>("/architecture/gabor/freq_high", 3.0);
        config.gaborThreshold = loader.get<double>("/architecture/gabor/threshold", 0.1);

        // Architecture parameters - Center-surround (default: small and medium scales)
        // Format: [[centerSigma1, surroundSigma1], [centerSigma2, surroundSigma2], ...]
        config.centerSurroundParams = {
            {1.2, 3.5},  // Small scale
            {2.0, 5.0}   // Medium scale
        };
        // TODO: Add JSON array parsing for center_surround_params when needed

        // Architecture parameters - Blob detectors (default: empty)
        config.blobSigmas = {};
        // TODO: Add JSON array parsing for blob_sigmas when needed

        // Output layer parameters
        config.neuronsPerClass = loader.get<int>("/architecture/output/neurons_per_class", 20);

        // Saccade parameters
        config.saccadesEnabled = loader.get<bool>("/saccades/enabled", false);
        config.numFixations = loader.get<int>("/saccades/num_fixations", 4);
        config.fixationDurationMs = loader.get<double>("/saccades/fixation_duration_ms", 100.0);

        // Load fixation regions from JSON array
        config.fixationRegions.clear();
        if (config.saccadesEnabled) {
            // Default regions if not specified in config
            config.fixationRegions = {
                {"top", 0, 13, 0, 27},
                {"bottom", 14, 27, 0, 27},
                {"center", 7, 20, 7, 20},
                {"full", 0, 27, 0, 27}
            };
            // TODO: Parse from JSON array when needed
        }

        // Position encoding parameters
        config.positionFeedbackEnabled = loader.get<bool>("/position_encoding/enabled", false);
        config.positionNeuronsPerFixation = loader.get<int>("/position_encoding/neurons_per_fixation", 16);

        return config;
    }
};

/**
 * @brief Create a Gabor filter kernel for orientation and spatial frequency selectivity
 * @param orientation Preferred orientation in degrees (0-180)
 * @param lambda Wavelength of sinusoid (spatial frequency): smaller = higher frequency
 * @param size Kernel size (default 9x9)
 */
std::vector<std::vector<double>> createGaborKernel(double orientation, double lambda, int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    double sigma = 2.5;      // Gaussian envelope width (increased for larger kernel)
    double gamma = 0.5;      // Spatial aspect ratio

    int center = size / 2;
    double theta = orientation * M_PI / 180.0;  // Convert to radians

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - center;

            // Rotate coordinates to orientation
            double x_theta = dx * cos(theta) + dy * sin(theta);
            double y_theta = -dx * sin(theta) + dy * cos(theta);

            // Gabor function: Gaussian envelope × sinusoidal grating
            double gaussian = exp(-(x_theta*x_theta + gamma*gamma*y_theta*y_theta) / (2*sigma*sigma));
            double sinusoid = cos(2*M_PI*x_theta/lambda);

            kernel[y][x] = gaussian * sinusoid;
        }
    }

    return kernel;
}

/**
 * @brief Create a center-surround (Difference of Gaussians) filter kernel
 * @param centerSigma Sigma for center Gaussian (smaller = tighter center)
 * @param surroundSigma Sigma for surround Gaussian (larger = wider surround)
 * @param onCenter If true, creates ON-center (bright center), else OFF-center (dark center)
 * @param size Kernel size (default 9x9)
 * @return DoG kernel for blob/hole detection
 */
std::vector<std::vector<double>> createCenterSurroundKernel(double centerSigma,
                                                             double surroundSigma,
                                                             bool onCenter = true,
                                                             int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;

    // Normalize the DoG so it sums to zero (balanced center-surround)
    double centerSum = 0.0;
    double surroundSum = 0.0;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - center;
            double distSq = dx*dx + dy*dy;

            // Center Gaussian (positive)
            double centerGaussian = exp(-distSq / (2*centerSigma*centerSigma));
            centerSum += centerGaussian;

            // Surround Gaussian (negative)
            double surroundGaussian = exp(-distSq / (2*surroundSigma*surroundSigma));
            surroundSum += surroundGaussian;
        }
    }

    // Create normalized DoG
    double polarity = onCenter ? 1.0 : -1.0;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - center;
            double distSq = dx*dx + dy*dy;

            double centerGaussian = exp(-distSq / (2*centerSigma*centerSigma)) / centerSum;
            double surroundGaussian = exp(-distSq / (2*surroundSigma*surroundSigma)) / surroundSum;

            // DoG = center - surround (or inverted for OFF-center)
            kernel[y][x] = polarity * (centerGaussian - surroundGaussian);
        }
    }

    return kernel;
}

/**
 * @brief Create a simple Gaussian blob detector
 * @param sigma Size of the blob to detect
 * @param size Kernel size (default 9x9)
 * @return Gaussian kernel for blob detection
 */
std::vector<std::vector<double>> createBlobKernel(double sigma, int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;
    double sum = 0.0;

    // Create Gaussian
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - center;
            double distSq = dx*dx + dy*dy;
            kernel[y][x] = exp(-distSq / (2*sigma*sigma));
            sum += kernel[y][x];
        }
    }

    // Normalize
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            kernel[y][x] /= sum;
        }
    }

    return kernel;
}

/**
 * @brief Create a top-region loop detector (for distinguishing 4 vs 9, 7 vs 9)
 * Detects closed loops in the upper-right portion of the image
 * Refined to avoid false positives on digit 3's curved top
 * @param size Kernel size (default 9x9)
 * @return Kernel that responds strongly to closed loops in top-right region
 */
std::vector<std::vector<double>> createTopLoopKernel(int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;

    // Create a tighter ring pattern focused on upper-right quadrant
    // This is where digit 9's loop is, but digit 3's curve is more open
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            // Shift center to upper-right (where digit 9's loop is)
            double dx = x - (center + 1);
            double dy = y - (center - 1);
            double dist = sqrt(dx*dx + dy*dy);

            // Tighter ring pattern with stronger contrast
            // Focus on top-right quadrant (x >= center, y <= center)
            if (x >= center - 1 && y <= center + 1) {
                if (dist >= 1.2 && dist <= 2.5) {
                    // Ring edge - strong positive
                    kernel[y][x] = 1.5;
                } else if (dist < 1.2) {
                    // Inside hole - strong negative (key for closed loop)
                    kernel[y][x] = -1.0;
                } else if (dist > 2.5 && dist < 3.5) {
                    // Outside ring - moderate negative
                    kernel[y][x] = -0.4;
                }
            }
        }
    }

    return kernel;
}

/**
 * @brief Create a gap detector for open regions (for detecting digit 4's open top)
 * Refined to strongly respond to horizontal gaps (open top) vs closed loops
 * @param size Kernel size (default 9x9)
 * @return Kernel that responds to gaps/openings in top region
 */
std::vector<std::vector<double>> createGapKernel(int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;

    // Detect horizontal gap in top-center region (digit 4's open top)
    // Strong response when there's a gap between left and right strokes
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            // Focus on top region (upper 60% of kernel)
            if (y <= center + 1) {
                // Create a horizontal gap detector
                // Left stroke region
                if (x <= center - 2) {
                    kernel[y][x] = 1.2;  // Strong positive for left edge
                }
                // Right stroke region
                else if (x >= center + 2) {
                    kernel[y][x] = 1.2;  // Strong positive for right edge
                }
                // Gap in middle (key discriminator)
                else {
                    // Very strong negative in the gap region
                    // This should fire strongly for digit 4 (open gap)
                    // but weakly for digit 9 (closed loop fills this region)
                    kernel[y][x] = -2.0;
                }
            }
        }
    }

    return kernel;
}

/**
 * @brief Create a bottom-curve detector (for distinguishing 2 vs 0)
 * Digit 2 has a curved stroke at bottom-left, digit 0 has a closed loop
 * @param size Kernel size (default 9x9)
 * @return Kernel that responds to bottom-left curves
 */
std::vector<std::vector<double>> createBottomCurveKernel(int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;

    // Detect curved stroke in bottom-left region (digit 2's tail)
    // Positive for diagonal/horizontal stroke, negative for vertical closure
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            // Focus on bottom half
            if (y >= center) {
                // Bottom-left quadrant (where digit 2's tail curves)
                if (x <= center) {
                    double dx = x - (center - 2);
                    double dy = y - (center + 2);
                    double dist = sqrt(dx*dx + dy*dy);

                    // Curved stroke pattern
                    if (dist >= 1.0 && dist <= 2.5) {
                        kernel[y][x] = 1.5;  // Curve
                    }
                }
                // Bottom-right (should be empty for digit 2, filled for digit 0)
                else if (x >= center + 1) {
                    kernel[y][x] = -0.8;  // Negative for closed loop
                }
            }
        }
    }

    return kernel;
}

/**
 * @brief Create a horizontal bar detector (for distinguishing 3 vs 5)
 * Digit 5 has a horizontal bar at top, digit 3 has curves
 * @param size Kernel size (default 9x9)
 * @return Kernel that responds to horizontal bars in top region
 */
std::vector<std::vector<double>> createHorizontalBarKernel(int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;

    // Detect horizontal bar in top region (digit 5's top bar)
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            // Top region only (upper 40%)
            if (y <= center - 1) {
                // Horizontal bar pattern
                if (x >= center - 2 && x <= center + 2) {
                    kernel[y][x] = 1.5;  // Strong positive for horizontal stroke
                }
                // Suppress curved patterns
                else if (x > center + 2) {
                    kernel[y][x] = -0.5;  // Negative for curves extending right
                }
            }
        }
    }

    return kernel;
}

/**
 * @brief Create a middle constriction detector (for distinguishing 8 vs 0)
 * Digit 8 has a constriction in the middle (figure-8), digit 0 is uniform
 * @param size Kernel size (default 9x9)
 * @return Kernel that responds to middle constrictions
 */
std::vector<std::vector<double>> createMiddleConstrictionKernel(int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;

    // Detect constriction in middle (digit 8's waist)
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dy = abs(y - center);
            double dx = abs(x - center);

            // Middle horizontal band (where digit 8 constricts)
            if (dy <= 1) {
                // Center should be narrow (constricted)
                if (dx <= 1) {
                    kernel[y][x] = -1.5;  // Strong negative in center
                }
                // Edges should have strokes
                else if (dx >= 2 && dx <= 3) {
                    kernel[y][x] = 1.0;  // Positive for edges
                }
            }
            // Top and bottom should be wider (loops)
            else if (dy >= 2 && dy <= 3) {
                if (dx >= 1 && dx <= 2) {
                    kernel[y][x] = 0.8;  // Positive for loops
                }
            }
        }
    }

    return kernel;
}

/**
 * @brief Extract a spatial region from an image for fixation
 * @param imagePixels Full 28x28 image
 * @param region Fixation region specification
 * @return Extracted region pixels (padded to 28x28 with zeros outside region)
 */
std::vector<uint8_t> extractFixationRegion(const std::vector<uint8_t>& imagePixels,
                                           const MultiColumnConfig::FixationRegion& region,
                                           int imgWidth = 28,
                                           int imgHeight = 28) {
    // Create a blank image (all zeros)
    std::vector<uint8_t> regionPixels(imgWidth * imgHeight, 0);

    // Copy only the specified region
    for (int y = region.rowStart; y <= region.rowEnd && y < imgHeight; ++y) {
        for (int x = region.colStart; x <= region.colEnd && x < imgWidth; ++x) {
            regionPixels[y * imgWidth + x] = imagePixels[y * imgWidth + x];
        }
    }

    return regionPixels;
}

/**
 * @brief Apply Gabor filter to raw image pixels
 */
std::vector<double> applyGaborFilter(const std::vector<uint8_t>& imagePixels,
                                     const std::vector<std::vector<double>>& gaborKernel,
                                     int gridSize,
                                     int imgWidth = 28,
                                     int imgHeight = 28) {
    int kernelSize = gaborKernel.size();
    int halfKernel = kernelSize / 2;

    // Apply Gabor filter via convolution on full 28x28 image
    std::vector<double> fullResponse(imgWidth * imgHeight, 0.0);

    for (int y = halfKernel; y < imgHeight - halfKernel; ++y) {
        for (int x = halfKernel; x < imgWidth - halfKernel; ++x) {
            double sum = 0.0;

            for (int ky = 0; ky < kernelSize; ++ky) {
                for (int kx = 0; kx < kernelSize; ++kx) {
                    int imgY = y + ky - halfKernel;
                    int imgX = x + kx - halfKernel;
                    double pixelValue = imagePixels[imgY * imgWidth + imgX] / 255.0;
                    sum += pixelValue * gaborKernel[ky][kx];
                }
            }

            fullResponse[y * imgWidth + x] = std::abs(sum);  // Rectify
        }
    }

    // Max pooling to reduce to gridSize x gridSize
    int poolSize = imgWidth / gridSize;  // Calculate pool size based on grid size
    std::vector<double> pooledResponse(gridSize * gridSize, 0.0);

    for (int gy = 0; gy < gridSize; ++gy) {
        for (int gx = 0; gx < gridSize; ++gx) {
            double maxVal = 0.0;

            for (int py = 0; py < poolSize; ++py) {
                for (int px = 0; px < poolSize; ++px) {
                    int imgY = gy * poolSize + py;
                    int imgX = gx * poolSize + px;
                    if (imgY < imgHeight && imgX < imgWidth) {
                        maxVal = std::max(maxVal, fullResponse[imgY * imgWidth + imgX]);
                    }
                }
            }

            pooledResponse[gy * gridSize + gx] = maxVal;
        }
    }

    return pooledResponse;
}

/**
 * @brief Copy spike pattern from source neurons to target neurons
 */
void copyLayerSpikePattern(const std::vector<std::shared_ptr<Neuron>>& sourceNeurons,
                          const std::vector<std::shared_ptr<Neuron>>& targetNeurons) {
    for (auto& targetNeuron : targetNeurons) {
        targetNeuron->clearSpikes();

        for (const auto& sourceNeuron : sourceNeurons) {
            const auto& spikes = sourceNeuron->getSpikes();
            for (double spikeTime : spikes) {
                targetNeuron->insertSpike(spikeTime);
            }
        }
    }
}

// Structure to hold a single cortical column
struct CorticalColumn {
    std::shared_ptr<Column> column;

    // Layers
    std::shared_ptr<Layer> layer1;   // Apical dendrites, modulatory
    std::shared_ptr<Layer> layer23;  // Superficial pyramidal
    std::shared_ptr<Layer> layer4;   // Granular input
    std::shared_ptr<Layer> layer5;   // Deep pyramidal output
    std::shared_ptr<Layer> layer6;   // Corticothalamic feedback

    // Neurons in each layer
    std::vector<std::shared_ptr<Neuron>> layer1Neurons;   // 32 neurons
    std::vector<std::shared_ptr<Neuron>> layer23Neurons;  // 128 neurons
    std::vector<std::shared_ptr<Neuron>> layer4Neurons;   // 64 neurons (8x8 grid)
    std::vector<std::shared_ptr<Neuron>> layer5Neurons;   // 64 neurons
    std::vector<std::shared_ptr<Neuron>> layer6Neurons;   // 32 neurons

    double orientation;  // Preferred orientation for this column (0-180 degrees)
    double spatialFrequency;  // Spatial frequency (lambda): low=8.0, med=5.0, high=3.0
    std::string featureType;  // Feature type: "low_freq", "med_freq", "high_freq"
    std::vector<std::vector<double>> gaborKernel;  // Gabor filter for this orientation and frequency
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
    
    try {
        // Load configuration
        std::cout << "=== MNIST Multi-Column V1 Architecture ===" << std::endl;
        std::cout << "Loading configuration from: " << argv[1] << std::endl;
        ConfigLoader configLoader(argv[1]);
        MultiColumnConfig config = MultiColumnConfig::fromConfigLoader(configLoader);
        
        std::cout << "\nConfiguration:" << std::endl;
        std::cout << "  Neuron window: " << config.neuronWindow << " ms" << std::endl;
        std::cout << "  Similarity threshold: " << config.neuronThreshold << std::endl;
        std::cout << "  Max patterns per neuron: " << config.neuronMaxPatterns << std::endl;
        std::cout << "  Training examples per letter: " << config.trainingExamplesPerLetter << std::endl;
        std::cout << "  Test images: " << config.testImages << std::endl;

        // Extract neuron parameters for convenience
        double neuronWindow = config.neuronWindow;
        double neuronThreshold = config.neuronThreshold;
        int neuronMaxPatterns = config.neuronMaxPatterns;
        
        // Create hierarchical structure
        std::cout << "\n=== Building Hierarchical Structure ===" << std::endl;
        NeuralObjectFactory factory;
        
        auto brain = factory.createBrain();
        brain->setName("Multi-Column Visual Processing Network");
        std::cout << "✓ Created Brain: " << brain->getName() << std::endl;
        
        auto hemisphere = factory.createHemisphere();
        hemisphere->setName("Left Hemisphere");
        brain->addHemisphere(hemisphere->getId());
        std::cout << "✓ Created Hemisphere: " << hemisphere->getName() << std::endl;
        
        auto occipitalLobe = factory.createLobe();
        occipitalLobe->setName("Occipital Lobe");
        hemisphere->addLobe(occipitalLobe->getId());
        std::cout << "✓ Created Lobe: " << occipitalLobe->getName() << std::endl;
        
        auto v1Region = factory.createRegion();
        v1Region->setName("Primary Visual Cortex (V1)");
        occipitalLobe->addRegion(v1Region->getId());
        std::cout << "✓ Created Region: " << v1Region->getName() << std::endl;
        
        auto v1Nucleus = factory.createNucleus();
        v1Nucleus->setName("V1 Multi-Column Nucleus");
        v1Region->addNucleus(v1Nucleus->getId());
        std::cout << "✓ Created Nucleus: " << v1Nucleus->getName() << std::endl;
        
        // Architecture parameters from config
        const int NUM_ORIENTATIONS = config.numOrientations;
        const int NUM_FREQUENCIES = config.numFrequencies;
        const int NUM_CS_SCALES = config.numCenterSurroundScales;
        const int NUM_CS_TYPES = config.numCenterSurroundTypes;
        const int NUM_BLOB_SCALES = config.numBlobScales;
        const int NUM_BLOB_TYPES = config.numBlobTypes;
        const int NUM_SPECIALIZED = config.numSpecializedDetectors;

        const int NUM_ORIENTATION_COLUMNS = NUM_ORIENTATIONS * NUM_FREQUENCIES;
        const int NUM_CS_COLUMNS = NUM_CS_SCALES * NUM_CS_TYPES;
        const int NUM_BLOB_COLUMNS = NUM_BLOB_SCALES * NUM_BLOB_TYPES;
        const int NUM_COLUMNS = NUM_ORIENTATION_COLUMNS + NUM_CS_COLUMNS + NUM_BLOB_COLUMNS + NUM_SPECIALIZED;
        const double ORIENTATION_STEP = 180.0 / NUM_ORIENTATIONS;

        // Spatial frequency channels (lambda values)
        const double FREQ_LOW = config.freqLow;
        const double FREQ_HIGH = config.freqHigh;

        const std::vector<double> SPATIAL_FREQUENCIES = {FREQ_LOW, FREQ_HIGH};
        const std::vector<std::string> FREQ_NAMES = {"low_freq", "high_freq"};

        // Center-surround parameters from config
        const std::vector<std::pair<double, double>> CS_PARAMS = config.centerSurroundParams;
        const std::vector<std::string> CS_SCALE_NAMES = {"small", "medium", "large", "xlarge"};
        const std::vector<std::string> CS_TYPE_NAMES = {"ON_center", "OFF_center"};

        // Blob detector parameters from config
        const std::vector<double> BLOB_SIGMAS = config.blobSigmas;
        std::vector<std::string> BLOB_SCALE_NAMES;
        for (size_t i = 0; i < BLOB_SIGMAS.size(); ++i) {
            BLOB_SCALE_NAMES.push_back("scale_" + std::to_string(i));
        }

        // Neuron counts per layer from config
        const int LAYER1_NEURONS = config.layer1Neurons;
        const int LAYER23_NEURONS = config.layer23Neurons;
        const int LAYER4_SIZE = config.layer4Size;
        const int LAYER5_NEURONS = config.layer5Neurons;
        const int LAYER6_NEURONS = config.layer6Neurons;

        std::vector<CorticalColumn> corticalColumns;

        std::cout << "\n=== Creating " << NUM_COLUMNS << " Cortical Columns ===" << std::endl;
        std::cout << "  " << NUM_ORIENTATION_COLUMNS << " orientation columns (" << NUM_ORIENTATIONS
                  << " orientations × " << NUM_FREQUENCIES << " frequencies)" << std::endl;
        std::cout << "  " << NUM_CS_COLUMNS << " center-surround columns (" << NUM_CS_SCALES
                  << " scales × " << NUM_CS_TYPES << " types)" << std::endl;
        std::cout << "  " << NUM_BLOB_COLUMNS << " blob detector columns (" << NUM_BLOB_SCALES
                  << " scales × " << NUM_BLOB_TYPES << " types)" << std::endl;

        int colIdx = 0;

        // Create orientation-selective columns (straight edge detectors)
        for (int oriIdx = 0; oriIdx < NUM_ORIENTATIONS; ++oriIdx) {
            double orientation = oriIdx * ORIENTATION_STEP;

            for (int freqIdx = 0; freqIdx < NUM_FREQUENCIES; ++freqIdx) {
                CorticalColumn col;
                col.orientation = orientation;
                col.spatialFrequency = SPATIAL_FREQUENCIES[freqIdx];
                col.featureType = "orientation_" + FREQ_NAMES[freqIdx];
                col.gaborKernel = createGaborKernel(col.orientation, col.spatialFrequency);

                // Create column
                col.column = factory.createColumn();
                v1Nucleus->addColumn(col.column->getId());

                std::cout << "\n--- Column " << colIdx << " (Orientation: " << col.orientation << "°, "
                          << col.featureType << ", λ=" << col.spatialFrequency << ") ---" << std::endl;

            // Create Layer 1 (Apical dendrites, modulatory)
            col.layer1 = factory.createLayer();
            col.column->addLayer(col.layer1->getId());
            auto layer1Cluster = factory.createCluster();
            col.layer1->addCluster(layer1Cluster->getId());

            for (int i = 0; i < LAYER1_NEURONS; ++i) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer1Neurons.push_back(neuron);
                layer1Cluster->addNeuron(neuron->getId());
            }
            std::cout << "  ✓ Layer 1: " << col.layer1Neurons.size() << " neurons (modulatory)" << std::endl;

            // Create Layer 2/3 (Superficial pyramidal)
            col.layer23 = factory.createLayer();
            col.column->addLayer(col.layer23->getId());
            auto layer23Cluster = factory.createCluster();
            col.layer23->addCluster(layer23Cluster->getId());

            for (int i = 0; i < LAYER23_NEURONS; ++i) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer23Neurons.push_back(neuron);
                layer23Cluster->addNeuron(neuron->getId());
            }
            std::cout << "  ✓ Layer 2/3: " << col.layer23Neurons.size() << " neurons (superficial pyramidal)" << std::endl;

            // Create Layer 4 (Granular input layer)
            col.layer4 = factory.createLayer();
            col.column->addLayer(col.layer4->getId());
            auto layer4Cluster = factory.createCluster();
            col.layer4->addCluster(layer4Cluster->getId());

            const int LAYER4_NEURONS = LAYER4_SIZE * LAYER4_SIZE;
            for (int i = 0; i < LAYER4_NEURONS; ++i) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer4Neurons.push_back(neuron);
                layer4Cluster->addNeuron(neuron->getId());
            }
            std::cout << "  ✓ Layer 4: " << col.layer4Neurons.size() << " neurons (granular input, "
                      << LAYER4_SIZE << "x" << LAYER4_SIZE << " grid)" << std::endl;

            // Create Layer 5 (Deep pyramidal output)
            col.layer5 = factory.createLayer();
            col.column->addLayer(col.layer5->getId());
            auto layer5Cluster = factory.createCluster();
            col.layer5->addCluster(layer5Cluster->getId());

            for (int i = 0; i < LAYER5_NEURONS; ++i) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer5Neurons.push_back(neuron);
                layer5Cluster->addNeuron(neuron->getId());
            }
            std::cout << "  ✓ Layer 5: " << col.layer5Neurons.size() << " neurons (deep pyramidal output)" << std::endl;

            // Create Layer 6 (Corticothalamic feedback)
            col.layer6 = factory.createLayer();
            col.column->addLayer(col.layer6->getId());
            auto layer6Cluster = factory.createCluster();
            col.layer6->addCluster(layer6Cluster->getId());

            for (int i = 0; i < LAYER6_NEURONS; ++i) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer6Neurons.push_back(neuron);
                layer6Cluster->addNeuron(neuron->getId());
            }
            std::cout << "  ✓ Layer 6: " << col.layer6Neurons.size() << " neurons (corticothalamic feedback)" << std::endl;

            corticalColumns.push_back(col);
                colIdx++;
            }
        }

        // Create center-surround columns (loop/hole detectors)
        for (int scaleIdx = 0; scaleIdx < NUM_CS_SCALES; ++scaleIdx) {
            auto [centerSigma, surroundSigma] = CS_PARAMS[scaleIdx];
            std::string scaleName = CS_SCALE_NAMES[scaleIdx];

            for (int typeIdx = 0; typeIdx < NUM_CS_TYPES; ++typeIdx) {
                bool onCenter = (typeIdx == 0);
                std::string typeName = CS_TYPE_NAMES[typeIdx];

                CorticalColumn col;
                col.orientation = 0.0;  // Not orientation-selective
                col.spatialFrequency = centerSigma;  // Store center sigma
                col.featureType = "center_surround_" + scaleName + "_" + typeName;
                col.gaborKernel = createCenterSurroundKernel(centerSigma, surroundSigma, onCenter);

                // Create column
                col.column = factory.createColumn();
                v1Nucleus->addColumn(col.column->getId());

                std::cout << "\n--- Column " << colIdx << " (Center-Surround: " << scaleName
                          << ", " << typeName << ", σ_c=" << centerSigma << ", σ_s=" << surroundSigma << ") ---" << std::endl;

                // Create layers (same structure as orientation columns)
                // Layer 1
                col.layer1 = factory.createLayer();
                col.column->addLayer(col.layer1->getId());
                auto layer1Cluster = factory.createCluster();
                col.layer1->addCluster(layer1Cluster->getId());
                for (int i = 0; i < LAYER1_NEURONS; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer1Neurons.push_back(neuron);
                    layer1Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 1: " << col.layer1Neurons.size() << " neurons" << std::endl;

                // Layer 2/3
                col.layer23 = factory.createLayer();
                col.column->addLayer(col.layer23->getId());
                auto layer23Cluster = factory.createCluster();
                col.layer23->addCluster(layer23Cluster->getId());
                for (int i = 0; i < LAYER23_NEURONS; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer23Neurons.push_back(neuron);
                    layer23Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 2/3: " << col.layer23Neurons.size() << " neurons" << std::endl;

                // Layer 4
                col.layer4 = factory.createLayer();
                col.column->addLayer(col.layer4->getId());
                auto layer4Cluster = factory.createCluster();
                col.layer4->addCluster(layer4Cluster->getId());
                for (int i = 0; i < LAYER4_SIZE * LAYER4_SIZE; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer4Neurons.push_back(neuron);
                    layer4Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 4: " << col.layer4Neurons.size() << " neurons" << std::endl;

                // Layer 5
                col.layer5 = factory.createLayer();
                col.column->addLayer(col.layer5->getId());
                auto layer5Cluster = factory.createCluster();
                col.layer5->addCluster(layer5Cluster->getId());
                for (int i = 0; i < LAYER5_NEURONS; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer5Neurons.push_back(neuron);
                    layer5Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 5: " << col.layer5Neurons.size() << " neurons" << std::endl;

                // Layer 6
                col.layer6 = factory.createLayer();
                col.column->addLayer(col.layer6->getId());
                auto layer6Cluster = factory.createCluster();
                col.layer6->addCluster(layer6Cluster->getId());
                for (int i = 0; i < LAYER6_NEURONS; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer6Neurons.push_back(neuron);
                    layer6Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 6: " << col.layer6Neurons.size() << " neurons" << std::endl;

                corticalColumns.push_back(col);
                colIdx++;
            }
        }

        // Create blob detector columns (solid region detectors)
        for (int scaleIdx = 0; scaleIdx < NUM_BLOB_SCALES; ++scaleIdx) {
            double sigma = BLOB_SIGMAS[scaleIdx];
            std::string scaleName = BLOB_SCALE_NAMES[scaleIdx];

            for (int typeIdx = 0; typeIdx < NUM_BLOB_TYPES; ++typeIdx) {
                double polarity = (typeIdx == 0) ? 1.0 : -1.0;
                std::string typeName = (typeIdx == 0) ? "positive" : "negative";

                CorticalColumn col;
                col.orientation = 0.0;  // Not orientation-selective
                col.spatialFrequency = sigma;  // Store sigma
                col.featureType = "blob_" + scaleName + "_" + typeName;

                // Create blob kernel and apply polarity
                auto blobKernel = createBlobKernel(sigma);
                if (polarity < 0) {
                    for (auto& row : blobKernel) {
                        for (auto& val : row) {
                            val *= -1.0;
                        }
                    }
                }
                col.gaborKernel = blobKernel;

                // Create column
                col.column = factory.createColumn();
                v1Nucleus->addColumn(col.column->getId());

                std::cout << "\n--- Column " << colIdx << " (Blob: " << scaleName
                          << ", " << typeName << ", σ=" << sigma << ") ---" << std::endl;

                // Create layers (same structure)
                // Layer 1
                col.layer1 = factory.createLayer();
                col.column->addLayer(col.layer1->getId());
                auto layer1Cluster = factory.createCluster();
                col.layer1->addCluster(layer1Cluster->getId());
                for (int i = 0; i < LAYER1_NEURONS; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer1Neurons.push_back(neuron);
                    layer1Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 1: " << col.layer1Neurons.size() << " neurons" << std::endl;

                // Layer 2/3
                col.layer23 = factory.createLayer();
                col.column->addLayer(col.layer23->getId());
                auto layer23Cluster = factory.createCluster();
                col.layer23->addCluster(layer23Cluster->getId());
                for (int i = 0; i < LAYER23_NEURONS; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer23Neurons.push_back(neuron);
                    layer23Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 2/3: " << col.layer23Neurons.size() << " neurons" << std::endl;

                // Layer 4
                col.layer4 = factory.createLayer();
                col.column->addLayer(col.layer4->getId());
                auto layer4Cluster = factory.createCluster();
                col.layer4->addCluster(layer4Cluster->getId());
                for (int i = 0; i < LAYER4_SIZE * LAYER4_SIZE; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer4Neurons.push_back(neuron);
                    layer4Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 4: " << col.layer4Neurons.size() << " neurons" << std::endl;

                // Layer 5
                col.layer5 = factory.createLayer();
                col.column->addLayer(col.layer5->getId());
                auto layer5Cluster = factory.createCluster();
                col.layer5->addCluster(layer5Cluster->getId());
                for (int i = 0; i < LAYER5_NEURONS; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer5Neurons.push_back(neuron);
                    layer5Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 5: " << col.layer5Neurons.size() << " neurons" << std::endl;

                // Layer 6
                col.layer6 = factory.createLayer();
                col.column->addLayer(col.layer6->getId());
                auto layer6Cluster = factory.createCluster();
                col.layer6->addCluster(layer6Cluster->getId());
                for (int i = 0; i < LAYER6_NEURONS; ++i) {
                    auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                    col.layer6Neurons.push_back(neuron);
                    layer6Cluster->addNeuron(neuron->getId());
                }
                std::cout << "  ✓ Layer 6: " << col.layer6Neurons.size() << " neurons" << std::endl;

                corticalColumns.push_back(col);
                colIdx++;
            }
        }

        // Create specialized detector columns (for 4→9 and 7→9 distinction)
        // 2 top-loop detectors + 2 gap detectors = 4 columns
        std::cout << "\n=== Creating Specialized Detector Columns ===" << std::endl;

        // Top-loop detectors (2 columns with different sensitivities)
        for (int i = 0; i < 2; ++i) {
            CorticalColumn col;
            col.orientation = 0.0;
            col.spatialFrequency = 0.0;
            col.featureType = "top_loop_detector_" + std::to_string(i);
            col.gaborKernel = createTopLoopKernel();

            // Apply different scaling for sensitivity variation
            if (i == 1) {
                for (auto& row : col.gaborKernel) {
                    for (auto& val : row) {
                        val *= 1.5;  // More sensitive version
                    }
                }
            }

            col.column = factory.createColumn();
            v1Nucleus->addColumn(col.column->getId());

            std::cout << "\n--- Column " << colIdx << " (Top-Loop Detector " << i << ") ---" << std::endl;

            // Create layers (same structure as other columns)
            col.layer1 = factory.createLayer();
            col.column->addLayer(col.layer1->getId());
            auto layer1Cluster = factory.createCluster();
            col.layer1->addCluster(layer1Cluster->getId());
            for (int j = 0; j < LAYER1_NEURONS; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer1Neurons.push_back(neuron);
                layer1Cluster->addNeuron(neuron->getId());
            }

            col.layer23 = factory.createLayer();
            col.column->addLayer(col.layer23->getId());
            auto layer23Cluster = factory.createCluster();
            col.layer23->addCluster(layer23Cluster->getId());
            for (int j = 0; j < LAYER23_NEURONS; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer23Neurons.push_back(neuron);
                layer23Cluster->addNeuron(neuron->getId());
            }

            col.layer4 = factory.createLayer();
            col.column->addLayer(col.layer4->getId());
            auto layer4Cluster = factory.createCluster();
            col.layer4->addCluster(layer4Cluster->getId());
            for (int j = 0; j < LAYER4_SIZE * LAYER4_SIZE; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer4Neurons.push_back(neuron);
                layer4Cluster->addNeuron(neuron->getId());
            }

            col.layer5 = factory.createLayer();
            col.column->addLayer(col.layer5->getId());
            auto layer5Cluster = factory.createCluster();
            col.layer5->addCluster(layer5Cluster->getId());
            for (int j = 0; j < LAYER5_NEURONS; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer5Neurons.push_back(neuron);
                layer5Cluster->addNeuron(neuron->getId());
            }

            col.layer6 = factory.createLayer();
            col.column->addLayer(col.layer6->getId());
            auto layer6Cluster = factory.createCluster();
            col.layer6->addCluster(layer6Cluster->getId());
            for (int j = 0; j < LAYER6_NEURONS; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer6Neurons.push_back(neuron);
                layer6Cluster->addNeuron(neuron->getId());
            }

            corticalColumns.push_back(col);
            colIdx++;
        }

        // Gap detectors (2 columns with different sensitivities)
        for (int i = 0; i < 2; ++i) {
            CorticalColumn col;
            col.orientation = 0.0;
            col.spatialFrequency = 0.0;
            col.featureType = "gap_detector_" + std::to_string(i);
            col.gaborKernel = createGapKernel();

            // Apply different scaling for sensitivity variation
            if (i == 1) {
                for (auto& row : col.gaborKernel) {
                    for (auto& val : row) {
                        val *= 1.5;  // More sensitive version
                    }
                }
            }

            col.column = factory.createColumn();
            v1Nucleus->addColumn(col.column->getId());

            std::cout << "\n--- Column " << colIdx << " (Gap Detector " << i << ") ---" << std::endl;

            // Create layers (same structure)
            col.layer1 = factory.createLayer();
            col.column->addLayer(col.layer1->getId());
            auto layer1Cluster = factory.createCluster();
            col.layer1->addCluster(layer1Cluster->getId());
            for (int j = 0; j < LAYER1_NEURONS; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer1Neurons.push_back(neuron);
                layer1Cluster->addNeuron(neuron->getId());
            }

            col.layer23 = factory.createLayer();
            col.column->addLayer(col.layer23->getId());
            auto layer23Cluster = factory.createCluster();
            col.layer23->addCluster(layer23Cluster->getId());
            for (int j = 0; j < LAYER23_NEURONS; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer23Neurons.push_back(neuron);
                layer23Cluster->addNeuron(neuron->getId());
            }

            col.layer4 = factory.createLayer();
            col.column->addLayer(col.layer4->getId());
            auto layer4Cluster = factory.createCluster();
            col.layer4->addCluster(layer4Cluster->getId());
            for (int j = 0; j < LAYER4_SIZE * LAYER4_SIZE; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer4Neurons.push_back(neuron);
                layer4Cluster->addNeuron(neuron->getId());
            }

            col.layer5 = factory.createLayer();
            col.column->addLayer(col.layer5->getId());
            auto layer5Cluster = factory.createCluster();
            col.layer5->addCluster(layer5Cluster->getId());
            for (int j = 0; j < LAYER5_NEURONS; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer5Neurons.push_back(neuron);
                layer5Cluster->addNeuron(neuron->getId());
            }

            col.layer6 = factory.createLayer();
            col.column->addLayer(col.layer6->getId());
            auto layer6Cluster = factory.createCluster();
            col.layer6->addCluster(layer6Cluster->getId());
            for (int j = 0; j < LAYER6_NEURONS; ++j) {
                auto neuron = factory.createNeuron(config.neuronWindow, config.neuronThreshold, config.neuronMaxPatterns);
                col.layer6Neurons.push_back(neuron);
                layer6Cluster->addNeuron(neuron->getId());
            }

            corticalColumns.push_back(col);
            colIdx++;
        }

        // REMOVED: Bottom-curve, horizontal-bar, and middle-constriction detectors for speed optimization
        // Keeping only top-loop and gap detectors (most important for 4→9, 7→9 distinction)

        std::cout << "\n✓ Created " << NUM_COLUMNS << " cortical columns (OPTIMIZED):" << std::endl;
        std::cout << "  - " << NUM_ORIENTATION_COLUMNS << " orientation columns (4 orientations × 2 frequencies)" << std::endl;
        std::cout << "  - " << NUM_CS_COLUMNS << " center-surround columns (2 scales × 2 types)" << std::endl;
        std::cout << "  - " << NUM_BLOB_COLUMNS << " blob detector columns (removed for speed)" << std::endl;
        std::cout << "  - " << NUM_SPECIALIZED << " specialized detector columns:" << std::endl;
        std::cout << "    * 2 top-loop detectors (4→9, 7→9)" << std::endl;
        std::cout << "    * 2 gap detectors (4→9)" << std::endl;

        // ========================================================================
        // Create Inter-Layer Connections Within Each Column
        // Following canonical cortical microcircuit:
        //   Input → Layer 4 → Layer 2/3 → Layer 5 → Layer 6 → (feedback to Layer 4)
        // ========================================================================
        std::cout << "\n=== Creating Inter-Layer Connections ===" << std::endl;

        // Storage for all created axons, synapses, and dendrites
        std::vector<std::shared_ptr<Axon>> allAxons;
        std::vector<std::shared_ptr<Synapse>> allSynapses;
        std::vector<std::shared_ptr<Dendrite>> allDendrites;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);

        int totalConnections = 0;

        for (int colIdx = 0; colIdx < NUM_COLUMNS; ++colIdx) {
            auto& col = corticalColumns[colIdx];

            std::cout << "Column " << colIdx << " (" << col.orientation << "°):" << std::endl;

            // ====================================================================
            // Layer 4 → Layer 2/3 (feedforward with spatial pooling structure)
            // ====================================================================
            // Layer 2/3 neurons are organized into functional groups:
            // - First 128 neurons: Random connectivity (general feature integration)
            // - Next 64 neurons: Spatial pooling neurons (4 quadrants × 16 neurons each)
            // - Last 64 neurons: Global pooling neurons (whole-field integration)

            int l4_to_l23 = 0;
            const int GENERAL_L23_NEURONS = 128;  // General feature integration
            const int SPATIAL_POOL_NEURONS = 64;  // Spatial pooling (4 quadrants × 16)
            const int GLOBAL_POOL_NEURONS = 64;   // Global pooling

            // Ensure we have enough L2/3 neurons (should be 256)
            if (col.layer23Neurons.size() < GENERAL_L23_NEURONS + SPATIAL_POOL_NEURONS + GLOBAL_POOL_NEURONS) {
                std::cerr << "Warning: Not enough L2/3 neurons for spatial pooling!" << std::endl;
            }

            // Create axons for all L4 neurons
            for (auto& l4Neuron : col.layer4Neurons) {
                if (l4Neuron->getAxonId() == 0) {
                    auto axon = factory.createAxon(l4Neuron->getId());
                    l4Neuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                }
            }

            // 1. General L2/3 neurons: Random 50% connectivity from all L4 neurons
            for (int i = 0; i < GENERAL_L23_NEURONS && i < col.layer23Neurons.size(); ++i) {
                auto& l23Neuron = col.layer23Neurons[i];

                for (auto& l4Neuron : col.layer4Neurons) {
                    if (dis(gen) < 0.5) {
                        auto dendrite = factory.createDendrite(l23Neuron->getId());
                        l23Neuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        auto synapse = factory.createSynapse(
                            l4Neuron->getAxonId(),
                            dendrite->getId(),
                            1.0,  // weight
                            1.0   // delay (ms)
                        );
                        allSynapses.push_back(synapse);
                        l4_to_l23++;
                    }
                }
            }

            // 2. Spatial pooling neurons: Connect to specific quadrants of L4 grid
            // L4 is 8x8 grid, divide into 4 quadrants (4x4 each)
            const int QUADRANT_SIZE = 4;  // 4x4 neurons per quadrant
            const int NEURONS_PER_QUADRANT = 16;  // 16 L2/3 neurons per quadrant

            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                int qRow = (quadrant / 2) * QUADRANT_SIZE;  // 0 or 4
                int qCol = (quadrant % 2) * QUADRANT_SIZE;  // 0 or 4

                // Connect 16 L2/3 neurons to this quadrant
                for (int neuronIdx = 0; neuronIdx < NEURONS_PER_QUADRANT; ++neuronIdx) {
                    int l23Idx = GENERAL_L23_NEURONS + quadrant * NEURONS_PER_QUADRANT + neuronIdx;
                    if (l23Idx >= col.layer23Neurons.size()) break;

                    auto& l23Neuron = col.layer23Neurons[l23Idx];

                    // Connect to L4 neurons in this quadrant with high connectivity (80%)
                    for (int row = qRow; row < qRow + QUADRANT_SIZE; ++row) {
                        for (int col_x = qCol; col_x < qCol + QUADRANT_SIZE; ++col_x) {
                            int l4Idx = row * LAYER4_SIZE + col_x;
                            if (l4Idx >= col.layer4Neurons.size()) continue;

                            auto& l4Neuron = col.layer4Neurons[l4Idx];

                            if (dis(gen) < 0.8) {  // High connectivity within quadrant
                                auto dendrite = factory.createDendrite(l23Neuron->getId());
                                l23Neuron->addDendrite(dendrite->getId());
                                allDendrites.push_back(dendrite);

                                auto synapse = factory.createSynapse(
                                    l4Neuron->getAxonId(),
                                    dendrite->getId(),
                                    1.2,  // Stronger weight for spatial pooling
                                    1.0   // delay (ms)
                                );
                                allSynapses.push_back(synapse);
                                l4_to_l23++;
                            }
                        }
                    }
                }
            }

            // 3. Global pooling neurons: Connect to all L4 neurons with moderate connectivity
            for (int i = 0; i < GLOBAL_POOL_NEURONS; ++i) {
                int l23Idx = GENERAL_L23_NEURONS + SPATIAL_POOL_NEURONS + i;
                if (l23Idx >= col.layer23Neurons.size()) break;

                auto& l23Neuron = col.layer23Neurons[l23Idx];

                for (auto& l4Neuron : col.layer4Neurons) {
                    if (dis(gen) < 0.6) {  // Moderate global connectivity
                        auto dendrite = factory.createDendrite(l23Neuron->getId());
                        l23Neuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        auto synapse = factory.createSynapse(
                            l4Neuron->getAxonId(),
                            dendrite->getId(),
                            0.8,  // Moderate weight for global pooling
                            1.0   // delay (ms)
                        );
                        allSynapses.push_back(synapse);
                        l4_to_l23++;
                    }
                }
            }

            std::cout << "  ✓ Layer 4 → Layer 2/3: " << l4_to_l23 << " synapses" << std::endl;
            std::cout << "    - " << GENERAL_L23_NEURONS << " general neurons (random connectivity)" << std::endl;
            std::cout << "    - " << SPATIAL_POOL_NEURONS << " spatial pooling neurons (4 quadrants)" << std::endl;
            std::cout << "    - " << GLOBAL_POOL_NEURONS << " global pooling neurons" << std::endl;
            totalConnections += l4_to_l23;

            // ====================================================================
            // Layer 2/3 → Layer 5 (feedforward, 40% connectivity)
            // ====================================================================
            int l23_to_l5 = 0;
            for (auto& l23Neuron : col.layer23Neurons) {
                // Create axon for L2/3 neuron if not exists
                if (l23Neuron->getAxonId() == 0) {
                    auto axon = factory.createAxon(l23Neuron->getId());
                    l23Neuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                }

                for (auto& l5Neuron : col.layer5Neurons) {
                    if (dis(gen) < 0.4) {
                        // Create dendrite for L5 neuron
                        auto dendrite = factory.createDendrite(l5Neuron->getId());
                        l5Neuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        // Create synapse
                        auto synapse = factory.createSynapse(
                            l23Neuron->getAxonId(),
                            dendrite->getId(),
                            1.0,  // weight
                            1.0   // delay (ms)
                        );
                        allSynapses.push_back(synapse);
                        l23_to_l5++;
                    }
                }
            }
            std::cout << "  ✓ Layer 2/3 → Layer 5: " << l23_to_l5 << " synapses" << std::endl;
            totalConnections += l23_to_l5;

            // ====================================================================
            // Layer 5 → Layer 6 (feedforward, 30% connectivity)
            // ====================================================================
            int l5_to_l6 = 0;
            for (auto& l5Neuron : col.layer5Neurons) {
                // Create axon for L5 neuron if not exists
                if (l5Neuron->getAxonId() == 0) {
                    auto axon = factory.createAxon(l5Neuron->getId());
                    l5Neuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                }

                for (auto& l6Neuron : col.layer6Neurons) {
                    if (dis(gen) < 0.3) {
                        // Create dendrite for L6 neuron
                        auto dendrite = factory.createDendrite(l6Neuron->getId());
                        l6Neuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        // Create synapse
                        auto synapse = factory.createSynapse(
                            l5Neuron->getAxonId(),
                            dendrite->getId(),
                            1.0,  // weight
                            1.0   // delay (ms)
                        );
                        allSynapses.push_back(synapse);
                        l5_to_l6++;
                    }
                }
            }
            std::cout << "  ✓ Layer 5 → Layer 6: " << l5_to_l6 << " synapses" << std::endl;
            totalConnections += l5_to_l6;

            // ====================================================================
            // Layer 6 → Layer 4 (feedback, 20% connectivity, weaker weights)
            // ====================================================================
            int l6_to_l4 = 0;
            for (auto& l6Neuron : col.layer6Neurons) {
                // Create axon for L6 neuron if not exists
                if (l6Neuron->getAxonId() == 0) {
                    auto axon = factory.createAxon(l6Neuron->getId());
                    l6Neuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                }

                for (auto& l4Neuron : col.layer4Neurons) {
                    if (dis(gen) < 0.2) {
                        // Create dendrite for L4 neuron
                        auto dendrite = factory.createDendrite(l4Neuron->getId());
                        l4Neuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        // Create synapse (weaker feedback)
                        auto synapse = factory.createSynapse(
                            l6Neuron->getAxonId(),
                            dendrite->getId(),
                            0.5,  // weaker weight for feedback
                            1.0   // delay (ms)
                        );
                        allSynapses.push_back(synapse);
                        l6_to_l4++;
                    }
                }
            }
            std::cout << "  ✓ Layer 6 → Layer 4 (feedback): " << l6_to_l4 << " synapses" << std::endl;
            totalConnections += l6_to_l4;

            // ====================================================================
            // Layer 2/3 → Layer 1 (modulatory, 10% connectivity, weak)
            // ====================================================================
            int l23_to_l1 = 0;
            for (auto& l1Neuron : col.layer1Neurons) {
                for (auto& l23Neuron : col.layer23Neurons) {
                    if (dis(gen) < 0.1) {
                        // Create dendrite for L1 neuron
                        auto dendrite = factory.createDendrite(l1Neuron->getId());
                        l1Neuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        // Create synapse (weak modulatory)
                        auto synapse = factory.createSynapse(
                            l23Neuron->getAxonId(),
                            dendrite->getId(),
                            0.3,  // weak modulatory weight
                            1.0   // delay (ms)
                        );
                        allSynapses.push_back(synapse);
                        l23_to_l1++;
                    }
                }
            }
            std::cout << "  ✓ Layer 2/3 → Layer 1 (modulatory): " << l23_to_l1 << " synapses" << std::endl;
            totalConnections += l23_to_l1;
        }

        std::cout << "\n✓ Total intra-column connections: " << totalConnections << " synapses" << std::endl;

        // ========================================================================
        // Create Lateral Inter-Column Connections (Layer 2/3 ↔ Layer 2/3)
        // Sparse connectivity between neighboring columns for horizontal integration
        // ========================================================================
        std::cout << "\n=== Creating Lateral Inter-Column Connections ===" << std::endl;

        int lateralConnections = 0;
        const double LATERAL_CONNECTIVITY = config.lateralConnectivity;
        const int NEIGHBOR_RANGE = config.neighborRange;

        for (int i = 0; i < NUM_COLUMNS; ++i) {
            // Connect to neighboring columns (wrap around for circular topology)
            for (int offset = -NEIGHBOR_RANGE; offset <= NEIGHBOR_RANGE; ++offset) {
                if (offset == 0) continue;  // Skip self-connections

                int j = (i + offset + NUM_COLUMNS) % NUM_COLUMNS;

                // Connect Layer 2/3 neurons between neighboring columns
                for (auto& neuronI : corticalColumns[i].layer23Neurons) {
                    for (auto& neuronJ : corticalColumns[j].layer23Neurons) {
                        if (dis(gen) < LATERAL_CONNECTIVITY) {
                            // Create dendrite for target neuron
                            auto dendrite = factory.createDendrite(neuronJ->getId());
                            neuronJ->addDendrite(dendrite->getId());
                            allDendrites.push_back(dendrite);

                            // Create synapse (weak lateral)
                            auto synapse = factory.createSynapse(
                                neuronI->getAxonId(),
                                dendrite->getId(),
                                0.3,  // Slightly stronger lateral weight
                                1.5   // Slightly longer delay for lateral propagation
                            );
                            allSynapses.push_back(synapse);
                            lateralConnections++;
                        }
                    }
                }
            }
        }
        std::cout << "✓ Created " << lateralConnections << " lateral connections between neighboring columns" << std::endl;
        std::cout << "  Connectivity: " << (LATERAL_CONNECTIVITY * 100) << "% between ±"
                  << NEIGHBOR_RANGE << " neighboring columns" << std::endl;

        // ========================================================================
        // Create Recurrent Connections Within Layer 2/3 (for temporal integration)
        // Allows neurons to maintain activity over time and integrate patterns
        // ========================================================================
        std::cout << "\n=== Creating Recurrent Connections Within Layer 2/3 ===" << std::endl;

        int recurrentConnections = 0;
        const double RECURRENT_CONNECTIVITY = config.recurrentConnectivity;
        const double RECURRENT_WEIGHT = config.recurrentWeight;
        const double RECURRENT_DELAY = config.recurrentDelay;

        for (int colIdx = 0; colIdx < NUM_COLUMNS; ++colIdx) {
            auto& col = corticalColumns[colIdx];

            // Create recurrent connections within Layer 2/3
            for (size_t i = 0; i < col.layer23Neurons.size(); ++i) {
                auto& sourceNeuron = col.layer23Neurons[i];

                // Ensure source neuron has an axon
                if (sourceNeuron->getAxonId() == 0) {
                    auto axon = factory.createAxon(sourceNeuron->getId());
                    sourceNeuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                }

                // Connect to other neurons in the same layer (excluding self)
                for (size_t j = 0; j < col.layer23Neurons.size(); ++j) {
                    if (i == j) continue;  // Skip self-connections

                    auto& targetNeuron = col.layer23Neurons[j];

                    if (dis(gen) < RECURRENT_CONNECTIVITY) {
                        // Create dendrite for target neuron
                        auto dendrite = factory.createDendrite(targetNeuron->getId());
                        targetNeuron->addDendrite(dendrite->getId());
                        allDendrites.push_back(dendrite);

                        // Create recurrent synapse
                        auto synapse = factory.createSynapse(
                            sourceNeuron->getAxonId(),
                            dendrite->getId(),
                            RECURRENT_WEIGHT,  // Moderate weight for temporal integration
                            RECURRENT_DELAY    // Delayed feedback for temporal dynamics
                        );
                        allSynapses.push_back(synapse);
                        recurrentConnections++;
                    }
                }
            }
        }

        std::cout << "✓ Created " << recurrentConnections << " recurrent connections within Layer 2/3" << std::endl;
        std::cout << "  Connectivity: " << (RECURRENT_CONNECTIVITY * 100) << "% within each column" << std::endl;
        std::cout << "  Weight: " << RECURRENT_WEIGHT << ", Delay: " << RECURRENT_DELAY << "ms" << std::endl;
        std::cout << "  Purpose: Temporal integration and sustained activity for pattern recognition" << std::endl;

        const int LAYER4_NEURONS = LAYER4_SIZE * LAYER4_SIZE;
        const int NEURONS_PER_COLUMN = LAYER1_NEURONS + LAYER23_NEURONS + LAYER4_NEURONS + LAYER5_NEURONS + LAYER6_NEURONS;

        std::cout << "\n=== Architecture Summary ===" << std::endl;
        std::cout << "Columns: " << NUM_COLUMNS << std::endl;
        std::cout << "Neurons per column:" << std::endl;
        std::cout << "  Layer 1: " << LAYER1_NEURONS << " (modulatory)" << std::endl;
        std::cout << "  Layer 2/3: " << LAYER23_NEURONS << " (superficial pyramidal)" << std::endl;
        std::cout << "  Layer 4: " << LAYER4_NEURONS << " (granular input, " << LAYER4_SIZE << "x" << LAYER4_SIZE << " grid)" << std::endl;
        std::cout << "  Layer 5: " << LAYER5_NEURONS << " (deep pyramidal)" << std::endl;
        std::cout << "  Layer 6: " << LAYER6_NEURONS << " (corticothalamic)" << std::endl;
        std::cout << "Total columnar neurons: " << (NUM_COLUMNS * NEURONS_PER_COLUMN) << std::endl;
        std::cout << "Total axons: " << allAxons.size() << std::endl;
        std::cout << "Total synapses: " << allSynapses.size() << std::endl;
        std::cout << "Total dendrites: " << allDendrites.size() << std::endl;
        std::cout << "Grand total: " << (NUM_COLUMNS * NEURONS_PER_COLUMN) << " neurons" << std::endl;

        std::cout << "\n✓ Multi-column architecture with full connectivity created successfully!" << std::endl;

        // ========================================================================
        // Load EMNIST Letters Data and Test Architecture
        // ========================================================================
        std::cout << "\n=== Loading EMNIST Letters Data ===" << std::endl;

        EMNISTLoader trainLoader(EMNISTLoader::Variant::LETTERS);
        if (!trainLoader.load(config.trainImagesPath, config.trainLabelsPath)) {
            std::cerr << "Failed to load training data" << std::endl;
            return 1;
        }
        std::cout << "✓ Loaded " << trainLoader.size() << " training images" << std::endl;

        // ========================================================================
        // Initialize SpikeProcessor and NetworkPropagator
        // ========================================================================
        std::cout << "\n=== Initializing Spike Processing System ===" << std::endl;

        auto spikeProcessor = std::make_shared<SpikeProcessor>(10000, config.numThreads);
        auto networkPropagator = std::make_shared<NetworkPropagator>(spikeProcessor);

        // Register all neurons
        std::vector<std::shared_ptr<Neuron>> allNeurons;
        for (auto& col : corticalColumns) {
            allNeurons.insert(allNeurons.end(), col.layer1Neurons.begin(), col.layer1Neurons.end());
            allNeurons.insert(allNeurons.end(), col.layer23Neurons.begin(), col.layer23Neurons.end());
            allNeurons.insert(allNeurons.end(), col.layer4Neurons.begin(), col.layer4Neurons.end());
            allNeurons.insert(allNeurons.end(), col.layer5Neurons.begin(), col.layer5Neurons.end());
            allNeurons.insert(allNeurons.end(), col.layer6Neurons.begin(), col.layer6Neurons.end());
        }

        for (const auto& neuron : allNeurons) {
            networkPropagator->registerNeuron(neuron);
            neuron->setNetworkPropagator(networkPropagator);
        }

        for (const auto& axon : allAxons) {
            networkPropagator->registerAxon(axon);
        }

        for (const auto& synapse : allSynapses) {
            networkPropagator->registerSynapse(synapse);
        }

        for (const auto& dendrite : allDendrites) {
            networkPropagator->registerDendrite(dendrite);
            dendrite->setNetworkPropagator(networkPropagator);
            spikeProcessor->registerDendrite(dendrite);
        }

        spikeProcessor->setRealTimeSync(false);  // Fast mode

        std::cout << "✓ Registered " << allNeurons.size() << " neurons" << std::endl;
        std::cout << "✓ Registered " << allAxons.size() << " axons" << std::endl;
        std::cout << "✓ Registered " << allSynapses.size() << " synapses" << std::endl;
        std::cout << "✓ Registered " << allDendrites.size() << " dendrites" << std::endl;

        // ========================================================================
        // Test with a few images to demonstrate architecture
        // ========================================================================
        std::cout << "\n=== Testing Architecture with Sample Images ===" << std::endl;

        const int NUM_TEST_IMAGES = 5;
        for (int i = 0; i < NUM_TEST_IMAGES && i < trainLoader.size(); ++i) {
            const auto& mnistImg = trainLoader.getImage(i);
            auto label = mnistImg.label;

            std::cout << "\nImage " << i << " (label=" << static_cast<int>(label) << "):" << std::endl;

            double currentTime = spikeProcessor->getCurrentTime();

            // Apply Gabor filters for each column and fire Layer 4 neurons
            for (int colIdx = 0; colIdx < NUM_COLUMNS; ++colIdx) {
                auto& col = corticalColumns[colIdx];

                // Apply Gabor filter at this column's orientation
                auto gaborResponse = applyGaborFilter(mnistImg.pixels, col.gaborKernel, LAYER4_SIZE);

                // Fire Layer 4 neurons based on Gabor response
                int firedCount = 0;
                for (size_t neuronIdx = 0; neuronIdx < col.layer4Neurons.size() && neuronIdx < gaborResponse.size(); ++neuronIdx) {
                    if (gaborResponse[neuronIdx] > config.gaborThreshold) {
                        double firingTime = currentTime + (1.0 - gaborResponse[neuronIdx]) * 10.0;
                        col.layer4Neurons[neuronIdx]->fireSignature(firingTime);
                        networkPropagator->fireNeuron(col.layer4Neurons[neuronIdx]->getId(), firingTime);
                        firedCount++;
                    }
                }

                if (firedCount > 0) {
                    std::cout << "  Column " << colIdx << " (" << col.orientation << "°): "
                              << firedCount << " Layer 4 neurons fired" << std::endl;
                }
            }

            // Advance time to allow propagation through all 6 layers
            // Layer 4 → 2/3 → 5 → 6 needs ~4-5ms with 1ms delays
            currentTime += 10.0;  // 10ms per image for full propagation
        }

        std::cout << "\n✓ Architecture test complete!" << std::endl;

        // ========================================================================
        // Create Output Layer with Population Coding
        // ========================================================================
        std::cout << "\n=== Creating Output Layer ===" << std::endl;

        // Create a separate column for output layer
        auto outputColumn = factory.createColumn();
        v1Nucleus->addColumn(outputColumn->getId());

        auto outputLayer = factory.createLayer();
        outputColumn->addLayer(outputLayer->getId());

        const int NUM_LETTERS = 26;  // A-Z
        const int NEURONS_PER_LETTER = config.neuronsPerClass;
        std::vector<std::vector<std::shared_ptr<Neuron>>> outputPopulations;

        for (int letter = 0; letter < NUM_LETTERS; ++letter) {
            auto letterCluster = factory.createCluster();
            outputLayer->addCluster(letterCluster->getId());

            std::vector<std::shared_ptr<Neuron>> population;
            for (int i = 0; i < NEURONS_PER_LETTER; ++i) {
                auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
                neuron->setSimilarityMetric(SimilarityMetric::HISTOGRAM);
                population.push_back(neuron);
                letterCluster->addNeuron(neuron->getId());
                allNeurons.push_back(neuron);
            }
            outputPopulations.push_back(population);
        }

        std::cout << "✓ Created output layer: " << (NUM_LETTERS * NEURONS_PER_LETTER) << " neurons ("
                  << NEURONS_PER_LETTER << " per letter)" << std::endl;

        // Connect Layer 5 neurons from all columns to output layer
        std::cout << "\n=== Connecting Layer 5 to Output Layer ===" << std::endl;

        std::random_device rd3;
        std::mt19937 gen3(rd3());
        std::uniform_real_distribution<> dis3(0.0, 1.0);

        int outputConnections = 0;
        double outputConnectivity = 0.5;  // 50% connectivity (increased from 30%)

        for (auto& col : corticalColumns) {
            for (auto& l5Neuron : col.layer5Neurons) {
                if (l5Neuron->getAxonId() == 0) {
                    auto axon = factory.createAxon(l5Neuron->getId());
                    l5Neuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                }

                for (auto& population : outputPopulations) {
                    for (auto& outputNeuron : population) {
                        if (dis3(gen3) < outputConnectivity) {
                            auto dendrite = factory.createDendrite(outputNeuron->getId());
                            outputNeuron->addDendrite(dendrite->getId());
                            allDendrites.push_back(dendrite);

                            auto synapse = factory.createSynapse(
                                l5Neuron->getAxonId(),
                                dendrite->getId(),
                                0.5,  // Initial weight
                                1.0   // Delay
                            );
                            allSynapses.push_back(synapse);
                            outputConnections++;
                        }
                    }
                }
            }
        }

        std::cout << "✓ Connected Layer 5 to output: " << outputConnections << " synapses" << std::endl;

        // Register output neurons with NetworkPropagator
        for (auto& population : outputPopulations) {
            for (auto& neuron : population) {
                networkPropagator->registerNeuron(neuron);
                neuron->setNetworkPropagator(networkPropagator);
            }
        }

        // Register new axons, synapses, dendrites
        for (size_t i = allAxons.size() - outputConnections; i < allAxons.size(); ++i) {
            networkPropagator->registerAxon(allAxons[i]);
        }

        for (size_t i = allSynapses.size() - outputConnections; i < allSynapses.size(); ++i) {
            networkPropagator->registerSynapse(allSynapses[i]);
        }

        for (size_t i = allDendrites.size() - outputConnections; i < allDendrites.size(); ++i) {
            networkPropagator->registerDendrite(allDendrites[i]);
            allDendrites[i]->setNetworkPropagator(networkPropagator);
            spikeProcessor->registerDendrite(allDendrites[i]);
        }

        std::cout << "✓ Registered output layer with spike processor" << std::endl;

        // ========================================================================
        // Create Position Encoding Layer (if enabled)
        // ========================================================================
        std::vector<std::vector<std::shared_ptr<Neuron>>> positionNeurons;  // [fixationIdx][neuronIdx]
        int positionSynapseCount = 0;  // Track for logging

        if (config.positionFeedbackEnabled && config.saccadesEnabled) {
            std::cout << "\n=== Creating Position Encoding Layer ===" << std::endl;

            // Create a separate column for position encoding
            auto positionColumn = factory.createColumn();
            v1Nucleus->addColumn(positionColumn->getId());

            auto positionLayer = factory.createLayer();
            positionColumn->addLayer(positionLayer->getId());

            // Create neurons for each fixation position
            for (int fixIdx = 0; fixIdx < config.numFixations; ++fixIdx) {
                auto fixationCluster = factory.createCluster();
                positionLayer->addCluster(fixationCluster->getId());

                std::vector<std::shared_ptr<Neuron>> fixationNeurons;
                for (int i = 0; i < config.positionNeuronsPerFixation; ++i) {
                    auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
                    fixationNeurons.push_back(neuron);
                    fixationCluster->addNeuron(neuron->getId());
                    allNeurons.push_back(neuron);
                }
                positionNeurons.push_back(fixationNeurons);
            }

            std::cout << "✓ Created position encoding layer: "
                      << (config.numFixations * config.positionNeuronsPerFixation) << " neurons ("
                      << config.positionNeuronsPerFixation << " per fixation)" << std::endl;

            // Connect position neurons DIRECTLY to output layer (bypass L2/3 and L5)
            // This allows position information to directly influence class predictions
            std::cout << "\n=== Connecting Position Neurons to Output Layer ===" << std::endl;

            int positionAxonCount = 0;
            double positionInitialWeight = 0.1;  // Small initial weight (output uses population coding)

            // Track starting indices for registration
            size_t axonStartIdx = allAxons.size();
            size_t synapseStartIdx = allSynapses.size();
            size_t dendriteStartIdx = allDendrites.size();

            for (auto& fixationNeurons : positionNeurons) {
                for (auto& posNeuron : fixationNeurons) {
                    // Create axon for position neuron
                    auto axon = factory.createAxon(posNeuron->getId());
                    posNeuron->setAxonId(axon->getId());
                    allAxons.push_back(axon);
                    positionAxonCount++;

                    // Connect to ALL output neurons (full connectivity)
                    // Each output neuron can learn which position neurons correlate with its class
                    for (auto& population : outputPopulations) {
                        for (auto& outputNeuron : population) {
                            auto dendrite = factory.createDendrite(outputNeuron->getId());
                            outputNeuron->addDendrite(dendrite->getId());
                            allDendrites.push_back(dendrite);

                            auto synapse = factory.createSynapse(
                                posNeuron->getAxonId(),
                                dendrite->getId(),
                                positionInitialWeight,  // Small initial weight
                                1.0   // 1ms delay
                            );
                            allSynapses.push_back(synapse);
                            positionSynapseCount++;
                        }
                    }
                }
            }

            int totalOutputNeurons = NUM_LETTERS * NEURONS_PER_LETTER;
            std::cout << "✓ Connected position neurons to output layer: "
                      << positionSynapseCount << " synapses from " << positionAxonCount << " axons" << std::endl;
            std::cout << "  - Full connectivity: " << positionAxonCount << " position neurons × "
                      << totalOutputNeurons << " output neurons = " << positionSynapseCount << " synapses" << std::endl;

            // Register position neurons with NetworkPropagator
            for (auto& fixationNeurons : positionNeurons) {
                for (auto& neuron : fixationNeurons) {
                    networkPropagator->registerNeuron(neuron);
                    neuron->setNetworkPropagator(networkPropagator);
                }
            }

            // Register new axons (use correct count!)
            for (size_t i = axonStartIdx; i < allAxons.size(); ++i) {
                networkPropagator->registerAxon(allAxons[i]);
            }

            // Register new synapses
            for (size_t i = synapseStartIdx; i < allSynapses.size(); ++i) {
                networkPropagator->registerSynapse(allSynapses[i]);
            }

            // Register new dendrites
            for (size_t i = dendriteStartIdx; i < allDendrites.size(); ++i) {
                networkPropagator->registerDendrite(allDendrites[i]);
                allDendrites[i]->setNetworkPropagator(networkPropagator);
                spikeProcessor->registerDendrite(allDendrites[i]);
            }

            std::cout << "✓ Registered position encoding layer with spike processor" << std::endl;
            std::cout << "  - " << positionAxonCount << " axons registered" << std::endl;
            std::cout << "  - " << positionSynapseCount << " synapses registered" << std::endl;
            std::cout << "  - " << (allDendrites.size() - dendriteStartIdx) << " dendrites registered" << std::endl;

            // Sample initial synapse weights for position neurons
            std::cout << "\n=== Position Encoding Initial Synapse Weights (Sample) ===" << std::endl;
            int sampleCount = 0;
            double totalWeight = 0.0;
            for (size_t i = synapseStartIdx; i < synapseStartIdx + std::min(size_t(100), size_t(positionSynapseCount)); ++i) {
                totalWeight += allSynapses[i]->getWeight();
                sampleCount++;
            }
            std::cout << "  Average weight of first 100 position synapses: " << (totalWeight / sampleCount) << std::endl;
        }

        // ========================================================================
        // Training Phase
        // ========================================================================
        std::cout << "\n=== Training Phase ===" << std::endl;

        // Select training images (balanced across letters)
        std::vector<size_t> trainingIndices;
        std::vector<int> trainCount(NUM_LETTERS, 0);
        int trainPerLetter = config.trainingExamplesPerLetter;

        for (size_t i = 0; i < trainLoader.size(); ++i) {
            int label = trainLoader.getImage(i).label - 1;  // EMNIST labels are 1-26, convert to 0-25
            if (label >= 0 && label < NUM_LETTERS && trainCount[label] < trainPerLetter) {
                trainingIndices.push_back(i);
                trainCount[label]++;
            }
        }

        std::cout << "  Selected " << trainingIndices.size() << " training images" << std::endl;
        std::cout << "  Using spike-based propagation with STDP learning" << std::endl;

        // Track position neuron firing statistics
        std::vector<int> positionNeuronFireCounts(config.positionFeedbackEnabled ? config.numFixations : 0, 0);

        auto trainStart = std::chrono::steady_clock::now();

        for (size_t idx = 0; idx < trainingIndices.size(); ++idx) {
            size_t i = trainingIndices[idx];
            const auto& emnistImg = trainLoader.getImage(i);
            int label = emnistImg.label - 1;  // EMNIST labels are 1-26, convert to 0-25
            char letterLabel = emnistImg.getCharLabel();  // Get 'A'-'Z'

            if (idx % 100 == 0) {
                std::cout << "  Processing training image " << idx << "/" << trainingIndices.size()
                          << " (label=" << letterLabel << ")" << std::endl;
            }

            // Clear all spike buffers
            for (const auto& neuron : allNeurons) {
                neuron->clearSpikes();
            }

            double currentTime = spikeProcessor->getCurrentTime();

            // Apply Gabor filters and fire Layer 4 neurons
            // Also fire Layer 5 neurons based on Layer 4 activity (hierarchical processing)
            std::vector<std::shared_ptr<Neuron>> layer5Neurons;

            // Determine number of fixations (1 if saccades disabled, or configured number)
            int numFixations = config.saccadesEnabled ? config.numFixations : 1;

            // Process each fixation sequentially
            for (int fixationIdx = 0; fixationIdx < numFixations; ++fixationIdx) {
                // Extract fixation region (or use full image if saccades disabled)
                std::vector<uint8_t> fixationPixels;
                if (config.saccadesEnabled && fixationIdx < static_cast<int>(config.fixationRegions.size())) {
                    fixationPixels = extractFixationRegion(emnistImg.pixels, config.fixationRegions[fixationIdx]);
                } else {
                    fixationPixels = emnistImg.pixels;  // Full image
                }

                // Time offset for this fixation
                double fixationTimeOffset = fixationIdx * config.fixationDurationMs;
                double fixationTime = currentTime + fixationTimeOffset;

                // Fire position encoding neurons for this fixation (if enabled)
                // Position neurons are connected directly to output layer
                // They fire at the start of each fixation to provide position context
                if (config.positionFeedbackEnabled && fixationIdx < static_cast<int>(positionNeurons.size())) {
                    double positionFireTime = fixationTime;  // Fire at fixation start

                    for (auto& posNeuron : positionNeurons[fixationIdx]) {
                        // Fire the position neuron through NetworkPropagator
                        posNeuron->fireSignature(positionFireTime);
                        posNeuron->fireAndAcknowledge(positionFireTime);
                        networkPropagator->fireNeuron(posNeuron->getId(), positionFireTime);
                        positionNeuronFireCounts[fixationIdx]++;
                    }
                }

                // First pass: Calculate column strengths for selective firing (PARALLELIZED)
                std::vector<double> columnStrengths(NUM_COLUMNS, 0.0);
                std::vector<std::vector<std::pair<size_t, double>>> columnActiveL4(NUM_COLUMNS);

                // Parallelize Gabor filter application across columns
                const int numThreads = std::min(24, NUM_COLUMNS);
                std::vector<std::future<void>> futures;

                auto processColumnBatch = [&](int startCol, int endCol) {
                    for (int colIdx = startCol; colIdx < endCol; ++colIdx) {
                        auto& col = corticalColumns[colIdx];
                        auto gaborResponse = applyGaborFilter(fixationPixels, col.gaborKernel, LAYER4_SIZE);

                        // Collect active Layer 4 neurons and calculate column strength
                        for (size_t neuronIdx = 0; neuronIdx < col.layer4Neurons.size() && neuronIdx < gaborResponse.size(); ++neuronIdx) {
                            if (gaborResponse[neuronIdx] > config.gaborThreshold) {
                                columnActiveL4[colIdx].push_back({neuronIdx, gaborResponse[neuronIdx]});
                                columnStrengths[colIdx] += gaborResponse[neuronIdx];
                            }
                        }
                    }
                };

                // Divide columns among threads
                int colsPerThread = (NUM_COLUMNS + numThreads - 1) / numThreads;
                for (int t = 0; t < numThreads; ++t) {
                    int startCol = t * colsPerThread;
                    int endCol = std::min(startCol + colsPerThread, NUM_COLUMNS);
                    if (startCol < NUM_COLUMNS) {
                        futures.push_back(std::async(std::launch::async, processColumnBatch, startCol, endCol));
                    }
                }

                // Wait for all threads to complete
                for (auto& f : futures) {
                    f.get();
                }

                // Calculate mean column strength for selective firing
                double meanStrength = 0.0;
                for (double s : columnStrengths) meanStrength += s;
                meanStrength /= NUM_COLUMNS;

                // Second pass: Fire neurons only from strong columns
                for (int colIdx = 0; colIdx < NUM_COLUMNS; ++colIdx) {
                    // Skip weak columns (selective firing for discrimination)
                    if (columnStrengths[colIdx] < meanStrength) {
                        // Still collect Layer 5 neurons for pattern copying (only on last fixation)
                        if (fixationIdx == numFixations - 1) {
                            layer5Neurons.insert(layer5Neurons.end(),
                                                corticalColumns[colIdx].layer5Neurons.begin(),
                                                corticalColumns[colIdx].layer5Neurons.end());
                        }
                        continue;
                    }

                    auto& col = corticalColumns[colIdx];
                    auto& activeL4 = columnActiveL4[colIdx];

                    // Fire Layer 4 neurons with fixation time offset
                    for (const auto& [neuronIdx, response] : activeL4) {
                        double firingTime = fixationTime + (1.0 - response) * 10.0;
                        col.layer4Neurons[neuronIdx]->fireSignature(firingTime);
                        col.layer4Neurons[neuronIdx]->fireAndAcknowledge(firingTime);
                        networkPropagator->fireNeuron(col.layer4Neurons[neuronIdx]->getId(), firingTime);
                    }

                    // Sort by response strength (highest first)
                    std::sort(activeL4.begin(), activeL4.end(),
                             [](const auto& a, const auto& b) { return a.second > b.second; });

                    // Fire MORE Layer 5 neurons (100% of active L4) for richer patterns
                    int numL5ToFire = std::min(static_cast<int>(col.layer5Neurons.size()),
                                              static_cast<int>(activeL4.size()));

                    for (int i = 0; i < numL5ToFire && i < static_cast<int>(activeL4.size()); ++i) {
                        size_t l4Idx = activeL4[i].first;
                        size_t l5Idx = l4Idx % col.layer5Neurons.size();

                        auto& l5Neuron = col.layer5Neurons[l5Idx];
                        // Even tighter temporal spacing for more precise patterns
                        // Add fixation time offset to create temporal sequences
                        double baseTime = fixationTime + 15.0 + (colIdx * 1.5) + (i * 0.2);
                        l5Neuron->fireSignature(baseTime);
                        l5Neuron->fireAndAcknowledge(baseTime);
                        networkPropagator->fireNeuron(l5Neuron->getId(), baseTime);
                        l5Neuron->learnCurrentPattern();
                    }

                    // Collect all Layer 5 neurons for pattern copying (only on last fixation)
                    if (fixationIdx == numFixations - 1) {
                        layer5Neurons.insert(layer5Neurons.end(), col.layer5Neurons.begin(), col.layer5Neurons.end());
                    }
                }
            }  // End of fixation loop

            // Supervised learning: teach output neuron for this letter
            int neuronIndex = idx % outputPopulations[label].size();
            auto& targetNeuron = outputPopulations[label][neuronIndex];

            // Copy Layer 5 spike pattern to target output neuron
            copyLayerSpikePattern(layer5Neurons, {targetNeuron});

            // Fire target neuron as teaching signal
            targetNeuron->fireAndAcknowledge(currentTime + 20.0);  // After propagation through 6 layers
            networkPropagator->fireNeuron(targetNeuron->getId(), currentTime + 20.0);

            // Apply STDP learning with stronger reward signal
            networkPropagator->applyRewardModulatedSTDP(targetNeuron->getId(), 2.5);

            // Learn the pattern
            targetNeuron->learnCurrentPattern();

            // Advance time (50ms per image for full propagation through 6 layers)
            currentTime += 50.0;
        }

        auto trainEnd = std::chrono::steady_clock::now();
        double trainTime = std::chrono::duration<double>(trainEnd - trainStart).count();

        std::cout << "✓ Training complete in " << std::fixed << std::setprecision(1)
                  << trainTime << "s" << std::endl;
        for (int l = 0; l < NUM_LETTERS; ++l) {
            char letter = 'A' + l;
            std::cout << "  Letter " << letter << ": " << trainCount[l] << " patterns" << std::endl;
        }

        // Report position neuron firing statistics
        if (config.positionFeedbackEnabled) {
            std::cout << "\n=== Position Encoding Statistics ===" << std::endl;
            for (int fixIdx = 0; fixIdx < config.numFixations; ++fixIdx) {
                std::cout << "  Fixation " << fixIdx << ": " << positionNeuronFireCounts[fixIdx]
                          << " neuron fires (" << (positionNeuronFireCounts[fixIdx] / config.positionNeuronsPerFixation)
                          << " images)" << std::endl;
            }

            // Sample synapse weights after training
            std::cout << "\n=== Position Synapse Weights After Training (Sample) ===" << std::endl;
            size_t synapseStartIdx = allSynapses.size() - positionSynapseCount;
            int sampleCount = 0;
            double totalWeight = 0.0;
            double minWeight = 1.0;
            double maxWeight = 0.0;
            for (size_t i = synapseStartIdx; i < synapseStartIdx + std::min(size_t(100), size_t(positionSynapseCount)); ++i) {
                double w = allSynapses[i]->getWeight();
                totalWeight += w;
                minWeight = std::min(minWeight, w);
                maxWeight = std::max(maxWeight, w);
                sampleCount++;
            }
            std::cout << "  Average weight of first 100 position synapses: " << (totalWeight / sampleCount) << std::endl;
            std::cout << "  Min weight: " << minWeight << ", Max weight: " << maxWeight << std::endl;
            std::cout << "  (Initial weight was 0.8)" << std::endl;
        }

        // ========================================================================
        // Load Test Data
        // ========================================================================
        std::cout << "\n=== Loading Test Data ===" << std::endl;

        EMNISTLoader testLoader(EMNISTLoader::Variant::LETTERS);
        if (!testLoader.load(config.testImagesPath, config.testLabelsPath)) {
            std::cerr << "Failed to load test data" << std::endl;
            return 1;
        }
        std::cout << "✓ Loaded " << testLoader.size() << " test images" << std::endl;

        // ========================================================================
        // Pre-compute Gabor Responses (CACHING OPTIMIZATION)
        // ========================================================================
        // Note: Caching disabled when saccades are enabled (would need 4D cache)
        bool useCaching = !config.saccadesEnabled;

        std::cout << "\n=== Pre-computing Gabor Responses ===" << std::endl;
        if (!useCaching) {
            std::cout << "  Caching disabled (saccades enabled)" << std::endl;
        }

        size_t numTestImages = std::min((size_t)config.testImages, testLoader.size());

        // Cache structure: gaborCache[imageIdx][columnIdx] = vector of responses
        std::vector<std::vector<std::vector<double>>> gaborCache(numTestImages);

        auto cacheStart = std::chrono::steady_clock::now();
        double cacheTime = 0.0;
        size_t cacheSize = 0;

        if (useCaching) {
            // Parallelize across images
            const int numCacheThreads = std::min(24, (int)numTestImages);
            std::vector<std::future<void>> cacheFutures;

            auto cacheImageBatch = [&](size_t startImg, size_t endImg) {
                for (size_t imgIdx = startImg; imgIdx < endImg; ++imgIdx) {
                    const auto& emnistImg = testLoader.getImage(imgIdx);
                    gaborCache[imgIdx].resize(NUM_COLUMNS);

                    for (int colIdx = 0; colIdx < NUM_COLUMNS; ++colIdx) {
                        auto& col = corticalColumns[colIdx];
                        gaborCache[imgIdx][colIdx] = applyGaborFilter(emnistImg.pixels, col.gaborKernel, LAYER4_SIZE);
                    }
                }
            };

            // Divide images among threads
            size_t imgsPerThread = (numTestImages + numCacheThreads - 1) / numCacheThreads;
            for (int t = 0; t < numCacheThreads; ++t) {
                size_t startImg = t * imgsPerThread;
                size_t endImg = std::min(startImg + imgsPerThread, numTestImages);
                if (startImg < numTestImages) {
                    cacheFutures.push_back(std::async(std::launch::async, cacheImageBatch, startImg, endImg));
                }
            }

            // Wait for all caching to complete
            for (auto& f : cacheFutures) {
                f.get();
            }

            auto cacheEnd = std::chrono::steady_clock::now();
            cacheTime = std::chrono::duration<double>(cacheEnd - cacheStart).count();

            // Calculate cache size
            cacheSize = numTestImages * NUM_COLUMNS * LAYER4_SIZE * LAYER4_SIZE * sizeof(double);
            std::cout << "✓ Pre-computed Gabor responses for " << numTestImages << " images" << std::endl;
            std::cout << "  Cache time: " << cacheTime << "s" << std::endl;
            std::cout << "  Cache size: " << (cacheSize / 1024 / 1024) << " MB" << std::endl;
        }

        // ========================================================================
        // Testing Phase
        // ========================================================================
        std::cout << "\n=== Testing Phase ===" << std::endl;
        std::cout << "  Using output layer population activations for classification" << std::endl;
        if (useCaching) {
            std::cout << "  Using cached Gabor responses (no re-computation)" << std::endl;
        } else {
            std::cout << "  Computing Gabor responses on-the-fly (saccades enabled)" << std::endl;
        }

        auto testStart = std::chrono::steady_clock::now();

        int correct = 0;
        std::vector<int> perLetterCorrect(NUM_LETTERS, 0);
        std::vector<int> perLetterTotal(NUM_LETTERS, 0);

        // Confusion matrix: confusionMatrix[true][predicted]
        std::vector<std::vector<int>> confusionMatrix(NUM_LETTERS, std::vector<int>(NUM_LETTERS, 0));

        for (size_t i = 0; i < numTestImages; ++i) {
            const auto& emnistImg = testLoader.getImage(i);
            int trueLabel = emnistImg.label - 1;  // EMNIST labels are 1-26, convert to 0-25

            if (i % 100 == 0) {
                std::cout << "  Testing image " << i << "/" << numTestImages << std::endl;
            }

            // Clear all spike buffers
            for (const auto& neuron : allNeurons) {
                neuron->clearSpikes();
            }

            double currentTime = spikeProcessor->getCurrentTime();

            // Apply Gabor filters and fire Layer 4 neurons (same as training)
            // Also fire Layer 5 neurons based on Layer 4 activity
            std::vector<std::shared_ptr<Neuron>> layer5Neurons;

            // Determine number of fixations (1 if saccades disabled, or configured number)
            int numFixations = config.saccadesEnabled ? config.numFixations : 1;

            // Process each fixation sequentially
            for (int fixationIdx = 0; fixationIdx < numFixations; ++fixationIdx) {
                // Extract fixation region (or use full image if saccades disabled)
                std::vector<uint8_t> fixationPixels;
                if (config.saccadesEnabled && fixationIdx < static_cast<int>(config.fixationRegions.size())) {
                    fixationPixels = extractFixationRegion(emnistImg.pixels, config.fixationRegions[fixationIdx]);
                } else {
                    fixationPixels = emnistImg.pixels;  // Full image
                }

                // Time offset for this fixation
                double fixationTimeOffset = fixationIdx * config.fixationDurationMs;
                double fixationTime = currentTime + fixationTimeOffset;

                // Fire position encoding neurons for this fixation (if enabled)
                // Position neurons are connected directly to output layer
                // They fire at the start of each fixation to provide position context
                if (config.positionFeedbackEnabled && fixationIdx < static_cast<int>(positionNeurons.size())) {
                    double positionFireTime = fixationTime;  // Fire at fixation start

                    for (auto& posNeuron : positionNeurons[fixationIdx]) {
                        // Fire the position neuron through NetworkPropagator
                        posNeuron->fireSignature(positionFireTime);
                        posNeuron->fireAndAcknowledge(positionFireTime);
                        networkPropagator->fireNeuron(posNeuron->getId(), positionFireTime);
                    }
                }

                // First pass: Calculate column strengths
                std::vector<double> columnStrengths(NUM_COLUMNS, 0.0);
                std::vector<std::vector<std::pair<size_t, double>>> columnActiveL4(NUM_COLUMNS);

                if (useCaching && fixationIdx == 0) {
                    // Use cached Gabor responses (only for first fixation, only when saccades disabled)
                    for (int colIdx = 0; colIdx < NUM_COLUMNS; ++colIdx) {
                        auto& col = corticalColumns[colIdx];
                        const auto& gaborResponse = gaborCache[i][colIdx];  // Cache lookup

                        for (size_t neuronIdx = 0; neuronIdx < col.layer4Neurons.size() && neuronIdx < gaborResponse.size(); ++neuronIdx) {
                            if (gaborResponse[neuronIdx] > config.gaborThreshold) {
                                columnActiveL4[colIdx].push_back({neuronIdx, gaborResponse[neuronIdx]});
                                columnStrengths[colIdx] += gaborResponse[neuronIdx];
                            }
                        }
                    }
                } else {
                    // Compute Gabor responses on-the-fly (for saccades or when caching disabled)
                    const int numThreads = std::min(24, NUM_COLUMNS);
                    std::vector<std::future<void>> futures;

                    auto processColumnBatch = [&](int startCol, int endCol) {
                        for (int colIdx = startCol; colIdx < endCol; ++colIdx) {
                            auto& col = corticalColumns[colIdx];
                            auto gaborResponse = applyGaborFilter(fixationPixels, col.gaborKernel, LAYER4_SIZE);

                            for (size_t neuronIdx = 0; neuronIdx < col.layer4Neurons.size() && neuronIdx < gaborResponse.size(); ++neuronIdx) {
                                if (gaborResponse[neuronIdx] > config.gaborThreshold) {
                                    columnActiveL4[colIdx].push_back({neuronIdx, gaborResponse[neuronIdx]});
                                    columnStrengths[colIdx] += gaborResponse[neuronIdx];
                                }
                            }
                        }
                    };

                    int colsPerThread = (NUM_COLUMNS + numThreads - 1) / numThreads;
                    for (int t = 0; t < numThreads; ++t) {
                        int startCol = t * colsPerThread;
                        int endCol = std::min(startCol + colsPerThread, NUM_COLUMNS);
                        if (startCol < NUM_COLUMNS) {
                            futures.push_back(std::async(std::launch::async, processColumnBatch, startCol, endCol));
                        }
                    }

                    for (auto& f : futures) {
                        f.get();
                    }
                }

                // Calculate mean column strength
                double meanStrength = 0.0;
                for (double s : columnStrengths) meanStrength += s;
                meanStrength /= NUM_COLUMNS;

                // Second pass: Fire neurons only from strong columns (same as training)
                for (int colIdx = 0; colIdx < NUM_COLUMNS; ++colIdx) {
                    if (columnStrengths[colIdx] < meanStrength) {
                        // Still collect Layer 5 neurons for pattern copying (only on last fixation)
                        if (fixationIdx == numFixations - 1) {
                            layer5Neurons.insert(layer5Neurons.end(),
                                                corticalColumns[colIdx].layer5Neurons.begin(),
                                                corticalColumns[colIdx].layer5Neurons.end());
                        }
                        continue;
                    }

                    auto& col = corticalColumns[colIdx];
                    auto& activeL4 = columnActiveL4[colIdx];

                    // Fire Layer 4 neurons with fixation time offset
                    for (const auto& [neuronIdx, response] : activeL4) {
                        double firingTime = fixationTime + (1.0 - response) * 10.0;
                        col.layer4Neurons[neuronIdx]->fireSignature(firingTime);
                        col.layer4Neurons[neuronIdx]->fireAndAcknowledge(firingTime);
                        networkPropagator->fireNeuron(col.layer4Neurons[neuronIdx]->getId(), firingTime);
                    }

                    // Sort by response strength
                    std::sort(activeL4.begin(), activeL4.end(),
                             [](const auto& a, const auto& b) { return a.second > b.second; });

                    // Fire Layer 5 neurons (same as training: 100%, tighter timing)
                    int numL5ToFire = std::min(static_cast<int>(col.layer5Neurons.size()),
                                              static_cast<int>(activeL4.size()));

                    for (int i = 0; i < numL5ToFire && i < static_cast<int>(activeL4.size()); ++i) {
                        size_t l4Idx = activeL4[i].first;
                        size_t l5Idx = l4Idx % col.layer5Neurons.size();

                        auto& l5Neuron = col.layer5Neurons[l5Idx];
                        // Add fixation time offset to create temporal sequences
                        double baseTime = fixationTime + 15.0 + (colIdx * 1.5) + (i * 0.2);
                        l5Neuron->fireSignature(baseTime);
                        l5Neuron->fireAndAcknowledge(baseTime);
                        networkPropagator->fireNeuron(l5Neuron->getId(), baseTime);
                    }

                    // Collect all Layer 5 neurons for pattern copying (only on last fixation)
                    if (fixationIdx == numFixations - 1) {
                        layer5Neurons.insert(layer5Neurons.end(), col.layer5Neurons.begin(), col.layer5Neurons.end());
                    }
                }
            }  // End of fixation loop

            // Copy Layer 5 spike pattern to ALL output populations for pattern matching
            for (int letter = 0; letter < NUM_LETTERS; ++letter) {
                copyLayerSpikePattern(layer5Neurons, outputPopulations[letter]);
            }

            // Get population activations (average across each population)
            std::vector<double> populationActivations(NUM_LETTERS, 0.0);
            for (int letter = 0; letter < NUM_LETTERS; ++letter) {
                double totalActivation = 0.0;
                for (const auto& neuron : outputPopulations[letter]) {
                    totalActivation += neuron->getActivation();
                }
                populationActivations[letter] = totalActivation / outputPopulations[letter].size();
            }

            // Classify based on highest population activation
            int predicted = 0;
            double maxActivation = populationActivations[0];
            for (int l = 1; l < NUM_LETTERS; ++l) {
                if (populationActivations[l] > maxActivation) {
                    maxActivation = populationActivations[l];
                    predicted = l;
                }
            }

            // Update statistics
            perLetterTotal[trueLabel]++;
            confusionMatrix[trueLabel][predicted]++;
            if (predicted == trueLabel) {
                correct++;
                perLetterCorrect[trueLabel]++;
            }

            // Advance time
            currentTime += 50.0;
        }

        auto testEnd = std::chrono::steady_clock::now();
        double testTime = std::chrono::duration<double>(testEnd - testStart).count();

        // ========================================================================
        // Results
        // ========================================================================
        std::cout << "\n=== Results ===" << std::endl;
        std::cout << "  Cache time: " << std::fixed << std::setprecision(1) << cacheTime << "s (one-time pre-computation)" << std::endl;
        std::cout << "  Test time: " << std::fixed << std::setprecision(1) << testTime << "s (classification only)" << std::endl;
        std::cout << "  Total test time: " << std::fixed << std::setprecision(1) << (cacheTime + testTime) << "s" << std::endl;
        std::cout << "  Overall accuracy: " << std::fixed << std::setprecision(2)
                  << (100.0 * correct / numTestImages) << "% (" << correct << "/" << numTestImages << ")" << std::endl;

        std::cout << "\n  Per-letter accuracy:" << std::endl;
        for (int l = 0; l < NUM_LETTERS; ++l) {
            if (perLetterTotal[l] > 0) {
                char letter = 'A' + l;
                double acc = 100.0 * perLetterCorrect[l] / perLetterTotal[l];
                std::cout << "    Letter " << letter << ": " << std::fixed << std::setprecision(1)
                          << acc << "% (" << perLetterCorrect[l] << "/" << perLetterTotal[l] << ")" << std::endl;
            }
        }

        // Print confusion matrix (26x26 is too large, so we'll skip the full matrix display)
        std::cout << "\n=== Confusion Matrix ===" << std::endl;
        std::cout << "(26×26 matrix - showing top confusions only)\n" << std::endl;

        // Analyze top confusions (excluding correct predictions)
        std::cout << "=== Top Confusions (True → Predicted) ===" << std::endl;
        std::vector<std::tuple<int, int, int, double>> confusions;  // (true, pred, count, percentage)

        for (int t = 0; t < NUM_LETTERS; ++t) {
            for (int p = 0; p < NUM_LETTERS; ++p) {
                if (t != p && confusionMatrix[t][p] > 0) {
                    double percentage = 100.0 * confusionMatrix[t][p] / perLetterTotal[t];
                    confusions.push_back({t, p, confusionMatrix[t][p], percentage});
                }
            }
        }

        // Sort by count (descending)
        std::sort(confusions.begin(), confusions.end(),
                 [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

        // Print top 30 confusions (more than MNIST since we have 26 classes)
        std::cout << "Rank  True→Pred  Count  % of True" << std::endl;
        for (size_t i = 0; i < std::min(size_t(30), confusions.size()); ++i) {
            auto [t, p, count, pct] = confusions[i];
            char trueLetter = 'A' + t;
            char predLetter = 'A' + p;
            std::cout << std::setw(4) << (i+1) << "  "
                      << std::setw(4) << trueLetter << "→" << std::setw(4) << predLetter << "  "
                      << std::setw(5) << count << "  "
                      << std::fixed << std::setprecision(1) << std::setw(6) << pct << "%"
                      << std::endl;
        }

        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

