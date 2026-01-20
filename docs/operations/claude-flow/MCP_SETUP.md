# MCP Configuration Setup

**Version:** 2.0
**Last Updated:** 2026-01-20
**Status:** Setup Guide

This document provides step-by-step instructions for configuring Claude-Flow MCP server integration with Claude Code, Cursor, or Claude Desktop.

---

## Integration Modes

Claude-Flow supports two integration modes:

| Mode | Description | API Keys Required? |
|------|-------------|-------------------|
| **MCP Server Mode** | Claude-Flow provides orchestration tools (swarm, memory, routing) while Claude Code handles LLM calls | **No** |
| **Standalone Mode** | Claude-Flow runs as a daemon with its own LLM provider | Yes (or use claude-code-provider) |

**Recommended:** MCP Server Mode with Claude Code - simplest setup, no API keys needed.

---

## Quick Start (Claude Code)

If you're using Claude Code CLI with a Max subscription, setup is simple:

```bash
# 1. Verify Claude-Flow is available
npx claude-flow@v3alpha --version

# 2. Run health check
npx claude-flow@v3alpha doctor

# 3. Add MCP server to ~/.claude/settings.json (see Configuration below)

# 4. Restart terminal/IDE
```

That's it. No API keys required - Claude Code uses your existing subscription.

---

## MCP Configuration Location

### Claude Code CLI

**Global Config** (Recommended):
- Path: `~/.claude/settings.json`
- MCP servers defined in `mcpServers` object

### Cursor IDE

**Repository-Local Config** (Preferred):
- Path: `.cursor/mcp.json` (in project root)

**Global Config** (Alternative):
- macOS: `~/Library/Application Support/Cursor/User/globalStorage/.../mcp.json`
- Windows: `%APPDATA%\Cursor\User\globalStorage\...\mcp.json`
- Linux: `~/.config/Cursor/User/globalStorage/.../mcp.json`

### Claude Desktop

**Global Config**:
- macOS: `~/Library/Application Support/Claude/claude_desktop_config.json`
- Windows: `%APPDATA%\Claude\claude_desktop_config.json`
- Linux: `~/.config/Claude/claude_desktop_config.json`

---

## Configuration Options

### Option A: Claude Code (No API Keys - Recommended)

Claude Code uses your existing Claude Max subscription via OAuth. No separate API keys required.

**Step 1: Add to Claude Code Settings**

Edit `~/.claude/settings.json` and add the `claude-flow` MCP server:

```json
{
  "mcpServers": {
    "claude-flow": {
      "command": "npx",
      "args": ["claude-flow@v3alpha", "mcp", "start"]
    }
  }
}
```

**Step 2: Restart Claude Code**

Restart your terminal session or Claude Code IDE extension.

**Step 3: Verify**

```bash
npx claude-flow@v3alpha doctor
# Should show: ✓ Claude Code CLI: vX.X.X
```

---

### Option B: Cursor/Claude Desktop (API Keys Required)

If using Cursor or Claude Desktop without Claude Code, API keys are required.

**Step 1: Create MCP Config File**

For Cursor, create `.cursor/mcp.json`:

```json
{
  "mcpServers": {
    "claude-flow": {
      "command": "npx",
      "args": ["claude-flow@v3alpha", "mcp", "start"],
      "env": {
        "ANTHROPIC_API_KEY": "${ANTHROPIC_API_KEY}",
        "OPENAI_API_KEY": "${OPENAI_API_KEY}",
        "GEMINI_API_KEY": "${GEMINI_API_KEY}"
      }
    }
  }
}
```

**Step 2: Set Environment Variables**

```bash
# Add to ~/.zshrc or ~/.bashrc
export ANTHROPIC_API_KEY="sk-ant-..."
export OPENAI_API_KEY="sk-..."
export GEMINI_API_KEY="..."
```

**Step 3: Restart IDE**

Restart Cursor or Claude Desktop for changes to take effect.

**Security Note**: API keys sourced from environment variables only (never committed to repository).

---

### Option C: Standalone Mode with Claude Code Provider

For running Claude-Flow as a daemon while using your Claude Max subscription (without separate API keys):

**Step 1: Build the Claude Code Provider Plugin**

```bash
cd plugins/claude-code-provider
npm install
npm run build
```

**Step 2: Test the Provider**

```bash
node test-provider.mjs
```

**Step 3: Configure Claude-Flow**

See [plugins/claude-code-provider/README.md](../../../plugins/claude-code-provider/README.md) for integration details.

This approach enables full swarm features without separate API billing.

---

## Verification

### Verify Claude-Flow Installation

```bash
# Test Claude-Flow availability
npx claude-flow@v3alpha --version
# Expected: v3.0.0-alpha.125 (or higher)

# Run diagnostics
npx claude-flow@v3alpha doctor
```

### Verify MCP Server

**Manual Test**:

```bash
# Start MCP server manually (for testing)
npx claude-flow@v3alpha mcp start

# Should output: "MCP server started on stdio" (or similar)
# Press Ctrl+C to stop
```

**Check Tool Availability**:

```bash
# List available MCP tools
npx claude-flow@v3alpha mcp tools
```

Once MCP server is connected, these tools are available:
- `agent/spawn`, `agent/list`, `agent/status`
- `swarm/init`, `swarm/status`, `swarm/shutdown`
- `memory/store`, `memory/search`, `memory/retrieve`
- `hooks/route`, `hooks/pre-edit`, `hooks/post-edit`
- 27+ orchestration tools total

### Verify in IDE

**In Cursor**:
- Settings → MCP → Check `claude-flow` server status (should be "Connected")

**In Claude Desktop**:
- Settings → Developers → MCP → Check `claude-flow` server status

---

## Troubleshooting

### MCP Config Not Found

**Symptom**: `claude-flow` server not appearing in MCP server list

**Resolution**:
- Verify config file location (`~/.claude/settings.json` or `.cursor/mcp.json`)
- Check JSON syntax (valid JSON, no trailing commas)
- Restart IDE/terminal after creating config

### Environment Variables Not Resolved (Option B only)

**Symptom**: MCP server fails to start (API key missing)

**Resolution**:
- Verify environment variables are set: `echo $ANTHROPIC_API_KEY`
- Check variable names match config
- Set at least one LLM provider API key

### npx Command Not Found

**Symptom**: MCP server fails to start (`npx: command not found`)

**Resolution**:
- Install Node.js 20+ (includes npm and npx)
- Verify npx availability: `npx --version`
- Use global install as fallback: `npm install -g claude-flow@v3alpha`

### Tools Not Accessible

**Symptom**: Tools not visible in tool picker or tool execution fails

**Resolution**:
- Verify MCP server is connected (check server status)
- Check tool names are correct (`agent/spawn`, not `agent_spawn`)
- Review MCP server logs

---

## Security Best Practices

1. **Never Commit API Keys**: Use environment variables only
2. **Use .gitignore**: Ensure local config files with sensitive data are ignored
3. **Restrict API Key Permissions**: Use API keys with minimal required permissions
4. **Rotate Keys Regularly**: Update API keys periodically

---

## Related Documentation

- **[validation-checklist.md](./validation-checklist.md)** - Setup and runtime verification
- **[overview.md](./overview.md)** - Strategic integration overview
- **[../../../plugins/claude-code-provider/README.md](../../../plugins/claude-code-provider/README.md)** - Claude Code Provider plugin

---

*This setup guide ensures Claude-Flow MCP integration is correctly configured. For Claude Code users, no API keys are required - just add the MCP server config and restart.*
