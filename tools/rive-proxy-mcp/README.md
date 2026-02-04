# Lightwave Rive Proxy MCP

Companion MCP server that provides **macro tools** on top of **Rive Early Access MCP**.

## What this is

- **Upstream**: Rive EA exposes an MCP server on `http://127.0.0.1:9791/sse` (legacy HTTP+SSE transport).
- **This server**: connects to that upstream and exposes higher-level tools like:
  - `rive_proxy_health`
  - `rive_proxy_snapshot`
  - `rive_proxy_ensure_view_models_from_spec`
  - `rive_proxy_create_layout_template`
  - `rive_proxy_scaffold_from_spec`
  - `rive_proxy_bind_inputs_1to1`
  - `rive_proxy_reconcile`

## Install

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tools/rive-proxy-mcp
npm install
npm run build
```

## Run (manual)

```bash
RIVE_MCP_URL="http://127.0.0.1:9791/sse" node dist/index.js
```

## Cursor config

Add this to your project `.cursor/mcp.json`:

```json
{
  "mcpServers": {
    "rive-proxy": {
      "command": "bash",
      "args": [
        "-lc",
        "cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tools/rive-proxy-mcp && node dist/index.js"
      ],
      "env": {
        "RIVE_MCP_URL": "http://127.0.0.1:9791/sse"
      }
    }
  }
}
```

Notes:
- You must have **Rive EA running** (so the upstream is listening).
- If you update the server code, re-run `npm run build`.
