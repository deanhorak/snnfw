# MNIST Full Dataset Test Results

## Summary

Tested the pattern-based learning approach on the full MNIST dataset with disappointing results: **11.4% accuracy** on 1000 test images after training on 100 examples per digit.

## Configuration

- **Training**: 100 examples per digit (1000 total)
- **Testing**: 1000 images from test set
- **Neuron parameters**:
  - Window size: 50ms
  - Similarity threshold: 0.7
  - Max patterns: 20
- **Recognition strategy**: Winner-takes-all based on best similarity score

## Results

### Overall Accuracy: 11.4% (114/1000)

### Per-Digit Accuracy:
```
Digit 0: 16.4% (11/67)
Digit 1: 72.7% (32/44)  ← Only digit with good accuracy!
Digit 2:  9.7% (9/93)
Digit 3:  7.3% (6/82)
Digit 4: 13.8% (12/87)
Digit 5: 14.3% (10/70)
Digit 6:  6.1% (4/66)
Digit 7: 12.3% (10/81)
Digit 8:  9.1% (6/66)
Digit 9: 17.9% (14/78)
```

### Confusion Matrix:
```
     0    1    2    3    4    5    6    7    8    9
0:  11    0    9    6    7    7    7    5    9    6
1:   0   32    0    0    1    4    1    6    0    0  ← Good!
2:  11    2    9   10    5   12   11    8   11   14
3:   4    1    8    6   11   11   12   11    6   12
4:  10    8    3    4   12   13    9   14    7    7
5:   8    1    7    4    8   10   11    4    8    9
6:   7    4    6    9    8    6    4    8    8    6
7:   5   10    2    6   17   10    6   10    5   10
8:  11    3    4   10    6    7    6    5    6    8
9:   0    5    4    6   14    4   10   15    6   14
```

**Observation**: Predictions are nearly random except for digit '1'!

## Performance

- **Training time**: 152 ms (6,579 images/sec)
- **Testing time**: 1,783 ms (561 images/sec)
- **Total time**: ~2 seconds

## Root Cause Analysis

### Problem 1: Exact Spike Count Requirement

The current pattern matching requires **exact spike count matches**:

```cpp
if (spikes.size() == refPattern.size()) {
    double similarity = cosineSimilarity(spikes, refPattern);
    ...
}
```

**Why this fails**:
- Different images of the same digit have different spike counts
- Digit '1' (simple): 58-91 spikes
- Digit '8' (complex): 146-205 spikes
- Even two '1' images: 89 vs 91 spikes → **NO MATCH!**

**Why digit '1' works better**:
- Simpler shape → more consistent spike counts
- Narrower range of spike counts (58-91)
- Higher chance of exact matches

### Problem 2: Rate Coding Variability

Our rate coding maps pixel intensity to spike timing:
```cpp
spikeTime = duration * (1.0 - intensity)
```

**Issues**:
- Small intensity differences → different spike times
- No temporal binning or tolerance
- Cosine similarity is sensitive to exact timing

### Problem 3: No Normalization

Patterns aren't normalized for:
- Total spike count
- Temporal distribution
- Intensity variations

## Comparison with Debug Test

| Metric | Debug (30 images) | Full Test (1000 images) |
|--------|-------------------|-------------------------|
| **Accuracy** | 73.3% | 11.4% |
| **Training per digit** | 3 examples | 100 examples |
| **Why different?** | Small test set, lucky matches | Real-world variability |

The debug test gave false confidence because:
1. Only 30 images (3 per digit)
2. High chance of exact spike count matches in small set
3. Not representative of full dataset variability

## What We Learned

### ✅ What Works:
1. **Pattern learning mechanism** - Neurons can store temporal patterns
2. **Spike encoding** - Images convert to discriminative spike patterns
3. **Fast training** - 6,579 images/sec
4. **Fast inference** - 561 images/sec

### ❌ What Doesn't Work:
1. **Exact spike count matching** - Too restrictive
2. **No temporal tolerance** - Spike timing must be exact
3. **No normalization** - Patterns not comparable across variations
4. **Direct pixel→digit mapping** - No feature extraction

## Solutions to Explore

### 1. Flexible Pattern Matching

**Option A: Spike Count Tolerance**
```cpp
// Allow ±10% spike count difference
if (abs(spikes.size() - refPattern.size()) / refPattern.size() < 0.1) {
    // Use DTW or elastic matching
}
```

**Option B: Normalize Spike Counts**
```cpp
// Resample patterns to fixed length
auto normalized = resamplePattern(spikes, FIXED_LENGTH);
```

**Option C: Temporal Binning**
```cpp
// Bin spikes into time windows (e.g., 5ms bins)
auto binned = binSpikes(spikes, BIN_SIZE);
```

### 2. Better Similarity Metrics

- **Dynamic Time Warping (DTW)**: Handles temporal variations
- **Earth Mover's Distance**: Robust to spike count differences
- **Spike train kernels**: Designed for temporal spike patterns

### 3. Hierarchical Feature Learning

Instead of direct pixel→digit:
```
Pixels → Edge detectors → Shape features → Digits
```

Add hidden layer to learn intermediate features.

### 4. Population Coding

Instead of single neuron per digit:
- Multiple neurons per digit
- Voting mechanism
- Ensemble predictions

### 5. Spike Normalization

Normalize patterns before learning:
- Scale to fixed duration
- Normalize spike density
- Remove outliers

## Recommendations

### Immediate (Quick Wins):

1. **Add temporal binning** to reduce sensitivity to exact timing
2. **Relax spike count requirement** to ±20% tolerance
3. **Normalize patterns** to fixed length before comparison

### Medium-term:

1. **Implement DTW** for flexible temporal matching
2. **Add hidden layer** for feature extraction
3. **Use population coding** (multiple neurons per digit)

### Long-term:

1. **Implement full STDP** with spike propagation through network
2. **Add lateral inhibition** for winner-takes-all dynamics
3. **Explore unsupervised learning** to discover features

## Conclusion

The pattern-based learning approach **works in principle** but needs more flexible pattern matching to handle real-world variability. The current exact-match requirement is too strict for MNIST.

**Key insight**: The 73% accuracy on the debug set was misleading - it worked because of lucky exact matches in a small dataset, not because the approach generalizes well.

**Next step**: Implement temporal binning or spike count normalization to make pattern matching more robust.

## Code Location

- **Experiment**: `experiments/mnist_full_test.cpp`
- **Pattern learning**: `src/Neuron.cpp` (`shouldFire()`, `getBestSimilarity()`)
- **MNIST loader**: `src/MNISTLoader.cpp`
- **Debug experiments**: `experiments/mnist_debug_retina.cpp`, `experiments/mnist_debug_learning.cpp`

## Performance Baseline

This establishes a baseline for future improvements:
- **Current**: 11.4% accuracy (barely better than random 10%)
- **Target**: >80% accuracy (competitive with simple SNNs)
- **State-of-art SNN**: ~99% accuracy (with proper architecture)

The gap shows we need significant improvements in pattern matching and/or network architecture.

