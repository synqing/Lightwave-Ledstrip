#!/usr/bin/env python3
"""
Fragment Cache for Context Pack

Caches outputs of common prompt fragments (effect registry, metadata list)
for instant reuse across multiple context pack generations.

Usage:
    from fragment_cache import FragmentCache
    cache = FragmentCache(cache_dir)
    effects_registry = cache.get_or_generate("effects_registry", generate_fn)
"""

import hashlib
import json
import os
import sys
from datetime import datetime, timedelta
from pathlib import Path
from typing import Callable, Dict, Optional


class FragmentCache:
    """
    Cache for common prompt fragments with TTL and invalidation.
    
    Cache structure:
    .contextpack_cache/
    ├── effects_registry.toon
    ├── effects_registry.meta.json  # TTL, source_hash, etc.
    ├── palette_summary.toon
    ├── palette_summary.meta.json
    └── ...
    """
    
    DEFAULT_TTL_HOURS = 1  # 1 hour default TTL
    
    def __init__(self, cache_dir: Path, ttl_hours: float = DEFAULT_TTL_HOURS):
        """
        Initialize fragment cache.
        
        Args:
            cache_dir: Cache directory (typically .contextpack_cache/)
            ttl_hours: Time-to-live in hours for cache entries
        """
        self.cache_dir = Path(cache_dir)
        self.ttl_hours = ttl_hours
        self.cache_dir.mkdir(parents=True, exist_ok=True)
    
    def _get_meta_path(self, cache_key: str) -> Path:
        """Get path to metadata file for cache key."""
        return self.cache_dir / f"{cache_key}.meta.json"
    
    def _get_cache_path(self, cache_key: str, extension: str = ".toon") -> Path:
        """Get path to cached content file."""
        return self.cache_dir / f"{cache_key}{extension}"
    
    def _hash_source_file(self, source_path: Path) -> str:
        """
        Compute SHA-256 hash of source file.
        
        Returns hash as hex string, or empty string if file doesn't exist.
        """
        if not source_path.exists():
            return ""
        
        try:
            content = source_path.read_bytes()
            sha256 = hashlib.sha256()
            sha256.update(content)
            return sha256.hexdigest()
        except IOError:
            return ""
    
    def _load_metadata(self, cache_key: str) -> Optional[Dict]:
        """Load metadata for cache key."""
        meta_path = self._get_meta_path(cache_key)
        if not meta_path.exists():
            return None
        
        try:
            with open(meta_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except (json.JSONDecodeError, IOError):
            return None
    
    def _save_metadata(self, cache_key: str, metadata: Dict) -> None:
        """Save metadata for cache key."""
        meta_path = self._get_meta_path(cache_key)
        try:
            with open(meta_path, "w", encoding="utf-8") as f:
                json.dump(metadata, f, indent=2)
        except IOError as e:
            print(f"Warning: Failed to save cache metadata: {e}", file=sys.stderr)
    
    def is_valid(self, cache_key: str, source_paths: Optional[list[Path]] = None) -> bool:
        """
        Check if cached fragment is valid (not expired, source unchanged).
        
        Args:
            cache_key: Cache key
            source_paths: Optional list of source file paths to check for changes
        
        Returns:
            True if cache entry is valid, False otherwise
        """
        metadata = self._load_metadata(cache_key)
        if not metadata:
            return False
        
        # Check TTL
        created_str = metadata.get("created")
        if created_str:
            try:
                created = datetime.fromisoformat(created_str.replace("Z", "+00:00"))
                age = datetime.now(created.tzinfo) - created
                if age > timedelta(hours=self.ttl_hours):
                    return False
            except (ValueError, TypeError):
                return False
        
        # Check source file hashes if provided
        if source_paths:
            cached_source_hashes = metadata.get("source_hashes", {})
            for source_path in source_paths:
                source_str = str(source_path)
                current_hash = self._hash_source_file(source_path)
                cached_hash = cached_source_hashes.get(source_str)
                
                if current_hash != cached_hash:
                    return False
        
        # Check cache file exists
        cache_path = self._get_cache_path(cache_key, metadata.get("extension", ".toon"))
        return cache_path.exists()
    
    def get(self, cache_key: str, source_paths: Optional[list[Path]] = None) -> Optional[str]:
        """
        Get cached fragment content if valid.
        
        Args:
            cache_key: Cache key
            source_paths: Optional list of source file paths to check for changes
        
        Returns:
            Cached content as string, or None if not valid/not found
        """
        if not self.is_valid(cache_key, source_paths):
            return None
        
        metadata = self._load_metadata(cache_key)
        if not metadata:
            return None
        
        cache_path = self._get_cache_path(cache_key, metadata.get("extension", ".toon"))
        
        try:
            return cache_path.read_text(encoding="utf-8")
        except IOError:
            return None
    
    def put(self, cache_key: str, content: str, source_paths: Optional[list[Path]] = None,
            extension: str = ".toon") -> None:
        """
        Store fragment in cache.
        
        Args:
            cache_key: Cache key
            content: Content to cache
            source_paths: Optional list of source file paths (for invalidation)
            extension: File extension (default: ".toon")
        """
        cache_path = self._get_cache_path(cache_key, extension)
        
        try:
            cache_path.write_text(content, encoding="utf-8")
        except IOError as e:
            print(f"Warning: Failed to write cache file: {e}", file=sys.stderr)
            return
        
        # Save metadata
        source_hashes = {}
        if source_paths:
            for source_path in source_paths:
                source_str = str(source_path)
                source_hashes[source_str] = self._hash_source_file(source_path)
        
        metadata = {
            "created": datetime.now().isoformat() + "Z",
            "extension": extension,
            "size": len(content.encode("utf-8")),
            "source_hashes": source_hashes,
        }
        
        self._save_metadata(cache_key, metadata)
    
    def get_or_generate(self, cache_key: str, generate_fn: Callable[[], str],
                       source_paths: Optional[list[Path]] = None,
                       extension: str = ".toon") -> str:
        """
        Get cached fragment or generate and cache it.
        
        Args:
            cache_key: Cache key
            generate_fn: Function that generates the fragment content
            source_paths: Optional list of source file paths (for invalidation)
            extension: File extension (default: ".toon")
        
        Returns:
            Fragment content (cached or newly generated)
        """
        cached = self.get(cache_key, source_paths)
        if cached is not None:
            return cached
        
        # Generate new content
        content = generate_fn()
        self.put(cache_key, content, source_paths, extension)
        return content
    
    def invalidate(self, cache_key: str) -> None:
        """Invalidate a specific cache entry."""
        meta_path = self._get_meta_path(cache_key)
        metadata = self._load_metadata(cache_key)
        
        if metadata:
            cache_path = self._get_cache_path(cache_key, metadata.get("extension", ".toon"))
            if cache_path.exists():
                try:
                    cache_path.unlink()
                except IOError:
                    pass
        
        if meta_path.exists():
            try:
                meta_path.unlink()
            except IOError:
                pass
    
    def clear(self) -> None:
        """Clear all cache entries."""
        if self.cache_dir.exists():
            for file_path in self.cache_dir.glob("*"):
                try:
                    file_path.unlink()
                except IOError:
                    pass


if __name__ == "__main__":
    # CLI test mode
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: python fragment_cache.py <cache_dir> <command> [args...]")
        print("Commands: get <key> [source_paths...], put <key> <content> [source_paths...], invalidate <key>, clear")
        sys.exit(1)
    
    cache_dir = Path(sys.argv[1])
    command = sys.argv[2]
    
    cache = FragmentCache(cache_dir)
    
    if command == "get":
        if len(sys.argv) < 4:
            print("Usage: python fragment_cache.py <cache_dir> get <key> [source_paths...]")
            sys.exit(1)
        cache_key = sys.argv[3]
        source_paths = [Path(p) for p in sys.argv[4:]] if len(sys.argv) > 4 else None
        content = cache.get(cache_key, source_paths)
        if content:
            print(content)
        else:
            print("Cache miss", file=sys.stderr)
            sys.exit(1)
    
    elif command == "put":
        if len(sys.argv) < 5:
            print("Usage: python fragment_cache.py <cache_dir> put <key> <content> [source_paths...]")
            sys.exit(1)
        cache_key = sys.argv[3]
        content = sys.argv[4]
        source_paths = [Path(p) for p in sys.argv[5:]] if len(sys.argv) > 5 else None
        cache.put(cache_key, content, source_paths)
        print("Cached")
    
    elif command == "invalidate":
        if len(sys.argv) < 4:
            print("Usage: python fragment_cache.py <cache_dir> invalidate <key>")
            sys.exit(1)
        cache_key = sys.argv[3]
        cache.invalidate(cache_key)
        print("Invalidated")
    
    elif command == "clear":
        cache.clear()
        print("Cleared")
    
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)
