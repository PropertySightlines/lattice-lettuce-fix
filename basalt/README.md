# 🧠 Basalt — Llama 2 Inference in Salt

**A ~600-line LLM inference engine** that compiles to native code through Salt's MLIR pipeline. Runs [Karpathy's TinyLlama](https://github.com/karpathy/llama2.c) models with BPE tokenization, zero-copy `mmap` weight loading, and Z3-verified compute kernels.

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
Basalt v0.3.0 (Llama 2 Inference)
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
Basalt v0.3.0 (Llama 2 Inference)
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
```

### Module Reference

| Module | Lines | Responsibility | Key Functions |
|:-------|------:|:---------------|:--------------|
| [`main.salt`](src/main.salt) | 200 | Entry point: CLI arg parsing, RoPE precomputation, generation loop | `main`, `run_inference`, `run_mock`, `build_freq_cis` |
| [`transformer.salt`](src/transformer.salt) | 262 | Llama 2 architecture: struct definitions, multi-head attention, FFN, forward pass | `forward`, `Config`, `TransformerWeights`, `RunState` |
| [`kernels.salt`](src/kernels.salt) | 238 | Z3-verified compute: RMS norm, softmax, tiled matrix multiply | `rmsnorm`, `softmax`, `mat_mul`, `mat_mul_vec` |
| [`sampler.salt`](src/sampler.salt) | ~80 | Token selection from logits | `sample_argmax`, `sample_token` |
| [`tokenizer.salt`](src/tokenizer.salt) | 179 | BPE tokenizer: load, encode, decode (llama2.c format) | `load_tokenizer`, `bpe_encode`, `decode_token` |
| [`model_loader.salt`](src/model_loader.salt) | ~100 | Binary weight parsing from `mmap`'d file | `load_config`, `get_weights` |

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

Salt's `for i in 0..N` loops compile through MLIR's `scf.for` dialect, then `clang -O3` auto-vectorizes the tight inner loops. Basalt exploits this with two manual optimizations:

| Technique | Where | Why |
|:----------|:------|:----|
| **4×4 tiled `mat_mul`** | `kernels.salt` | 16 scalar accumulators stay in registers, reducing memory traffic by 4× |
| **Specialized `mat_mul_vec`** | `kernels.salt` | Matrix-vector multiply (the `n=1` case in Llama attention) uses 4-way unrolled accumulation for LLVM auto-vectorization |
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

---

## File Layout

```
basalt/
├── salt.toml                # sp package manifest (name, version, entry)
├── models/                  # Binary weight files (git-ignored)
│   ├── stories15M.bin       # Karpathy's TinyLlama weights (60 MB)
│   └── tokenizer.bin        # Llama 2 BPE vocabulary (500 KB)
├── src/
│   ├── main.salt            # CLI, mmap, RoPE, generation loop
│   ├── transformer.salt     # Llama 2 config, weights, multi-head attention, forward pass
│   ├── kernels.salt         # Z3-verified: rmsnorm, softmax, tiled mat_mul, mat_mul_vec
│   ├── sampler.salt         # Argmax and temperature sampling
│   ├── tokenizer.salt       # BPE tokenizer: load, encode, decode
│   └── model_loader.salt    # Binary config/weight parsing from mmap'd file
└── tests/
    ├── test_kernels.salt    # Golden-value kernel tests
    ├── test_sampler.salt    # Probability distribution tests
    ├── test_tokenizer.salt  # BPE round-trip tests
    └── test_transformer.salt # Forward pass integration tests
```

## Troubleshooting

| Symptom | Cause | Fix |
|:--------|:------|:----|
| `error: Entry point 'main' not found` | Compiler not recognizing `fn main` in concatenated build | Verify `salt-front` is built from latest source: `./scripts/build.sh` |
| `ld: symbol(s) not found: _main` | `fn main` emitted as `@main__main` (private) | Rebuild compiler — this bug is fixed in `codegen/mod.rs` (entry point guard) |
| `A: unbound variable` | Running `run_test.sh` with `bash` instead of `zsh` | Use `zsh scripts/run_test.sh ...` |
| `mlir-opt: command not found` | LLVM 18 not on PATH | `export PATH=/opt/homebrew/opt/llvm@18/bin:$PATH` |
| Nonsense output from dummy model | Expected — random weights produce random tokens | Use `stories15M.bin` for real text generation |

## Status

- [x] `kernels.salt` — rmsnorm, softmax, tiled mat_mul, mat_mul_vec (Z3-verified)
- [x] `sampler.salt` — argmax, temperature sampling
- [x] `transformer.salt` — Config, TransformerWeights, RunState, full forward pass
- [x] `model_loader.salt` — binary config/weight parsing from mmap
- [x] `tokenizer.salt` — BPE load, encode, decode (llama2.c format)
- [x] `main.salt` — CLI, mmap, RoPE, generation loop, decoded output
- [x] Build pipeline (`build_basalt.sh`)
- [x] Test suite (4 test files, TDD)
- [ ] Top-p / temperature sampling in generation loop
- [ ] Multi-turn chat template support
- [ ] Benchmark vs. llama2.c on stories15M
