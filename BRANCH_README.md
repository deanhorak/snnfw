# EnhancedOptimizer Branch

## Purpose

This branch implements extensive enhancements to the SNNFW hyperparameter optimization system to reach the global optimum for MNIST classification (target: 96-98% accuracy).

## Branch Strategy

- **Base Branch**: `main` (at commit `a549a12`)
- **Current Status**: 92.70% MNIST accuracy with 7 basic hyperparameters
- **Target**: 96-98% accuracy with 20+ hyperparameters and architectural improvements
- **Fallback**: If this approach leads to a dead end, we can safely return to `main` branch

## What's Being Added

### 1. Abstraction Layers (Pluggable Strategies)
- `EncodingStrategy` - Multiple spike encoding methods
- `EdgeOperator` - Multiple feature extraction methods
- `PatternUpdateStrategy` - Multiple learning strategies
- `ClassificationStrategy` - Multiple classification methods

### 2. Encoding Strategies
- **RateEncoder** - Current simple rate coding
- **TemporalEncoder** - Precise spike timing based on feature intensity
- **PopulationEncoder** - Multiple neurons per feature with overlapping tuning curves

### 3. Feature Extraction Methods
- **SobelOperator** - Current edge detection
- **GaborOperator** - Biologically realistic Gabor filters (V1 cortex)
- **DoGOperator** - Difference of Gaussians

### 4. Learning Strategies
- **AppendStrategy** - Current append-until-full
- **ReplaceWorstStrategy** - Replace least representative pattern
- **MergeSimilarStrategy** - Merge similar patterns to maintain diversity

### 5. Classification Strategies
- **MajorityVoting** - Current simple k-NN
- **WeightedDistance** - Distance-weighted k-NN
- **WeightedSimilarity** - Similarity-weighted k-NN

### 6. Preprocessing
- Image normalization (global, local, adaptive)
- Contrast enhancement
- Spatial pooling

### 7. Extended Parameter Space
Total of 20 parameters (up from 7):
- 7 original parameters (grid_size, num_orientations, etc.)
- 13 new parameters (encoding mode, edge operator, learning strategy, etc.)

## Expected Impact

| Component | Expected Accuracy Gain |
|-----------|----------------------|
| Encoding strategies | +2-5% |
| Gabor filters | +1-3% |
| Learning strategies | +1-2% |
| Weighted k-NN | +0.5-2% |
| Preprocessing | +0.5-1.5% |
| **Total** | **+3.3-5.3%** |

**Target**: 96-98% accuracy (from current 92.70%)

## Implementation Timeline

- **Week 1**: Abstraction layers + Encoding strategies
- **Week 2**: Feature extraction + Classification
- **Week 3**: Learning strategies + Testing + Optimization runs

**Total**: ~2-3 weeks

## Key Files

### Documentation
- `docs/OPTIMIZATION_ROADMAP.md` - Comprehensive implementation guide
- `configs/mnist_optimization_extended.json` - Extended parameter configuration

### To Be Created
- `include/snnfw/encoding/EncodingStrategy.h`
- `include/snnfw/features/EdgeOperator.h`
- `include/snnfw/learning/PatternUpdateStrategy.h`
- `include/snnfw/classification/ClassificationStrategy.h`
- `src/encoding/RateEncoder.cpp`
- `src/encoding/TemporalEncoder.cpp`
- `src/encoding/PopulationEncoder.cpp`
- `src/features/SobelOperator.cpp`
- `src/features/GaborOperator.cpp`
- `src/features/DoGOperator.cpp`
- And more...

## Testing Strategy

1. **Unit Tests**: Each strategy independently
2. **Integration Tests**: Full pipeline with different strategy combinations
3. **Benchmark Tests**: Performance comparison
4. **Optimization Runs**: 100-500 iterations to find optimal configuration

## Success Criteria

- ✅ All tests passing
- ✅ Accuracy ≥ 96% on MNIST test set
- ✅ Performance acceptable (training time < 2x current)
- ✅ Code quality maintained (no regressions)

## Rollback Plan

If this approach doesn't work out:
```bash
git checkout main
git branch -D EnhancedOptimizer
```

All work on `main` branch remains safe and functional.

## Current Status

- [x] Branch created
- [x] Documentation written
- [x] Extended parameter configuration created
- [ ] Abstraction layers implemented
- [ ] Encoding strategies implemented
- [ ] Feature extraction methods implemented
- [ ] Classification strategies implemented
- [ ] Learning strategies implemented
- [ ] Testing complete
- [ ] Optimization runs complete
- [ ] Target accuracy achieved

## Notes

This is an experimental branch. The goal is to explore whether architectural improvements can push SNNFW's MNIST accuracy to state-of-the-art SNN levels (96-98%). If successful, this will be merged back to `main`. If not, we have a clean fallback.

---

**Created**: 2025-10-17  
**Base Commit**: `a549a12` - "docs: Add extended optimization roadmap and configuration"  
**Branch**: `EnhancedOptimizer`

