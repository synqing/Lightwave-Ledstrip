#!/usr/bin/env python3
"""
Capture baseline metrics for WebServer refactoring.

Captures:
- LOC (lines of code) for WebServer.cpp/h
- Cyclomatic complexity estimates
- File structure metrics
"""

import os
import re
import sys
from pathlib import Path

def count_lines(filepath):
    """Count total lines and non-empty lines."""
    with open(filepath, 'r') as f:
        lines = f.readlines()
        total = len(lines)
        non_empty = len([l for l in lines if l.strip()])
        return total, non_empty

def estimate_complexity(filepath):
    """Estimate cyclomatic complexity by counting control flow statements."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Count control flow keywords
    if_count = len(re.findall(r'\bif\s*\(', content))
    else_count = len(re.findall(r'\belse\b', content))
    for_count = len(re.findall(r'\bfor\s*\(', content))
    while_count = len(re.findall(r'\bwhile\s*\(', content))
    switch_count = len(re.findall(r'\bswitch\s*\(', content))
    case_count = len(re.findall(r'\bcase\s+', content))
    catch_count = len(re.findall(r'\bcatch\s*\(', content))
    
    # Cyclomatic complexity = 1 + decisions
    decisions = if_count + for_count + while_count + switch_count + catch_count
    complexity = 1 + decisions
    
    return {
        'complexity': complexity,
        'if': if_count,
        'else': else_count,
        'for': for_count,
        'while': while_count,
        'switch': switch_count,
        'case': case_count,
        'catch': catch_count,
        'decisions': decisions
    }

def count_functions(filepath):
    """Count function definitions."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Match function definitions (C++ style)
    func_pattern = r'\w+\s+\w+::\w+\s*\([^)]*\)\s*\{'
    func_count = len(re.findall(func_pattern, content))
    
    # Also count standalone functions
    standalone_pattern = r'(?:^|\n)\s*(?:static\s+)?(?:inline\s+)?(?:void|bool|int|uint\d+_t|String|float|double|\w+)\s+\w+\s*\([^)]*\)\s*\{'
    standalone_count = len(re.findall(standalone_pattern, content, re.MULTILINE))
    
    return func_count + standalone_count

def analyze_webserver():
    """Analyze WebServer files."""
    base_path = Path(__file__).parent.parent
    cpp_file = base_path / 'v2/src/network/WebServer.cpp'
    h_file = base_path / 'v2/src/network/WebServer.h'
    
    results = {}
    
    for filepath, name in [(cpp_file, 'WebServer.cpp'), (h_file, 'WebServer.h')]:
        if not filepath.exists():
            print(f"Warning: {filepath} not found", file=sys.stderr)
            continue
        
        total_lines, non_empty = count_lines(filepath)
        complexity = estimate_complexity(filepath)
        func_count = count_functions(filepath)
        
        results[name] = {
            'total_lines': total_lines,
            'non_empty_lines': non_empty,
            'complexity': complexity,
            'function_count': func_count,
            'file_size_bytes': filepath.stat().st_size
        }
    
    return results

def main():
    """Main entry point."""
    results = analyze_webserver()
    
    print("=== WebServer Baseline Metrics ===")
    print()
    
    total_loc = 0
    total_complexity = 0
    total_functions = 0
    
    for name, metrics in results.items():
        print(f"{name}:")
        print(f"  Total lines: {metrics['total_lines']}")
        print(f"  Non-empty lines: {metrics['non_empty_lines']}")
        print(f"  File size: {metrics['file_size_bytes']} bytes")
        print(f"  Functions: {metrics['function_count']}")
        print(f"  Cyclomatic complexity: {metrics['complexity']['complexity']}")
        print(f"    - if statements: {metrics['complexity']['if']}")
        print(f"    - for loops: {metrics['complexity']['for']}")
        print(f"    - while loops: {metrics['complexity']['while']}")
        print(f"    - switch statements: {metrics['complexity']['switch']}")
        print()
        
        total_loc += metrics['total_lines']
        total_complexity += metrics['complexity']['complexity']
        total_functions += metrics['function_count']
    
    print("Totals:")
    print(f"  Combined LOC: {total_loc}")
    print(f"  Combined complexity: {total_complexity}")
    print(f"  Combined functions: {total_functions}")
    print()
    
    # Output JSON for programmatic use
    import json
    output = {
        'baseline': results,
        'totals': {
            'loc': total_loc,
            'complexity': total_complexity,
            'functions': total_functions
        }
    }
    
    output_file = Path(__file__).parent.parent / 'reports' / 'webserver_baseline_metrics.json'
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w') as f:
        json.dump(output, f, indent=2)
    
    print(f"JSON output saved to: {output_file}")
    return 0

if __name__ == '__main__':
    sys.exit(main())

