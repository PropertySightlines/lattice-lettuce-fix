#!/usr/bin/env zsh
set -euo pipefail

export PATH="/opt/homebrew/opt/llvm@18/bin:$PATH"
PROJECT=${1:-/Users/kevin/projects/lattice}
SF=$PROJECT/salt-front
OUT=/tmp/salt_sweep
MODEL=$PROJECT/.bench_basalt/models/stories15M.bin
TOK=$PROJECT/.bench_basalt/models/tokenizer.bin

mkdir -p $OUT

concat_basalt() {
    local dest=$1
    local kernels_override=${2:-}
    echo "package main" > "$dest"
    echo "use std.core.ptr.Ptr" >> "$dest"
    echo "" >> "$dest"
    for f in kernels sampler transformer model_loader tokenizer main; do
        local src_file="$PROJECT/basalt/src/$f.salt"
        if [[ "$f" == "kernels" && -n "$kernels_override" ]]; then
            src_file="$kernels_override"
        fi
        grep -v "^package " "$src_file" | grep -v "^use basalt\." >> "$dest"
    done
}

build_and_bench() {
    local label=$1
    local mlir_flags=$2
    local src=$3
    local compiler=$4

    printf "%-35s " "$label"

    $compiler "$src" > $OUT/tmp.mlir 2>$OUT/tmp_err.log || { echo "COMPILE FAIL"; return; }

    eval mlir-opt $OUT/tmp.mlir $mlir_flags -o $OUT/tmp.opt.mlir 2>$OUT/tmp_opt.log || { echo "MLIR-OPT FAIL"; return; }
    sed -i '' '/"salt.verify"/d' $OUT/tmp.opt.mlir 2>/dev/null || true

    mlir-translate --mlir-to-llvmir $OUT/tmp.opt.mlir -o $OUT/tmp.ll 2>$OUT/tmp_tr.log || { echo "TRANSLATE FAIL"; return; }

    clang -O3 -ffast-math -march=native $OUT/tmp.ll $SF/runtime.c -o $OUT/tmp_bin -lm -Wno-override-module 2>$OUT/tmp_cl.log || { echo "CLANG FAIL"; return; }

    local best=0
    for run in 1 2 3; do
        local output=$($OUT/tmp_bin $MODEL $TOK 2>&1)
        local toks=$(echo "$output" | grep "tok/s:" | awk '{print $2}')
        if [[ -z "$toks" ]]; then
            toks=$(echo "$output" | grep -oE '[0-9]+\.[0-9]+ tok/s' | awk '{print $1}')
        fi
        toks=${toks:-0}
        toks=${toks%.*}
        if (( toks > best )); then best=$toks; fi
    done
    echo "${best} tok/s"
}

BASELINE_FLAGS="--allow-unregistered-dialect --convert-scf-to-cf --convert-cf-to-llvm --convert-arith-to-llvm --convert-func-to-llvm --reconcile-unrealized-casts"
OPT_FLAGS="--allow-unregistered-dialect --canonicalize --cse --loop-invariant-code-motion --sccp --canonicalize --cse --convert-scf-to-cf --convert-cf-to-llvm --convert-arith-to-llvm --convert-func-to-llvm --reconcile-unrealized-casts"

echo "╔══════════════════════════════════════════════╗"
echo "║   Basalt Performance Sweep                   ║"
echo "║   $(date '+%Y-%m-%d %H:%M:%S')                        ║"
echo "╚══════════════════════════════════════════════╝"
echo ""
echo "Model: stories15M.bin (256 tokens)"
echo ""

concat_basalt $OUT/b.salt

echo "━━━ Phase 1: Compiler + MLIR Passes ━━━"
if [[ -f "$SF/target/debug/salt-front" ]]; then
    build_and_bench "debug-compiler + no-opt" "$BASELINE_FLAGS" "$OUT/b.salt" "$SF/target/debug/salt-front"
    build_and_bench "debug-compiler + full-opt" "$OPT_FLAGS" "$OUT/b.salt" "$SF/target/debug/salt-front"
fi
if [[ -f "$SF/target/release/salt-front" ]]; then
    build_and_bench "release-compiler + no-opt" "$BASELINE_FLAGS" "$OUT/b.salt" "$SF/target/release/salt-front"
    build_and_bench "release-compiler + full-opt" "$OPT_FLAGS" "$OUT/b.salt" "$SF/target/release/salt-front"
fi
echo ""

echo "━━━ Phase 2: Tiling Sweep (tile=d%N hint) ━━━"
BEST_COMPILER="$SF/target/debug/salt-front"
[[ ! -f "$BEST_COMPILER" ]] && BEST_COMPILER="$SF/target/release/salt-front"

for TILE in 4 16 64 256; do
    local tile_kernels=$OUT/kernels_tile${TILE}.salt
    cp $PROJECT/basalt/src/kernels.salt $tile_kernels
    sed -i '' "s/let d4 = d - (d % 4);/let d4 = d - (d % ${TILE});/" $tile_kernels
    sed -i '' "s/let m4 = m - (m % 4);/let m4 = m - (m % ${TILE});/" $tile_kernels

    concat_basalt $OUT/b_tile${TILE}.salt $tile_kernels
    build_and_bench "tile=${TILE} (no-opt)" "$BASELINE_FLAGS" "$OUT/b_tile${TILE}.salt" "$BEST_COMPILER"
    build_and_bench "tile=${TILE} (full-opt)" "$OPT_FLAGS" "$OUT/b_tile${TILE}.salt" "$BEST_COMPILER"
done

echo ""
echo "━━━ Done ━━━"
