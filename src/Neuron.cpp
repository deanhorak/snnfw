#include "snnfw/Neuron.h"
#include "snnfw/Logger.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <algorithm>

using json = nlohmann::json;

namespace snnfw {

Neuron::Neuron(double windowSizeMs, double similarityThreshold, size_t maxReferencePatterns, uint64_t neuronId)
    : NeuralObject(neuronId), windowSize(windowSizeMs), threshold(similarityThreshold), maxPatterns(maxReferencePatterns), axonId(0) {}

void Neuron::insertSpike(double spikeTime) {
    spikes.push_back(spikeTime);
    removeOldSpikes(spikeTime);

    // NOTE: Removed shouldFire() check here because it causes issues during training
    // When learning patterns, we don't want the neuron to fire based on previously
    // learned patterns. Use getBestSimilarity() or checkShouldFire() explicitly
    // during testing/inference instead.

    // if (shouldFire()) {
    //     SNNFW_INFO("Neuron {} fires a new spike at time: {}", getId(), spikeTime);
    // }
}

void Neuron::learnCurrentPattern() {
    if (spikes.empty()) {
        SNNFW_WARN("Neuron {}: Cannot learn pattern - no spikes in window", getId());
        return;
    }

    if (referencePatterns.size() < maxPatterns) {
        referencePatterns.push_back(spikes);
        SNNFW_DEBUG("Neuron {}: Learned new pattern (size={})", getId(), spikes.size());
    } else {
        int bestIndex = findMostSimilarPattern(spikes);
        if (bestIndex != -1) {
            blendPattern(referencePatterns[bestIndex], spikes, 0.2);
            SNNFW_DEBUG("Neuron {}: Blended new pattern into pattern #{}", getId(), bestIndex);
        } else {
            // Optional: replace oldest or least-used pattern instead
            size_t randIndex = rand() % referencePatterns.size();
            referencePatterns[randIndex] = spikes;
            SNNFW_DEBUG("Neuron {}: Replaced pattern #{} with new pattern", getId(), randIndex);
        }
    }
}

void Neuron::printSpikes() const {
    std::string spikesStr;
    for (double spikeTime : spikes) {
        spikesStr += std::to_string(spikeTime) + " ";
    }
    SNNFW_INFO("Neuron {}: Current spikes in window: {}", getId(), spikesStr);
}

void Neuron::printReferencePatterns() const {
    SNNFW_INFO("Neuron {}: Stored reference patterns ({})", getId(), referencePatterns.size());
    for (size_t i = 0; i < referencePatterns.size(); ++i) {
        std::string patternStr;
        for (double val : referencePatterns[i]) {
            patternStr += std::to_string(val) + " ";
        }
        SNNFW_INFO("  Pattern #{}: {}", i, patternStr);
    }
}

void Neuron::removeOldSpikes(double currentTime) {
    while (!spikes.empty() && (currentTime - spikes.front() > windowSize)) {
        spikes.erase(spikes.begin());
    }
}

// Convert spike pattern to temporal histogram (fuzzy representation)
// Uses cumulative distribution function (CDF) instead of histogram
// to preserve temporal ordering information
std::vector<double> Neuron::spikeToHistogram(const std::vector<double>& pattern) const {
    const int bins = 50;  // More bins for better temporal resolution
    std::vector<double> cdf(bins, 0.0);

    if (pattern.empty()) return cdf;

    // Sort spikes for CDF computation
    std::vector<double> sortedSpikes = pattern;
    std::sort(sortedSpikes.begin(), sortedSpikes.end());

    double binSize = windowSize / bins;

    // Build cumulative distribution function
    // CDF[i] = fraction of spikes that occurred before time i*binSize
    size_t spikeIdx = 0;
    for (int bin = 0; bin < bins; ++bin) {
        double binEnd = (bin + 1) * binSize;

        // Count spikes up to this bin
        while (spikeIdx < sortedSpikes.size() && sortedSpikes[spikeIdx] < binEnd) {
            spikeIdx++;
        }

        // Normalize by total spike count
        cdf[bin] = static_cast<double>(spikeIdx) / sortedSpikes.size();
    }

    return cdf;
}

// Compute similarity between two CDFs using hybrid approach:
// 1. Direct CDF similarity (captures overall temporal distribution)
// 2. First derivative similarity (captures spike rate changes)
// 3. L1 distance penalty (penalizes different distributions)
double Neuron::histogramSimilarity(const std::vector<double>& cdf1, const std::vector<double>& cdf2) const {
    if (cdf1.size() < 2 || cdf2.size() < 2) return 0.0;

    // 1. Cosine similarity on CDFs directly
    double dot_cdf = 0.0, norm1_cdf = 0.0, norm2_cdf = 0.0;
    for (size_t i = 0; i < cdf1.size(); ++i) {
        dot_cdf += cdf1[i] * cdf2[i];
        norm1_cdf += cdf1[i] * cdf1[i];
        norm2_cdf += cdf2[i] * cdf2[i];
    }
    double cdf_sim = 0.0;
    if (norm1_cdf > 1e-10 && norm2_cdf > 1e-10) {
        cdf_sim = dot_cdf / (std::sqrt(norm1_cdf) * std::sqrt(norm2_cdf));
    }

    // 2. Compute first derivatives (spike rate) and their similarity
    std::vector<double> deriv1, deriv2;
    for (size_t i = 0; i < cdf1.size() - 1; ++i) {
        deriv1.push_back(cdf1[i+1] - cdf1[i]);
        deriv2.push_back(cdf2[i+1] - cdf2[i]);
    }

    double dot_deriv = 0.0, norm1_deriv = 0.0, norm2_deriv = 0.0;
    for (size_t i = 0; i < deriv1.size(); ++i) {
        dot_deriv += deriv1[i] * deriv2[i];
        norm1_deriv += deriv1[i] * deriv1[i];
        norm2_deriv += deriv2[i] * deriv2[i];
    }
    double deriv_sim = 0.0;
    if (norm1_deriv > 1e-10 && norm2_deriv > 1e-10) {
        deriv_sim = dot_deriv / (std::sqrt(norm1_deriv) * std::sqrt(norm2_deriv));
    }

    // 3. L1 distance (lower is better, so invert it)
    double l1_dist = 0.0;
    for (size_t i = 0; i < cdf1.size(); ++i) {
        l1_dist += std::abs(cdf1[i] - cdf2[i]);
    }
    double l1_sim = 1.0 / (1.0 + l1_dist);  // Convert distance to similarity

    // Weighted combination: emphasize derivative (temporal structure)
    return 0.3 * cdf_sim + 0.5 * deriv_sim + 0.2 * l1_sim;
}

bool Neuron::shouldFire() const {
    // Convert current spikes to histogram
    auto currentHist = spikeToHistogram(spikes);

    for (const auto& refPattern : referencePatterns) {
        // Convert reference pattern to histogram
        auto refHist = spikeToHistogram(refPattern);

        // Compare histograms (fuzzy matching)
        double similarity = histogramSimilarity(currentHist, refHist);
        if (similarity >= threshold) {
            return true;
        }
    }
    return false;
}

double Neuron::getBestSimilarity() const {
    // Return 0 if no spikes or no reference patterns
    if (spikes.empty() || referencePatterns.empty()) {
        return 0.0;
    }

    double bestSim = -1.0;

    // Use spike distance metric (lower distance = higher similarity)
    for (const auto& refPattern : referencePatterns) {
        // Skip empty reference patterns
        if (refPattern.empty()) continue;

        // Compute spike distance
        double distance = spikeDistance(spikes, refPattern);

        // Convert distance to similarity: similarity = 1 / (1 + distance)
        // This gives values in range [0, 1], with 1 being perfect match
        double similarity = 1.0 / (1.0 + distance);

        if (similarity > bestSim) {
            bestSim = similarity;
        }
    }
    return (bestSim < 0.0) ? 0.0 : bestSim;
}

int Neuron::findMostSimilarPattern(const std::vector<double>& newPattern) const {
    int bestIndex = -1;
    double bestSim = -1.0;

    for (size_t i = 0; i < referencePatterns.size(); ++i) {
        const auto& ref = referencePatterns[i];
        if (ref.size() != newPattern.size()) continue;

        double sim = cosineSimilarity(ref, newPattern);
        if (sim > bestSim) {
            bestSim = sim;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

void Neuron::blendPattern(std::vector<double>& target, const std::vector<double>& newPattern, double alpha) {
    for (size_t i = 0; i < target.size(); ++i) {
        target[i] = (1.0 - alpha) * target[i] + alpha * newPattern[i];
    }
}

double Neuron::cosineSimilarity(const std::vector<double>& a, const std::vector<double>& b) {
    double dot = 0.0, normA = 0.0, normB = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    if (normA == 0.0 || normB == 0.0) return 0.0;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}

// Compute spike distance using a simplified Victor-Purpura-like metric
// Returns a distance (lower is better), which we convert to similarity
double Neuron::spikeDistance(const std::vector<double>& spikes1, const std::vector<double>& spikes2) const {
    // Cost parameter: how much it costs to shift a spike by 1ms
    const double q = 0.5;

    // Handle empty cases
    if (spikes1.empty() && spikes2.empty()) return 0.0;
    if (spikes1.empty()) return static_cast<double>(spikes2.size());
    if (spikes2.empty()) return static_cast<double>(spikes1.size());

    // Simplified greedy matching: for each spike in spikes1, find closest in spikes2
    double totalCost = 0.0;
    std::vector<bool> matched2(spikes2.size(), false);

    for (double s1 : spikes1) {
        double minDist = std::numeric_limits<double>::max();
        int bestMatch = -1;

        // Find closest unmatched spike in spikes2
        for (size_t j = 0; j < spikes2.size(); ++j) {
            if (!matched2[j]) {
                double dist = std::abs(s1 - spikes2[j]);
                if (dist < minDist) {
                    minDist = dist;
                    bestMatch = static_cast<int>(j);
                }
            }
        }

        // Cost is either: shift cost (q * time_diff) or delete+insert cost (2.0)
        if (bestMatch >= 0) {
            double shiftCost = q * minDist;
            double deleteInsertCost = 2.0;

            if (shiftCost < deleteInsertCost) {
                totalCost += shiftCost;
                matched2[bestMatch] = true;
            } else {
                totalCost += deleteInsertCost;
            }
        } else {
            totalCost += 1.0;  // Delete cost
        }
    }

    // Add cost for unmatched spikes in spikes2 (insert cost)
    for (bool matched : matched2) {
        if (!matched) totalCost += 1.0;
    }

    return totalCost;
}

void Neuron::addDendrite(uint64_t dendriteId) {
    // Check if dendrite is already connected
    auto it = std::find(dendriteIds.begin(), dendriteIds.end(), dendriteId);
    if (it == dendriteIds.end()) {
        dendriteIds.push_back(dendriteId);
        SNNFW_DEBUG("Neuron {}: Added dendrite {} (total: {})",
                    getId(), dendriteId, dendriteIds.size());
    } else {
        SNNFW_WARN("Neuron {}: Dendrite {} already connected", getId(), dendriteId);
    }
}

bool Neuron::removeDendrite(uint64_t dendriteId) {
    auto it = std::find(dendriteIds.begin(), dendriteIds.end(), dendriteId);
    if (it != dendriteIds.end()) {
        dendriteIds.erase(it);
        SNNFW_DEBUG("Neuron {}: Removed dendrite {} (remaining: {})",
                    getId(), dendriteId, dendriteIds.size());
        return true;
    }
    SNNFW_WARN("Neuron {}: Dendrite {} not found for removal", getId(), dendriteId);
    return false;
}

std::string Neuron::toJson() const {
    json j;
    j["type"] = "Neuron";
    j["id"] = getId();
    j["windowSize"] = windowSize;
    j["threshold"] = threshold;
    j["maxPatterns"] = maxPatterns;
    j["axonId"] = axonId;
    j["dendriteIds"] = dendriteIds;
    j["spikes"] = spikes;
    j["referencePatterns"] = referencePatterns;
    return j.dump();
}

bool Neuron::fromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        // Validate type
        if (j["type"] != "Neuron") {
            SNNFW_ERROR("Invalid type in JSON: expected 'Neuron', got '{}'", j["type"].get<std::string>());
            return false;
        }

        // Deserialize ID from base class
        setId(j["id"]);

        // Deserialize fields
        windowSize = j["windowSize"];
        threshold = j["threshold"];
        maxPatterns = j["maxPatterns"];
        axonId = j["axonId"];
        dendriteIds = j["dendriteIds"].get<std::vector<uint64_t>>();
        spikes = j["spikes"].get<std::vector<double>>();
        referencePatterns = j["referencePatterns"].get<std::vector<std::vector<double>>>();

        return true;
    } catch (const std::exception& e) {
        SNNFW_ERROR("Failed to deserialize Neuron from JSON: {}", e.what());
        return false;
    }
}

} // namespace snnfw