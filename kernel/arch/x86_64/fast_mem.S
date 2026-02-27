.intel_syntax noprefix

/* =============================================================================
 * kernel/arch/x86_64/fast_mem.S
 * Hardware-Accelerated Memory Operations (ERMS)
 * =============================================================================
 *
 * Uses Enhanced REP MOVSB (ERMS), available on all Intel Skylake+ and
 * AMD Zen+ processors. When the CPU detects ERMS support via CPUID,
 * REP MOVSB internally uses 256-bit vector registers for the transfer,
 * achieving throughput competitive with hand-tuned AVX2 memcpy.
 *
 * ERMS advantage over compiler-emitted byte loops:
 *   - Single µop dispatch per ~64 bytes (vs 1 µop per byte)
 *   - Internal prefetch eliminates cache-line splits
 *   - No loop branch prediction overhead
 *
 * Salt declarations:
 *   extern fn fast_memcpy(dst: u64, src: u64, count: u64);
 *   extern fn fast_memset(dst: u64, val: u8, count: u64);
 * ============================================================================= */

/* fast_memcpy — ERMS-accelerated block copy
 * ABI (System V x86_64):
 *   RDI = destination address
 *   RSI = source address
 *   RDX = byte count
 *
 * Clobbers: RCX, RSI, RDI (per REP MOVSB semantics)
 * Direction flag: Cleared by FMASK on SYSCALL entry.
 *                 Explicitly cleared here for safety. */
.global fast_memcpy
.type fast_memcpy, @function
fast_memcpy:
    mov rcx, rdx        /* RCX = count (REP counter) */
    cld                  /* Ensure forward direction (DF=0) */
    rep movsb            /* Copy RCX bytes from [RSI] to [RDI] */
    ret

/* fast_memset — ERMS-accelerated block fill
 * ABI:
 *   RDI = destination address
 *   RSI = fill byte (only low 8 bits used)
 *   RDX = byte count */
.global fast_memset
.type fast_memset, @function
fast_memset:
    mov rcx, rdx        /* RCX = count */
    mov rax, rsi        /* AL = fill byte */
    cld
    rep stosb            /* Fill RCX bytes at [RDI] with AL */
    ret

/* cpu_cli — Disable maskable interrupts (Clear Interrupt Flag)
 * Used to create an interrupt-free measurement window for benchmarks.
 * Salt: extern fn cpu_cli(); */
.global cpu_cli
.type cpu_cli, @function
cpu_cli:
    cli
    ret

/* cpu_sti — Enable maskable interrupts (Set Interrupt Flag)
 * Salt: extern fn cpu_sti(); */
.global cpu_sti
.type cpu_sti, @function
cpu_sti:
    sti
    ret
