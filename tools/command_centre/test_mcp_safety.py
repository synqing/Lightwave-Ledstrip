#!/usr/bin/env python3
import json
import tempfile
import unittest
from pathlib import Path

from mcp_safety import MCPToolDenied, MCPToolGateway, MCPSafetyWrapper


def _policy_fixture():
    return {
        "tool_allowlist": {
            "documentation": {
                "allow": ["read_file"],
                "allow_scopes": ["repo_read"],
                "deny": ["run_terminal_cmd.*upload"],
            }
        },
        "resource_scopes": {
            "repo_read": {
                "description": "Read-only access to repository files",
                "tools": ["read_file"],
            }
        },
    }


class TestMCPGatewayEnforcement(unittest.TestCase):
    def test_blocked_tool_rejected(self):
        policy = _policy_fixture()
        with tempfile.TemporaryDirectory() as tmpdir:
            audit_log = Path(tmpdir) / "session.jsonl"
            wrapper = MCPSafetyWrapper(policy, dry_run=False, audit_log_path=audit_log)
            gateway = MCPToolGateway(wrapper, executor=lambda _tool, _args: "ok")

            with self.assertRaises(MCPToolDenied):
                gateway.execute(
                    "run_terminal_cmd upload",
                    "documentation",
                    {},
                    ["repo_read"],
                )

    def test_blocked_call_logged(self):
        policy = _policy_fixture()
        with tempfile.TemporaryDirectory() as tmpdir:
            audit_log = Path(tmpdir) / "session.jsonl"
            wrapper = MCPSafetyWrapper(policy, dry_run=False, audit_log_path=audit_log)
            gateway = MCPToolGateway(wrapper, executor=lambda _tool, _args: "ok")

            with self.assertRaises(MCPToolDenied):
                gateway.execute(
                    "run_terminal_cmd upload",
                    "documentation",
                    {},
                    ["repo_read"],
                )

            self.assertTrue(audit_log.exists())
            entries = audit_log.read_text(encoding="utf-8").strip().splitlines()
            self.assertGreaterEqual(len(entries), 1)
            last_entry = json.loads(entries[-1])
            self.assertFalse(last_entry["allowed"])

    def test_secrets_redacted_in_audit_log(self):
        policy = _policy_fixture()
        with tempfile.TemporaryDirectory() as tmpdir:
            audit_log = Path(tmpdir) / "session.jsonl"
            wrapper = MCPSafetyWrapper(policy, dry_run=False, audit_log_path=audit_log)
            gateway = MCPToolGateway(wrapper, executor=lambda _tool, _args: "ok")

            args = {
                "api_key": "sk_live_abc123xyz789",
                "path": "/Users/john/secret.txt",
            }
            gateway.execute("read_file", "documentation", args, ["repo_read"])

            log_content = audit_log.read_text(encoding="utf-8")
            self.assertNotIn("sk_live_abc123xyz789", log_content)
            self.assertNotIn("/Users/john/", log_content)
            self.assertIn("[REDACTED]", log_content)


if __name__ == "__main__":
    unittest.main()
