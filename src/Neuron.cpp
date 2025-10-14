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

    if (shouldFire()) {
        SNNFW_INFO("Neuron {} fires a new spike at time: {}", getId(), spikeTime);
    }
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

bool Neuron::shouldFire() const {
    for (const auto& refPattern : referencePatterns) {
        if (spikes.size() == refPattern.size()) {
            double similarity = cosineSimilarity(spikes, refPattern);
            if (similarity >= threshold) {
                return true;
            }
        }
    }
    return false;
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