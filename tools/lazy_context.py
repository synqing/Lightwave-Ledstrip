#!/usr/bin/env python3
"""
Lazy Context Loader for Context Pack

Implements Just-in-Time (JIT) context loading for large files, loading only
what's needed when requested, with priority-based ordering.

Usage:
    from lazy_context import LazyContextLoader
    loader = LazyContextLoader(repo_root)
    critical_files = loader.get_critical_files()
    large_file_chunk = loader.load_chunk("large_file.cpp", start_line=100, end_line=200)
"""

from pathlib import Path
from typing import Dict, List, Optional, Tuple


class LazyContextLoader:
    """
    Lazy context loader with priority-based file ordering and chunked loading.
    
    Priority tiers:
    - CRITICAL: Core interfaces, headers, essential configuration
    - HIGH: Effect implementations, key business logic
    - MEDIUM: Supporting utilities, helper functions
    - LOW: Tests, examples, documentation
    """
    
    # Priority patterns (checked in order)
    CRITICAL_PATTERNS = [
        "**/IEffect.h",
        "**/EffectContext.h",
        "**/PatternRegistry.cpp",
        "**/CoreEffects.h",
        "**/Config.h",
        "**/main.cpp",
    ]
    
    HIGH_PATTERNS = [
        "**/effects/**/*Effect.cpp",
        "**/effects/**/*Effect.h",
        "**/network/**/*Handler.cpp",
        "**/core/actors/RendererActor.*",
    ]
    
    MEDIUM_PATTERNS = [
        "**/utils/**",
        "**/helpers/**",
        "**/palettes/**",
    ]
    
    LOW_PATTERNS = [
        "**/test/**",
        "**/examples/**",
        "**/*.md",
    ]
    
    # Files to always defer (never load eagerly)
    DEFERRED_PATTERNS = [
        "**/*.log",
        "**/*.bin",
        "**/node_modules/**",
        "**/.pio/**",
    ]
    
    def __init__(self, repo_root: Path):
        """
        Initialize lazy context loader.
        
        Args:
            repo_root: Repository root directory
        """
        self.repo_root = Path(repo_root)
        self._file_cache: Dict[str, str] = {}
        self._loaded_files: set = set()
    
    def get_priority(self, file_path: Path) -> str:
        """
        Determine priority tier for a file.
        
        Returns: "CRITICAL", "HIGH", "MEDIUM", "LOW", or "DEFERRED"
        """
        file_str = str(file_path)
        
        # Check deferred patterns first
        if self._matches_patterns(file_str, self.DEFERRED_PATTERNS):
            return "DEFERRED"
        
        # Check priority patterns in order
        if self._matches_patterns(file_str, self.CRITICAL_PATTERNS):
            return "CRITICAL"
        elif self._matches_patterns(file_str, self.HIGH_PATTERNS):
            return "HIGH"
        elif self._matches_patterns(file_str, self.MEDIUM_PATTERNS):
            return "MEDIUM"
        elif self._matches_patterns(file_str, self.LOW_PATTERNS):
            return "LOW"
        else:
            # Default to MEDIUM for unknown files
            return "MEDIUM"
    
    def _matches_patterns(self, file_str: str, patterns: List[str]) -> bool:
        """Check if file matches any pattern in list."""
        import fnmatch
        
        for pattern in patterns:
            if fnmatch.fnmatch(file_str, pattern):
                return True
            # Also check against filename only
            if fnmatch.fnmatch(Path(file_str).name, Path(pattern).name):
                return True
        return False
    
    def get_critical_files(self, file_list: List[str]) -> List[str]:
        """
        Filter file list to only critical priority files.
        
        Returns: List of critical file paths (relative to repo_root)
        """
        critical = []
        for file_path in file_list:
            full_path = self.repo_root / file_path
            if self.get_priority(full_path) == "CRITICAL":
                critical.append(file_path)
        return critical
    
    def get_priority_ordered_files(self, file_list: List[str]) -> List[str]:
        """
        Sort file list by priority (CRITICAL -> HIGH -> MEDIUM -> LOW).
        
        Returns: Sorted list of file paths (relative to repo_root)
        """
        priority_order = {"CRITICAL": 0, "HIGH": 1, "MEDIUM": 2, "LOW": 3, "DEFERRED": 4}
        
        def priority_key(file_path: str) -> int:
            full_path = self.repo_root / file_path
            priority = self.get_priority(full_path)
            return priority_order.get(priority, 3)
        
        return sorted(file_list, key=priority_key)
    
    def load_chunk(self, file_path: Path, start_line: int = 0, end_line: Optional[int] = None) -> str:
        """
        Load a chunk of a file (line range).
        
        Args:
            file_path: Path to file (relative to repo_root or absolute)
            start_line: Starting line (0-indexed)
            end_line: Ending line (exclusive). If None, loads to end.
        
        Returns: File chunk content as string
        """
        if isinstance(file_path, str):
            file_path = Path(file_path)
        
        # Resolve relative paths
        if not file_path.is_absolute():
            file_path = self.repo_root / file_path
        
        if not file_path.exists():
            return ""
        
        # Check cache first
        cache_key = f"{file_path}:{start_line}:{end_line}"
        if cache_key in self._file_cache:
            return self._file_cache[cache_key]
        
        try:
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
            
            # Extract chunk
            start_idx = max(0, start_line)
            end_idx = len(lines) if end_line is None else min(len(lines), end_line)
            chunk_lines = lines[start_idx:end_idx]
            
            content = "".join(chunk_lines)
            
            # Cache chunk
            self._file_cache[cache_key] = content
            self._loaded_files.add(str(file_path))
            
            return content
        except IOError:
            return ""
    
    def load_file(self, file_path: Path) -> str:
        """
        Load entire file (if not already cached).
        
        Args:
            file_path: Path to file (relative to repo_root or absolute)
        
        Returns: File content as string
        """
        return self.load_chunk(file_path, start_line=0, end_line=None)
    
    def estimate_tokens(self, content: str) -> int:
        """
        Estimate token count for content (rough heuristic: ~4 chars per token).
        
        Args:
            content: Content string
        
        Returns: Estimated token count
        """
        return len(content.encode("utf-8")) // 4
    
    def get_large_files(self, file_list: List[str], min_size_kb: int = 50) -> List[str]:
        """
        Identify large files that should be loaded lazily.
        
        Args:
            file_list: List of file paths (relative to repo_root)
            min_size_kb: Minimum size in KB to consider "large"
        
        Returns: List of large file paths
        """
        large_files = []
        min_size_bytes = min_size_kb * 1024
        
        for file_path in file_list:
            full_path = self.repo_root / file_path
            if full_path.exists():
                try:
                    size = full_path.stat().st_size
                    if size >= min_size_bytes:
                        large_files.append(file_path)
                except (IOError, OSError):
                    pass
        
        return large_files
    
    def clear_cache(self) -> None:
        """Clear file cache."""
        self._file_cache.clear()
        self._loaded_files.clear()


if __name__ == "__main__":
    # CLI test mode
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: python lazy_context.py <repo_root> <command> [args...]")
        print("Commands: priority <file>, critical <file1> [file2] ..., chunk <file> <start> [end]")
        sys.exit(1)
    
    repo_root = Path(sys.argv[1])
    command = sys.argv[2]
    
    loader = LazyContextLoader(repo_root)
    
    if command == "priority":
        if len(sys.argv) < 4:
            print("Usage: python lazy_context.py <repo_root> priority <file>")
            sys.exit(1)
        file_path = Path(sys.argv[3])
        priority = loader.get_priority(file_path)
        print(priority)
    
    elif command == "critical":
        files = sys.argv[3:] if len(sys.argv) > 3 else []
        critical = loader.get_critical_files(files)
        for f in critical:
            print(f)
    
    elif command == "chunk":
        if len(sys.argv) < 5:
            print("Usage: python lazy_context.py <repo_root> chunk <file> <start_line> [end_line]")
            sys.exit(1)
        file_path = Path(sys.argv[3])
        start_line = int(sys.argv[4])
        end_line = int(sys.argv[5]) if len(sys.argv) > 5 else None
        content = loader.load_chunk(file_path, start_line, end_line)
        print(content)
    
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)
