#!/usr/bin/env bash
# =============================================================================
# scripts/pico_ci.sh
# =============================================================================
# Full validation pipeline for CubeSat OBC firmware.
# Intended to run inside the pico-test Docker container but also works
# locally if the required tools are on PATH.
#
# Stages:
#   1  host-test   — Build host binary + run 29/29 CTest suite
#   2  pico-build  — Cross-compile cubesat_obc_pico.uf2 (RP2350, pico2_w)
#   3  emu-build   — Cross-compile cubesat_obc_emu.elf  (RP2040,  pico_w)
#   4  emulate     — rp2040js boot smoke-test on the RP2040 ELF
#   (optional)
#   5  static      — clang-format, clang-tidy, cppcheck
#   6  coverage    — gcovr HTML + text summary
#
# Usage:
#   bash scripts/pico_ci.sh [all|host-test|pico-build|emu-build|emulate|
#                            static|coverage]
#   Default: all
#
# On success the /artifacts (or ./artifacts) directory contains:
#   cubesat_obc_pico.uf2   ← ready-to-flash Pico 2W firmware
#   cubesat_obc_pico.bin
#   cubesat_obc_pico.hex
#   test_results/           ← CTest JUnit XML
#   coverage/               ← gcovr HTML report
#   build.log               ← full build log
# =============================================================================

set -euo pipefail

# ─── Paths ──────────────────────────────────────────────────────────────────
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_HOST="${REPO_ROOT}/build_ci"
BUILD_PICO="${REPO_ROOT}/build_pico_ci"
BUILD_EMU="${REPO_ROOT}/build_emu_ci"
ARTIFACTS="${ARTIFACTS_DIR:-${REPO_ROOT}/artifacts}"
PICO_SDK_PATH="${PICO_SDK_PATH:-/opt/pico-sdk}"
EMU_SCRIPT="${REPO_ROOT}/docker/emulate_boot.mjs"

# ─── Colours ────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

stage() { echo -e "\n${CYAN}${BOLD}══ Stage: $* ══${NC}"; }
pass()  { echo -e "${GREEN}✅  $*${NC}"; }
fail()  { echo -e "${RED}❌  $*${NC}"; }
info()  { echo -e "${YELLOW}ℹ  $*${NC}"; }

# ─── Global result tracking ─────────────────────────────────────────────────
PASS_STAGES=()
FAIL_STAGES=()

record() {
  local status="$1" name="$2"
  if [[ "$status" == "0" ]]; then PASS_STAGES+=("$name")
  else                             FAIL_STAGES+=("$name"); fi
}

mkdir -p "$ARTIFACTS"

# =============================================================================
# Stage 1 — Host unit + integration tests
# =============================================================================
run_host_test() {
  stage "1 / host-test — CTest (29/29)"

  mkdir -p "$BUILD_HOST"
  cmake -S "$REPO_ROOT" -B "$BUILD_HOST" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DPICO_ENABLED=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        2>&1 | tee -a "$ARTIFACTS/build.log"

  cmake --build "$BUILD_HOST" --parallel "$(nproc)" \
        2>&1 | tee -a "$ARTIFACTS/build.log"

  mkdir -p "$ARTIFACTS/test_results"

  ctest --test-dir "$BUILD_HOST" \
        --output-on-failure \
        --output-junit "$ARTIFACTS/test_results/host_tests.xml" \
        2>&1 | tee "$ARTIFACTS/test_results/host_tests.txt"

  local rc=${PIPESTATUS[0]}
  record "$rc" "host-test"
  [[ "$rc" == "0" ]] && pass "Host tests: PASS" || fail "Host tests: FAIL"
  return "$rc"
}

# =============================================================================
# Stage 2 — Pico 2W (RP2350) firmware build → .uf2
# =============================================================================
run_pico_build() {
  stage "2 / pico-build — RP2350 (pico2_w) → .uf2"

  if [[ ! -f "${PICO_SDK_PATH}/external/pico_sdk_import.cmake" ]]; then
    fail "Pico SDK not found at ${PICO_SDK_PATH}"
    info "Set PICO_SDK_PATH or run inside the pico-test container."
    record "1" "pico-build"
    return 1
  fi

  mkdir -p "$BUILD_PICO"
  cmake -S "$REPO_ROOT" -B "$BUILD_PICO" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DPICO_ENABLED=ON \
        -DPICO_SDK_PATH="${PICO_SDK_PATH}" \
        -DPICO_BOARD=pico2_w \
        2>&1 | tee -a "$ARTIFACTS/build.log"

  cmake --build "$BUILD_PICO" \
        --target cubesat_obc_pico \
        --parallel "$(nproc)" \
        2>&1 | tee -a "$ARTIFACTS/build.log"

  local rc=$?
  record "$rc" "pico-build"

  if [[ "$rc" == "0" ]]; then
    # Copy artefacts
    for ext in uf2 bin hex elf map; do
      src="${BUILD_PICO}/src/cubesat_obc_pico.${ext}"
      [[ -f "$src" ]] && cp "$src" "$ARTIFACTS/"
    done
    local uf2_size
    uf2_size=$(stat -c%s "${ARTIFACTS}/cubesat_obc_pico.uf2" 2>/dev/null || echo "?")
    pass "Pico (RP2350) build: PASS — .uf2 size: ${uf2_size} bytes"
    arm-none-eabi-size "${BUILD_PICO}/src/cubesat_obc_pico.elf" \
        2>/dev/null || true
  else
    fail "Pico (RP2350) build: FAIL"
  fi
  return "$rc"
}

# =============================================================================
# Stage 3 — Emulation build (RP2040 / pico_w) for rp2040js smoke-test
# =============================================================================
run_emu_build() {
  stage "3 / emu-build — RP2040 (pico_w) → .elf for emulation"

  if [[ ! -f "${PICO_SDK_PATH}/external/pico_sdk_import.cmake" ]]; then
    info "Skipping emulation build (Pico SDK not available)"
    record "0" "emu-build (skipped)"
    return 0
  fi

  mkdir -p "$BUILD_EMU"
  cmake -S "$REPO_ROOT" -B "$BUILD_EMU" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DPICO_ENABLED=ON \
        -DPICO_SDK_PATH="${PICO_SDK_PATH}" \
        -DPICO_BOARD=pico_w \
        2>&1 | tee -a "$ARTIFACTS/build.log"

  cmake --build "$BUILD_EMU" \
        --target cubesat_obc_pico \
        --parallel "$(nproc)" \
        2>&1 | tee -a "$ARTIFACTS/build.log"

  local rc=$?
  record "$rc" "emu-build"

  if [[ "$rc" == "0" ]]; then
    local elf="${BUILD_EMU}/src/cubesat_obc_pico.elf"
    [[ -f "$elf" ]] && cp "$elf" "${ARTIFACTS}/cubesat_obc_emu.elf"
    pass "Emulation build (RP2040): PASS"
  else
    fail "Emulation build (RP2040): FAIL"
  fi
  return "$rc"
}

# =============================================================================
# Stage 4 — Boot smoke-test via rp2040js emulator
# =============================================================================
run_emulate() {
  stage "4 / emulate — rp2040js boot smoke-test"

  local elf="${ARTIFACTS}/cubesat_obc_emu.elf"
  if [[ ! -f "$elf" ]]; then
    elf="${BUILD_EMU}/src/cubesat_obc_pico.elf"
  fi

  if [[ ! -f "$elf" ]]; then
    info "Emulation ELF not found — skipping boot smoke-test."
    record "0" "emulate (skipped)"
    return 0
  fi

  if ! command -v node &>/dev/null; then
    info "Node.js not found — skipping emulation stage."
    record "0" "emulate (skipped)"
    return 0
  fi

  # Run emulator; capture exit code
  node "${EMU_SCRIPT}" "$elf" 10000 \
       2>&1 | tee "${ARTIFACTS}/emulate_boot.log"
  local rc=${PIPESTATUS[0]}

  record "$rc" "emulate"
  [[ "$rc" == "0" ]] \
    && pass  "Boot smoke-test: PASS" \
    || fail  "Boot smoke-test: FAIL (see artifacts/emulate_boot.log)"
  return "$rc"
}

# =============================================================================
# Stage 5 — Static analysis
# =============================================================================
run_static() {
  stage "5 / static — clang-format · clang-tidy · cppcheck"

  bash "${REPO_ROOT}/scripts/static_analysis.sh" \
       2>&1 | tee "${ARTIFACTS}/static_analysis.log"
  local rc=${PIPESTATUS[0]}

  record "$rc" "static"
  [[ "$rc" == "0" ]] \
    && pass "Static analysis: PASS" \
    || fail "Static analysis: FAIL (see artifacts/static_analysis.log)"
  return "$rc"
}

# =============================================================================
# Stage 6 — Coverage report
# =============================================================================
run_coverage() {
  stage "6 / coverage — gcovr HTML report"

  mkdir -p "$ARTIFACTS/coverage"

  # Ensure host build exists
  [[ -d "$BUILD_HOST" ]] || run_host_test

  gcovr \
    --root "${REPO_ROOT}/src" \
    --filter "${REPO_ROOT}/src/control/" \
    --filter "${REPO_ROOT}/src/core/" \
    --filter "${REPO_ROOT}/src/services/" \
    --html-details "${ARTIFACTS}/coverage/index.html" \
    --txt "${ARTIFACTS}/coverage/summary.txt" \
    --print-summary \
    "${BUILD_HOST}" \
    2>&1 | tee "${ARTIFACTS}/coverage/gcovr.log"

  local rc=${PIPESTATUS[0]}
  record "$rc" "coverage"
  [[ "$rc" == "0" ]] \
    && pass "Coverage report: PASS (see artifacts/coverage/index.html)" \
    || fail "Coverage report: FAIL"
  return "$rc"
}

# =============================================================================
# Final summary
# =============================================================================
print_summary() {
  echo -e "\n${BOLD}════════════════════════════════════════${NC}"
  echo -e "${BOLD} Pipeline Summary${NC}"
  echo -e "${BOLD}════════════════════════════════════════${NC}"

  for s in "${PASS_STAGES[@]:-}"; do
    [[ -n "$s" ]] && echo -e "  ${GREEN}✅  $s${NC}"
  done
  for s in "${FAIL_STAGES[@]:-}"; do
    [[ -n "$s" ]] && echo -e "  ${RED}❌  $s${NC}"
  done

  echo -e "${BOLD}════════════════════════════════════════${NC}"

  if [[ "${#FAIL_STAGES[@]}" -gt 0 ]]; then
    echo -e "${RED}${BOLD}PIPELINE FAILED — firmware NOT ready to deploy${NC}\n"
    return 1
  else
    local uf2="${ARTIFACTS}/cubesat_obc_pico.uf2"
    echo -e "${GREEN}${BOLD}PIPELINE PASSED — firmware ready to deploy${NC}"
    if [[ -f "$uf2" ]]; then
      echo -e "  📦  Flash artifact: ${uf2}"
      echo -e "  📋  Flash instructions:"
      echo -e "       1. Hold BOOTSEL and connect USB → RPI-RP2 disk appears"
      echo -e "       2. cp ${uf2} /media/\$USER/RPI-RP2/"
      echo -e "       3. minicom -D /dev/ttyACM0 -b 115200\n"
    fi
    return 0
  fi
}

# =============================================================================
# Entry point
# =============================================================================
COMMAND="${1:-all}"

case "$COMMAND" in
  all)
    run_host_test
    run_pico_build
    run_emu_build
    run_emulate
    run_static
    run_coverage
    ;;
  host-test)   run_host_test ;;
  pico-build)  run_pico_build ;;
  emu-build)   run_emu_build ;;
  emulate)     run_emulate ;;
  static)      run_static ;;
  coverage)    run_coverage ;;
  *)
    echo "Usage: $0 [all|host-test|pico-build|emu-build|emulate|static|coverage]"
    exit 2
    ;;
esac

print_summary
