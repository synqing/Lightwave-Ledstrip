# Codex Custom Instructions — LightwaveOS (K1-Lightwave)

## Project Identity

LightwaveOS is an ESP32-S3 firmware driving 320 WS2812B LEDs (2×160 strips) edge-injected into a dual-strip acrylic Light Guide Plate. Audio-reactive: captures audio via I2S microphone, extracts beat/tempo/harmonic features in real time, maps them to visual effects at 120 FPS. The device is the K1-Lightwave, a hardware music visualisation product by SpectraSynq.

## Architecture

Actor model on FreeRTOS dual-core: AudioActor (Core 0) → ControlBus → RendererActor (Core 1) → Effects → FastLED → WS2812 LEDs. Two audio backends exist; only ESV11 is production-active (PipelineCore is broken — do not use). 802 source files, 5.84 MB. CQRS state management with actor message bus.

## Hard Constraints (Violating these is a session failure)

- **Centre origin**: All effects originate from LED indices 79/80 outward. No linear sweeps. No exceptions.
- **No rainbows**: No rainbow cycling or full hue-wheel sweeps.
- **No heap in render**: No `new`/`malloc`/`String` in `render()` or any function called from `render()`. Static buffers only.
- **120 FPS / 2.0ms ceiling**: Per-frame effect code must complete in under 2.0ms. Hard ceiling, not approximate.
- **dt-correct smoothing**: All temporal smoothing must use delta-time, not frame-count.
- **Sub-8ms audio-to-visual latency**: End-to-end pipeline constraint.
- **K1 is AP-ONLY**: WiFi Access Point mode. Never enable STA mode. This is architecturally resolved and must not be revisited.
- **British English**: All comments, docs, logs, and UI strings use British spelling (centre, colour, initialise, behaviour).

## Working Conventions

- **Cite sources, not assumptions.** When referencing project behaviour, cite specific files, functions, parameters, or source lines. Do not make claims about the codebase based on inference alone.
- **Flag uncertainty explicitly.** If you are unsure about a finding or classification, say so. A confidently wrong statement is worse than an honestly uncertain one.
- **Structured output over prose.** When the task specifies an output format, use it exactly. Do not convert structured formats into narrative prose.
- **Write to disk between major steps.** For multi-step analytical tasks, write intermediate outputs to disk before proceeding. This creates recovery points and forces consolidation.
- **Read reference docs first.** Before exploring source files:
  - Codebase structure: `firmware-v3/docs/reference/codebase-map.md`
  - State machines: `firmware-v3/docs/reference/fsm-reference.md`
  - API contracts: `docs/protocol/k1-ws-contract.yaml` and `docs/protocol/k1-rest-contract.yaml`

## Research Task Conventions

When performing analytical or research tasks (as opposed to code modification):
- Read all specified source documents before beginning analysis. Build a mental model first, decompose second.
- Prioritise depth over breadth. Deep analysis of 70% of the domain is more valuable than shallow coverage of 100%.
- If context or quality degrades during a long task, state this explicitly rather than continuing at reduced quality.
- Do not scope-creep into implementation. Analysis produces maps and recommendations. Code changes are separate tasks requiring explicit approval from Captain.

## Who is Captain

Captain is the sole founder and CEO of SpectraSynq. He is a strategist, planner, and orchestrator — not an engineer. He directs AI agents for execution. He communicates directly, uses strong language when frustrated, and values brutal honesty over validation. Address him as Captain.
