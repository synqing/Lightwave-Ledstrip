# Findings: PRISM/Trinity ↔ Lightwave Integration

## Quick Summary
- Lightwave already has an internal DSP “ControlBus” (ControlBusFrame) consumed by effects via `EffectContext`.
- Lightwave already implements a WebSocket Trinity ingestion surface (behind `FEATURE_AUDIO_SYNC`):
  - `trinity.sync` (start/stop/pause/resume/seek + position + optional bpm, optional `_id` ACK)
  - `trinity.beat` (bpm, beat_phase, tick/downbeat flags, beat_in_bar)
  - `trinity.macro` (energy, vocal_presence, bass_weight, percussiveness, brightness) in `[0,1]`
- RendererActor can switch audio source between “live” ControlBus frames and Trinity proxy frames (`TrinityControlBusProxy`), with 250ms staleness protection.
- PRISM Dashboard V4 canonicalises segment labels into stable tokens: `intro`, `verse`, `chorus`, `bridge`, `breakdown`, `drop`, `solo`, `inst`, `start`, `end`.
- PRISM Dashboard V4 can export a `k1_mapping_v1` JSON containing per-segment `palette_id` and `motion_id` assignments + `segment_fade_ms`.

## Lightwave-Ledstrip: Relevant Entry Points
- Docs to read first:
  - `docs/shows/README.md`
  - `firmware/v2/docs/PHASE4_SHOWDIRECTOR_ACTOR.md`
  - `firmware/v2/src/core/README.md` (Show System section)
  - `docs/audio-visual/audio-visual-semantic-mapping.md`

## PRISM/Trinity: Relevant Entry Points
- `SpectraSynq.trinity/Trinity.s3hostside/trinity_contract/contract_types.ts` defines the canonical “Trinity Contract”:
  - Contract meta includes `rhythm` (beats/downbeats), `structure.segments` (labelled sections), and optional `blobs`.
  - WebSocket message types include **`trinity.segment`** in addition to beat/macro/sync.
- `SpectraSynq.trinity/Trinity.s3hostside/trinity_contract/trinity_loader.ts` shows a reference host implementation that streams:
  - `trinity.beat` on beat/downbeat boundaries
  - `trinity.segment` on section changes
  - `trinity.macro` at fixed FPS

## Protocol / Mapping Notes
- (Schema ideas, parameter naming, expected ranges, smoothing strategies.)

### Trinity WS Schema (as implemented in firmware)
- `trinity.sync` required fields: `action` ("start"|"stop"|"pause"|"resume"|"seek"), `position_sec` (>=0). Optional: `bpm` (default 120), `_id` (int64; if present, server replies `{_type:"ack", _id:<same>}`).
- `trinity.beat` required fields: `bpm` (30–300), `beat_phase` ([0,1)). Optional: `tick` (bool), `downbeat` (bool), `beat_in_bar` (int; packed to 2 bits in message).
- `trinity.macro` optional floats (clamped to [0,1]): `energy`, `vocal_presence`, `bass_weight`, `percussiveness`, `brightness`.

### Gap Identified (Isolation Point)
- Trinity contract/loader emits `trinity.segment` (section/structure). This was previously missing from Lightwave firmware, so messages were rejected as unknown types.

### Resolution Implemented
- Added firmware support for `trinity.segment` and a host-side bridge that can consume PRISM’s `k1_mapping_v1` export to drive Lightwave effect/palette selection on segment changes.

## Decisions & Rationale (Expanded)
- (Keep design rationale here once the approach stabilises.)

## References
- Paths, commands, links:
  - `docs/api/api-v1.md`
  - `firmware/v2/src/network/WebServer.cpp`

---

# Findings: Claude-Mem Audio Effect Reference Audit (2026-02-16)

## Method
- Use Claude-mem 3-layer workflow per effect family:
- Step 1: `search` for candidate observations
- Step 2: `timeline` around strongest anchors
- Step 3: `get_observations` for filtered IDs only

## Targets
- Kuramoto
- SB Waveform (Parity)
- SB Waveform (REF)
- ES Analog
- ES Spectrum
- ES Octave
- ES Bloom
- ES Waveform
- Snapwave
- Beat Pulse

## Extraction Log
- `Kuramoto`
  - Key observations: `#28119`, `#28122`, `#28124`, `#28126`, `#32995`, `#32996`, `#33003`, `#33004`
  - Reference chain: design brief + implementation guide + metadata registration + memory/layout deep dives
  - Trace summary: Kuramoto transport architecture is explicitly split into invisible oscillator field and visible transport buffer, with coherence/phase-slip feature extraction and dt-correct transport maths.

- `SB Waveform (Parity)`
  - Key observations: `#28421`, `#28426`, `#32947`, `#33009`
  - Reference chain: PatternRegistry registration/categorization + implementation architecture and smoothing/normalization notes
  - Trace summary: parity effect uses intensity-only rendering, dynamic max-follower normalisation, 4-frame waveform history, and dt-correct smoothing.

- `SB Waveform (REF)`
  - Key observations: `#28417`, `#28191`, `#28193`
  - Reference chain: SB reference header analysis + archived SensoryBridge reference file discovery
  - Trace summary: REF path is tied to Sensory Bridge 3.1.0 waveform behavior (note-chromagram colour summation + waveform history smoothing).

- `ES Analog`
  - Key observations: `#28416` (grouped waveform implementations)
  - Cross-check: `PatternRegistry.cpp` and `CoreEffects.cpp` confirm ES reference registration and instance wiring.
  - Trace summary: sparse direct Claude-mem entries; no dedicated deep-dive observation for `EsAnalogRefEffect.cpp` located in memory history.

- `ES Spectrum`
  - Key observations: `#28416` (grouped waveform implementations)
  - Cross-check: `PatternRegistry.cpp` and `CoreEffects.cpp` confirm ES reference registration and instance wiring.
  - Trace summary: sparse direct Claude-mem entries; no dedicated deep-dive observation for `EsSpectrumRefEffect.cpp` located in memory history.

- `ES Octave`
  - Key observations: `#28416` (grouped waveform implementations)
  - Cross-check: `PatternRegistry.cpp` and `CoreEffects.cpp` confirm ES reference registration and instance wiring.
  - Trace summary: sparse direct Claude-mem entries; no dedicated deep-dive observation for `EsOctaveRefEffect.cpp` located in memory history.

- `ES Bloom`
  - Key observations: `#28416` (grouped waveform implementations)
  - Cross-check: `PatternRegistry.cpp` and `CoreEffects.cpp` confirm ES reference registration and instance wiring.
  - Trace summary: sparse direct Claude-mem entries; no dedicated deep-dive observation for `EsBloomRefEffect.cpp` located in memory history.

- `ES Waveform`
  - Key observations: `#28416` (grouped implementations including ES waveform path)
  - Cross-check: `PatternRegistry.cpp` and `CoreEffects.cpp` confirm ES reference registration and instance wiring.
  - Trace summary: ES waveform is consistently referenced as part of the reference waveform stack, but memory granularity is mostly grouped rather than per-file deep analysis.

- `Snapwave`
  - Key observations: `#27724`, `#28237`
  - Reference chain: implementation behavior note + ID-mismatch/regression context
  - Trace summary: SnapwaveLinear is single-strip with history trail/oscillation logic; historical ID mapping conflicts were documented (`ID 98` mismatch context in older audit paths).

- `Beat Pulse`
  - Key observations: `#27372`, `#27504`, `#27639`, `#27913`, `#27953`
  - Reference chain: utility spine creation + parity-core lock decision + family architecture audit + inventory
  - Trace summary: BeatPulse family history documents parity-constant locking (`BeatPulseHTML` maths), sub-family behavior distinctions, and prior mismatch reports before later timing/logic corrections.

## Notes on Coverage
- Claude-mem provided strong historical depth for Kuramoto, SB waveform parity/ref, Snapwave, and Beat Pulse.
- Claude-mem was sparse for per-file ES reference deep dives (`EsAnalogRefEffect`, `EsSpectrumRefEffect`, `EsOctaveRefEffect`, `EsBloomRefEffect`); evidence is mostly grouped under waveform/reference inventory observations.

---

# Findings: Experimental Audio-Reactive Effects Pack (2026-02-17)

## Hard Constraints Confirmed
- Centre-origin rendering is mandatory (`centerPairDistance` / `SET_CENTER_PAIR` patterns).
- No heap allocation in render path.
- Effects with buffers >64 bytes should be PSRAM-allocated in `init()` and freed in `cleanup()`.
- Raw timing exists and should be used for audio-coupled maths where speed neutrality matters (`ctx.getSafeRawDeltaSeconds()`, `ctx.rawTotalTimeMs`).
- `limits::MAX_EFFECTS` is currently 152 and must be updated when adding effect IDs.

## Integration Points Confirmed
- Registration factory: `firmware/v2/src/effects/CoreEffects.cpp` (`registerAllEffects()`).
- Metadata table: `firmware/v2/src/effects/PatternRegistry.cpp` (`PATTERN_METADATA[]`).
- Reactive cycling register: `REACTIVE_EFFECT_IDS` in `PatternRegistry.cpp`.
- Global capacity: `firmware/v2/src/config/limits.h` (`MAX_EFFECTS`).

## Design Direction Chosen
- Implement one new pack in `firmware/v2/src/effects/ieffect/` with 10 effect classes to minimise integration churn.
- Use audio-derived parameters from current pipeline (`rms`, `flux`, `beatStrength`, `heavy_bands`, `chroma`, `bins64Adaptive`) with safe fallbacks when audio is unavailable.
- Keep palette-driven colour identity and avoid hue-wheel/rainbow cycling behaviour.

## Implemented Effects (IDs 152-161)
1. `LGP Flux Rift` - fast-flux envelope with travelling rift ring
2. `LGP Beat Prism` - beat-front prism rays with treble weighting
3. `LGP Harmonic Tide` - chord/saliency anchored tidal bands
4. `LGP Bass Quake` - heavy-bass compression core with outward shock release
5. `LGP Treble Net` - timbral shimmer interference lattice
6. `LGP Rhythmic Gate` - rhythmic-saliency gate with travelling seam
7. `LGP Spectral Knot` - low/mid/high balance knot crossings
8. `LGP Saliency Bloom` - overall novelty controlled bloom radius
9. `LGP Transient Lattice` - snare/flux triggered impact scaffold
10. `LGP Wavelet Mirror` - waveform crest mirroring with beat ridge

---

# Findings: SPH0645 Audio Capture Failure Forensics (2026-02-22)

## Investigation Scope
- Validate end-to-end microphone data flow through `PipelineCore`
- Compare pre/post `PipelineCore` capture configuration
- Identify exact zero-data interruption point with source-level evidence

## Runtime Failure Signature (Provided Serial Log)
- Timestamp window: ~`12:12:11` to `12:12:25` (user capture)
- Repeating indicators:
  - `DC_DIAG: mean=0 min=0 max=0 rms=0.0000 zeros=<increasing>`
  - `AudioCapture [DIAG-A1]: LEFT raw=[00000000..00000000], RIGHT raw=[00000000..00000000]`
- Initial inference: zero originates at or before raw capture handoff, not merely post-DSP attenuation.

## Notes
- `auggie` MCP tooling is not available in this environment; historical reconstruction will use `claude-mem` plus local git history as the authoritative fallback.

## Forensics Update: Controlled A/B/A Regression on Real Hardware (2026-02-22)

### Runtime Trace Acquisition (live board)
- Local timestamp: `2026-02-22 12:46:34 AWST`
- Device: `/dev/cu.usbmodem101`
- Backend under test: `esp32dev_audio_pipelinecore`
- Serial tools used: `platformio device monitor`, commands `adbg 5`, `adbg status`, `adbg spectrum`, `adbg beat`

### A (Current repo config: GPIO 13/12/14)
- Observed raw capture signature:
  - `LEFT raw=[00000000..00000000]`
  - `RIGHT raw=[00000000..00000000]`
- Observed post-capture metrics:
  - `DC_DIAG: mean=0 min=0 max=0 rms=0.0000`
  - `zeros` monotonically increasing
  - `adbg status` showed `RMS: 0.0000`, `Flux: 0.0000`
- Conclusion: hard zero at/preceding raw DMA source.

### B (Pin-only revert build, temp copy)
- Temporary firmware path: `/tmp/lightwave_regtest_v2`
- Only code delta applied:
  - `I2S_BCLK_PIN: 13 -> 14`
  - `I2S_DIN_PIN: 12 -> 13`
  - `I2S_DOUT_PIN: 12 -> 13`
  - `I2S_LRCL_PIN: 14 -> 12`
  - Applied for both `FEATURE_AUDIO_BACKEND_PIPELINECORE` and default `#else` branch in `audio_config.h`.
- Build/flash result: success (`esp32dev_audio_pipelinecore`).
- Observed raw capture signature after flash:
  - `LEFT raw=[00000000..00000000]`
  - `RIGHT raw=[F9B38000..FA210000]` (non-zero and varying)
- Observed post-capture metrics:
  - `DC_DIAG` no longer zero (`mean/min/max/rms` varying, `zeros=0`)
  - `adbg status`: non-zero RMS/flux
  - `adbg spectrum`: non-zero 8-band/chroma/bins256
- Conclusion: capture restored by pin map rollback alone.

### A' (Reflash current repo config back)
- Reflashed from workspace `firmware/v2` current `esp32dev_audio_pipelinecore`.
- Failure returned immediately:
  - `LEFT raw=[00000000..00000000]`
  - `RIGHT raw=[00000000..00000000]`
  - `DC_DIAG mean/min/max/rms = 0`
  - `adbg status RMS/Flux = 0`
- Conclusion: A/B/A confirmed. The regression is strongly tied to current I2S pin mapping in `audio_config.h`, not PipelineCore DSP internals.

### Confidence
- Root-cause confidence: **high** for GPIO mapping regression in `audio_config.h`.
- Secondary contributors (sample-rate/watchdog/timing) may still affect quality/performance but are not primary cause of hard all-zero capture.
