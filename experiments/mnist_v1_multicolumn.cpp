/**
 * @file mnist_v1_multicolumn.cpp
 * @brief MNIST classification using multi-column hierarchical V1 architecture
 * 
 * Architecture:
 * - 10 cortical columns (orientation-selective)
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
 * Each column processes a different orientation (0°, 18°, 36°, ..., 162°)
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
#include "snnfw/MNISTLoader.h"

using namespace snnfw;

// Configuration structure
struct MultiColumnConfig {
    double neuronWindow;
    double neuronThreshold;
    int neuronMaxPatterns;
    int trainingExamplesPerDigit;
    int testImages;
    std::string trainImagesPath;
    std::string trainLabelsPath;
    std::string testImagesPath;
    std::string testLabelsPath;

    static MultiColumnConfig fromConfigLoader(const ConfigLoader& loader) {
        MultiColumnConfig config;
        config.neuronWindow = loader.get<double>("/neuron/window_size_ms", 200.0);
        config.neuronThreshold = loader.get<double>("/neuron/similarity_threshold", 0.90);
        config.neuronMaxPatterns = loader.get<int>("/neuron/max_patterns", 100);
        config.trainingExamplesPerDigit = loader.get<int>("/training/examples_per_digit", 500);
        config.testImages = loader.get<int>("/training/test_images", 1000);
        config.trainImagesPath = loader.getRequired<std::string>("/data/train_images");
        config.trainLabelsPath = loader.getRequired<std::string>("/data/train_labels");
        config.testImagesPath = loader.getRequired<std::string>("/data/test_images");
        config.testLabelsPath = loader.getRequired<std::string>("/data/test_labels");
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
 * Detects closed loops in the upper portion of the image
 * @param size Kernel size (default 9x9)
 * @return Kernel that responds strongly to closed loops in top region
 */
std::vector<std::vector<double>> createTopLoopKernel(int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;

    // Create a ring pattern in the top half
    // Strong positive response for circular patterns in upper region
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - (center - 1);  // Shift center up slightly
            double dist = sqrt(dx*dx + dy*dy);

            // Ring pattern: positive at radius ~2-3, negative inside and outside
            if (y < center + 1) {  // Focus on top half
                if (dist >= 1.5 && dist <= 3.0) {
                    kernel[y][x] = 1.0;  // Ring
                } else if (dist < 1.5) {
                    kernel[y][x] = -0.5;  // Inside (hole)
                } else if (dist > 3.0 && dist < 4.0) {
                    kernel[y][x] = -0.3;  // Outside
                }
            }
        }
    }

    return kernel;
}

/**
 * @brief Create a gap detector for open regions (for detecting digit 4's open top)
 * @param size Kernel size (default 9x9)
 * @return Kernel that responds to gaps/openings in top region
 */
std::vector<std::vector<double>> createGapKernel(int size = 9) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));
    int center = size / 2;

    // Detect vertical gaps in top region
    // Positive response for vertical edges with gap in middle
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (y < center) {  // Top half only
                if (x < center - 1) {
                    kernel[y][x] = 0.5;  // Left edge
                } else if (x > center + 1) {
                    kernel[y][x] = 0.5;  // Right edge
                } else {
                    kernel[y][x] = -1.0;  // Gap in middle
                }
            }
        }
    }

    return kernel;
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
        std::cout << "  Training examples per digit: " << config.trainingExamplesPerDigit << std::endl;
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
        
        // Create multi-modal cortical columns:
        // - 12 orientations × 2 spatial frequencies (low + high) = 24 columns
        // - 4 center-surround scales × 2 types (ON-center + OFF-center) = 8 columns
        // - 4 blob detectors (4 scales × 2 polarities) = 8 columns
        // - 4 specialized detectors (2 top-loop + 2 gap detectors) = 4 columns
        // Total: 44 columns (optimized for 4→9 and 7→9 distinction)
        const int NUM_ORIENTATIONS = 12;
        const int NUM_FREQUENCIES = 2;  // Low + High (skip medium to make room for center-surround)
        const int NUM_CS_SCALES = 4;    // Center-surround scales (added extra for loop counting)
        const int NUM_CS_TYPES = 2;     // ON-center (bright blob) + OFF-center (dark blob)
        const int NUM_BLOB_SCALES = 4;  // Blob detector scales (added extra for better localization)
        const int NUM_BLOB_TYPES = 2;   // Positive + Negative polarity
        const int NUM_SPECIALIZED = 4;  // Top-loop and gap detectors (for 4→9, 7→9)

        const int NUM_ORIENTATION_COLUMNS = NUM_ORIENTATIONS * NUM_FREQUENCIES;
        const int NUM_CS_COLUMNS = NUM_CS_SCALES * NUM_CS_TYPES;
        const int NUM_BLOB_COLUMNS = NUM_BLOB_SCALES * NUM_BLOB_TYPES;
        const int NUM_COLUMNS = NUM_ORIENTATION_COLUMNS + NUM_CS_COLUMNS + NUM_BLOB_COLUMNS + NUM_SPECIALIZED;
        const double ORIENTATION_STEP = 180.0 / NUM_ORIENTATIONS;

        // Spatial frequency channels (lambda values)
        const double FREQ_LOW = 8.0;     // Low frequency: thick strokes, overall shape
        const double FREQ_HIGH = 3.0;    // High frequency: fine details, thin strokes

        const std::vector<double> SPATIAL_FREQUENCIES = {FREQ_LOW, FREQ_HIGH};
        const std::vector<std::string> FREQ_NAMES = {"low_freq", "high_freq"};

        // Center-surround parameters (for loop/hole detection)
        // MNIST digits are 28x28, so we need to tune for digit-scale features
        // After max-pooling to 8x8, features are ~3.5x smaller
        // Adjusted for better loop detection and counting:
        const std::vector<std::pair<double, double>> CS_PARAMS = {
            {0.7, 2.0},  // Extra small: very tight curves (digit 6, 9 small loops)
            {1.2, 3.5},  // Small: digit 8 individual loops, digit 4 open top
            {2.0, 5.0},  // Medium: digit 0 loop, digit 8 combined loops
            {3.0, 7.0}   // Large: whole digit scale (distinguish 0 vs 8)
        };
        const std::vector<std::string> CS_SCALE_NAMES = {"extra_small", "small", "medium", "large"};
        const std::vector<std::string> CS_TYPE_NAMES = {"ON_center", "OFF_center"};

        // Blob detector parameters (for endpoint/solid region detection)
        // Tuned to detect stroke endpoints, junctions, and solid regions at multiple scales
        const std::vector<double> BLOB_SIGMAS = {0.8, 1.5, 2.5, 4.0};  // 4 scales for better feature coverage
        const std::vector<std::string> BLOB_SCALE_NAMES = {"tiny_blob", "small_blob", "med_blob", "large_blob"};

        // Neuron counts per layer (OPTIMAL CONFIGURATION)
        // After systematic testing, L2/3 = 256 achieves best accuracy (70.63%)
        const int LAYER1_NEURONS = 32;    // Modulatory
        const int LAYER23_NEURONS = 256;  // Superficial pyramidal (integration) - OPTIMAL: 256
        const int LAYER4_SIZE = 8;        // Grid size for Layer 4 (8x8 = 64 neurons)
        const int LAYER5_NEURONS = 64;    // Deep pyramidal (output)
        const int LAYER6_NEURONS = 32;    // Corticothalamic feedback

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

        std::cout << "\n✓ Created " << NUM_COLUMNS << " cortical columns:" << std::endl;
        std::cout << "  - " << NUM_ORIENTATION_COLUMNS << " orientation columns" << std::endl;
        std::cout << "  - " << NUM_CS_COLUMNS << " center-surround columns" << std::endl;
        std::cout << "  - " << NUM_BLOB_COLUMNS << " blob detector columns" << std::endl;
        std::cout << "  - " << NUM_SPECIALIZED << " specialized detector columns (top-loop + gap)" << std::endl;

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
        const double LATERAL_CONNECTIVITY = 0.20;  // 20% sparse connectivity (increased for better integration)
        const int NEIGHBOR_RANGE = 2;  // Connect to ±2 neighboring columns

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
        const double RECURRENT_CONNECTIVITY = 0.15;  // 15% sparse recurrent connectivity
        const double RECURRENT_WEIGHT = 0.4;  // Moderate weight for temporal integration
        const double RECURRENT_DELAY = 2.0;  // 2ms delay for recurrent feedback

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
        // Load MNIST Data and Test Architecture
        // ========================================================================
        std::cout << "\n=== Loading MNIST Data ===" << std::endl;

        MNISTLoader trainLoader;
        if (!trainLoader.load(config.trainImagesPath, config.trainLabelsPath)) {
            std::cerr << "Failed to load training data" << std::endl;
            return 1;
        }
        std::cout << "✓ Loaded " << trainLoader.size() << " training images" << std::endl;

        // ========================================================================
        // Initialize SpikeProcessor and NetworkPropagator
        // ========================================================================
        std::cout << "\n=== Initializing Spike Processing System ===" << std::endl;

        auto spikeProcessor = std::make_shared<SpikeProcessor>(10000, 20);
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
                    if (gaborResponse[neuronIdx] > 0.1) {  // Threshold
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

        const int NEURONS_PER_DIGIT = 20;  // Increased from 10 for better representation
        std::vector<std::vector<std::shared_ptr<Neuron>>> outputPopulations;

        for (int digit = 0; digit < 10; ++digit) {
            auto digitCluster = factory.createCluster();
            outputLayer->addCluster(digitCluster->getId());

            std::vector<std::shared_ptr<Neuron>> population;
            for (int i = 0; i < NEURONS_PER_DIGIT; ++i) {
                auto neuron = factory.createNeuron(neuronWindow, neuronThreshold, neuronMaxPatterns);
                neuron->setSimilarityMetric(SimilarityMetric::HISTOGRAM);
                population.push_back(neuron);
                digitCluster->addNeuron(neuron->getId());
                allNeurons.push_back(neuron);
            }
            outputPopulations.push_back(population);
        }

        std::cout << "✓ Created output layer: " << (10 * NEURONS_PER_DIGIT) << " neurons ("
                  << NEURONS_PER_DIGIT << " per digit)" << std::endl;

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
        // Training Phase
        // ========================================================================
        std::cout << "\n=== Training Phase ===" << std::endl;

        // Select training images (balanced across digits)
        std::vector<size_t> trainingIndices;
        std::vector<int> trainCount(10, 0);
        int trainPerDigit = config.trainingExamplesPerDigit;

        for (size_t i = 0; i < trainLoader.size(); ++i) {
            int label = trainLoader.getImage(i).label;
            if (trainCount[label] < trainPerDigit) {
                trainingIndices.push_back(i);
                trainCount[label]++;
            }
        }

        std::cout << "  Selected " << trainingIndices.size() << " training images" << std::endl;
        std::cout << "  Using spike-based propagation with STDP learning" << std::endl;

        auto trainStart = std::chrono::steady_clock::now();

        for (size_t idx = 0; idx < trainingIndices.size(); ++idx) {
            size_t i = trainingIndices[idx];
            const auto& mnistImg = trainLoader.getImage(i);
            int label = mnistImg.label;

            if (idx % 100 == 0) {
                std::cout << "  Processing training image " << idx << "/" << trainingIndices.size()
                          << " (label=" << label << ")" << std::endl;
            }

            // Clear all spike buffers
            for (const auto& neuron : allNeurons) {
                neuron->clearSpikes();
            }

            double currentTime = spikeProcessor->getCurrentTime();

            // Apply Gabor filters and fire Layer 4 neurons
            // Also fire Layer 5 neurons based on Layer 4 activity (hierarchical processing)
            std::vector<std::shared_ptr<Neuron>> layer5Neurons;

            // First pass: Calculate column strengths for selective firing (PARALLELIZED)
            std::vector<double> columnStrengths(NUM_COLUMNS, 0.0);
            std::vector<std::vector<std::pair<size_t, double>>> columnActiveL4(NUM_COLUMNS);

            // Parallelize Gabor filter application across columns
            const int numThreads = std::min(24, NUM_COLUMNS);
            std::vector<std::future<void>> futures;

            auto processColumnBatch = [&](int startCol, int endCol) {
                for (int colIdx = startCol; colIdx < endCol; ++colIdx) {
                    auto& col = corticalColumns[colIdx];
                    auto gaborResponse = applyGaborFilter(mnistImg.pixels, col.gaborKernel, LAYER4_SIZE);

                    // Collect active Layer 4 neurons and calculate column strength
                    for (size_t neuronIdx = 0; neuronIdx < col.layer4Neurons.size() && neuronIdx < gaborResponse.size(); ++neuronIdx) {
                        if (gaborResponse[neuronIdx] > 0.1) {
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
                    // Still collect Layer 5 neurons for pattern copying
                    layer5Neurons.insert(layer5Neurons.end(),
                                        corticalColumns[colIdx].layer5Neurons.begin(),
                                        corticalColumns[colIdx].layer5Neurons.end());
                    continue;
                }

                auto& col = corticalColumns[colIdx];
                auto& activeL4 = columnActiveL4[colIdx];

                // Fire Layer 4 neurons
                for (const auto& [neuronIdx, response] : activeL4) {
                    double firingTime = currentTime + (1.0 - response) * 10.0;
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
                    double baseTime = currentTime + 15.0 + (colIdx * 1.5) + (i * 0.2);
                    l5Neuron->fireSignature(baseTime);
                    l5Neuron->fireAndAcknowledge(baseTime);
                    networkPropagator->fireNeuron(l5Neuron->getId(), baseTime);
                    l5Neuron->learnCurrentPattern();
                }

                // Collect all Layer 5 neurons for pattern copying
                layer5Neurons.insert(layer5Neurons.end(), col.layer5Neurons.begin(), col.layer5Neurons.end());
            }

            // Supervised learning: teach output neuron for this digit
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
        for (int d = 0; d < 10; ++d) {
            std::cout << "  Digit " << d << ": " << trainCount[d] << " patterns" << std::endl;
        }

        // ========================================================================
        // Load Test Data
        // ========================================================================
        std::cout << "\n=== Loading Test Data ===" << std::endl;

        MNISTLoader testLoader;
        if (!testLoader.load(config.testImagesPath, config.testLabelsPath)) {
            std::cerr << "Failed to load test data" << std::endl;
            return 1;
        }
        std::cout << "✓ Loaded " << testLoader.size() << " test images" << std::endl;

        // ========================================================================
        // Testing Phase
        // ========================================================================
        std::cout << "\n=== Testing Phase ===" << std::endl;
        std::cout << "  Using output layer population activations for classification" << std::endl;

        auto testStart = std::chrono::steady_clock::now();

        int correct = 0;
        std::vector<int> perDigitCorrect(10, 0);
        std::vector<int> perDigitTotal(10, 0);

        // Confusion matrix: confusionMatrix[true][predicted]
        std::vector<std::vector<int>> confusionMatrix(10, std::vector<int>(10, 0));

        size_t numTestImages = std::min((size_t)config.testImages, testLoader.size());

        for (size_t i = 0; i < numTestImages; ++i) {
            const auto& mnistImg = testLoader.getImage(i);
            int trueLabel = mnistImg.label;

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

            // First pass: Calculate column strengths (PARALLELIZED)
            std::vector<double> columnStrengths(NUM_COLUMNS, 0.0);
            std::vector<std::vector<std::pair<size_t, double>>> columnActiveL4(NUM_COLUMNS);

            // Parallelize Gabor filter application across columns
            const int numThreads = std::min(24, NUM_COLUMNS);
            std::vector<std::future<void>> futures;

            auto processColumnBatch = [&](int startCol, int endCol) {
                for (int colIdx = startCol; colIdx < endCol; ++colIdx) {
                    auto& col = corticalColumns[colIdx];
                    auto gaborResponse = applyGaborFilter(mnistImg.pixels, col.gaborKernel, LAYER4_SIZE);

                    for (size_t neuronIdx = 0; neuronIdx < col.layer4Neurons.size() && neuronIdx < gaborResponse.size(); ++neuronIdx) {
                        if (gaborResponse[neuronIdx] > 0.1) {
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

            // Calculate mean column strength
            double meanStrength = 0.0;
            for (double s : columnStrengths) meanStrength += s;
            meanStrength /= NUM_COLUMNS;

            // Second pass: Fire neurons only from strong columns (same as training)
            for (int colIdx = 0; colIdx < NUM_COLUMNS; ++colIdx) {
                if (columnStrengths[colIdx] < meanStrength) {
                    layer5Neurons.insert(layer5Neurons.end(),
                                        corticalColumns[colIdx].layer5Neurons.begin(),
                                        corticalColumns[colIdx].layer5Neurons.end());
                    continue;
                }

                auto& col = corticalColumns[colIdx];
                auto& activeL4 = columnActiveL4[colIdx];

                // Fire Layer 4 neurons
                for (const auto& [neuronIdx, response] : activeL4) {
                    double firingTime = currentTime + (1.0 - response) * 10.0;
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
                    double baseTime = currentTime + 15.0 + (colIdx * 1.5) + (i * 0.2);
                    l5Neuron->fireSignature(baseTime);
                    l5Neuron->fireAndAcknowledge(baseTime);
                    networkPropagator->fireNeuron(l5Neuron->getId(), baseTime);
                }

                layer5Neurons.insert(layer5Neurons.end(), col.layer5Neurons.begin(), col.layer5Neurons.end());
            }

            // Copy Layer 5 spike pattern to ALL output populations for pattern matching
            for (int digit = 0; digit < 10; ++digit) {
                copyLayerSpikePattern(layer5Neurons, outputPopulations[digit]);
            }

            // Get population activations (average across each population)
            std::vector<double> populationActivations(10, 0.0);
            for (int digit = 0; digit < 10; ++digit) {
                double totalActivation = 0.0;
                for (const auto& neuron : outputPopulations[digit]) {
                    totalActivation += neuron->getActivation();
                }
                populationActivations[digit] = totalActivation / outputPopulations[digit].size();
            }

            // Classify based on highest population activation
            int predicted = 0;
            double maxActivation = populationActivations[0];
            for (int d = 1; d < 10; ++d) {
                if (populationActivations[d] > maxActivation) {
                    maxActivation = populationActivations[d];
                    predicted = d;
                }
            }

            // Update statistics
            perDigitTotal[trueLabel]++;
            confusionMatrix[trueLabel][predicted]++;
            if (predicted == trueLabel) {
                correct++;
                perDigitCorrect[trueLabel]++;
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
        std::cout << "  Test time: " << std::fixed << std::setprecision(1) << testTime << "s" << std::endl;
        std::cout << "  Overall accuracy: " << std::fixed << std::setprecision(2)
                  << (100.0 * correct / numTestImages) << "% (" << correct << "/" << numTestImages << ")" << std::endl;

        std::cout << "\n  Per-digit accuracy:" << std::endl;
        for (int d = 0; d < 10; ++d) {
            if (perDigitTotal[d] > 0) {
                double acc = 100.0 * perDigitCorrect[d] / perDigitTotal[d];
                std::cout << "    Digit " << d << ": " << std::fixed << std::setprecision(1)
                          << acc << "% (" << perDigitCorrect[d] << "/" << perDigitTotal[d] << ")" << std::endl;
            }
        }

        // Print confusion matrix
        std::cout << "\n=== Confusion Matrix ===" << std::endl;
        std::cout << "Rows = True Label, Columns = Predicted Label\n" << std::endl;

        // Header
        std::cout << "True\\Pred";
        for (int p = 0; p < 10; ++p) {
            std::cout << std::setw(6) << p;
        }
        std::cout << std::endl;

        // Matrix rows
        for (int t = 0; t < 10; ++t) {
            std::cout << std::setw(9) << t;
            for (int p = 0; p < 10; ++p) {
                std::cout << std::setw(6) << confusionMatrix[t][p];
            }
            std::cout << std::endl;
        }

        // Analyze top confusions (excluding correct predictions)
        std::cout << "\n=== Top Confusions (True → Predicted) ===" << std::endl;
        std::vector<std::tuple<int, int, int, double>> confusions;  // (true, pred, count, percentage)

        for (int t = 0; t < 10; ++t) {
            for (int p = 0; p < 10; ++p) {
                if (t != p && confusionMatrix[t][p] > 0) {
                    double percentage = 100.0 * confusionMatrix[t][p] / perDigitTotal[t];
                    confusions.push_back({t, p, confusionMatrix[t][p], percentage});
                }
            }
        }

        // Sort by count (descending)
        std::sort(confusions.begin(), confusions.end(),
                 [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

        // Print top 20 confusions
        std::cout << "Rank  True→Pred  Count  % of True" << std::endl;
        for (size_t i = 0; i < std::min(size_t(20), confusions.size()); ++i) {
            auto [t, p, count, pct] = confusions[i];
            std::cout << std::setw(4) << (i+1) << "  "
                      << std::setw(4) << t << "→" << std::setw(4) << p << "  "
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

