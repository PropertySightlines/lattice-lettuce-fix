workspace(name = "lattice")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# 1. Rules Rust (For salt-front)
http_archive(
    name = "rules_rust",
    urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.38.0/rules_rust-v0.38.0.tar.gz"],
    sha256 = "6b348c196561e30279c9314c9523f392233c489708940c4974f074d284305809", # Verify SHA
)

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")
rules_rust_dependencies()
rust_register_toolchains()

# 2. Bazel Skylib (Utility library required by many rules)
http_archive(
    name = "bazel_skylib",
    urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-1.5.0.tar.gz"],
    sha256 = "cd55a062e763b9349921f0f5db8c3933288dc8ba4f76dd9416aac68ace54cb94",
)
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

# 3. LLVM Toolchain (For salt-opt & Kernel Linking)
# We will configure a local LLVM toolchain for now to use your existing install
new_local_repository(
    name = "llvm_local",
    path = "/opt/homebrew/opt/llvm",  # Your Mac path
    build_file_content = """
package(default_visibility = ["//visibility:public"])
cc_library(
    name = "mlir",
    srcs = glob(["lib/libMLIR*.a"]),
    hdrs = glob(["include/**/*"]),
    includes = ["include"],
)
""",
)
