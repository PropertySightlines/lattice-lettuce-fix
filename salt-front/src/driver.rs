// =============================================================================
// [SOVEREIGN V2.0] Iron Driver — MLIR → Native Binary Pipeline
//
// Orchestrates the 4-step LLVM toolchain to produce Sovereign binaries:
//   1. mlir-opt:       MLIR → LLVM dialect
//   2. mlir-translate: LLVM dialect → LLVM IR (.ll)
//   3. llc:            LLVM IR → Object code (.o) with x19 reservation
//   4. clang:          Link with sovereign_rt.o → Mach-O/ELF binary
//
// The driver is testable via dry-run: `build_pipeline()` returns the steps
// without executing them, so TDD tests can verify flag correctness.
// =============================================================================

use std::path::PathBuf;

/// Paths to the LLVM toolchain binaries.
#[derive(Debug, Clone)]
pub struct ToolchainPaths {
    pub mlir_opt: PathBuf,
    pub mlir_translate: PathBuf,
    pub llc: PathBuf,
    pub clang: PathBuf,
}

impl Default for ToolchainPaths {
    fn default() -> Self {
        let base = PathBuf::from("/opt/homebrew/opt/llvm/bin");
        Self {
            mlir_opt: base.join("mlir-opt"),
            mlir_translate: base.join("mlir-translate"),
            llc: base.join("llc"),
            clang: base.join("clang"),
        }
    }
}

/// A single step in the MLIR → binary pipeline.
#[derive(Debug, Clone)]
pub struct PipelineStep {
    pub name: &'static str,
    pub tool: PathBuf,
    pub args: Vec<String>,
    pub input: PathBuf,
    pub output: PathBuf,
}

impl PipelineStep {
    /// Check if this step's args contain a specific flag.
    pub fn has_flag(&self, flag: &str) -> bool {
        self.args.iter().any(|a| a.contains(flag))
    }
}

/// Target platform for the produced binary.
#[derive(Debug, Clone, Copy)]
pub enum DriverTarget {
    DarwinArm64,
    LinuxArm64,
}

impl Default for DriverTarget {
    fn default() -> Self {
        if cfg!(target_os = "macos") {
            DriverTarget::DarwinArm64
        } else {
            DriverTarget::LinuxArm64
        }
    }
}

/// The Iron Driver: orchestrates MLIR → native binary production.
#[derive(Debug, Clone)]
pub struct SaltDriver {
    pub target: DriverTarget,
    pub build_dir: PathBuf,
    pub toolchain: ToolchainPaths,
    pub runtime_obj: PathBuf,
}

impl SaltDriver {
    pub fn new(build_dir: PathBuf) -> Self {
        Self {
            target: DriverTarget::default(),
            build_dir: build_dir.clone(),
            toolchain: ToolchainPaths::default(),
            runtime_obj: build_dir.join("sovereign_rt.o"),
        }
    }

    pub fn with_target(mut self, target: DriverTarget) -> Self {
        self.target = target;
        self
    }

    pub fn with_toolchain(mut self, toolchain: ToolchainPaths) -> Self {
        self.toolchain = toolchain;
        self
    }

    pub fn with_runtime(mut self, runtime_obj: PathBuf) -> Self {
        self.runtime_obj = runtime_obj;
        self
    }

    /// Build the pipeline steps without executing them (dry-run for TDD).
    /// Returns the 4 steps: mlir-opt → mlir-translate → llc → link.
    pub fn build_pipeline(&self, output_name: &str) -> Vec<PipelineStep> {
        let mlir_file = self.build_dir.join(format!("{}.mlir", output_name));
        let _scf_file = self.build_dir.join(format!("{}_scf.mlir", output_name));
        let opt_file = self.build_dir.join(format!("{}_opt.mlir", output_name));
        let ll_file = self.build_dir.join(format!("{}.ll", output_name));
        let obj_file = self.build_dir.join(format!("{}.o", output_name));
        let bin_file = self.build_dir.join(output_name);

        let target_triple = match self.target {
            DriverTarget::DarwinArm64 => "arm64-apple-macosx14.0.0",
            DriverTarget::LinuxArm64 => "aarch64-unknown-linux-gnu",
        };

        vec![
            // Step 1: mlir-opt — lower to LLVM dialect
            PipelineStep {
                name: "mlir-opt",
                tool: self.toolchain.mlir_opt.clone(),
                args: vec![
                    "--convert-linalg-to-loops".into(),
                    "--cse".into(),
                    "--lower-affine".into(),
                    "--convert-vector-to-scf".into(),
                    "--convert-scf-to-cf".into(),
                    "--convert-cf-to-llvm".into(),
                    "--convert-vector-to-llvm".into(),
                    "--convert-math-to-llvm".into(),
                    "--convert-arith-to-llvm".into(),
                    "--finalize-memref-to-llvm".into(),
                    "--convert-func-to-llvm".into(),
                    "--reconcile-unrealized-casts".into(),
                ],
                input: mlir_file,
                output: opt_file.clone(),
            },
            // Step 2: mlir-translate — MLIR → LLVM IR
            PipelineStep {
                name: "mlir-translate",
                tool: self.toolchain.mlir_translate.clone(),
                args: vec!["--mlir-to-llvmir".into()],
                input: opt_file,
                output: ll_file.clone(),
            },
            // Step 3: llc — LLVM IR → object code with Sovereign flags
            PipelineStep {
                name: "llc",
                tool: self.toolchain.llc.clone(),
                args: vec![
                    "-O3".into(),
                    format!("-mtriple={}", target_triple),
                    "-mcpu=apple-m4".into(),
                    "-mattr=+lse".into(),
                    "-reserved-reg=aarch64:x19".into(),
                    "--frame-pointer=none".into(),
                    "-filetype=obj".into(),
                ],
                input: ll_file,
                output: obj_file.clone(),
            },
            // Step 4: clang — link with sovereign_rt.o, no libc
            PipelineStep {
                name: "link",
                tool: self.toolchain.clang.clone(),
                args: {
                    let mut args = vec![
                        "-nostdlib".into(),
                        "-static".into(),
                        "-O3".into(),
                    ];
                    args.push(self.runtime_obj.to_string_lossy().into_owned());
                    args
                },
                input: obj_file,
                output: bin_file,
            },
        ]
    }

    /// Execute the full pipeline: write MLIR → run steps → produce binary.
    pub fn compile(&self, mlir_source: &str, output_name: &str) -> Result<PathBuf, String> {
        let mlir_path = self.build_dir.join(format!("{}.mlir", output_name));

        // Ensure build dir exists
        std::fs::create_dir_all(&self.build_dir)
            .map_err(|e| format!("Failed to create build dir: {}", e))?;

        // Write MLIR source
        std::fs::write(&mlir_path, mlir_source)
            .map_err(|e| format!("Failed to write MLIR: {}", e))?;

        let steps = self.build_pipeline(output_name);

        for step in &steps {
            let mut cmd = std::process::Command::new(&step.tool);
            cmd.args(&step.args);

            // For mlir-opt and mlir-translate: input via arg, output via -o
            match step.name {
                "mlir-opt" => {
                    cmd.arg(step.input.to_str().unwrap());
                    cmd.arg("-o");
                    cmd.arg(step.output.to_str().unwrap());
                }
                "mlir-translate" => {
                    cmd.arg(step.input.to_str().unwrap());
                    cmd.arg("-o");
                    cmd.arg(step.output.to_str().unwrap());
                }
                "llc" => {
                    cmd.arg(step.input.to_str().unwrap());
                    cmd.arg("-o");
                    cmd.arg(step.output.to_str().unwrap());
                }
                "link" => {
                    cmd.arg(step.input.to_str().unwrap());
                    cmd.arg("-o");
                    cmd.arg(step.output.to_str().unwrap());
                }
                _ => {}
            }

            let status = cmd.status()
                .map_err(|e| format!("{} failed to execute: {}", step.name, e))?;

            if !status.success() {
                return Err(format!("{} failed with exit code: {:?}", step.name, status.code()));
            }
        }

        let output = steps.last().unwrap().output.clone();
        Ok(output)
    }
}

// =============================================================================
// TESTS
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    fn test_driver() -> SaltDriver {
        SaltDriver::new(PathBuf::from("/tmp/salt-build"))
    }

    #[test]
    fn test_pipeline_has_4_steps() {
        let driver = test_driver();
        let steps = driver.build_pipeline("echo_test");

        assert_eq!(steps.len(), 4,
            "Pipeline must have exactly 4 steps: mlir-opt, mlir-translate, llc, link");
        assert_eq!(steps[0].name, "mlir-opt");
        assert_eq!(steps[1].name, "mlir-translate");
        assert_eq!(steps[2].name, "llc");
        assert_eq!(steps[3].name, "link");
    }

    #[test]
    fn test_llc_step_reserves_x19() {
        let driver = test_driver();
        let steps = driver.build_pipeline("echo_test");
        let llc = &steps[2];

        assert!(llc.has_flag("-reserved-reg=aarch64:x19"),
            "llc step MUST reserve x19 — Sovereign deadline register");
    }

    #[test]
    fn test_llc_step_enables_lse() {
        let driver = test_driver();
        let steps = driver.build_pipeline("echo_test");
        let llc = &steps[2];

        assert!(llc.has_flag("+lse"),
            "llc step MUST enable LSE atomics for M4 CAS/LDADD");
    }

    #[test]
    fn test_link_step_uses_nostdlib() {
        let driver = test_driver();
        let steps = driver.build_pipeline("echo_test");
        let link = &steps[3];

        assert!(link.has_flag("-nostdlib"),
            "Link step MUST use -nostdlib to eliminate C runtime tax");
    }

    #[test]
    fn test_link_step_includes_runtime() {
        let driver = test_driver();
        let steps = driver.build_pipeline("echo_test");
        let link = &steps[3];

        assert!(link.has_flag("sovereign_rt.o"),
            "Link step MUST include sovereign_rt.o runtime object");
    }

    #[test]
    fn test_toolchain_paths_default() {
        let paths = ToolchainPaths::default();

        assert!(paths.mlir_opt.to_str().unwrap().contains("/opt/homebrew/opt/llvm/bin/"),
            "Default mlir-opt path must be in /opt/homebrew/opt/llvm/bin/");
        assert!(paths.llc.to_str().unwrap().contains("llc"),
            "Default llc path must contain 'llc'");
        assert!(paths.clang.to_str().unwrap().contains("clang"),
            "Default clang path must contain 'clang'");
    }
}
