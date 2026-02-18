# Scheduler Policies

**The Mission:** Decide *what* runs next to maximize hardware efficiency.

## Invariants

### 1. The L1-First Affinity Strategy
We map software IDs to hardware cores to exploit the L1/L2 caches.
- **Rule:** Fibers with adjacent IDs ($N$ and $N+1$) are pinned to the same core.
- **Mechanism:** `get_affinity_core(id) -> id / 1250`
- **Why?** Stacks for $N$ and $N+1$ are adjacent in physical memory (due to `slab.salt`). Accessing Stack $N$ triggers the **Hardware Prefetcher** to pull Stack $N+1$ into the L2 Cache *before* the context switch occurs.

## Components

| File | Role |
|------|------|
| [`affinity.salt`](./affinity.salt) | **Topology Logic.** Maps Fiber IDs to Physical Cores. |

## Visualizing the Optimization
```mermaid
graph LR
    CPU[CPU Core 0] --> L1[L1 Cache]
    L1 --> L2[L2 Cache]
    L2 --> RAM[Main Memory]
    
    subgraph RAM_Layout
    S1[Stack N]
    S2[Stack N+1]
    end
    
    CPU -- Accesses --> S1
    S1 -. Prefetch .-> S2
    S2 -- Already in --> L2
```
