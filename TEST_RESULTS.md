# SNNFW Unit Test Results

## Test Summary

**Date:** 2025-10-14  
**Total Tests:** 15  
**Passed:** 15 ✅  
**Failed:** 0  
**Success Rate:** 100%

## Test Coverage

### Basic Functionality Tests

1. **ConstructorWithParameters** ✅
   - Verifies neuron can be constructed with window size, threshold, and max patterns

2. **InsertSingleSpike** ✅
   - Tests insertion of a single spike
   - Verifies spike appears in output

3. **InsertMultipleSpikes** ✅
   - Tests insertion of multiple spikes
   - Verifies all spikes are tracked

### Rolling Window Tests

4. **RollingWindowRemovesOldSpikes** ✅
   - Tests that spikes outside the time window are removed
   - Window size: 50ms
   - Verifies old spikes (10, 20, 30) are removed when spike at 85ms arrives

5. **WindowSizeAffectsSpikeRetention** ✅
   - Tests that window size parameter correctly affects spike retention
   - Uses 30ms window
   - Verifies spikes are removed based on window size

### Pattern Learning Tests

6. **LearnPattern** ✅
   - Tests basic pattern learning functionality
   - Verifies pattern is stored in reference patterns

7. **StoreMultiplePatterns** ✅
   - Tests storage of multiple distinct patterns
   - Verifies both patterns are stored separately

8. **MaximumPatternCapacity** ✅
   - Tests behavior when maximum pattern capacity is reached
   - Max capacity: 3 patterns
   - Verifies that 4th pattern triggers blending or replacement

9. **EmptyPatternLearning** ✅
   - Tests that learning is skipped when no spikes are present
   - Verifies no empty patterns are stored

### Pattern Recognition Tests

10. **PatternRecognitionTriggersFiring** ✅
    - Tests that neuron fires when a learned pattern is recognized
    - Learned pattern: [10, 20, 30]
    - Test pattern: [160, 170, 180]
    - Cosine similarity: ~0.9493
    - Threshold: 0.94
    - Verifies neuron fires when similarity exceeds threshold

11. **DifferentPatternSizesDontMatch** ✅
    - Tests that patterns with different numbers of spikes don't match
    - 3-spike pattern vs 2-spike pattern
    - Verifies no firing occurs

12. **TemporalOrderingMatters** ✅
    - Tests that temporal pattern matching works correctly
    - Verifies similar patterns at different times are recognized

### Threshold Tests

13. **LowSimilarityThreshold** ✅
    - Tests behavior with low similarity threshold (0.5)
    - Documents that lower thresholds allow more flexible matching

14. **HighSimilarityThreshold** ✅
    - Tests behavior with high similarity threshold (0.99)
    - Verifies that high thresholds require near-exact matches
    - Slightly different patterns don't trigger firing

### Utility Tests

15. **PrintFunctionsWork** ✅
    - Tests that print functions work without crashes
    - Tests both empty and populated states

## Key Findings

### Pattern Matching Behavior

The current implementation uses **cosine similarity on absolute spike timestamps**, not relative intervals. This means:

- Pattern [10, 20, 30] has cosine similarity ~0.9493 with pattern [160, 170, 180]
- Patterns must have the same number of spikes to be compared
- The similarity decreases as the absolute time values diverge

### Rolling Window Behavior

- Spikes are removed when `currentTime - spikeTime > windowSize`
- Spikes exactly at the window boundary are retained
- The window is checked on each spike insertion

### Pattern Capacity Management

When maximum capacity is reached:
- The system finds the most similar existing pattern
- If found, it blends the new pattern into it (alpha = 0.2)
- If not found, it randomly replaces an existing pattern

## Test Execution

```bash
cd build
cmake -DBUILD_TESTS=ON ..
make
LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH ./tests/test_neuron
```

Or using CTest:

```bash
cd build
LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH ctest --output-on-failure
```

## Notes for Future Development

1. **Pattern Representation**: Consider whether to use absolute timestamps or relative intervals for pattern matching
2. **Similarity Metric**: Current cosine similarity works but may need adjustment for temporal patterns
3. **Window Management**: Consider edge cases at window boundaries
4. **Pattern Blending**: The alpha parameter (0.2) is hardcoded; may want to make it configurable

## Conclusion

All unit tests pass successfully, confirming that the core Neuron functionality is working as designed. The implementation correctly handles:
- Spike insertion and tracking
- Rolling time window management
- Pattern learning and storage
- Pattern recognition and firing
- Capacity-limited memory with blending

