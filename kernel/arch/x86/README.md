# x86 (Boot & Setup)

**The Mission:** Take the CPU from a raw Multiboot state (32-bit Protected Mode) to a pristine 64-bit Long Mode environment.

## Invariants

### 1. The 1MB Identity Map
The kernel is loaded at physical address `0x100000` (1MB).
We identity map the first 16MB of physical memory using 2MB Huge Pages in `boot.S`.
$$Virt(0x100000) \rightarrow Phys(0x100000)$$

### 2. The Dirty BSS Trap
The Salt compiler assumes `.bss` is zeroed. The bootloader *should* do this, but we explicitly `rep stosd` the page tables and BSS to zero in `boot.S` to prevent "Ghost in the Machine" bugs where uninitialized page tables cause triple faults.

## Components

| File | Role | Key Mechanism |
|------|------|---------------|
| [`boot.S`](./boot.S) | **The Entry Point.** Multiboot header and mode scaffolding. | `start`: The first instruction executed. |
| [`idt.salt`](./idt.salt) | **Interrupt Descriptor Table.** The 64-bit IDT definition. | `lidt_wrapper`: Loads the 10-byte IDT pointer. |
| [`gdt.salt`](./gdt.salt) | **Global Descriptor Table.** Segment definitions. | Defines Code/Data segments for functionality. |

## Entry Point
**`boot.S`** -> `start:`
1. **Diagnostic 'Y':** Signals entry.
2. **Page Table Setup:** Identity maps 0-16MB.
3. **Long Mode Enable:** Sets EFER.LME, CR4.PAE, CR0.PG.
4. **Far Jump:** `retf` to 64-bit code segment.
5. **Stack Switch:** Moves RSP to higher-half virtual address.
6. **Call kmain:** Jumps to `kernel.core.main.kmain`.

## Troubleshooting
**Symptom:** "QEMU hangs at 'SeaBIOS...'"
- **Cause:** Multiboot header magic is wrong or not within the first 8KB.
- **Fix:** Check `linker.ld` ensures `.multiboot` section is at `0x100000`.

**Symptom:** "Triple Fault (Reboot) instantly"
- **Cause:** Page tables malformed or GDT invalid.
- **Fix:** Verify `pml4`, `pdpt`, and `pd` are 4KB aligned and zeroed before linkage.
