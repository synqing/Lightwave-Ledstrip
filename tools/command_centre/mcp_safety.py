#!/usr/bin/env python3
"""
MCP Tool Safety Wrapper for Claude-Flow Integration

Provides allowlist-based tool call validation and audit logging for MCP tools.
This wrapper is designed to be integrated with Claude-Flow MCP server to enforce
tool allowlists per domain.

Usage:
    from mcp_safety import MCPSafetyWrapper
    wrapper = MCPSafetyWrapper(policy, dry_run=False, audit_log_path=Path("reports/mcp_calls/session.jsonl"))
    allowed, reason = wrapper.check_tool_call("read_file", "firmware_effects")
"""

import fnmatch
import json
import re
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple


class MCPToolDenied(RuntimeError):
    """Raised when a tool call is denied by policy or grants."""

    def __init__(self, tool_name: str, domain: str, reason: str):
        super().__init__(f"Tool {tool_name} denied in domain {domain}: {reason}")
        self.tool_name = tool_name
        self.domain = domain
        self.reason = reason


class AuditRedactor:
    """Redacts sensitive data from audit logs and CI artefacts."""

    SECRET_PATTERNS = [
        r'(?i)(api[_-]?key|secret|token|password|auth)\s*[=:]\s*["\']?[^\s"\']{8,}',
        r'sk_live_[a-zA-Z0-9]+',
        r'ghp_[a-zA-Z0-9]+',
        r'(?i)aws[_-]?(access|secret)',
    ]

    MAX_OUTPUT_SIZE = 10_000

    def redact(self, data: Any) -> Any:
        """Redact secrets, paths, and oversized outputs."""
        if isinstance(data, str):
            return self._redact_string(data)
        if isinstance(data, dict):
            return {k: self.redact(v) for k, v in data.items()}
        if isinstance(data, list):
            return [self.redact(v) for v in data]
        return data

    def _redact_string(self, value: str) -> str:
        if len(value) > self.MAX_OUTPUT_SIZE:
            value = value[:self.MAX_OUTPUT_SIZE] + f"...[TRUNCATED {len(value) - self.MAX_OUTPUT_SIZE} chars]"

        value = re.sub(r"/Users/[a-zA-Z0-9_-]+/", "/Users/[REDACTED]/", value)
        value = re.sub(r"/home/[a-zA-Z0-9_-]+/", "/home/[REDACTED]/", value)

        for pattern in self.SECRET_PATTERNS:
            value = re.sub(pattern, "[REDACTED]", value)

        return value


class MCPToolGateway:
    """Mandatory gateway for all MCP tool calls."""

    def __init__(self, wrapper: "MCPSafetyWrapper", executor: Callable[[str, Dict], Any]):
        self.wrapper = wrapper
        self.executor = executor
        self.redactor = AuditRedactor()

    def execute(self, tool_name: str, domain: str, args: Dict, session_grants: List[str]) -> Any:
        """Execute tool through safety wrapper; no bypass allowed."""
        allowed, reason = self.wrapper.check_tool_call(tool_name, domain)

        if allowed:
            allowed, reason = self._check_grants(tool_name, domain, args, session_grants)

        self.wrapper.log_tool_call(
            tool_name,
            domain,
            args,
            allowed,
            reason,
        )

        if not allowed:
            raise MCPToolDenied(tool_name, domain, reason)

        result = self.executor(tool_name, args)
        return self.redactor.redact(result)

    def _check_grants(
        self,
        tool_name: str,
        domain: str,
        args: Dict,
        session_grants: List[str],
    ) -> Tuple[bool, str]:
        tool_allowlist = self.wrapper.policy.get("tool_allowlist", {})
        domain_config = tool_allowlist.get(domain, {})
        required_scopes = domain_config.get("allow_scopes", [])
        missing = set(required_scopes) - set(session_grants)
        if missing:
            return False, f"Missing scope grants: {sorted(missing)}"

        scopes = self.wrapper.policy.get("resource_scopes", {})
        for scope_name in required_scopes:
            scope = scopes.get(scope_name, {})
            if not scope:
                return False, f"Unknown resource scope: {scope_name}"

            if "deny" in scope:
                for pattern in scope["deny"]:
                    if re.search(pattern, tool_name):
                        return False, f"Tool {tool_name} denied by scope {scope_name}"

            if "tools" in scope:
                allowed_tools = scope.get("tools", [])
                if tool_name not in allowed_tools and "*" not in allowed_tools:
                    return False, f"Tool {tool_name} not allowed by scope {scope_name}"

            if "paths" in scope:
                path_candidates = self._extract_paths(args)
                if not path_candidates:
                    return False, f"Missing path arguments for scoped tool {tool_name}"
                if not self._paths_allowed(path_candidates, scope["paths"]):
                    return False, f"Path arguments violate scope {scope_name}"

        return True, ""

    def _extract_paths(self, args: Dict) -> List[str]:
        candidates = []
        for key in ("path", "file", "target_directory", "target_file", "downloadPath"):
            value = args.get(key)
            if isinstance(value, str):
                candidates.append(value)
        return candidates

    def _paths_allowed(self, candidates: List[str], patterns: List[str]) -> bool:
        repo_root = Path.cwd()
        normalised = []
        for value in candidates:
            path = Path(value)
            try:
                if path.is_absolute():
                    path = path.resolve().relative_to(repo_root)
                normalised.append(str(path))
            except ValueError:
                normalised.append(str(path))

        for candidate in normalised:
            for pattern in patterns:
                if fnmatch.fnmatch(candidate, pattern):
                    return True
        return False


class MCPSafetyWrapper:
    """
    MCP tool safety wrapper with allowlist validation and audit logging.
    
    This wrapper validates MCP tool calls against policy allowlists and logs
    all tool calls to an audit log for security and debugging.
    
    Note: Actual tool interception happens in Claude-Flow MCP server code (external).
    This wrapper provides the validation logic that Claude-Flow reads from policy.yaml.
    """
    
    def __init__(self, policy: Dict, dry_run: bool = False, audit_log_path: Optional[Path] = None):
        """
        Initialize MCP safety wrapper.
        
        Args:
            policy: Policy dict loaded from policy.yaml
            dry_run: If True, log tool calls but don't execute
            audit_log_path: Path to JSONL audit log file
        """
        self.policy = policy
        self.dry_run = dry_run
        self.audit_log_path = audit_log_path
        self.tool_calls = []
        
        # Ensure audit log directory exists
        if audit_log_path:
            audit_log_path.parent.mkdir(parents=True, exist_ok=True)
    
    def check_tool_call(self, tool_name: str, domain: str) -> Tuple[bool, str]:
        """
        Check if tool call is allowed for domain.
        
        Args:
            tool_name: Name of MCP tool (e.g., "read_file", "run_terminal_cmd")
            domain: Domain name from policy (e.g., "firmware_effects", "network_api")
        
        Returns:
            Tuple of (allowed, reason)
        """
        tool_allowlist = self.policy.get("tool_allowlist", {})
        domain_config = tool_allowlist.get(domain, {})
        
        if not domain_config:
            # No allowlist defined for domain - default deny
            return False, f"No tool allowlist defined for domain {domain}"
        
        allowed = domain_config.get("allow", [])
        denied_patterns = domain_config.get("deny", [])
        
        # Check deny patterns first
        for pattern in denied_patterns:
            if re.search(pattern, tool_name):
                return False, f"Tool {tool_name} denied by pattern {pattern}"
        
        # Check allow list
        if tool_name in allowed:
            return True, ""
        elif "*" in allowed:  # Wildcard allow
            return True, ""
        else:
            return False, f"Tool {tool_name} not in allowlist for domain {domain}"
    
    def log_tool_call(self, tool_name: str, domain: str, args: Dict, allowed: bool, reason: str) -> None:
        """
        Log tool call to audit log and in-memory list.
        
        Args:
            tool_name: Name of MCP tool
            domain: Domain name
            args: Tool arguments (dict)
            allowed: Whether tool call is allowed
            reason: Reason for allow/deny decision
        """
        redactor = AuditRedactor()
        entry = {
            "timestamp": datetime.now().isoformat(),
            "tool": tool_name,
            "domain": domain,
            "args": redactor.redact(args),
            "allowed": allowed,
            "reason": redactor.redact(reason),
            "executed": not self.dry_run and allowed,
        }
        self.tool_calls.append(entry)
        
        if self.audit_log_path:
            try:
                with open(self.audit_log_path, "a", encoding="utf-8") as f:
                    f.write(json.dumps(entry) + "\n")
            except IOError as e:
                # Don't fail if audit log write fails
                pass
    
    def get_tool_call_stats(self) -> Dict:
        """
        Get statistics about tool calls.
        
        Returns:
            Dict with counts and statistics
        """
        total = len(self.tool_calls)
        allowed = sum(1 for call in self.tool_calls if call["allowed"])
        denied = total - allowed
        executed = sum(1 for call in self.tool_calls if call.get("executed", False))
        
        return {
            "total_calls": total,
            "allowed": allowed,
            "denied": denied,
            "executed": executed,
            "dry_run": self.dry_run,
        }
