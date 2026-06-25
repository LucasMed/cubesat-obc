#!/usr/bin/env bash
# =============================================================================
# scripts/pico_ci.sh
# =============================================================================
# Full validation pipeline for CubeSat OBC firmware.
# Intended to run inside the pico-test Docker container but also works
# locally if the required tools are on PATH.
#
# Stages:
#   1  host-test       — Build host binary + run 29/29 CTest suite
#   2a pico-build      — Cross-compile cubesat_obc_pico.uf2 (RP2350, pico2_w)
#   2b bootloader-build — Cross-compile cubesat_obc_bootloader.uf2 + combined
#   3  emu-build       — Cross-compile cubesat_obc_emu.elf  (RP2040,  pico_w)
#   4  emulate         — rp2040js boot smoke-test on the RP2040 ELF
#   (optional)
#   5  static      — clang-format, clang-tidy, cppcheck
#   5b coverity    — Coverity Scan static analysis (optional, tool required)
#   6  coverage    — gcovr HTML + text summary
#
# Usage:
#   bash scripts/pico_ci.sh [all|host-test|pico-build|bootloader-build|
#                            emu-build|emulate|static|coverity|coverage|sanitize]
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
# Resolve SDK: env var → in-tree third_party copy → /opt fallback → home fallback
if [[ -z "${PICO_SDK_PATH:-}" ]]; then
    if [[ -f "${REPO_ROOT}/third_party/pico-sdk/pico_sdk_init.cmake" ]]; then
        PICO_SDK_PATH="${REPO_ROOT}/third_party/pico-sdk"
    elif [[ -f "/opt/pico-sdk/pico_sdk_init.cmake" ]]; then
        PICO_SDK_PATH="/opt/pico-sdk"
    elif [[ -f "$HOME/pico-sdk/pico_sdk_init.cmake" ]]; then
        PICO_SDK_PATH="$HOME/pico-sdk"
    fi
fi
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

  # Clean build directory to avoid CMake cache path issues
  rm -rf "${BUILD_HOST:?}"/* || true

  cmake -S "$REPO_ROOT" -B "$BUILD_HOST" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DPICO_ENABLED=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        2>&1 | tee -a "$ARTIFACTS/build.log" \
  || { fail "Host CMake configure: FAIL"; record 1 "host-test"; return 1; }

  cmake --build "$BUILD_HOST" --parallel "$(nproc)" \
        2>&1 | tee -a "$ARTIFACTS/build.log" \
  || { fail "Host CMake build: FAIL"; record 1 "host-test"; return 1; }

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

# ------------------------------------------------------------------
# Shared CMake configure for RP2350 builds
# ------------------------------------------------------------------
_ensure_pico_build_dir() {
  if [[ -f "$BUILD_PICO/build.ninja" ]]; then
    return 0  # already configured
  fi
  mkdir -p "$BUILD_PICO"
  cmake -S "$REPO_ROOT" -B "$BUILD_PICO" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DPICO_ENABLED=ON \
        -DPICO_SDK_PATH="${PICO_SDK_PATH}" \
        -DPICO_BOARD=pico2_w \
        2>&1 | tee -a "$ARTIFACTS/build.log"
}

# ------------------------------------------------------------------
# Soft-fail for known GCC 15 + Pico SDK 2.2.0 incompatibility
# See: https://github.com/raspberrypi/pico-sdk/issues/2718
# ------------------------------------------------------------------
_gcc15_softfail() {
  local rc=$1
  if [[ "$rc" != "0" ]] && grep -qE "(nvic_hw->icpr|subscripted value|hardware/irq.h:453)" "$ARTIFACTS/build.log" 2>/dev/null; then
    info "Known GCC 15 + Pico SDK 2.2.0 incompatibility detected"
    info "See: https://github.com/raspberrypi/pico-sdk/issues/2718"
    info "Consider using GCC 14 or older, or Pico SDK 2.1.x"
    return 0
  fi
  return "$rc"
}

# =============================================================================
# Stage 2a — Pico 2W (RP2350) firmware build → .uf2
# =============================================================================
run_pico_build() {
  stage "2a / pico-build — cubesat_obc_pico"

  if [[ ! -f "${PICO_SDK_PATH}/external/pico_sdk_import.cmake" ]]; then
    fail "Pico SDK not found at ${PICO_SDK_PATH}"
    info "Set PICO_SDK_PATH or run inside the pico-test container."
    record "1" "pico-build"
    return 1
  fi

  rm -rf "$BUILD_PICO"  # fresh configure
  _ensure_pico_build_dir || { fail "CMake configure: FAIL"; record 1 "pico-build"; return 1; }

  cmake --build "$BUILD_PICO" \
        --target cubesat_obc_pico \
        --parallel "$(nproc)" \
        2>&1 | tee -a "$ARTIFACTS/build.log"

  local rc=$?
  _gcc15_softfail "$rc"; rc=$?
  record "$rc" "pico-build"

  if [[ "$rc" == "0" ]]; then
    for ext in uf2 bin hex elf map; do
      src="${BUILD_PICO}/src/cubesat_obc_pico.${ext}"
      [[ -f "$src" ]] && cp "$src" "$ARTIFACTS/"
    done
    local sz
    sz=$(stat -c%s "${ARTIFACTS}/cubesat_obc_pico.uf2" 2>/dev/null || echo "?")
    pass "Pico firmware: PASS — .uf2 size: ${sz} bytes"
    arm-none-eabi-size "${BUILD_PICO}/src/cubesat_obc_pico.elf" 2>/dev/null || true
  else
    fail "Pico firmware: FAIL"
  fi
  return "$rc"
}

# =============================================================================
# Stage 2b — Pico 2W bootloader build → .uf2 + combined UF2
# =============================================================================
run_bootloader_build() {
  stage "2b / bootloader-build — cubesat_obc_bootloader"

  if [[ ! -f "${PICO_SDK_PATH}/external/pico_sdk_import.cmake" ]]; then
    fail "Pico SDK not found at ${PICO_SDK_PATH}"
    record "1" "bootloader-build"
    return 1
  fi

  # Reuse existing build dir if firmware was already built; configure if not
  _ensure_pico_build_dir || { fail "CMake configure: FAIL"; record "1" "bootloader-build"; return 1; }

  cmake --build "$BUILD_PICO" \
        --target cubesat_obc_bootloader \
        --parallel "$(nproc)" \
        2>&1 | tee -a "$ARTIFACTS/build.log"

  local rc=$?
  _gcc15_softfail "$rc"; rc=$?
  record "$rc" "bootloader-build"

  if [[ "$rc" == "0" ]]; then
    for ext in uf2 bin hex elf map; do
      src="${BUILD_PICO}/bootloader/cubesat_obc_bootloader.${ext}"
      [[ -f "$src" ]] && cp "$src" "$ARTIFACTS/cubesat_obc_bootloader.${ext}"
    done
    local sz
    sz=$(stat -c%s "${ARTIFACTS}/cubesat_obc_bootloader.uf2" 2>/dev/null || echo "?")
    pass "Bootloader: PASS — .uf2 size: ${sz} bytes"
    arm-none-eabi-size "${BUILD_PICO}/bootloader/cubesat_obc_bootloader.elf" 2>/dev/null || true

    # Generate combined firmware + bootloader UF2 (dual-slot boot)
    if [[ -f "${ARTIFACTS}/cubesat_obc_bootloader.uf2" && \
          -f "${ARTIFACTS}/cubesat_obc_pico.uf2" ]]; then
      local combine="${REPO_ROOT}/scripts/combine_uf2.py"
      if [[ -f "$combine" ]]; then
        python3 "$combine" \
          "${ARTIFACTS}/cubesat_obc_bootloader.uf2" \
          "${ARTIFACTS}/cubesat_obc_pico.uf2" \
          "${ARTIFACTS}/cubesat_obc_combined.uf2" \
          2>&1 | tee -a "$ARTIFACTS/build.log"
      fi
    fi
  else
    fail "Bootloader: FAIL"
  fi
  return "$rc"
}

# =============================================================================
# Stage 3 — Emulation build: minimal smoke-test firmware (Thumb-16 only)
# =============================================================================
# Builds tests/emulation/smoke_test.s — a tiny assembly program that writes
# the required boot strings directly to UART0 DR (0x40034000) using only
# Thumb-16 instructions.  No Pico SDK needed.
# rp2040js fires onByte for every write to that address, so no peripheral
# initialisation is required.
# =============================================================================
run_emu_build() {
  stage "3 / emu-build — minimal smoke-test firmware (Thumb-16 only)"

  if ! command -v arm-none-eabi-gcc &>/dev/null; then
    info "arm-none-eabi-gcc not found — skipping emulation build."
    record "0" "emu-build (skipped)"
    return 0
  fi

  local asm_src="${REPO_ROOT}/tests/emulation/smoke_test.s"
  local ld_script="${REPO_ROOT}/tests/emulation/smoke_linker.ld"
  local out_elf="${ARTIFACTS}/cubesat_obc_emu.elf"

  if [[ ! -f "$asm_src" ]]; then
    info "Smoke-test source not found ($asm_src) — skipping."
    record "0" "emu-build (skipped)"
    return 0
  fi

  arm-none-eabi-gcc \
    -nostartfiles -nostdlib \
    -mcpu=cortex-m0plus -mthumb \
    -T "${ld_script}" \
    "${asm_src}" \
    -o "${out_elf}" \
    2>&1 | tee -a "${ARTIFACTS}/build.log"

  local rc=$?
  record "$rc" "emu-build"

  if [[ "$rc" == "0" ]]; then
    local sz
    sz=$(stat -c%s "${out_elf}" 2>/dev/null || echo '?')
    pass "Emulation build: PASS — .elf size: ${sz} bytes"
  else
    fail "Emulation build: FAIL"
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

  # Ensure rp2040js local deps are installed (ESM import needs node_modules, not global)
  local docker_dir="${REPO_ROOT}/docker"
  if [[ ! -d "${docker_dir}/node_modules/rp2040js" ]]; then
    info "Installing rp2040js locally (docker/node_modules)..."
    npm install --prefix "${docker_dir}" --silent 2>&1
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
# Stage 5b — Coverity Scan (optional — tool must be installed separately)
# =============================================================================
run_coverity() {
  stage "5b / coverity — Coverity Scan static analysis"

  # Check if Coverity tool is available
  local COV_DIR="${COVERITY_DIR:-${REPO_ROOT}/cov-analysis}"
  local COV=""
  if [[ -x "${COV_DIR}/bin/cov-build" ]]; then
    COV="${COV_DIR}/bin"
  elif command -v cov-build &>/dev/null; then
    COV="$(dirname "$(command -v cov-build)")"
  fi

  if [[ -z "$COV" ]]; then
    info "Coverity tool not found — skipping."
    info "  Download from: https://scan.coverity.com/download#section-downloads"
    info "  Or set COVERITY_DIR=/path/to/cov-analysis"
    record "0" "coverity (skipped)"
    return 0
  fi

  # Coverity binaries are platform-specific — Linux x64 won't run on macOS ARM64.
  # Check if the binary can execute before attempting the full analysis.
  if ! "${COV}/cov-build" --version &>/dev/null; then
    info "Coverity binary not executable on this platform — skipping."
    info "  Coverity Scan requires Linux. On GitHub Actions (ubuntu-latest) it will run."
    info "  For local macOS analysis, use: brew install ... or Docker"
    record "0" "coverity (skipped: wrong platform)"
    return 0
  fi

  info "Using Coverity from: ${COV}"

  # Run Coverity scan script
  COVERITY_TOKEN="${COVERITY_TOKEN:-}" \
    bash "${REPO_ROOT}/scripts/coverity_scan.sh" \
    2>&1 | tee "${ARTIFACTS}/coverity_scan.log"

  local rc=$?

  record "$rc" "coverity"

  if [[ "$rc" == "0" ]]; then
    pass "Coverity analysis: PASS (see artifacts/coverity_defects.txt)"
    return 0
  elif [[ "$rc" == "3" ]]; then
    info "COVERITY_TOKEN not set — Coverity submission skipped (analysis-only run)"
    pass "Coverity analysis: PASS (local only, no submission)"
    return 0
  else
    fail "Coverity analysis: FAIL (see artifacts/coverity_scan.log)"
    return "$rc"
  fi
}

# =============================================================================
# Stage 6b — AddressSanitizer + UBSan (host tests)
# =============================================================================
run_sanitize() {
  stage "6b / sanitize — AddressSanitizer + UndefinedBehaviorSanitizer"
  local sanitize_dir="${REPO_ROOT}/build_sanitize"

  rm -rf "${sanitize_dir}"/*
  mkdir -p "$sanitize_dir"

  cmake -S "$REPO_ROOT" -B "$sanitize_dir" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DPICO_ENABLED=OFF \
        -DSANITIZE=ON \
        2>&1 | tee -a "$ARTIFACTS/build.log" \
  || { fail "Sanitize configure: FAIL"; record 1 "sanitize"; return 1; }

  cmake --build "$sanitize_dir" --parallel "$(nproc)" \
        2>&1 | tee -a "$ARTIFACTS/build.log" \
  || { fail "Sanitize build: FAIL"; record 1 "sanitize"; return 1; }

  mkdir -p "$ARTIFACTS/test_results"
  ctest --test-dir "$sanitize_dir" \
        --output-on-failure \
        --output-junit "$ARTIFACTS/test_results/sanitize_tests.xml" \
        2>&1 | tee "$ARTIFACTS/test_results/sanitize_tests.txt"

  local rc=${PIPESTATUS[0]}
  record "$rc" "sanitize"
  [[ "$rc" == "0" ]] \
    && pass "Sanitizer: PASS" \
    || fail "Sanitizer: FAIL (memory bugs detected!)"
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
    run_host_test        || true
    run_pico_build       || true
    run_bootloader_build || true
    run_emu_build        || true
    run_emulate          || true
    run_static           || true
    run_coverity         || true
    run_coverage         || true
    run_sanitize         || true
    ;;
  host-test)       run_host_test ;;
  pico-build)      run_pico_build ;;
  bootloader-build) run_bootloader_build ;;
  emu-build)       run_emu_build ;;
  emulate)         run_emulate ;;
  static)          run_static ;;
  coverity)        run_coverity ;;
  coverage)        run_coverage ;;
  sanitize)        run_sanitize ;;
  *)
    echo "Usage: $0 [all|host-test|pico-build|bootloader-build|emu-build|emulate|static|coverity|coverage|sanitize]"
    exit 2
    ;;
esac

print_summary
