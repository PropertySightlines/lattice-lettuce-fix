.intel_syntax noprefix

# =============================================================================
# cpuid_stubs.S — CPUID instruction wrappers for Salt
# =============================================================================

# cpuid_leaf1_ecx() -> u64
# Returns ECX from CPUID leaf 1 (feature flags, bit 31 = hypervisor)
.global cpuid_leaf1_ecx
cpuid_leaf1_ecx:
    push rbx
    mov eax, 1
    cpuid
    mov rax, rcx
    pop rbx
    ret

# cpuid_hypervisor_id() -> u64
# Returns EBX from CPUID leaf 0x40000000 (first 4 bytes of hypervisor brand)
# KVM: "KVMK" = 0x4b564d4b
# Microsoft: "Micr" = 0x7263694d
.global cpuid_hypervisor_id
cpuid_hypervisor_id:
    push rbx
    mov eax, 0x40000000
    cpuid
    mov rax, rbx
    pop rbx
    ret

# cpuid_base_freq_mhz() -> u64
# Returns EAX from CPUID leaf 0x16 (Processor Frequency Information)
# EAX = Base frequency in MHz (e.g. 4000 for 4.0 GHz)
# Returns 0 if leaf 0x16 is not supported.
# Supported on Intel Skylake+ (including Xeon 8151 / z1d.metal).
.global cpuid_base_freq_mhz
cpuid_base_freq_mhz:
    push rbx
    # First check if leaf 0x16 is supported (max basic leaf >= 0x16)
    xor eax, eax
    cpuid               # EAX = max supported basic leaf
    cmp eax, 0x16
    jb .no_leaf16
    mov eax, 0x16
    cpuid
    # EAX = base frequency in MHz
    and rax, 0xFFFF     # only low 16 bits are defined
    pop rbx
    ret
.no_leaf16:
    xor eax, eax        # return 0 = unsupported
    pop rbx
    ret
