/* kernel/arch/x86/linker.ld */
ENTRY(start)

SECTIONS
{
    /* 1. Identity Mapped Boot Section (Physical = Virtual) */
    . = 0x00100000;
    _phys_base = .;

    .boot :
    {
        *(.multiboot)
        *(.entry)
    }

    /* 2. Higher Half Kernel Section (-2GB) */
    . = 0xFFFFFFFF80101000;
    _virt_base = .;

    .text : AT(ADDR(.text) - 0xFFFFFFFF80000000)
    {
        _code = .;
        *(.text)
        . = ALIGN(4096);
    }

    .rodata : AT(ADDR(.rodata) - 0xFFFFFFFF80000000)
    {
        _rodata = .;
        *(.rodata)
        . = ALIGN(4096);
    }

    .data : AT(ADDR(.data) - 0xFFFFFFFF80000000)
    {
        _data = .;
        *(.data)
        *(.got)
        . = ALIGN(4096);
        _data_end = .;
    }

    .bss : AT(ADDR(.bss) - 0xFFFFFFFF80000000)
    {
        _bss = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(4096);
        _bss_end = .;
    }

    _end = .;

    /* Physical address aliases for boot.S (using direct subtraction of the higher-half offset) */
    pml4_phys = pml4 - 0xFFFFFFFF80000000;
    pdpt_phys = pdpt - 0xFFFFFFFF80000000;
    pd_phys   = pd - 0xFFFFFFFF80000000;
    stack_top_phys = stack_top - 0xFFFFFFFF80000000;
    long_mode_start_phys = long_mode_start;
    gdt64_ptr_phys = gdt64_ptr - 0xFFFFFFFF80000000;
}
