#!/usr/bin/env python3
"""
Delta Ledger for Context Pack

Tracks "already-in-context" items across sessions to avoid re-sending
unchanged payloads in prompts.

Usage:
    from delta_ledger import DeltaLedger
    ledger = DeltaLedger(ledger_path)
    if ledger.is_known(content_hash):
        # Skip unchanged content
    else:
        ledger.record(content_hash, session_id)
"""

import hashlib
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set


class DeltaLedger:
    """
    Tracks content hashes across sessions to identify unchanged content.
    
    Ledger schema:
    {
        "version": 1,
        "sessions": {
            "session-abc123": {
                "started": "2026-01-18T10:00:00Z",
                "content_hashes": ["sha256:abc...", "sha256:def..."]
            }
        },
        "global_hashes": {
            "sha256:abc...": {
                "type": "diff",
                "file": "CoreEffects.cpp",
                "last_seen": "2026-01-18T10:00:00Z"
            }
        }
    }
    """
    
    def __init__(self, ledger_path: Path):
        """
        Initialize or load delta ledger.
        
        Args:
            ledger_path: Path to ledger JSON file (typically .contextpack_ledger.json)
        """
        self.ledger_path = Path(ledger_path)
        self.data = self._load()
    
    def _load(self) -> Dict:
        """Load ledger from file or create empty structure."""
        if self.ledger_path.exists():
            try:
                with open(self.ledger_path, "r", encoding="utf-8") as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                # Corrupted or unreadable - create new
                pass
        
        # Empty structure
        return {
            "version": 1,
            "sessions": {},
            "global_hashes": {}
        }
    
    def _save(self) -> None:
        """Save ledger to file."""
        try:
            self.ledger_path.parent.mkdir(parents=True, exist_ok=True)
            with open(self.ledger_path, "w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=2, sort_keys=True)
        except IOError as e:
            print(f"Warning: Failed to save ledger: {e}", file=sys.stderr)
    
    def hash_content(self, content: str) -> str:
        """
        Compute SHA-256 hash of content.
        
        Returns hash in format "sha256:hexdigest".
        """
        sha256 = hashlib.sha256()
        sha256.update(content.encode("utf-8"))
        return f"sha256:{sha256.hexdigest()}"
    
    def is_known(self, content_hash: str, session_id: Optional[str] = None) -> bool:
        """
        Check if content hash is known (unchanged since last session).
        
        Args:
            content_hash: SHA-256 hash of content
            session_id: Optional session ID to check within specific session
        
        Returns:
            True if content hash is known, False otherwise
        """
        if session_id:
            # Check within specific session
            session = self.data.get("sessions", {}).get(session_id, {})
            session_hashes = session.get("content_hashes", [])
            return content_hash in session_hashes
        else:
            # Check global hashes
            return content_hash in self.data.get("global_hashes", {})
    
    def record(self, content_hash: str, session_id: str, content_type: str = "diff", 
               file_path: Optional[str] = None) -> None:
        """
        Record content hash in ledger.
        
        Args:
            content_hash: SHA-256 hash of content
            session_id: Session ID to associate with
            content_type: Type of content ("diff", "fixture", "anchor", etc.)
            file_path: Optional file path associated with content
        """
        now = datetime.now().isoformat() + "Z"
        
        # Record in session
        if "sessions" not in self.data:
            self.data["sessions"] = {}
        
        if session_id not in self.data["sessions"]:
            self.data["sessions"][session_id] = {
                "started": now,
                "content_hashes": []
            }
        
        if content_hash not in self.data["sessions"][session_id]["content_hashes"]:
            self.data["sessions"][session_id]["content_hashes"].append(content_hash)
        
        # Record in global hashes
        if "global_hashes" not in self.data:
            self.data["global_hashes"] = {}
        
        if content_hash not in self.data["global_hashes"]:
            self.data["global_hashes"][content_hash] = {
                "type": content_type,
                "file": file_path,
                "last_seen": now
            }
        else:
            # Update last_seen timestamp
            self.data["global_hashes"][content_hash]["last_seen"] = now
        
        self._save()
    
    def get_unchanged_files(self, file_hashes: Dict[str, str], session_id: Optional[str] = None) -> Set[str]:
        """
        Get set of unchanged file paths (hashes match known content).
        
        Args:
            file_hashes: Dict mapping file_path -> content_hash
            session_id: Optional session ID to check within specific session
        
        Returns:
            Set of file paths that are unchanged
        """
        unchanged = set()
        
        for file_path, content_hash in file_hashes.items():
            if self.is_known(content_hash, session_id):
                unchanged.add(file_path)
        
        return unchanged
    
    def clear_session(self, session_id: str) -> None:
        """
        Clear all content hashes for a specific session.
        
        Args:
            session_id: Session ID to clear
        """
        if "sessions" in self.data and session_id in self.data["sessions"]:
            del self.data["sessions"][session_id]
            self._save()
    
    def get_stats(self) -> Dict:
        """
        Get ledger statistics.
        
        Returns:
            Dict with keys: total_sessions, total_hashes, oldest_session, newest_session
        """
        sessions = self.data.get("sessions", {})
        global_hashes = self.data.get("global_hashes", {})
        
        session_times = []
        for session_id, session_data in sessions.items():
            started = session_data.get("started", "")
            if started:
                session_times.append(started)
        
        return {
            "total_sessions": len(sessions),
            "total_hashes": len(global_hashes),
            "oldest_session": min(session_times) if session_times else None,
            "newest_session": max(session_times) if session_times else None,
        }


def hash_file_content(file_path: Path) -> str:
    """
    Compute SHA-256 hash of file content.
    
    Returns hash in format "sha256:hexdigest".
    """
    try:
        content = file_path.read_text(encoding="utf-8", errors="ignore")
        sha256 = hashlib.sha256()
        sha256.update(content.encode("utf-8"))
        return f"sha256:{sha256.hexdigest()}"
    except IOError:
        return ""


if __name__ == "__main__":
    # CLI test mode
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: python delta_ledger.py <ledger_path> <command> [args...]")
        print("Commands: hash <content>, is_known <hash> [session_id], record <hash> <session_id>, stats")
        sys.exit(1)
    
    ledger_path = Path(sys.argv[1])
    command = sys.argv[2]
    
    ledger = DeltaLedger(ledger_path)
    
    if command == "hash":
        if len(sys.argv) < 4:
            print("Usage: python delta_ledger.py <ledger_path> hash <content>")
            sys.exit(1)
        content = sys.argv[3]
        print(ledger.hash_content(content))
    
    elif command == "is_known":
        if len(sys.argv) < 4:
            print("Usage: python delta_ledger.py <ledger_path> is_known <hash> [session_id]")
            sys.exit(1)
        content_hash = sys.argv[3]
        session_id = sys.argv[4] if len(sys.argv) > 4 else None
        result = ledger.is_known(content_hash, session_id)
        print("KNOWN" if result else "UNKNOWN")
    
    elif command == "record":
        if len(sys.argv) < 5:
            print("Usage: python delta_ledger.py <ledger_path> record <hash> <session_id> [type] [file]")
            sys.exit(1)
        content_hash = sys.argv[3]
        session_id = sys.argv[4]
        content_type = sys.argv[5] if len(sys.argv) > 5 else "diff"
        file_path = sys.argv[6] if len(sys.argv) > 6 else None
        ledger.record(content_hash, session_id, content_type, file_path)
        print("Recorded")
    
    elif command == "stats":
        stats = ledger.get_stats()
        print(json.dumps(stats, indent=2))
    
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)
