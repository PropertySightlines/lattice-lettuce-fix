#!/bin/bash
set -e
echo "--- Phase 30: Saturation Attack ---"

# 1. Cleanup
echo "Cleaning up old processes and artifacts..."
pkill -9 salt_front || true
pkill -9 cargo-llvm-cov || true
rm -f salt-front/default_*.profraw
rm -f salt-opt-*.profraw

# 2. Rust Frontend Coverage
echo "[1/4] Running Saturation Suite (Rust Coverage)..."
cd salt-front
INSTA_UPDATE=always cargo llvm-cov --summary-only
cd ..

# 3. C++ Verification Coverage
echo "[2/4] Generating MLIR for Pathological Cases..."
mkdir -p coverage_artifacts
rm -f coverage_artifacts/*.mlir

FILES=(
    "salt-front/tests/cases/coverage_matrix.salt"
    "salt-front/tests/cases/coverage_deep_structs.salt"
    "salt-front/tests/cases/coverage_intrinsics.salt"
    "salt-front/tests/cases/coverage_loops.salt"
    "salt-front/tests/cases/coverage_generics.salt"
    "salt-front/tests/cases/coverage_arrays_floats.salt"
    "salt-front/tests/cases/verify_timeout.salt"
    "salt-front/tests/cases/verify_unsat.salt"
)

for f in "${FILES[@]}"; do
    name=$(basename "$f" .salt)
    echo "Compiling $f..."
    ./salt-front/target/debug/salt-front "$f" --release > "coverage_artifacts/$name.mlir" || echo "Note: $name compilation failed"
done

echo "[3/4] Running Z3 Pathological Proofs (C++ Coverage)..."
export LLVM_PROFILE_FILE="salt-opt-%p.profraw"
for f in coverage_artifacts/*.mlir; do
    if [ -s "$f" ]; then
        echo "Verifying $f..."
        # We allow failures here as long as the binary runs and instruments
        ./salt/build/salt-opt --verify --emit-llvm "$f" --output "coverage_artifacts/$(basename "$f" .mlir).ll" || true
    fi
done

# 4. Merge C++ Profile
echo "[4/4] Merging C++ Profiles..."
xcrun llvm-profdata merge -sparse salt-opt-*.profraw -o salt-opt.profdata

# 5. Generate Reports
echo "--- Saturation Results (Rust) ---"
cd salt-front
cargo llvm-cov --summary-only
cd ..

echo "--- Saturation Results (C++) ---"
if [ -f salt-opt.profdata ]; then
    # Filter to only show Z3Verify.cpp in the summary
    xcrun llvm-cov report ./salt/build/salt-opt -instr-profile=salt-opt.profdata | grep "passes/Z3Verify.cpp" || echo "Z3Verify.cpp not found in report"
else
    echo "No C++ profile data found."
fi

echo "Saturation Complete!"
