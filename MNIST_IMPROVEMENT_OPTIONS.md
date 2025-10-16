# MNIST Accuracy Improvement Options

## Current State: 53.8% Accuracy

### Strong Performance (>80%):
- **Digit 0: 90.6%** - Excellent
- **Digit 9: 90.4%** - Excellent  
- **Digit 1: 83.3%** - Very good

### Moderate Performance (60-80%):
- **Digit 8: 74.2%** - Good
- **Digit 6: 66.7%** - Acceptable

### Weak Performance (<50%):
- **Digit 3: 41.1%** - Poor (confused with 8: 26 times, 0: 21 times)
- **Digit 4: 36.4%** - Poor (confused with 9: 44 times!)
- **Digit 2: 28.5%** - Very poor (confused with 0: 26 times, 8: 23 times)
- **Digit 7: 28.3%** - Very poor (confused with 9: 44 times!)
- **Digit 5: 2.3%** - Critical failure (confused with 8: 49 times!)

## Major Confusion Patterns

1. **5 → 8** (49 cases): Similar curved shapes, both have loops
2. **7 → 9** (44 cases): Similar top structure and vertical stroke
3. **4 → 9** (44 cases): Both have vertical strokes and angular features
4. **2 → 0** (26 cases): Both have curved bottoms
5. **3 → 8** (26 cases): Both have stacked curves
6. **2 → 8** (23 cases): Similar curved structures
7. **3 → 0** (21 cases): Curved shapes

## Improvement Options

### Option 1: More Training Data ⭐⭐⭐
**Effort**: Low | **Expected Gain**: +5-10%

Currently using only 100 examples per digit (out of ~6000 available).

**Action**:
- Increase to 500-1000 training examples per digit
- Test if more patterns improve generalization

**Pros**:
- Simple to implement (change one constant)
- Likely to help with confused digits
- More reference patterns = better matching

**Cons**:
- Slower training time
- Diminishing returns after certain point

---

### Option 2: Finer Spatial Resolution ⭐⭐
**Effort**: Low | **Expected Gain**: +3-7%

Current: 7×7 grid (4×4 pixel regions, 392 features)
Hyperparameter search showed 7×7 was much better than 4×4.

**Action**:
- Try 14×14 grid (2×2 pixel regions, 1568 features)
- Or 28×28 grid (1×1 pixel, 6272 features)

**Pros**:
- Captures finer details
- May help distinguish similar digits (5 vs 8, 7 vs 9)

**Cons**:
- Much higher dimensionality
- Slower computation
- May overfit to noise

---

### Option 3: Discriminative Features for Confused Pairs ⭐⭐⭐⭐
**Effort**: Medium | **Expected Gain**: +10-15%

Add specialized feature detectors for problematic pairs:

**5 vs 8 discriminator**:
- Top-left corner detector (5 has horizontal stroke, 8 has curve)
- Bottom loop detector (8 has two loops, 5 has one)

**7 vs 9 discriminator**:
- Bottom-left detector (9 has vertical stroke, 7 doesn't)
- Top horizontal stroke detector (7 has it, 9 doesn't)

**4 vs 9 discriminator**:
- Left vertical stroke detector (4 has gap, 9 is continuous)
- Top-right closure detector (9 is closed, 4 is open)

**Action**:
- Add region-specific feature extractors
- Weight certain regions more heavily for specific digit pairs
- Use topology features (holes, endpoints, junctions)

**Pros**:
- Directly addresses the main failure modes
- Biologically inspired (specialized detectors)
- High impact on worst-performing digits

**Cons**:
- Requires manual feature engineering
- May not generalize to other datasets

---

### Option 4: Multi-Scale Features ⭐⭐⭐
**Effort**: Medium | **Expected Gain**: +5-10%

Process image at multiple grid resolutions simultaneously:
- Coarse: 4×4 grid (global structure)
- Medium: 7×7 grid (current)
- Fine: 14×14 grid (local details)

**Action**:
- Extract features at all three scales
- Concatenate into single activation vector
- Total features: 64 + 392 + 1568 = 2024

**Pros**:
- Captures both global shape and local details
- Biologically plausible (V1 has multiple receptive field sizes)
- May help with scale-variant confusions

**Cons**:
- Higher dimensionality
- More computation
- Need to balance scales

---

### Option 5: Topological Features ⭐⭐⭐⭐⭐
**Effort**: High | **Expected Gain**: +15-20%

Add features based on topology:
- **Hole counting**: 0,4,6,8,9 have holes; 1,2,3,5,7 don't
- **Endpoint counting**: 1 has 2 endpoints, 5 has 2, 7 has 2, etc.
- **Junction counting**: 4 has a T-junction, 5 has corners
- **Stroke counting**: Number of connected components
- **Euler characteristic**: Topological invariant

**Action**:
- Implement connected component analysis
- Add hole detection (flood fill from outside)
- Count endpoints (pixels with 1 neighbor)
- Count junctions (pixels with 3+ neighbors)

**Pros**:
- **Extremely discriminative** for key confusions:
  - 5 vs 8: 8 has 2 holes, 5 has 0-1
  - 7 vs 9: 9 has 1 hole, 7 has 0
  - 4 vs 9: 9 has 1 hole, 4 has 0
- Rotation and scale invariant
- Biologically plausible (shape perception)

**Cons**:
- More complex to implement
- Sensitive to noise and broken strokes
- May need preprocessing (thinning, denoising)

---

### Option 6: Temporal Dynamics (True STDP) ⭐⭐
**Effort**: Very High | **Expected Gain**: +5-10%

Currently we're not using true temporal dynamics - just pattern matching.

**Action**:
- Implement real STDP weight updates
- Use spike timing for learning
- Add lateral inhibition (winner-take-all)
- Implement homeostatic plasticity

**Pros**:
- More biologically realistic
- May discover features automatically
- Could generalize better

**Cons**:
- Very complex to implement correctly
- Requires careful tuning
- May not outperform engineered features
- Slower training

---

### Option 7: Ensemble Methods ⭐⭐⭐
**Effort**: Medium | **Expected Gain**: +8-12%

Train multiple classifiers and combine predictions:

**Action**:
- Train with different random subsets of training data
- Train with different grid sizes
- Train with different orientation sets
- Combine via voting or weighted average

**Pros**:
- Often improves accuracy significantly
- Reduces overfitting
- Can combine different feature types

**Cons**:
- Multiple models = more memory/computation
- Diminishing returns after 3-5 models

---

### Option 8: Better Similarity Metric ⭐⭐⭐
**Effort**: Medium | **Expected Gain**: +5-8%

Current: Simple cosine similarity on activation vectors

**Alternatives**:
- **Learned metric**: Train a small neural network to compute similarity
- **Mahalanobis distance**: Account for feature correlations
- **Weighted cosine**: Weight features by discriminative power
- **k-NN with k>1**: Use multiple nearest neighbors

**Action**:
- Analyze which features are most discriminative
- Weight them more heavily in similarity computation
- Or use k=3 or k=5 nearest neighbors instead of k=1

**Pros**:
- Can significantly improve discrimination
- Relatively easy to implement weighted version

**Cons**:
- Learned metrics require more data
- May overfit

---

### Option 9: Data Augmentation ⭐⭐⭐⭐
**Effort**: Medium | **Expected Gain**: +10-15%

Generate synthetic training examples:

**Action**:
- Small rotations (±5-10 degrees)
- Small translations (±2 pixels)
- Elastic deformations (common for MNIST)
- Scaling variations (±10%)

**Pros**:
- Dramatically increases effective training set size
- Improves generalization
- Helps with variations in test set

**Cons**:
- Requires image transformation code
- Slower training
- May introduce unrealistic examples

---

### Option 10: Preprocessing ⭐⭐
**Effort**: Low-Medium | **Expected Gain**: +3-5%

**Action**:
- Center digits in frame (center of mass)
- Normalize size (scale to consistent height)
- Denoise (median filter)
- Enhance edges (contrast adjustment)
- Skeletonize (thin to 1-pixel strokes)

**Pros**:
- Reduces variability
- May help with alignment issues

**Cons**:
- Can lose information
- May introduce artifacts

---

## Recommended Strategy

### Phase 1: Quick Wins (1-2 days)
1. **More training data** (Option 1): Change constant, rerun → +5-10%
2. **Better similarity metric** (Option 8): Try k=3 nearest neighbors → +3-5%
3. **Preprocessing** (Option 10): Center and normalize → +2-3%

**Expected total: 60-70% accuracy**

### Phase 2: High-Impact Features (3-5 days)
4. **Topological features** (Option 5): Hole counting, endpoints → +10-15%
5. **Discriminative features** (Option 3): Specialized detectors for confused pairs → +5-10%

**Expected total: 75-85% accuracy**

### Phase 3: Advanced Techniques (1-2 weeks)
6. **Data augmentation** (Option 9): Rotations, translations → +5-10%
7. **Multi-scale features** (Option 4): Multiple grid sizes → +3-5%
8. **Ensemble methods** (Option 7): Combine multiple models → +3-5%

**Expected total: 85-95% accuracy**

---

## Effort vs Impact Matrix

```
High Impact, Low Effort (DO FIRST):
- More training data ⭐⭐⭐
- k-NN similarity ⭐⭐⭐

High Impact, Medium Effort (DO NEXT):
- Topological features ⭐⭐⭐⭐⭐
- Discriminative features ⭐⭐⭐⭐
- Data augmentation ⭐⭐⭐⭐
- Multi-scale features ⭐⭐⭐

Medium Impact, Low Effort (NICE TO HAVE):
- Preprocessing ⭐⭐
- Finer resolution ⭐⭐

Low Priority:
- True STDP (high effort, uncertain gain)
```

---

## Next Steps

**Immediate action**: Implement Phase 1 (quick wins) to get to ~65-70% accuracy.

**Most promising single improvement**: Topological features (Option 5) - directly addresses the main failure modes (5→8, 7→9, 4→9 confusions) with high discriminative power.

