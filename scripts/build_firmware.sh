#!/usr/bin/env bash
set -euo pipefail

mkdir -p build
cd build
cmake ..
cmake --build . -- -j$(nproc)

echo "Build complete"
