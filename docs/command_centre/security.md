# Command Centre Security Architecture

This document describes the security controls, trust boundaries, and threat mitigations implemented in the Command Centre MCP safety system.

## Trust Boundaries

```
┌─────────────────────────────────────────────────────────────────┐
│                         HOST SYSTEM                             │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    Claude-Flow Agent                       │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │              MCP Safety Wrapper                      │  │  │
│  │  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │  │  │
│  │  │  │ Policy.yaml │  │  Gateway    │  │  Redactor   │  │  │  │
│  │  │  │ (allowlist) │  │  (enforce)  │  │  (sanitise) │  │  │  │
│  │  │  └─────────────┘  └─────────────┘  └─────────────┘  │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  │                           │                                │  │
│  │                           ▼                                │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │                   MCP Tools                          │  │  │
│  │  │  read_file, grep, codebase_search, run_terminal_cmd  │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    Audit Logs (JSONL)                      │  │
│  │  reports/mcp_calls/session.jsonl                          │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Boundary 1: Agent → MCP Safety Wrapper
- All tool calls MUST pass through `MCPToolGateway`
- Direct tool execution is blocked by design
- Gateway enforces policy before any tool runs

### Boundary 2: MCP Safety Wrapper → Tools
- Tools only execute if allowed by policy AND session grants
- Deny patterns checked before allow patterns
- Unknown domains default to DENY

### Boundary 3: Tools → Audit Logs
- All tool calls logged (allowed and denied)
- Logs redacted before write (secrets, paths, oversized)
- CI verifies redaction with `verify_redaction.py`

## Threat Model

### Risk 1: Tool Abuse (Unauthorised Tool Execution)

**Threat**: Malicious or accidental use of dangerous tools (e.g., `rm -rf`, `upload`).

**Mitigations**:
- `MCPSafetyWrapper` validates every tool call against `policy.yaml`
- Per-domain allowlists define permitted tools
- Deny patterns block dangerous operations (e.g., `run_terminal_cmd.*upload`)
- `MCPToolGateway` raises `MCPToolDenied` for blocked calls

**Verification**:
- Unit tests in `test_mcp_safety.py` prove blocked tools are rejected
- CI runs integration tests on every PR

**Implementation**:
- `tools/command_centre/mcp_safety.py`: `MCPToolGateway.execute()`
- `tools/command_centre/policy.yaml`: `tool_allowlist` section

### Risk 2: Permission Escalation (Scope Grants Bypass)

**Threat**: Tool executes without proper authorisation, bypassing access controls.

**Mitigations**:
- Tool allowlist is necessary but not sufficient
- Session grants required in `packet.md` (e.g., `grants: [repo_read, firmware_write]`)
- Gateway checks grants cover required scopes before execution
- Missing grants result in `MCPToolDenied`

**Verification**:
- `_check_grants()` validates session grants against resource scopes
- Tests verify that allowed tools still fail without proper grants

**Implementation**:
- `tools/command_centre/mcp_safety.py`: `MCPToolGateway._check_grants()`
- `tools/command_centre/policy.yaml`: `resource_scopes` section
- `docs/contextpack/packet.md`: `## Session Grants` section

### Risk 3: Log Leakage (Secrets/Paths in Audit Logs)

**Threat**: Sensitive data (API keys, tokens, user paths) exposed in logs or CI artifacts.

**Mitigations**:
- `AuditRedactor` applies pattern matching before log writes
- Patterns cover: API keys, tokens, passwords, AWS credentials, GitHub PATs
- Absolute paths (`/Users/*/`, `/home/*/`) redacted to `[REDACTED]`
- Oversized outputs truncated to 10KB max
- CI artifacts redacted before upload

**Verification**:
- `verify_redaction.py` scans all artifacts post-redaction
- CI fails if any secret patterns detected after redaction
- Unit tests verify redaction with planted secrets

**Implementation**:
- `tools/command_centre/mcp_safety.py`: `AuditRedactor` class
- `tools/command_centre/verify_redaction.py`: Post-redaction scanner
- `.github/workflows/contextpack_test.yml`: Redact + verify steps

### Risk 4: Prompt Injection via Tool Outputs

**Threat**: Malicious content in tool outputs influences agent behaviour.

**Mitigations**:
- Tool outputs redacted before return to agent
- Oversized outputs truncated (prevents context flooding)
- Audit logs capture all tool interactions for review

**Implementation**:
- `tools/command_centre/mcp_safety.py`: `MCPToolGateway.execute()` redacts results

### Risk 5: Workflow YAML Errors

**Threat**: Malformed GitHub Actions YAML breaks CI or introduces security gaps.

**Mitigations**:
- `actionlint` runs on every PR as first CI step
- Syntax and schema errors fail CI before merge
- Explicit `permissions:` block enforces least privilege

**Verification**:
- CI lint step validates all `.github/workflows/*.yml`
- Workflow has minimal permissions: `contents: read`, `pull-requests: read`, `actions: read`

**Implementation**:
- `.github/workflows/contextpack_test.yml`: Lint step + permissions block

## Controls Checklist

| Control | Status | Implementation |
|---------|--------|----------------|
| Tool allowlist per domain | Implemented | `policy.yaml` |
| Deny patterns for dangerous ops | Implemented | `policy.yaml` |
| Resource scope requirements | Implemented | `policy.yaml` + `packet.md` |
| Session grant validation | Implemented | `MCPToolGateway._check_grants()` |
| Audit logging (JSONL) | Implemented | `MCPSafetyWrapper.log_tool_call()` |
| Secret redaction | Implemented | `AuditRedactor` |
| Path redaction | Implemented | `AuditRedactor` |
| Output truncation | Implemented | `AuditRedactor.MAX_OUTPUT_SIZE` |
| CI post-redaction scan | Implemented | `verify_redaction.py` |
| Workflow linting | Implemented | `actionlint` step |
| Least-privilege permissions | Implemented | `permissions:` block |
| Artifact retention policy | Implemented | 30 days explicit |

## Security Best Practices Alignment

This implementation follows MCP security best practices:

1. **Least Privilege**: Tools only available per-domain, with explicit scope grants
2. **Explicit Authorisation**: Session grants required in packet.md
3. **Audit Trail**: All tool calls logged to JSONL with timestamps
4. **Data Minimisation**: Secrets redacted, outputs truncated
5. **Continuous Verification**: CI validates redaction on every run

## References

- [MCP Security Best Practices](https://modelcontextprotocol.io/specification/draft/basic/security_best_practices)
- [OWASP Logging Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html)
- [GitHub Actions Security](https://docs.github.com/actions/security-for-github-actions)
