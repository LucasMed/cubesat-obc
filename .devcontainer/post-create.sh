#!/usr/bin/env bash
# .devcontainer/post-create.sh
# Runs once inside the container immediately after creation.
set -e

echo "=== CubeSat OBC – Dev Container setup ==="

# Initialise git submodules if not already done
if [ -d ".git" ]; then
    echo "→ Updating git submodules..."
    git submodule update --init --recursive
fi

# Create out-of-source build directory and configure for host (Linux)
echo "→ Configuring CMake (host / Linux build)..."
cmake -S . -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DPICO_ENABLED=OFF

echo ""
echo "✓ Dev container ready!"
echo "  Build   : cmake --build build"
echo "  Tests   : cmake --build build && ctest --test-dir build -V"
echo "  Coverage: bash scripts/run_tests.sh"
echo ""
