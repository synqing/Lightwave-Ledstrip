# Claude-Flow Strategic Integration Overview

**Version:** 1.0  
**Last Updated:** 2026-01-18  
**Status:** Operational Guide

This document provides a comprehensive overview of Claude-Flow (RuVector) integration for the Lightwave-Ledstrip project, covering strategic rationale, architectural approach, and tactical implementation patterns.

---

## Executive Summary

Claude-Flow v3 provides self-learning neural capabilities for agent orchestration, enabling intelligent task routing, specialised agent deployment, and pattern memory across the Lightwave-Ledstrip codebase. This integration transforms how we manage parallel workstreams across firmware effects, network APIs, dashboard UI, and documentation.

**Key Capabilities Leveraged:**

- **54+ Specialised Agents**: Domain-specific experts for firmware, network, UI, and documentation
- **Coordinated Agent Swarms**: Hierarchical (queen/workers) and mesh (peer-to-peer) patterns
- **Self-Learning Routing**: Pattern memory prevents catastrophic forgetting of successful approaches
- **MCP Integration**: Native Claude Code integration via Model Context Protocol
- **Production Security**: Built-in protection against prompt injection, path traversal, command injection

---

## Strategic Rationale

### Why Claude-Flow for Lightwave-Ledstrip?

**1. Multi-Domain Complexity**

The Lightwave-Ledstrip project spans four distinct domains:
- **Firmware Effects** (`firmware/v2/src/effects/`) - Centre-origin constraints, no heap allocation, 120+ FPS
- **Network/API** (`firmware/v2/src/network/`) - REST/WebSocket handlers, protected file policies
- **Dashboard UI** (`lightwave-dashboard/`) - React/TypeScript, accessibility standards, E2E tests
- **Documentation** (`docs/`) - British English, audio-reactive pipeline docs, architectural references

Each domain requires specialised knowledge that benefits from dedicated agent routing.

**2. Invariant Enforcement**

Core constraints must be enforced consistently:
- Centre-origin effects (LEDs 79/80)
- No heap allocation in render loops
- No rainbow colour cycling
- British English in all documentation
- Protected file review protocols (WiFiManager, WebServer, AudioActor)

Claude-Flow's routing and memory systems ensure these invariants are remembered and applied across all agent interactions.

**3. Parallel Workstream Management**

Common workflows span multiple domains:
- Effect migration: IEffect class + PatternRegistry + docs
- API changes: Handler + OpenAPI + dashboard client + docs
- Dashboard features: Component + test + accessibility + docs

Swarm orchestration enables safe parallel execution with quality gates.

---

## Architectural Approach

### Agent Routing Map

Files and tasks route to specialised agents based on domain boundaries:

```
firmware/v2/src/effects/**     → Firmware/Embedded Agent
firmware/v2/src/network/**     → Network/API Agent  
lightwave-dashboard/**         → Frontend/UI Agent
docs/**                        → Documentation Agent
```

**Protected Files** (require extra review):
- `firmware/v2/src/network/WiFiManager.*` (FreeRTOS EventGroup landmines)
- `firmware/v2/src/network/WebServer.*` (rate limiting, WebSocket thread safety)
- `firmware/v2/src/audio/AudioActor.*` (cross-core atomic buffer)

### Swarm Patterns

**Hierarchical (Queen/Workers)**: Use when ordering matters
- Effect migration: Queen coordinates class → registry → docs sequence
- API changes: Queen ensures handler → OpenAPI → client → docs ordering

**Mesh (Peer-to-Peer)**: Use for independent subtasks
- Documentation updates: Parallel docs across effects/API/UI
- Test suite expansion: Independent test files across domains

### Memory Search Integration

Pattern memory accelerates repeated tasks:
- `memory_search` for "effect migration" finds previous IEffect → PatternRegistry patterns
- `memory_search` for "palette change" retrieves British English palette doc templates
- `memory_search` for "REST endpoint" surfaces additive JSON field patterns

---

## MCP Integration Setup

### Configuration

Claude-Flow integrates via MCP server configuration in `.cursor/mcp.json`:

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

**Security Note**: API keys sourced from environment variables only (never committed).

### Verification

Test MCP integration:

```bash
# List available Claude-Flow tools
claude mcp list

# Verify swarm_init, agent_spawn, memory_search are available
claude --help | grep -i claude-flow
```

---

## Usage Patterns

### When to Use Swarms vs Single Agents

**Use Swarms When:**
- Task spans 3+ domains (firmware + API + dashboard + docs)
- Multiple independent subtasks can run in parallel
- Quality gates require coordination (e.g., effect migration sequence)

**Use Single Agent When:**
- Task is domain-contained (e.g., add new dashboard component)
- Sequential ordering is strict (e.g., fix WiFiManager EventGroup bug)
- Experimental/exploratory work (no clear pattern yet)

### Pair Programming Modes

**Navigator Mode**: Use for high-risk changes
- Render loop modifications (no heap allocation verification)
- Protected file edits (WiFiManager, WebServer, AudioActor)
- Effect registration changes (ID stability requirements)

**TDD Mode**: Use when introducing new behaviour
- New IEffect implementations (test-first ensures centre-origin)
- REST endpoint additions (test defines API contract)
- Dashboard component development (accessibility test-first)

**Switch Mode**: Use for balanced collaboration
- General feature development
- Documentation updates with code changes

---

## Related Documentation

- **[agent-routing.md](./agent-routing.md)** - Detailed routing matrix and acceptance criteria
- **[swarm-templates.md](./swarm-templates.md)** - Reusable swarm configurations for common workflows
- **[pair-programming.md](./pair-programming.md)** - Pair programming guidance and mode selection
- **[validation-checklist.md](./validation-checklist.md)** - Setup verification and runtime checks

---

## Success Metrics

**Adoption Indicators:**
- Agent routing accuracy (tasks reach correct specialists)
- Pattern memory hit rate (reused patterns vs new searches)
- Swarm completion time (parallel vs sequential execution)
- Invariant violation rate (centre-origin, heap allocation, British English)

**Quality Indicators:**
- Protected file edit safety (no EventGroup bugs, no race conditions)
- Documentation consistency (British English, correct spelling)
- Effect validation pass rate (`validate <id>` checks)

---

*This integration transforms Lightwave-Ledstrip development from sequential domain work to coordinated multi-agent execution, preserving institutional knowledge through pattern memory and enforcing invariants through intelligent routing.*
