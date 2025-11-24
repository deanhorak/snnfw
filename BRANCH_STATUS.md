# Branch Status: AdvancedFeatureDetection

**Created:** 2025-11-21  
**Parent Branch:** MultiLayeredArchitecture  
**Status:** 🆕 New Branch - Ready for Development

---

## Purpose

This branch is dedicated to exploring advanced feature detection methods to improve EMNIST letters classification beyond the current 71.93% accuracy.

---

## Current State

### Starting Point
- **Accuracy:** 71.93% (EMNIST letters A-Z)
- **Architecture:** 24 cortical columns, 16,542 neurons, 4.9M synapses
- **Configuration:** `configs/emnist_letters_saccades_best_v2.json`
- **Branch:** AdvancedFeatureDetection (clean working tree)

### Previous Branch (MultiLayeredArchitecture)
- ✅ Implemented saccade-based sequential spatial attention
- ✅ Completed 80-trial Optuna optimization
- ✅ Achieved 71.93% accuracy (+4.53% over baseline)
- ✅ Comprehensive documentation and analysis
- ✅ Production-ready configuration

---

## Potential Improvements to Explore

### 1. Additional Feature Columns
**Goal:** Add specialized detectors for difficult letters (G, Q, R)

**Ideas:**
- **Curvature detectors** - Detect curved vs straight strokes
- **Corner detectors** - Detect sharp corners and angles
- **Stroke-width detectors** - Detect thick vs thin strokes
- **Junction detectors** - Detect T-junctions, Y-junctions, X-junctions
- **Endpoint detectors** - Detect stroke endpoints and terminals

**Expected Impact:** +1-3% accuracy improvement

### 2. Improved Saccade Patterns
**Goal:** Better spatial coverage and attention

**Ideas:**
- **More fixations** - 6-8 fixations instead of 4
- **Different regions** - Quadrants, diagonals, specific letter parts
- **Adaptive saccades** - Based on uncertainty or feature density
- **Hierarchical saccades** - Coarse-to-fine attention
- **Letter-specific saccades** - Different patterns for different letters

**Expected Impact:** +0.5-2% accuracy improvement

### 3. Enhanced Learning Rules
**Goal:** Better pattern learning and generalization

**Ideas:**
- **Triplet STDP** - Learn to distinguish similar letters (I/L, G/Q)
- **Reward-modulated STDP** - Strengthen correct classifications
- **Homeostatic plasticity** - Prevent runaway excitation/inhibition
- **Meta-learning** - Learn to learn faster
- **Contrastive learning** - Maximize inter-class distance

**Expected Impact:** +1-2% accuracy improvement

### 4. Ensemble Methods
**Goal:** Combine multiple networks for better accuracy

**Ideas:**
- **Multiple initializations** - Train 3-5 networks with different seeds
- **Different architectures** - Combine different column configurations
- **Voting strategies** - Majority voting, weighted voting, confidence-based
- **Boosting** - Focus on difficult examples
- **Stacking** - Meta-classifier on top of multiple networks

**Expected Impact:** +2-4% accuracy improvement

### 5. Architecture Refinements
**Goal:** Optimize network structure

**Ideas:**
- **Deeper hierarchies** - Add more layers or sub-layers
- **Skip connections** - Direct L4→L5 connections
- **Attention mechanisms** - Learn which columns to attend to
- **Dynamic routing** - Route spikes based on content
- **Modular networks** - Specialized sub-networks for letter groups

**Expected Impact:** +0.5-1.5% accuracy improvement

### 6. Data Augmentation
**Goal:** Improve generalization through training diversity

**Ideas:**
- **Rotation** - Small rotations (±5-10 degrees)
- **Translation** - Small shifts (±2-3 pixels)
- **Scaling** - Small size variations (±10%)
- **Elastic deformations** - Realistic handwriting variations
- **Noise injection** - Gaussian noise, salt-and-pepper

**Expected Impact:** +1-3% accuracy improvement

---

## Difficult Letters to Target

### Worst Performers (<60% accuracy)
1. **G: 37.8%** - Confused with Q, S, C, O, A
2. **Q: 52.1%** - Confused with O, A, G
3. **R: 58.6%** - Confused with E, P

### High Confusion Pairs
1. **I ↔ L** (25.6% / 20.9%) - Need better vertical stroke discrimination
2. **G → Q** (17.4%) - Need better tail/loop detection
3. **Y → V** (14.1%) - Need better junction detection
4. **D → O** (13.9%) - Need better vertical stroke detection
5. **F → P** (12.1%) - Need better horizontal stroke position

---

## Recommended Next Steps

### Phase 1: Quick Wins (1-2 weeks)
1. **Add curvature detectors** - 2-3 new columns
2. **Add corner detectors** - 2-3 new columns
3. **Re-optimize** - 30-trial focused optimization
4. **Expected:** 72.5-73.5% accuracy

### Phase 2: Saccade Improvements (1-2 weeks)
1. **Implement 6-fixation saccades** - Better coverage
2. **Try quadrant-based saccades** - Different spatial pattern
3. **Re-optimize** - 30-trial focused optimization
4. **Expected:** 73.0-74.0% accuracy

### Phase 3: Learning Enhancements (2-3 weeks)
1. **Implement triplet STDP** - Better discrimination
2. **Add reward modulation** - Reinforce correct classifications
3. **Re-optimize** - 30-trial focused optimization
4. **Expected:** 73.5-74.5% accuracy

### Phase 4: Ensemble Methods (1-2 weeks)
1. **Train 5 networks** - Different initializations
2. **Implement voting** - Majority or weighted
3. **Evaluate** - Compare single vs ensemble
4. **Expected:** 74.5-76.0% accuracy

---

## Success Criteria

### Minimum Viable Improvement
- **Target:** 73.0% accuracy (+1.07% over current)
- **Justification:** Demonstrates continued progress

### Good Improvement
- **Target:** 74.0% accuracy (+2.07% over current)
- **Justification:** Significant architectural advancement

### Excellent Improvement
- **Target:** 75.0% accuracy (+3.07% over current)
- **Justification:** Major breakthrough, approaching state-of-art for SNNs

### Stretch Goal
- **Target:** 76.0%+ accuracy (+4.07%+ over current)
- **Justification:** World-class SNN performance on EMNIST letters

---

## Resources

### Documentation
- `FINAL_OPTIMIZATION_REPORT.md` - Complete optimization history
- `EXPERIMENT_STATUS.md` - Current experiment status
- `OPTIMIZATION_STATUS.md` - Optimization tracking
- `README.md` - Project overview

### Configurations
- `configs/emnist_letters_saccades_best_v2.json` - Current best config
- `configs/emnist_letters_saccades_best.json` - Previous best (71.91%)

### Scripts
- `scripts/optimize_saccades.py` - General optimization
- `scripts/optimize_saccades_focused.py` - Focused optimization
- `scripts/analyze_optimization.py` - Correlation analysis

### Study Data
- `optimization_study_saccades.pkl` - 50-trial study
- `optimization_study_saccades_focused.pkl` - 30-trial study
- `optimization_analysis.txt` - Detailed analysis

---

## Notes

- Current architecture appears to plateau around 72%
- Further improvements likely require architectural changes, not just hyperparameter tuning
- Focus on difficult letters (G, Q, R) and high-confusion pairs (I/L, G/Q, Y/V)
- Consider biological plausibility vs pure performance trade-offs
- Document all experiments thoroughly for reproducibility

---

*Last updated: 2025-11-21*

