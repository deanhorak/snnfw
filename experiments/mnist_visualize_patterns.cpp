/**
 * MNIST Pattern Visualization - Visualize spike pattern shapes
 *
 * This experiment visualizes the cumulative spike patterns to understand
 * their "shape" or "curvature" for developing better similarity metrics.
 */

#include "snnfw/MNISTLoader.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace snnfw;

// Convert MNIST image to spike pattern using rate coding
std::vector<double> imageToSpikePattern(const MNISTLoader::Image& img, double duration = 50.0) {
    std::vector<double> spikes;
    
    for (int row = 0; row < img.rows; ++row) {
        for (int col = 0; col < img.cols; ++col) {
            double intensity = img.getNormalizedPixel(row, col);
            if (intensity > 0.1) {
                double spikeTime = duration * (1.0 - intensity);
                spikes.push_back(spikeTime);
            }
        }
    }
    
    std::sort(spikes.begin(), spikes.end());
    return spikes;
}

// Normalize spike pattern to fixed number of points (for comparison)
std::vector<double> normalizePattern(const std::vector<double>& spikes, int targetSize = 100) {
    if (spikes.empty()) return {};
    if (spikes.size() == 1) return {0.0};
    
    std::vector<double> normalized;
    normalized.reserve(targetSize);
    
    double minTime = spikes.front();
    double maxTime = spikes.back();
    double range = maxTime - minTime;
    
    if (range < 1e-6) {
        // All spikes at same time
        for (int i = 0; i < targetSize; ++i) {
            normalized.push_back(0.0);
        }
        return normalized;
    }
    
    // Normalize to [0, 1] range
    for (double spike : spikes) {
        normalized.push_back((spike - minTime) / range);
    }
    
    // Resample to target size using linear interpolation
    std::vector<double> resampled;
    resampled.reserve(targetSize);
    
    for (int i = 0; i < targetSize; ++i) {
        double targetIdx = (i * (normalized.size() - 1)) / (double)(targetSize - 1);
        int idx = (int)targetIdx;
        double frac = targetIdx - idx;
        
        if (idx >= (int)normalized.size() - 1) {
            resampled.push_back(normalized.back());
        } else {
            double interpolated = normalized[idx] * (1.0 - frac) + normalized[idx + 1] * frac;
            resampled.push_back(interpolated);
        }
    }
    
    return resampled;
}

// Compute cumulative distribution (for visualizing pattern "shape")
std::vector<double> cumulativeDistribution(const std::vector<double>& spikes, double duration = 50.0) {
    if (spikes.empty()) return {};
    
    const int bins = 100;
    std::vector<double> cumulative(bins, 0.0);
    
    int spikeIdx = 0;
    for (int i = 0; i < bins; ++i) {
        double timeThreshold = (i + 1) * duration / bins;
        
        // Count spikes up to this time
        while (spikeIdx < (int)spikes.size() && spikes[spikeIdx] <= timeThreshold) {
            spikeIdx++;
        }
        
        cumulative[i] = spikeIdx / (double)spikes.size();
    }
    
    return cumulative;
}

// Visualize cumulative distribution
void visualizeCumulative(const std::vector<double>& cumulative, const std::string& label) {
    const int width = 80;
    
    std::cout << "\n" << label << " - Cumulative Distribution:\n";
    std::cout << "Time →\n";
    
    for (int i = 0; i < (int)cumulative.size(); i += 2) {
        int barWidth = (int)(cumulative[i] * width);
        std::cout << std::setw(3) << (i * 2) << "% |";
        for (int j = 0; j < barWidth; ++j) std::cout << "█";
        std::cout << " " << std::fixed << std::setprecision(3) << cumulative[i] << "\n";
    }
}

// Compute temporal histogram with ABSOLUTE time (not normalized)
std::vector<double> temporalHistogram(const std::vector<double>& spikes, double duration = 50.0, int bins = 50) {
    std::vector<double> hist(bins, 0.0);
    if (spikes.empty()) return hist;

    double binSize = duration / bins;

    // Count spikes in each absolute time bin
    for (double spike : spikes) {
        int bin = std::min(bins - 1, (int)(spike / binSize));
        hist[bin] += 1.0;
    }

    // Normalize to probability distribution (sum = 1.0)
    double sum = 0.0;
    for (double h : hist) sum += h;
    if (sum > 0) {
        for (double& h : hist) h /= sum;
    }

    return hist;
}

// Bhattacharyya coefficient (measures overlap between probability distributions)
double bhattacharyyaSimilarity(const std::vector<double>& hist1, const std::vector<double>& hist2) {
    double bc = 0.0;
    for (size_t i = 0; i < hist1.size(); ++i) {
        bc += std::sqrt(hist1[i] * hist2[i]);
    }
    return bc;  // Range: [0, 1], 1 = identical distributions
}

// Hellinger distance (related to Bhattacharyya)
double hellingerSimilarity(const std::vector<double>& hist1, const std::vector<double>& hist2) {
    double sumSquaredDiff = 0.0;
    for (size_t i = 0; i < hist1.size(); ++i) {
        double diff = std::sqrt(hist1[i]) - std::sqrt(hist2[i]);
        sumSquaredDiff += diff * diff;
    }
    double hellingerDist = std::sqrt(sumSquaredDiff) / std::sqrt(2.0);
    return 1.0 - hellingerDist;  // Convert to similarity
}

// Compute "curvature" of cumulative distribution (second derivative)
std::vector<double> computeCurvature(const std::vector<double>& spikes, double duration = 50.0, int bins = 50) {
    // First compute cumulative distribution
    std::vector<double> cumulative(bins, 0.0);
    double binSize = duration / bins;

    int spikeIdx = 0;
    for (int i = 0; i < bins; ++i) {
        double timeThreshold = (i + 1) * binSize;
        while (spikeIdx < (int)spikes.size() && spikes[spikeIdx] <= timeThreshold) {
            spikeIdx++;
        }
        cumulative[i] = spikeIdx;
    }

    // Compute second derivative (curvature)
    std::vector<double> curvature(bins, 0.0);
    for (int i = 1; i < bins - 1; ++i) {
        // Second derivative: f''(x) ≈ f(x+1) - 2*f(x) + f(x-1)
        curvature[i] = cumulative[i+1] - 2.0 * cumulative[i] + cumulative[i-1];
    }

    return curvature;
}

// Compute curvature-based similarity
double curvatureSimilarity(const std::vector<double>& pattern1, const std::vector<double>& pattern2) {
    const int bins = 50;
    const double duration = 50.0;

    auto curv1 = computeCurvature(pattern1, duration, bins);
    auto curv2 = computeCurvature(pattern2, duration, bins);

    // Compute correlation between curvatures
    double dot = 0.0, norm1 = 0.0, norm2 = 0.0;
    for (int i = 0; i < bins; ++i) {
        dot += curv1[i] * curv2[i];
        norm1 += curv1[i] * curv1[i];
        norm2 += curv2[i] * curv2[i];
    }

    if (norm1 < 1e-10 || norm2 < 1e-10) return 0.0;

    // Cosine similarity of curvatures
    double cosineSim = dot / (std::sqrt(norm1) * std::sqrt(norm2));

    // Map from [-1, 1] to [0, 1]
    return (cosineSim + 1.0) / 2.0;
}

// Compute Earth Mover's Distance (Wasserstein distance)
double earthMoverDistance(const std::vector<double>& pattern1, const std::vector<double>& pattern2) {
    auto norm1 = normalizePattern(pattern1, 100);
    auto norm2 = normalizePattern(pattern2, 100);
    
    if (norm1.empty() || norm2.empty()) return 1.0;
    
    // Compute cumulative distributions
    std::vector<double> cum1(100, 0.0), cum2(100, 0.0);
    
    for (size_t i = 0; i < 100; ++i) {
        double time = i / 100.0;
        
        int count1 = 0, count2 = 0;
        for (double t : norm1) if (t <= time) count1++;
        for (double t : norm2) if (t <= time) count2++;
        
        cum1[i] = count1 / (double)norm1.size();
        cum2[i] = count2 / (double)norm2.size();
    }
    
    // EMD is the area between cumulative distributions
    double emd = 0.0;
    for (size_t i = 0; i < 100; ++i) {
        emd += std::abs(cum1[i] - cum2[i]);
    }
    emd /= 100.0;
    
    // Convert to similarity
    return 1.0 - emd;
}

int main() {
    std::cout << "=== MNIST Spike Pattern Visualization ===\n\n";

    // Load MNIST data
    MNISTLoader loader;
    std::string dataPath = "/home/dean/repos/ctm/data/MNIST/raw";

    if (!loader.load(dataPath + "/train-images-idx3-ubyte",
                     dataPath + "/train-labels-idx1-ubyte", 100)) {
        std::cerr << "Failed to load MNIST data\n";
        return 1;
    }

    std::cout << "Loaded " << loader.size() << " images\n\n";

    // Find first 3 examples of digit '1' and '8'
    std::vector<std::vector<double>> patterns1, patterns8;

    for (size_t i = 0; i < loader.size() && (patterns1.size() < 3 || patterns8.size() < 3); ++i) {
        const auto& img = loader.getImage(i);
        auto pattern = imageToSpikePattern(img);

        if (img.label == 1 && patterns1.size() < 3) {
            patterns1.push_back(pattern);
            std::cout << "Digit 1 example " << (patterns1.size()) << ": "
                      << pattern.size() << " spikes\n";
        } else if (img.label == 8 && patterns8.size() < 3) {
            patterns8.push_back(pattern);
            std::cout << "Digit 8 example " << (patterns8.size()) << ": "
                      << pattern.size() << " spikes\n";
        }
    }
    
    // Visualize cumulative distributions
    std::cout << "\n=== Cumulative Distributions ===\n";
    
    for (size_t i = 0; i < patterns1.size(); ++i) {
        auto cum = cumulativeDistribution(patterns1[i]);
        visualizeCumulative(cum, "Digit 1 #" + std::to_string(i + 1));
    }
    
    for (size_t i = 0; i < patterns8.size(); ++i) {
        auto cum = cumulativeDistribution(patterns8[i]);
        visualizeCumulative(cum, "Digit 8 #" + std::to_string(i + 1));
    }
    
    // Compare similarities
    std::cout << "\n=== Similarity Comparisons ===\n\n";
    
    std::cout << "Curvature Similarity:\n";
    std::cout << "  1-1 (same digit):  " << curvatureSimilarity(patterns1[0], patterns1[1]) << "\n";
    std::cout << "  1-1 (same digit):  " << curvatureSimilarity(patterns1[0], patterns1[2]) << "\n";
    std::cout << "  1-8 (diff digit):  " << curvatureSimilarity(patterns1[0], patterns8[0]) << "\n";
    std::cout << "  8-8 (same digit):  " << curvatureSimilarity(patterns8[0], patterns8[1]) << "\n";
    std::cout << "  8-8 (same digit):  " << curvatureSimilarity(patterns8[0], patterns8[2]) << "\n";
    
    std::cout << "\nEarth Mover's Distance Similarity:\n";
    std::cout << "  1-1 (same digit):  " << earthMoverDistance(patterns1[0], patterns1[1]) << "\n";
    std::cout << "  1-1 (same digit):  " << earthMoverDistance(patterns1[0], patterns1[2]) << "\n";
    std::cout << "  1-8 (diff digit):  " << earthMoverDistance(patterns1[0], patterns8[0]) << "\n";
    std::cout << "  8-8 (same digit):  " << earthMoverDistance(patterns8[0], patterns8[1]) << "\n";
    std::cout << "  8-8 (same digit):  " << earthMoverDistance(patterns8[0], patterns8[2]) << "\n";
    
    std::cout << "\n=== Analysis ===\n";
    std::cout << "Good similarity metric should show:\n";
    std::cout << "  - HIGH similarity for same digit (>0.8)\n";
    std::cout << "  - LOW similarity for different digits (<0.5)\n";
    
    return 0;
}

