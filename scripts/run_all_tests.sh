#!/usr/bin/env zsh
# =============================================================================
# Salt Test Suite Runner — Run All Tests
# =============================================================================
# Runs all test_*.salt files through the full pipeline and reports results.
#
# Usage:
#   ./scripts/run_all_tests.sh                  # Run all tests
#   ./scripts/run_all_tests.sh --filter thread  # Run tests matching "thread"
# =============================================================================

set -uo pipefail

SCRIPT_DIR="${0:A:h}"
PROJECT_ROOT="${SCRIPT_DIR:h}"
RUN_TEST="$SCRIPT_DIR/run_test.sh"

FILTER="${1:-}"
[[ "$FILTER" == "--filter" ]] && FILTER="${2:-}" || true

PASSED=0
FAILED=0
SKIPPED=0
FAILURES=()

echo "🧪 Salt Test Suite"
echo "==================="
echo ""

for test_file in "$PROJECT_ROOT"/tests/test_*.salt; do
    BASENAME=$(basename "$test_file" .salt)

    # Apply filter if provided
    if [[ -n "$FILTER" && "$BASENAME" != *"$FILTER"* ]]; then
        continue
    fi

    printf "%-40s " "$BASENAME"

    # Run the test, capture output and exit code
    OUTPUT=$("$RUN_TEST" "$test_file" 2>&1)
    EXIT_CODE=$?

    if [[ $EXIT_CODE -eq 0 ]]; then
        echo "✅ PASS"
        ((PASSED++))
    else
        echo "❌ FAIL (exit $EXIT_CODE)"
        ((FAILED++))
        FAILURES+=("$BASENAME")
    fi
done

echo ""
echo "==================="
echo "Results: $PASSED passed, $FAILED failed"

if [[ $FAILED -gt 0 ]]; then
    echo ""
    echo "Failed tests:"
    for f in "${FAILURES[@]}"; do
        echo "  ❌ $f"
    done
    exit 1
fi

echo "✅ All tests passed!"
