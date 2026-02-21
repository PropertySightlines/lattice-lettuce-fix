---
description: Test-driven development workflow for Salt compiler and kernel changes
---

# Test-Driven Development Workflow

// turbo-all

## Core Principle

**Always write failing tests FIRST, then fix the code to make them pass.** Never guess at the implementation — let tests guide every layer.

## Steps

1. **Write the failing end-to-end test** (Salt `.salt` file) that specifies the ideal behavior
2. **Run it** and capture the exact error message
3. **At each layer the error touches**, write a unit test:
   - **Grammar/Parser level**: `syn::parse_str::<SynType>(...)` or `syn::parse_str::<SaltFile>(...)`
   - **Preprocessor level**: `convert_xxx("input")` → assert output
   - **Codegen level**: `compile_to_mlir(source)` → assert MLIR contents
   - **Linker level**: end-to-end binary execution
4. **Run all unit tests** — confirm they fail with the expected error
5. **Fix the code** at the lowest failing layer first
6. **Re-run tests** — confirm the fixed layer passes, then move up to next layer
7. **Repeat** until all layers pass
8. **Run full test suite** (`./scripts/build.sh --test`) to verify no regressions

## Test File Conventions

| Layer | Location | Pattern |
|-------|----------|---------|
| Rust unit tests | `salt-front/src/codegen/tests_*.rs` | `#[test] fn test_...()` |
| Preprocessor tests | `salt-front/src/lib.rs` (bottom) | `#[test] fn test_preprocess_...()` |
| Salt end-to-end | `tests/lib/test_*.salt` | `fn main() -> i32 { ... }` |
| Kernel regression | QEMU via `/tmp/build_and_test.sh` | Interleave output check |

## Running Tests

```bash
# Single Rust test module
cd salt-front && Z3_SYS_Z3_HEADER=/opt/homebrew/include/z3.h CPATH=/opt/homebrew/include LIBRARY_PATH=/opt/homebrew/lib DYLD_LIBRARY_PATH=/opt/homebrew/lib cargo test --lib tests_MODULE_NAME -- --nocapture

# Single Salt test  
./scripts/run_test.sh tests/test_foo.salt

# Full suite
./scripts/build.sh --test
```
