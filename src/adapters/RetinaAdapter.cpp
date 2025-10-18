#include "snnfw/adapters/RetinaAdapter.h"
#include "snnfw/features/EdgeOperator.h"
#include "snnfw/features/SobelOperator.h"
#include "snnfw/features/GaborOperator.h"
#include "snnfw/features/DoGOperator.h"
#include "snnfw/encoding/EncodingStrategy.h"
#include "snnfw/encoding/RateEncoder.h"
#include "snnfw/encoding/TemporalEncoder.h"
#include "snnfw/encoding/PopulationEncoder.h"
#include "snnfw/Logger.h"
#include <algorithm>
#include <cmath>

namespace snnfw {
namespace adapters {

RetinaAdapter::RetinaAdapter(const Config& config)
    : SensoryAdapter(config)
    , gridSize_(0)
    , regionSize_(0)
    , numOrientations_(0)
    , edgeThreshold_(0.15)
    , temporalWindow_(100.0)
    , neuronWindowSize_(200.0)
    , neuronThreshold_(0.7)
    , neuronMaxPatterns_(100)
    , imageRows_(0)
    , imageCols_(0)
{
    // Load configuration parameters
    gridSize_ = getIntParam("grid_size", 7);
    numOrientations_ = getIntParam("num_orientations", 8);
    edgeThreshold_ = getDoubleParam("edge_threshold", 0.15);
    temporalWindow_ = config.temporalWindow > 0 ? config.temporalWindow : 100.0;

    neuronWindowSize_ = getDoubleParam("neuron_window_size", 200.0);
    neuronThreshold_ = getDoubleParam("neuron_threshold", 0.7);
    neuronMaxPatterns_ = getIntParam("neuron_max_patterns", 100);

    // Create edge operator
    std::string edgeOperatorType = getStringParam("edge_operator", "sobel");
    features::EdgeOperator::Config edgeConfig;
    edgeConfig.name = edgeOperatorType;
    edgeConfig.numOrientations = numOrientations_;
    edgeConfig.edgeThreshold = edgeThreshold_;

    // Copy edge operator parameters from config
    if (config.stringParams.count("edge_operator_params") > 0) {
        // Parse nested params if needed - for now use direct params
    }
    edgeConfig.doubleParams["wavelength"] = getDoubleParam("wavelength", 4.0);
    edgeConfig.doubleParams["sigma"] = getDoubleParam("sigma", 2.0);
    edgeConfig.doubleParams["gamma"] = getDoubleParam("gamma", 0.5);
    edgeConfig.doubleParams["phase_offset"] = getDoubleParam("phase_offset", 0.0);
    edgeConfig.doubleParams["sigma1"] = getDoubleParam("sigma1", 1.0);
    edgeConfig.doubleParams["sigma2"] = getDoubleParam("sigma2", 1.6);
    edgeConfig.intParams["kernel_size"] = getIntParam("kernel_size", 5);

    edgeOperator_ = features::EdgeOperatorFactory::create(edgeOperatorType, edgeConfig);

    // Create encoding strategy
    std::string encodingType = getStringParam("encoding_strategy", "rate");
    encoding::EncodingStrategy::Config encodingConfig;
    encodingConfig.name = encodingType;
    encodingConfig.temporalWindow = temporalWindow_;
    encodingConfig.baselineTime = 0.0;
    encodingConfig.intensityScale = temporalWindow_;

    // Copy encoding parameters
    encodingConfig.doubleParams["timing_jitter"] = getDoubleParam("timing_jitter", 0.0);
    encodingConfig.doubleParams["min_spike_interval"] = getDoubleParam("min_spike_interval", 5.0);
    encodingConfig.doubleParams["tuning_width"] = getDoubleParam("tuning_width", 0.3);
    encodingConfig.doubleParams["min_response"] = getDoubleParam("min_response", 0.1);
    encodingConfig.intParams["dual_spike_mode"] = getIntParam("dual_spike_mode", 0);
    encodingConfig.intParams["population_size"] = getIntParam("population_size", 5);

    encodingStrategy_ = encoding::EncodingStrategyFactory::create(encodingType, encodingConfig);

    SNNFW_INFO("RetinaAdapter '{}': grid={}x{}, orientations={}, threshold={}, edge={}, encoding={}",
               getName(), gridSize_, gridSize_, numOrientations_, edgeThreshold_,
               edgeOperatorType, encodingType);
}

bool RetinaAdapter::initialize() {
    if (!SensoryAdapter::initialize()) {
        return false;
    }
    
    createNeurons();
    
    SNNFW_INFO("RetinaAdapter '{}': initialized with {} neurons", 
               getName(), neurons_.size());
    return true;
}

void RetinaAdapter::createNeurons() {
    // Clear existing neurons completely before creating new ones
    neurons_.clear();
    neuronGrid_.clear();

    int numRegions = gridSize_ * gridSize_;
    neuronGrid_.resize(numRegions);

    int neuronId = 0;
    for (int region = 0; region < numRegions; ++region) {
        neuronGrid_[region].resize(numOrientations_);
        for (int orient = 0; orient < numOrientations_; ++orient) {
            auto neuron = std::make_shared<Neuron>(
                neuronWindowSize_,
                neuronThreshold_,
                neuronMaxPatterns_,
                neuronId++
            );
            neuronGrid_[region][orient] = neuron;
            neurons_.push_back(neuron);
        }
    }
}

std::vector<uint8_t> RetinaAdapter::extractRegion(const Image& image, 
                                                   int regionRow, 
                                                   int regionCol) const {
    std::vector<uint8_t> region(regionSize_ * regionSize_);
    
    int startRow = regionRow * regionSize_;
    int startCol = regionCol * regionSize_;
    
    for (int r = 0; r < regionSize_; ++r) {
        for (int c = 0; c < regionSize_; ++c) {
            int imgRow = startRow + r;
            int imgCol = startCol + c;
            region[r * regionSize_ + c] = image.getPixel(imgRow, imgCol);
        }
    }
    
    return region;
}

std::vector<double> RetinaAdapter::extractEdgeFeatures(const std::vector<uint8_t>& region,
                                                        int regionSize) const {
    // Use pluggable edge operator
    return edgeOperator_->extractEdges(region, regionSize);
}

std::vector<double> RetinaAdapter::featuresToSpikes(const std::vector<double>& features) const {
    return featuresToSpikeTimes(features, temporalWindow_);
}

SensoryAdapter::SpikePattern RetinaAdapter::processData(const DataSample& data) {
    // Convert raw data to image
    Image image;
    image.pixels = data.rawData;
    
    // Infer image dimensions if not set
    if (imageRows_ == 0 || imageCols_ == 0) {
        // Assume square image
        int totalPixels = data.rawData.size();
        imageRows_ = imageCols_ = static_cast<int>(std::sqrt(totalPixels));
        regionSize_ = imageRows_ / gridSize_;
        
        SNNFW_INFO("RetinaAdapter '{}': inferred image size {}x{}, region size {}", 
                   getName(), imageRows_, imageCols_, regionSize_);
    }
    
    image.rows = imageRows_;
    image.cols = imageCols_;
    
    // Extract features and encode as spikes
    auto features = extractFeatures(data);
    return encodeFeatures(features);
}

SensoryAdapter::FeatureVector RetinaAdapter::extractFeatures(const DataSample& data) {
    FeatureVector result;
    result.timestamp = data.timestamp;
    
    // Convert to image
    Image image;
    image.pixels = data.rawData;
    image.rows = imageRows_;
    image.cols = imageCols_;
    
    // Extract features for each region
    for (int row = 0; row < gridSize_; ++row) {
        for (int col = 0; col < gridSize_; ++col) {
            auto region = extractRegion(image, row, col);
            auto edgeFeatures = extractEdgeFeatures(region, regionSize_);
            
            // Add to feature vector
            for (double feature : edgeFeatures) {
                result.features.push_back(feature);
            }
        }
    }
    
    return result;
}

SensoryAdapter::SpikePattern RetinaAdapter::encodeFeatures(const FeatureVector& features) {
    SpikePattern pattern;
    pattern.timestamp = features.timestamp;
    pattern.duration = temporalWindow_;
    pattern.spikeTimes.resize(neurons_.size());

    // Clear all neurons first
    clearNeuronStates();

    // Get neurons per feature from encoding strategy
    int neuronsPerFeature = encodingStrategy_->getNeuronsPerFeature();

    // Encode features as spikes and insert into neurons
    size_t featureIdx = 0;
    for (int row = 0; row < gridSize_; ++row) {
        for (int col = 0; col < gridSize_; ++col) {
            for (int orient = 0; orient < numOrientations_; ++orient) {
                double featureValue = features.features[featureIdx++];

                // Use encoding strategy to generate spike times
                std::vector<double> spikeTimes = encodingStrategy_->encode(featureValue, orient);

                // For rate/temporal encoding (1 neuron per feature), insert into single neuron
                if (neuronsPerFeature == 1) {
                    auto& neuron = neuronGrid_[row * gridSize_ + col][orient];
                    int neuronIdx = row * gridSize_ * numOrientations_ +
                                   col * numOrientations_ + orient;

                    for (double spikeTime : spikeTimes) {
                        neuron->insertSpike(spikeTime);
                        pattern.spikeTimes[neuronIdx].push_back(spikeTime);
                    }
                } else {
                    // For population encoding (multiple neurons per feature)
                    // Each spike goes to a different neuron in the population
                    // Note: This requires neuron structure to support population encoding
                    // For now, we'll distribute spikes across the single neuron
                    // TODO: Extend neuron grid to support population encoding
                    auto& neuron = neuronGrid_[row * gridSize_ + col][orient];
                    int neuronIdx = row * gridSize_ * numOrientations_ +
                                   col * numOrientations_ + orient;

                    for (double spikeTime : spikeTimes) {
                        neuron->insertSpike(spikeTime);
                        pattern.spikeTimes[neuronIdx].push_back(spikeTime);
                    }
                }
            }
        }
    }

    return pattern;
}

std::vector<double> RetinaAdapter::getActivationPattern() const {
    std::vector<double> activations(neurons_.size(), 0.0);

    for (size_t i = 0; i < neurons_.size(); ++i) {
        // Use best similarity as activation (0.0 to 1.0)
        // Returns -1.0 if no patterns learned, so we clamp to 0.0
        double similarity = neurons_[i]->getBestSimilarity();
        activations[i] = (similarity >= 0.0) ? similarity : 0.0;
    }

    return activations;
}

void RetinaAdapter::clearNeuronStates() {
    for (auto& neuron : neurons_) {
        neuron->clearSpikes();
    }
}

std::shared_ptr<Neuron> RetinaAdapter::getNeuronAt(int row, int col, int orientation) const {
    if (row < 0 || row >= gridSize_ || col < 0 || col >= gridSize_ || 
        orientation < 0 || orientation >= numOrientations_) {
        return nullptr;
    }
    
    return neuronGrid_[row * gridSize_ + col][orientation];
}

std::vector<double> RetinaAdapter::processImage(const Image& image) {
    DataSample sample;
    sample.rawData = image.pixels;
    sample.timestamp = 0.0;
    
    // Set image dimensions
    imageRows_ = image.rows;
    imageCols_ = image.cols;
    regionSize_ = imageRows_ / gridSize_;
    
    // Process and get activation pattern
    processData(sample);
    return getActivationPattern();
}

} // namespace adapters
} // namespace snnfw

