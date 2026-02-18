//! Salt Build System — CLI Entry Point
//!
//! Usage:
//!   salt build       — Compile the project
//!   salt run         — Compile and run the project
//!   salt test        — Compile and run tests
//!   salt init <name> — Initialize a new project

mod manifest;
mod resolver;
mod compiler;

use clap::{Parser, Subcommand};
use std::path::PathBuf;

#[derive(Parser)]
#[command(name = "salt", version, about = "Salt build system")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Compile the project
    Build {
        /// Path to project directory (default: current directory)
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Build in release mode
        #[arg(long)]
        release: bool,
    },

    /// Compile and run the project
    Run {
        /// Path to project directory
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Arguments to pass to the program
        #[arg(trailing_var_arg = true)]
        args: Vec<String>,
    },

    /// Run tests
    Test {
        /// Path to project directory
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Filter tests by name
        #[arg(long)]
        filter: Option<String>,
    },

    /// Initialize a new Salt project
    Init {
        /// Project name
        name: String,
    },
}

fn main() {
    let cli = Cli::parse();

    let result = match cli.command {
        Commands::Build { path, release } => cmd_build(&path, release),
        Commands::Run { path, args } => cmd_run(&path, &args),
        Commands::Test { path, filter } => cmd_test(&path, filter.as_deref()),
        Commands::Init { name } => cmd_init(&name),
    };

    if let Err(e) = result {
        eprintln!("error: {}", e);
        std::process::exit(1);
    }
}

fn cmd_build(path: &PathBuf, release: bool) -> Result<(), String> {
    let manifest_path = path.join("salt.toml");
    let manifest = manifest::load(&manifest_path)?;

    println!("📦 Building {} v{}", manifest.package.name, manifest.package.version);

    // Resolve dependencies
    let build_order = resolver::resolve(&manifest, path)?;

    println!("   {} module(s) to compile", build_order.len());

    // Compile each module in order
    for module in &build_order {
        println!("   🔨 Compiling {}", module.display());
        compiler::compile_module(module, path, release)?;
    }

    // Link final binary
    let output = compiler::link(&manifest, path, release)?;
    println!("✅ Built: {}", output.display());

    Ok(())
}

fn cmd_run(path: &PathBuf, args: &[String]) -> Result<(), String> {
    cmd_build(path, false)?;

    let manifest_path = path.join("salt.toml");
    let manifest = manifest::load(&manifest_path)?;

    let binary = compiler::output_path(&manifest, path, false);
    println!("🚀 Running {}", binary.display());

    let status = std::process::Command::new(&binary)
        .args(args)
        .status()
        .map_err(|e| format!("Failed to run: {}", e))?;

    if !status.success() {
        return Err(format!("Process exited with status: {}", status));
    }

    Ok(())
}

fn cmd_test(path: &PathBuf, filter: Option<&str>) -> Result<(), String> {
    let manifest_path = path.join("salt.toml");
    let manifest = manifest::load(&manifest_path)?;

    // Find test files
    let test_dir = path.join("tests");
    if !test_dir.exists() {
        println!("No tests directory found");
        return Ok(());
    }

    let mut test_files: Vec<PathBuf> = std::fs::read_dir(&test_dir)
        .map_err(|e| format!("Failed to read tests/: {}", e))?
        .filter_map(|entry| entry.ok())
        .map(|entry| entry.path())
        .filter(|p| p.extension().map_or(false, |e| e == "salt"))
        .collect();

    if let Some(f) = filter {
        test_files.retain(|p| {
            p.file_stem()
                .map_or(false, |s| s.to_string_lossy().contains(f))
        });
    }

    test_files.sort();

    println!("🧪 Running {} test(s) for {}", test_files.len(), manifest.package.name);

    let mut passed = 0;
    let mut failed = 0;

    for test_file in &test_files {
        let name = test_file.file_stem().unwrap().to_string_lossy();
        print!("   {} ... ", name);

        match compiler::run_test(test_file, path) {
            Ok(_) => {
                println!("✅ PASS");
                passed += 1;
            }
            Err(e) => {
                println!("❌ FAIL: {}", e);
                failed += 1;
            }
        }
    }

    println!("\nResults: {} passed, {} failed", passed, failed);

    if failed > 0 {
        Err(format!("{} test(s) failed", failed))
    } else {
        Ok(())
    }
}

fn cmd_init(name: &str) -> Result<(), String> {
    let project_dir = PathBuf::from(name);

    if project_dir.exists() {
        return Err(format!("Directory '{}' already exists", name));
    }

    // Create project structure
    std::fs::create_dir_all(project_dir.join("src"))
        .map_err(|e| format!("Failed to create directory: {}", e))?;
    std::fs::create_dir_all(project_dir.join("tests"))
        .map_err(|e| format!("Failed to create tests/: {}", e))?;

    // Write salt.toml
    let manifest = format!(
        r#"[package]
name = "{}"
version = "0.1.0"
entry = "src/main.salt"
"#,
        name
    );
    std::fs::write(project_dir.join("salt.toml"), manifest)
        .map_err(|e| format!("Failed to write salt.toml: {}", e))?;

    // Write main.salt
    let main_salt = r#"package main

fn main() -> i32 {
    println("Hello, Salt!");
    return 0;
}
"#;
    std::fs::write(project_dir.join("src/main.salt"), main_salt)
        .map_err(|e| format!("Failed to write main.salt: {}", e))?;

    println!("✨ Created project '{}' at {}/", name, project_dir.display());
    println!("   salt.toml");
    println!("   src/main.salt");
    println!("   tests/");

    Ok(())
}
