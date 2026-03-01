# Lettuce Linux Segfault — Technical Analysis

**Author:** Claude (session handoff)  
**Date:** March 1, 2026  
**Status:** Unsolved — hypotheses documented for next investigator

---

## The Bug in One Sentence

Lettuce server starts fine, binds :6379, then **segfaults the instant a client connects**.

---

## What We Know For Certain

| Observation | Implication |
|-------------|-------------|
| Build succeeds | Compiler, Z3, MLIR chain all working |
| Server binds port | `http_tcp_listen()` in epoll bridge works |
| Crash on connect, not accept | Problem is **after** accept(), likely in `handle_client()` or `Slab<T>` allocation |
| Runtime warning: `[Sovereign] Blocking functions detected: ["main"]` | Sovereign async runtime has opinions about blocking — **this may be causal** |

---

## Hypothesis 1: Sovereign Runtime Mismatch (HIGH CONFIDENCE)

### The Warning
[Sovereign] Blocking functions detected: ["main"]

### What This Likely Means Sovereign is Salt's async runtime (like Tokio for Rust). It expects `main()` to be non-blocking, yielding to an event loop. On macOS, this probably works due to kqueue semantics. On Linux with our epoll bridge, something about the blocking detection is triggering. ### Why This Could Cause Segfault If Sovereign expects to own the event loop but `main()` is blocking on `http_kq_wait()`, we might have: - Two things polling the same fd - Stack corruption from unexpected re-entry - Arena getting freed while still in use ### Where to Look
std/runtime/sovereign.salt — async runtime implementation lettuce/src/server.salt — main() function, check async annotations

### What to Try 1. Look for `@async` or `@blocking` annotations in server.salt 2. Check if macOS version of main() differs from what we're running 3. See if there's a `sovereign_run()` or similar entry point we should use --- ## Hypothesis 2: Slab<T> Allocation Bug (MEDIUM CONFIDENCE) ### The Theory `Slab<ClientSession>` is a pool allocator. When a client connects, we allocate a session. If Slab has platform-specific assumptions: - Pointer size (macOS arm64 vs Linux x86_64?) - Alignment requirements - mmap behavior differences ### Suspicious Code Pattern ```salt // Pseudocode of what probably happens let session = slab.alloc() // <-- crash here? session.socket = accepted_fd
Where to Look
std/collections/slab.salt — the allocator itself lettuce/src/server.salt — handle_client(), find Slab usage
What to Try
# Add debug prints before/after slab operations
# Or use GDB to catch exact crash point (see below)
Hypothesis 3: Epoll Bridge Semantic Gap (MEDIUM CONFIDENCE)
The Issue
Our http_bridge.c emulates kqueue using epoll. But there are subtle differences:

kqueue	epoll	Risk
Returns actual event count	Returns ready count	Should be fine
EVFILT_READ = -1	EPOLLIN = 0x001	We map this — verify correctness
EV_EOF handling	EPOLLHUP/EPOLLRDHUP	Edge cases may differ
kevent timeout in ms	epoll_wait timeout in ms	Should be fine
Specific Concern
When a client connects, kqueue might deliver the event differently than epoll. If Salt code expects a specific event structure, our translation might be wrong.

Where to Look
std/net/http_bridge.c — our epoll implementation std/net/kqueue.salt — what Salt expects (if exists)
What to Try
Add logging to http_kq_wait() to print exactly what events we're returning.

Hypothesis 4: Arena Lifetime Issue (LOWER CONFIDENCE)
The Theory
Arena for client session gets allocated, but something triggers early free:

Sovereign runtime "helping" by cleaning up blocking context
Connection accepted in one arena, handled in another
Z3 verification passed on macOS assumptions, fails on Linux
Where to Look
lettuce/src/server.salt — arena usage in handle_client std/mem/arena.salt — arena implementation
Debugging Commands
GDB Approach
# Build with debug symbols (check if sp has a debug flag)
sp build lettuce --debug  # or similar

# Run under GDB
gdb /tmp/salt_build/server
(gdb) run
# In another terminal: redis-cli ping
# GDB will catch the segfault

(gdb) bt                    # backtrace — WHERE did it crash
(gdb) info registers        # register state
(gdb) x/20x $rsp            # stack inspection
strace Approach
strace -f /tmp/salt_build/server 2>&1 | tee /tmp/strace.log
# Connect with redis-cli, then examine log
# Look for last successful syscall before crash
Add Printf Debugging
If you can modify server.salt, add prints:

The Test Suite Strategy
RUN TESTS FIRST — sp test lettuce

The 66 tests likely include unit tests for:

RESP parsing (resp.salt)
Command execution (store.salt)
Possibly Slab operations
If tests fail, they'll point directly at the broken component. If tests pass but server crashes, the bug is specifically in the integration — how components connect at runtime.

Files by Investigation Priority
Priority	File	Why
1	lettuce/src/server.salt	Contains main(), handle_client() — crash site
2	std/runtime/sovereign.salt	The "blocking functions" warning originates here
3	std/collections/slab.salt	Session allocation
4	std/net/http_bridge.c	Our Linux port — verify event translation
5	std/mem/arena.salt	If arena lifetime is the issue
Quick Wins to Try
Compare main() with working examples

grep -r "fn main" ~/lattice --include="*.salt" | head -20
See how other Salt programs structure main() — does Lettuce differ?

Check for platform conditionals

grep -r "linux\|darwin\|macos\|platform" ~/lattice --include="*.salt"
Maybe there's platform-specific code we're missing.

Look at Sovereign initialization

grep -r "Sovereign\|sovereign\|blocking" ~/lattice --include="*.salt"
Understand what triggers that warning.

What Success Looks Like
$ sp test lettuce
Running 66 tests...
66/66 passed ✓

$ sp build lettuce &
[Lettuce] Listening on :6379

$ redis-cli ping
PONG

$ redis-cli set foo bar
OK

$ redis-cli get foo
"bar"
If You Get Stuck
File findings in GitHub issue (already open): https://github.com/bneb/lattice/issues/
The maintainer (bneb) has responded to issues before, though slowly
Consider trying Basalt instead — different code path, might reveal if issue is Lettuce-specific or systemic
Good luck. The bug is solvable — it's just a matter of finding which assumption breaks on Linux. EOF

