# Learning Strategies - Biological Plausibility

This document explains the biological basis and implementation of pattern learning strategies in SNNFW.

## Overview

SNNFW implements three pattern update strategies that determine how neurons manage their learned temporal spike patterns. All strategies are designed to maintain biological plausibility while providing practical performance improvements.

## Biological Foundation

### Current Pattern Learning Mechanism

The base neuron implementation already uses biologically-plausible mechanisms:

1. **Temporal Spike Patterns** - Neurons learn sequences of spike times, not just weights
   - Biological basis: Real neurons respond to temporal patterns of input spikes
   - Implementation: Rolling time window stores recent spike times

2. **Hebbian Learning** - "Neurons that fire together, wire together"
   - Biological basis: Synaptic strengthening through correlated activity
   - Implementation: Pattern blending reinforces similar patterns

3. **Temporal Integration** - Rolling time window for spike processing
   - Biological basis: Neurons integrate inputs over time (membrane time constant)
   - Implementation: `windowSize` parameter defines integration period

4. **Similarity-Based Matching** - Cosine similarity for pattern comparison
   - Biological basis: Correlation detection in neural circuits
   - Implementation: Normalized dot product of spike patterns

## Pattern Update Strategies

### 1. AppendStrategy (Baseline)

**Biological Analogy**: Early learning / Hippocampal encoding

**Description**: Simple append-only strategy that stores new patterns until capacity is reached, then blends similar patterns.

**Biological Basis**:
- Mimics rapid encoding of new experiences in the hippocampus
- Similar to episodic memory formation during initial learning
- No pruning or consolidation (unlike biological systems)

**Algorithm**:
```
if patterns.size() < maxPatterns:
    patterns.append(newPattern)
else:
    mostSimilar = findMostSimilar(newPattern)
    if similarity >= threshold:
        blend(mostSimilar, newPattern, alpha=0.2)
    else:
        replace random pattern  # Fallback
```

**Use Case**: Baseline for comparison, rapid initial learning

---

### 2. ReplaceWorstStrategy (Synaptic Pruning)

**Biological Analogy**: Synaptic pruning / Homeostatic plasticity

**Description**: Tracks usage of each pattern and replaces least-used patterns when capacity is reached.

**Biological Basis**:
- **Synaptic Pruning**: Unused synapses are eliminated during development and learning
  - Reference: Chechik et al. (1998) - "Synaptic pruning in development"
- **Homeostatic Plasticity**: Neurons maintain optimal activity levels by adjusting synaptic strengths
  - Reference: Turrigiano & Nelson (2004) - "Homeostatic plasticity in the developing nervous system"
- **Use It or Lose It**: Synapses that are rarely activated undergo elimination
  - Reference: Zenke et al. (2013) - "Synaptic plasticity in neural networks needs homeostasis"

**Algorithm**:
```
if patterns.size() < maxPatterns:
    patterns.append(newPattern)
    usageCounts.append(0)
else:
    mostSimilar = findMostSimilar(newPattern)
    if similarity >= threshold:
        blend(mostSimilar, newPattern, alpha=0.2)
        usageCounts[mostSimilar]++
    else:
        leastUsed = findLeastUsed()  # Find pattern with minimum usage count
        patterns[leastUsed] = newPattern
        usageCounts[leastUsed] = 0
```

**Key Features**:
- Tracks usage count for each pattern
- Replaces least-activated patterns (synaptic pruning)
- Maintains network efficiency by removing unused connections
- Allows adaptation to changing input statistics

**Use Case**: Dynamic environments where input statistics change over time

---

### 3. MergeSimilarStrategy (Synaptic Consolidation)

**Biological Analogy**: Memory consolidation / Prototype learning

**Description**: Merges similar patterns into prototypes, creating compressed representations.

**Biological Basis**:
- **Memory Consolidation**: Hippocampal memories are gradually consolidated into neocortical representations
  - Reference: McClelland et al. (1995) - "Why there are complementary learning systems"
- **Prototype Learning**: Brain forms general concepts from specific examples
  - Reference: Kumaran et al. (2016) - "What learning systems do intelligent agents need?"
- **Sleep-Dependent Consolidation**: Similar memories are merged during sleep
  - Reference: Stickgold & Walker (2013) - "Sleep-dependent memory triage"
- **Semantic Memory Formation**: Episodic memories → Semantic memories
  - Reference: Marr (1971) - "Simple memory: A theory for archicortex"

**Algorithm**:
```
mostSimilar = findMostSimilar(newPattern)

if similarity >= threshold:
    # Consolidate into existing prototype
    merge(mostSimilar, newPattern, weight=0.3)
    mergeCounts[mostSimilar]++
else:
    if patterns.size() < maxPatterns:
        patterns.append(newPattern)
        mergeCounts.append(0)
    else:
        # Replace least representative (outlier)
        leastRepresentative = findLeastRepresentative()
        patterns[leastRepresentative] = newPattern
        mergeCounts[leastRepresentative] = 0
```

**Key Features**:
- Creates "prototype" patterns representing multiple similar examples
- Reduces redundancy through consolidation
- Tracks merge count (higher = more general prototype)
- Replaces outliers when at capacity

**Use Case**: Learning general concepts from specific examples, memory compression

---

## Comparison

| Strategy | Biological Process | Advantage | Disadvantage |
|----------|-------------------|-----------|--------------|
| **Append** | Hippocampal encoding | Fast, simple | No pruning, may accumulate redundant patterns |
| **ReplaceWorst** | Synaptic pruning | Removes unused patterns, adapts to change | Requires usage tracking |
| **MergeSimilar** | Memory consolidation | Creates prototypes, compresses memory | May lose specific details |

## Configuration

All strategies share common parameters:

```json
{
  "learning": {
    "strategy": "replace_worst",  // or "append", "merge_similar"
    "max_patterns": 100,
    "similarity_threshold": 0.7,
    "blend_alpha": 0.2,           // For Append and ReplaceWorst
    "merge_weight": 0.3            // For MergeSimilar
  }
}
```

### Parameter Guidelines

**max_patterns**: Maximum number of patterns per neuron
- Biological: Limited synaptic capacity
- Typical: 20-100 patterns
- Higher = more memory, slower matching

**similarity_threshold**: Threshold for considering patterns similar
- Biological: Correlation threshold for synaptic strengthening
- Typical: 0.6-0.8
- Higher = more selective, less merging/blending

**blend_alpha / merge_weight**: Weight for new pattern in merge
- Biological: Learning rate / synaptic plasticity rate
- Typical: 0.1-0.3
- Higher = faster adaptation, less stability

## Expected Performance Impact

Based on biological principles and preliminary analysis:

**ReplaceWorst**:
- Expected improvement: +0.5-1.5%
- Mechanism: Removes patterns that don't contribute to classification
- Best for: Datasets with noise or changing statistics

**MergeSimilar**:
- Expected improvement: +0.5-1.0%
- Mechanism: Creates more general, robust prototypes
- Best for: Datasets with high intra-class variability

**Combined with hyperparameter tuning**:
- Expected total improvement: +1.5-3.0%
- Should reach 96-98% target accuracy on MNIST

## Implementation Notes

### Maintaining Biological Plausibility

All strategies maintain the core principles:

1. ✅ **Spike-based computation**: Patterns are temporal spike sequences
2. ✅ **Local learning**: Each neuron manages its own patterns independently
3. ✅ **Hebbian principles**: Similar patterns strengthen existing representations
4. ✅ **Temporal integration**: Rolling time window for spike processing
5. ✅ **Capacity limits**: Finite pattern storage (like biological synapses)
6. ✅ **Competitive learning**: Patterns compete for limited resources

### What We Avoid

To maintain biological plausibility, we do NOT use:

- ❌ Global error backpropagation (not biologically plausible)
- ❌ Explicit weight matrices (we use temporal patterns instead)
- ❌ Batch normalization (not biological)
- ❌ Dropout (we use natural capacity limits instead)
- ❌ Gradient descent (we use Hebbian-style pattern blending)

## References

1. Chechik, G., Meilijson, I., & Ruppin, E. (1998). Synaptic pruning in development: A computational account. *Neural Computation*, 10(7), 1759-1777.

2. Turrigiano, G. G., & Nelson, S. B. (2004). Homeostatic plasticity in the developing nervous system. *Nature Reviews Neuroscience*, 5(2), 97-107.

3. Zenke, F., Hennequin, G., & Gerstner, W. (2013). Synaptic plasticity in neural networks needs homeostasis with a fast rate detector. *PLoS Computational Biology*, 9(11), e1003330.

4. McClelland, J. L., McNaughton, B. L., & O'Reilly, R. C. (1995). Why there are complementary learning systems in the hippocampus and neocortex. *Psychological Review*, 102(3), 419.

5. Kumaran, D., Hassabis, D., & McClelland, J. L. (2016). What learning systems do intelligent agents need? *Trends in Cognitive Sciences*, 20(7), 512-534.

6. Stickgold, R., & Walker, M. P. (2013). Sleep-dependent memory triage: evolving generalization through selective processing. *Nature Neuroscience*, 16(2), 139-145.

7. Marr, D. (1971). Simple memory: a theory for archicortex. *Philosophical Transactions of the Royal Society B*, 262(841), 23-81.

8. Fusi, S., Drew, P. J., & Abbott, L. F. (2005). Cascade models of synaptically stored memories. *Neuron*, 45(4), 599-611.

9. Zenke, F., Agnes, E. J., & Gerstner, W. (2015). Diverse synaptic plasticity mechanisms orchestrated to form and retrieve memories in spiking neural networks. *Nature Communications*, 6(1), 1-13.

## Next Steps

1. Test all three strategies on MNIST dataset
2. Compare accuracy improvements
3. Analyze pattern diversity and usage statistics
4. Combine best strategy with optimized hyperparameters
5. Target: 96-98% accuracy on MNIST

