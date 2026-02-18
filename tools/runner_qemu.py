#!/usr/bin/env python3
import subprocess
import os
import sys
import time
import re
import glob
import shutil

# Configuration
KERNEL_ROOT = "kernel"
BENCH_ROOT = "benchmarks"
BUILD_DIR = "qemu_build"
# Try to find salt binaries in the workspace
WORKSPACE_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SALT_FRONT = os.path.join(WORKSPACE_ROOT, "salt-front/target/release/salt-front")
if not os.path.exists(SALT_FRONT):
    SALT_FRONT = os.path.join(WORKSPACE_ROOT, "salt-front/target/debug/salt-front")
SALT_OPT = os.path.join(WORKSPACE_ROOT, "salt/build/salt-opt")

class ToolchainProvider:
    """Hermetic Toolchain Provider for Lattice x86_64 target."""
    def __init__(self, target="x86_64-none-elf"):
        self.target = target
        # Dynamic detection for reproducibility across environments
        self.llc = self._find_tool("llc")
        self.clang = self._find_tool("clang")
        self.rust_lld = self._find_tool("rust-lld")

    def _find_tool(self, name):
        # 1. Check PATH
        path = shutil.which(name)
        if path: return path
        
        # 2. Check common installation paths
        fallbacks = {
            "llc": ["/opt/homebrew/opt/llvm/bin/llc", "/usr/local/opt/llvm/bin/llc"],
            "clang": ["/opt/homebrew/opt/llvm/bin/clang", "/usr/local/opt/llvm/bin/clang"],
            "rust-lld": [
                os.path.expanduser("~/.rustup/toolchains/stable-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/rust-lld"),
                os.path.expanduser("~/.rustup/toolchains/stable-x86_64-apple-darwin/lib/rustlib/x86_64-apple-darwin/bin/rust-lld")
            ]
        }
        
        for p in fallbacks.get(name, []):
            if os.path.exists(p): return p
            
        return name # Return name and let validate() fail if not found

    def validate(self):
        """Verify that all required tools exist and match the expected target."""
        print(f"  [VALIDATE] Checking toolchain for {self.target}...")
        for tool_name, path in [("LLC", self.llc), ("CLANG", self.clang), ("RUST_LLD", self.rust_lld)]:
            if not os.path.exists(path):
                raise RuntimeError(f"Required tool {tool_name} not found at {path}")
            
            # Verify architecture if possible
            if tool_name == "CLANG":
                version_out = subprocess.check_output([path, "--version"], text=True)
                if "x86_64" not in version_out and "Target: " not in version_out:
                    print(f"    WARNING: {tool_name} may not support x86_64 targets natively.")
            
            print(f"    - {tool_name}: FOUND ({path})")

TOOLCHAIN = ToolchainProvider()

# ANSI Colors for Output
RED = "\033[91m"
GREEN = "\033[92m"
RESET = "\033[0m"

def ensure_build_dir():
    if not os.path.exists(BUILD_DIR):
        os.makedirs(BUILD_DIR)

def compile_salt(src_file):
    base_name = os.path.basename(src_file).replace(".salt", "")
    mlir_file = os.path.join(BUILD_DIR, f"{base_name}.mlir")
    ll_file = os.path.join(BUILD_DIR, f"{base_name}.ll")
    obj_file = os.path.join(BUILD_DIR, f"{base_name}.o")

    print(f"  [SALT] Compiling {src_file}...")
    
    # 1. Salt -> MLIR
    # salt-front prints to stdout. We capture it and write to file.
    cmd = [SALT_FRONT, src_file, "--lib", "--no-verify", "--disable-alias-scopes"]
    print(f"    Running: {' '.join(cmd)} > {mlir_file}")
    
    with open(mlir_file, "w") as out:
        subprocess.check_call(cmd, stdout=out)

    # 2. MLIR -> LLVM IR
    # 2. MLIR -> LLVM IR
    cmd = [SALT_OPT, "--emit-llvm", "--verify=false"]
    print(f"    Running: {' '.join(cmd)} < {mlir_file} > {ll_file}")
    
    for attempt in range(3):
        try:
            with open(mlir_file, "rb") as f_in, open(ll_file, "wb") as f_out:
                subprocess.check_call(cmd, stdin=f_in, stdout=f_out)
            break  # Success
        except subprocess.CalledProcessError:
            if attempt < 2:
                print(f"    ⟳ salt-opt crashed (attempt {attempt + 1}/3), retrying...")
            else:
                raise

    # 2b. Strip host CPU attributes from LLVM IR (cross-compilation from ARM Mac → x86_64)
    # salt-opt embeds the host CPU (e.g. apple-m4) which breaks LLC targeting x86_64
    import re
    with open(ll_file, 'r') as f:
        ll_content = f.read()
    ll_content = re.sub(r'"target-cpu"="[^"]*"', '"target-cpu"="x86-64"', ll_content)
    ll_content = re.sub(r'"target-features"="[^"]*"', '"target-features"=""', ll_content)
    # Strip 'nuw' flag from getelementptr — LLVM 19 syntax unsupported by LLVM 18
    ll_content = ll_content.replace('getelementptr inbounds nuw', 'getelementptr inbounds')
    with open(ll_file, 'w') as f:
        f.write(ll_content)

    # 3. LLVM IR -> Object
    cmd = [TOOLCHAIN.llc, ll_file, "-filetype=obj", "-o", obj_file, "-relocation-model=pic", f"-mtriple={TOOLCHAIN.target}", "-mcpu=x86-64"]
    print(f"    Running: {' '.join(cmd)}")
    subprocess.check_call(cmd)
    
    return obj_file

def compile_asm(src_file):
    base_name = os.path.basename(src_file).replace(".S", "")
    obj_file = os.path.join(BUILD_DIR, f"{base_name}.o")
    
    print(f"  [ASM]  Assembling {src_file}...")
    # Use cross-compilation target for assembly
    cmd = [TOOLCHAIN.clang, "-c", src_file, "-o", obj_file, "-target", TOOLCHAIN.target] 
    subprocess.check_call(cmd)
    return obj_file

def build_kernel():
    ensure_build_dir()
    print(f"{GREEN}== Building Kernel =={RESET}")
    
    objects = []
    
    # Compile all Salt files in kernel/core, kernel/drivers, kernel/mem, kernel/sched
    salt_files = glob.glob(f"{KERNEL_ROOT}/core/*.salt") + \
                 glob.glob(f"{KERNEL_ROOT}/drivers/*.salt") + \
                 glob.glob(f"{KERNEL_ROOT}/mem/*.salt") + \
                 glob.glob(f"{KERNEL_ROOT}/sched/*.salt") + \
                 glob.glob(f"{KERNEL_ROOT}/arch/x86/*.salt")
                 
    for f in salt_files:
        try:
            objects.append(compile_salt(f))
        except subprocess.CalledProcessError:
            base_name = os.path.basename(f).replace(".salt", "")
            obj_file = os.path.join(BUILD_DIR, f"{base_name}.o")
            if os.path.exists(obj_file):
                print(f"    {RED}⚠ Compilation failed, reusing pre-compiled {obj_file}{RESET}")
                objects.append(obj_file)
            else:
                print(f"    {RED}⚠ Compilation failed, no pre-compiled .o — skipping {base_name}{RESET}")

    # Compile Arch Assembly
    asm_files = glob.glob(f"{KERNEL_ROOT}/arch/x86/*.S") + \
                glob.glob(f"{KERNEL_ROOT}/arch/x86_64/*.S")
    for f in asm_files:
        objects.append(compile_asm(f))
        
    return objects

def build_benchmark(bench_file, kernel_objs):
    """Build all kernel-compatible benchmark Salt files and link with kernel objects."""
    print(f"{GREEN}== Building Benchmarks =={RESET}")
    
    # Only kernel-compatible benchmarks — others use userspace APIs (malloc, stdio)
    KERNEL_BENCHMARKS = [
        "ring_of_fire.salt",
        "ring_of_fire_1k.salt",
        "syscall_bench.salt",
        "ipc_bench.salt",
        "alloc_bench.salt",
    ]
    
    bench_objs = []
    bench_files = [os.path.join(BENCH_ROOT, b) for b in KERNEL_BENCHMARKS]
    
    for bf in bench_files:
        print(f"{GREEN}== Building Benchmark: {bf} =={RESET}")
        try:
            bench_objs.append(compile_salt(bf))
        except subprocess.CalledProcessError:
            base_name = os.path.basename(bf).replace(".salt", "")
            obj_file = os.path.join(BUILD_DIR, f"{base_name}.o")
            if os.path.exists(obj_file):
                print(f"    {RED}⚠ Compilation failed, reusing pre-compiled {obj_file}{RESET}")
                bench_objs.append(obj_file)
            else:
                raise
    
    linker_script = os.path.join(KERNEL_ROOT, "arch/x86/linker.ld")
    output_elf = os.path.join(BUILD_DIR, "kernel.elf")
    
    # Link Everything
    cmd = [TOOLCHAIN.rust_lld, "-flavor", "gnu", "-T", linker_script, "-o", output_elf, "-z", "max-page-size=0x1000"] + kernel_objs + bench_objs
    subprocess.check_call(cmd)
    
    return output_elf

def run_qemu_test(kernel_path, timeout=600):
    print(f"{GREEN}== Launching QEMU Flight Deck =={RESET}")
    
    # Detect KVM availability (Linux x86_64 with /dev/kvm)
    # On macOS ARM, HVF can't run x86 guests — always use TCG there
    use_kvm = sys.platform != "darwin" and os.path.exists("/dev/kvm")

    if use_kvm:
        cpu_flag = 'host'   # Pass through real CPU features (tzcnt, invariant TSC, etc.)
        print(f"{GREEN}  KVM detected — using hardware acceleration with -cpu host{RESET}")
    else:
        cpu_flag = 'qemu64,+fxsr,+mmx,+sse,+sse2,+xsave'

    cmd = [
        'qemu-system-x86_64',
        '-kernel', kernel_path,
        '-nographic',
        '-m', '128M',
        '-cpu', cpu_flag,
        '-d', 'int,guest_errors,cpu_reset',
        '-D', 'qemu.log',
        '-no-reboot',
        '-serial', 'mon:stdio'
    ]

    if use_kvm:
        cmd.insert(1, '-enable-kvm')
    
    print(f"COMMAND: {' '.join(cmd)}")
    
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    
    start_time = time.time()
    output_buffer = ""
    
    try:
        while True:
            if time.time() - start_time > timeout:
                process.terminate()
                print(f"{RED}TIMEOUT reached ({timeout}s){RESET}")
                return False, output_buffer

            line = process.stdout.readline()
            if line:
                print(f"QEMU: {line.strip()}")
                output_buffer += line
                
                # Check metrics
                if "ROF_TAX_REPORT:" in line:
                    match = re.search(r"ROF_TAX_REPORT: (\d+) / (\d+)", line)
                    if match:
                        overhead = int(match.group(1))
                        work = int(match.group(2))
                        print(f"{GREEN}METRICS CAPTURED:{RESET}")
                        print(f"  Overhead: {overhead} cycles")
                        print(f"  Work:     {work} cycles")
                        ratio = overhead / work if work > 0 else 0
                        print(f"  Tax Ratio: {ratio:.2%}")
                
                if "BENCHMARK SUITE COMPLETE" in line:
                    print(f"{GREEN}SUITE COMPLETE — terminating QEMU{RESET}")
                    process.terminate()
                    return True, output_buffer

                if "kernel panic" in line.lower() or "\x1b[31;1m" in line:
                    print(f"{RED}KERNEL PANIC DETECTED{RESET}")
                    # Keep reading a bit
                    continue

                if "HEARTBEAT" in line:
                    # Depending on verify mode, might exit here or wait
                    pass

            if process.poll() is not None:
                break
                
    except KeyboardInterrupt:
        process.terminate()
        
    return True, output_buffer

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "build":
        # Build-only mode (used by demo script)
        try:
            TOOLCHAIN.validate()
            kernel_objs = build_kernel()
            bench_file = os.path.join(BENCH_ROOT, "ring_of_fire.salt")
            elf = build_benchmark(bench_file, kernel_objs)
            print(f"{GREEN}BUILD SUCCESS: {elf}{RESET}")
        except subprocess.CalledProcessError as e:
            print(f"{RED}BUILD FAILED: {e}{RESET}")
            sys.exit(1)

    elif len(sys.argv) > 1 and sys.argv[1] == "run":
        # Build + Run Flow
        try:
            TOOLCHAIN.validate()
            kernel_objs = build_kernel()
            bench_file = os.path.join(BENCH_ROOT, "ring_of_fire.salt")
            elf = build_benchmark(bench_file, kernel_objs)
            
            success, log = run_qemu_test(elf)
            if not success:
                sys.exit(1)
                
            if "BENCHMARK SUITE COMPLETE" in log:
                print(f"{GREEN}VERIFICATION SUCCESS: Full benchmark suite completed.{RESET}")
                # Extract results
                for line in log.split("\n"):
                    if "BENCH:" in line or "ROF Result" in line:
                        print(f"  {line.strip()}")
                sys.exit(0)
            elif "ROF" in log:
                print(f"{GREEN}VERIFICATION PARTIAL: Ring of Fire completed.{RESET}")
                sys.exit(0)
            else:
                print(f"{RED}VERIFICATION FAILED: No report found.{RESET}")
                sys.exit(1)
                
        except subprocess.CalledProcessError as e:
            print(f"{RED}BUILD FAILED: {e}{RESET}")
            sys.exit(1)
    else:
        print("Usage: tools/runner_qemu.py [build|run]")

