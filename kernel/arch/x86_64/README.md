# x86_64 (Runtime)

**The Mission:** Provide the low-level assembly primitives required for the Kernel Core to operate in 64-bit Long Mode.

## Invariants

### 1. The Stack Frame Sentinel
Salt code does not guarantee 16-byte stack alignment. `context_switch_asm.S` enforces this dynamically:
```nasm
and rsp, -16  # Force alignment
```
Failure to do this causes `#GP` faults when `fxsave` or generic SSE instructions are used.

### 2. FPU State Preservation
We save the full 512-byte FPU/SSE state using `fxsave`. This buffer must be 16-byte aligned.

## Components

| File | Role |
|------|------|
| [`context_switch_asm.S`](./context_switch_asm.S) | **The Switch.** Saves/restores GPRs, Flags, and FPU state. |
| [`fiber_loop.S`](./fiber_loop.S) | **The Synthetic Workload.** A recursive assembly loop for benchmarks that avoids Salt IR complexity. |
| [`rdtsc.S`](./rdtsc.S) | **Timekeeping.** Readings of the Time Stamp Counter. |

## Entry Points
- **Context Switch:** `switch_stacks(old_sp_ptr, new_sp)`
- **Fiber Init:** `stack_init(stack_top, entry_addr)`

## The Anatomy of a Context Switch
When `switch_stacks` is called:
1. **Push Callee-Saved Regs:** RBP, RBX, R12-R15.
2. **Save RFLAGS:** `pushfq`.
3. **Disable Interrupts:** `cli` (Critical Section).
4. **Align & Save FPU:** `fxsave` to aligned stack slot.
5. **Swap RSP:** `mov [rdi], rsp` / `mov rsp, rsi`.
6. **Restore Reverse Order:** `fxrstor` -> `popfq` -> GPRs.
7. **Return:** To the new fiber's instruction pointer.
