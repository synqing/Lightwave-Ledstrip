# Codex Agents Guide (Lightwave-Ledstrip / LightwaveOS)

This repository contains firmware for an ESP32-S3 driving a dual-strip Light Guide Plate (LGP) system, plus a web dashboard. This file is written for automated coding agents (Codex Agents) to work productively and safely in this codebase.

---

## Quick Start for Agents

| I need to... | Read this first |
|--------------|-----------------|
| Understand the project | [docs/LLM_CONTEXT.md](docs/LLM_CONTEXT.md) — Stable LLM prefix |
| Follow Claude Code rules | [CLAUDE.md](CLAUDE.md) — **MANDATORY** |
| Prepare efficient prompts | [docs/contextpack/README.md](docs/contextpack/README.md) — Delta-only prompting |
| Find agent inventory | [.claude/agents/README.md](.claude/agents/README.md) — Specialist agents |
| Check constraints | [constraints.md](constraints.md) — Hard limits |
| Check architecture | [architecture.md](architecture.md) — Quick reference |

---

## Non-Negotiable Invariants

- **Centre origin only**: All effects must originate from the centre LEDs **79/80** and propagate **outward** (or converge inward to the centre). Do not implement linear 0→159 sweeps.
- **No rainbows**: Do not introduce rainbow cycling / full hue-wheel sweeps.
- **No heap allocation in render loops**: No `new`, `malloc`, `std::vector` growth, `String` concatenations, or other dynamic allocation in `render()` hot paths.
- **Performance matters**: Rendering runs at high frame rate (target 120 FPS, often higher). Keep per-frame work predictable.
- **British English**: Use British spelling in comments and documentation (centre, colour, initialise, etc.).

---

## Where Things Live

| Purpose | Path |
|---------|------|
| **v2 firmware** | `firmware/v2/src/` (actor system, zones, IEffect plugins, web server) |
| **Encoder firmware** | `firmware/Tab5.encoder/` (M5Stack Tab5 with encoder control) |
| **Effects (IEffect)** | `firmware/v2/src/effects/ieffect/*.h|*.cpp` |
| **Effects (legacy)** | `firmware/v2/src/effects/*.cpp` |
| **Effect runtime API** | `firmware/v2/src/plugins/api/` (`IEffect.h`, `EffectContext.h`) |
| **Renderer / render loop** | `firmware/v2/src/core/actors/RendererActor.*` |
| **Zones** | `firmware/v2/src/effects/zones/ZoneComposer.*` |
| **Web server (REST + WS)** | `firmware/v2/src/network/WebServer.*` |
| **REST handlers** | `firmware/v2/src/network/webserver/handlers/*` |
| **Dashboard (web app)** | `lightwave-dashboard/` |

---

## Build / Flash / Monitor (PlatformIO)

### Device Port Mapping

**CRITICAL: Device Port Assignments**
- **ESP32-S3 (v2 firmware)**: `/dev/cu.usbmodem1101`
- **Tab5 (encoder firmware)**: `/dev/cu.usbmodem101`

Always use these specific ports for uploads and monitoring.

### LightwaveOS Build Commands

```bash
# Build (default, audio-enabled)
cd firmware/v2 && pio run -e esp32dev_audio

# Build + upload (ESP32-S3 on usbmodem1101)
cd firmware/v2 && pio run -e esp32dev_audio -t upload --upload-port /dev/cu.usbmodem1101

# Serial monitor (ESP32-S3 on usbmodem1101)
pio device monitor -p /dev/cu.usbmodem1101 -b 115200
```

### Encoder Build Commands

```bash
# Build
cd firmware/Tab5.encoder && pio run -e tab5

# Build + upload (Tab5 on usbmodem101)
cd firmware/Tab5.encoder && pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem101

# Serial monitor (Tab5 on usbmodem101)
pio device monitor -p /dev/cu.usbmodem101 -b 115200
```

---

## LLM Context and Prompting

### Stable Context File

Use **[docs/LLM_CONTEXT.md](docs/LLM_CONTEXT.md)** as a stable prefix for LLM prompts instead of re-explaining the project from scratch.

### Context Pack Pipeline

Use **[docs/contextpack/README.md](docs/contextpack/README.md)** for delta-only prompting:

1. Include stable context file
2. Add diff (not whole files)
3. Add trimmed logs (relevant window only)
4. Add fixtures (TOON for uniform arrays, CSV for flat tables, JSON for nested)
5. Add acceptance checks

**Generator script**: `python tools/contextpack.py`

### Fixture Format Selection

| Data Shape | Format | When to Use |
|------------|--------|-------------|
| Large uniform arrays | TOON | Effects lists, palettes, zones (~40% fewer tokens) |
| Purely flat tabular | CSV | Simple key-value data, logs |
| Nested / non-uniform | JSON | Config objects, complex structures |

See [docs/architecture/TOON_FORMAT_EVALUATION.md](docs/architecture/TOON_FORMAT_EVALUATION.md) for the full decision document.

---

## Runtime Testing (Firmware)

The firmware provides a serial command interface (115200 baud). Useful checks:

| Command | Action |
|---------|--------|
| `SPACE` | Next effect |
| `n/N` | Next/prev effect |
| `,` / `.` | Prev/next palette |
| `P` | List palettes |
| `s` | Print FPS/CPU status |
| `l` | List effects |
| `z` | Toggle zone mode |
| `Z` | Print zone status |
| `validate <id>` | Run centre-origin, hue-span, FPS, and heap-delta checks |

---

## Common Codex Tasks (Quick Reference)

### Add a New Native IEffect

1. Create `firmware/v2/src/effects/ieffect/<Name>Effect.h|.cpp`
2. Implement `init()`, `render()`, `cleanup()`, `getMetadata()`
3. Register in `firmware/v2/src/effects/CoreEffects.cpp`
4. Add taxonomy metadata in `firmware/v2/src/effects/PatternRegistry.cpp`
5. Verify: `validate <id>`, centre-origin, no heap alloc in render

### Extend REST API

1. Add handler in `firmware/v2/src/network/webserver/handlers/`
2. Register route in `firmware/v2/src/network/WebServer.cpp`
3. Update OpenAPI spec if applicable
4. Test with `curl` or dashboard

### Modify Web Dashboard

1. Edit files in `lightwave-dashboard/src/`
2. Build: `cd lightwave-dashboard && npm run build`
3. Test locally or deploy

---

## Common Pitfalls (Avoid Regressions)

- **Palette staleness**: Do not cache palette pointers across frames
- **Centre mapping errors**: Centre is between LEDs 79 and 80, not at LED 80
- **Arduino macro collisions**: Avoid using Arduino-core macro identifiers as variable or field names in codecs/handlers. Common no-fly examples: `degrees`, `radians`, `min`, `max`, `abs`, `round`, `sq`, `constrain`, `map`, `pow`, `sqrt`. These are defined as macros in Arduino headers and can cause silent compilation failures or unexpected behaviour. Use descriptive alternatives (e.g., `degreesValue`, `minValue`, `maxValue`) instead.
- **ZoneComposer buffer sizes**: Don't assume global `ledCount` inside effects
- **Build paths**: Always `cd` into `firmware/v2` before running PlatformIO commands

---

## See Also

- [CLAUDE.md](CLAUDE.md) — Claude Code project instructions (**MANDATORY**)
- [docs/LLM_CONTEXT.md](docs/LLM_CONTEXT.md) — Stable LLM prefix file
- [docs/contextpack/README.md](docs/contextpack/README.md) — Context Pack pipeline
- [docs/INDEX.md](docs/INDEX.md) — Full documentation index
- [constraints.md](constraints.md) — Hard limits
- [architecture.md](architecture.md) — Quick reference
- [docs/operations/claude-flow/overview.md](docs/operations/claude-flow/overview.md) — Claude-Flow integration
