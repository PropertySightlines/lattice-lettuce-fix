//! sp — Salt Packaging
//!
//! Two keystrokes. Zero friction.
//!
//! Usage:
//!   sp new <name>   — Create a new Salt project
//!   sp build        — Compile the project
//!   sp run          — Compile and run the project
//!   sp test         — Compile and run tests
//!   sp check        — Verify contracts without building
//!   sp clean        — Remove build artifacts
//!   sp add <dep>    — Add a dependency (future: registry)
//!   sp fetch        — Download dependencies without building

mod manifest;
mod resolver;
mod compiler;
mod cache;

use clap::{Parser, Subcommand};
use std::path::PathBuf;
use std::time::Instant;

#[derive(Parser)]
#[command(
    name = "sp",
    version = "0.1.0",
    about = "🧂 sp — Salt Packaging. Two keystrokes. Zero friction.",
    long_about = "The Salt package manager.\n\nBuilt on three pillars: Fast Enough, Supremely Ergonomic, Formally Verified."
)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Create a new Salt project
    New {
        /// Project name
        name: String,

        /// Use a library template instead of a binary
        #[arg(long)]
        lib: bool,
    },

    /// Compile the project
    Build {
        /// Path to project directory (default: current directory)
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Build in release mode (O3 + verification)
        #[arg(long)]
        release: bool,

        /// Build a specific package in a workspace
        #[arg(short, long)]
        package: Option<String>,
    },

    /// Compile and run the project
    Run {
        /// Path to project directory
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Build in release mode
        #[arg(long)]
        release: bool,

        /// Arguments to pass to the program
        #[arg(trailing_var_arg = true, allow_hyphen_values = true)]
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

    /// Verify contracts without building
    Check {
        /// Path to project directory
        #[arg(default_value = ".")]
        path: PathBuf,
    },

    /// Remove build artifacts
    Clean {
        /// Path to project directory
        #[arg(default_value = ".")]
        path: PathBuf,
    },

    /// Add a dependency
    Add {
        /// Dependency name (optionally with @version)
        dep: String,

        /// Path to project directory
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Add as a dev-dependency
        #[arg(long)]
        dev: bool,
    },

    /// Download dependencies without building
    Fetch {
        /// Path to project directory
        #[arg(default_value = ".")]
        path: PathBuf,
    },
}

fn main() {
    let cli = Cli::parse();

    let result = match cli.command {
        Commands::New { name, lib } => cmd_new(&name, lib),
        Commands::Build { path, release, package } => cmd_build(&path, release, package.as_deref()),
        Commands::Run { path, release, args } => cmd_run(&path, release, &args),
        Commands::Test { path, filter } => cmd_test(&path, filter.as_deref()),
        Commands::Check { path } => cmd_check(&path),
        Commands::Clean { path } => cmd_clean(&path),
        Commands::Add { dep, path, dev } => cmd_add(&dep, &path, dev),
        Commands::Fetch { path } => cmd_fetch(&path),
    };

    if let Err(e) = result {
        eprintln!("\x1b[1;31merror\x1b[0m: {}", e);
        std::process::exit(1);
    }
}

// ─── sp new ──────────────────────────────────────────────────────────────────

fn cmd_new(name: &str, lib: bool) -> Result<(), String> {
    let project_dir = PathBuf::from(name);

    if project_dir.exists() {
        return Err(format!("directory '{}' already exists", name));
    }

    // Extract just the project name from the path (e.g., "/tmp/hello" → "hello")
    let project_name = project_dir
        .file_name()
        .ok_or_else(|| "invalid project name".to_string())?
        .to_string_lossy()
        .to_string();

    // Create project structure
    std::fs::create_dir_all(project_dir.join("src"))
        .map_err(|e| format!("failed to create directory: {}", e))?;
    std::fs::create_dir_all(project_dir.join("tests"))
        .map_err(|e| format!("failed to create tests/: {}", e))?;

    // Write salt.toml
    let manifest = format!(
        r#"[package]
name = "{project_name}"
version = "0.1.0"
edition = "2026"
entry = "src/main.salt"
"#
    );
    std::fs::write(project_dir.join("salt.toml"), manifest)
        .map_err(|e| format!("failed to write salt.toml: {}", e))?;

    // Write entry point
    let source = if lib {
        format!(
            r#"package {project_name}

/// Add two numbers.
pub fn add(a: i32, b: i32) -> i32 {{
    return a + b;
}}
"#
        )
    } else {
        format!(
            r#"package main

fn main() -> i32 {{
    println("Hello from {project_name}!");
    return 0;
}}
"#
        )
    };

    let entry_path = if lib { "src/lib.salt" } else { "src/main.salt" };
    std::fs::write(project_dir.join(entry_path), source)
        .map_err(|e| format!("failed to write {}: {}", entry_path, e))?;

    // Write a starter test
    let test_source = if lib {
        format!(
            r#"package test

use {project_name}.add

fn main() -> i32 {{
    let result = add(2, 3);
    if result == 5 {{
        println("PASS: add(2, 3) == 5");
    }} else {{
        println("FAIL: add(2, 3) != 5");
        return 1;
    }}
    return 0;
}}
"#
        )
    } else {
        r#"package test

fn main() -> i32 {
    println("PASS: smoke test");
    return 0;
}
"#
        .to_string()
    };

    std::fs::write(project_dir.join("tests/test_smoke.salt"), test_source)
        .map_err(|e| format!("failed to write test: {}", e))?;

    // Write .gitignore
    let gitignore = "target/\n*.o\n*.ll\n*.mlir\n";
    std::fs::write(project_dir.join(".gitignore"), gitignore)
        .map_err(|e| format!("failed to write .gitignore: {}", e))?;

    println!("✨ Created project '{}'\n", project_name);
    println!("   {}/", name);
    println!("   ├── salt.toml");
    println!("   ├── src/");
    println!("   │   └── {}", if lib { "lib.salt" } else { "main.salt" });
    println!("   ├── tests/");
    println!("   │   └── test_smoke.salt");
    println!("   └── .gitignore");
    println!();
    if !lib {
        println!("   Run it: cd {} && sp run", name);
    }

    Ok(())
}

// ─── sp build ────────────────────────────────────────────────────────────────

fn cmd_build(path: &PathBuf, release: bool, _package: Option<&str>) -> Result<(), String> {
    let start = Instant::now();
    let manifest_path = path.join("salt.toml");
    let manifest = manifest::load(&manifest_path)?;

    let mode_str = if release { "release" } else { "debug" };
    println!(
        "📦 Building \x1b[1m{}\x1b[0m v{} [{}]",
        manifest.package.name, manifest.package.version, mode_str
    );

    // Resolve dependencies — collect search roots for the compiler
    let (build_order, search_roots) = resolver::resolve(&manifest, path)?;

    let dep_count = manifest.dependencies.len();
    if dep_count > 0 {
        println!("   {} dependency(ies) resolved", dep_count);
    }

    // Check cache
    let cache = cache::ArtifactCache::new()?;
    let cache_key = cache.compute_key(&manifest, path, release, &search_roots)?;

    if let Some(cached) = cache.lookup(&cache_key) {
        let elapsed = start.elapsed();
        println!(
            "⚡ \x1b[1;32mCached\x1b[0m {} ({}ms)",
            cached.display(),
            elapsed.as_millis()
        );
        return Ok(());
    }

    // Compile via salt-front with search roots
    println!("   🔨 Compiling {} module(s)...", build_order.len());
    let output = compiler::build(&manifest, path, release, &search_roots)?;

    // Store in cache
    if let Ok(ref out) = Ok::<_, String>(output.clone()) {
        let _ = cache.store(&cache_key, out);
    }

    let elapsed = start.elapsed();
    println!(
        "✅ Built \x1b[1m{}\x1b[0m in {:.1}s",
        output.display(),
        elapsed.as_secs_f64()
    );

    Ok(())
}

// ─── sp run ──────────────────────────────────────────────────────────────────

fn cmd_run(path: &PathBuf, release: bool, args: &[String]) -> Result<(), String> {
    cmd_build(path, release, None)?;

    let manifest_path = path.join("salt.toml");
    let manifest = manifest::load(&manifest_path)?;

    let binary = compiler::output_path(&manifest, path, release);
    println!("🧂 Running \x1b[1m{}\x1b[0m\n", manifest.package.name);

    let status = std::process::Command::new(&binary)
        .args(args)
        .env("DYLD_LIBRARY_PATH", "/opt/homebrew/lib")
        .status()
        .map_err(|e| format!("failed to run: {}", e))?;

    if !status.success() {
        return Err(format!("process exited with {}", status));
    }

    Ok(())
}

// ─── sp test ─────────────────────────────────────────────────────────────────

fn cmd_test(path: &PathBuf, filter: Option<&str>) -> Result<(), String> {
    let start = Instant::now();
    let manifest_path = path.join("salt.toml");
    let manifest = manifest::load(&manifest_path)?;

    // Find test files
    let test_dir = path.join("tests");
    if !test_dir.exists() {
        println!("No tests/ directory found");
        return Ok(());
    }

    let mut test_files: Vec<PathBuf> = std::fs::read_dir(&test_dir)
        .map_err(|e| format!("failed to read tests/: {}", e))?
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

    println!(
        "🧪 Running {} test(s) for \x1b[1m{}\x1b[0m\n",
        test_files.len(),
        manifest.package.name
    );

    let mut passed = 0;
    let mut failed = 0;

    for test_file in &test_files {
        let name = test_file.file_stem().unwrap().to_string_lossy();
        print!("   {} ... ", name);

        match compiler::run_test(test_file, path) {
            Ok(_) => {
                println!("\x1b[32m✓ pass\x1b[0m");
                passed += 1;
            }
            Err(e) => {
                println!("\x1b[31m✗ FAIL\x1b[0m");
                eprintln!("     {}", e);
                failed += 1;
            }
        }
    }

    let elapsed = start.elapsed();
    println!(
        "\n   Result: {} passed, {} failed ({:.1}s)",
        passed,
        failed,
        elapsed.as_secs_f64()
    );

    if failed > 0 {
        Err(format!("{} test(s) failed", failed))
    } else {
        Ok(())
    }
}

// ─── sp check ────────────────────────────────────────────────────────────────

fn cmd_check(path: &PathBuf) -> Result<(), String> {
    let start = Instant::now();
    let manifest_path = path.join("salt.toml");
    let manifest = manifest::load(&manifest_path)?;

    println!(
        "🔍 Checking \x1b[1m{}\x1b[0m v{}",
        manifest.package.name, manifest.package.version
    );

    // Resolve deps and compile with --verify flag
    let (_build_order, search_roots) = resolver::resolve(&manifest, path)?;
    compiler::check(&manifest, path, &search_roots)?;

    let elapsed = start.elapsed();
    println!(
        "✅ All contracts verified ({:.1}s)",
        elapsed.as_secs_f64()
    );

    Ok(())
}

// ─── sp clean ────────────────────────────────────────────────────────────────

fn cmd_clean(path: &PathBuf) -> Result<(), String> {
    let target_dir = path.join("target");

    if !target_dir.exists() {
        println!("   Nothing to clean");
        return Ok(());
    }

    let size = dir_size(&target_dir);
    std::fs::remove_dir_all(&target_dir)
        .map_err(|e| format!("failed to remove target/: {}", e))?;

    println!("🧹 Removed build artifacts ({:.1} MB)", size as f64 / 1_048_576.0);

    Ok(())
}

fn dir_size(path: &PathBuf) -> u64 {
    std::fs::read_dir(path)
        .map(|entries| {
            entries
                .filter_map(|e| e.ok())
                .map(|e| {
                    let p = e.path();
                    if p.is_dir() {
                        dir_size(&p)
                    } else {
                        e.metadata().map(|m| m.len()).unwrap_or(0)
                    }
                })
                .sum()
        })
        .unwrap_or(0)
}

// ─── sp add ──────────────────────────────────────────────────────────────────

fn cmd_add(dep: &str, path: &PathBuf, dev: bool) -> Result<(), String> {
    let manifest_path = path.join("salt.toml");
    if !manifest_path.exists() {
        return Err("no salt.toml found. Run `sp new <name>` to create a project.".into());
    }

    // Parse dep@version syntax
    let (name, version) = if let Some(at) = dep.find('@') {
        (&dep[..at], &dep[at + 1..])
    } else {
        (dep, "*")
    };

    // Read and modify manifest using toml_edit for non-destructive editing
    let content = std::fs::read_to_string(&manifest_path)
        .map_err(|e| format!("failed to read salt.toml: {}", e))?;

    let mut doc = content
        .parse::<toml_edit::DocumentMut>()
        .map_err(|e| format!("failed to parse salt.toml: {}", e))?;

    let table_name = if dev { "dev-dependencies" } else { "dependencies" };

    // Ensure the table exists
    if doc.get(table_name).is_none() {
        doc[table_name] = toml_edit::Item::Table(toml_edit::Table::new());
    }

    // Add the dependency
    doc[table_name][name] = toml_edit::value(version);

    std::fs::write(&manifest_path, doc.to_string())
        .map_err(|e| format!("failed to write salt.toml: {}", e))?;

    let section = if dev { "dev-dependencies" } else { "dependencies" };
    println!(
        "✨ Added \x1b[1m{}\x1b[0m {} to [{}]",
        name, version, section
    );

    Ok(())
}

// ─── sp fetch ────────────────────────────────────────────────────────────────

fn cmd_fetch(path: &PathBuf) -> Result<(), String> {
    let manifest_path = path.join("salt.toml");
    let manifest = manifest::load(&manifest_path)?;

    let dep_count = manifest.dependencies.len();
    if dep_count == 0 {
        println!("   No dependencies to fetch");
        return Ok(());
    }

    let (_build_order, _search_roots) = resolver::resolve(&manifest, path)?;

    println!(
        "📥 Fetched {} package(s)",
        dep_count
    );

    Ok(())
}
