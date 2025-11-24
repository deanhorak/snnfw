# Knowledge Transfer: MNIST V1 Multi-Column Architecture Optimization

**Date:** 2025-11-01  
**Project:** Biologically Plausible Spiking Neural Network Framework (SNNFW)  
**Experiment:** MNIST Digit Classification with Multi-Column V1 Architecture  
**Current Status:** 86.52% accuracy (target: >90%)

---

## Executive Summary

We are developing a biologically plausible spiking neural network (SNN) for MNIST digit classification using a multi-column V1 cortical architecture. The system has progressed from 69.56% to **86.52% accuracy** through systematic optimization of architecture, features, and learning parameters. We recently implemented **lateral inhibition** between cortical columns, which showed promising results (+0.31% improvement). However, we need to reach **>90% accuracy** to meet project goals.

**Critical Insight:** Parameter tuning alone cannot break through the 86% plateau. Architectural changes (lateral inhibition, specialized detectors, winner-take-all dynamics) are required to reach high 90% accuracy.

---

## Project Architecture

### Canonical Cortical Microcircuit (6-Layer V1 Model)

Each cortical column implements the standard mammalian cortical circuit:

```
Input (Gabor-filtered features)
    ↓
Layer 4 (Granular, 8×8=64 neurons) - Thalamic input, spatial grid
    ↓
Layer 2/3 (Superficial Pyramidal, 256 neurons) - Feature integration, lateral connections
    ↓
Layer 5 (Deep Pyramidal, 64 neurons) - Output layer
    ↓
Layer 6 (Corticothalamic, 32 neurons) - Feedback to Layer 4
    ↑_______________|
```

**Layer 1** (32 neurons): Modulatory, apical dendrites  
**Total neurons per column:** 448 neurons

### Current Architecture (56 Columns)

**24 Orientation Columns:**
- 12 orientations × 2 spatial frequencies (low=8.0, high=3.0)
- Gabor filters: 9×9 kernel, σ=2.5, γ=0.5

**8 Center-Surround Columns:**
- 4 scales × 2 types (ON-center, OFF-center)
- Difference of Gaussians (DoG) for loop/hole detection
- Scales: (0.7,2.0), (1.2,3.5), (2.0,5.0), (3.0,7.0)

**8 Blob Detector Columns:**
- 4 scales × 2 polarities (positive, negative)
- Gaussian kernels for solid region detection
- Scales: 0.8, 1.5, 2.5, 4.0

**16 Specialized Detector Columns:**
1. **Top-Loop Detectors (2):** Ring pattern in upper-right quadrant (4→9, 7→9)
2. **Gap Detectors (2):** Horizontal gap in top-center (4→9)
3. **Bottom-Curve Detectors (2):** Curved stroke in bottom-left (2→0)
4. **Horizontal-Bar Detectors (2):** Horizontal bar in top region (3→5)
5. **Middle-Constriction Detectors (2):** Figure-8 waist (8→0)
6. **Diagonal-Stroke Detectors (2):** Diagonal line top-left to bottom-right (7→9) - NEW
7. **Closed-Loop Detectors (2):** Complete circular enclosure (4→9, 6→0) - NEW
8. **Vertical-Stem Detectors (2):** Vertical stem on right side (4→6) - NEW

### Lateral Connectivity (RECENTLY IMPLEMENTED)

**Dual Mechanism in Layer 2/3:**
- **Excitatory connections (weight=+0.25):** Between similar feature types (cooperative)
- **Inhibitory connections (weight=-0.4):** Between different feature types (competitive/winner-take-all)
- Connectivity: 15% sparse, ±2 neighbor range
- Purpose: Create competition between feature detectors for better discrimination

**Recurrent Connections (Layer 2/3):**
- 15% self-connectivity within each column
- Weight: 0.4, Delay: 2.0ms
- Purpose: Temporal integration and sustained activity

### Output Layer

- **Population coding:** 20 neurons per digit (200 total)
- **Connectivity:** Layer 5 → Output (30% sparse, weight=1.0)
- **Classification:** Winner-take-all based on spike count

---

## Optimized Parameters

### Neuron Parameters
- **Window size:** 100ms (temporal integration window)
- **Similarity threshold:** 0.85 (pattern matching threshold)
- **Max patterns:** 400 (pattern storage capacity per neuron)
- **Similarity metric:** Histogram-based

### STDP (Spike-Timing-Dependent Plasticity)
- **a_plus:** 0.020 (potentiation strength)
- **a_minus:** 0.015 (depression strength)
- **tau_plus/minus:** 20.0ms (time constants)

### Training
- **Examples per digit:** 6,000 (increased from 4,000)
- **Test images:** 10,000
- **Training time:** ~2 hours on 24-core system

---

## Optimization Journey

### Phase 1-5: Parameter Optimization (69.56% → 70.97%)
- Column count: 12 optimal
- Layer 2/3 neurons: 256 optimal
- STDP: a_plus=0.020, a_minus=0.015
- Similarity threshold: 0.85
- Window size: 100ms

### Phase 6-7: Multi-Modal Features (70.97% → 78.16%, +7.19%)
**Key insight from user:** "Need additional context beyond simple orientation features"
- Implemented 3 spatial frequencies × 12 orientations = 36 columns
- Major breakthrough in feature representation

### Phase 8-10: Confusion Analysis & Failed Curvature Filters (78.16% → 76.71%)
- Implemented confusion matrix tracking
- Attempted curvature filters: **FAILED** (don't work for pixelated MNIST)
- Top confusions identified: 4→9, 2→3, 7→9, 8→6

### Phase 11-12: Center-Surround & Blob Detectors (76.71% → 80.17%, +3.46%)
- Added DoG filters for loop/hole detection
- Added blob detectors for solid regions
- 40 columns total (24 orientation + 8 CS + 8 blob)

### Phase 13-14: Recurrent & Spatial Pooling (80.17%, no change)
- Recurrent connections: No improvement
- Spatial pooling: No improvement

### Phase 15-16: Training Data & Catastrophic Interference (80.17% → 63.40% → 82.82%)
- Increased training from 4,000 to 6,000 examples: **CATASTROPHIC FAILURE** (-16.77%)
- Root cause: max_patterns=200 too low
- Fix: Increased max_patterns to 400: **RECOVERED** to 82.82% (+2.65%)

### Phase 17-18: Specialized Detectors (82.82% → 86.21%, +3.39%)
- Added confusion-targeted detectors (top-loop, gap, bottom-curve, horizontal-bar, middle-constriction)
- 50 columns total
- Achieved 86.21% accuracy

### Phase 19-21: Failed Parameter Tuning (86.21%, plateau)
Attempted without success:
- ❌ Strengthening detectors: 85.71% (-0.50%)
- ❌ Multi-scale detectors: 85.97% (-0.24%)
- ❌ Dual-loop detector: 86.04% (-0.17%)
- ❌ Increase max_patterns to 500: 86.04% (no change)
- ❌ STDP adjustment: 86.21% (no change)
- ❌ Similarity threshold 0.88: 86.21% (no change)

**Conclusion:** System at local optimum. Parameter tuning cannot break through 86.21%.

### Phase 22: Lateral Inhibition (CURRENT, 86.21% → 86.52%, +0.31%)
- Implemented dual excitatory/inhibitory lateral connections
- Added 6 new specialized detectors (diagonal-stroke, closed-loop, vertical-stem)
- 56 columns total
- **Digit 9 improved dramatically:** 81.2% → 89.2% (+8.0%)
- **Digit 8 regressed:** 81.2% → 75.8% (-5.4%)
- **Digit 7 regressed:** 82.8% → 79.3% (-3.5%)

---

## Current Performance Analysis (86.52% Accuracy)

### Per-Digit Accuracy
| Digit | Accuracy | Status | Notes |
|-------|----------|--------|-------|
| 0 | 96.0% | ✅ Excellent | Improved from 94.5% |
| 1 | 97.2% | ✅ Excellent | Consistently high |
| 2 | 83.2% | ⚠️ Needs work | Confused with 3, 8, 7 |
| 3 | 87.7% | ✅ Good | Confused with 5 |
| 4 | 77.4% | ⚠️ Needs work | Major confusion with 9 |
| 5 | 86.4% | ✅ Good | Confused with 3 |
| 6 | 91.8% | ✅ Excellent | Slight regression from 92.9% |
| 7 | 79.3% | ⚠️ Needs work | Regressed, confused with 9 |
| 8 | 75.8% | ❌ Critical | Regressed significantly |
| 9 | 89.2% | ✅ Excellent | Major improvement! |

### Top Confusions (from latest run)
1. **4→9 (136 errors, 13.8%):** Open top vs closed loop - CRITICAL
2. **7→9 (111 errors, 10.8%):** Diagonal stroke vs loop - CRITICAL
3. **5→3 (54 errors, 6.1%):** Horizontal bar vs curves
4. **8→5 (48 errors, 4.9%):** Double loops vs horizontal bar
5. **3→5 (45 errors, 4.5%):** Curves vs horizontal bar
6. **8→3 (43 errors, 4.4%):** Double loops vs curves
7. **8→9 (42 errors, 4.3%):** Double loops vs single loop
8. **2→3 (42 errors, 4.1%):** Tail vs curves

### Key Observations
- **Lateral inhibition is working** but may be too strong (weight=-0.4)
- **Digit 9 benefited** from closed-loop detector and inhibition
- **Digits 7 and 8 suffered** from over-suppression
- **4→9 confusion remains the biggest problem** (13.8% of digit 4 images)

---

## Technical Implementation Details

### File Structure
- **Main experiment:** `experiments/mnist_v1_multicolumn.cpp` (2624 lines)
- **Configuration:** `configs/mnist_v1_multicolumn_optimized.json`
- **Build directory:** `/home/dean/repos/snnfw/build`
- **Compilation:** `make mnist_v1_multicolumn -j24`

### Key Code Sections

**Lateral Connections (lines 1782-1843):**
```cpp
// Dual mechanism: excitatory for similar features, inhibitory for competing features
bool similarFeatures = (corticalColumns[i].featureType == corticalColumns[j].featureType);
double weight = similarFeatures ? 0.25 : -0.4;  // Excitatory vs Inhibitory
```

**Specialized Detector Kernels (lines 200-480):**
- `createTopLoopKernel()`: Ring pattern detector
- `createGapKernel()`: Horizontal gap detector
- `createBottomCurveKernel()`: Bottom curve detector
- `createHorizontalBarKernel()`: Horizontal bar detector
- `createMiddleConstrictionKernel()`: Figure-8 waist detector
- `createDiagonalStrokeKernel()`: Diagonal line detector (NEW)
- `createClosedLoopKernel()`: Circular enclosure detector (NEW)
- `createVerticalStemKernel()`: Vertical stem detector (NEW)

### Performance
- **Training time:** ~2 hours (6,000 examples/digit)
- **Testing time:** ~15 minutes (10,000 images)
- **Memory:** ~2.5GB peak
- **CPU:** 24 threads, 107% utilization

---

## Goals and Requirements

### Primary Goal
**Achieve >90% accuracy on MNIST** using biologically plausible mechanisms

**User's explicit requirement:** "we must reach accuracy into the high 90% range or this is pointless"

### Biological Plausibility Constraints
- ✅ Spiking neurons with temporal dynamics
- ✅ STDP learning (no backpropagation)
- ✅ Canonical cortical microcircuit (6 layers)
- ✅ Lateral connections and recurrence
- ✅ Population coding
- ❌ NO fake code or hardcoded results (zero tolerance)

---

## Recommendations for Continued Progress

### Immediate Next Steps (Priority 1)

**1. Tune Lateral Inhibition Strength**
- **Current:** weight=-0.4 (too strong, hurting digits 7 and 8)
- **Recommendation:** Try weight=-0.25 or -0.3
- **Expected impact:** Reduce over-suppression while maintaining competition
- **File to edit:** `experiments/mnist_v1_multicolumn.cpp`, line ~1827

**2. Implement Adaptive Inhibition**
- **Concept:** Inhibition strength varies by feature type
- **Example:** Specialized detectors get weaker inhibition than orientation detectors
- **Rationale:** Specialized detectors are already rare/specific, don't need strong suppression

**3. Add More Specialized Detectors for Critical Confusions**
- **4→9 (13.8% errors):** Need stronger open-top detector
- **7→9 (10.8% errors):** Diagonal-stroke detector may need tuning
- **8→5, 8→3 (4.9%, 4.4% errors):** Digit 8 needs better double-loop detector

### Medium-Term Improvements (Priority 2)

**4. Implement Winner-Take-All Dynamics**
- **Current:** Lateral inhibition is passive (fixed weights)
- **Proposed:** Active WTA where strongest columns suppress weaker ones
- **Implementation:** Track column activation levels, dynamically adjust inhibition
- **Expected impact:** +2-3% accuracy

**5. Temporal Dynamics Enhancement**
- **Current:** Static pattern matching
- **Proposed:** Temporal sequences of features (e.g., stroke order)
- **Implementation:** Multi-timescale STDP, sequence learning
- **Expected impact:** +1-2% accuracy

**6. Ensemble Methods**
- **Approach:** Train multiple networks with different random seeds
- **Combination:** Voting or confidence-weighted averaging
- **Expected impact:** +2-4% accuracy (but less biologically plausible)

### Long-Term Architectural Changes (Priority 3)

**7. Hierarchical Feature Learning**
- **Current:** Single V1 layer
- **Proposed:** Add V2/V4 layers for higher-level features
- **Rationale:** Biological vision systems use hierarchical processing
- **Expected impact:** +5-10% accuracy (major undertaking)

**8. Attention Mechanisms**
- **Concept:** Top-down modulation of feature detectors
- **Implementation:** Layer 6 → Layer 4 feedback with learned weights
- **Expected impact:** +3-5% accuracy

**9. Data Augmentation During Training**
- **Current:** Raw MNIST images
- **Proposed:** Slight rotations (±5°), translations (±2 pixels)
- **Rationale:** Improve generalization
- **Expected impact:** +2-3% accuracy

---

## Known Issues and Limitations

### Critical Issues
1. **Digit 8 accuracy dropped to 75.8%** after lateral inhibition (was 81.2%)
2. **Digit 7 accuracy dropped to 79.3%** after lateral inhibition (was 82.8%)
3. **4→9 confusion remains very high** (13.8% of digit 4 images)

### Technical Limitations
1. **Pattern capacity:** max_patterns=400 may still be limiting for complex digits
2. **Spatial resolution:** 8×8 grid may be too coarse for fine details
3. **No temporal sequences:** System doesn't learn stroke order
4. **Fixed architecture:** Column types are hand-designed, not learned

### Biological Plausibility Trade-offs
1. **Specialized detectors:** Hand-crafted kernels are less plausible than learned features
2. **Population coding:** Simple spike-count voting is oversimplified
3. **No feedback learning:** Layer 6 → Layer 4 connections exist but don't learn

---

## Testing and Validation

### How to Run Experiments

**Compile:**
```bash
cd /home/dean/repos/snnfw/build
make mnist_v1_multicolumn -j24
```

**Run with timing:**
```bash
cd /home/dean/repos/snnfw/build
LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH \
time ./mnist_v1_multicolumn ../configs/mnist_v1_multicolumn_optimized.json \
2>&1 | tee /tmp/mnist_run.log | tail -120
```

**Expected output:**
- Training progress (6,000 examples × 10 digits)
- Testing progress (10,000 images)
- Confusion matrix
- Per-digit accuracy
- Overall accuracy

### Configuration Files
- **Main config:** `configs/mnist_v1_multicolumn_optimized.json`
- **Key parameters:** neuron.similarity_threshold, neuron.max_patterns, stdp.a_plus, stdp.a_minus

---

## Context for New Agent

### What Works Well
- ✅ Multi-modal feature extraction (orientation + center-surround + blob + specialized)
- ✅ Specialized confusion-targeted detectors (bottom-curve, horizontal-bar work great)
- ✅ STDP learning with optimized parameters
- ✅ Lateral inhibition concept (needs tuning)
- ✅ High accuracy on digits 0, 1, 6, 9

### What Needs Work
- ⚠️ Lateral inhibition strength (too strong, hurting digits 7 and 8)
- ⚠️ Digit 4 vs 9 confusion (13.8% error rate)
- ⚠️ Digit 8 accuracy (75.8%, needs improvement)
- ⚠️ Digit 7 accuracy (79.3%, needs improvement)

### User Preferences
- User wants **>90% accuracy** (non-negotiable)
- User values **biological plausibility** (no backprop, no fake code)
- User is willing to try **architectural changes** (not just parameter tuning)
- User has been patient through 22 phases of optimization

### Immediate Action Required
**Reduce lateral inhibition strength from -0.4 to -0.25** and test. This is the most promising next step based on current results.

---

**End of Knowledge Transfer Document**

