# CLAUDE.md

**Protocol Version:** 2.1.0
**Last Updated:** 2026-01-28
**Status:** Active

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What Is This Project?

**LightwaveOS** is an ESP32-S3 LED control system for dual 160-LED WS2812 strips (320 total). It features 100+ visual effects, multi-zone composition, audio-reactive rendering, and web-based control. The hardware is designed as a **Light Guide Plate (LGP)** where LEDs fire into an acrylic waveguide to create interference effects.

## Current Focus

- **Primary Build**: `pio run -e esp32dev_audio` (WiFi, audio, web server all included)
- **Active Work**: WiFi fallback hardening, Tab5 encoder improvements, effect expansion
- **Recent**: AP-mode fallback on scan failure, centralized network config, LGPHolographicAutoCycleEffect, thread-safe HttpClient discovery, local WiFi scanning on Tab5

## First Steps for New Agents

1. Read this entire file
2. **MANDATORY:** Complete Pre-Task Agent Selection Protocol (see below)
3. Build: `pio run -e esp32dev_audio -t upload`
4. Access web UI at `http://lightwaveos.local`

---

## MANDATORY: Pre-Task Agent Selection Protocol

**⚠️ CRITICAL REQUIREMENT:** Before initiating ANY research, debugging, troubleshooting, or implementation task, you MUST complete this protocol.

### Step 1: Review Complete Agent Inventory

**MANDATORY ACTION:** Read `.claude/agents/README.md` to review the complete inventory of available specialist agents.

**Required Information:**
- List of all available agents (currently 10 specialist agents)
- Domain expertise of each agent
- Capability matrix and selection guidance

**Verification:** You must be able to name all available agents and their primary domains before proceeding.

### Step 2: Analyze Domain Expertise

**MANDATORY ACTION:** For each available agent, analyze:
- Primary domain expertise
- Secondary domain capabilities
- Complexity level (High/Medium/Low)
- When to select this agent

**Reference:** Use `.claude/agents/README.md` as the authoritative source for agent capabilities.

### Step 3: Strategically Select Optimal Agent Combination

**MANDATORY ACTION:** Based on your task analysis:

1. **Identify task domain(s):**
   - Hardware/firmware → `embedded-system-engineer`
   - Visual effects → `visual-fx-architect`
   - Network/API → `network-api-engineer`
   - Serial interface → `serial-interface-engineer`
   - Color/palettes → `palette-specialist`
   - Web development → `agent-nextjs`, `agent-lvgl-uiux`
   - Backend → `agent-convex`, `agent-vercel`, `agent-clerk`

2. **Determine if multiple agents are needed:**
   - Single domain task → One primary agent
   - Multi-domain task → Multiple agents (see coordination patterns)
   - Complex integration → Sequential or hybrid coordination

3. **Select agent combination:**
   - Use `.claude/agents/README.md` decision matrix
   - Consider parallel execution potential (see Step 4)
   - Document your selection rationale

### Step 4: Evaluate Parallel Execution Potential

**MANDATORY ACTION:** For complex tasks, evaluate if parallel execution is beneficial:

**Parallel Execution Criteria:**
- ✅ Task can be divided into independent components
- ✅ Multiple agents can work on different subsystems simultaneously
- ✅ No shared state conflicts
- ✅ Different file sets or test files

**If parallel execution is possible:**
- **MUST** use `.claude/skills/dispatching-parallel-agents/` skill
- **MUST** divide task into parallelizable components
- **MUST** deploy multiple agents simultaneously

**If sequential execution is required:**
- Tasks are tightly coupled
- Shared state dependencies exist
- Ordered execution is necessary

### Step 5: Review Available Skills

**MANDATORY ACTION:** Check `.claude/skills/` directory for relevant skills:
- `dispatching-parallel-agents` - For parallel execution
- `subagent-driven-development` - For implementation plans
- `test-driven-development` - For TDD workflows
- `software-architecture` - For architectural decisions
- Plus 24+ additional specialized skills

**Reference:** `.claude/skills/` contains 28+ specialized skills for specific tasks.

### Protocol Compliance

**This protocol applies to ALL task types:**
- ✅ Research tasks
- ✅ Debugging tasks
- ✅ Troubleshooting tasks
- ✅ Code changes
- ✅ Feature development
- ✅ System modifications
- ✅ Architectural updates

**Non-compliance is not acceptable.** Skipping this protocol leads to suboptimal agent selection and reduced efficiency.

---

## Claude Code Resources

### Skills, Agents, and Plans
- **`.claude/skills/`** - 28 specialized skills for specific tasks
  - **MANDATORY:** Check for relevant skills before implementing features
  - Key skills: `test-driven-development`, `software-architecture`, `brainstorming`, `finishing-a-development-branch`, `dispatching-parallel-agents`, `subagent-driven-development`
- **`.claude/agents/`** - Custom agent personas
  - **MANDATORY:** Review `.claude/agents/README.md` for complete inventory
  - **MANDATORY:** Invoke agents using Task tool when their expertise matches
  - **MANDATORY:** Use strategic selection based on domain expertise analysis
- **`.claude/plans/`** - Implementation blueprints
  - Reference existing plans when starting features

**CRITICAL:** The Pre-Task Agent Selection Protocol above is MANDATORY for all tasks. Always complete Steps 1-5 before starting work.

### Best Practices
- **Modular Code**: Write focused, reusable code to optimize token usage
- **UI-First for Web**: When modifying `data/` web interface, build UI first, then add functionality
- **Test build**: `pio run -e esp32dev_audio -t upload` (primary build, includes WiFi/audio/web)

### Command Centre — Claude-Flow Integration

For comprehensive AI agent orchestration, multi-agent coordination, and pair programming modes:

→ **[docs/operations/claude-flow/](docs/operations/claude-flow/README.md)** — Claude-Flow Strategic Integration

**Quick Start (No API Keys Required):**
```bash
# Verify Claude-Flow is available
npx claude-flow@v3alpha doctor

# Check MCP tools (27+ orchestration tools)
npx claude-flow@v3alpha mcp tools
```

**Configuration:** Add to `~/.claude/settings.json`:
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

**Quick Links:**
- [MCP Setup Guide](docs/operations/claude-flow/MCP_SETUP.md) — Configuration for Claude Code, Cursor, Claude Desktop
- [Agent Routing Matrix](docs/operations/claude-flow/agent-routing.md) — Domain-to-agent mapping
- [Swarm Templates](docs/operations/claude-flow/swarm-templates.md) — Reusable multi-agent workflows
- [Pair Programming Modes](docs/operations/claude-flow/pair-programming.md) — Navigator, TDD, Switch modes
- [Validation Checklist](docs/operations/claude-flow/validation-checklist.md) — Setup and runtime verification

**Claude-Code Provider Plugin:** For standalone Claude-Flow daemon mode without API keys, see [plugins/claude-code-provider/](plugins/claude-code-provider/README.md)

### LLM Context and Prompting

For efficient LLM interactions and token-optimised prompting:

→ **[docs/LLM_CONTEXT.md](docs/LLM_CONTEXT.md)** — Stable LLM prefix file (include instead of re-explaining the project)

**Quick Links:**
- [Context Pack Pipeline](docs/contextpack/README.md) — Delta-only prompting discipline
- [Packet Template](docs/contextpack/packet.md) — Prompt packet template (goal, symptom, acceptance checks)
- [Fixture Formats](docs/contextpack/fixtures/README.md) — TOON/CSV/JSON format selection guide
- [TOON Evaluation](docs/architecture/TOON_FORMAT_EVALUATION.md) — Prompt codec decision document

**Generator Script:** `python tools/contextpack.py` — Generates prompt bundles with diff, logs, and fixtures.

---

## Critical Constraints

**NO RAINBOWS** - Effects using rainbow color cycling will be rejected.

**CENTER ORIGIN ONLY** - ALL effects MUST originate from LED 79/80 (center point):
- Propagate OUTWARD from center (79/80) to edges (0/159)
- OR propagate INWARD from edges (0/159) to center (79/80)
- NO linear left-to-right or right-to-left patterns

**AUDIO-REACTIVE EFFECTS: Mandatory Protocol**

Before adding ANY audio-reactive features, agents MUST read:
**[docs/audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md](docs/audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md)**

Key requirements:
1. **Complete Layer Audit Protocol** - understand ALL layers of the effect
2. **NO rigid frequency→visual bindings** - "bass→expansion" is a TRAP
3. **Musical saliency analysis** - respond to what's IMPORTANT, not all signals
4. **Style-adaptive response** - EDM/vocal/ambient need different strategies
5. **Temporal context** - use history, not just instantaneous values

**Frequency spectrum isolation is AMATEUR DSP. Musical intelligence has evolved past it.**

---

## Protected Files - CRITICAL CODE

**⚠️ STOP: These files contain critical infrastructure that has caused recurring bugs when modified carelessly.**

Before modifying ANY protected file, agents MUST:
1. **Read the ENTIRE file** and understand the state machine/architecture
2. **Read `.claude/harness/PROTECTED_FILES.md`** for known landmines and required tests
3. **Run all related unit tests** before AND after changes
4. **Understand FreeRTOS synchronization** if the file uses mutexes/semaphores/event groups

### WiFiManager (v2/src/network/WiFiManager.*)

**CRITICALITY: 🔴 CRITICAL**

**LANDMINE WARNING:** FreeRTOS EventGroup bits **persist across interrupted connections**. This has caused the "Connected! IP: 0.0.0.0" bug MULTIPLE TIMES.

**MANDATORY RULES:**
- `setState()` **MUST** call `xEventGroupClearBits()` when entering `STATE_WIFI_CONNECTING`
- Clear bits: `EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED`
- All state transitions **MUST** be mutex-protected via `m_stateMutex`
- Never assume EventGroup bits are clean - they persist!

**The Fix (lines 499-505 in WiFiManager.cpp) is LOAD-BEARING:**
```cpp
if (newState == STATE_WIFI_CONNECTING) {
    // CRITICAL FIX: Clear stale event bits
    if (m_wifiEventGroup) {
        xEventGroupClearBits(m_wifiEventGroup,
            EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED);
    }
}
```

**Required Test Before Commit:**
```bash
pio run -e esp32dev_audio -t upload
# Then verify: WiFi connects WITHOUT "IP: 0.0.0.0" appearing first
```

**Why This Matters:** Stale EventGroup bits cause false-positive "Connected" states that waste HOURS of debugging. This bug has resurfaced 3+ times.

### WebServer (v2/src/network/WebServer.*)

**CRITICALITY: 🟠 HIGH**

- Rate limiting state must be thread-safe
- WebSocket handlers run on different task than HTTP handlers
- AsyncWebServer callbacks are NOT synchronized - use mutexes

---

## Design Philosophy

- **CENTER ORIGIN**: Effects radiate from LED 79/80 because the LGP creates interference patterns - edge-originating effects look wrong on this hardware
- **Optional HMI**: M5ROTATE8 encoder support available (`src/hardware/EncoderManager.cpp`), but primary control is web/serial
- **Performance**: Target 120 FPS, minimize heap allocations in render loops
- **Dual-Strip Symmetry**: Strip 1 and Strip 2 should complement each other

## How To: Common Tasks

### Add a New Effect
1. Create an `IEffect` subclass in `src/effects/ieffect/` (must be CENTER ORIGIN compliant)
2. Register in `registerAllEffects()` in `src/effects/CoreEffects.cpp`
3. Update `EXPECTED_EFFECT_COUNT` in CoreEffects.cpp
4. Bump `MAX_EFFECTS` if needed in RendererActor.h, SystemState.h, ZoneConfigManager.h, AudioEffectMapping.h, RequestValidator.h
5. Test with serial menu (`e` command) or web UI

### Modify Web Interface
1. Edit files in `data/` (index.html, app.js, styles.css)
2. Build: `pio run -e esp32dev_audio -t upload`
3. Access: `http://lightwaveos.local`

### Add a Zone Effect
1. Effect must work with buffer proxy (renders to temp buffer)
2. Zone system calls effect function, then composites
3. Don't assume LED indices - use provided buffer size

## Avoid These (Past Mistakes)

- Linear left-to-right effects (violates CENTER ORIGIN)
- Rainbow color cycling (explicitly forbidden)
- `new`/`malloc` in render loops (causes frame drops)
- Assuming `leds[]` size - always use 320 or constants
- Hardcoded WiFi credentials in code (use network_config.h)

---

## Build Commands

```bash
# Primary build (WiFi + audio + web server)
pio run -e esp32dev_audio -t upload

# Audio benchmark build (per-hop timing instrumentation)
pio run -e esp32dev_audio_benchmark -t upload

# Audio trace build (Perfetto timeline visualization)
pio run -e esp32dev_audio_trace -t upload

# Secondary board build (alternate GPIO mapping)
pio run -e esp32dev_SSB -t upload

# Monitor serial (115200 baud)
pio device monitor -b 115200

# Clean build
pio run -e esp32dev_audio -t clean

# OTA firmware update (K1 or any networked device)
# Via Web UI: http://lightwaveos.local → OTA Update tab
# Via curl:
curl -X POST http://lightwaveos.local/api/v1/firmware/update \
     -H "X-OTA-Token: <device-token>" \
     -F "firmware=@.pio/build/esp32dev_audio/firmware.bin"

# OTA web assets update (LittleFS)
pio run -e esp32dev_audio -t buildfs
curl -X POST http://lightwaveos.local/api/v1/firmware/filesystem \
     -H "X-OTA-Token: <device-token>" \
     -F "filesystem=@.pio/build/esp32dev_audio/littlefs.bin"

# Retrieve device OTA token
curl http://lightwaveos.local/api/v1/device/ota-token
```

## Hardware Configuration

- **ESP32-S3-DevKitC-1** @ 240MHz, 8MB flash
- **Dual WS2812 strips**: GPIO 4 (Strip 1), GPIO 5 (Strip 2)
- **160 LEDs per strip** = 320 total
- **Center point**: LED 79/80 (effects originate here)
- **Optional HMI**: M5ROTATE8 8-encoder unit via I2C (enable with `FEATURE_ROTATE8_ENCODER`)

### Hardware Deployments

| Device | MAC | IP (typical) | USB Access | Update Method |
|--------|-----|--------------|------------|---------------|
| **K1 Prototype** | `b4:3a:45:a5:87:f8` | `192.168.1.101` | Requires disassembly | **OTA only** |
| **Dev Board** (bench test) | — | — | USB-C available | USB or OTA |

**K1 Prototype:** The K1 is a sealed LGP assembly. Once assembled, the ESP32-S3 USB-C port is physically inaccessible without disassembly. All firmware and web asset updates **must** use OTA via `http://lightwaveos.local` or direct IP. The K1 runs firmware v2.0.0+ with full OTA support (boot rollback, MD5 verification, per-device auth token, LED progress feedback).

**Dev Board:** The regular esp32dev_audio test setup remains available on the workbench with direct USB-C access for `pio run -t upload` and serial monitoring.

## Architecture Overview

```
┌──────────────────────────────────────┐
│  Web Interface (data/*.html/js/css)  │
│  WebSocket + REST API                │
└──────────────────────────────────────┘
         ↓
┌──────────────────────────────────────┐
│  WebServer (src/network/)            │
│  REST: /api/brightness, effect, etc  │
│  WS: zone, effect, parameter updates │
└──────────────────────────────────────┘
         ↓
┌──────────────────────────────────────┐
│  Zone Composer (src/effects/zones/)  │
│  Up to 4 independent zones           │
│  Buffer proxy for zone isolation     │
└──────────────────────────────────────┘
         ↓
┌──────────────────────────────────────┐
│  Effect System (src/effects/)        │
│  100+ effects (ieffect/)             │
│  TransitionEngine (12 types)         │
└──────────────────────────────────────┘
         ↓
┌──────────────────────────────────────┐
│  LED Buffers                         │
│  leds[320] - unified effect buffer   │
│  strip1[160], strip2[160] - hardware │
└──────────────────────────────────────┘
         ↓
┌──────────────────────────────────────┐
│  FastLED + RMT Driver                │
│  WS2812 @ 800kHz, ~9.6ms/frame       │
└──────────────────────────────────────┘
```

## Key Source Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, actor system bootstrap, main loop |
| `src/effects/CoreEffects.cpp` | Effect registration (101 effects via IEffect) |
| `src/effects/ieffect/*.cpp` | Individual IEffect implementations |
| `src/config/features.h` | Compile-time feature flags |
| `src/config/hardware_config.h` | Pin definitions, LED counts, zone config |
| `src/config/network_config.h` | WiFi credentials, ports, timing constants |
| `src/core/actors/RendererActor.h` | Render pipeline, effect management |
| `src/effects/zones/ZoneComposer.h` | Multi-zone orchestration |
| `src/effects/transitions/TransitionEngine.h` | 12 transition types |
| `src/network/WebServer.cpp` | REST + WebSocket handlers |
| `src/network/WiFiManager.cpp` | WiFi state machine with AP fallback |
| `data/app.js` | Web interface JavaScript |

## Effect Registration Pattern

Effects implement the `IEffect` interface and are registered in `CoreEffects.cpp`:

```cpp
// In src/effects/CoreEffects.cpp — registerAllEffects()
static ieffect::MyNewEffect myNewEffectInstance;
if (renderer->registerEffect(total, &myNewEffectInstance)) {
    total++;
}
```

Each effect receives an `EffectContext` with access to LED buffers, timing, palette, audio data, and parameters. See existing effects in `src/effects/ieffect/` for examples.

## Feature Flags

Set in `platformio.ini` or `src/config/features.h`:

| Flag | Purpose |
|------|---------|
| `FEATURE_WEB_SERVER` | WiFi/WebServer (enabled by default in common flags) |
| `FEATURE_SERIAL_MENU` | Serial command interface |
| `FEATURE_PERFORMANCE_MONITOR` | FPS/memory tracking |
| `FEATURE_INTERFERENCE_CALC` | Wave physics calculations |
| `FEATURE_PHYSICS_SIMULATION` | Physics-based effects |

### Musical Intelligence Feature Flags (v2 Only)

These features compute musical context for audio-reactive effects. They default to matching `FEATURE_AUDIO_SYNC`.

| Flag | Default | CPU Cost | Purpose |
|------|---------|----------|---------|
| `FEATURE_MUSICAL_SALIENCY` | `FEATURE_AUDIO_SYNC` | ~80 µs/hop | Computes harmonic/rhythmic/timbral/dynamic novelty |
| `FEATURE_STYLE_DETECTION` | `FEATURE_AUDIO_SYNC` | ~60 µs/hop | Classifies music as rhythmic/harmonic/melodic/texture/dynamic |

**To disable:** Set to 0 in `v2/src/config/features.h` or via `-D` flag in platformio.ini.

**When to disable:** If no effects use `ctx.audio.harmonicSaliency()`, `ctx.audio.musicStyle()`, etc.

**Effects using these features:** BreathingEffect, LGPPhotonicCrystalEffect, LGPStarBurstNarrativeEffect (3 of 101 effects)

## Web API

### API v1 (Recommended)

The v1 API provides standardized responses, rate limiting, and rich metadata.

**Base URL**: `http://lightwaveos.local/api/v1/`

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/` | GET | API discovery with HATEOAS links |
| `/api/v1/openapi.json` | GET | OpenAPI 3.0 specification |
| `/api/v1/device/status` | GET | Uptime, heap, network status |
| `/api/v1/device/info` | GET | Firmware, SDK, flash info |
| `/api/v1/effects` | GET | Paginated effects with categories |
| `/api/v1/effects/current` | GET | Current effect state |
| `/api/v1/effects/metadata?id=N` | GET | Effect metadata by ID |
| `/api/v1/effects/set` | POST | Set effect `{effectId: N}` |
| `/api/v1/parameters` | GET/POST | Visual parameters |
| `/api/v1/transitions/types` | GET | 12 transition types |
| `/api/v1/transitions/trigger` | POST | Trigger transition |
| `/api/v1/batch` | POST | Batch operations (max 10) |

**Response Format**:
```json
{"success": true, "data": {...}, "timestamp": 12345, "version": "1.0"}
```

**Rate Limiting**: 20 req/sec HTTP, 50 msg/sec WebSocket. Exceeding returns 429.

**Full Documentation**: [docs/api/API_V1.md](docs/api/API_V1.md)

### Legacy API (Backward Compatible)

```
POST /api/effect      - Set effect by ID
POST /api/brightness  - Set brightness (0-255)
POST /api/speed       - Set animation speed (1-50)
POST /api/palette     - Select palette by ID
POST /api/zone/count  - Set zone count (1-4)
POST /api/zone/effect - Set zone-specific effect
```

### WebSocket Commands

Connect to `ws://lightwaveos.local/ws`

**v1 Commands**: `transition.trigger`, `effects.getMetadata`, `batch`, `parameters.get`

**Legacy Commands**: `setEffect`, `setBrightness`, `setZoneEffect`, etc.

## Zone System

The Zone Composer (`src/effects/zones/`) provides independent effect control:

- Up to 4 zones (40 LEDs each, symmetric around center)
- Buffer proxy pattern: each zone renders to temp buffer then composites
- NVS persistence for zone configurations
- 5 built-in presets: Single, Dual, Triple, Quad, Alternating

## Serial Commands

Connect at 115200 baud. Key commands:
- `e` - Effects menu
- `p` - Palette selection
- `b <0-255>` - Set brightness
- `s <1-50>` - Set speed
- `h` - Help menu

### Debug System (Unified)

Per-domain debug control with on-demand diagnostics. **Full docs:** [firmware/v2/docs/debugging/DEBUG_SYSTEM.md](firmware/v2/docs/debugging/DEBUG_SYSTEM.md)

```
dbg                  - Show current debug config
dbg <0-5>            - Set global level (0=OFF, 2=WARN default, 5=TRACE)
dbg audio <0-5>      - Set audio domain level
dbg render <0-5>     - Set render domain level
dbg status           - Print audio health NOW (one-shot)
dbg spectrum         - Print 64-bin FFT NOW (one-shot)
dbg memory           - Print heap/stack NOW (one-shot)
dbg interval status <N>    - Auto-print every N seconds (0=off)
```

**Legacy `adbg` still works:** `adbg` / `adbg <0-5>` / `adbg status` equivalent to `dbg audio`.

## Dependencies

- `FastLED@3.10.0` - LED control with ESP32 RMT driver
- `ArduinoJson@7.0.4` - JSON parsing
- `ESPAsyncWebServer@3.9.3` - Async HTTP/WebSocket
- `AsyncTCP@3.4.9` - Async networking

---

## Domain Memory Harness (for Autonomous Agents)

This project includes a domain memory harness for persistent agent progress across sessions.

**Location**: `.claude/harness/`

### Harness Files

| File | Purpose | Read First? |
|------|---------|-------------|
| `.claude/harness/HARNESS_RULES.md` | **Protocol rules** - mutation, validation, revert | Yes |
| `ARCHITECTURE.md` | Extracted system facts for quick reference | Yes |
| `CONSTRAINTS.md` | Hard limits (timing, memory, power) | Yes |
| `.claude/harness/feature_list.json` | Structured backlog with attempts[] history | Yes |
| `.claude/harness/agent-progress.md` | Run history + Lessons Learned section | Yes |
| `.claude/harness/init.sh` | Boot ritual - verifies project health | Run it |

### WORKER MODE Ritual

**Read `.claude/harness/HARNESS_RULES.md` for complete protocol.** Summary:

1. **Boot**: Run `.claude/harness/init.sh` - if fails, fix that first
2. **Read memory**:
   - `.claude/harness/agent-progress.md` (Lessons Learned + last 3 runs)
   - `.claude/harness/feature_list.json` (backlog + attempts history)
   - `ARCHITECTURE.md` and `CONSTRAINTS.md` (if touching code)
3. **Select ONE item**: Highest priority FAILING, warn if dependencies FAILING
4. **Implement**: Follow constraints, use CENTER ORIGIN for effects
5. **Record attempt**: Add to `attempts[]` array regardless of result
6. **On success**: Update status to PASSING with evidence, commit
7. **On failure**: Keep status FAILING, revert code (not harness), log investigation
8. **Stop**: One item per run

### Verification Commands
```bash
.claude/harness/init.sh             # Boot ritual
pio run -e esp32dev_audio            # Full build with WiFi
git status --porcelain | wc -l      # Check uncommitted changes
```

### Status Values
- `FAILING` - Not yet done or verification failed
- `PASSING` - Done and verified (requires evidence)
- `BLOCKED` - External blocker or max attempts reached (requires reason)
- `CANCELLED` - No longer needed (requires reason)

### Escape Hatches
- `override_reason` field in .claude/harness/feature_list.json items
- `--force` flags in harness.py (when implemented)
- Document why if bypassing rules
