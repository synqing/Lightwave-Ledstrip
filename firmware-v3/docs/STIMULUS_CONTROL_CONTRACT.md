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
- Triggered via commands / API (REST, WS, serial).
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

## K1 (Audio) Implementation

### Choke Point

- `RendererActor::renderFrame()` builds the per-frame `AudioContext`.
- At the top of the audio block, it:
  - Chooses an active `SnapshotBuffer<ControlBusFrame>`:
    - Live: `m_controlBusBuffer` (AudioActor-controlled).
    - Stimulus: `m_stimulusControlBusBuffer` (ActorSystem-controlled).
  - Reads latest frame by value into `m_lastControlBus`.
  - Computes `AudioTime` and freshness.
  - Populates `m_sharedAudioCtx` with `controlBus`, `musicalGrid`, and availability flags.

### Mode and Buffer Ownership

- ActorSystem:
  - Owns `m_stimulusControlBusBuffer` and `m_stimulusLastFrame`.
  - Tracks `m_stimulusMode` with values:
    - 0 = live
    - 1 = trinity (audio-specific)
    - 2 = stimulus override
  - Exposes:
    - `setStimulusMode(uint8_t mode)`
    - `clearStimulus()`
    - `publishStimulusFrame(const ControlBusFrame& frame)`
    - `getStimulusFrame() const`
    - `getStimulusControlBusBuffer() const`
- RendererActor:
  - Has `AudioInputMode` enum mirroring the ActorSystem’s mode values.
  - Updates `AudioInputMode` and reset flags via messages:
    - `STIMULUS_SET_MODE`
    - `STIMULUS_CLEAR`

### Mode Semantics

- `setStimulusMode(mode)`:
  - Sends `STIMULUS_SET_MODE` to RendererActor.
  - Only updates `m_stimulusMode` to `mode` if `send()` succeeds.
- `clearStimulus()`:
  - Resets `m_stimulusLastFrame` to defaults.
  - Publishes the cleared frame into the stimulus buffer.
  - Resets `m_stimulusMode` to live (0).
  - Sends `STIMULUS_CLEAR` so RendererActor switches its `AudioInputMode` to live and drops any stimulus latch.

### Timebase Semantics

- All timestamps for stimulus frames share the same microsecond monotonic clock as the engine:
  - On device: `esp_timer_get_time()` → `monotonic_us`.
  - In native/test builds: `micros()` for the same field.
- This keeps:
  - Age calculations (`AudioTime_SecondsBetween`) consistent.
  - Beat/tempo grid logic stable even under override.

### Pulse Semantics (Audio)

- `beat_tick` / `tempoBeatTick` is a one-patch pulse:
  - Before applying a new patch, the handler sets `tempoBeatTick = false` in the working frame.
  - If the JSON/WS payload includes `beat_tick: true`, the frame carries a pulse.
  - If `beat_tick` is omitted, no pulse is carried.
- Between patches:
  - The renderer reuses the last published frame until a new one is published.
  - This means a single patch with `beat_tick: true` yields a beat that persists across render frames until the next patch; this is acceptable for control loops and deterministic from the agent’s perspective.

### REST / WS Contract (Audio)

- REST:
  - `POST /api/v1/stimulus/mode`:
    - Body: `{"mode":"live"|"trinity"|"stimulus"}`
  - `POST /api/v1/stimulus/patch`:
    - Body: `{"rms":..., "flux":..., "bands":[...], "chroma":[...], "tempo_bpm":..., "beat_tick":..., "beat_strength":...}`
    - Partial patches supported; unknown fields ignored.
  - `POST /api/v1/stimulus/clear`:
    - Clears stimulus frame and mode.
  - `GET /api/v1/stimulus/status`:
    - Returns `mode`, `usingStimulus`, and `sequence` of the stimulus buffer.
- WebSocket:
  - Commands:
    - `{"op":"stimulus.mode","id":N,"mode":"..."}` → ack with mode and `usingStimulus`.
    - `{"op":"stimulus.patch","id":N,"patch":{...}}` → ack with `hop_seq` and `sequence`.
    - `{"op":"stimulus.clear","id":N}` → ack with success.
    - `{"op":"stimulus.status","id":N}` → rsp with `mode`, `usingStimulus`, and `sequence`.

## Tab5.encoder Alignment

The same contract applies to encoder/UI stimulus:

- Injection surface:
  - Where encoder deltas and button events are converted into parameter updates or UI navigation.
- Parallel buffer:
  - Real encoder state vs simulated encoder state (stimulus struct).
- Mode switch:
  - Live vs simulated controlled via small commands (REST/WS/Serial).
- Status:
  - Reports both desired mode and whether the simulated path is actually being used.
- Timebase:
  - Any time-sensitive stimulus events (e.g. pulse timings) must live in the same clock domain as the runtime loop that consumes them.
- Pulse semantics:
  - Button presses, encoder clicks, or one-shot events must be represented as pulses (explicitly set per event), not as sticky booleans that survive across multiple updates.

Implementation on Tab5.encoder follows the same pattern as K1, but the state schema is encoder-centric instead of audio-centric. The agentic control layer should treat both systems uniformly:

- Set mode → patch stimulus state → query status
- Assume pulses are one-event/one-patch semantics
- Assume timebase is coherent across injected and hardware paths

