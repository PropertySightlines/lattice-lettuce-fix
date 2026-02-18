# Memory Subsystem

**The Mission:** Provide clear, deterministic, and constant-time memory access for the kernel's critical paths.

## Invariants

> [!TIP]
> **The O(1) Slab Law**
> Allocation time must be constant, regardless of heap fragmentation or total uptime.
> We achieve this via lock-free atomic bump allocation on a pre-allocated slab.

### 1. Pre-Allocation Strategy
We do not ask the OS for memory at runtime. We take it all at boot.
- **Base:** `0xFFFFFFFF90000000` (Higher Half)
- **Size:** 160MB
- **Capacity:** 10,240 Fibers (16KB per stack)

### 2. The Deterministic Panic
If the slab is exhausted, the kernel panics immediately (`SLAB EXHAUSTED`).
**Why?** In a predictable embedded system, running out of pre-calculated resources is a fatal design flaw, not a recoverable runtime error.

## Components

| File | Role | Mechanism |
|------|------|-----------|
| [`slab.salt`](./slab.salt) | **Slab Allocator.** O(1) Stack allocation. | `pop_stack()`: `fetch_add` bump pointer. |

## Performance Impact
This module eliminates `malloc/free` overhead from the fiber spawn path, contributing heavily to the system's ability to spawn 10,000 threads in sub-millisecond time.
