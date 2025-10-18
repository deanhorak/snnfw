#include "snnfw/features/SobelOperator.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace snnfw {
namespace features {

SobelOperator::SobelOperator(const Config& config)
    : EdgeOperator(config) {}

double SobelOperator::computeOrientedGradient(
    const std::vector<uint8_t>& region,
    int regionSize,
    int orientation) const {
    
    double gradient = 0.0;
    
    // Compute gradient for each pixel (excluding borders)
    for (int r = 1; r < regionSize - 1; ++r) {
        for (int c = 1; c < regionSize - 1; ++c) {
            // Get 8 neighbors
            double top = getPixelNormalized(region, r-1, c, regionSize);
            double bottom = getPixelNormalized(region, r+1, c, regionSize);
            double left = getPixelNormalized(region, r, c-1, regionSize);
            double right = getPixelNormalized(region, r, c+1, regionSize);
            double topLeft = getPixelNormalized(region, r-1, c-1, regionSize);
            double topRight = getPixelNormalized(region, r-1, c+1, regionSize);
            double bottomLeft = getPixelNormalized(region, r+1, c-1, regionSize);
            double bottomRight = getPixelNormalized(region, r+1, c+1, regionSize);
            
            // Compute gradient based on orientation
            // For first 8 orientations, use predefined patterns
            if (orientation == 0 && config_.numOrientations >= 1) {
                // 0° (horizontal)
                gradient += std::abs(right - left);
            } else if (orientation == 1 && config_.numOrientations >= 2) {
                // 22.5°
                gradient += std::abs(topRight - bottomLeft);
            } else if (orientation == 2 && config_.numOrientations >= 3) {
                // 45° (diagonal)
                gradient += std::abs(top + topRight - bottom - bottomLeft);
            } else if (orientation == 3 && config_.numOrientations >= 4) {
                // 67.5°
                gradient += std::abs(topRight - bottomLeft);
            } else if (orientation == 4 && config_.numOrientations >= 5) {
                // 90° (vertical)
                gradient += std::abs(bottom - top);
            } else if (orientation == 5 && config_.numOrientations >= 6) {
                // 112.5°
                gradient += std::abs(bottomRight - topLeft);
            } else if (orientation == 6 && config_.numOrientations >= 7) {
                // 135° (diagonal)
                gradient += std::abs(bottom + bottomRight - top - topLeft);
            } else if (orientation == 7 && config_.numOrientations >= 8) {
                // 157.5°
                gradient += std::abs(bottomRight - topLeft);
            } else if (orientation >= 8) {
                // For more than 8 orientations, distribute evenly across 180 degrees
                double angle = (orientation * 180.0) / config_.numOrientations;
                double radians = angle * M_PI / 180.0;
                double dx = std::cos(radians);
                double dy = std::sin(radians);
                
                // Approximate gradient in this direction
                gradient += std::abs(dx * (right - left) + dy * (bottom - top));
            }
        }
    }
    
    return gradient;
}

std::vector<double> SobelOperator::extractEdges(
    const std::vector<uint8_t>& region,
    int regionSize) const {
    
    std::vector<double> features(config_.numOrientations, 0.0);
    
    // Compute gradient for each orientation
    for (int orient = 0; orient < config_.numOrientations; ++orient) {
        features[orient] = computeOrientedGradient(region, regionSize, orient);
    }
    
    // Normalize features to [0, 1]
    features = normalizeFeatures(features);
    
    // Apply threshold
    features = applyThreshold(features);
    
    return features;
}

std::string SobelOperator::getName() const {
    return "SobelOperator";
}

} // namespace features
} // namespace snnfw

