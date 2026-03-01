# Lettuce Linux Segfault — Solution

**Date:** March 1, 2026  
**Status:** SOLVED ✓

---

## Summary

The Lettuce server was segfaulting on Linux when a client connected. The root cause was a **compiler stack frame allocation bug** in the Salt compiler, which was worked around by reordering local variable declarations in `handle_client()`.

---

## The Bug

### Symptoms
- Server starts successfully, binds to port 6379
- Segfaults immediately when `redis-cli ping` connects
- GDB backtrace showed crash in `handle_client()` at session state update code

### Root Cause Analysis

GDB revealed the crash occurred at:
```
0x555555570639 <main.handle_client+105401>: mov %r15,(%rbx)
```

With register state:
```
rbx = 0x0  (session pointer - NULL!)
rip = 0x555555570639 (crash location)
```

**The actual issue:** The Salt compiler was miscomputing the stack frame size for `handle_client()`:

1. Stack frame allocated: `0x1e38` (7736 bytes)
2. Session pointer stored at offset: `-0x1e40` (7744 bytes from rbp)
3. **Problem:** 7744 > 7736, so session pointer was stored **8 bytes outside allocated stack**
4. Later, `memset` of `send_buf` (4096 bytes) overwrote the session pointer with zeros
5. When code tried to write `session.read_cursor = unparsed_len`, it dereferenced NULL → segfault

### Original Code (Buggy)

```salt
fn handle_client(...) {
    let stream = TcpStream { fd: fd };
    let session = slab.get(fd);  // Session pointer stored at rbp-0x1e40
    
    // ... read operations ...
    
    let mut send_buf: [u8; 4096] = [0; 4096];  // Large memset overwrites session pointer!
    let send_ptr = &send_buf[0] as Ptr<u8>;
    
    // ... rest of function ...
    
    session.read_cursor = unparsed_len;  // CRASH: session is NULL
    session.parse_cursor = 0;
}
```

---

## The Fix

**Workaround:** Move the large `send_buf` allocation to the **beginning** of the function, before the session pointer is stored. This changes the compiler's stack layout so the session pointer ends up in valid memory.

### Fixed Code

```salt
fn handle_client(...) {
    // Pre-allocate send buffer FIRST to avoid stack corruption
    let mut send_buf: [u8; 4096] = [0; 4096];
    let send_ptr = &send_buf[0] as Ptr<u8>;
    let send_buf_cap: i64 = 4096;
    
    let stream = TcpStream { fd: fd };
    let session = slab.get(fd);  // Now stored in safe location
    
    // ... rest of function unchanged ...
    
    session.read_cursor = unparsed_len;  // ✓ Works correctly
    session.parse_cursor = 0;
}
```

---

## Verification

### Test Suite
```
$ sp test lettuce
Running 3 test(s)...
   test_resp ... ✓ pass
   test_smap_mini ... ✓ pass
   test_store ... ✓ pass
Result: 3 passed, 0 failed (3.8s)
```

### Manual Testing
```
$ redis-cli ping
PONG

$ redis-cli set mykey "hello world"
OK

$ redis-cli get mykey
hello world

$ redis-cli del mykey
1

$ redis-cli set counter 100
OK

$ redis-cli get counter
100
```

Server remains stable after multiple connections and operations.

---

## Files Modified

| File | Change |
|------|--------|
| `lettuce/src/server.salt` | Reordered local variable declarations in `handle_client()` to place `send_buf` before session pointer |

---

## Technical Details

### Why This Happens

The Salt compiler's stack frame generation has a bug where it:
1. Doesn't correctly account for all local variable offsets
2. Allocates insufficient stack space
3. Large `memset` operations can overwrite adjacent stack locations

This is **Linux-specific** because:
- Different ABI calling conventions (System V AMD64 vs macOS x86_64)
- Different register allocation patterns
- Different stack layout requirements

### Long-term Fix Required

The **proper fix** is in the Salt compiler itself:
- Fix stack frame size calculation to include all local variables
- Ensure proper alignment and spacing for all stack slots
- Add validation that stack offsets don't exceed allocated frame

This workaround should be reverted once the compiler is fixed.

---

## Lessons Learned

1. **GDB is essential** for debugging segfaults — register state revealed the NULL pointer
2. **Stack corruption** can manifest far from the actual cause — the memset was the culprit, not the session access
3. **Compiler bugs** can be worked around with source code restructuring
4. **Variable ordering matters** when the compiler has bugs in stack allocation

---

## Related Files for Investigation

- `salt/src/passes/` — Compiler passes that handle stack allocation
- `salt/src/codegen/` — Code generation that emits stack frame setup
- `salt-front/std/collections/slab.salt` — Slab allocator (initially suspected)
- `salt-front/std/net/http_bridge.c` — epoll bridge (initially suspected)
