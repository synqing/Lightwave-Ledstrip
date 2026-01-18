# MCP Configuration Setup

**Version:** 1.0  
**Last Updated:** 2026-01-18  
**Status:** Setup Guide

This document provides step-by-step instructions for configuring Claude-Flow MCP server integration in Cursor or Claude Desktop.

---

## MCP Configuration Location

### Cursor IDE

**Repository-Local Config** (Preferred):
- Path: `.cursor/mcp.json` (in project root)
- Note: This path may be filtered by `.gitignore` - create manually if needed

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

## Configuration Steps

### Step 1: Create MCP Config File

**For Cursor (Repository-Local)**:

1. Create `.cursor/` directory if it doesn't exist:
   ```bash
   mkdir -p .cursor
   ```

2. Create `.cursor/mcp.json` with the following content:

```json
{
  "mcpServers": {
    "claude-flow": {
      "command": "npx",
      "args": ["claude-flow@v3alpha", "mcp", "start"],
      "env": {
        "ANTHROPIC_API_KEY": "${ANTHROPIC_API_KEY}",
        "OPENAI_API_KEY": "${OPENAI_API_KEY}",
        "GEMINI_API_KEY": "${GEMINI_API_KEY}",
        "CLAUDE_FLOW_PROJECT_ROOT": "${PROJECT_ROOT}"
      }
    }
  }
}
```

**Security Note**: API keys sourced from environment variables only (never committed to repository).

---

### Step 2: Set Environment Variables

Set at least one LLM provider API key:

**macOS/Linux**:

```bash
# Add to ~/.zshrc or ~/.bashrc
export ANTHROPIC_API_KEY="sk-ant-..."
export OPENAI_API_KEY="sk-..."
export GEMINI_API_KEY="..."

# Or use .env file (if supported by Cursor)
echo "ANTHROPIC_API_KEY=sk-ant-..." >> .env
echo "OPENAI_API_KEY=sk-..." >> .env
```

**Windows**:

```powershell
# Set environment variables (User or System level)
[System.Environment]::SetEnvironmentVariable("ANTHROPIC_API_KEY", "sk-ant-...", "User")
[System.Environment]::SetEnvironmentVariable("OPENAI_API_KEY", "sk-...", "User")
```

**Verify Environment Variables**:

```bash
# Check if API keys are set
echo $ANTHROPIC_API_KEY  # Should output key (or empty)
echo $OPENAI_API_KEY     # Should output key (or empty)
```

---

### Step 3: Verify Claude-Flow Installation

**Using npx** (No global install needed):

```bash
# Test Claude-Flow availability
npx claude-flow@v3alpha --version
# Expected: v3alpha or higher
```

**Global Install** (Optional):

```bash
# Install Claude-Flow globally
npm install -g claude-flow@v3alpha

# Verify installation
claude-flow --version
```

---

### Step 4: Test MCP Server

**Manual Test**:

```bash
# Start MCP server manually (for testing)
npx claude-flow@v3alpha mcp start

# Should output: "MCP server started on stdio" (or similar)
# Press Ctrl+C to stop
```

**Via Cursor/Claude Desktop**:

1. **Cursor**: Open Settings → MCP → Verify `claude-flow` server appears in list
2. **Claude Desktop**: Open Settings → Developers → MCP → Verify `claude-flow` server appears

**Check Tool Availability**:

Once MCP server is connected, verify tools are available:
- `swarm_init`
- `agent_spawn`
- `memory_search`
- `hooks_route`
- (170+ more tools)

---

### Step 5: Restart IDE/Desktop

After creating MCP config:

1. **Cursor**: Restart Cursor IDE (File → Quit, then reopen)
2. **Claude Desktop**: Restart Claude Desktop (Quit and reopen)

MCP servers are loaded at startup, so restart is required for config changes to take effect.

---

## Verification

### Verify MCP Server Connection

**In Cursor**:
- Settings → MCP → Check `claude-flow` server status (should be "Connected" or "Running")

**In Claude Desktop**:
- Settings → Developers → MCP → Check `claude-flow` server status

### Verify Tool Access

**Test Tool Execution**:

```bash
# Via MCP client (Cursor/Claude Desktop), test tools:
# - swarm_init --help
# - agent_spawn --help
# - memory_search "test"
# - hooks_route --status
```

If tools are not accessible, check:
1. MCP config file syntax (valid JSON)
2. Environment variables are set
3. Claude-Flow installation (npx or global)
4. IDE/Desktop was restarted after config changes

---

## Troubleshooting

### MCP Config Not Found

**Symptom**: `claude-flow` server not appearing in MCP server list

**Resolution**:
- Verify config file location (`.cursor/mcp.json` or global config path)
- Check JSON syntax (valid JSON, no trailing commas)
- Restart IDE/Desktop after creating config

---

### Environment Variables Not Resolved

**Symptom**: MCP server fails to start (API key missing)

**Resolution**:
- Verify environment variables are set: `echo $ANTHROPIC_API_KEY`
- Check variable names match config (`ANTHROPIC_API_KEY`, not `ANTHROPIC_KEY`)
- Set at least one LLM provider API key (Anthropic, OpenAI, or Gemini)

---

### npx Command Not Found

**Symptom**: MCP server fails to start (`npx: command not found`)

**Resolution**:
- Install Node.js 18+ (includes npm and npx)
- Verify npx availability: `npx --version`
- Use global install as fallback: `npm install -g claude-flow@v3alpha` (then update config to use `claude-flow` command instead of `npx`)

---

### Tools Not Accessible

**Symptom**: Tools not visible in tool picker or tool execution fails

**Resolution**:
- Verify MCP server is connected (check server status in Settings)
- Check tool names are correct (swarm_init, agent_spawn, etc.)
- Review MCP server logs (Cursor/Claude Desktop console or logs)

---

## Security Best Practices

1. **Never Commit API Keys**: Use environment variables only, never hardcode keys in config files
2. **Use .gitignore**: Ensure `.cursor/mcp.json` (if local) is in `.gitignore` if it contains any sensitive paths
3. **Restrict API Key Permissions**: Use API keys with minimal required permissions
4. **Rotate Keys Regularly**: Update API keys periodically for security

---

## Related Documentation

- **[validation-checklist.md](./validation-checklist.md)** - Setup and runtime verification
- **[overview.md](./overview.md)** - Strategic integration overview

---

*This setup guide ensures Claude-Flow MCP integration is correctly configured with secure API key handling and proper IDE/Desktop restart procedures.*
