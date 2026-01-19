#!/usr/bin/env python3
"""
File Priority and Token Budget Manager for Context Pack

Implements priority-based file ranking and token budget enforcement for
truncating low-priority content when exceeding token limits.

Usage:
    from file_priority import FilePriorityManager, PriorityTier
    manager = FilePriorityManager()
    priority = manager.get_file_priority(file_path)
    token_estimate = manager.estimate_tokens(content)
    truncated_list = manager.enforce_budget(file_list, token_budget=10000)
"""

from pathlib import Path
from typing import Dict, List, Optional, Tuple
from enum import Enum


class PriorityTier(Enum):
    """Priority tier for file ranking."""
    CRITICAL = 0  # Core interfaces, headers, essential config
    HIGH = 1      # Effect implementations, key business logic
    MEDIUM = 2    # Supporting utilities, helper functions
    LOW = 3       # Tests, examples, documentation


class FilePriorityManager:
    """
    Manages file priorities and token budget enforcement.
    
    Priority patterns (checked in order):
    - CRITICAL: Core interfaces, headers, essential configuration
    - HIGH: Effect implementations, key business logic
    - MEDIUM: Supporting utilities, helper functions
    - LOW: Tests, examples, documentation
    """
    
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
    
    # Token estimation heuristic (chars per token)
    CHARS_PER_TOKEN = 4.0  # Rough estimate: 4 chars ≈ 1 token
    
    def __init__(self, repo_root: Optional[Path] = None):
        """
        Initialize file priority manager.
        
        Args:
            repo_root: Optional repository root for resolving relative paths
        """
        self.repo_root = Path(repo_root) if repo_root else None
        self._priority_cache: Dict[str, PriorityTier] = {}
    
    def get_file_priority(self, file_path: Path) -> PriorityTier:
        """
        Determine priority tier for a file.
        
        Args:
            file_path: Path to file (relative or absolute)
        
        Returns: PriorityTier enum value
        """
        # Resolve relative paths
        if isinstance(file_path, str):
            file_path = Path(file_path)
        
        if not file_path.is_absolute() and self.repo_root:
            file_path = self.repo_root / file_path
        
        file_str = str(file_path)
        
        # Check cache
        if file_str in self._priority_cache:
            return self._priority_cache[file_str]
        
        # Check patterns in priority order
        if self._matches_patterns(file_str, self.CRITICAL_PATTERNS):
            priority = PriorityTier.CRITICAL
        elif self._matches_patterns(file_str, self.HIGH_PATTERNS):
            priority = PriorityTier.HIGH
        elif self._matches_patterns(file_str, self.MEDIUM_PATTERNS):
            priority = PriorityTier.MEDIUM
        elif self._matches_patterns(file_str, self.LOW_PATTERNS):
            priority = PriorityTier.LOW
        else:
            # Default to MEDIUM for unknown files
            priority = PriorityTier.MEDIUM
        
        # Cache result
        self._priority_cache[file_str] = priority
        return priority
    
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
    
    def estimate_tokens(self, content: str) -> int:
        """
        Estimate token count for content (heuristic: chars / 4).
        
        Args:
            content: Content string
        
        Returns: Estimated token count
        """
        if not content:
            return 0
        return int(len(content.encode("utf-8")) / self.CHARS_PER_TOKEN)
    
    def estimate_file_tokens(self, file_path: Path) -> int:
        """
        Estimate token count for a file.
        
        Args:
            file_path: Path to file
        
        Returns: Estimated token count (0 if file not found)
        """
        if isinstance(file_path, str):
            file_path = Path(file_path)
        
        # Resolve relative paths
        if not file_path.is_absolute() and self.repo_root:
            file_path = self.repo_root / file_path
        
        if not file_path.exists():
            return 0
        
        try:
            content = file_path.read_text(encoding="utf-8", errors="ignore")
            return self.estimate_tokens(content)
        except IOError:
            return 0
    
    def rank_files(self, file_list: List[str]) -> List[Tuple[str, PriorityTier, int]]:
        """
        Rank files by priority and estimate tokens.
        
        Args:
            file_list: List of file paths (relative to repo_root)
        
        Returns: List of (file_path, priority, token_estimate) tuples, sorted by priority
        """
        ranked = []
        
        for file_path in file_list:
            full_path = Path(file_path)
            if self.repo_root and not full_path.is_absolute():
                full_path = self.repo_root / file_path
            
            priority = self.get_file_priority(full_path)
            token_estimate = self.estimate_file_tokens(full_path)
            
            ranked.append((file_path, priority, token_estimate))
        
        # Sort by priority (CRITICAL=0 first), then by token count (larger first)
        ranked.sort(key=lambda x: (x[1].value, -x[2]))
        
        return ranked
    
    def enforce_budget(
        self, file_list: List[str], token_budget: int, 
        include_all_critical: bool = True
    ) -> Tuple[List[str], List[str], int]:
        """
        Enforce token budget by excluding low-priority files.
        
        Args:
            file_list: List of file paths
            token_budget: Token budget limit
            include_all_critical: If True, always include all CRITICAL files regardless of budget
        
        Returns: Tuple of (included_files, excluded_files, total_tokens)
        """
        ranked = self.rank_files(file_list)
        
        included = []
        excluded = []
        total_tokens = 0
        
        for file_path, priority, token_estimate in ranked:
            # Always include CRITICAL files if flag set
            if include_all_critical and priority == PriorityTier.CRITICAL:
                included.append(file_path)
                total_tokens += token_estimate
                continue
            
            # Check if adding this file would exceed budget
            if total_tokens + token_estimate <= token_budget:
                included.append(file_path)
                total_tokens += token_estimate
            else:
                excluded.append(file_path)
        
        return included, excluded, total_tokens
    
    def truncate_content(self, content: str, max_tokens: int) -> str:
        """
        Truncate content to fit within token budget (preserves first N tokens).
        
        Args:
            content: Content string to truncate
            max_tokens: Maximum token count
        
        Returns: Truncated content
        """
        if not content:
            return content
        
        estimated_tokens = self.estimate_tokens(content)
        
        if estimated_tokens <= max_tokens:
            return content
        
        # Truncate by character count (rough approximation)
        max_chars = int(max_tokens * self.CHARS_PER_TOKEN)
        
        # Truncate at line boundary (preserve structure)
        lines = content.split("\n")
        truncated_lines = []
        current_chars = 0
        
        for line in lines:
            line_chars = len(line.encode("utf-8")) + 1  # +1 for newline
            if current_chars + line_chars > max_chars:
                break
            truncated_lines.append(line)
            current_chars += line_chars
        
        # Add truncation indicator
        truncated_content = "\n".join(truncated_lines)
        if len(truncated_content) < len(content):
            truncated_content += f"\n\n... [truncated: {estimated_tokens - max_tokens} tokens over budget]"
        
        return truncated_content


if __name__ == "__main__":
    # CLI test mode
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: python file_priority.py <repo_root> <command> [args...]")
        print("Commands: priority <file>, rank <file1> [file2] ..., budget <file1> [file2] ... <tokens>")
        sys.exit(1)
    
    repo_root = Path(sys.argv[1])
    command = sys.argv[2]
    
    manager = FilePriorityManager(repo_root)
    
    if command == "priority":
        if len(sys.argv) < 4:
            print("Usage: python file_priority.py <repo_root> priority <file>")
            sys.exit(1)
        file_path = Path(sys.argv[3])
        priority = manager.get_file_priority(file_path)
        print(priority.name)
    
    elif command == "rank":
        files = sys.argv[3:] if len(sys.argv) > 3 else []
        ranked = manager.rank_files(files)
        for file_path, priority, tokens in ranked:
            print(f"{priority.name:10} {tokens:6} tokens  {file_path}")
    
    elif command == "budget":
        if len(sys.argv) < 5:
            print("Usage: python file_priority.py <repo_root> budget <tokens> <file1> [file2] ...")
            sys.exit(1)
        try:
            token_budget = int(sys.argv[3])
            files = sys.argv[4:]
            included, excluded, total = manager.enforce_budget(files, token_budget)
            print(f"Token budget: {token_budget}")
            print(f"Total tokens: {total}")
            print(f"Included ({len(included)} files):")
            for f in included:
                print(f"  - {f}")
            if excluded:
                print(f"Excluded ({len(excluded)} files):")
                for f in excluded:
                    print(f"  - {f}")
        except ValueError:
            print("Error: token_budget must be an integer")
            sys.exit(1)
    
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)
