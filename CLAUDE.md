# LightwaveOS

## Context Management

This CLAUDE.md is loaded into every conversation. Keep main context for decisions and outcomes only.

**Core rule:** If a task reads 3+ files or produces verbose output, delegate to a subagent and return a distilled summary. This codebase has 802 source files (5.84 MB) — unmanaged exploration destroys working memory.

**Delegate by default:** effect audits (349 files), audio pipeline investigation (65 files), network/API exploration (130 files), crash investigation, cross-backend comparison, any doc over 200 lines.

**Stay in main context:** direct edits, single-file reads, iterative design work.

**Subagents must return:** files inspected, findings, confidence, open questions.

For detailed subsystem costs, delegation scenarios, and context traps: [CONTEXT_GUIDE.md](firmware-v3/docs/CONTEXT_GUIDE.md)

## Hard Constraints

- **Centre origin**: All effects originate from LED 79/80 outward (or inward to 79/80). No linear sweeps.
- **No rainbows**: No rainbow cycling or full hue-wheel sweeps.
- **No heap alloc in render**: No `new`/`malloc`/`String` in `render()` paths. Use static buffers.
- **120 FPS target**: Keep per-frame effect code under ~2 ms.
- **British English** in comments and docs (centre, colour, initialise).

## Architecture

ESP32-S3 LED controller for a dual-strip Light Guide Plate. 320 WS2812 LEDs (2x160), 100+ effects, audio-reactive, web-controlled.

**Two audio backends** (conditional compilation, only one active per build):
- `esp32dev_audio_pipelinecore` — primary development (256-bin FFT, beat tracking, onset detection)
- `esp32dev_audio_esv11` — legacy (64-bin Goertzel)

**Actor model** (FreeRTOS tasks): AudioActor (Core 0) | RendererActor (Core 1) | ShowDirectorActor | CommandActor | PluginManagerActor

**Data flow:** Microphone → I2S DMA → AudioActor → PipelineCore/ESV11 → PipelineAdapter → ControlBus → RendererActor → Effects → FastLED → RMT → WS2812 LEDs

**Key abstractions:**
- `ControlBus` — shared audio state: `bands[0..7]` (octave energy), `chroma[0..11]` (pitch class), `rms`, `beat`, `onset`
- `RenderContext` — per-frame: `leds[]`, `dt`, `zoneId`, `controlBus`. Zone ID `0xFF` = global render.
- Effects inherit `EffectBase`, implement `render(RenderContext&)`. All zone-indexed access must bounds-check: `(ctx.zoneId < kMaxZones) ? ctx.zoneId : 0`

## Build (PlatformIO)

```bash
cd firmware-v3

# Primary (PipelineCore)
pio run -e esp32dev_audio_pipelinecore
pio run -e esp32dev_audio_pipelinecore -t upload

# Legacy (ESV11)
pio run -e esp32dev_audio_esv11

# Serial monitor
pio device monitor -b 115200
```

## Parallel Agent Sandboxing (MANDATORY)

When running parallel subagents that modify or build firmware code, **each agent MUST work in an isolated sandbox copy**. This is a hard rule — no exceptions.

**What happened:** Parallel agents modified `BeatTracker.cpp` source directly, corrupting 6 parameters and causing a regression from 17/17 to 15/17 synthetic tests. Hours of work lost to diagnosis and restoration.

**The rule:**
1. Before launch, copy the firmware tree: `cp -r firmware-v3 /tmp/agent_<name>_<timestamp>/`
2. Agent works EXCLUSIVELY in its sandbox — all edits, builds, and tests happen there
3. Agent returns ONLY data (numerical results, findings) — never file modifications
4. The orchestrator compares results across agents, then applies the winning change to the real tree exactly once
5. Agent prompt MUST include: *"Your working directory is `/tmp/<sandbox>/`. Do NOT modify files outside this directory."*

**Why this works:** firmware-v3 is ~15MB, copies in <1s. WAV test files resolve via absolute paths. PlatformIO toolchain is global (~/.platformio/). Each sandbox is self-contained with zero coupling.

**When to use:** Any time 2+ agents will modify source code, build, or run tests on the same codebase concurrently.

## Further Docs

Read **only** when the task requires it — do not load eagerly:

| Topic | File | Lines | When to read |
|-------|------|-------|-------------|
| Context delegation guide | [firmware-v3/docs/CONTEXT_GUIDE.md](firmware-v3/docs/CONTEXT_GUIDE.md) | 80 | Before delegating complex research to subagents |
| Timing & memory budgets | [firmware-v3/CONSTRAINTS.md](firmware-v3/CONSTRAINTS.md) | 170 | Performance work, memory optimisation |
| Audio-reactive protocol | [firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md](firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md) | 467 | Writing/debugging audio-reactive effects |
| Full REST API reference | [firmware-v3/docs/api/api-v1.md](firmware-v3/docs/api/api-v1.md) | 2,124 | API endpoint work — **search, don't read in full** |
| CQRS state architecture | [firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md](firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md) | 652 | State management, command dispatch |
| Harness worker mode | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) | 364 | Harness/test infrastructure |
