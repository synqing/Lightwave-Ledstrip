# Agent Guide (Lightwave-Ledstrip / LightwaveOS)

This repository contains firmware for an ESP32-S3 driving a dual-strip Light Guide Plate (LGP) system, plus a web dashboard. This file provides a quick reference for agents working in this codebase.

## Quick Navigation

| I need to... | Read this |
|--------------|-----------|
| Understand the project | [CLAUDE.md](CLAUDE.md) - **MANDATORY** |
| See agent inventory | [.claude/agents/README.md](.claude/agents/README.md) |
| Browse available skills | [.claude/skills/](.claude/skills/) (30 skills) |
| Work in WORKER MODE | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) |
| Check constraints | See "Non-Negotiable Invariants" below |

---

## MANDATORY: Pre-Task Agent Selection Protocol

**Before ANY task, complete the 5-step protocol in [CLAUDE.md](CLAUDE.md):**

1. Review agent inventory -> `.claude/agents/README.md`
2. Analyse domain expertise of each agent
3. Select optimal agent combination
4. Evaluate parallel execution potential
5. Review available skills -> `.claude/skills/`

**Non-compliance is not acceptable.**

---

## Specialist Agents (11 Available)

Full details: [.claude/agents/README.md](.claude/agents/README.md)

| Agent | Domain | When to Use |
|-------|--------|-------------|
| `embedded-system-engineer` | ESP32, FreeRTOS, GPIO | Hardware, threading, memory |
| `visual-fx-architect` | Effects, Zones, Transitions | Visual rendering, CENTRE ORIGIN |
| `network-api-engineer` | REST, WebSocket, WiFi | API, networking |
| `serial-interface-engineer` | Serial, telemetry | Debug, commands |
| `palette-specialist` | Colour science | Palettes, power management |
| `agent-lvgl-uiux` | LVGL embedded UI | Touch interfaces |
| `m5gfx-dashboard-architect` | M5GFX displays | Tab5, dashboards |
| `agent-nextjs` | Next.js, React | Web frontend |
| `agent-convex` | Convex backend | Database, storage |
| `agent-vercel` | Deployment | CI/CD, production |
| `agent-clerk` | Authentication | Auth integration |

### Domain Routing Quick Reference

```
Hardware/firmware issue    -> embedded-system-engineer
Visual effect bug/feature  -> visual-fx-architect
API/WebSocket issue        -> network-api-engineer
Serial debug/telemetry     -> serial-interface-engineer
Colour/palette work        -> palette-specialist
Tab5 LVGL UI               -> agent-lvgl-uiux
Tab5 M5GFX display         -> m5gfx-dashboard-architect
Web dashboard              -> agent-nextjs
Backend/database           -> agent-convex
Deployment/CI              -> agent-vercel
Auth integration           -> agent-clerk
```

---

## Available Skills (30)

Browse: `.claude/skills/`

**Core (MANDATORY for complex work):**
- `test-driven-development` - TDD workflow
- `software-architecture` - Clean Architecture, DDD
- `subagent-driven-development` - Plan execution
- `dispatching-parallel-agents` - Parallel tasks
- `finishing-a-development-branch` - Merge/PR decisions
- `ralph-loop` - Autonomous feature implementation

**Specialised:** `brainstorming`, `esp32-rust-embedded`, `playwright-skill`, `front-end-design`, `Changelog-generator`, + 24 more

---

## Domain Memory Harness (WORKER MODE)

Full protocol: [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md)

**Files:**
- `init.sh` - Boot ritual (run first)
- `feature_list.json` - Backlog with attempts[]
- `agent-progress.md` - Run log + Lessons Learned
- `HARNESS_RULES.md` - Authoritative protocol

**WORKER MODE ritual:** Boot -> Read memory -> Select ONE item -> Implement -> Record attempt -> Update status -> Stop

---

## Slash Commands (14)

Browse: `.claude/commands/`

Key commands: `/debug`, `/new-effect`, `/write-tests`, `/plan`, `/start-session`, `/recall`, `/track`, `/analyze-code`

---

## Protected Files (CRITICAL)

See [.claude/harness/PROTECTED_FILES.md](.claude/harness/PROTECTED_FILES.md)

| File | Risk | Key Landmine |
|------|------|--------------|
| `WiFiManager.*` | CRITICAL | FreeRTOS EventGroup bits persist - must clear on STATE_WIFI_CONNECTING |
| `WebServer.*` | HIGH | Thread safety - network handlers on Core 0, renderer on Core 1 |

---

## Non-Negotiable Invariants

- **Centre origin only**: All effects must originate from the centre LEDs **79/80** and propagate **outward** (or converge inward to the centre). Do not implement linear 0->159 sweeps.
- **No rainbows**: Do not introduce rainbow cycling / full hue-wheel sweeps.
- **No heap allocation in render loops**: No `new`, `malloc`, `std::vector` growth, `String` concatenations, or other dynamic allocation in `render()` hot paths.
- **Performance matters**: Rendering runs at high frame rate (target 120 FPS, often higher). Keep per-frame work predictable.
- **British English**: Use British spelling in comments and documentation (centre, colour, initialise, etc.).

---

## Where Things Live

- **v2 firmware**: `firmware/v2/src/` (actor system, zones, IEffect plugins, web server)
- **Encoder firmware**: `firmware/K1.8encoderS3/` (M5Stack AtomS3 with 8-encoder control)
- **Tab5 Encoder**: `firmware/Tab5.encoder/` (LVGL UI controller)
- **Effects**:
  - Legacy function effects: `firmware/v2/src/effects/*.cpp`
  - Native IEffect effects: `firmware/v2/src/effects/ieffect/*.h|*.cpp`
- **Effect runtime API**: `firmware/v2/src/plugins/api/` (`IEffect.h`, `EffectContext.h`)
- **Renderer / render loop**: `firmware/v2/src/core/actors/RendererActor.*`
- **LED topology / centre config**: `firmware/v2/src/core/actors/RendererActor.h` (`LedConfig`)
- **Zones**: `firmware/v2/src/effects/zones/ZoneComposer.*`
- **Web server (REST + WS)**: `firmware/v2/src/network/WebServer.*`
- **Dashboard (web app)**: `lightwave-dashboard/`

---

## Build / Flash / Monitor (PlatformIO)

### LightwaveOS Build Commands

```bash
# Build (default, audio-enabled):
cd firmware/v2 && pio run -e esp32dev_audio

# Build + upload:
cd firmware/v2 && pio run -e esp32dev_audio -t upload

# Serial monitor:
pio device monitor -b 115200
```

### Tab5 Encoder Build Commands

**CRITICAL: Tab5 (ESP32-P4/RISC-V) requires a SPECIFIC build invocation.**

```bash
# CORRECT - Run from repository root with clean PATH:
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -d firmware/Tab5.encoder

# Build + Upload (auto-detect port):
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -t upload -d firmware/Tab5.encoder

# Serial monitor:
pio device monitor -d firmware/Tab5.encoder -b 115200
```

**NEVER** `cd` into the directory, add hardcoded toolchain paths, or specify upload port manually.

---

## Serial Commands

Connect at 115200 baud. Key commands:
- `e` - Effects menu
- `p` - Palette selection
- `b <0-255>` - Set brightness
- `s <1-50>` - Set speed
- `h` - Help menu
- `l` - List effects
- `net status` - Show WiFi status
- `validate <id>` - Run effect validation

---

## Common Pitfalls

- **Palette staleness**: Do not cache palette pointers across frames.
- **Centre mapping errors**: Centre is between LEDs 79 and 80. Be careful with integer rounding.
- **ZoneComposer buffer sizes**: ZoneComposer renders to a temp buffer; avoid assuming global `ledCount`.
- **Colour correction sensitivity**: Check `PatternRegistry::isLGPSensitive()` for interference effects.
- **Build paths**: Follow the exact commands above for each project.

---

## See Also

- [CLAUDE.md](CLAUDE.md) - Main project instructions
- [.claude/agents/README.md](.claude/agents/README.md) - Full agent inventory
- [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) - Harness protocol
- [docs/api/API_V1.md](docs/api/API_V1.md) - REST API documentation
