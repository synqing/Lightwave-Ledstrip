# Agents & Tools

> **When to read this:** When selecting specialist agents for a task, or when using the domain memory harness in worker mode.

## Specialist Agents

Full inventory and selection matrix: [.claude/agents/README.md](../../.claude/agents/README.md)

| Agent | Domain |
|-------|--------|
| `embedded-system-engineer` | ESP32, FreeRTOS, GPIO, memory |
| `visual-fx-architect` | Effects, zones, transitions, palettes |
| `network-api-engineer` | REST, WebSocket, WiFi |
| `serial-interface-engineer` | Serial, telemetry, debug |
| `palette-specialist` | Colour science, palette design |
| `agent-lvgl-uiux` | LVGL embedded UI |
| `m5gfx-dashboard-architect` | M5GFX displays, Tab5 |
| `agent-nextjs` | Next.js, React |
| `agent-convex` | Convex backend |
| `agent-vercel` | Deployment, CI/CD |
| `agent-clerk` | Authentication |

## Skills

29 skills in `.claude/skills/`. Key ones:

- `test-driven-development` -- TDD workflow
- `software-architecture` -- Clean architecture, DDD
- `subagent-driven-development` -- Plan execution with sub-agents
- `dispatching-parallel-agents` -- Parallel task execution
- `finishing-a-development-branch` -- Merge/PR decisions

## Domain Memory Harness (Worker Mode)

Full protocol: [.claude/harness/HARNESS_RULES.md](../../.claude/harness/HARNESS_RULES.md)

**Summary:** Boot (`init.sh`) -> Read memory (`agent-progress.md`, `feature_list.json`) -> Select ONE item -> Implement -> Record attempt -> Update status -> Stop.

Harness files live in `.claude/harness/`.
