#!/usr/bin/env zsh
# =============================================================================
# Salt Compiler Build Script
# =============================================================================
# Builds the salt-front compiler with Z3 dependencies.
#
# Usage:
#   ./scripts/build.sh              # Debug build
#   ./scripts/build.sh --release    # Release build
#   ./scripts/build.sh --test       # Build + run cargo tests
# =============================================================================

set -euo pipefail

SCRIPT_DIR="${0:A:h}"
PROJECT_ROOT="${SCRIPT_DIR:h}"
SALT_FRONT="$PROJECT_ROOT/salt-front"

# Z3 dependencies
export Z3_SYS_Z3_HEADER=/opt/homebrew/include/z3.h
export LIBRARY_PATH=/opt/homebrew/lib
export DYLD_LIBRARY_PATH=/opt/homebrew/lib

cd "$SALT_FRONT"

if [[ "${1:-}" == "--release" ]]; then
    echo "🔨 Building salt-front (release)..."
    cargo build --release
    echo "✅ Release build complete: $SALT_FRONT/target/release/salt-front"
elif [[ "${1:-}" == "--test" ]]; then
    echo "🧪 Building and testing salt-front..."
    cargo test 2>&1 | tail -20
    echo "✅ Tests complete"
else
    echo "🔨 Building salt-front (debug)..."
    cargo build
    echo "✅ Debug build complete: $SALT_FRONT/target/debug/salt-front"
fi
