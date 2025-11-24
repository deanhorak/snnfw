# Hyperparameter Optimization Roadmap
## Reaching Global Optimum for MNIST

### Current Status

**Current Best Accuracy**: 92.70% (with manual tuning)
**Optimization Framework**: ✅ Implemented (Grid, Random, Bayesian, Genetic)
**Current Parameters**: 7 basic parameters

### Problem Analysis

The current optimization is limited to **tuning existing parameters** within a **fixed architecture**. To reach the global optimum (potentially 95-98% for SNNs on MNIST), we need to explore:

1. **Structural parameters** (network topology)
2. **Encoding strategies** (how features → spikes)
3. **Learning mechanisms** (how patterns are stored/updated)
4. **Feature extraction** (edge detection methods)
5. **Classification strategies** (beyond simple k-NN)

---

## Phase 1: Extended Parameter Space ✅

**Status**: Configuration created (`configs/mnist_optimization_extended.json`)

### New Parameters (13 additional)

#### Encoding Parameters
- `encoding.spike_timing_mode`: rate_coding | temporal_coding | population_coding
- `encoding.spike_intensity_scale`: 0.1 - 5.0 (how feature intensity maps to spike time)
- `encoding.baseline_spike_time`: 0 - 100ms (offset for spike times)

#### Preprocessing Parameters
- `preprocessing.normalization`: none | global | local | adaptive
- `preprocessing.contrast_enhancement`: 0.5 - 2.0

#### Feature Extraction Parameters
- `feature_extraction.edge_operator`: sobel | gabor | difference_of_gaussians
- `feature_extraction.spatial_pooling`: none | max | average | sum

#### Learning Parameters
- `learning.pattern_update_strategy`: append | replace_worst | merge_similar
- `learning.similarity_metric`: cosine | euclidean | manhattan | correlation

#### Classification Parameters
- `classification.voting_strategy`: majority | weighted_distance | weighted_similarity
- `classification.distance_weighting`: 0.0 - 2.0

#### Network Structure Parameters
- `network.neurons_per_orientation`: 1 - 5 (population coding)
- `network.lateral_inhibition`: none | winner_take_all | soft_competition
- `network.temporal_integration`: 0.0 - 1.0 (for multi-frame processing)

**Total Parameters**: 20 (7 original + 13 new)

---

## Phase 2: Implementation Requirements

### 2.1 Encoding Strategies (HIGH IMPACT)

**Current**: Simple rate coding (stronger edge → earlier spike)

**Proposed Implementations**:

#### A. Temporal Coding
```cpp
// Encode feature intensity in precise spike timing
double encodeTemporalSpike(double featureIntensity, double baseline, double scale) {
    // Stronger features spike earlier
    return baseline + (1.0 - featureIntensity) * scale;
}
```

#### B. Population Coding
```cpp
// Multiple neurons per orientation with overlapping tuning curves
std::vector<double> encodePopulation(double featureIntensity, int numNeurons) {
    std::vector<double> spikeTimes(numNeurons);
    for (int i = 0; i < numNeurons; ++i) {
        double preferredValue = static_cast<double>(i) / (numNeurons - 1);
        double response = gaussian(featureIntensity, preferredValue, sigma);
        spikeTimes[i] = responseToSpikeTime(response);
    }
    return spikeTimes;
}
```

**Files to Modify**:
- `src/adapters/RetinaAdapter.cpp::encodeFeatures()`
- Add `include/snnfw/encoding/EncodingStrategy.h`
- Add `src/encoding/TemporalEncoder.cpp`
- Add `src/encoding/PopulationEncoder.cpp`

**Expected Impact**: +2-5% accuracy

---

### 2.2 Feature Extraction Methods (MEDIUM-HIGH IMPACT)

**Current**: Hardcoded Sobel-like edge detection

**Proposed Implementations**:

#### A. Gabor Filters (Biologically Realistic)
```cpp
double gaborFilter(const Image& region, int x, int y, double theta, 
                   double lambda, double sigma, double gamma) {
    // Gabor filter: Gaussian envelope × sinusoidal carrier
    double xTheta = x * cos(theta) + y * sin(theta);
    double yTheta = -x * sin(theta) + y * cos(theta);
    
    double gaussian = exp(-(xTheta*xTheta + gamma*gamma*yTheta*yTheta) / (2*sigma*sigma));
    double sinusoid = cos(2 * M_PI * xTheta / lambda);
    
    return gaussian * sinusoid;
}
```

#### B. Difference of Gaussians (DoG)
```cpp
double differenceOfGaussians(const Image& region, int x, int y, 
                             double sigma1, double sigma2) {
    return gaussianBlur(region, x, y, sigma1) - 
           gaussianBlur(region, x, y, sigma2);
}
```

**Files to Modify**:
- `src/adapters/RetinaAdapter.cpp::extractEdgeFeatures()`
- Add `include/snnfw/features/EdgeOperator.h`
- Add `src/features/GaborOperator.cpp`
- Add `src/features/DoGOperator.cpp`

**Expected Impact**: +1-3% accuracy

---

### 2.3 Learning Strategies (MEDIUM IMPACT)

**Current**: Append patterns until max, then stop learning

**Proposed Implementations**:

#### A. Replace Worst Pattern
```cpp
void Neuron::learnPattern(const std::vector<double>& pattern) {
    if (referencePatterns_.size() < maxReferencePatterns_) {
        referencePatterns_.push_back(pattern);
    } else {
        // Find pattern with lowest average similarity to others
        int worstIdx = findLeastRepresentativePattern();
        referencePatterns_[worstIdx] = pattern;
    }
}
```

#### B. Merge Similar Patterns
```cpp
void Neuron::learnPattern(const std::vector<double>& pattern) {
    // Find most similar existing pattern
    auto [bestIdx, bestSim] = findMostSimilarPattern(pattern);
    
    if (bestSim > mergeThreshold) {
        // Merge with existing pattern (weighted average)
        mergePatterns(referencePatterns_[bestIdx], pattern, 0.5);
    } else if (referencePatterns_.size() < maxReferencePatterns_) {
        referencePatterns_.push_back(pattern);
    } else {
        int worstIdx = findLeastRepresentativePattern();
        referencePatterns_[worstIdx] = pattern;
    }
}
```

**Files to Modify**:
- `src/Neuron.cpp::learnCurrentPattern()`
- Add `include/snnfw/learning/PatternUpdateStrategy.h`

**Expected Impact**: +1-2% accuracy

---

### 2.4 Similarity Metrics (LOW-MEDIUM IMPACT)

**Current**: Cosine similarity only

**Proposed Implementations**:

```cpp
enum class SimilarityMetric {
    COSINE,
    EUCLIDEAN,
    MANHATTAN,
    CORRELATION
};

double computeSimilarity(const std::vector<double>& a, 
                        const std::vector<double>& b,
                        SimilarityMetric metric) {
    switch (metric) {
        case SimilarityMetric::COSINE:
            return cosineSimilarity(a, b);
        case SimilarityMetric::EUCLIDEAN:
            return 1.0 / (1.0 + euclideanDistance(a, b));
        case SimilarityMetric::MANHATTAN:
            return 1.0 / (1.0 + manhattanDistance(a, b));
        case SimilarityMetric::CORRELATION:
            return pearsonCorrelation(a, b);
    }
}
```

**Files to Modify**:
- `src/Neuron.cpp::matchPattern()`
- `experiments/mnist_hyperparameter_optimization.cpp::classifyKNN()`

**Expected Impact**: +0.5-1% accuracy

---

### 2.5 Classification Strategies (MEDIUM IMPACT)

**Current**: Simple k-NN with majority voting

**Proposed Implementations**:

#### A. Distance-Weighted Voting
```cpp
int classifyWeightedKNN(const std::vector<double>& testPattern,
                       const std::vector<TrainingPattern>& trainingPatterns,
                       int k, double distanceExponent) {
    std::vector<std::pair<double, int>> similarities;
    for (const auto& pattern : trainingPatterns) {
        double sim = cosineSimilarity(testPattern, pattern.activations);
        similarities.push_back({sim, pattern.label});
    }
    
    std::sort(similarities.begin(), similarities.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    std::vector<double> votes(10, 0.0);
    for (int i = 0; i < k && i < static_cast<int>(similarities.size()); ++i) {
        double weight = std::pow(similarities[i].first, distanceExponent);
        votes[similarities[i].second] += weight;
    }
    
    return std::max_element(votes.begin(), votes.end()) - votes.begin();
}
```

**Files to Modify**:
- `experiments/mnist_hyperparameter_optimization.cpp::classifyKNN()`

**Expected Impact**: +0.5-2% accuracy

---

### 2.6 Preprocessing (LOW-MEDIUM IMPACT)

**Current**: No preprocessing

**Proposed Implementations**:

```cpp
Image preprocessImage(const Image& input, 
                     NormalizationType normType,
                     double contrastFactor) {
    Image result = input;
    
    // Normalization
    switch (normType) {
        case NormalizationType::GLOBAL:
            result = globalNormalize(result);
            break;
        case NormalizationType::LOCAL:
            result = localNormalize(result, windowSize);
            break;
        case NormalizationType::ADAPTIVE:
            result = adaptiveNormalize(result);
            break;
    }
    
    // Contrast enhancement
    if (contrastFactor != 1.0) {
        result = enhanceContrast(result, contrastFactor);
    }
    
    return result;
}
```

**Files to Modify**:
- `src/adapters/RetinaAdapter.cpp::processData()`
- Add `include/snnfw/preprocessing/ImagePreprocessor.h`

**Expected Impact**: +0.5-1.5% accuracy

---

## Phase 3: Implementation Priority

### High Priority (Implement First)
1. ✅ **Extended parameter configuration** (Done)
2. **Encoding strategies** (temporal + population coding)
3. **Feature extraction methods** (Gabor filters)
4. **Classification strategies** (weighted k-NN)

### Medium Priority
5. **Learning strategies** (replace worst, merge similar)
6. **Preprocessing** (normalization, contrast)
7. **Similarity metrics** (euclidean, correlation)

### Low Priority (Nice to Have)
8. **Lateral inhibition** (winner-take-all)
9. **Spatial pooling** (max/average pooling)
10. **Temporal integration** (multi-frame)

---

## Phase 4: Expected Accuracy Progression

| Phase | Features | Expected Accuracy | Cumulative Gain |
|-------|----------|-------------------|-----------------|
| Baseline | Current system | 92.70% | - |
| Phase 1 | Extended parameter tuning | 93-94% | +0.3-1.3% |
| Phase 2 | Encoding strategies | 94-95% | +1.3-2.3% |
| Phase 3 | Gabor filters | 95-96% | +2.3-3.3% |
| Phase 4 | Weighted k-NN | 95-96.5% | +2.3-3.8% |
| Phase 5 | Learning strategies | 96-97% | +3.3-4.3% |
| Phase 6 | All optimizations | 96-98% | +3.3-5.3% |

**Target**: 96-98% accuracy (competitive with other SNN approaches on MNIST)

---

## Phase 5: Implementation Checklist

### Step 1: Create Abstraction Layers
- [ ] `include/snnfw/encoding/EncodingStrategy.h` - Base class for encoding
- [ ] `include/snnfw/features/EdgeOperator.h` - Base class for edge detection
- [ ] `include/snnfw/learning/PatternUpdateStrategy.h` - Base class for learning
- [ ] `include/snnfw/classification/ClassificationStrategy.h` - Base class for classification

### Step 2: Implement Concrete Strategies
- [ ] `src/encoding/RateEncoder.cpp` - Current rate coding
- [ ] `src/encoding/TemporalEncoder.cpp` - Temporal coding
- [ ] `src/encoding/PopulationEncoder.cpp` - Population coding
- [ ] `src/features/SobelOperator.cpp` - Current edge detection
- [ ] `src/features/GaborOperator.cpp` - Gabor filters
- [ ] `src/features/DoGOperator.cpp` - Difference of Gaussians

### Step 3: Modify Existing Components
- [ ] Update `RetinaAdapter` to use pluggable encoding/feature strategies
- [ ] Update `Neuron` to use pluggable learning strategies
- [ ] Update MNIST experiment to use pluggable classification strategies
- [ ] Update `HyperparameterOptimizer` to handle categorical parameters

### Step 4: Testing
- [ ] Unit tests for each encoding strategy
- [ ] Unit tests for each feature extraction method
- [ ] Integration tests for full pipeline
- [ ] Benchmark tests for performance

### Step 5: Optimization
- [ ] Run extended hyperparameter optimization (100-500 iterations)
- [ ] Analyze which parameters have highest impact
- [ ] Fine-tune top configurations
- [ ] Document optimal configuration

---

## Estimated Timeline

- **Phase 1** (Extended config): ✅ Complete
- **Phase 2** (Abstraction layers): 2-3 days
- **Phase 3** (Encoding strategies): 2-3 days
- **Phase 4** (Feature extraction): 2-3 days
- **Phase 5** (Classification): 1-2 days
- **Phase 6** (Learning strategies): 1-2 days
- **Phase 7** (Testing): 2-3 days
- **Phase 8** (Optimization runs): 1-2 days (compute time)

**Total**: ~2-3 weeks for full implementation

---

## Next Steps

1. **Immediate**: Implement encoding strategy abstraction
2. **Short-term**: Add Gabor filters and temporal coding
3. **Medium-term**: Implement all strategies and run optimization
4. **Long-term**: Analyze results and publish findings

Would you like me to start implementing any of these phases?

