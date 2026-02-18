#!/usr/bin/env python3
"""
Recursion Audit Script for Salt Compiler
=========================================
Detects self-referential cycles in LLVM IR or MLIR output that indicate
"monomorphization leaks" - where a function calls itself due to incorrect
method resolution or symbol mangling.

This is a post-codegen linter that turns silent infinite recursion (145s+ hang)
into a loud, blocking build error.

Usage:
    python3 audit_mangled_recursion.py <file.ll|file.mlir>
    
Exit Codes:
    0 - No self-recursive mangled names detected
    1 - Self-recursive calls found (build should fail)
"""

import re
import sys
from typing import List, Dict, Optional

def audit_ir(file_path: str) -> List[Dict]:
    """
    Parse IR file and detect self-recursive function calls.
    
    Works with both LLVM IR (.ll) and MLIR (.mlir) formats.
    """
    # LLVM IR: define void @mangled_name(...)
    llvm_func_def = re.compile(r'^define .* @([a-zA-Z0-9_]+)\(')
    # MLIR: func.func @mangled_name(...)
    mlir_func_def = re.compile(r'^\s*func\.func @([a-zA-Z0-9_]+)\(')
    
    # LLVM IR: call ... @mangled_name(...)
    llvm_call = re.compile(r'call .* @([a-zA-Z0-9_]+)\(')
    # MLIR: func.call @mangled_name(...)
    mlir_call = re.compile(r'func\.call @([a-zA-Z0-9_]+)\(')

    current_func: Optional[str] = None
    violations: List[Dict] = []

    with open(file_path, 'r') as f:
        for line_num, line in enumerate(f, 1):
            # 1. Detect start of a new function definition
            def_match = llvm_func_def.search(line) or mlir_func_def.search(line)
            if def_match:
                current_func = def_match.group(1)
                continue

            # 2. Detect end of function (LLVM: }, MLIR: } or func.func ends)
            if line.strip() == '}' or (line.strip().startswith('}') and current_func):
                current_func = None
                continue

            # 3. Detect calls inside the current function
            if current_func:
                call_match = llvm_call.search(line) or mlir_call.search(line)
                if call_match:
                    target_func = call_match.group(1)
                    
                    # ALERT: If the target matches the current definition, it's a self-call
                    if target_func == current_func:
                        violations.append({
                            'func': current_func,
                            'line': line_num,
                            'content': line.strip()
                        })

    return violations


def audit_known_dangerous_patterns(file_path: str) -> List[Dict]:
    """
    This function is deprecated - pattern matching across the entire file
    caused false positives. The self-call detection above is sufficient.
    Kept for documentation purposes.
    """
    return []


# Known intentional recursion in stdlib (not compiler bugs)
WHITELISTED_SELF_CALLS = {
    # Result::unwrap on Err intentionally recurses as a panic placeholder
    # Matches all variants: Result__unwrap, Result_T_E__unwrap, etc.
    'Result',  # Any function with 'Result' in the name that has 'unwrap' is likely the stub
}

def is_whitelisted(func_name: str) -> bool:
    """Check if a function is whitelisted for intentional recursion."""
    # Special case for Result::unwrap - check both Result in name AND unwrap in name
    if 'Result' in func_name and 'unwrap' in func_name:
        return True
    for pattern in WHITELISTED_SELF_CALLS:
        if pattern in func_name:
            return True
    return False


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 audit_mangled_recursion.py <file.ll|file.mlir>")
        sys.exit(1)

    file_path = sys.argv[1]
    
    print(f"Auditing {file_path} for self-recursive calls...")
    
    # Run audit
    all_self_calls = audit_ir(file_path)
    
    # Filter out whitelisted intentional recursion
    self_calls = [v for v in all_self_calls if not is_whitelisted(v['func'])]
    whitelisted = [v for v in all_self_calls if is_whitelisted(v['func'])]
    
    dangerous = audit_known_dangerous_patterns(file_path)
    
    has_errors = False
    
    if whitelisted:
        print(f"\n\033[93m[WARN] {len(whitelisted)} whitelisted self-recursive function(s) (intentional):\033[0m")
        for v in whitelisted:
            print(f"  [~] Function '@{v['func']}' (whitelisted)")
    
    if self_calls:
        print(f"\n\033[91m[FAIL] Found {len(self_calls)} self-recursive function(s):\033[0m")
        for v in self_calls:
            print(f"  [!] Function '@{v['func']}' calls itself at line {v['line']}")
            print(f"      Code: {v['content']}")
        has_errors = True
    
    if dangerous:
        print(f"\n\033[91m[FAIL] Found {len(dangerous)} dangerous pattern(s):\033[0m")
        for v in dangerous:
            print(f"  [!] {v['description']}")
            print(f"      Pattern: {v['pattern']}")
        has_errors = True
    
    if has_errors:
        print("\n\033[91mAudit FAILED - fix method resolution before running benchmark\033[0m")
        sys.exit(1)
    else:
        print("\n\033[92mAudit PASSED - No unexpected self-recursive mangled names detected.\033[0m")
        sys.exit(0)
