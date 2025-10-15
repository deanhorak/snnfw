# MNIST Digit Recognition Progress Summary

## Overview
Systematic exploration of spike-based pattern matching for MNIST digit recognition using the SNNFW framework.

## Approaches Tested

### 1. Exact Spike Count Matching (Baseline)
- **Accuracy**: 11.4% (114/1000)
- **Method**: Required exact spike count match before computing cosine similarity
- **Problem**: Too restrictive - different images of same digit have different spike counts
- **Best digit**: Digit 1 at 72.7% (simple, consistent spike patterns)

### 2. Fuzzy Matching Attempts

#### 2a. Temporal Binning with Histogram
- **Accuracy**: 17%
- **Method**: 20 bins, cosine similarity on histograms
- **Problem**: All similarities extremely high (0.92-0.98), non-discriminative

#### 2b. CDF with Curvature
- **Accuracy**: 11%
- **Method**: CDF + second derivatives
- **Problem**: Wrong features captured, similarities too close

#### 2c. Hybrid CDF Approach
- **Accuracy**: 10%
- **Method**: CDF similarity + first derivative + L1 distance
- **Problem**: Similarities bunched together (0.61-0.73)

### 3. Spatial SDRs
- **Accuracy**: 15% (15/100)
- **Method**: 
  - Divided 28x28 image into 4x4 grid (16 regions of 7x7 pixels)
  - Each digit has 16 neurons (one per region)
  - Match based on average similarity across regions
- **Problem**: Similarities still too close (0.386-0.520)
- **Insight**: Preserving spatial structure helps slightly but not enough

### 4. Edge Features (V1-like)
- **Accuracy**: 16% (16/100)
- **Method**:
  - 4 orientations per region (H, V, D1, D2)
  - 64 neurons per digit (16 regions × 4 orientations)
  - Mimics biological V1 simple cells
- **Problem**: Similarities still clustered (0.433-0.527)
- **Insight**: Feature extraction alone doesn't solve the similarity metric problem

### 5. Two-Layer Network (with CDF similarity)
- **Accuracy**: 12% (12/100)
- **Method**:
  - Layer 1: 64 feature neurons (shared across digits)
  - Layer 2: 10 integration neurons (one per digit)
  - Layer 2 learns spike patterns from Layer 1 activations
- **Problem**: Converting activations to spikes loses information

### 6. Spike Distance Metric (Victor-Purpura-like)
- **Accuracy**: 9% (9/100)
- **Method**: Replaced CDF similarity with spike distance metric
- **Problem**: More discriminative (0.12-1.0 range) but worse accuracy
- **Insight**: The metric is better, but the spike encoding approach is flawed

### 7. Activation Pattern Matching ✅ **BEST RESULT**
- **Accuracy**: 34% (34/100)
- **Method**:
  - Layer 1: 64 feature neurons using spike-based pattern matching
  - Layer 2: Direct cosine similarity on 64-dimensional activation vectors
  - No spike encoding for Layer 2
- **Per-Digit Results**:
  - Digit 1: **92.9%** (13/14) ⭐
  - Digit 6: 50.0% (5/10)
  - Digit 8: 50.0% (1/2)
  - Digit 7: 33.3% (5/15)
  - Digit 0: 37.5% (3/8)
  - Digit 9: 27.3% (3/11)
  - Digit 4: 21.4% (3/14)
  - Digit 2: 12.5% (1/8)
  - Digit 3: 0.0% (0/11)
  - Digit 5: 0.0% (0/7)
- **Training**: 30 examples per digit
- **Similarity spread**: 0.209-0.581 (37% range - much better!)

## Key Insights

### What Works ✅
1. **Spike-based pattern matching for local features** (Layer 1)
2. **Direct vector similarity for integration** (Layer 2)
3. **Spatial structure preservation** (region-based processing)
4. **Edge-based features** (orientation-selective neurons)
5. **Hybrid approach**: Spikes for temporal patterns, vectors for high-dimensional matching

### What Doesn't Work ❌
1. **Exact spike count requirements** - too restrictive
2. **CDF-based similarity** - produces non-discriminative scores (all too similar)
3. **Converting activation vectors to spikes** - loses information
4. **Single-layer direct pixel→digit mapping** - insufficient abstraction

### Root Cause Analysis
The fundamental problem with approaches 1-6 was that **similarity metrics on spike patterns were not discriminative enough**:
- CDF-based approaches: All similarities 0.7-0.9 (too close)
- Spike distance: Better spread but wrong encoding
- The issue: Spike timing encodes temporal information well, but MNIST digits are spatial patterns

**Solution**: Use spikes for what they're good at (local temporal patterns in edge detection), but use vector similarity for high-dimensional pattern matching.

## Architecture of Best Approach

```
Input Image (28x28)
    ↓
[16 Regions (7x7 each)]
    ↓
[Edge Detection: 4 orientations per region]
    ↓
[Spike Encoding: Feature strength → spike timing]
    ↓
[Layer 1: 64 Neurons with spike pattern matching]
    ↓
[64-dimensional Activation Vector]
    ↓
[Layer 2: Cosine similarity against learned patterns]
    ↓
Output: Digit classification
```

## Performance Comparison

| Approach | Accuracy | Similarity Spread | Key Issue |
|----------|----------|-------------------|-----------|
| Exact match | 11.4% | N/A | Too restrictive |
| CDF fuzzy | 10-17% | 5-12% | Non-discriminative |
| Spatial SDR | 15% | 13% | Still too similar |
| Edge features | 16% | 9% | Metric problem |
| Two-layer (CDF) | 12% | 12% | Info loss |
| Spike distance | 9% | 88% | Wrong encoding |
| **Activation matching** | **34%** | **37%** | **Works!** |

## Next Steps to Improve

1. **More training data**: Currently 30 examples/digit, could use 100+
2. **Better features**: 
   - More orientations (8 instead of 4)
   - Multiple scales (different region sizes)
   - Gabor filters instead of simple edge detection
3. **Normalization**: Normalize activation vectors before comparison
4. **Ensemble methods**: Combine multiple pattern matchers
5. **Temporal dynamics**: Use actual STDP learning instead of pattern storage
6. **Deeper hierarchy**: Add more layers (V1 → V2 → V4 → IT)

## Biological Plausibility

**High**:
- Layer 1 uses spike-based pattern matching (like V1 neurons)
- Edge detection with orientation selectivity (V1 simple cells)
- Spatial receptive fields (retinotopic organization)

**Medium**:
- Pattern storage mechanism (simplified from STDP)
- Spike distance metric (inspired by neuroscience)

**Low**:
- Direct vector similarity in Layer 2 (not spike-based)
- Cosine similarity (mathematical, not biological)

**Trade-off**: Pure spike-based approaches (1-6) were more biologically plausible but performed poorly. The hybrid approach (7) sacrifices some biological realism for practical performance.

## Conclusion

We successfully demonstrated that:
1. **Spike-based pattern matching can work for MNIST** (34% accuracy, 93% on digit 1)
2. **The key is using spikes appropriately**: for local temporal patterns, not high-dimensional vectors
3. **Hierarchical processing helps**: Layer 1 for features, Layer 2 for integration
4. **The similarity metric matters more than the encoding**: CDF was the bottleneck, not the spike representation

The 34% accuracy with 30 training examples per digit shows promise. With more training data and better features, this approach could potentially reach 60-70% accuracy while maintaining biological plausibility in the feature detection layer.

