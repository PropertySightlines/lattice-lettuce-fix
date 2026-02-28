# 🧠 Basalt — Llama 2 Inference in Salt

**A ~600-line LLM inference engine** that compiles to native code through Salt's MLIR pipeline — and to **WASM for browser-side inference**. Runs [Karpathy's TinyLlama](https://github.com/karpathy/llama2.c) models with BPE tokenization, zero-copy weight loading, and Z3-verified compute kernels.

**C-parity performance** on `stories15M.bin` (~870 tok/s, matching `clang -O3 -ffast-math -march=native` on Apple M4).

Basalt exists to prove one claim: **Salt can replace C in performance-critical ML workloads while providing compile-time safety guarantees that C cannot.**

---

## Quick Start

### Prerequisites

| Requirement | Purpose |
|:------------|:--------|
| Salt compiler built | `./scripts/build.sh` from monorepo root |
| LLVM 18 on PATH | `brew install llvm@18` — provides `mlir-opt`, `mlir-translate`, `clang` |
| Python 3 | Only for generating dummy test models |

### Build & Run (Mock Mode)

```bash
# Build everything — compiler + Basalt binary
bash scripts/build_basalt.sh
```

This will compile Basalt and run it in **mock mode** (no model file). Expected output:

```
Basalt v0.4.1 (Llama 2 Inference)
Running in MOCK mode (no model file provided).
Sampled token: 0
```

> [!TIP]
> Mock mode allocates a zeroed weight buffer and runs a single forward pass. Use it to verify the build pipeline works before downloading real models.

### Build & Run (With Model)

```bash
# Generate a small test model + tokenizer
python3 scripts/gen_dummy_model.py
mv dummy.bin tokenizer.bin /tmp/salt_build/

# Run inference with tokenizer
/tmp/salt_build/basalt /tmp/salt_build/dummy.bin /tmp/salt_build/tokenizer.bin
```

Expected output:

```
Basalt v0.4.1 (Llama 2 Inference)
Loading model...
Config: dim=64, layers=2, heads=4, vocab=256
Tokenizer loaded (256 entries).
Generating 32 tokens...
<c4>(<c4>(<c4>(...
```

> [!IMPORTANT]
> The dummy model has random weights, so the output is nonsensical — this is expected. To get real text output, use Karpathy's `stories15M.bin` and `tokenizer.bin` from the [llama2.c repo](https://github.com/karpathy/llama2.c).

### Run with Real Weights

```bash
# Download TinyLlama (60MB)
mkdir -p basalt/models
cd basalt/models
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin
wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin
cd ../..

# Build and run
bash scripts/build_basalt.sh
/tmp/salt_build/basalt basalt/models/stories15M.bin basalt/models/tokenizer.bin
```

### CLI

```
basalt                                    # Mock mode (no args)
basalt <model.bin>                        # Inference, numeric token IDs
basalt <model.bin> <tokenizer.bin>        # Inference, decoded text output
```

---

## Architecture

```mermaid
graph LR
    A["main.salt<br/><i>CLI · mmap · gen loop</i>"] --> B["transformer.salt<br/><i>Config · Weights · forward()</i>"]
    B --> C["kernels.salt<br/><i>rmsnorm · softmax · mat_mul</i>"]
    A --> D["sampler.salt<br/><i>argmax · top-p</i>"]
    A --> E["tokenizer.salt<br/><i>BPE encode/decode</i>"]
    A --> F["model_loader.salt<br/><i>mmap · config parse</i>"]
    A --> G["basalt_wasm.c<br/><i>WASM exports · shims</i>"]
```

### Module Reference

| Module | Lines | Responsibility | Key Functions |
|:-------|------:|:---------------|:--------------|
| [`main.salt`](src/main.salt) | ~370 | Entry point: CLI arg parsing, RoPE precomputation, generation loop, **WASM step functions** | `main`, `run_inference`, `basalt_engine_init/reset/prefill/generate_step/free` |
| [`transformer.salt`](src/transformer.salt) | 262 | Llama 2 architecture: struct definitions, multi-head attention, FFN, forward pass | `forward`, `Config`, `TransformerWeights`, `RunState` |
| [`kernels.salt`](src/kernels.salt) | ~230 | Z3-verified compute: RMS norm, softmax, **SIMD-vectorized** tiled matrix multiply | `rmsnorm`, `softmax`, `mat_mul`, `mat_mul_vec` (v128 SIMD) |
| [`sampler.salt`](src/sampler.salt) | ~80 | Token selection from logits | `sample_argmax`, `sample_token` |
| [`tokenizer.salt`](src/tokenizer.salt) | 179 | BPE tokenizer: load, encode, decode (llama2.c format) | `load_tokenizer`, `bpe_encode`, `decode_token` |
| [`model_loader.salt`](src/model_loader.salt) | ~100 | Binary weight parsing from `mmap`'d file | `load_config`, `get_weights` |
| [`basalt_wasm.c`](wasm/basalt_wasm.c) | ~280 | C bridge runtime: 7 WASM exports, I/O shims | `basalt_init`, `basalt_ingest_prompt`, `basalt_generate_next`, `basalt_reset`, `basalt_free` |
| [`engine-worker.js`](wasm/engine-worker.js) | ~160 | JS Web Worker: tokenizer, WASM bridge, streaming | `BPETokenizer`, `initEngine`, `generate` |

### Data Flow

```mermaid
sequenceDiagram
    participant main as main.salt
    participant loader as model_loader
    participant tok as tokenizer
    participant xfr as transformer
    participant kern as kernels
    participant samp as sampler

    main->>loader: mmap(model.bin) → Config, Weights
    main->>tok: mmap(tokenizer.bin) → Tokenizer
    main->>main: build_freq_cis(Config) → RoPE tables

    loop for each position
        main->>xfr: forward(cfg, weights, state, token, pos)
        xfr->>kern: rmsnorm(out, x, weight, dim)
        xfr->>kern: mat_mul(xq, x, wq, dim, dim, 1)
        xfr->>kern: softmax(att, seq_len)
        xfr->>kern: mat_mul(xb, att, v_cache, ...)
        xfr-->>main: state.logits populated
        main->>samp: sample_argmax(logits, vocab_size)
        samp-->>main: next token ID
        main->>tok: decode_token(tok, token_id) → text
        main->>main: write(stdout, text)
    end
```

---

## Why It's Fast

Salt's `for i in 0..N` loops compile through MLIR's `scf.for` dialect, then `clang -O3` auto-vectorizes the tight inner loops. Basalt exploits this with three tiers of optimization:

| Technique | Where | Why |
|:----------|:------|:----|
| **WASM SIMD v128 `mat_mul_vec`** | `kernels.salt` | The 95% hotpath uses explicit `v_load` → `v_fma` → `v_hsum` intrinsics. Salt emits MLIR `vector<4xf32>` ops; `-msimd128` lowers them to native WASM `v128.load` / `f32x4.mul` / `f32x4.add` (4 floats per cycle) |
| **4×4 tiled `mat_mul`** | `kernels.salt` | General matrix multiply with 16 scalar accumulators in registers, reducing memory traffic by 4× |
| **Zero-copy `mmap`** | `main.salt` | Model weights are memory-mapped directly from disk — no allocation, no deserialization boot cost |

### Compilation Pipeline

```mermaid
graph LR
    S["Salt modules"] -->|build_basalt.sh| C[Concatenated .salt]
    C -->|salt-front| M[MLIR .mlir]
    M -->|mlir-opt| O[Optimized .mlir]
    O -->|mlir-translate| L[LLVM IR .ll]
    L -->|clang -O3| B[Native binary]
```

> [!NOTE]
> The build script concatenates all modules into a single compilation unit so that `salt-front` sees every function definition — enabling cross-module inlining. Individual module packages (`basalt.kernels`, etc.) are stripped during concatenation and replaced with a single `package main`.

## Why It's Safe

Every kernel function carries `requires` contracts verified by Z3 at compile time:

```salt
fn rmsnorm(out: Ptr<f32>, x: Ptr<f32>, weight: Ptr<f32>, size: i64)
    requires(size > 0)
{
    // Z3 proves: loop bounds [0..size) are non-negative
    // Z3 proves: division by sqrt(ss/size + 1e-5) is non-zero
    // No runtime bounds-check overhead
}
```

| Guarantee | Mechanism |
|:----------|:----------|
| No out-of-bounds access | `requires(size > 0)` — Z3 proves all loop indices are in-range |
| No division by zero | RMSnorm denominator is `sqrt(mean + ε)` — always positive |
| No integer overflow | Matrix dimensions are `i64` — 2⁶³ element ceiling |

---

## Benchmarking: Basalt vs llama2.c

### Latest Results (Apple M4, macOS 15.6)

| Engine | Flags | tok/s |
|:-------|:------|------:|
| **Basalt** (Salt, MLIR pipeline) | `mlir-opt` → `clang -O3` | **~870** |
| llama2.c (C) | `clang -O3 -ffast-math -march=native` | **~877** |
| llama2.c (C) | `clang -O3` only | 185 |

> **Basalt matches C at full optimization.** Both produce identical, coherent output. The `mat_mul_vec` kernel uses 4-wide unrolled accumulation that LLVM auto-vectorizes to NEON instructions. When llama2.c is compiled without `-ffast-math -march=native`, its inner loop misses NEON vectorization and runs 5× slower — but that's an unfair comparison.
>
> With fair flags, Basalt achieves **99% of C speed** with Z3-verified kernels that prove all matrix dimensions are in-bounds at compile time.

### Run It Yourself

```bash
bash scripts/bench_basalt.sh
```

The script is fully **idempotent** — downloads models and builds both engines only if missing. Re-run safely at any time.

| Flag | Effect |
|:-----|:-------|
| *(no flags)* | Full benchmark: download, build, run, compare |
| `--rebuild` | Force rebuild of both engines |
| `--clean` | Remove all cached artifacts |

Results are saved to `.bench_basalt/results.txt` with hardware info for reproducibility.

---

## Testing

All tests follow strict **Test-Driven Development** — tests were written and passing before implementation was extracted into modules.

```bash
# Run kernel tests (rmsnorm, softmax, mat_mul)
zsh scripts/run_test.sh basalt/tests/test_kernels.salt

# Run sampler tests
zsh scripts/run_test.sh basalt/tests/test_sampler.salt

# Run tokenizer tests (BPE encode/decode)
zsh scripts/run_test.sh basalt/tests/test_tokenizer.salt

# Run transformer tests (forward pass)
zsh scripts/run_test.sh basalt/tests/test_transformer.salt
```

> [!WARNING]
> The test runner script (`run_test.sh`) uses zsh-specific syntax (`${0:A:h}`). Run with `zsh`, not `bash`. If you see `A: unbound variable`, you're using the wrong shell.

| Test File | What It Validates |
|:----------|:------------------|
| [`test_kernels.salt`](tests/test_kernels.salt) | Golden-value tests for `rmsnorm`, `softmax`, `mat_mul` against hand-computed results |
| [`test_sampler.salt`](tests/test_sampler.salt) | Argmax selection from known probability distributions |
| [`test_tokenizer.salt`](tests/test_tokenizer.salt) | BPE encode/decode with a 7-token hand-built vocabulary; covers merges, single-byte fallback, round-trip |
| [`test_transformer.salt`](tests/test_transformer.salt) | Forward pass with controlled weights; verifies attention + FFN + residual connections |

## WASM — Browser-Side Inference

### Quickstart (Pre-built Binary)

No toolchain required — grab the pre-built binary:

```bash
basalt/wasm/dist/basalt.wasm    # 22KB inference engine
basalt/wasm/engine-worker.js    # JS Web Worker
```

```javascript
const worker = new Worker('/engine-worker.js');
worker.postMessage({ type: 'LOAD_MODEL', modelUrl: '/model.bin', tokenizerUrl: '/tokenizer.bin' });
worker.postMessage({ type: 'RUN_PROMPT', prompt: 'Once upon a time', maxNewTokens: 256 });
worker.onmessage = ({ data }) => {
    if (data.type === 'TOKEN') process.stdout.write(data.text);
    if (data.type === 'DONE')  console.log(`${data.totalTokens} tokens in ${data.elapsedMs}ms`);
};
```

### Build WASM from Source

```bash
cargo build --release --manifest-path salt-front/Cargo.toml
bash scripts/build_basalt_wasm.sh
# Output: basalt/wasm/dist/basalt.wasm (22KB)
```

### 7-Export API

| Export | Signature | Purpose |
|--------|-----------|--------|
| `basalt_alloc` | `(bytes: i64) → ptr` | Allocate WASM linear memory for model |
| `basalt_init` | `(ptr, size: i64) → i32` | Parse config, alloc state, build RoPE (0=ok, -1=fail) |
| `basalt_ingest_prompt` | `(tokens_ptr, count: i64)` | Bulk prefill (1 boundary crossing for entire prompt) |
| `basalt_generate_next` | `() → i64` | One forward + sample → token ID (-1 = EOS/done) |
| `basalt_get_config` | `(param_id: i64) → i64` | Unified config getter (-1 = invalid ID) |
| `basalt_free` | `()` | Burn the context down |
| `basalt_reset` | `()` | Zero KV cache + reset position (multi-turn chat, keeps loaded weights) |

### Conversation Context

The KV cache supports **reset without re-init** — enabling multi-turn chat without re-parsing model weights.

| Scenario | How |
|----------|-----|
| Multi-turn chat | `basalt_reset()` → `basalt_ingest_prompt(new_history)` — clears KV cache, keeps weights |
| Switch models | `worker.terminate()` → new Worker (only way to reclaim WASM memory) |

### Config Param IDs

| ID | Field | ID | Field |
|----|-------|----|-------|
| 0 | dim | 4 | n_kv_heads |
| 1 | hidden_dim | 5 | vocab_size |
| 2 | n_layers | 6 | seq_len |
| 3 | n_heads | | |

### Lifecycle

```mermaid
sequenceDiagram
    participant JS as engine-worker.js
    participant W as WASM (basalt.wasm)

    JS->>JS: BPE tokenize (O(1) hashmap)
    JS->>W: basalt_alloc(size)
    JS->>W: basalt_init(model_ptr, size)
    JS->>W: basalt_ingest_prompt(tokens_ptr, count)
    Note over W: Full prefill loop runs inside WASM

    loop Generate (until EOS or max)
        JS->>W: basalt_generate_next()
        W-->>JS: token ID (or -1)
        JS->>JS: decode + render
    end

    Note over JS: User starts new conversation
    JS->>W: basalt_reset()
    Note over W: KV cache zeroed, pos=0 (weights preserved)
    JS->>W: basalt_ingest_prompt(new_tokens, count)

    JS->>W: basalt_free()
```

### Key Design Decisions

- **JS owns BPE.** WASM emits integers, JS decodes via vocab hashmap. No string allocation in Salt/C.
- **Bulk prefill.** `basalt_ingest_prompt` runs the entire prefill loop inside WASM (1 boundary crossing instead of N).
- **JS owns the loop.** `generate_next()` per token, yielding to event loop between calls for UI responsiveness.

### The Road to 1B Parameters (The "Boss Fight")

Supporting a modern 1B parameter model (like Llama 3.2 1B or TinyLlama 1.1B) introduces fundamental architectural constraints that require ascending the optimization tiers:

1. **The WASM 4GB Memory Wall**: WebAssembly32 has a hard 4GB memory limit. A 1B model with raw `f32` weights requires ~4GB, meaning it will instantly OOM the browser tab upon loading.
2. **Weight Quantization (Mandatory)**: To fit a 1B model into WASM, Basalt must implement **int8 or q4_0 quantization** (e.g., GGUF formats). This shrinks weights to ~1GB and avoids the memory wall, requiring Salt kernels to dequantize weights on the fly during matrix multiplication.
3. **WebGPU (Tier 3)**: Even with WASM SIMD (Tier 2), pushing 1B parameters through a single-threaded CPU will yield unusable token generation rates (10-20 seconds per token). Sustained 1B inference demands Tier 3 WebGPU to keep weights in VRAM and execute massive parallel kernels.

### Performance & Capability Roadmap

| Tier | Technique | Capability Unlocked | Status |
|------|-----------|---------------------|--------|
| **1** | Cache-blocking / Loop unrolling | 1.5–2× native speedup | ✅ Done |
| **2** | WASM SIMD v128 (`f32x4`) | 2–3× WASM speedup | ✅ Done — `v_load`/`v_fma`/`v_hsum` intrinsics + `-msimd128` |
| **2.5** | **Weight Quantization (`q8_0`)** | **Bypass 4GB WASM limit** | ⬜ Dequantization kernels in Salt |
| **3** | **WebGPU Orchestration** | **Real-time 1B Inference** | ⬜ Compiler: opaque GPU buffer FFI, WGSL shaders |
| **4** | SharedArrayBuffer threading | Multi-core CPU fallback | ⬜ Compiler: atomics + Z3 concurrency tracking |

---

## Status

- [x] `kernels.salt` — rmsnorm, softmax, tiled mat_mul, **SIMD `mat_mul_vec`** (Z3-verified)
- [x] `sampler.salt` — argmax, temperature sampling
- [x] `transformer.salt` — Config, TransformerWeights, RunState, full forward pass
- [x] `model_loader.salt` — binary config/weight parsing from mmap
- [x] `tokenizer.salt` — BPE load, encode, decode (llama2.c format)
- [x] `main.salt` — CLI, mmap, RoPE, generation loop, decoded output
- [x] Build pipeline (`build_basalt.sh`, `build_basalt_wasm.sh`)
- [x] Test suite (4 test files, TDD)
- [x] WASM API: C bridge + Salt engine + JS worker + pre-built binary
- [x] Multi-turn chat (`basalt_reset` — KV cache clear without re-init)
- [x] WASM SIMD v128 kernel optimization (Tier 2) — `v_load`/`v_fma`/`v_hsum` intrinsics
- [ ] Top-p / temperature sampling in generation loop
- [ ] Weight quantization (q8_0) for 1B models (Tier 2.5)

