package(default_visibility = ["//visibility:public"])
load("//tools:salt.bzl", "salt_kernel_binary")

# 1. Wrapper for the Rust Frontend
# We treat your existing release binary as a "source tool" for Bazel
sh_binary(
    name = "salt_front_wrapper",
    srcs = ["salt-front/target/release/salt-front"],
)

# 2. Wrapper for the C++ Backend
sh_binary(
    name = "salt_opt_wrapper",
    srcs = ["salt/build/salt-opt"],
)

# 3. The "Hello World" of your OS
# This rule tests if the entire chain (Salt -> MLIR -> Object) works inside Bazel
salt_kernel_binary(
    name = "test_compile",
    src = "test.salt",
)
