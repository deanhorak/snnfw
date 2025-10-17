#include "snnfw/adapters/RetinaAdapter.h"
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
    
    SNNFW_INFO("RetinaAdapter '{}': grid={}x{}, orientations={}, threshold={}", 
               getName(), gridSize_, gridSize_, numOrientations_, edgeThreshold_);
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
    std::vector<double> features(numOrientations_, 0.0);

    // Edge detection with Gabor-like filters at multiple orientations
    for (int r = 1; r < regionSize - 1; ++r) {
        for (int c = 1; c < regionSize - 1; ++c) {
            int idx = r * regionSize + c;
            double center = static_cast<double>(region[idx]);

            // Get neighbors
            double top = static_cast<double>(region[(r-1) * regionSize + c]);
            double bottom = static_cast<double>(region[(r+1) * regionSize + c]);
            double left = static_cast<double>(region[r * regionSize + (c-1)]);
            double right = static_cast<double>(region[r * regionSize + (c+1)]);
            double topLeft = static_cast<double>(region[(r-1) * regionSize + (c-1)]);
            double topRight = static_cast<double>(region[(r-1) * regionSize + (c+1)]);
            double bottomLeft = static_cast<double>(region[(r+1) * regionSize + (c-1)]);
            double bottomRight = static_cast<double>(region[(r+1) * regionSize + (c+1)]);

            // Calculate gradients for up to 8 orientations (based on numOrientations_)
            // Only compute the orientations we actually need
            if (numOrientations_ >= 1) {
                // 0° (horizontal)
                features[0] += std::abs(right - left);
            }

            if (numOrientations_ >= 2) {
                // 22.5°
                features[1] += std::abs(topRight - bottomLeft);
            }

            if (numOrientations_ >= 3) {
                // 45° (diagonal)
                features[2] += std::abs(top + topRight - bottom - bottomLeft);
            }

            if (numOrientations_ >= 4) {
                // 67.5°
                features[3] += std::abs(topRight - bottomLeft);
            }

            if (numOrientations_ >= 5) {
                // 90° (vertical)
                features[4] += std::abs(bottom - top);
            }

            if (numOrientations_ >= 6) {
                // 112.5°
                features[5] += std::abs(bottomRight - topLeft);
            }

            if (numOrientations_ >= 7) {
                // 135° (diagonal)
                features[6] += std::abs(bottom + bottomRight - top - topLeft);
            }

            if (numOrientations_ >= 8) {
                // 157.5°
                features[7] += std::abs(bottomRight - topLeft);
            }

            // For more than 8 orientations, distribute evenly across 180 degrees
            for (int orient = 8; orient < numOrientations_; ++orient) {
                double angle = (orient * 180.0) / numOrientations_;
                double radians = angle * M_PI / 180.0;
                double dx = std::cos(radians);
                double dy = std::sin(radians);

                // Approximate gradient in this direction
                double grad = std::abs(dx * (right - left) + dy * (bottom - top));
                features[orient] += grad;
            }
        }
    }
    
    // Normalize features
    double maxFeature = *std::max_element(features.begin(), features.end());
    if (maxFeature > 0.0) {
        for (double& f : features) {
            f /= maxFeature;
        }
    }
    
    return features;
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
    
    // Encode features as spikes and insert into neurons
    size_t featureIdx = 0;
    for (int row = 0; row < gridSize_; ++row) {
        for (int col = 0; col < gridSize_; ++col) {
            for (int orient = 0; orient < numOrientations_; ++orient) {
                double featureValue = features.features[featureIdx++];
                
                // Only generate spike if above threshold
                if (featureValue >= edgeThreshold_) {
                    double spikeTime = featureToSpikeTime(featureValue, temporalWindow_);
                    if (spikeTime >= 0.0) {
                        auto& neuron = neuronGrid_[row * gridSize_ + col][orient];
                        neuron->insertSpike(spikeTime);
                        pattern.spikeTimes[row * gridSize_ * numOrientations_ + 
                                          col * numOrientations_ + orient].push_back(spikeTime);
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

