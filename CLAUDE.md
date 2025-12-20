# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What Is This Project?

**LightwaveOS** is an ESP32-S3 LED control system for dual 160-LED WS2812 strips (320 total). It features 45+ visual effects, multi-zone composition, and web-based control. The hardware is designed as a **Light Guide Plate (LGP)** where LEDs fire into an acrylic waveguide to create interference effects.

## Current Focus

- **Branch**: `main`
- **Active Work**: Security hardening, performance optimization, encoder support
- **Recent**: WiFi credentials externalized, CORS headers added, TrigLookup optimization, M5ROTATE8 encoder support

## First Steps for New Agents

1. Read this entire file
2. Check `.claude/skills/` for relevant skills before coding
3. Build: `pio run -e esp32dev_wifi -t upload`
4. Access web UI at `http://lightwaveos.local`

---

## Claude Code Resources

### Skills, Agents, and Plans
- **`.claude/skills/`** - 28 specialized skills for specific tasks
  - Check for relevant skills before implementing features
  - Key skills: `test-driven-development`, `software-architecture`, `brainstorming`, `finishing-a-development-branch`
- **`.claude/agents/`** - Custom agent personas
  - Invoke agents using Task tool when their expertise matches
- **`.claude/plans/`** - Implementation blueprints
  - Reference existing plans when starting features

**IMPORTANT**: Always check these directories when starting a new feature or task.

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
