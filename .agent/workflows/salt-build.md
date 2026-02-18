---
description: Build and test the Salt compiler with Z3 dependencies
---

# Salt Build Workflow

## Build the compiler

// turbo
1. Build the compiler (debug):
```bash
./scripts/build.sh
```

// turbo
2. Build the compiler (release):
```bash
./scripts/build.sh --release
```

// turbo
3. Run cargo tests (unit + integration):
```bash
./scripts/build.sh --test
```

## Run a single Salt test through the full MLIR pipeline

// turbo
4. Run a specific test:
```bash
./scripts/run_test.sh tests/test_thread.salt
```

5. Compile without executing:
```bash
./scripts/run_test.sh examples/http_server.salt --compile-only
```

6. With extra C bridge:
```bash
./scripts/run_test.sh tests/test_http_client.salt --bridge std/net/http_bridge.c
```

## Run all Salt tests

// turbo
7. Run the full test suite:
```bash
./scripts/run_all_tests.sh
```

8. Filter to specific tests:
```bash
./scripts/run_all_tests.sh --filter thread
```

## Notes
- The scripts auto-detect which C bridges to link based on imports
- LLVM 18 must be installed at `/opt/homebrew/opt/llvm@18/`
- Z3 must be installed at `/opt/homebrew/`
- Build artifacts go to `/tmp/salt_build/`
