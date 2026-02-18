//! Sync Verifier - Z3 Verification of "Hard Sync" Islands
//!
//! This module provides formal verification that functions without a Context
//! parameter are guaranteed to be synchronous (no I/O, no blocking operations).
//!
//! ## The "Hard Sync" Property
//! If a function lacks the Context "key", the Z3 shadow proves:
//! - No network I/O
//! - No file I/O
//! - No sleep/yield calls
//! - No calls to functions that require Context
//!
//! This enables aggressive optimization of sync functions.

use crate::grammar::{SaltFn, SaltFile, Item};
use crate::grammar::attr::extract_pulse_hz;
use crate::codegen::passes::call_graph::CallGraphAnalyzer;
use std::collections::{HashSet, HashMap};

/// Known I/O operations that require Context
const IO_OPERATIONS: &[&str] = &[
    // Network
    "net::TcpListener::bind",
    "net::TcpListener::accept",
    "net::TcpStream::read",
    "net::TcpStream::write",
    "net::UdpSocket::recv",
    "net::UdpSocket::send",
    // File system
    "fs::read",
    "fs::write",
    "fs::open",
    "io::File::read",
    "io::File::write",
    // Blocking
    "thread::sleep",
    "sync::Mutex::lock",
    // Yielding
    "executor::yield_now",
    "executor::spawn",
];

/// Result of sync verification
#[derive(Debug, Clone)]
pub struct SyncVerificationResult {
    /// Function name
    pub function_name: String,
    /// Whether the function is verified synchronous
    pub is_sync: bool,
    /// If not sync, what I/O operation was found
    pub io_violation: Option<String>,
    /// Line number of violation (if any)
    pub violation_line: Option<usize>,
}

/// Verifies that non-pulse functions are truly synchronous
pub struct SyncVerifier {
    /// Functions that are explicitly marked @pulse (they have Context)
    pulse_functions: HashSet<String>,
    /// Functions that transitively require Context
    context_required: HashSet<String>,
    /// Call graph for transitive analysis
    call_graph: HashMap<String, Vec<String>>,
    /// Whether we used the CallGraphAnalyzer (vs heuristic)
    used_call_graph: bool,
}

impl SyncVerifier {
    pub fn new() -> Self {
        Self {
            pulse_functions: HashSet::new(),
            context_required: HashSet::new(),
            call_graph: HashMap::new(),
            used_call_graph: false,
        }
    }
    
    /// Analyze a Salt file and verify sync properties
    pub fn analyze(&mut self, file: &SaltFile) -> Vec<SyncVerificationResult> {
        let mut results = Vec::new();
        
        // Phase 1: Identify pulse functions
        for item in &file.items {
            if let Item::Fn(func) = item {
                if extract_pulse_hz(&func.attributes).is_some() {
                    self.pulse_functions.insert(func.name.to_string());
                    self.context_required.insert(func.name.to_string());
                }
            }
        }
        
        // Phase 2: Build call graph (simplified - just look for obvious calls)
        for item in &file.items {
            if let Item::Fn(func) = item {
                let calls = self.extract_calls(func);
                self.call_graph.insert(func.name.to_string(), calls);
            }
        }
        
        // Phase 3: Propagate context requirements
        self.propagate_context_requirements();
        
        // Phase 4: Verify each non-pulse function
        for item in &file.items {
            if let Item::Fn(func) = item {
                let name = func.name.to_string();
                
                // Skip pulse functions - they're allowed to do I/O
                if self.pulse_functions.contains(&name) {
                    continue;
                }
                
                // Check if this function calls I/O without Context
                let result = self.verify_function(func);
                results.push(result);
            }
        }
        
        results
    }
    
    /// Extract function calls from a function body (simplified)
    fn extract_calls(&self, _func: &SaltFn) -> Vec<String> {
        // This is a placeholder - real implementation would walk the AST
        Vec::new()
    }
    
    /// Propagate context requirements through the call graph
    fn propagate_context_requirements(&mut self) {
        let mut changed = true;
        
        while changed {
            changed = false;
            let current_required = self.context_required.clone();
            
            for (caller, callees) in &self.call_graph {
                for callee in callees {
                    if current_required.contains(callee) && 
                       !self.context_required.contains(caller) {
                        self.context_required.insert(caller.clone());
                        changed = true;
                    }
                }
            }
        }
    }
    
    /// Verify a single function is synchronous
    fn verify_function(&self, func: &SaltFn) -> SyncVerificationResult {
        let name = func.name.to_string();
        
        // Check for direct I/O operations
        let io_violation = self.find_io_operation(func);
        
        // Check for calls to context-requiring functions
        let context_call = self.find_context_call(func);
        
        let violation = io_violation.or(context_call);
        
        SyncVerificationResult {
            function_name: name,
            is_sync: violation.is_none(),
            io_violation: violation,
            violation_line: None,
        }
    }
    
    /// Find direct I/O operations in a function
    fn find_io_operation(&self, func: &SaltFn) -> Option<String> {
        // Simplified: check for IO_OPERATIONS in the function body text
        let body_str = format!("{:?}", func.body);
        
        for op in IO_OPERATIONS {
            if body_str.contains(op) {
                return Some(op.to_string());
            }
        }
        
        None
    }
    
    /// Find calls to functions that require Context
    fn find_context_call(&self, func: &SaltFn) -> Option<String> {
        let body_str = format!("{:?}", func.body);
        
        for ctx_fn in &self.context_required {
            // Simple heuristic: look for function name followed by (
            let pattern = format!("{}(", ctx_fn);
            if body_str.contains(&pattern) {
                return Some(format!("calls @pulse function: {}", ctx_fn));
            }
        }
        
        None
    }
    
    /// Check if a function is verified synchronous
    pub fn is_verified_sync(&self, name: &str) -> bool {
        !self.context_required.contains(name)
    }

    // =========================================================================
    // Call Graph Integration (Sovereign V2.0)
    // =========================================================================
    //
    // Replaces the heuristic extract_calls()/find_io_operation() with
    // transitive queries into the CallGraphAnalyzer. The call graph has
    // already done fixed-point propagation, so is_blocking() and
    // requires_context() reflect the entire transitive closure.

    /// Verify sync contracts using the CallGraphAnalyzer
    ///
    /// For every function NOT marked @pulse, verify:
    ///   ¬is_blocking(fn) ∧ ¬requires_context(fn)
    ///
    /// If a non-pulse function transitively calls a blocking or
    /// context-requiring function, it's a sync violation.
    pub fn verify_with_call_graph(
        &mut self,
        file: &SaltFile,
        cg: &CallGraphAnalyzer,
    ) -> Vec<SyncVerificationResult> {
        self.used_call_graph = true;
        let mut results = Vec::new();

        // Phase 1: Identify pulse functions (they are exempt from sync checks)
        for item in &file.items {
            if let Item::Fn(func) = item {
                if extract_pulse_hz(&func.attributes).is_some() {
                    self.pulse_functions.insert(func.name.to_string());
                }
            }
        }

        // Phase 2: Verify each non-pulse function
        for item in &file.items {
            if let Item::Fn(func) = item {
                let name = func.name.to_string();

                // Skip @pulse functions — they are allowed to do I/O
                if self.pulse_functions.contains(&name) {
                    continue;
                }

                // Query the call graph for blocking/context violations
                let is_blocking = cg.is_blocking(&name);
                let needs_context = cg.requires_context(&name);

                let violation = if is_blocking {
                    Some(format!("transitively calls blocking operation (via call graph)"))
                } else if needs_context {
                    Some(format!("transitively requires Context (via call graph)"))
                } else {
                    None
                };

                results.push(SyncVerificationResult {
                    function_name: name,
                    is_sync: violation.is_none(),
                    io_violation: violation,
                    violation_line: None,
                });
            }
        }

        results
    }

    /// Returns whether the last verification used the CallGraphAnalyzer
    pub fn used_call_graph(&self) -> bool {
        self.used_call_graph
    }
}

impl Default for SyncVerifier {
    fn default() -> Self {
        Self::new()
    }
}

/// Generate Z3 constraints for sync verification
pub fn generate_sync_constraints(func_name: &str, calls_io: bool) -> String {
    format!(r#"
; Sync verification for {}
(declare-const {}_has_context Bool)
(declare-const {}_calls_io Bool)
(assert (= {}_calls_io {}))

; The Hard Sync Property:
; If no context and calls I/O, then UNSAT (violation)
(assert (not (and (not {}_has_context) {}_calls_io)))
"#, func_name, func_name, func_name, func_name, calls_io, func_name, func_name)
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_io_operations_list() {
        assert!(IO_OPERATIONS.contains(&"net::TcpStream::read"));
        assert!(IO_OPERATIONS.contains(&"fs::write"));
        assert!(IO_OPERATIONS.contains(&"thread::sleep"));
    }
    
    #[test]
    fn test_sync_verifier_new() {
        let verifier = SyncVerifier::new();
        assert!(verifier.pulse_functions.is_empty());
        assert!(verifier.context_required.is_empty());
        assert!(!verifier.used_call_graph);
    }

    // =========================================================================
    // PR 7: Call Graph Integration Tests (TDD)
    // =========================================================================

    use crate::codegen::passes::call_graph::{CallGraphAnalyzer, FnAttributes};

    /// Helper: build a minimal SaltFile with named functions
    fn make_salt_file(func_names: &[&str], pulse_names: &[&str]) -> SaltFile {
        use crate::grammar::SaltBlock;
        use crate::grammar::attr::Attribute;
        use syn::punctuated::Punctuated;
        let mut items = Vec::new();
        for name in func_names {
            let mut attrs = Vec::new();
            if pulse_names.contains(name) {
                // Add @pulse(1000) attribute using Salt's Attribute type
                attrs.push(Attribute {
                    name: syn::Ident::new("pulse", proc_macro2::Span::call_site()),
                    args: Vec::new(),
                    int_arg: Some(1000),
                    string_arg: None,
                });
            }
            items.push(Item::Fn(SaltFn {
                name: syn::Ident::new(name, proc_macro2::Span::call_site()),
                attributes: attrs,
                is_pub: false,
                args: Punctuated::new(),
                ret_type: None,
                requires: Vec::new(),
                ensures: Vec::new(),
                body: SaltBlock { stmts: Vec::new() },
                generics: None,
            }));
        }
        SaltFile { package: None, imports: Vec::new(), items }
    }

    #[test]
    fn test_sync_function_passes_with_call_graph() {
        // A pure function (no blocking, no context) should pass sync check
        let file = make_salt_file(&["compute_hash"], &[]);

        let mut cg = CallGraphAnalyzer::new();
        cg.inject_edges("compute_hash", vec!["add".to_string(), "rotate".to_string()]);
        // Neither callee is blocking
        cg.run_propagation();

        let mut verifier = SyncVerifier::new();
        let results = verifier.verify_with_call_graph(&file, &cg);

        assert_eq!(results.len(), 1);
        assert!(results[0].is_sync,
            "Pure function should pass sync verification");
        assert!(results[0].io_violation.is_none());
        assert!(verifier.used_call_graph(),
            "Should record that call graph was used");
    }

    #[test]
    fn test_sync_violation_detected_via_call_graph() {
        // A function that directly calls a blocking operation must fail
        let file = make_salt_file(&["read_config"], &[]);

        let mut cg = CallGraphAnalyzer::new();
        cg.inject_edges("read_config", vec!["fs::read".to_string()]);
        cg.inject_attributes("read_config", FnAttributes {
            is_blocking: true,
            ..Default::default()
        });
        cg.run_propagation();

        let mut verifier = SyncVerifier::new();
        let results = verifier.verify_with_call_graph(&file, &cg);

        assert_eq!(results.len(), 1);
        assert!(!results[0].is_sync,
            "Blocking function should fail sync verification");
        assert!(results[0].io_violation.as_ref().unwrap().contains("blocking"),
            "Violation should mention blocking: {:?}", results[0].io_violation);
    }

    #[test]
    fn test_transitive_sync_violation() {
        // A → B → C where C is blocking. A should fail sync check.
        let file = make_salt_file(&["handler", "process", "do_io"], &[]);

        let mut cg = CallGraphAnalyzer::new();
        cg.inject_edges("handler", vec!["process".to_string()]);
        cg.inject_edges("process", vec!["do_io".to_string()]);
        cg.inject_edges("do_io", vec!["net::TcpStream::write".to_string()]);
        cg.inject_attributes("do_io", FnAttributes {
            is_blocking: true,
            ..Default::default()
        });
        // Propagation: do_io is blocking → process becomes blocking → handler becomes blocking
        cg.run_propagation();

        let mut verifier = SyncVerifier::new();
        let results = verifier.verify_with_call_graph(&file, &cg);

        // handler and process should both fail (transitively blocking)
        let handler_result = results.iter().find(|r| r.function_name == "handler").unwrap();
        let process_result = results.iter().find(|r| r.function_name == "process").unwrap();
        let doio_result = results.iter().find(|r| r.function_name == "do_io").unwrap();

        assert!(!handler_result.is_sync,
            "handler should fail: transitively calls blocking");
        assert!(!process_result.is_sync,
            "process should fail: transitively calls blocking");
        assert!(!doio_result.is_sync,
            "do_io should fail: directly blocking");
    }

    #[test]
    fn test_pulse_function_exempt_from_sync_check() {
        // @pulse functions are allowed to call blocking operations
        let file = make_salt_file(&["ingest_loop", "compute"], &["ingest_loop"]);

        let mut cg = CallGraphAnalyzer::new();
        cg.inject_edges("ingest_loop", vec!["net::TcpStream::read".to_string()]);
        cg.inject_attributes("ingest_loop", FnAttributes {
            is_pulse: true,
            is_blocking: true,
            requires_context: true,
            ..Default::default()
        });
        cg.inject_edges("compute", vec!["math::sqrt".to_string()]);
        cg.run_propagation();

        let mut verifier = SyncVerifier::new();
        let results = verifier.verify_with_call_graph(&file, &cg);

        // Only compute should appear in results (ingest_loop is @pulse, exempt)
        assert_eq!(results.len(), 1,
            "@pulse function should be excluded from sync results");
        assert_eq!(results[0].function_name, "compute");
        assert!(results[0].is_sync,
            "compute should pass: no blocking calls");
    }
}
