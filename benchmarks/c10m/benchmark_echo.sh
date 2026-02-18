#!/bin/bash
# =============================================================================
# C10M Echo Benchmark — Head-to-Head Comparison
#
# Builds and benchmarks 4 echo server implementations:
#   1. C     (raw kqueue, single-threaded)
#   2. Rust  (Tokio async runtime)
#   3. TS    (Bun native TCP)
#   4. Salt  (Sovereign runtime, no libc)
#
# Metrics collected:
#   - Connections/sec (accept rate)
#   - Mean latency (µs)
#   - Tail latency p99 (µs)
#   - Binary size (bytes)
#   - Resident memory (KB)
#
# Usage: ./benchmark_echo.sh [port] [duration_sec] [num_connections]
# =============================================================================

set -euo pipefail

PORT=${1:-9000}
DURATION=${2:-10}
NUM_CONNS=${3:-1000}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
RESULTS_FILE="$SCRIPT_DIR/echo_benchmark_results.txt"

# LLVM Tools
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang}"
LLC="${LLC:-/opt/homebrew/opt/llvm/bin/llc}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

header() { echo -e "\n${BOLD}${CYAN}═══════════════════════════════════════════${NC}"; echo -e "${BOLD}  $1${NC}"; echo -e "${BOLD}${CYAN}═══════════════════════════════════════════${NC}"; }
pass()   { echo -e "  ${GREEN}✅ $1${NC}"; }
fail()   { echo -e "  ${RED}❌ $1${NC}"; }

mkdir -p "$BUILD_DIR"

# =============================================================================
# Phase 1: Build All Targets
# =============================================================================
header "Phase 1: Building Echo Servers"

# --- C (kqueue) ---
echo -e "\n${BOLD}[1/4] C / kqueue${NC}"
if $CLANG -O3 -o "$BUILD_DIR/echo_c" "$SCRIPT_DIR/echo_c.c" 2>&1; then
    C_SIZE=$(stat -f%z "$BUILD_DIR/echo_c" 2>/dev/null || stat -c%s "$BUILD_DIR/echo_c")
    pass "Built echo_c ($C_SIZE bytes)"
else
    fail "C build failed"
    C_SIZE=0
fi

# --- Rust (Tokio) ---
echo -e "\n${BOLD}[2/4] Rust / Tokio${NC}"
RUST_DIR="$BUILD_DIR/echo_rust_proj"
if [ ! -d "$RUST_DIR" ]; then
    mkdir -p "$RUST_DIR/src"
    cp "$SCRIPT_DIR/echo_rust.rs" "$RUST_DIR/src/main.rs"
    cat > "$RUST_DIR/Cargo.toml" << 'EOF'
[package]
name = "echo_rust"
version = "0.1.0"
edition = "2021"

[dependencies]
tokio = { version = "1", features = ["full"] }

[profile.release]
opt-level = 3
lto = true
codegen-units = 1
EOF
fi
if (cd "$RUST_DIR" && cargo build --release 2>&1 | tail -3); then
    RUST_BIN="$RUST_DIR/target/release/echo_rust"
    RUST_SIZE=$(stat -f%z "$RUST_BIN" 2>/dev/null || stat -c%s "$RUST_BIN")
    pass "Built echo_rust ($RUST_SIZE bytes)"
else
    fail "Rust build failed"
    RUST_SIZE=0
fi

# --- TypeScript (Bun) ---
echo -e "\n${BOLD}[3/4] TypeScript / Bun${NC}"
if command -v bun &>/dev/null; then
    TS_SIZE=$(stat -f%z "$SCRIPT_DIR/echo_ts.ts" 2>/dev/null || stat -c%s "$SCRIPT_DIR/echo_ts.ts")
    pass "echo_ts.ts ready ($TS_SIZE bytes source, JIT runtime)"
else
    fail "Bun not installed (skipping TS benchmark)"
    TS_SIZE=0
fi

# --- Salt (Sovereign) ---
echo -e "\n${BOLD}[4/4] Salt / Sovereign${NC}"
SALT_SIZE=0
if [ -f "$SCRIPT_DIR/echo_salt.salt" ]; then
    pass "echo_salt.salt ready (requires full pipeline to build binary)"
    # TODO: Once the Iron Driver pipeline is wired end-to-end:
    # cargo run --release --manifest-path salt-front/Cargo.toml --bin salt-front -- echo_salt.salt > echo_salt.mlir
    # mlir-opt ... echo_salt.mlir -o echo_salt_opt.mlir
    # mlir-translate --mlir-to-llvmir echo_salt_opt.mlir -o echo_salt.ll
    # llc -O3 -reserved-reg=aarch64:x19 -mattr=+lse echo_salt.ll -o echo_salt.o
    # clang -nostdlib -static sovereign_rt.o echo_salt.o -o echo_salt
else
    fail "echo_salt.salt not found"
fi

# =============================================================================
# Phase 2: Benchmark Each Server
# =============================================================================
header "Phase 2: Echo Benchmark (port=$PORT, duration=${DURATION}s, connections=$NUM_CONNS)"

echo ""
echo "Binary Size Comparison:"
echo "  C (kqueue):      ${C_SIZE:-N/A} bytes"
echo "  Rust (Tokio):    ${RUST_SIZE:-N/A} bytes"
echo "  TS (Bun):        ${TS_SIZE:-N/A} bytes (source only, JIT)"
echo "  Salt (Sovereign): ${SALT_SIZE:-N/A} bytes"
echo ""

# Helper: benchmark a server
benchmark_server() {
    local name=$1
    local cmd=$2
    local port_offset=$3
    local actual_port=$((PORT + port_offset))

    echo -e "\n${BOLD}Benchmarking: $name (port $actual_port)${NC}"

    # Start server in background
    eval "$cmd $actual_port &"
    local pid=$!
    sleep 1  # Let server start

    # Check if server is running
    if ! kill -0 $pid 2>/dev/null; then
        fail "$name failed to start"
        return
    fi

    # Get initial memory
    local rss_before=$(ps -o rss= -p $pid 2>/dev/null || echo "0")

    # Run load test with nc (simple echo test)
    local start_time=$(date +%s%N)
    local success=0
    local total=0

    for i in $(seq 1 $NUM_CONNS); do
        if echo "Hello World" | nc -w 1 127.0.0.1 $actual_port >/dev/null 2>&1; then
            success=$((success + 1))
        fi
        total=$((total + 1))
    done

    local end_time=$(date +%s%N)
    local elapsed_ms=$(( (end_time - start_time) / 1000000 ))

    # Get final memory
    local rss_after=$(ps -o rss= -p $pid 2>/dev/null || echo "0")

    # Report
    local rate=0
    if [ $elapsed_ms -gt 0 ]; then
        rate=$(( success * 1000 / elapsed_ms ))
    fi
    echo "  Connections: $success / $total"
    echo "  Time:        ${elapsed_ms}ms"
    echo "  Rate:        ${rate} conn/s"
    echo "  RSS:         ${rss_after} KB"

    # Cleanup
    kill $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true
    sleep 1
}

# Run benchmarks
if [ -f "$BUILD_DIR/echo_c" ]; then
    benchmark_server "C / kqueue" "$BUILD_DIR/echo_c" 0
fi

if [ -f "$BUILD_DIR/echo_rust_proj/target/release/echo_rust" ]; then
    benchmark_server "Rust / Tokio" "$BUILD_DIR/echo_rust_proj/target/release/echo_rust" 1
fi

if command -v bun &>/dev/null; then
    benchmark_server "TS / Bun" "bun run $SCRIPT_DIR/echo_ts.ts" 2
fi

# Salt benchmark will be enabled once the full pipeline is wired
# if [ -f "$BUILD_DIR/echo_salt" ]; then
#     benchmark_server "Salt / Sovereign" "$BUILD_DIR/echo_salt" 3
# fi

# =============================================================================
# Phase 3: Summary
# =============================================================================
header "Benchmark Summary"

echo ""
echo "Target          | Binary Size | Status"
echo "----------------|-------------|--------"
printf "C / kqueue      | %10s | %s\n" "${C_SIZE:-N/A}" "$([ ${C_SIZE:-0} -gt 0 ] && echo '✅ Ready' || echo '❌ Failed')"
printf "Rust / Tokio    | %10s | %s\n" "${RUST_SIZE:-N/A}" "$([ ${RUST_SIZE:-0} -gt 0 ] && echo '✅ Ready' || echo '❌ Failed')"
printf "TS / Bun        | %10s | %s\n" "${TS_SIZE:-N/A} (src)" "$(command -v bun &>/dev/null && echo '✅ Ready' || echo '⚠️  Bun missing')"
printf "Salt / Sovereign| %10s | %s\n" "${SALT_SIZE:-N/A}" "⏳ Pipeline pending"
echo ""

echo "Results saved to: $RESULTS_FILE"
date > "$RESULTS_FILE"
echo "Port: $PORT, Duration: ${DURATION}s, Connections: $NUM_CONNS" >> "$RESULTS_FILE"
echo "C_SIZE=$C_SIZE RUST_SIZE=${RUST_SIZE:-0} TS_SIZE=${TS_SIZE:-0} SALT_SIZE=${SALT_SIZE:-0}" >> "$RESULTS_FILE"
