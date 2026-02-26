# Nogic Visualizer workflow

Nogic may report:

1. **No Architecture Map** — “Do `!nogicreview` on your MCP client”
2. **No canvas yet** — “Use the `agent_canvas` MCP tool to generate visual diagrams, flowcharts, and dashboards”

## What to do

### 1. Run the Nogic review

- **Where:** In the MCP client where **Nogic Visualizer** is installed (e.g. Cursor with Nogic MCP server).
- **Action:** Run the review command (e.g. `!nogicreview` or the equivalent in your client). That builds/updates Nogic’s internal project index and architecture map.
- **Note:** This agent session does not have Nogic MCP tools; the review must be run from your side.

### 2. Generate a canvas

- **Tool:** Use the **`agent_canvas`** MCP tool (from the same client where Nogic is connected).
- **Purpose:** Create architecture diagrams, flowcharts, or dashboards from the codebase. Nogic uses its index to drive diagram generation.
- **Reference:** For content and structure, use the repo’s architecture docs (see below) so the canvas matches LightwaveOS.

### 3. Repo architecture references

| Document | Purpose |
|----------|---------|
| [ARCHITECTURE.md](/ARCHITECTURE.md) | Quick reference: invariants, data flow, key files. |
| [docs/architecture/ARCHITECTURE_MAP.md](../architecture/ARCHITECTURE_MAP.md) | Single high-level architecture map (Mermaid + links). |
| [docs/architecture/ARCHITECTURE_CANVAS.html](../architecture/ARCHITECTURE_CANVAS.html) | Browser-viewable architecture canvas (flow + sequence diagrams). |
| [docs/architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md](../architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md) | Full infrastructure: Mermaid diagrams, timing, memory, network. |

Use these when prompting Nogic’s `agent_canvas` (e.g. “Generate a canvas from our ARCHITECTURE_MAP and 00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE data flow”).

## How this fits with other tools

- **Auggie** — Semantic code search (“find code by meaning”).
- **claude-mem** — Cross-session memory (“what we did before”).
- **Nogic** — Structure and impact: symbol graph, callers/callees, blast radius, execution flows, **architecture map**, and **canvas** (diagrams).

The architecture map and canvas are the parts only Nogic provides; the repo docs above feed into them.
