#!/usr/bin/env python3
"""
Semantic Compression for Context Pack

Compresses documentation and code files by removing verbosity while
preserving essential semantic information (signatures, docstrings).

Usage:
    from semantic_compress import SemanticCompressor
    compressor = SemanticCompressor()
    compressed = compressor.compress(code_content, level="light")
"""

from pathlib import Path
from typing import Optional
import re


class CompressionLevel:
    """Compression levels."""
    NONE = "none"          # No compression
    LIGHT = "light"        # Strip comments, preserve structure
    AGGRESSIVE = "aggressive"  # Signature-only mode (strip function bodies)


class SemanticCompressor:
    """
    Compresses code and documentation while preserving semantic meaning.
    
    Compression modes:
    - NONE: No compression (pass-through)
    - LIGHT: Strip comments, normalise whitespace, preserve structure
    - AGGRESSIVE: Signature-only mode (function/class signatures + docstrings, strip bodies)
    """
    
    def __init__(self):
        """Initialize semantic compressor."""
        pass
    
    def compress(self, content: str, level: str = CompressionLevel.LIGHT) -> str:
        """
        Compress content based on compression level.
        
        Args:
            content: Content to compress
            level: Compression level ("none", "light", "aggressive")
        
        Returns: Compressed content
        """
        if level == CompressionLevel.NONE:
            return content
        elif level == CompressionLevel.LIGHT:
            return self._compress_light(content)
        elif level == CompressionLevel.AGGRESSIVE:
            return self._compress_aggressive(content)
        else:
            # Default to light
            return self._compress_light(content)
    
    def _compress_light(self, content: str) -> str:
        """
        Light compression: strip comments, normalise whitespace.
        
        Preserves:
        - Function/class signatures
        - Function bodies
        - Docstrings
        - Structure
        
        Removes:
        - Single-line comments (// ...)
        - Multi-line comments (/* ... */)
        - Trailing whitespace
        - Excessive blank lines (> 2 consecutive)
        """
        lines = content.split("\n")
        compressed = []
        prev_empty = False
        empty_count = 0
        
        in_multiline_comment = False
        
        for line in lines:
            stripped = line.rstrip()
            
            # Skip empty lines (normalise to max 1 consecutive)
            if not stripped:
                empty_count += 1
                if empty_count <= 1:
                    compressed.append("")
                prev_empty = True
                continue
            
            empty_count = 0
            prev_empty = False
            
            # Handle multi-line comments (C/C++)
            if "/*" in stripped:
                in_multiline_comment = True
                # Include comment start if it's not standalone
                if not stripped.strip().startswith("/*"):
                    compressed.append(stripped.split("/*")[0].rstrip())
            elif "*/" in stripped and in_multiline_comment:
                in_multiline_comment = False
                # Include comment end if it's not standalone
                if not stripped.strip().endswith("*/"):
                    compressed.append(stripped.split("*/")[1].lstrip())
                continue
            elif in_multiline_comment:
                # Skip lines inside multi-line comment
                continue
            
            # Strip single-line comments (// ...)
            if "//" in stripped:
                # Check if it's not inside a string literal
                parts = stripped.split("//")
                code_part = parts[0]
                # Simple check: if code_part has unmatched quotes, keep the comment
                if code_part.count('"') % 2 == 0 and code_part.count("'") % 2 == 0:
                    stripped = code_part.rstrip()
                    if not stripped:
                        continue  # Skip pure comment lines
            
            compressed.append(stripped)
        
        # Remove trailing blank lines
        while compressed and not compressed[-1]:
            compressed.pop()
        
        return "\n".join(compressed)
    
    def _compress_aggressive(self, content: str) -> str:
        """
        Aggressive compression: signature-only mode.
        
        Preserves:
        - Class/struct/enum definitions (signatures only)
        - Function declarations (signatures + docstrings)
        - Interface method signatures
        - #define constants
        - #include directives
        
        Removes:
        - Function body implementations (except simple inline functions)
        - Class member implementations (except inline)
        - Detailed comments (keeps brief docstrings)
        """
        lines = content.split("\n")
        compressed = []
        
        in_function = False
        in_class = False
        brace_depth = 0
        function_start_line = None
        last_docstring = []
        
        i = 0
        while i < len(lines):
            line = lines[i]
            stripped = line.strip()
            
            # Track brace depth for function/class bodies
            brace_depth += line.count("{") - line.count("}")
            
            # Capture docstrings (brief ones)
            if stripped.startswith("///") or stripped.startswith("/**"):
                doc_lines = []
                doc_i = i
                while doc_i < len(lines) and (lines[doc_i].strip().startswith("///") or 
                                             lines[doc_i].strip().startswith("/**") or
                                             lines[doc_i].strip().startswith("*") or
                                             lines[doc_i].strip() == ""):
                    doc_lines.append(lines[doc_i].strip())
                    doc_i += 1
                # Keep only brief docstrings (≤ 3 lines)
                if len(doc_lines) <= 3:
                    last_docstring = doc_lines
                i = doc_i - 1
                i += 1
                continue
            
            # Include docstrings before functions/classes
            if last_docstring:
                compressed.extend(last_docstring)
                last_docstring = []
            
            # Include #include directives
            if stripped.startswith("#include"):
                compressed.append(line.rstrip())
                i += 1
                continue
            
            # Include #define directives
            if stripped.startswith("#define"):
                compressed.append(line.rstrip())
                i += 1
                continue
            
            # Detect function/class signatures
            is_signature = False
            
            # Function signature patterns
            if re.match(r'^(virtual\s+)?[a-zA-Z_][a-zA-Z0-9_<>*&:]*\s+\w+\s*\([^)]*\)', stripped):
                is_signature = True
            
            # Class/struct signature patterns
            if re.match(r'^(class|struct|enum|enum class)\s+\w+', stripped):
                is_signature = True
            
            # Include signature lines
            if is_signature:
                compressed.append(line.rstrip())
                
                # If opening brace on same line, check depth
                if "{" in line:
                    brace_depth = 1
                    # Find matching closing brace (skip body)
                    body_start_i = i
                    i += 1
                    
                    while i < len(lines) and brace_depth > 0:
                        brace_depth += lines[i].count("{") - lines[i].count("}")
                        if brace_depth == 0:
                            # Include closing brace line if it has content
                            if lines[i].strip() != "}" and ";" in lines[i]:
                                compressed.append(lines[i].rstrip())
                            break
                        i += 1
                else:
                    i += 1
                continue
            
            # Include simple inline functions (single line)
            if "inline" in stripped and "{" in stripped and "}" in stripped:
                compressed.append(line.rstrip())
                i += 1
                continue
            
            # Skip function/class body content (when inside braces)
            if brace_depth > 0:
                i += 1
                continue
            
            # Include non-body lines (outside functions/classes)
            if not stripped.startswith("//") or not stripped.startswith("/*"):
                compressed.append(line.rstrip())
            
            i += 1
        
        return "\n".join(compressed)
    
    def compress_file(self, file_path: Path, level: str = CompressionLevel.LIGHT, 
                     output_path: Optional[Path] = None) -> str:
        """
        Compress a file and optionally write to output.
        
        Args:
            file_path: Path to input file
            level: Compression level
            output_path: Optional output path (if None, returns compressed content only)
        
        Returns: Compressed content
        """
        try:
            content = file_path.read_text(encoding="utf-8", errors="ignore")
        except IOError:
            return ""
        
        compressed = self.compress(content, level)
        
        if output_path:
            try:
                output_path.write_text(compressed, encoding="utf-8")
            except IOError:
                pass
        
        return compressed
    
    def estimate_savings(self, content: str, level: str = CompressionLevel.LIGHT) -> float:
        """
        Estimate token savings from compression.
        
        Args:
            content: Original content
            level: Compression level
        
        Returns: Savings percentage (0.0 - 1.0)
        """
        if not content:
            return 0.0
        
        compressed = self.compress(content, level)
        
        original_tokens = len(content.encode("utf-8")) / 4  # Rough estimate
        compressed_tokens = len(compressed.encode("utf-8")) / 4
        
        if original_tokens == 0:
            return 0.0
        
        savings = 1.0 - (compressed_tokens / original_tokens)
        return max(0.0, min(1.0, savings))  # Clamp to [0, 1]


if __name__ == "__main__":
    # CLI test mode
    import sys
    
    if len(sys.argv) < 4:
        print("Usage: python semantic_compress.py <input_file> <output_file> <level>")
        print("Levels: none, light, aggressive")
        sys.exit(1)
    
    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    level = sys.argv[3]
    
    compressor = SemanticCompressor()
    compressed = compressor.compress_file(input_path, level, output_path)
    
    # Show savings
    original_content = input_path.read_text(encoding="utf-8", errors="ignore")
    savings = compressor.estimate_savings(original_content, level)
    
    print(f"Compressed: {input_path.name} → {output_path.name}")
    print(f"Level: {level}")
    print(f"Token savings: {savings * 100:.1f}%")
