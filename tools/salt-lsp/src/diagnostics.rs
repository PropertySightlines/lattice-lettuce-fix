//! Salt LSP Diagnostics — Parse error detection
//!
//! Provides syntax-level diagnostics by scanning for common Salt errors.
//! In a full implementation, this would invoke salt-front's parser;
//! for now we do lightweight pattern-based checking.

use tower_lsp::lsp_types::*;

/// Diagnose a Salt source file and return LSP diagnostics.
pub fn diagnose(text: &str) -> Vec<Diagnostic> {
    let mut diags = Vec::new();

    for (line_idx, line) in text.lines().enumerate() {
        let trimmed = line.trim();

        // Skip empty lines and comments
        if trimmed.is_empty() || trimmed.starts_with("//") {
            continue;
        }

        // Check: `import` keyword should be `use`
        if trimmed.starts_with("import ") {
            diags.push(make_diagnostic(
                line_idx,
                0,
                6,
                "The `import` keyword is abolished in Salt. Use `use` instead.",
                DiagnosticSeverity::ERROR,
            ));
        }

        // Check: functions missing explicit return
        // Heuristic: if a line has `fn ` and `->` but the block doesn't end with `return`
        // This is a simplified check; full analysis requires AST traversal.

        // Check: NativePtr / NodePtr usage (abolished types)
        if trimmed.contains("NativePtr") {
            let col = line.find("NativePtr").unwrap_or(0);
            diags.push(make_diagnostic(
                line_idx,
                col,
                col + 9,
                "Legacy type `NativePtr` is abolished. Use `Ptr<T>` instead.",
                DiagnosticSeverity::ERROR,
            ));
        }
        if trimmed.contains("NodePtr") {
            let col = line.find("NodePtr").unwrap_or(0);
            diags.push(make_diagnostic(
                line_idx,
                col,
                col + 7,
                "Legacy type `NodePtr` is abolished. Use `Ptr<T>` instead.",
                DiagnosticSeverity::ERROR,
            ));
        }

        // Check: double underscore in identifiers (reserved for mangling)
        // Simple heuristic: look for __ in let/fn declarations
        if (trimmed.starts_with("let ") || trimmed.starts_with("fn ")) && trimmed.contains("__") {
            let col = line.find("__").unwrap_or(0);
            diags.push(make_diagnostic(
                line_idx,
                col,
                col + 2,
                "Identifiers cannot contain `__` (reserved for symbol mangling).",
                DiagnosticSeverity::WARNING,
            ));
        }

        // Check: unclosed string literals
        let in_comment = trimmed.starts_with("//");
        if !in_comment {
            let quote_count = trimmed.chars().filter(|c| *c == '"').count();
            // Skip f-strings which may have nested quotes
            if quote_count % 2 != 0 && !trimmed.contains("f\"") {
                diags.push(make_diagnostic(
                    line_idx,
                    0,
                    line.len(),
                    "Unclosed string literal.",
                    DiagnosticSeverity::ERROR,
                ));
            }
        }

        // Check: missing semicolons on let statements
        if trimmed.starts_with("let ") && !trimmed.ends_with('{') && !trimmed.ends_with(';') {
            diags.push(make_diagnostic(
                line_idx,
                line.len().saturating_sub(1),
                line.len(),
                "Missing semicolon after `let` statement.",
                DiagnosticSeverity::WARNING,
            ));
        }
    }

    diags
}

fn make_diagnostic(
    line: usize,
    start_col: usize,
    end_col: usize,
    message: &str,
    severity: DiagnosticSeverity,
) -> Diagnostic {
    Diagnostic {
        range: Range {
            start: Position {
                line: line as u32,
                character: start_col as u32,
            },
            end: Position {
                line: line as u32,
                character: end_col as u32,
            },
        },
        severity: Some(severity),
        code: None,
        code_description: None,
        source: Some("salt-lsp".to_string()),
        message: message.to_string(),
        related_information: None,
        tags: None,
        data: None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_import_keyword_error() {
        let diags = diagnose("import std.core.result.Result");
        assert_eq!(diags.len(), 1);
        assert!(diags[0].message.contains("abolished"));
    }

    #[test]
    fn test_native_ptr_error() {
        let diags = diagnose("let x: NativePtr = null;");
        assert!(diags.iter().any(|d| d.message.contains("NativePtr")));
    }

    #[test]
    fn test_clean_code_no_errors() {
        let code = r#"
package main

use std.core.result.Result

fn main() -> i32 {
    let x: i32 = 42;
    return 0;
}
"#;
        let diags = diagnose(code);
        assert!(diags.is_empty(), "Clean code should have no diagnostics, got: {:?}", diags);
    }

    #[test]
    fn test_double_underscore_warning() {
        let diags = diagnose("let my__var: i32 = 0;");
        assert!(diags.iter().any(|d| d.message.contains("__")));
    }
}
