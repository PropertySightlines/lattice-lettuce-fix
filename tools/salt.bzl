def _salt_kernel_binary_impl(ctx):
    src = ctx.file.src
    mlir_file = ctx.actions.declare_file(ctx.label.name + ".mlir")
    ll_file = ctx.actions.declare_file(ctx.label.name + ".ll")
    obj_file = ctx.actions.declare_file(ctx.label.name + ".o")

    # 1. Salt Front: src -> mlir
    # Command: salt-front < src > mlir
    ctx.actions.run_shell(
        tools = [ctx.executable._salt_front],
        inputs = [src],
        outputs = [mlir_file],
        command = "{salt_front} < {src} > {out}".format(
            salt_front = ctx.executable._salt_front.path,
            src = src.path,
            out = mlir_file.path,
        ),
        mnemonic = "SaltFront",
        progress_message = "Compiling {} to MLIR".format(src.short_path),
    )

    # 2. Salt Opt: mlir -> ll
    # Command: salt-opt --emit-llvm mlir_file --output ll_file
    ctx.actions.run(
        executable = ctx.executable._salt_opt,
        arguments = ["--emit-llvm", mlir_file.path, "--output", ll_file.path],
        inputs = [mlir_file],
        outputs = [ll_file],
        mnemonic = "SaltOpt",
        progress_message = "Lowering {} to LLVM IR".format(mlir_file.short_path),
    )

    # 3. LLC: ll -> o
    # Command: llc -O3 -filetype=obj ll_file -o obj_file
    # Hardcoding llc path to ensure consistent toolchain usage
    llc_path = "/opt/homebrew/Cellar/llvm/21.1.8/bin/llc"
    target = "x86_64-none-elf"
    ctx.actions.run_shell(
        inputs = [ll_file],
        outputs = [obj_file],
        command = "{llc} -O3 -filetype=obj {src} -o {out} -mtriple={target}".format(
            llc = llc_path,
            src = ll_file.path,
            out = obj_file.path,
            target = target,
        ),
        mnemonic = "LLC",
        progress_message = "Compiling {} to Object".format(ll_file.short_path),
    )

    return [DefaultInfo(files = depset([obj_file]))]

salt_kernel_binary = rule(
    implementation = _salt_kernel_binary_impl,
    attrs = {
        "src": attr.label(allow_single_file = [".salt"], mandatory = True),
        "_salt_front": attr.label(
            default = Label("//:salt_front_wrapper"),
            executable = True,
            cfg = "exec",
        ),
        "_salt_opt": attr.label(
            default = Label("//:salt_opt_wrapper"),
            executable = True,
            cfg = "exec",
        ),
    },
)
