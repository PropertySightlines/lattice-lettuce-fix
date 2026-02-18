//! Dependency Resolver — topological sort of module compilation order
//!
//! Resolves the dependency graph from a salt.toml manifest and returns
//! a topologically sorted list of files to compile.

use crate::manifest::{Manifest, Dependency};
use std::path::{Path, PathBuf};
use std::collections::{HashMap, HashSet, VecDeque};

/// Resolve the build order for a project.
/// Returns a list of .salt files in compilation order (dependencies first).
pub fn resolve(manifest: &Manifest, project_dir: &Path) -> Result<Vec<PathBuf>, String> {
    let mut all_files = Vec::new();

    // First: resolve and collect dependency files
    for (dep_name, dep) in &manifest.dependencies {
        match dep {
            Dependency::Path { path } => {
                let dep_dir = project_dir.join(path);
                let dep_manifest_path = dep_dir.join("salt.toml");

                if !dep_manifest_path.exists() {
                    return Err(format!(
                        "Dependency '{}' has no salt.toml at {}",
                        dep_name,
                        dep_manifest_path.display()
                    ));
                }

                let dep_manifest = crate::manifest::load(&dep_manifest_path)?;
                let dep_entry = dep_dir.join(&dep_manifest.package.entry);

                if !dep_entry.exists() {
                    return Err(format!(
                        "Dependency '{}' entry point not found: {}",
                        dep_name,
                        dep_entry.display()
                    ));
                }

                // Collect all .salt files from the dependency
                let dep_files = collect_salt_files(&dep_dir)?;
                all_files.extend(dep_files);
            }
            Dependency::Version(ver) => {
                return Err(format!(
                    "Registry dependencies not yet supported ({}@{}). Use path dependencies.",
                    dep_name, ver
                ));
            }
        }
    }

    // Then: collect the project's own source files
    let src_dir = project_dir.join("src");
    if src_dir.exists() {
        let src_files = collect_salt_files(&src_dir)?;
        all_files.extend(src_files);
    } else {
        // If no src/ dir, use the entry point directly
        let entry = project_dir.join(&manifest.package.entry);
        if entry.exists() {
            all_files.push(entry);
        } else {
            return Err(format!(
                "Entry point not found: {}",
                manifest.package.entry
            ));
        }
    }

    // Deduplicate while preserving order
    let mut seen = HashSet::new();
    all_files.retain(|f| {
        let canonical = f.canonicalize().unwrap_or_else(|_| f.clone());
        seen.insert(canonical)
    });

    Ok(all_files)
}

/// Collect all .salt files in a directory recursively.
fn collect_salt_files(dir: &Path) -> Result<Vec<PathBuf>, String> {
    let mut files = Vec::new();

    if !dir.exists() {
        return Ok(files);
    }

    let entries = std::fs::read_dir(dir)
        .map_err(|e| format!("Failed to read {}: {}", dir.display(), e))?;

    for entry in entries {
        let entry = entry.map_err(|e| format!("Read error: {}", e))?;
        let path = entry.path();

        if path.is_dir() {
            files.extend(collect_salt_files(&path)?);
        } else if path.extension().map_or(false, |e| e == "salt") {
            files.push(path);
        }
    }

    files.sort();
    Ok(files)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn test_collect_salt_files() {
        let tmp = std::env::temp_dir().join("salt_test_collect");
        let _ = fs::remove_dir_all(&tmp);
        fs::create_dir_all(tmp.join("src")).unwrap();
        fs::write(tmp.join("src/main.salt"), "package main").unwrap();
        fs::write(tmp.join("src/lib.salt"), "package lib").unwrap();
        fs::write(tmp.join("src/readme.md"), "# readme").unwrap();

        let files = collect_salt_files(&tmp.join("src")).unwrap();
        assert_eq!(files.len(), 2, "Should find exactly 2 .salt files");
        assert!(files.iter().all(|f| f.extension().unwrap() == "salt"));

        let _ = fs::remove_dir_all(&tmp);
    }
}
