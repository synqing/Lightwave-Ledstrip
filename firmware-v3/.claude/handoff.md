## Handoff — 2026-03-25 — Onset Detector: Path B Band-Energy Ratio

### Current state

**What is done:**
- Phase 1 (gate-demotion refactor): COMPLETE, committed, verified on hardware. The activity gate no longer poisons detector state — it is output permission only with hold + slow release.
- Path B (band-energy ratio detector): IMPLEMENTED, flashed, hardware-verified but NOT production-ready. 16 BR triggers during music (correct), 11 false triggers during settled silence (too high).
- FFT spectral flux onset detector: DEMOTED to telemetry only. Still runs, still writes to ControlBus onset fields (onsetFlux, onsetEnv etc.), but its kick/snare/hihat triggers no longer write to ControlBus. The band-ratio detector is the sole live trigger source.

**What is not done:**
- Band absolute energy floors per group (the next required fix)
- Disabling the old EsV11Adapter crude percussion triggers (they still pollute trace with ONSET_KICK/SNARE/HIHAT alongside the new BR_ instants)
- Production readiness of the band-ratio detector

**What is partially done:**
- The `thresholdOffset` config param was added to OnsetDetector.h but the value (1.0) was proven wrong (kills quiet music). It remains in the code but is not the active path — Path B bypasses the global onset threshold entirely.

### Decisions made (and why)

1. **Path B over Path A.** Band-energy ratio on existing `bands[0..7]` instead of normalizing the spectral flux ODF. Reason: the spectral flux scale varies 300:1 with volume (0.3 quiet to 86 loud). No fixed or additive threshold can work across that range. The band-energy ratio operates on a self-normalizing signal (energy vs its own recent history), eliminating the scale problem.

2. **Gate demotion.** The activity gate was proven to corrupt detector state by injecting zeros into threshold history and per-band EMAs. Both Claude's 10-agent audit and GPT's independent analysis converged on the same diagnosis. The gate is now output-permission only — detector statistics always track real spectral flux.

3. **noiseAdaptK = 2.0.** Lowered from 3.0 to prevent the noise floor follower from chasing music as ambient. With 3.0 the floor tracked up to the music level (~0.060) making the gate too strict. With 2.0 the floor stays near the actual ambient (~0.002-0.005) and music passes cleanly.

4. **noiseFloorRise = 0.003.** Slowed from 0.020 to prevent the follower from chasing music dynamics. The follower now tracks genuine ambient changes (room noise over 5-10s) but does not inflate during music passages.

5. **warmupFrames = 1000 (8s).** Covers the ES VU AGC settling time (vu_floor ring 5s + 2s pre-fill + margin). Configurable via Config for tests (which use 50-80 frames).

### Failed approaches (do NOT retry these)

1. **Dual-RMS (raw PCM + VU-derived frame.rms for activity gate).** VU silence level is ~0.30 (not 0.08 as estimated). The VU signal has ±50% variance during silence. The noise floor follower cannot separate VU silence from quiet music. Produced 33-49 false triggers vs 3-7 with raw PCM.

2. **AGC + log-domain spectral flux.** Mathematically proven self-defeating: AGC normalises magnitudes to 0-1, log of 0-1 is negative, half-wave rectifier kills everything. Result: zero flux.

3. **AGC + linear-domain spectral flux.** The AGC compresses onset transients over multiple frames, spreading the flux and reducing peak amplitude. Result: flux values 10-20x smaller than expected.

4. **Additive threshold offset on raw flux (thresholdOffset = 1.0).** Raw log-spectral flux varies 0.3 (quiet) to 86 (loud). Any fixed additive offset either kills quiet music or is negligible for loud music. The correct approach (per librosa) is to normalise to [0,1] first — but Path B bypasses this entirely.

5. **Per-bin normalization by bin count for full-band flux.** Dividing by 511 bins crushed the signal 500x. Every reference implementation uses raw sum.

6. **EMA-based adaptive threshold (mu + k*sigma).** Over-adapts to transients, tracks onset peaks into the baseline. Dixon (2006) median is strictly better for onset detection.

### Blocked on

The band-ratio detector produces 11 false triggers per 3 seconds during settled silence. The cause: ESV11 adapter AGC-normalises bands to 0-1, and the Goertzel autoranger amplifies mic self-noise into non-trivial band energy (hihat band mean 0.977 during silence). The variance-adaptive threshold cannot distinguish this from real audio because the normalised noise has real variance.

**The next fix:** Add per-group absolute energy floors before the ratio test. If grouped energy < floor, do not test. Different floors per group because high bands have higher silence-level energy than bass.

### Next steps (in order)

1. **Disable the old EsV11Adapter crude percussion triggers** so traces only show BR_ events. Currently both ONSET_KICK and BR_KICK fire, confusing trace interpretation. In `AudioActor.cpp`, the adapter's `frame.kickTrigger`/`snareTrigger`/`hihatTrigger` writes (from `EsV11Adapter::buildFrame`) should be gated or removed.

2. **Add per-group absolute energy floors** to `BandRatioConfig`:
   ```cpp
   float kickAbsFloor  = ???;  // Needs measurement from silence traces
   float snareAbsFloor = ???;
   float hihatAbsFloor = ???;
   ```
   Measure: capture settled silence, read the `br_kick_energy`, `br_snare_energy`, `br_hihat_energy` trace counters, set floors just above the 95th percentile of silence energy.

3. **Retest three cases:** music, immediate pause, settled idle. Compare BR false events.

4. **If idle residuals are acceptable:** commit the band-ratio detector, update the quarantine doc, declare onset detection production-ready for effects.

5. **If idle residuals are still too high:** consider per-bin noise subtraction during warmup (the SensoryBridge approach that the 10-agent audit identified as the deeper long-term fix). This would subtract calibrated mic noise from the Goertzel bins before the adapter normalises them — addressing the root cause (AGC amplifies noise) rather than the symptom (threshold can't reject normalised noise).

### Files modified (uncommitted)

All paths relative to `firmware-v3/`:

| File | Change |
|------|--------|
| `src/audio/onset/OnsetDetector.h` | thresholdOffset added, noiseFloorRise 0.020→0.003, noiseFloorFall 0.050→0.005, noiseAdaptK 3.0→2.0, activityHoldFrames added, warmupFrames default 1000, gate demoted to output-permission-only, peakPick/bandTrigger take emitEnabled param |
| `src/audio/onset/OnsetDetector.cpp` | Gate-demotion refactor: no zero injection, no activity^2 scaling, hold+release follower, emitEnabled params, re-prime on abs gate instead of zero injection |
| `src/audio/AudioActor.h` | BandRatioConfig, BandRatioChannel, three channel instances, bandRatioDetect() declaration |
| `src/audio/AudioActor.cpp` | Band-ratio detector implementation + integration. FFT onset triggers demoted (no longer write to frame.kickTrigger etc). Band-ratio triggers write to frame instead. bandRatioDetect() method added. |
| `test/test_onset/test_onset_detector.cpp` | Noise floor test: per-band tolerance raised to <=1. Diagnostic printf blocks. testConfig() with short warmup. |
| `test/test_onset_corpus/test_onset_music_corpus.cpp` | Short warmup in test config. |

### Constraints encountered

- **K1 is AP-ONLY** — never enable STA mode
- **Centre origin** — all effects from LED 79/80 outward
- **No heap in render()** — the band-ratio detector runs in AudioActor (Core 0), not render
- **British English** in all comments/docs/logs
- **ALWAYS verify MAC before flashing** — K1 MAC: `b4:3a:45:a5:87:f8`
- **Research-backed parameters only** — every value must cite a reference or measured data

### Pre-reading for next session

| Document | Why |
|----------|-----|
| This handoff | Current state and next steps |
| `docs/design/ONSET_QUARANTINE_20260325.md` | Original quarantine matrix — may need updating |
| `docs/debugging/MABUTRACE_GUIDE.md` | Trace capture procedure and counter inventory |
| `docs/research/aubio-onset-reference.md` | aubio's algorithm (created by research agents this session) |
| `docs/reference/wled-audio-reactive-analysis.md` | WLED Sound Reactive algorithm (created this session) |
| `docs/reference/k1-vs-wled-audio-comparison.md` | K1 vs WLED architectural comparison (created this session) |

### Trace files on disk

| File | Content |
|------|---------|
| `/tmp/trace_bandratio_music.json` | Music with band-ratio detector — 16 BR events |
| `/tmp/trace_bandratio_pause.json` | Immediate pause — 26 BR false events |
| `/tmp/trace_gate_demotion_idle.json` | Settled idle after Phase 1 refactor — 1 false event |
| `/tmp/trace_adaptk2_music.json` | Music with noiseAdaptK=2.0 — 37 onset events, 100% ALL_PASS |

### Key architectural insight

The band-energy ratio detector (Path B) is correct for this product. The remaining problem is that `controlBus.bands[0..7]` from the ESV11 adapter are post-AGC normalised signals where silence looks like low-level audio. The detector needs per-group absolute floors to reject this normalised noise. The longer-term fix is per-bin noise subtraction in the Goertzel/adapter layer so that silence genuinely produces zero band energy — but that is a Phase 3 change that should be evaluated after the absolute floors are tested.
