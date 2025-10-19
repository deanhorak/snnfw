#!/bin/bash

# Benchmark script to test different thread counts

echo "=== OpenMP Thread Count Benchmark ==="
echo "Testing MNIST V1 Hierarchical with different thread counts"
echo ""

for threads in 1 4 8 12 16 20; do
    echo "========================================="
    echo "Testing with $threads threads..."
    echo "========================================="
    
    OMP_NUM_THREADS=$threads LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH \
        ./build/mnist_v1_hierarchical ./configs/mnist_v1_hierarchical.json 2>&1 | \
        tee results/v1_hierarchical_${threads}threads.txt | \
        grep -E "(OpenMP|Training:|Testing:|Overall Accuracy)"
    
    echo ""
done

echo "========================================="
echo "Benchmark Complete!"
echo "========================================="
echo ""
echo "Summary:"
for threads in 1 4 8 12 16 20; do
    echo "--- $threads threads ---"
    grep -E "(Training:|Testing:|Overall Accuracy)" results/v1_hierarchical_${threads}threads.txt
    echo ""
done

