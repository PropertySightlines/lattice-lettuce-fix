# Lattice Kernel Userspace Capabilities Investigation

**Date:** March 1, 2026  
**Purpose:** Investigate whether Lattice kernel can run agent workloads (Basalt + Lettuce) as Ring 3 services

---

## Executive Summary

**Finding:** Lattice kernel has a **functional Ring 3 userspace** with ELF loading, 4-core SMP, memory isolation (KPTI), and a complete syscall interface. However, **neither Basalt nor Lettuce can run today** due to missing OS facilities:

| Component | Can Run Today? | Blocker |
|-----------|----------------|---------|
| **Basalt** (LLM inference) | ❌ No | No file system / file-backed mmap |
| **Lettuce** (Redis server) | ❌ No | No TCP sockets / event polling |

**Path forward:** Both could run with targeted kernel development (see Section 7).

---

## 1. Syscall Interface

### 1.1 Syscall Entry Mechanism

Lattice uses **SYSCALL/SYSRET** (not INT 0x80) for fast syscall entry:

**Entry Point:** `/home/property.sightlines/lattice/kernel/arch/x86_64/syscall_entry_fast.S`

**8-Phase Entry Sequence:**
1. `swapgs` — User GS → Kernel GS (PerCpuData)
2. Save user RSP to `gs:[104]`
3. KPTI stack pivot to scratch stack
4. CR3 swap to kernel PML4 (with PCID NOFLUSH)
5. Switch to real kernel stack
6. Save user context (RSP, RCX, R11)
7. Sentinel dispatch (0xBEEF, 0xDEAD, 60, 200, 201)
8. Slow path: save regs, call `handle_syscall()`

**MSR Configuration:** (`/home/property.sightlines/lattice/kernel/arch/x86_64/syscall_msr_init.S`)
- `LSTAR` → `syscall_entry_fast`
- `STAR` → Segment selectors (0x18/0x08)
- `FMASK` → Clear IF, DF, TF flags
- `EFER.SCE` → Enable SYSCALL instruction

### 1.2 Syscall ABI (Calling Convention)

| Register | Input | Output |
|----------|-------|--------|
| **RAX** | Syscall number | Return value |
| **RDI** | Arg 0 | — |
| **RSI** | Arg 1 | — |
| **RDX** | Arg 2 | — |
| **R10** | Arg 3 | — |
| **R8** | Arg 4 (slow path) | — |
| **R9** | Arg 5 (slow path) | — |

**Hardware clobbers:** RCX (holds user RIP), R11 (holds user RFLAGS)

### 1.3 Syscall Table

| Number | Name | Handler | Status |
|--------|------|---------|--------|
| 0 | `noop` | Fast path | ✅ Implemented |
| 1 | `sys_write` | `handle_syscall()` | ✅ Implemented (stdout only) |
| 9 | `sys_mmap` | `handle_syscall()` | ✅ Anonymous mappings only |
| 12 | `sys_brk` | `handle_syscall()` | ✅ Implemented |
| 60 | `sys_exit` | Assembly fast path | ✅ Implemented |
| 119 | `sched_yield` | `handle_syscall()` | ✅ Implemented |
| 200 | `sys_ipc_send` | Assembly fast path | ✅ Implemented |
| 201 | `sys_ipc_recv` | Assembly fast path | ✅ Implemented |
| 202 | `sys_shm_grant` | `handle_syscall()` | ✅ Implemented |

**Missing for agents:**
- No `sys_open`, `sys_read`, `sys_close` (file I/O)
- No `sys_socket_*` (TCP/UDP sockets)
- No `sys_poll` / `sys_epoll` (event polling)
- No file-backed `mmap`

### 1.4 Userspace Wrappers

**Location:** `/home/property.sightlines/lattice/user/lib/syscall.salt`

```salt
pub fn sys_write(fd: u64, buf: u64, len: u64)
pub fn sys_exit(code: u64)
pub fn sys_brk(new_brk: u64) -> u64
pub fn sys_mmap(length: u64, prot: u64) -> u64
pub fn sys_ipc_send(target_pid: u64, msg0: u64, msg1: u64, msg2: u64) -> u64
pub fn sys_ipc_recv() -> u64
pub fn sys_shm_grant(target_pid: u64, src_vaddr: u64, dst_vaddr: u64, flags: u64) -> u64
```

---

## 2. Ring 3 Userspace Examples

### 2.1 ELF Loading Process

**Location:** `/home/property.sightlines/lattice/kernel/core/exec_user.salt`

**`spawn_process(elf_base, kernel_pml4)` flow:**

1. **ELF Validation** — Check magic bytes (0x7F, 'E', 'L', 'F')
2. **Parse ELF Header** — Extract entry point, program headers
3. **Allocate Process Slot** — From global `PROC_TABLE` (16 slots max)
4. **Allocate Kernel Stack** — 4KB via PMM for syscall handling
5. **Create User PML4** — Isolated address space via `create_user_pml4()`
6. **Map PT_LOAD Segments** — For each loadable segment:
   - Allocate physical pages via PMM
   - Zero page, copy ELF data
   - Map with `USER_PAGE_FLAGS` (PRESENT | WRITE | USER)
7. **Map User Stack** — 16KB at `USER_STACK_TOP` (0x7FFFFFFFE000)
8. **Setup Kernel Stack IRETQ Frame** — SS, RSP, RFLAGS, CS, RIP
9. **Register in Process Table** — PCB with PML4, stacks, entry, PCID
10. **Setup Heap Boundaries** — `brk_base`, `brk_current` after last ELF segment

### 2.2 Embedded User Programs

**Location:** `/home/property.sightlines/lattice/kernel/arch/x86/embedded_user.S`

```assembly
user_elf_a_start: .incbin "/tmp/test_memory"   // Memory test
user_elf_b_start: .incbin "/tmp/ring3_test_b"  // Ring 3 verification
user_elf_c_start: .incbin "/tmp/hello"         // Lifecycle test
```

**Source Files:** (`/home/property.sightlines/lattice/user/`)
- `test_memory.salt` — Tests `sys_brk()` and `sys_mmap()` with magic values (0xCAFEBABE)
- `hello.salt` — Heap expansion, sentinel writes, preemption test
- `ring3_test.salt` — Basic "Hello from Ring 3!" with sys_write + sys_exit

### 2.3 Ring 3 Transition

**Location:** `/home/property.sightlines/lattice/kernel/arch/x86/ring3_entry.S`

```assembly
jump_to_ring3:
    cli                          // Disable interrupts
    mov ax, 0x23                 // User Data Selector (RPL 3)
    mov ds, ax; mov es, ax; mov fs, ax
    
    // Build IRETQ frame
    push 0x23                    // SS
    push rsi                     // RSP (user_stack_top)
    push 0x202                   // RFLAGS (IF=1)
    push 0x2B                    // CS (User Code 64, RPL 3)
    push rdi                     // RIP (entry_point)
    
    iretq                        // Hardware privilege drop to Ring 3
```

### 2.4 Process Control Block

**Location:** `/home/property.sightlines/lattice/kernel/core/process.salt`

```salt
pub struct Process {
    pub pid: u64,
    pub state: u64,  // FREE/READY/RUNNING/ZOMBIE/IPC_BLOCKED
    pub user_pml4: u64,  // Physical address of user page tables
    pub kernel_rsp: u64,  // Saved kernel stack pointer
    pub kernel_stack_base: u64,
    pub kernel_stack_top: u64,
    pub user_entry: u64,  // ELF entry point
    pub user_stack_top: u64,  // 0x7FFFFFFFE000
    pub brk_base: u64,
    pub brk_current: u64,
    pub mmap_base: u64,
    pub pcid: u16,  // 1..4095 (0 reserved for kernel)
    pub ipc_sender: u64,
    pub ipc_msg0: u64,
    pub ipc_msg1: u64,
    pub ipc_msg2: u64,
}
```

### 2.5 Boot Test Results (Verified)

From QEMU boot test:
```
RING3 IRETQ FRAME TEST SUITE — ALL_PASS (6/6)
RING3 KPTI TEST SUITE — ALL_PASS (3/3)
RING3 E2E TEST — ALL_PASS (2/2)
RING3 SWAPGS NMI TEST — PASS
PCID ALLOCATION TEST — ALL_PASS (3/3)
PCID CR3 NOFLUSH TEST — ALL_PASS (3/3)
KPTI USER PML4 ISOLATION TEST — ALL_PASS (2/2)
```

**Key boot messages:**
```
[Lattice] GDT/TSS Ring 3 ready (IST1=NMI, IST2=DF)
[Lattice] IST gates wired: NMI=0x02/IST1, DF=0x08/IST2
[Lattice] PCID enabled (CR4.PCIDE=1)
[Lattice] SYSCALL MSRs configured
[Lattice] Task 0 (dispatcher) spawned
[Lattice] SPAWNING PROCESSES
[Lattice] Process A spawned
[Lattice] Process B spawned
[Lattice] Process C spawned
```

---

## 3. IPC Mechanisms

### 3.1 SPSC Ring Buffer (Shared Memory)

**Location:** `/home/property.sightlines/lattice/kernel/lib/ipc_shm.salt`

**Memory Layout (4KB page):**
```
Cache Line 0 (0-63):   Producer-owned: head (u64), capacity (u64)
Cache Line 1 (64-127): Consumer-owned: tail (u64)
Bytes 128-4095:        Data ring (3968 bytes usable)
```

**API:**
```salt
pub fn spsc_init(base_addr: u64)
pub fn spsc_push(base_addr: u64, byte: u8) -> bool
pub fn spsc_pop(base_addr: u64) -> i64
pub fn spsc_push_bulk(base_addr: u64, src: u64, len: u64) -> u64
pub fn spsc_pop_bulk(base_addr: u64, dst: u64, len: u64) -> u64
pub fn spsc_available(base_addr: u64) -> u64
pub fn spsc_free_space(base_addr: u64) -> u64
```

**Design:** Lock-free, cache-line isolated, ERMS-accelerated bulk operations.

### 3.2 Message Passing IPC (Register-Level)

**Location:** `/home/property.sightlines/lattice/kernel/core/syscall.salt`

**Syscalls:**
```salt
// Send 3-word message (fast path, assembly)
pub fn sys_ipc_send(target_pid: u64, msg0: u64, msg1: u64, msg2: u64) -> u64

// Block until message arrives
pub fn sys_ipc_recv() -> u64  // Returns sender_pid
```

**PCB IPC Fields:**
```salt
pub struct Process {
    pub ipc_sender: u64,    // PID of sender
    pub ipc_msg0: u64,      // Payload word 0
    pub ipc_msg1: u64,      // Payload word 1
    pub ipc_msg2: u64,      // Payload word 2
}
```

**Process State:** `PROC_IPC_BLOCKED` (4) — blocked on IPC receive

### 3.3 Shared Memory Grant

**Location:** `/home/property.sightlines/lattice/kernel/core/syscall.salt`

```salt
// Grant target process access to physical frames
pub fn sys_shm_grant(target_pid: u64, src_vaddr: u64, dst_vaddr: u64, num_pages: u64) -> u64
```

**Security Model:**
- Zero-copy (same physical transistors)
- Explicit grant (no global shared memory namespace)
- Target's dst_vaddr must be unmapped
- Max 16 pages (64KB) per grant

### 3.4 Network Bridge IPC

**Location:** `/home/property.sightlines/lattice/kernel/net/netd_bridge.salt`

**Frame Format:**
```
[2 bytes: frame_len (LE)] [frame_len bytes: Ethernet frame]
```

**API:**
```salt
pub fn init(netd_pid: u64) -> u64
pub fn push_frame(buf: u64, frame_len: u64) -> bool
pub fn pop_frame_len(ring_base: u64) -> u64
```

### 3.5 Socket Data Plane

**Location:** `/home/property.sightlines/lattice/kernel/benchmarks/socket_bench.salt`

**VADDR Layout:**
```salt
const SOCKET_VADDR_BASE: u64 = 0x600000000000
const SOCKET_REGION_SIZE: u64 = 8192  // Per socket
const SOCKET_PAGE_SIZE: u64 = 4096    // RX and TX each get 4KB

fn socket_rx_vaddr(fd: u64) -> u64 {
    return SOCKET_VADDR_BASE + (fd * SOCKET_REGION_SIZE)
}
fn socket_tx_vaddr(fd: u64) -> u64 {
    return SOCKET_VADDR_BASE + (fd * SOCKET_REGION_SIZE) + SOCKET_PAGE_SIZE
}
```

---

## 4. Lettuce Kernel Integration

### 4.1 Lettuce's OS Dependencies

**Location:** `/home/property.sightlines/lattice/lettuce/src/server.salt`

**Required Operations:**
```salt
use std.net.tcp.{TcpListener, TcpStream}
use std.net.poller.{Poller, Filter}

// TCP operations
TcpListener::bind(6379)
listener.accept()
stream.recv(buf, len)
stream.send(buf, len)

// Event polling
Poller::new()
poll.register(fd, Filter::Read)
poll.wait(events, max_events, timeout)
```

**Underlying syscalls needed:**
- `http_tcp_listen(port)` → Listening socket
- `http_accept(listen_fd)` → Accept connection
- `http_recv(fd, buf, len)` → Read from socket
- `http_send(fd, buf, len)` → Write to socket
- `http_kq_create()` → Event poller
- `http_kq_register(kq, fd, filter)` → Register fd
- `http_kq_wait(kq, events, max, timeout)` → Wait for events

### 4.2 Lattice's Network Architecture

**Design:** Userspace network daemon (NetD) with IPC

```
┌─────────────────────────────────────────┐
│ Ring 3: NetD Daemon (PID 5)             │
│  - Owns VirtIO NIC via kernel bridge    │
│  - Handles TCP/IP stack                 │
│  - Manages SPSC rings for data plane    │
└─────────────────────────────────────────┘
                    ↑↓ IPC (sys_ipc_send/recv)
┌─────────────────────────────────────────┐
│ Ring 3: User Applications               │
│  - socket.bind() → IPC to NetD          │
│  - socket.read() → Direct SPSC access   │
│  - socket.write() → Direct SPSC access  │
└─────────────────────────────────────────┘
```

### 4.3 What Lattice Provides

**Network Stack Status:**

| Component | Status | Location |
|-----------|--------|----------|
| VirtIO-Net Driver | ✅ Full | `kernel/drivers/virtio_net.salt` |
| Ethernet (L2) | ✅ Full | `kernel/net/eth.salt` |
| ARP | ✅ Kernel + NetD | `kernel/net/arp.salt`, `kernel/net/netd_arp.salt` |
| IPv4 (L3) | ⚠️ Basic (no routing/ICMP) | `kernel/net/ip.salt` |
| UDP (L4) | ✅ Full | `kernel/net/udp.salt` |
| TCP (L4) | ⚠️ TCB + Parser only | `kernel/net/netd_tcp*.salt` |
| Socket API | ✅ User-space | `user/lib/socket.salt` |
| NetD Daemon | ❌ Stub dispatcher | `user/netd.salt` |
| LatticeReactor | ❌ Stub (returns errors) | `std/io/reactor_lattice.salt` |

### 4.4 Architecture Mismatch

**Lettuce expects:**
```
Lettuce (Ring 3) → std.net.tcp → std.os.syscall → C bridge → Linux kqueue/epoll
```

**Lattice provides:**
```
App (Ring 3) → user.lib.socket → IPC → NetD (Ring 3) → kernel bridge → VirtIO
```

**Key Differences:**
- Lettuce uses **direct syscalls** to kernel networking
- Lattice uses **IPC to NetD** for user-space networking
- Lattice has no event polling (epoll/kqueue equivalent)

### 4.5 Feasibility Assessment

**Could Lettuce run on Lattice today?** ❌ **No**

**Blockers:**
1. No TCP socket syscalls exposed to Ring 3
2. No event polling (LatticeReactor is stub)
3. NetD daemon incomplete (doesn't process packets)
4. Architecture mismatch (direct syscalls vs IPC)

**Path Forward (2-4 weeks):**
1. Complete NetD daemon implementation
2. Port Lettuce to use Lattice's socket API
3. Add event notification to socket API

---

## 5. Network Stack

### 5.1 NIC Driver Support

**Implemented:**
- ✅ VirtIO-Net (Legacy PCI) — `/home/property.sightlines/lattice/kernel/drivers/virtio_net.salt`
  - PCI device discovery (vendor 0x1AF4, device 0x1000)
  - RX/TX virtqueue management (256 descriptors)
  - 64 pre-posted RX buffers (1524 bytes each)
  - MAC address from device config
  - Poll-based RX, descriptor-based TX

**Missing:**
- ❌ e1000/e1000e (Intel)
- ❌ rtl8139/rtl8168 (Realtek)
- ❌ Modern VirtIO (MMIO, multi-queue)
- ❌ Physical NIC drivers (QEMU-emulated only)

### 5.2 Network Stack Layers

| Layer | Component | Status |
|-------|-----------|--------|
| L2 | Ethernet | ✅ Full (`eth.salt`) |
| L2.5 | ARP | ✅ Kernel + NetD |
| L3 | IPv4 | ⚠️ Basic (no routing/ICMP) |
| L4 | UDP | ✅ Full (`udp.salt`) |
| L4 | TCP | ⚠️ TCB + Parser only |

### 5.3 Data Plane

**RX Bridge:** (`kernel/net/netd_bridge.salt`)
- Pushes length-prefixed frames: `[2B len][N bytes frame]`
- Signals NetD via IPC (CMD_RX_NOTIFY)

**TX Bridge:** (`kernel/net/netd_tx_bridge.salt`)
- Drains frames from NetD TX ring to VirtIO
- Runt/oversize protection (14-1514 bytes)

### 5.4 Benchmark Suite

**Location:** `/home/property.sightlines/lattice/kernel/benchmarks/netd_bench.salt`

**19 Test Gates:**
1. Data Plane: SPSC push/pop, UDP parsing, IPC notification
2. Bridge Framing: Length-prefixed push/pop
3. Protocol Parsers: bswap16, ARP cache update/evict
4. TX Bridge: Frame push/pop
5. TCP Stack: TCB allocation, lookup, header parsing, checksum
6. Throughput: 10K frame TX pipeline

**Expected Performance (3.0 GHz):**
- TCG (QEMU emulation): ~2M PPS
- KVM (hardware virt): ~60M+ PPS

---

## 6. Basalt Dependencies

### 6.1 Basalt's OS Requirements

**Location:** `/home/property.sightlines/lattice/basalt/src/main.salt`

**Required Operations:**
```salt
// Memory mapping (zero-copy model loading)
extern fn open(path: Ptr<u8>, flags: i32) -> i32
extern fn mmap(addr: Ptr<u8>, len: u64, prot: i32, flags: i32, fd: i32, offset: i64) -> Ptr<u8>
extern fn close(fd: i32) -> i32

// Memory allocation
extern fn malloc(size: i64) -> Ptr<u8>
extern fn free(ptr: Ptr<u8>)

// Math library
extern fn cosf(x: f32) -> f32
extern fn sinf(x: f32) -> f32
extern fn expf(x: f32) -> f32
extern fn sqrtf(x: f32) -> f32

// I/O and timing
extern fn write(fd: i32, buf: Ptr<u8>, count: i64) -> i64
extern fn salt_clock_now() -> i64
extern fn salt_get_argc() -> i32
extern fn salt_get_argv(idx: i32) -> Ptr<u8>
```

**Memory Requirements (stories15M.bin):**
- Model weights: 15MB (mmap'd, zero-copy)
- Run state: ~10-20 MB (malloc'd)
- KV cache: ~17 MB
- Total: ~50-100 MB

### 6.2 What Lattice Provides

| Feature | Basalt Needs | Lattice Provides | Status |
|---------|-------------|------------------|--------|
| Anonymous mmap | ✅ `mmap(len, prot)` | ✅ `sys_mmap(length, prot)` | ✅ Works |
| malloc/free | ✅ Variable-size alloc | ⚠️ Raw mmap/brk only | ⚠️ Needs wrapper |
| File-backed mmap | ✅ `mmap(fd, offset)` | ❌ Not implemented | ❌ Missing |
| open/read/close | ✅ File I/O | ❌ Not implemented | ❌ Missing |
| libm (math) | ✅ cosf, sinf, expf, sqrtf | ⚠️ Compiler intrinsics? | ⚠️ Needs impl |
| stdout write | ✅ `write(1, buf, len)` | ✅ `sys_write(1, buf, len)` | ✅ Works |
| Clock | ✅ `salt_clock_now()` | ✅ `salt_clock_now()` | ✅ Works |
| argc/argv | ✅ Command-line args | ⚠️ Exists in std/args | ⚠️ Verify |

### 6.3 Critical Gaps

**1. File System / Block Storage** ❌ **MISSING**

Lattice has:
- `/home/property.sightlines/lattice/std/fs/fs.salt` — POSIX-style API declarations
- `extern fn salt_opendir`, `salt_readdir` — Directory iteration

Lattice **doesn't have:**
- No `open()`, `read()`, `close()` for regular files
- No `mmap()` for file-backed mappings
- No block device driver integration with user-space

**ARCHITECTURE.md mentions:**
> **LatticeStore** — Block storage via VMO (planned v0.9.2)

This is listed as "planned" — not yet implemented.

**2. File-Backed mmap** ❌ **MISSING**

Basalt needs:
```salt
mmap(addr, len, prot, flags, fd, offset)  // File-backed mapping
```

Lattice provides:
```salt
sys_mmap(length, prot)  // Anonymous mapping only (like MAP_ANONYMOUS)
```

### 6.4 Feasibility Assessment

**Could Basalt run on Lattice today?** ❌ **No**

**Blocker:** No file system / file-backed mmap for model loading

**Workaround Options:**

**Option A: Compile-Time Model Embedding** (Recommended for demo)
- Convert `model.bin` → Salt data array at build time
- Link weights directly into binary
- No runtime file I/O needed

**Option B: Network-Loaded Model**
- Use Lattice's VirtIO networking
- Fetch model from host via TCP
- Store in mmap'd buffer

**Option C: Full File System** (Long-term)
- Complete LatticeStore (planned v0.9.2)
- Integrate VirtIO block driver
- Add `open/read/close/mmap` syscalls

---

## 7. Summary & Recommendations

### 7.1 Current Userspace Capabilities

**What Works:**
- ✅ ELF loading and process spawning
- ✅ 4-core SMP with per-core scheduling
- ✅ Memory isolation (KPTI, PCID-tagged TLBs)
- ✅ Syscall interface (8 syscalls implemented)
- ✅ IPC mechanisms (SPSC rings, message passing, shared memory grants)
- ✅ Network stack (VirtIO-Net, Ethernet, ARP, IPv4, UDP)
- ✅ Socket API (control plane via IPC, data plane via SPSC rings)

**What's Missing:**
- ❌ File system / block storage
- ❌ File-backed mmap
- ❌ TCP socket syscalls for user-space
- ❌ Event polling (epoll/kqueue equivalent)
- ❌ Complete NetD daemon
- ❌ malloc/free wrapper

### 7.2 Agent Compatibility

| Agent | Can Run? | Blockers | Effort to Fix |
|-------|----------|----------|---------------|
| **Basalt** | ❌ No | No file system, no file-backed mmap | Medium (2-4 weeks for embedding workaround) |
| **Lettuce** | ❌ No | No TCP syscalls, no event polling | Medium-High (4-6 weeks for NetD completion) |

### 7.3 Recommended Next Steps

**Phase 1: Quick Wins (1-2 weeks)**
1. Implement malloc/free wrapper using `sys_mmap` as backing
2. Verify argc/argv passing for user processes
3. Add math intrinsics to Salt compiler (cosf, sinf, expf, sqrtf)

**Phase 2: Basalt Demo (2-4 weeks)**
1. Write Python script to convert `model.bin` → Salt data array
2. Modify Basalt to use embedded weights instead of mmap
3. Test inference on Lattice kernel in QEMU

**Phase 3: Lettuce Enablement (4-6 weeks)**
1. Complete NetD daemon implementation
2. Port Lettuce to use Lattice's socket API
3. Add event notification/polling to socket API

**Phase 4: Full File System (8-12 weeks)**
1. Complete LatticeStore (v0.9.2)
2. Integrate VirtIO block driver
3. Add `open/read/close/mmap` syscalls
4. Port Basalt to use native file I/O

### 7.4 Files Reference

**Kernel Core:**
- `/home/property.sightlines/lattice/kernel/core/syscall.salt` — Syscall dispatch
- `/home/property.sightlines/lattice/kernel/core/exec_user.salt` — ELF loader
- `/home/property.sightlines/lattice/kernel/core/process.salt` — PCB definition
- `/home/property.sightlines/lattice/kernel/arch/x86_64/syscall_entry_fast.S` — SYSCALL entry
- `/home/property.sightlines/lattice/kernel/arch/x86/ring3_entry.S` — Ring 3 transition

**IPC:**
- `/home/property.sightlines/lattice/kernel/lib/ipc_shm.salt` — SPSC ring buffer
- `/home/property.sightlines/lattice/kernel/lib/ipc_ring.salt` — Z3-verified SPSC struct
- `/home/property.sightlines/lattice/kernel/lib/ipc_arbiter.salt` — SipHash validation

**Network:**
- `/home/property.sightlines/lattice/kernel/drivers/virtio_net.salt` — VirtIO-Net driver
- `/home/property.sightlines/lattice/kernel/net/eth.salt` — Ethernet parser
- `/home/property.sightlines/lattice/kernel/net/ip.salt` — IPv4 parser
- `/home/property.sightlines/lattice/kernel/net/udp.salt` — UDP parser
- `/home/property.sightlines/lattice/kernel/net/netd_tcp.salt` — TCP TCB pool
- `/home/property.sightlines/lattice/kernel/net/netd_bridge.salt` — RX bridge
- `/home/property.sightlines/lattice/kernel/net/netd_tx_bridge.salt` — TX bridge

**User-Space:**
- `/home/property.sightlines/lattice/user/lib/syscall.salt` — Syscall wrappers
- `/home/property.sightlines/lattice/user/lib/socket.salt` — Socket API
- `/home/property.sightlines/lattice/user/netd.salt` — NetD daemon (stub)

**Applications:**
- `/home/property.sightlines/lattice/lettuce/src/server.salt` — Lettuce server
- `/home/property.sightlines/lattice/basalt/src/main.salt` — Basalt inference engine

---

**Report Generated:** March 1, 2026  
**Investigation Status:** Complete  
**Next Review:** After Phase 1 implementation
