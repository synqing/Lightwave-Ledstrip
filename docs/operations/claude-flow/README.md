# Claude-Flow Integration Documentation

**Version:** 2.0
**Last Updated:** 2026-01-20
**Status:** Operational

This directory contains comprehensive documentation for Claude-Flow v3 (RuVector) strategic integration with the Lightwave-Ledstrip project.

---

## Quick Start (No API Keys Required)

**If using Claude Code with Max subscription, no API keys are needed.**

```bash
# 1. Verify Claude-Flow
npx claude-flow@v3alpha doctor

# 2. Add to ~/.claude/settings.json
{
  "mcpServers": {
    "claude-flow": {
      "command": "npx",
      "args": ["claude-flow@v3alpha", "mcp", "start"]
    }
  }
}

# 3. Restart terminal/IDE - done!
```

See **[MCP_SETUP.md](./MCP_SETUP.md)** for detailed configuration options (including API key setup for Cursor/Claude Desktop).

### Verify Integration

```bash
# Check available tools (27+ orchestration tools)
npx claude-flow@v3alpha mcp tools

# Run health check
npx claude-flow@v3alpha doctor
```

### Start Using

See **[swarm-templates.md](./swarm-templates.md)** for reusable workflows:
- Effect migration: IEffect class + PatternRegistry + docs
- API change: Handler + OpenAPI + dashboard client + docs
- Dashboard UI feature: Component + test + accessibility + docs

---

## Documentation Index

### Strategic Overview

- **[overview.md](./overview.md)** - Strategic integration overview, rationale, architectural approach
- **[PHALANX_LESSON_LEARNED.md](./PHALANX_LESSON_LEARNED.md)** - PHALANX documentation (lesson learned, FMEA)

### Operational Guides

- **[agent-routing.md](./agent-routing.md)** - Agent routing matrix, domain rules, acceptance criteria
- **[swarm-templates.md](./swarm-templates.md)** - Reusable swarm configurations, CLI snippets
- **[pair-programming.md](./pair-programming.md)** - Pair programming mode selection, usage patterns
- **[validation-checklist.md](./validation-checklist.md)** - Setup and runtime verification, troubleshooting

### Setup

- **[MCP_SETUP.md](./MCP_SETUP.md)** - MCP configuration setup, environment variables, verification

---

## Key Concepts

### Agent Routing

Tasks route to domain specialists automatically:
- **Firmware Effects** (`firmware/v2/src/effects/**`) → `embedded`/`firmware` agent
- **Network/API** (`firmware/v2/src/network/**`) → `api`/`network` agent
- **Dashboard UI** (`lightwave-dashboard/**`) → `frontend`/`ui` agent
- **Documentation** (`docs/**`) → `documentation`/`docs` agent

**Protected Files** require review gates:
- `WiFiManager.*`: CRITICAL (EventGroup bit clearing)
- `WebServer.*`: HIGH (thread safety)
- `AudioActor.*`: HIGH (atomic buffer correctness)

### Swarm Patterns

**Hierarchical** (Queen/Workers): Use when ordering matters
- Effect migration: Queen coordinates class → registry → docs sequence

**Mesh** (Peer-to-Peer): Use for independent subtasks
- Documentation updates: Parallel docs across effects/API/UI

### Pattern Memory

Successful patterns stored for reuse:
- `memory_search "effect_migration"` - IEffect class + registration + PatternRegistry
- `memory_search "centre_origin"` - Centre-origin math helpers (CENTER_LEFT/CENTER_RIGHT)
- `memory_search "british_english"` - British spelling patterns (centre/colour/initialise)

### Pair Programming Modes

- **Navigator**: High-risk changes (protected files, render loops)
- **TDD**: Test-first development (new IEffect, REST endpoint, component)
- **Switch**: General collaboration (docs with code, cross-references)

---

## Common Workflows

### Effect Migration

```bash
# Initialize hierarchical swarm
npx claude-flow@v3alpha swarm_init \
  --pattern hierarchical \
  --queen-agent embedded \
  --task "Migrate legacy effect to IEffect: <EffectName>"

# See swarm-templates.md for complete workflow
```

### API Change

```bash
# Initialize hierarchical swarm
npx claude-flow@v3alpha swarm_init \
  --pattern hierarchical \
  --queen-agent api \
  --task "Add REST endpoint: <Method> /api/v1/<resource>/<action>"

# See swarm-templates.md for complete workflow
```

### Dashboard UI Feature

```bash
# Initialize mesh swarm for parallel UI work
npx claude-flow@v3alpha swarm_init \
  --pattern mesh \
  --task "Add dashboard component: <ComponentName>"

# See swarm-templates.md for complete workflow
```

---

## Acceptance Criteria

### Firmware Effects Domain

- Centre-origin compliance (LEDs 79/80)
- No heap allocation in `render()` (no `new`/`malloc`/`std::vector` growth)
- 120+ FPS target (`s` serial command)
- No rainbows (no full hue-wheel sweeps)
- Effect ID stability (matches PatternRegistry indices)

**Validation**: `validate <effectId>` passes (centre-origin, hue-span, FPS, heap-delta)

### Network/API Domain

- Protected file protocols (WiFiManager, WebServer require review gate)
- Additive JSON only (backward compatibility)
- Thread safety (WebSocket handlers, rate limiting)
- OpenAPI sync (handler changes update OpenAPI spec)

**Validation**: WiFi connects without "IP: 0.0.0.0", GET /api/v1/effects unchanged

### Dashboard UI Domain

- Accessibility (WCAG 2.1 AA, `accessibility.spec.ts`)
- E2E tests (`npm run test:e2e`)
- No inline responsive styles (Tailwind classes only)
- Focus management (keyboard navigation)

**Validation**: E2E tests pass, accessibility checks pass, `npm run lint` passes

### Documentation Domain

- British English (centre/colour/initialise, not center/color/initialize)
- Spelling consistency (automated check: `grep -ri "center\|color\|initialize" docs/`)
- Cross-references resolve (internal doc links valid)

**Validation**: No American spellings in prose, all cross-references valid

---

## Troubleshooting

See **[validation-checklist.md](./validation-checklist.md)** for troubleshooting:
- MCP server not connecting
- Agent routing not working
- Memory search returns empty
- Tools not accessible

---

## Related Documentation

- **[AGENTS.md](../../../AGENTS.md)** - Codex Agents Guide (main agent documentation)
- **[CLAUDE.md](../../../CLAUDE.md)** - Protected files and critical code warnings

---

*This integration transforms Lightwave-Ledstrip development from sequential domain work to coordinated multi-agent execution, preserving institutional knowledge through pattern memory and enforcing invariants through intelligent routing.*
