#!/bin/bash
# =============================================================================
# HTTP Server Benchmark — Salt vs C vs Node.js
# =============================================================================
# Usage: ./run_http_benchmark.sh
# Prerequisites: wrk, compiled Salt server, compiled C server, node
# =============================================================================

set -e

DURATION=10
THREADS=2
CONNECTIONS=100
ENDPOINT="/health"
PORT=8080
URL="http://localhost:${PORT}${ENDPOINT}"

SALT_BIN="/tmp/salt_http_server"
C_BIN="/tmp/http_server_c"
NODE_SCRIPT="$(dirname "$0")/node_http_server.js"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

print_header() {
    echo ""
    echo -e "${BOLD}${CYAN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}${CYAN}  $1${NC}"
    echo -e "${BOLD}${CYAN}════════════════════════════════════════════════════════════${NC}"
}

wait_for_server() {
    for i in $(seq 1 20); do
        if curl -s http://localhost:${PORT}/health > /dev/null 2>&1; then
            return 0
        fi
        sleep 0.25
    done
    echo "ERROR: Server failed to start on port ${PORT}"
    return 1
}

kill_server() {
    lsof -ti:${PORT} 2>/dev/null | xargs kill -9 2>/dev/null || true
    sleep 0.5
}

run_benchmark() {
    local name="$1"
    print_header "Benchmarking: ${name}"
    echo -e "  wrk -t${THREADS} -c${CONNECTIONS} -d${DURATION}s ${URL}"
    echo ""
    wrk -t${THREADS} -c${CONNECTIONS} -d${DURATION}s ${URL}
    echo ""
}

# Ensure clean state
kill_server

# =============================================================================
# 1. Salt Server
# =============================================================================
if [ -f "$SALT_BIN" ]; then
    print_header "Starting Salt HTTP Server"
    $SALT_BIN &
    SALT_PID=$!
    wait_for_server
    
    run_benchmark "Salt (MLIR/LLVM, kqueue)"
    
    kill_server
else
    echo "WARNING: Salt binary not found at $SALT_BIN — skipping"
fi

# =============================================================================
# 2. C Baseline
# =============================================================================
if [ -f "$C_BIN" ]; then
    print_header "Starting C HTTP Server"
    $C_BIN &
    C_PID=$!
    wait_for_server
    
    run_benchmark "C (clang -O3, kqueue)"
    
    kill_server
else
    echo "WARNING: C binary not found at $C_BIN — skipping"
fi

# =============================================================================
# 3. Node.js
# =============================================================================
if [ -f "$NODE_SCRIPT" ]; then
    print_header "Starting Node.js HTTP Server"
    node "$NODE_SCRIPT" &
    NODE_PID=$!
    wait_for_server
    
    run_benchmark "Node.js (v$(node --version), http module)"
    
    kill_server
else
    echo "WARNING: Node script not found at $NODE_SCRIPT — skipping"
fi

# =============================================================================
# Summary
# =============================================================================
print_header "Benchmark Complete"
echo -e "  Config: ${THREADS} threads, ${CONNECTIONS} connections, ${DURATION}s duration"
echo -e "  Endpoint: ${ENDPOINT}"
echo ""
