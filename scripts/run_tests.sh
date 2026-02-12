#!/usr/bin/env bash
set -euo pipefail

cd build
ctest --output-on-failure
