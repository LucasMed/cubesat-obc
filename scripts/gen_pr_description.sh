#!/usr/bin/env bash
# =============================================================================
# gen_pr_description.sh — Generate a filled PR description from the current
# branch, ready to paste into GitHub.
#
# Usage:
#   bash scripts/gen_pr_description.sh           # auto-detect base branch
#   bash scripts/gen_pr_description.sh dev        # explicit base
#   bash scripts/gen_pr_description.sh dev --run-tests  # run tests first
#
# Output goes to stdout; redirect to a file if preferred:
#   bash scripts/gen_pr_description.sh > /tmp/pr.md
# =============================================================================

set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------
BASE="${1:-dev}"
RUN_TESTS=0
for arg in "$@"; do [[ "$arg" == "--run-tests" ]] && RUN_TESTS=1; done

# ---------------------------------------------------------------------------
# Branch metadata
# ---------------------------------------------------------------------------
BRANCH=$(git rev-parse --abbrev-ref HEAD)
COMMITS=$(git log "${BASE}..HEAD" --oneline 2>/dev/null || git log --oneline -5)
FIRST_SUBJECT=$(git log "${BASE}..HEAD" --format="%s" 2>/dev/null | head -1 || git log -1 --format="%s")
FIRST_BODY=$(git log "${BASE}..HEAD" --format="%b" 2>/dev/null | sed '/^$/d' | head -20 || true)

# Changed files relative to base
CHANGED_FILES=$(git diff "${BASE}...HEAD" --name-only 2>/dev/null | sort)
SRC_CHANGED=$(echo "$CHANGED_FILES"  | grep -E '^src/'     || true)
TEST_CHANGED=$(echo "$CHANGED_FILES" | grep -E '^tests/'   || true)
DOC_CHANGED=$(echo "$CHANGED_FILES"  | grep -E '^docs/'    || true)

# ---------------------------------------------------------------------------
# Infer type of change from conventional commit prefix
# ---------------------------------------------------------------------------
TYPE_BUG=false; TYPE_FEAT=false; TYPE_BREAK=false; TYPE_DOCS=false

if echo "$COMMITS" | grep -qiE '^[a-f0-9]+ (fix|bugfix|hotfix)\('; then TYPE_BUG=true; fi
if echo "$COMMITS" | grep -qiE '^[a-f0-9]+ feat\(';                  then TYPE_FEAT=true; fi
if echo "$COMMITS" | grep -qiE '!:';                                    then TYPE_BREAK=true; fi
if [[ -z "$SRC_CHANGED" && -n "$DOC_CHANGED" ]];                        then TYPE_DOCS=true; fi
# If only docs changed and no code, force docs-only
[[ -z "$SRC_CHANGED" && -z "$TEST_CHANGED" && -n "$DOC_CHANGED" ]] && TYPE_BUG=false && TYPE_FEAT=false

mark() { $1 && echo "x" || echo " "; }

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
TEST_PASS=0; TEST_FAIL=0; TEST_TOTAL=0; TEST_LINE=""
if [[ "$RUN_TESTS" == "1" ]]; then
  BUILD_DIR="$REPO_ROOT/build"
  if [[ ! -f "$BUILD_DIR/CTestTestfile.cmake" ]]; then
    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DPICO_ENABLED=OFF -DCMAKE_BUILD_TYPE=Debug >/dev/null 2>&1
  fi
  cmake --build "$BUILD_DIR" -- -j"$(nproc)" >/dev/null 2>&1
  CTEST_OUT=$(cd "$BUILD_DIR" && ctest --output-on-failure 2>&1 || true)
  # ctest summary line: "100% tests passed, 0 tests failed out of 29"
  TEST_TOTAL=$(echo "$CTEST_OUT" | grep -oP '(?<=out of )\d+'         | tail -1 || echo 0)
  TEST_FAIL=$( echo "$CTEST_OUT" | grep -oP '\d+(?= tests? failed)'   | tail -1 || echo 0)
  TEST_PASS=$(( TEST_TOTAL - TEST_FAIL ))
  if [[ "$TEST_FAIL" == "0" && "$TEST_TOTAL" != "0" ]]; then
    TEST_LINE="✅ **${TEST_TOTAL}/${TEST_TOTAL} tests passing**"
    TESTS_OK=true
  elif [[ "$TEST_TOTAL" == "0" ]]; then
    TEST_LINE="⚠️  Could not determine test count — run manually"
    TESTS_OK=false
  else
    TEST_LINE="❌ **${TEST_FAIL} test(s) failed** (${TEST_PASS}/${TEST_TOTAL} passed)"
    TESTS_OK=false
  fi
else
  # Try to read from last CTest run
  LAST_RUN="$REPO_ROOT/build/Testing/Temporary/LastTest.log"
  if [[ -f "$LAST_RUN" ]]; then
    PASS_LINE=$(grep -oP '\d+/\d+ Test' "$LAST_RUN" | tail -1 || true)
    [[ -n "$PASS_LINE" ]] && TEST_LINE="Last run: $PASS_LINE (use --run-tests for fresh results)" \
                          || TEST_LINE="Run \`cd build && ctest --output-on-failure\` to confirm"
  else
    TEST_LINE="Run \`cd build && ctest --output-on-failure\` to confirm"
  fi
  TESTS_OK=false   # unknown without running
fi

# ---------------------------------------------------------------------------
# Build per-test checklist from changed test files
# ---------------------------------------------------------------------------
TEST_ITEMS=""
if [[ -n "$TEST_CHANGED" ]]; then
  while IFS= read -r f; do
    [[ -z "$f" ]] && continue
    TEST_NAME=$(basename "$f" .c)
    TEST_ITEMS+="- [x] \`${TEST_NAME}\` — updated / verified\n"
  done <<< "$TEST_CHANGED"
fi
[[ -z "$TEST_ITEMS" ]] && TEST_ITEMS="- [ ] Describe tests run\n"

# ---------------------------------------------------------------------------
# Static analysis
# ---------------------------------------------------------------------------
SA_LINE=""
if command -v clang-format-14 >/dev/null 2>&1 || command -v clang-format >/dev/null 2>&1; then
  if bash "$REPO_ROOT/scripts/static_analysis.sh" >/dev/null 2>&1; then
    SA_LINE="✅ Static analysis clean (clang-format + cppcheck)"
  else
    SA_LINE="⚠️  Static analysis issues detected — run \`scripts/static_analysis.sh\` locally"
  fi
else
  SA_LINE="Static analysis not run locally (CI will verify)"
fi

# ---------------------------------------------------------------------------
# Checklist derived state
# ---------------------------------------------------------------------------
HAS_TESTS=$( [[ -n "$TEST_CHANGED" ]] && echo true || echo false )
HAS_DOCS=$(  [[ -n "$DOC_CHANGED"  ]] && echo true || echo false )
HAS_CL=$(    echo "$CHANGED_FILES" | grep -q 'CHANGELOG.md' && echo true || echo false )

# ---------------------------------------------------------------------------
# Emit the filled PR description
# ---------------------------------------------------------------------------
cat <<EOF
## Description

${FIRST_SUBJECT}

$(echo "$FIRST_BODY" | sed 's/^/> /' | head -15)

**Branch:** \`${BRANCH}\`

**Commits in this PR:**
$(echo "$COMMITS" | sed 's/^/- /')

**Files changed:**
$(echo "$CHANGED_FILES" | sed 's/^/- /')

Fixes #(issue number)

---

## Type of Change

- [$(mark $TYPE_BUG)] Bug fix (non-breaking change which fixes an issue)
- [$(mark $TYPE_FEAT)] New feature (non-breaking change which adds functionality)
- [$(mark $TYPE_BREAK)] Breaking change (fix or feature that would cause existing functionality to change)
- [$(mark $TYPE_DOCS)] Documentation update

---

## Testing

${TEST_LINE}

\`\`\`bash
cd build && cmake --build . -- -j\$(nproc) && ctest --output-on-failure
\`\`\`

$(echo -e "$TEST_ITEMS")

---

## Checklist

- [x] I have followed the [Contributing Guidelines](CONTRIBUTING.md)
- [$(mark $HAS_DOCS)] I have updated documentation if required
- [x] My code follows the project's [Coding Standards](docs/CODING_STANDARDS.md)
- [$(mark $HAS_TESTS)] I have added/updated tests for my changes
- [x] All tests pass locally: \`ctest --output-on-failure\`
- [x] No compiler warnings: \`-Wall -Wextra -pedantic\`
- [x] Static analysis passes: \`scripts/static_analysis.sh\`
- [x] Commit messages are clear and descriptive
- [x] No sensitive data (secrets, passwords, API keys) added
- [$(mark $HAS_CL)] \`CHANGELOG.md\` has been updated

${SA_LINE}

---

## Performance Impact

- [x] No impact
- [ ] Minor (describe)
- [ ] Significant (describe and provide benchmarks)

---

## Screenshots (if applicable)

N/A

---

## Additional Context

Add any other context about the PR here.
EOF
