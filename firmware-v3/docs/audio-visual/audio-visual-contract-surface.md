# Audio-Visual Contract Surface

**Version:** 1.1.0  
**Last Updated:** 2026-03-25  
**Status:** Implementation source of truth

This document defines the contract between the audio pipeline (producer) and the visual pipeline (consumer) for `firmware-v3`.

## 1) Producer -> Consumer Mechanics

### Producer side (audio)
- `AudioActor` produces `audio::ControlBusFrame` snapshots from active backend:
  - `PipelineCore` path: feature frame -> `PipelineAdapter::adapt(...)` -> `ControlBusRawInput` -> `ControlBus::Update(...)`.
  - `ESV11` path: adapted frame construction in audio actor/backend adapter.
- Producer publishes latest frame through lock-free shared-memory snapshot:
  - `audio::SnapshotBuffer<ControlBusFrame>::Publish(const T& v)`.

### Consumer side (visual)
- `RendererActor` fetches latest frame each render tick:
  - `uint32_t seq = m_controlBusBuffer->ReadLatest(m_lastControlBus);`
- Freshness is computed from sequence progression and age tolerance:
  - `audioAvailable = sequence_changed || age_within_tolerance`
  - staleness tolerance sourced from `audio_config.h` (`STALENESS_THRESHOLD_MS = 100.0f`).
- Renderer builds `plugins::EffectContext`, including copied audio data (`ctx.audio`) and calls:
  - `effect->render(ctx);`

## 2) Communication Mechanisms

### Shared memory buffer (data plane)
- Type: `SnapshotBuffer<T>` double buffer, lock-free.
- Reader receives by-value copy:
  - `uint32_t ReadLatest(T& out) const`.
- Writer publishes by-value snapshot:
  - `void Publish(const T& v)`.
- Sequence counter supports stale/new detection:
  - `uint32_t Sequence() const`.

### Message queues / events (control plane)
- Actor command flow uses actor mailboxes (`Actor::send(...)`) and typed `Message`.
- System broadcasts use `MessageBus` and events such as `FRAME_RENDERED` and `EFFECT_CHANGED`.
- Trinity sync path is event/message-driven; it does not bypass render-context contract.

## 3) Interface Surface (Consumer API)

### Mandatory effect lifecycle (`plugins::IEffect`)
- `bool init(EffectContext& ctx)`
- `void render(EffectContext& ctx)`
- `void cleanup()`
- `const EffectMetadata& getMetadata() const`

### Optional parameter API
- `uint8_t getParameterCount() const`
- `const EffectParameter* getParameter(uint8_t index) const`
- `bool setParameter(const char* name, float value)`
- `float getParameter(const char* name) const`

### Audio access in effects (`EffectContext::AudioContext`)
- Contract requires accessor usage (no direct `ctx.audio.controlBus` reads in `ieffect` code).
- Key accessors:
  - Energy/rhythm: `rms()`, `flux()`, `bass()`, `mid()`, `treble()`, `beatPhase()`, `isOnBeat()`, `beatStrength()`.
  - Onset semantics: `ctx.audio.onset.*`, `hasOnsetEvent()`, `onsetEnv()`, `onsetEvent()`, `isKickHit()`, `isSnareHit()`, `isHihatHit()`.
  - Spectrum: `bins64()`, `bins64Adaptive()`, `bins256()`, `binHz()`, `energyInRange(...)`, named-band helpers.
  - Chroma/harmony: `chroma()`, `heavyChroma()`, `chordState()`, `musicStyle()`, `styleConfidence()`.
  - Behaviour: `shouldPulseOnBeat()`, `shouldDriftWithHarmony()`, `shouldShimmerWithMelody()`, `shouldTextureFlow()`, `recommendedBehavior()`.
  - Waveform/parity: `waveform()`, `sbWaveform()`, `sbWaveformPeakScaled()`, `preferredWaveform()`.

### First-class onset semantics
- Renderer constructs `ctx.audio.onset` every render tick from `ControlBusFrame` plus `MusicalGridSnapshot`.
- Semantic channels:
  - `beat`, `downbeat`, `transient`, `kick`, `snare`, `hihat`
- Each channel exposes:
  - `fired`, `strength01`, `level01`, `ageMs`, `intervalMs`, `sequence`, `reliable`
- Timing metadata:
  - `phase01`, `bpm`, `tempoConfidence`, `timingReliable`
- Raw detector diagnostics:
  - `raw.flux`, `raw.env`, `raw.event`, `raw.bassFlux`, `raw.midFlux`, `raw.highFlux`
- Compatibility wrappers remain valid for one release cycle:
  - `isOnBeat() -> onset.beat.fired`
  - `beatStrength() -> onset.beat.level01`
  - `isKickHit() -> onset.kick.fired`
  - `isSnareHit() -> onset.snare.fired`
  - `isHihatHit() -> onset.hihat.fired`
  - `hasOnsetEvent()/onsetEnv()/onsetEvent() -> onset.transient/raw`

### Onset reliability contract
- `beat` and `downbeat` are reliable only when live audio is available and tempo confidence is meaningful.
- `transient`, `kick`, and raw detector diagnostics are reliable only when the live detector path is active and Trinity override is not driving the frame.
- `snare` and `hihat` may still carry fallback transport values, but must report `reliable=false` when they are not backed by the native detector path.
- Effects should treat `reliable=false` as a hint to fall back to tempo or lower-risk behaviour.

### Mapping matrix

| Effect-facing onset field | Transport source |
|---------------------------|------------------|
| `onset.beat.*` | `MusicalGridSnapshot.beat_*` |
| `onset.downbeat.*` | `MusicalGridSnapshot.downbeat_tick` + beat strength |
| `onset.transient.*` | `ControlBusFrame.onsetEvent/onsetEnv` |
| `onset.kick.*` | `ControlBusFrame.kickTrigger/onsetBassFlux` |
| `onset.snare.*` | `ControlBusFrame.snareTrigger` + `max(snareEnergy, onsetMidFlux)` |
| `onset.hihat.*` | `ControlBusFrame.hihatTrigger` + `max(hihatEnergy, onsetHighFlux)` |

### Audio->visual mapping hook
- Applied in renderer before `render()`:
  - `AudioMappingRegistry::applyMappings(EffectId, const ControlBusFrame&, const MusicalGridSnapshot&, bool, float, uint8_t&, uint8_t&, uint8_t&, uint8_t&, uint8_t&, uint8_t&, uint8_t&)`

## 4) Performance Requirements

### Latency and cadence
- Render target: **120 FPS** (`~8.33ms` frame budget).
- Effect hot-path budget: **<2ms** per frame (`CONSTRAINTS.md`).
- Audio freshness threshold: **100ms** (`STALENESS_THRESHOLD_MS`).
- Hop throughput by backend configuration:
  - ESV11 default: `12.8kHz / 256 = 50Hz`.
  - PipelineCore: `32kHz / 256 = 125Hz`.
  - Spine16k: `16kHz / 128 = 125Hz`.

### Memory and allocation constraints
- No heap allocation inside `render()` paths.
- Large effect buffers must allocate in PSRAM during `init()`, free in `cleanup()`.
- Snapshot transport is allocation-free on publish/read.

## 5) Error/Failure Semantics

- Contract uses boolean/availability semantics, not numeric error codes:
  - `init(...)` returns `false` on setup failure (for example PSRAM allocation failure).
  - `ctx.audio.available == false` indicates stale/absent audio and must be handled gracefully.
  - Mapping/parameter calls return boolean success/failure.
- Message queue failures are signalled by send-return status; retry/handling is actor-level.

## 6) Current Contract Rules for Effect Authors

- Centre-origin only: render from LEDs `79/80` outward (or inward).
- No rainbow cycling or full hue-wheel sweeps.
- No heap allocs in `render()`.
- Access audio via `AudioContext` accessors only.
- Prefer `bins256` (`binHz`) for frequency-accurate spectrum effects; use `bins64` as fallback.
- Use behaviour/style context when adaptive response is needed.

## 7) Regression Gate

- Local checker:
  - `python3 firmware-v3/tools/check_effect_contracts.py`
- CI workflow:
  - `.github/workflows/effect_contract_check.yml`
- Semantic ADR:
  - [ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md](./ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md)
