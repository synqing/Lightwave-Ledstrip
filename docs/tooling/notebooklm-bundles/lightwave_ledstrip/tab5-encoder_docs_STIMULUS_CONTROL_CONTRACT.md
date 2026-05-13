# Stimulus Control Contract

## Purpose

Unify how “stimulus” inputs override hardware-derived state across:
- Tab5.encoder (encoder and UI inputs)
- LightwaveOS v2 K1 (audio ControlBus / MusicalGrid inputs)

The goal is:
- One canonical injection surface per subsystem
- Strict status truth (no desync between “mode” and reality)
- Deterministic semantics for agents and test harnesses

## Core Principles

- Single choke point:
  - All injected stimulus flows through exactly one point in the runtime where the real pipeline normally consumes hardware-derived state.
- Parallel buffer:
  - Hardware path owns its canonical buffer.
  - Stimulus path owns a parallel buffer with the same shape.
  - The engine reads from exactly one of these at the choke point, based on mode.
- Mode switch is small:
  - Mode changes are expressed as tiny commands, not by shipping the whole state object through the message bus.
- Status is truthful:
  - The reported mode and “usingStimulus” must always match what the engine is actually doing.
- Timebase is coherent:
  - Stimulus timestamps live in the same clock domain and units as the engine’s own timing.
- Pulses are explicit:
  - Pulse fields (ticks, button presses, beat flags) are one-patch or one-event semantics, never indefinitely sticky.

## Shared Concepts

### 1. Injection Surface

- A well-defined function or block where the runtime builds the context visible to downstream logic.
- Hardware vs stimulus is abstracted behind this surface:
  - Encoders: where encoder deltas and button events are turned into parameter updates.
  - Audio: where `ControlBusFrame` and `MusicalGridSnapshot` are read and transformed into `AudioContext`.

### 2. Mode Enumeration

Each subsystem exposes a small mode enum:

- Live: consume hardware-derived state only.
- Stimulus: consume the stimulus buffer instead of hardware.
- Hybrid modes (e.g. Trinity for audio) are allowed, but must never cause status to lie about active source.

Mode changes are:
- Triggered via commands / API (REST/WS/Serial).
- Propagated to the engine via a message or direct call.
- Reflected in status only after they are actually applied.

### 3. Stimulus Buffer

- Owned centrally (not by the UI or network layer).
- Same schema as the hardware-produced state:
  - Encoders: a state struct capturing positions, deltas, button flags, etc.
  - Audio: `ControlBusFrame` + associated timing fields.
- Written by:
  - REST / WS handlers.
  - Serial/test harness commands.
- Read only by:
  - The engine at the choke point.

## Tab5.encoder Implementation

### Choke Point

- The Tab5 main loop builds a “control context” that feeds:
  - Parameter updates (brightness, speed, etc.)
  - UI state (screen transitions, focus, selection).
- At the injection surface:
  - Live path: consumes encoder deltas and button events from `DualEncoderService` / hardware layer.
  - Stimulus path: consumes a parallel encoder stimulus state struct.

### Mode and Buffer Ownership

- Central stimulus controller (Tab5):
  - Owns the encoder stimulus state.
  - Tracks an explicit mode:
    - Live: hardware only.
    - Stimulus: override from stimulus struct.
  - Exposes:
    - `setStimulusMode(mode)`
    - `clearStimulus()`
    - `publishStimulusPatch(patch)`
    - `getStimulusState() const`
- UI / parameter handlers:
  - Never read hardware directly.
  - Always operate on the unified “control context” built at the injection surface.

### Timebase Semantics

- Any time-sensitive stimulus inputs (e.g. pulses, gated events) must:
  - Use the same timebase as the loop that consumes them.
  - Be compared in that timebase when implementing hold / decay / hysteresis.

### Pulse Semantics (Encoders)

- Pulses (button press, click, one-frame tick) are one-event pulses:
  - Clearing happens either:
    - At merge time (before applying a new patch), or
    - Immediately after the event has been consumed by the control loop.
  - Patches that omit a pulse field do not re-trigger it.
- Between patches:
  - The engine may keep the last continuous state (position, coarse mode, etc.).
  - Pulse semantics remain explicit: a pulse only exists if a patch or input path sets it in that cycle.

### REST / WS Contract (Encoders)

The encoder stimulus API mirrors the audio stimulus pattern:

- Mode:
  - Command to set mode (live vs stimulus).
  - Status reflecting actual mode and whether stimulus is in use.
- Patch:
  - Partial updates to the encoder stimulus struct (positions, deltas, flags).
- Clear:
  - Reset stimulus state and return to live mode.
- Status:
  - Provide:
    - Current mode.
    - Whether stimulus is active.
    - Any sequence / versioning needed for agents to detect changes.

## K1 (Audio) Alignment

The same contract is implemented for K1 audio stimulus:

- Injection surface:
  - `RendererActor::renderFrame()` reads either live or stimulus `ControlBusFrame` via a `SnapshotBuffer`.
- Parallel buffer:
  - Live buffer owned by AudioActor.
  - Stimulus buffer owned by ActorSystem.
- Mode switch:
  - `AudioInputMode` in RendererActor controlled via messages (`STIMULUS_SET_MODE`, `STIMULUS_CLEAR`).
  - `m_stimulusMode` in ActorSystem reflects only successfully applied modes.
- Status:
  - REST and WS endpoints report both `mode` and `usingStimulus`, and expose buffer `sequence`.
- Timebase:
  - Stimulus `ControlBusFrame` timestamps use the same microsecond monotonic clock as the renderer’s clock spine.
- Pulse semantics:
  - `beat_tick` is explicitly reset before each patch merge, ensuring one-patch semantics.

With this contract, Tab5.encoder and K1 can be driven by the same higher-level agent logic:

- Set mode → send patch → query status.
- Rely on pulses being one-event/one-patch.
- Assume timebase and schema are consistent between hardware and stimulus paths.

