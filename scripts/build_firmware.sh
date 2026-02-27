#!/usr/bin/env bash
set -euo pipefail

# Apply third-party patches (idempotent — safe to run multiple times)
bash "$(dirname "$0")/apply_patches.sh"

mkdir -p build
cd build
cmake ..
cmake --build . -- -j$(nproc)

echo "Build complete"
