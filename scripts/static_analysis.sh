#!/usr/bin/env bash
# =============================================================================
# static_analysis.sh — CubeSat OBC static analysis runner
#
# Tools (in order of execution):
#   1. clang-format  — formatting compliance check (diff only, no rewrite)
#   2. clang-tidy    — semantic / safety checks
#   3. cppcheck      — portability, UB, performance checks
#
# Exit codes:
#   0 — all checks passed (or tools not installed, soft-fail)
#   1 — one or more violations found
#
# Usage:
#   ./scripts/static_analysis.sh            # check only (CI mode)
#   ./scripts/static_analysis.sh --fix      # auto-fix formatting with clang-format
# =============================================================================

set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

FIX_MODE=0
for arg in "$@"; do
  [[ "$arg" == "--fix" ]] && FIX_MODE=1
done

PASS=0
FAIL=0

# Colour helpers
RED='\033[0;31m'
GRN='\033[0;32m'
YEL='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GRN}[PASS]${NC} $*"; PASS=$((PASS+1)); }
fail() { echo -e "${RED}[FAIL]${NC} $*"; FAIL=$((FAIL+1)); }
info() { echo -e "${YEL}[INFO]${NC} $*"; }

# C source files under src/ and include/ (exclude third_party and generated)
SRC_FILES=$(find src include -name '*.c' -o -name '*.h' 2>/dev/null | \
            grep -v third_party | sort)

# =============================================================================
# 1. clang-format
# =============================================================================

CLANG_FORMAT=$(command -v clang-format-14 || command -v clang-format || true)

if [[ -n "$CLANG_FORMAT" ]]; then
  info "clang-format: $($CLANG_FORMAT --version)"
  FORMAT_FAIL=0
  for f in $SRC_FILES; do
    if [[ "$FIX_MODE" == "1" ]]; then
      "$CLANG_FORMAT" -i "$f"
    else
      DIFF=$("$CLANG_FORMAT" --dry-run --Werror "$f" 2>&1 || true)
      if [[ -n "$DIFF" ]]; then
        echo "  Formatting issue: $f"
        FORMAT_FAIL=1
      fi
    fi
  done
  if [[ "$FIX_MODE" == "1" ]]; then
    pass "clang-format: applied auto-fix to all files"
  elif [[ "$FORMAT_FAIL" == "0" ]]; then
    pass "clang-format: all files conform to .clang-format"
  else
    fail "clang-format: some files need reformatting (run with --fix to auto-correct)"
  fi
else
  info "clang-format not found — skipping format check"
  info "  Install: sudo apt-get install clang-format-14"
fi

# =============================================================================
# 2. clang-tidy
# =============================================================================

CLANG_TIDY=$(command -v clang-tidy-14 || command -v clang-tidy || true)

if [[ -n "$CLANG_TIDY" ]]; then
  info "clang-tidy: $($CLANG_TIDY --version | head -1)"
  # clang-tidy needs a compile_commands.json — generate from host build dir
  BUILD_DIR="$REPO_ROOT/build"
  COMPILE_CMDS="$BUILD_DIR/compile_commands.json"
  if [[ ! -f "$COMPILE_CMDS" ]]; then
    info "  Generating compile_commands.json..."
    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPICO_ENABLED=OFF \
          -Wno-dev
  fi

  TIDY_FAIL=0
  C_FILES=$(find src -name '*.c' | grep -v third_party | grep -v '/pico_' | sort)
  for f in $C_FILES; do
    # Add FatFs include path for diskio.c
    if [[ "$f" == *"payload/diskio.c"* ]]; then
      RESULT=$("$CLANG_TIDY" -p "$BUILD_DIR" "$f" --extra-arg=-I$REPO_ROOT/pico-sdk/lib/tinyusb/lib/fatfs/source 2>&1 || true)
    else
      RESULT=$("$CLANG_TIDY" -p "$BUILD_DIR" "$f" 2>&1 || true)
    fi
    if echo "$RESULT" | grep -q "warning:\|error:"; then
      echo "$RESULT"
      TIDY_FAIL=1
    fi
  done
  if [[ "$TIDY_FAIL" == "0" ]]; then
    pass "clang-tidy: no issues found"
  else
    fail "clang-tidy: violations found (see output above)"
  fi
else
  info "clang-tidy not found — skipping semantic checks"
  info "  Install: sudo apt-get install clang-tidy-14"
fi

# =============================================================================
# 3. cppcheck
# =============================================================================

if command -v cppcheck >/dev/null 2>&1; then
  info "cppcheck: $(cppcheck --version)"
  CPPCHECK_OUT=$(cppcheck \
    --enable=warning,performance,portability,style \
    --inconclusive \
    --std=c11 \
    --force \
    --inline-suppr \
    --suppress=missingInclude \
    --suppress=missingIncludeSystem \
    --suppress=ConfigurationNotChecked \
    --error-exitcode=1 \
    -I include \
    src \
    2>&1 || true)

  # Filter out pure informational lines
  ISSUES=$(echo "$CPPCHECK_OUT" | grep -E "\[.*\]" | grep -v "^$" || true)

  if [[ -z "$ISSUES" ]]; then
    pass "cppcheck: no issues found"
  else
    echo "$ISSUES"
    fail "cppcheck: violations found (see output above)"
  fi
else
  info "cppcheck not found — skipping"
  info "  Install: sudo apt-get install cppcheck"
fi

# =============================================================================
# Summary
# =============================================================================

echo ""
echo "==========================================="
echo " Static Analysis Summary"
echo "==========================================="
printf "  Passed: ${GRN}%d${NC}   Failed: ${RED}%d${NC}\n" "$PASS" "$FAIL"
echo "==========================================="

if [[ "$FAIL" -gt 0 ]]; then
  exit 1
fi
exit 0
