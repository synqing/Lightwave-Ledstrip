# Codex Agents Guide (Lightwave-Ledstrip / LightwaveOS)

This repository contains firmware for an ESP32‑S3 driving a dual‑strip Light Guide Plate (LGP) system, plus a web dashboard. This file is written for automated coding agents (Codex Agents) to work productively and safely in this codebase.

## Non‑Negotiable Invariants

- **Centre origin only**: All effects must originate from the centre LEDs **79/80** and propagate **outward** (or converge inward to the centre). Do not implement linear 0→159 sweeps.
- **No rainbows**: Do not introduce rainbow cycling / full hue‑wheel sweeps.
- **No heap allocation in render loops**: No `new`, `malloc`, `std::vector` growth, `String` concatenations, or other dynamic allocation in `render()` hot paths.
- **Performance matters**: Rendering runs at high frame rate (target 120 FPS, often higher). Keep per‑frame work predictable.
- **British English**: Use British spelling in comments and documentation (centre, colour, initialise, etc.).

## Where Things Live

- **v2 firmware**: `firmware/v2/src/` (actor system, zones, IEffect plugins, web server)
- **Encoder firmware**: `firmware/K1.8encoderS3/` (M5Stack AtomS3 with 8-encoder control)
- **Effects**:
  - Legacy function effects: `firmware/v2/src/effects/*.cpp`
  - Native IEffect effects: `firmware/v2/src/effects/ieffect/*.h|*.cpp`
- **Effect runtime API**: `firmware/v2/src/plugins/api/` (`IEffect.h`, `EffectContext.h`)
- **Renderer / render loop**: `firmware/v2/src/core/actors/RendererActor.*`
- **LED topology / centre config**: `firmware/v2/src/core/actors/RendererActor.h` (`LedConfig`), `firmware/v2/src/hal/led/LedDriverConfig.h`; centre‑pair helpers in `firmware/v2/src/effects/CoreEffects.h`
- **Zones**: `firmware/v2/src/effects/zones/ZoneComposer.*`
- **Pattern taxonomy registry**: `firmware/v2/src/effects/PatternRegistry.*`
- **Web server (REST + WS)**:
  - Core: `firmware/v2/src/network/WebServer.*`
  - Handlers: `firmware/v2/src/network/webserver/handlers/*`
- **Dashboard (web app)**: `lightwave-dashboard/`

## Build / Flash / Monitor (PlatformIO)

This repo uses PlatformIO with two standalone projects:
- **LightwaveOS (v2)**: `firmware/v2/`
- **Encoder (K1.8encoderS3)**: `firmware/K1.8encoderS3/`

### LightwaveOS Build Commands

- **Build (default, audio-enabled)**:
  - `cd firmware/v2 && pio run -e esp32dev_audio`
- **Build + upload**:
  - `cd firmware/v2 && pio run -e esp32dev_audio -t upload`
- **Serial monitor**:
  - `pio device monitor -b 115200`

### Encoder Build Commands

- **Build**:
  - `cd firmware/K1.8encoderS3 && pio run -e atoms3`
- **Build + upload**:
  - `cd firmware/K1.8encoderS3 && pio run -e atoms3 -t upload`

Notes:
- `wifi_credentials.ini` is loaded via `extra_configs` and may be gitignored. See `wifi_credentials.ini.template`.
- There is also a `native` environment for host builds, but it only compiles a small subset.
- Other useful environments: `esp32dev_debug`, `memory_debug`, `esp32dev_enhanced` (see `firmware/v2/platformio.ini`).

## Runtime Testing (Firmware)

The firmware provides a serial command interface (115200 baud). Useful checks:

- **Effect switching**: use the printed key map (e.g. `SPACE`, `0-9/a-k`, `n/N`).
- **Palettes**: `,` and `.` cycle palettes; use `P` to list palettes.
- **Status**: `s` prints FPS/CPU and current effect.
- **List effects**: `l`
- **Zone mode**: `z` toggles zone mode; `Z` prints zone status.
- **Validate effect**: `validate <id>` runs centre‑origin, hue‑span (<60°), FPS, and heap‑delta checks.

If you change the effect pipeline, always verify:
- Palette switching still affects legacy and IEffect effects.
- Zone mode still renders per‑zone effects correctly.

## Effect System: IEffect

### Key concepts

- **All effects are IEffect instances** in the v2 pipeline:
  - Native IEffect effects: classes under `v2/src/effects/ieffect/`
  - Legacy effects: wrapped via adapter in `v2/src/plugins/runtime/LegacyEffectAdapter.*`
- **EffectContext** (`v2/src/plugins/api/EffectContext.h`) is the injection surface:
  - LED buffer (`ctx.leds`, `ctx.ledCount`)
  - Palette (`ctx.palette`)
  - Parameters (`brightness`, `speed`, `gHue`, etc.)
  - Helpers enforce/encourage centre‑origin math
- **Centre point is configurable**: `RendererActor` sets `ctx.centerPoint` from `LedConfig::CENTER_POINT` (currently 79). Core effects use `CENTER_LEFT/CENTER_RIGHT` (79/80) from `v2/src/effects/CoreEffects.h`. Avoid hardcoding `80` or assuming linear 0→319 symmetry; mirror per‑strip unless you intentionally handle both halves.

### Creating / migrating an effect to native IEffect

1. Create `firmware/v2/src/effects/ieffect/<Name>Effect.h|.cpp`
2. Implement:
   - `bool init(EffectContext&)`: reset instance state
   - `void render(EffectContext&)`: do the work (no heap alloc)
   - `void cleanup()`: usually no‑op
   - `const EffectMetadata& getMetadata() const`
3. Move any `static` render state into **instance members**.
4. Register it in `firmware/v2/src/effects/CoreEffects.cpp` using:
   - `renderer->registerEffect(effectId, &instance);`
5. Keep the effect ID stable (must match PatternRegistry indices).

## PatternRegistry vs IEffect Metadata

`firmware/v2/src/effects/PatternRegistry.*` provides taxonomy metadata (family/tags/story).
`IEffect::getMetadata()` provides runtime effect metadata (name/description/category/version).

When exposing metadata externally (API/UI), prefer IEffect metadata when available, and supplement with PatternRegistry taxonomy fields.

## Web API Expectations

The REST API is served from `firmware/v2/src/network/WebServer.*` and handlers in `firmware/v2/src/network/webserver/handlers/`.

Effects endpoints to keep stable:
- `GET /api/v1/effects`
- `GET /api/v1/effects/current`
- `POST /api/v1/effects/set`
- `PUT /api/v1/effects/current` (compat alias)
- `GET /api/v1/effects/families`
- `GET /api/v1/effects/metadata?id=N`

When extending responses, add fields in a backward‑compatible way (additive JSON fields).

## Common Codex Tasks (with file touch lists)

Use this section as a fast “where do I change what?” map. Keep changes minimal, centre‑origin compliant, and allocation‑free in render loops.

### Effects & rendering

- **Add a new native IEffect**
  - **Create**: `v2/src/effects/ieffect/<Name>Effect.h`, `v2/src/effects/ieffect/<Name>Effect.cpp`
  - **Register** (stable ID): `v2/src/effects/CoreEffects.cpp`
  - **Taxonomy metadata** (family/tags/story): `v2/src/effects/PatternRegistry.cpp`
  - **Quick checks**: centre‑origin, palette cycling works, no heap alloc in `render()`, `validate <id>`

- **Migrate a legacy effect (function) to native IEffect**
  - **Source** (legacy): typically `v2/src/effects/CoreEffects.cpp` or other `v2/src/effects/*.cpp`
  - **New IEffect**: `v2/src/effects/ieffect/<Name>Effect.h|.cpp`
  - **Registration swap**: `v2/src/effects/CoreEffects.cpp` (keep the effect ID stable; avoid double‑register)
  - **Static→instance state**: move any `static` render variables into class members, reset in `init()`
  - **Quick checks**: no palette pointer caching, zone mode doesn’t break, `validate <id>`

- **Modify EffectContext population (add a field, change semantics)**
  - **Where EffectContext is created**: `v2/src/core/actors/RendererActor.cpp` (`renderFrame()` and effect init path)
  - **EffectContext definition**: `v2/src/plugins/api/EffectContext.h`
  - **Adapter compatibility**: `v2/src/plugins/runtime/LegacyEffectAdapter.*`
  - **Quick checks**: palette updates propagate, no heap delta during rendering, zone mode still works

- **Add or adjust an effect’s runtime metadata**
  - **IEffect metadata**: `v2/src/plugins/api/IEffect.h` (struct), per‑effect `getMetadata()`
  - **Taxonomy metadata**: `v2/src/effects/PatternRegistry.*`
  - **Expose via API**: `v2/src/network/webserver/handlers/EffectHandlers.*`
  - **Quick checks**: `/api/v1/effects`, `/api/v1/effects/metadata?id=N`, `l` output looks sane

- **Add custom effect parameters (beyond the global brightness/speed/etc.)**
  - **Firmware surface**: `v2/src/plugins/api/IEffect.h` (add a parameter schema/descriptor if introducing one)
  - **Transport**: `v2/src/network/webserver/handlers/EffectHandlers.*`, `v2/src/network/WebServer.cpp` (WS types)
  - **Dashboard**: `lightwave-dashboard/src/`
  - **Quick checks**: backwards compatible JSON (add fields), validate parsing, no allocations in hot paths

- **Add a new palette / palette behaviour**
  - **Palette implementation**: search under `v2/src/` for palette manager code and palette tables
  - **Serial/UI exposure**: `v2/src/main.cpp` (serial), `v2/src/network/*` (REST/WS), dashboard as needed
  - **Quick checks**: `,` and `.` work; legacy adapters must not cache palette pointers

- **Adjust colour correction rules (LGP‑sensitive / stateful)**
  - **Engine**: `v2/src/effects/enhancement/ColorCorrectionEngine.*`
  - **Classification**: `v2/src/effects/PatternRegistry.*` (`isLGPSensitive`, `isStatefulEffect`)
  - **Quick checks**: verify the sensitive effect list, run `validate <id>` on interference patterns

- **Optimise performance hot paths**
  - **Renderer**: `v2/src/core/actors/RendererActor.*`
  - **Zones**: `v2/src/effects/zones/ZoneComposer.*`
  - **Effects**: `v2/src/effects/ieffect/*` and `v2/src/effects/*.cpp`
  - **Quick checks**: FPS/CPU (`s`), heap delta (validate), no dynamic allocations in render

### Zones

- **Add zone‑specific palettes**
  - **Renderer/composer**: `v2/src/effects/zones/ZoneComposer.*` (currently copies global palette; extend to per‑zone)
  - **Persistence**: wherever zone config is saved/loaded (search `ZoneConfig`, `NVS`, `Preferences` in `v2/src/`)
  - **Quick checks**: zone mode toggle `z`, verify per‑zone palette changes don’t affect globals

- **Add a new zone preset/layout**
  - **Zone definitions/config**: `v2/src/effects/zones/*`
  - **Serial/UI controls**: `v2/src/main.cpp`, plus REST/WS handlers if exposed
  - **Quick checks**: `Z` status output, zone compositor bounds safety

- **Make an effect zone‑aware**
  - **Use**: `EffectContext` fields `zoneId`, `zoneStart`, `zoneLength`
  - **Avoid**: hardcoding global LED indices; assume `ctx.ledCount` could be zone‑sized
  - **Quick checks**: effect renders correctly both in global and in zones

### REST, WebSocket, dashboard

- **Extend effects list payload**
  - **REST**: `v2/src/network/webserver/handlers/EffectHandlers.*` (`handleList`, `handleMetadata`, `handleCurrent`)
  - **WS**: `v2/src/network/WebServer.cpp` (WS message types like `effects.list`)
  - **Dashboard**: `lightwave-dashboard/src/`
  - **Quick checks**: additive fields only; existing clients keep working

- **Add a new REST endpoint**
  - **Routing**: `v2/src/network/WebServer.cpp` (route registration)
  - **Handler**: `v2/src/network/webserver/handlers/*`
  - **Docs**: `v2/src/network/WebServer.cpp` (OpenAPI builder) and `docs/api/`

- **Add a new WS message type**
  - **WS dispatcher**: `v2/src/network/WebServer.cpp`
  - **Client**: `lightwave-dashboard/src/`
  - **Quick checks**: rate limiting, message size, non‑blocking behaviour

### Persistence, configuration, build

- **Persist new settings in NVS**
  - **Search targets**: `Preferences`, `NVS`, `saveToNVS`, `loadFromNVS` in `v2/src/`
  - **Quick checks**: first‑boot defaults, backwards compatible schema evolution

- **Add a new PlatformIO environment / build flag**
  - **Config**: `platformio.ini`
  - **Consumers**: `v2/src/config/*` and compile‑time `#if FEATURE_*` gates
  - **Quick checks**: default env still builds, WiFi env still builds, memory_debug doesn’t regress

### Diagnostics & QA

- **Add a serial command**
  - **Command parser**: `v2/src/main.cpp`
  - **Quick checks**: don’t allocate heavily or spam output in fast loops; keep messages concise

- **Add or extend validation**
  - **Serial validate command**: `v2/src/main.cpp` (or wherever `validate <id>` is implemented)
  - **Checks**: centre origin, hue span guardrails, FPS, heap delta

- **Add an automated test script (host)**
  - **Scripts**: `tools/` (serial automation, regressions, reports)
  - **Quick checks**: deterministic output formats, non‑interactive flags, safe defaults

### Docs & taxonomy

- **Add/update pattern taxonomy**
  - **Firmware registry**: `v2/src/effects/PatternRegistry.*`
  - **Docs**: `docs/analysis/`, `docs/effects/`, `docs/architecture/`

- **Add/update architecture docs for agents**
  - **High‑level**: `ARCHITECTURE.md`
  - **Deep dive**: `docs/architecture/`

## Common Pitfalls (Avoid Regressions)

- **Palette staleness**: Do not cache palette pointers across frames unless the palette object is guaranteed stable. If adapting legacy code, fetch the palette reference per frame.
- **Centre mapping errors**: Centre is between LEDs 79 and 80. Be careful with integer rounding when computing signed position.
- **ZoneComposer buffer sizes**: ZoneComposer renders to a temp buffer; avoid assuming global `ledCount` inside effects.
- **Colour correction sensitivity**: Some LGP/interference effects are sensitive; check `PatternRegistry::isLGPSensitive()` and related logic.
- **Build paths**: Always `cd` into `firmware/v2` or `firmware/K1.8encoderS3` before running PlatformIO commands.

## Agent Workflow Expectations

- Prefer small, reviewable commits (logical changesets).
- After changing effect registration or metadata, verify:
  - Effects count matches expected (boot log parity checks).
  - `l` output lists correct names.
  - `GET /api/v1/effects` returns sane metadata.
- Keep documentation truthful (update this file if you change fundamental workflows).

## Claude-Flow Strategic Integration

This repository uses **Claude-Flow v3 (RuVector)** for intelligent agent orchestration, enabling specialised domain routing, pattern memory, and coordinated swarm execution.

### Quick Start

**MCP Integration**: See [`docs/operations/claude-flow/MCP_SETUP.md`](docs/operations/claude-flow/MCP_SETUP.md) for configuration.

**Agent Routing**: Tasks route to domain specialists automatically:
- `firmware/v2/src/effects/**` → Firmware/Embedded Agent (centre-origin, no heap allocation)
- `firmware/v2/src/network/**` → Network/API Agent (protected file protocols, thread safety)
- `lightwave-dashboard/**` → Frontend/UI Agent (accessibility, E2E tests)
- `docs/**` → Documentation Agent (British English, spelling)

**Swarm Templates**: Reusable swarm configurations for common workflows:
- Effect migration: IEffect class + PatternRegistry + docs
- API change: Handler + OpenAPI + dashboard client + docs
- Dashboard UI feature: Component + test + accessibility + docs

**Pattern Memory**: Successful patterns stored for reuse:
- `memory_search "effect_migration"` - IEffect class + registration + PatternRegistry patterns
- `memory_search "centre_origin"` - Centre-origin math helpers (CENTER_LEFT/CENTER_RIGHT)
- `memory_search "british_english"` - British spelling patterns (centre/colour/initialise)

### Full Documentation

- **[Overview](docs/operations/claude-flow/overview.md)** - Strategic integration overview
- **[Agent Routing](docs/operations/claude-flow/agent-routing.md)** - Detailed routing matrix and acceptance criteria
- **[Swarm Templates](docs/operations/claude-flow/swarm-templates.md)** - Reusable swarm configurations
- **[Pair Programming](docs/operations/claude-flow/pair-programming.md)** - Pair programming mode selection
- **[Validation Checklist](docs/operations/claude-flow/validation-checklist.md)** - Setup and runtime verification

