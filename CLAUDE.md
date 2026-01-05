# CLAUDE.md

**Protocol Version:** 2.0.0  
**Last Updated:** 2025-01-XX  
**Status:** Active

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What Is This Project?

**LightwaveOS** is an ESP32-S3 LED control system for dual 160-LED WS2812 strips (320 total). It features 45+ visual effects, multi-zone composition, and web-based control. The hardware is designed as a **Light Guide Plate (LGP)** where LEDs fire into an acrylic waveguide to create interference effects.

## Current Focus

- **Branch**: `main`
- **Active Work**: Security hardening, performance optimization, encoder support
- **Recent**: WiFi credentials externalized, CORS headers added, TrigLookup optimization, M5ROTATE8 encoder support

## First Steps for New Agents

1. Read this entire file
2. **MANDATORY:** Complete Pre-Task Agent Selection Protocol (see below)
3. Build: `pio run -e esp32dev_wifi -t upload`
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
- **Test with WiFi build**: `pio run -e esp32dev_wifi -t upload` for web interface testing

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
pio run -e esp32dev_wifi -t upload
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
1. Create function in `src/effects/strip/` (must be CENTER ORIGIN compliant)
2. Register in `effects[]` array in `main.cpp` (~line 202)
3. Use global `leds[]` buffer (320 LEDs total)
4. Test with serial menu (`e` command)

### Modify Web Interface
1. Edit files in `data/` (index.html, app.js, styles.css)
2. Build: `pio run -e esp32dev_wifi -t upload`
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

### LightwaveOS v2 (ESP32-S3)

```bash
# Default build (no WiFi)
pio run -t upload

# WiFi-enabled build
pio run -e esp32dev_wifi -t upload

# Memory debug build
pio run -e memory_debug -t upload

# Monitor serial (115200 baud)
pio device monitor -b 115200

# Clean build
pio run -t clean
```

### Tab5 Encoder (ESP32-P4) - CRITICAL BUILD REQUIREMENTS

**⚠️ Tab5 uses ESP32-P4 (RISC-V). It requires a SPECIFIC build invocation.**

#### ⛔ FORBIDDEN - These commands WILL FAIL:

```bash
# ❌ WRONG - Missing PATH isolation
pio run -e tab5 -d firmware/Tab5.encoder

# ❌ WRONG - Never cd into directory
cd firmware/Tab5.encoder && pio run -e tab5

# ❌ WRONG - Never add hardcoded toolchain paths
PATH="...:$HOME/.platformio/packages/toolchain-riscv32-esp/..." pio run ...

# ❌ WRONG - Never specify upload port manually
pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem21401 ...
```

#### ✅ CORRECT - Run from repository root:

```bash
# Build:
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -d firmware/Tab5.encoder

# Build + Upload (auto-detect port):
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -t upload -d firmware/Tab5.encoder

# Monitor:
pio device monitor -d firmware/Tab5.encoder -b 115200
```

**Why:** The pre-build hook (`scripts/pio_pre.py`) injects the RISC-V toolchain. A clean PATH lets it work correctly.

**See:** `firmware/Tab5.encoder/README.md` for full details.

## Hardware Configuration

- **ESP32-S3-DevKitC-1** @ 240MHz, 8MB flash
- **Dual WS2812 strips**: GPIO 4 (Strip 1), GPIO 5 (Strip 2)
- **160 LEDs per strip** = 320 total
- **Center point**: LED 79/80 (effects originate here)
- **Optional HMI**: M5ROTATE8 8-encoder unit via I2C (enable with `FEATURE_ROTATE8_ENCODER`)

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
│  45+ effects (strip/, lightguide/)   │
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
| `src/main.cpp` | Entry point, global state, main loop, effect registration |
| `src/config/features.h` | Compile-time feature flags |
| `src/config/hardware_config.h` | Pin definitions, LED counts, zone config |
| `src/config/network_config.h` | WiFi credentials, ports |
| `src/effects/effects.h` | Effect struct definition, extern declarations |
| `src/effects/strip/StripEffects.cpp` | Core CENTER ORIGIN effects |
| `src/effects/strip/LGP*.cpp` | Light Guide Plate physics effects |
| `src/effects/zones/ZoneComposer.h` | Multi-zone orchestration |
| `src/effects/transitions/TransitionEngine.h` | 12 transition types |
| `src/network/WebServer.cpp` | REST + WebSocket handlers |
| `data/app.js` | Web interface JavaScript |

## Effect Registration Pattern

Effects are parameterless void functions registered in `main.cpp`:

```cpp
// In main.cpp effects[] array
Effect effects[] = {
    {"Fire", fire, EFFECT_TYPE_STANDARD},
    {"Ocean", stripOcean, EFFECT_TYPE_STANDARD},
    // ... 45+ effects
};
```

Effects access global state: `leds[]`, `currentPalette`, `gHue`, `fadeAmount`, `brightnessVal`.

## Feature Flags

Set in `platformio.ini` or `src/config/features.h`:

| Flag | Purpose |
|------|---------|
| `FEATURE_WEB_SERVER` | Enable WiFi/WebServer (use `esp32dev_wifi` env) |
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

**Effects using these features:** BreathingEffect, LGPPhotonicCrystalEffect, LGPStarBurstNarrativeEffect (3 of 76 effects)

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
| `/api/v1/debug/memory/zones` | GET | Zone system memory stats (debug) |
| `/api/v1/audio/agc` | PUT | Toggle AGC (Automatic Gain Control) `{enabled: true/false}` |
| `/api/v1/network/status` | GET | Network status (AP/STA mode, IP addresses, RSSI) |
| `/api/v1/network/sta/enable` | POST | Enable STA mode with optional auto-revert `{durationSeconds: N, revertToApOnly: true}` |
| `/api/v1/network/ap/enable` | POST | Force AP-only mode |

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

**Audio Commands** (requires `esp32dev_audio` build):
- `audio.parameters.get` - Get audio pipeline tuning parameters (including `agcEnabled`)
- `audio.parameters.set` - Set audio pipeline parameters (including `agcEnabled`)
- `audio.zone-agc.get` - Get zone AGC state
- `audio.zone-agc.set` - Configure zone AGC parameters

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
- `net status` - Show WiFi status (AP/STA mode, IP addresses, RSSI)
- `net sta [seconds]` - Enable STA mode (optional auto-revert to AP-only after seconds)
- `net ap` - Force AP-only mode

## Dependencies

- `FastLED@3.10.0` - LED control with ESP32 RMT driver
- `ArduinoJson@7.0.4` - JSON parsing
- `ESPAsyncWebServer@3.6.0` - Async HTTP/WebSocket
- `AsyncTCP@3.4.4` - Async networking

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
pio run -e esp32dev_wifi            # Full build with WiFi
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
