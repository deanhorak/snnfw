#!/bin/bash

# Script to run the visualization demo with correct library paths

cd "$(dirname "$0")"

# Use system libraries to avoid Anaconda conflicts
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

# Run the demo
./build/visualization_demo


