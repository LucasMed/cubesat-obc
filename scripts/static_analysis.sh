#!/usr/bin/env bash
set -euo pipefail

if ! command -v cppcheck >/dev/null 2>&1; then
  echo "Please install cppcheck to run static analysis"
  exit 0
fi

cppcheck --enable=warning,performance,portability --inconclusive --std=c11 --force src include || true
