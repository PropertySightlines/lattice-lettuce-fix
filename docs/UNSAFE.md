# Salt `unsafe` Blocks

## Overview

`unsafe` in Salt is a **stdlib-only escape hatch** for operations that bypass Salt's memory safety guarantees. User code cannot use `unsafe` — all low-level operations must go through the standard library's safe abstractions.

> [!IMPORTANT]
> This is a deliberate design choice. Salt's safety model guarantees that if your code compiles without `unsafe`, it is memory-safe. Since only the stdlib (authored by the Salt team) can use `unsafe`, the attack surface for memory bugs is small and auditable.

## What `unsafe` Gates (Stdlib Only)

| Operation | Safe Alternative |
|-----------|-----------------|
| Raw pointer construction from integer (`42 as Ptr<u8>`) | `Vec<T>`, `Arena`, `File::open()` |
| `reinterpret_cast<T>(ptr)` | Pattern matching, `as` for numeric casts |
| Direct `sys_*` syscall FFI | `File`, `std.os.tcp`, `std.env` |
| Manual memory layout assumptions | `struct` with compile-time layout |

## How It Works

```salt
// In stdlib code (e.g., std/core/mem.salt):
unsafe {
    let raw = salt_mmap(size) as Ptr<u8>;  // OK — stdlib can do this
    raw.write(0);
}

// In user code:
unsafe {  // ← COMPILE ERROR: unsafe blocks are not allowed in user code
    let raw = 42 as Ptr<u8>;
}
```

## Why Not Rust's Model?

Rust allows `unsafe` in any crate. Salt takes a stricter approach:

- **Smaller audit surface**: Only `~20 files` in `salt-front/std/` need safety review
- **Simpler mental model**: "If it compiles, it's safe" — no need to audit deps
- **Ecosystem safety**: Third-party Salt packages cannot introduce memory unsafety

## For Stdlib Authors

When writing stdlib code that requires `unsafe`:
1. Keep the `unsafe` block as small as possible
2. Document the safety invariant in a comment
3. Expose a safe public API that upholds the invariant
