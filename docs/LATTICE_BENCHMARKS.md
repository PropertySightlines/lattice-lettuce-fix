# Lattice OS Kernel Benchmarks

Verified performance characteristics of the Lattice unikernel, measured on two platforms:

- **KVM** (hardware-virtualized): AWS z1d.metal, Intel Xeon Platinum 8151 (Skylake), QEMU 8.2.2 + KVM
- **TCG** (software-emulated): QEMU x86_64 TCG, Apple M4 host

All benchmarks run on bare metal with no OS abstraction layer — Salt compiles directly to kernel code via MLIR → LLVM IR → ELF.

## Benchmark Results (February 18, 2026)

### KVM — Hardware-Virtualized (AWS z1d.metal, Intel Xeon Platinum 8151)

> [!IMPORTANT]
> These are the **authoritative** performance numbers. KVM runs the kernel on real x86 hardware via hardware-assisted virtualization with `-cpu host`. Cycle counts reflect actual CPU pipeline behavior.

| Benchmark | Avg (cycles) | Min | Max | Samples | What It Measures |
|:----------|------------:|---------:|---------:|------:|:-----------------|
| **Ring of Fire** | **936** | — | — | 1,000 | Context switch latency (4 fibers) |
| **Ring of Fire 1K** | **354,285** | — | — | 1,000 | Scheduler scalability (1000 fibers) |
| **Syscall** | **96** | 94 | 15,030 | 10,000 | `syscall`/`sysret` null round-trip |
| **IPC Ping-Pong** | **360,594** | 352,598 | 397,148 | 10 | Fiber-to-fiber message latency (post-1K) |
| **Slab Allocator** | **52** | 50 | 94 | 1,000 | Bump-allocator slot throughput |

### TCG — Software-Emulated (QEMU, Apple M4 host)

> [!NOTE]
> TCG inflates absolute cycle counts substantially because the CPU is emulated in software. These numbers are useful for relative comparisons within the suite but should not be treated as hardware performance claims.

| Benchmark | Avg (cycles) | Min | Max | Samples | TCG/KVM Ratio |
|:----------|------------:|---------:|---------:|------:|:------|
| **Ring of Fire** | **36,480** | — | — | 1,000 | 39× |
| **Ring of Fire 1K** | **7,896,068** | — | — | 1,000 | 22× |
| **Syscall** | **375** | 358 | 11,436 | 9,999 | 3.9× |
| **IPC Ping-Pong** | **7,951,655** | 7,895,202 | 8,276,004 | 10 | 22× |
| **Slab Allocator** | **191** | 186 | 664 | 999 | 3.7× |

## Analysis

### Syscall — 96 Cycles

A 96-cycle null `syscall`/`sysret` round-trip. For context, Linux measures ~100–150 cycles for a null syscall on Skylake. Lattice achieves this with a minimal fast path: `test rax, rax; jz; sysretq` (3 instructions, zero register saves).

- MSRs configured at boot: EFER.SCE, STAR, LSTAR → `syscall_entry_fast`, FMASK = 0x200
- GDT includes Ring 3 segments + 16-byte TSS descriptor for interrupt delivery from Ring 3
- Previous `int 0x80` IDT path measured ~18,042 cycles (TCG) — **17.9× improvement** before KVM

### Ring of Fire — 936 Cycles (4 Fibers)

Sub-microsecond cooperative context switching. Spawns 4 fibers in a ring, each yielding cooperatively. Measures `rdtsc` gap across 1,000 consecutive context switches.

- 936 cycles includes: `sched_yield()` → O(1) bitmap scan (BSF/TZCNT) → `switch_stacks` (full GPR + 512-byte FXSAVE)
- Sub-microsecond at 3.4 GHz (~275 ns)

### Ring of Fire 1K — 354,285 Cycles (1000 Fibers)

1000 concurrent fibers, measuring scheduler scalability under pressure.

- Dominated by FXSAVE/FXRSTOR cache pressure at 1000 active fibers
- Each fiber's 512-byte FPU context thrashes L1/L2 cache

### IPC Ping-Pong — 360,594 Cycles

Sender writes to shared `MAILBOX` via volatile store, signals responder, yields. Responder acknowledges, yields back.

- Elevated because IPC runs after ROF1K (1000+ fibers in scheduler ring)
- In isolation (4 fibers only), IPC achieves ~2,666 cycles/round-trip (TCG estimate)
- IPC is zero-copy, zero-crossing (shared address space unikernel)

### Slab Allocator — 52 Cycles

- Single pointer increment + bounds check
- 50-cycle minimum is near the floor for bump allocation with `rdtsc` measurement overhead

## Architecture

> [!NOTE]
> The SYSCALL/SYSRET fast path required several hardware-level changes beyond MSR programming:
> - **GDT expansion**: 6 entries → 8 (Ring 3 User CS32/DS/CS64 + 16-byte TSS descriptor)
> - **TSS with RSP0**: Required for interrupt delivery when CPU is at Ring 3 (CPL=3) after SYSRET
> - **Page table User bits**: PML4/PDPT/PD entries set to 0x07/0x07/0x87 for Ring 3 memory access
> - **Ring 0 re-escalation**: Syscall 128 returns via IRETQ with kernel segments (CS=0x08, SS=0x10)

## KVM Compatibility: GDT Boot Fix

During cloud benchmarking, we discovered a latent bug that only manifests under hardware virtualization:

**Root cause**: The Kernel Data GDT descriptor at offset `0x10` had `limit=0` and `D/B=0`:

```diff
-    .quad 0x0000920000000000 # 0x10: Kernel Data (DPL 0) — limit=0, D/B=0
+    .quad 0x00CF92000000FFFF # 0x10: Kernel Data (DPL 0) — flat 4GB, G=1, D/B=1
```

After loading this into SS during the 32→64-bit transition, any stack operation (`push` for `retf`) exceeded the zero-byte segment limit, causing a triple fault. TCG's emulator doesn't enforce segment limits in this transitional state. KVM, backed by the real CPU, does.

**Diagnostic**: The boot sequence outputs characters `Y12Z789!X` at each stage. Under KVM, output stopped at `Y12Z78` — after GDT load ('8') but before the far jump ('9'). The stack push for `retf` is exactly where the zero-limit SS would fault.

## Cloud Benchmarking: Operational Chronicle

This section documents the full debugging journey for reproducibility and institutional knowledge.

### Infrastructure

| Item | Detail |
|:--|:--|
| **Instance type** | z1d.metal (48 vCPUs, Intel Xeon Platinum 8151, bare-metal) |
| **Region** | us-east-1 |
| **AMI** | Ubuntu 24.04 LTS (ami-0136735c2bb5cf5bf) |
| **Strategy** | Build kernel locally (macOS), scp kernel.elf to instance, run QEMU+KVM remotely |

### Issues Encountered and Resolved

#### 1. Stale AMI ID
The hardcoded AMI in `cloud_config.sh` was no longer valid. Fixed by looking up the current Ubuntu 24.04 canonical AMI for us-east-1.

#### 2. Spot Instance Quota
New AWS accounts have low spot instance request quotas. The first spot request failed with `MaxSpotInstanceCountExceeded`. Fixed by adding an on-demand fallback in `run_benchmarks.sh`.

#### 3. On-Demand vCPU Quota
The `c5.metal` (96 vCPUs) exceeded the default on-demand limit of 64. Switched to `z1d.metal` (48 vCPUs) which fits within the 64 vCPU default quota.

#### 4. Zombie Instances
Metal instances take 10–20 minutes to transition from `shutting-down` to `terminated`. Multiple failed runs left zombie instances consuming vCPU quota, blocking subsequent launches. Resolution: wait for instances to fully terminate, or use `aws ec2 describe-instances` to check state before launching.

#### 5. salt-opt MLIR API Mismatch
The `salt-opt` MLIR optimizer wouldn't compile on Ubuntu 24.04 — the apt-packaged LLVM/MLIR 18 has different C++ API signatures than the Homebrew version on macOS. This was a non-issue once we switched to build-local/run-remote strategy.

#### 6. Missing CMake Dev Packages
The LLVM 18 CMake config on Ubuntu expects `ZLIB::ZLIB` as a CMake imported target, which requires `zlib1g-dev`. Also missing: `libzstd-dev`, `libcurl4-openssl-dev`, `libedit-dev`. Fixed in `setup_instance.sh`.

#### 7. CMake Error Swallowed by `set -e`
Bash's `set -euo pipefail` killed the setup script before the CMake error message could be printed, making debugging impossible. Fixed by capturing the exit code with `|| CMAKE_EXIT=$?`.

#### 8. KVM Triple Fault (Root Cause)
The kernel's Kernel Data GDT descriptor had `limit=0`. Under KVM, the CPU enforces segment limits during the 32→64-bit transition, causing a triple fault on the first stack push. See [GDT Boot Fix](#kvm-compatibility-gdt-boot-fix).

### Final Working Strategy

Instead of building the entire toolchain on the remote instance, the successful approach was:

1. **Build locally** on macOS: `python3 tools/runner_qemu.py build`
2. **scp** the 41KB `kernel.elf` to the instance
3. **Run QEMU+KVM** remotely: `qemu-system-x86_64 -enable-kvm -cpu host -kernel kernel.elf ...`
4. **Terminate** the instance when done

This avoids all toolchain compatibility issues and reduces the remote setup to just QEMU.

### Cost Summary

| Run | Instance | Duration | Cost |
|:--|:--|:--|:--|
| Run 1 (c5.metal, spot, setup fail) | c5.metal | ~3 min | ~$0.20 |
| Run 2 (z1d.metal, on-demand, CMake fail) | z1d.metal | ~4 min | ~$0.30 |
| Run 3 (z1d.metal, spot, CMake fail) | z1d.metal | ~3 min | ~$0.08 |
| Run 4 (z1d.metal, spot, CMake fail) | z1d.metal | ~3 min | ~$0.08 |
| Run 5 (z1d.metal, interactive debug + bench) | z1d.metal | ~15 min | ~$0.38 |
| **Total** | | | **~$1.04** |

## Reproduce

### Local (TCG, any host)
```bash
python3 tools/runner_qemu.py build   # Compile Salt → MLIR → LLVM IR → kernel.elf
python3 tools/runner_qemu.py run     # Boot QEMU, run suite, parse results
```

### Cloud (KVM, AWS)
```bash
# Automated (launches instance, runs, terminates):
./tools/cloud/run_benchmarks.sh

# Manual (for debugging / iterating):
# 1. Launch z1d.metal instance
# 2. scp qemu_build/kernel.elf ubuntu@<IP>:/tmp/kernel.elf
# 3. ssh ubuntu@<IP> 'qemu-system-x86_64 -enable-kvm -cpu host \
#      -kernel /tmp/kernel.elf -nographic -m 128M -no-reboot -serial mon:stdio'
```

Requires: LLVM 18, QEMU x86_64, Rust toolchain. See [ARCH.md](ARCH.md) for build prerequisites. For cloud runs: AWS CLI, SSH key pair.

## Comparison: Userspace Benchmarks

For Salt vs C/Rust userspace benchmarks (22 compute benchmarks, Basalt LLM inference, TCP networking, HTTP server), see [benchmarks/BENCHMARKS.md](../benchmarks/BENCHMARKS.md).
