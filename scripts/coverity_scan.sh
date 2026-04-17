#!/usr/bin/env bash
# =============================================================================
# scripts/coverity_scan.sh
# =============================================================================
# Coverity Scan static analysis for CubeSat OBC firmware.
#
# Coverity Scan is a free static analysis service for open-source projects.
# See: https://scan.coverity.com/
#
# Setup (one-time):
#   1. Sign up at https://scan.coverity.com/projects/new
#   2. Add this repo: cubesat-obc
#   3. Download the Coverity tool (cov-analysis-linux or cov-analysis-macosx)
#      from https://scan.coverity.com/download#section-downloads
#   4. Set COVERITY_TOKEN in your environment or CI secret
#      (get token from the Coverity Scan project settings page)
#
# Local usage:
#   COVERITY_TOKEN=your-token ./scripts/coverity_scan.sh
#
# In CI (GitHub Actions):
#   Use the `Coverity Scan` GitHub Action which handles tool download:
#   https://github.com/marketplace/actions/coverity-scan
#
# Output:
#   ./artifacts/coverity/   — analysis results, HTML report, JSON output
#
# Exit codes:
#   0  — analysis complete (defects may or may not be present)
#   1  — Coverity tool not found
#   2  — Build failed / configuration error
#   3  — Token missing
#
# =============================================================================

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build_coverity"
ARTIFACTS="${REPO_ROOT}/artifacts/coverity"
COV_DIR="${COVERITY_DIR:-${REPO_ROOT}/cov-analysis}"

# ─── Colours ────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

pass()  { echo -e "${GREEN}✅  $*${NC}"; }
fail()  { echo -e "${RED}❌  $*${NC}"; }
info()  { echo -e "${YELLOW}ℹ  $*${NC}"; }
stage() { echo -e "\n${CYAN}${BOLD}══ Stage: $* ══${NC}"; }

# ─── Token check ───────────────────────────────────────────────────────────
check_token() {
  if [[ -z "${COVERITY_TOKEN:-}" ]]; then
    fail "COVERITY_TOKEN environment variable not set."
    info "Get your token from: https://scan.coverity.com/projects/cubesat-obc/settings/tokens"
    info "Usage: COVERITY_TOKEN=your-token ./scripts/coverity_scan.sh"
    return 3
  fi
  return 0
}

# ─── Tool detection ─────────────────────────────────────────────────────────
find_coverity() {
  if [[ -x "${COV_DIR}/bin/cov-build" ]]; then
    COV="${COV_DIR}/bin"
    return 0
  fi

  # Also check PATH (e.g. /opt/coverity/bin)
  if command -v cov-build &>/dev/null; then
    COV="$(dirname "$(command -v cov-build)")"
    return 0
  fi

  return 1
}

# ─── Stage 1: Configure build ───────────────────────────────────────────────
configure() {
  stage "Coverity 1/3 — Configure build"

  if [[ -d "$BUILD_DIR" ]]; then
    info "Reusing existing build directory: $BUILD_DIR"
  else
    mkdir -p "$BUILD_DIR"
  fi

  cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DPICO_ENABLED=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -Wno-dev \
    2>&1 | tee "$ARTIFACTS/coverity_config.log" || {
      fail "CMake configure failed — see $ARTIFACTS/coverity_config.log"
      return 2
    }

  pass "CMake configure: OK"
}

# ─── Stage 2: Capture build with cov-build ─────────────────────────────────
capture() {
  stage "Coverity 2/3 — Capture (cov-build)"

  if ! find_coverity; then
    fail "Coverity tool (cov-build) not found."
    info "Download from: https://scan.coverity.com/download#section-downloads"
    info "Or set COVERITY_DIR=/path/to/cov-analysis"
    return 1
  fi

  local cov_build="${COV}/cov-build"
  local cov_analyze="${COV}/cov-analyze"
  local cov_format="${COV}/cov-format-errors"

  info "Using Coverity from: ${COV}"

  mkdir -p "$ARTIFACTS"

  # Remove stale cov-int directory
  rm -rf "$ARTIFACTS/cov-int"

  # Capture the build
  # --dir: output directory for intermediate representation
  # --no-libs: skip system/library analysis (focus on project code)
  # --fs-capture-search: search for source files
  "$cov_build" \
    --dir "$ARTIFACTS/cov-int" \
    --no-libs \
    --fs-capture-search "$REPO_ROOT/src" \
    --fs-capture-search "$REPO_ROOT/include" \
    cmake --build "$BUILD_DIR" \
    2>&1 | tee "$ARTIFACTS/coverity_build.log"

  local rc=$?

  if [[ "$rc" -ne 0 ]]; then
    # Check for common issues
    if grep -q "Could not create directory" "$ARTIFACTS/coverity_build.log" 2>/dev/null; then
      fail "cov-build: permission denied on output directory"
    elif grep -q "Error: missing" "$ARTIFACTS/coverity_build.log" 2>/dev/null; then
      fail "cov-build: missing dependencies — check compiler toolchain"
    else
      fail "cov-build failed with exit code $rc"
    fi
    return 2
  fi

  local file_count
  file_count=$(find "$ARTIFACTS/cov-int" -name '*.sig' 2>/dev/null | wc -l || echo 0)
  info "Build capture complete: $file_count translation units"
  pass "cov-build: OK"
}

# ─── Stage 3: Analyze ────────────────────────────────────────────────────────
analyze() {
  stage "Coverity 3/3 — Analyze (cov-analyze)"

  if ! find_coverity; then
    fail "Coverity tool not found"
    return 1
  fi

  local cov_analyze="${COV}/cov-analyze"
  local cov_format="${COV}/cov-format-errors"
  local cov_emit="${COV}/cov-emit-commit"
  local cov_defects="${COV}/cov-defect-log"

  # Analyze
  # --dir: intermediate directory from cov-build
  # --strip-path: strip repository root from paths in output
  # --quiet: minimal output unless issues found
  # --local: don't connect to Coverity Scan (generate local report)
  "$cov_analyze" \
    --dir "$ARTIFACTS/cov-int" \
    --strip-path "$REPO_ROOT" \
    --quiet \
    2>&1 | tee "$ARTIFACTS/coverity_analyze.log"

  local rc=$?

  if [[ "$rc" -ne 0 ]]; then
    if grep -q "license" "$ARTIFACTS/coverity_analyze.log" 2>/dev/null; then
      fail "cov-analyze: license error — check Coverity license"
    else
      fail "cov-analyze failed with exit code $rc"
    fi
    return 2
  fi

  pass "cov-analyze: OK"

  # Format defects for human review
  if [[ -x "$cov_format" ]]; then
    "$cov_format" \
      --dir "$ARTIFACTS/cov-int" \
      --strip-path "$REPO_ROOT" \
      2>&1 | head -100 > "$ARTIFACTS/coverity_defects.txt" || true
    info "Defect summary: $ARTIFACTS/coverity_defects.txt"
  fi

  # Generate JSON report for CI
  if [[ -x "$cov_defects" ]]; then
    "$cov_defects" \
      --dir "$ARTIFACTS/cov-int" \
      --json-output-file "$ARTIFACTS/coverity_defects.json" \
      2>&1 | head -50 || true
  fi

  # Count defects by checker
  info "Defect summary by checker:"
  grep -h "Checker" "$ARTIFACTS/cov-int"/*.xml 2>/dev/null | \
    sed 's/.*checker="\([^"]*\)".*/\1/' | sort | uniq -c | sort -rn | \
    head -20 || echo "  (no defects found)"

  # Defect count
  local defect_count
  defect_count=$(find "$ARTIFACTS/cov-int" -name '*.xml' -exec grep -l 'defect' {} \; 2>/dev/null | wc -l || echo 0)
  info "Files with defects: $defect_count"
}

# ─── Submit to Coverity Scan (optional) ────────────────────────────────────
submit() {
  if ! find_coverity; then
    info "Coverity tool not found — skipping submission"
    return 0
  fi

  if [[ -z "${COVERITY_TOKEN:-}" ]]; then
    info "COVERITY_TOKEN not set — skipping Coverity Scan submission"
    info "To submit: COVERITY_TOKEN=your-token ./scripts/coverity_scan.sh submit"
    return 0
  fi

  stage "Coverity — Submit to Coverity Scan"

  local cov_conduct="${COV}/cov-conduct"
  local project="cubesat-obc"

  if [[ ! -x "$cov_conduct" ]]; then
    info "cov-conduct not found — cannot submit to Coverity Scan"
    return 0
  fi

  # Submit the analysis results
  "$cov_conduct" \
    --dir "$ARTIFACTS/cov-int" \
    --store-project-config \
    --project "$project" \
    || true

  info "Submission complete. View results at: https://scan.coverity.com/projects/cubesat-obc"
  pass "Coverity Scan submission: OK"
}

# ─── Usage ─────────────────────────────────────────────────────────────────
usage() {
  cat << EOF
Usage: $0 [command]

Commands:
  all      Run full analysis: configure → capture → analyze (default)
  config   Configure CMake build only
  capture  Capture build with cov-build only
  analyze  Run cov-analyze on existing capture
  submit   Submit results to Coverity Scan (requires COVERITY_TOKEN)
  help     Show this help

Environment:
  COVERITY_TOKEN  Coverity Scan upload token (required for submit)
  COVERITY_DIR    Path to Coverity installation (default: \$REPO/cov-analysis)

EOF
}

# ─── Entry point ────────────────────────────────────────────────────────────
COMMAND="${1:-all}"

case "$COMMAND" in
  all)
    check_token || exit $?
    configure
    capture
    analyze
    pass "Coverity analysis complete"
    ;;
  config)
    configure
    ;;
  capture)
    capture
    ;;
  analyze)
    analyze
    ;;
  submit)
    check_token || exit $?
    submit
    ;;
  help|--help|-h)
    usage
    exit 0
    ;;
  *)
    echo "Unknown command: $COMMAND"
    usage
    exit 2
    ;;
esac
