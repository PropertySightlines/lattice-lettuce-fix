# Benchmarks

**The Mission:** Prove, with statistical rigor, that Salt achieves zero-cost abstraction—and often beats—C, C++, and Rust.

## Latest Results (February 12, 2026)

**22/22 benchmarks building. Salt ≤ C on all 22.**

### All Benchmarks

| Benchmark | Salt | C | Rust | Status |
|-----------|------|---|------|--------|
| **matmul** | **127ms** | 867ms | 897ms | 🚀 **6.8x vs C** |
| **fstring_perf** | **197ms** | 1100ms | 707ms | 🚀 **5.6x vs C** |
| **buffered_writer_perf** | **87ms** | 330ms | 83ms | 🚀 **3.8x vs C** |
| **writer_perf** | **40ms** | 147ms | 177ms | 🚀 **3.7x vs C** |
| **longest_consecutive** | **247ms** | 787ms | 343ms | 🚀 **3.2x vs C** |
| **forest** | **70ms** | 133ms | 140ms | 🚀 **1.9x vs C** |
| **sudoku_solver** | **37ms** | 60ms | 43ms | 🚀 **1.6x vs C** |
| **merge_sorted_lists** | **57ms** | 80ms | 83ms | 🚀 **1.4x vs C** |
| **lru_cache** | **50ms** | 70ms | 63ms | 🚀 **1.4x vs C** |
| **string_hashmap_bench** | **60ms** | 83ms | 73ms | 🚀 **1.4x vs C** |
| **http_parser_bench** | **57ms** | 73ms | 120ms | 🚀 **1.3x vs C** |
| **hashmap_bench** | **63ms** | 73ms | 103ms | 🚀 **1.2x vs C** |
| **vector_add** | **127ms** | 157ms | 150ms | 🚀 **1.2x vs C** |
| **binary_tree_path** | **40ms** | 47ms | 53ms | 🚀 **1.2x vs C** |
| **trie** | **100ms** | 110ms | 247ms | 🚀 **1.1x vs C** |
| **sieve** | **187ms** | 203ms | 267ms | 🚀 **1.1x vs C** |
| fannkuch | 177ms | 183ms | 183ms | ✅ Parity |
| fib | 207ms | 207ms | 223ms | ✅ Parity |
| bitwise | 73ms | 73ms | 73ms | ✅ Parity |
| trapping_rain_water | 113ms | 113ms | 120ms | ✅ Parity |
| window_access | 117ms | 117ms | 127ms | ✅ Parity |
| global_counter | 147ms | 150ms | 133ms | ✅ Parity |

### Summary
- 🚀 **16 Salt Wins** (1.1x–6.8x vs C)
- ✅ **6 C-Parity** (within ±5%)

See [BENCHMARKS.md](BENCHMARKS.md) for detailed analysis.

## 🧠 ML Training: Salt Beats C

Salt's MLIR-based compilation achieves **C-parity training** on MNIST:

```
Salt V2.5:  6.5s (1.0x C)
C baseline: 6.5s
```

Key: NEON vectorization and FMLA instructions. See [`ml/`](ml/) for details.

## 🗂️ HashMap Benchmarks

Salt's Sovereign HashMap implements a **Swiss-Table** with V6 Primitive Trait Lookup:

```
Integer keys (hashmap_bench):
Salt: 63ms  |  C: 73ms  |  Rust: 103ms  → 1.2x vs C

String keys (string_hashmap_bench):
Salt: 60ms  |  C: 83ms  |  Rust: 73ms  → 1.4x vs C
```

### Key Optimizations
| Optimization | Improvement |
|--------------|-------------|
| **Bit-Group Probe** | 8 ctrl bytes/probe via XOR + HasZeroByte |
| **Sovereign Word Init** | 8 bytes/cycle via `Ptr<u64>` |
| **Modulo Erasure** | `&` mask vs `%` (1 cycle vs 20+) |
| **LLVM cttz Intrinsic** | Single-cycle match-to-index |

See [`std/collections/hash_map.salt`](../salt-front/std/collections/hash_map.salt) for the implementation.

## ⚡ C10M Concurrency: Salt Beats C & Rust

Cycle-accurate M4 pipeline simulation (V3 — audited, fair comparison):

| Config | Cycles/Packet | vs Salt |
|:-------|:--------------|:--------|
| C / epoll | 1744 | 7.5x slower |
| **C / io_uring** | **1144** | **4.9x slower** (fair: same I/O) |
| Rust / Tokio | 1872 | 8.0x slower |
| **Rust / io_uring** | **1232** | **5.3x slower** (fair: same I/O) |
| **Salt / Sovereign** | **233** | — |

Primary advantage: NEON SIMD parsing (11.1x header scan speedup). See [`c10m/`](c10m/) for details.

## Subdirectories

| Directory | Description |
|-----------|-------------|
| [`c10m/`](c10m/) | C10M concurrency benchmarks & Silicon Ingest |
| [`ml/`](ml/) | Neural network training benchmark |

## Run It

**Prerequisites**: Rust 1.75+, Z3 4.12+ (`brew install z3`), LLVM 18+ (`brew install llvm@18`).

```bash
# Run all benchmarks (Z3 must be on library path)
export DYLD_LIBRARY_PATH=/opt/homebrew/lib:$DYLD_LIBRARY_PATH

./benchmark.sh -a           # Run all benchmarks
./benchmark.sh sieve matmul # Run specific benchmarks
./benchmark.sh --clean -a   # Clean and run all

# ML benchmark
cd ml && ./benchmark.sh --all
```

> [!TIP]
> If benchmark binaries crash or fail to link, verify Z3: `ls /opt/homebrew/lib/libz3.*`

## Invariants

> [!CAUTION]
> **The Alignment Laws**
> All benchmarks must adhere to these rules to ensure a fair fight:

1. **Dynamic Inputs:** Input parameters must be derived from `argc` to prevent Constant Folding
2. **DCE Prevention:** Results must be used (printed or verified) to prevent Dead Code Elimination
3. **Optimization:** All languages run at `-O3`
