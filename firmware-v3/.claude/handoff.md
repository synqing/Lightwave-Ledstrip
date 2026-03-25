## Handoff — 2026-03-25 — Onset Detector: Candidate V1 Live Path

### Current state

**Candidate V1 architecture (FROZEN — validate, do not modify):**

```
Raw PCM hop RMS  →  brRmsGate (0.015)  →  eligible?
                                              │
                                    YES       │       NO
                                    ↓         │       ↓
                          BR detector on      │    suppress all
                          grouped bands       │    BR triggers
                              ↓               │
                    variance-adaptive         │
                    threshold + refractory    │
                              ↓               │
                    kick/snare/hihat          │
                    trigger events            │
```

Two jobs, two signals:
- **Raw PCM RMS** answers "any acoustic energy?" (physical presence)
- **Band-ratio detector** answers "which percussion event?" (event classification)

**What is done:**
- Phase 1 (gate-demotion refactor): COMPLETE, committed, verified
- Path B (band-energy ratio detector): IMPLEMENTED with raw RMS eligibility gate
- FFT spectral flux onset detector: DEMOTED to telemetry only
- Old EsV11Adapter crude percussion triggers: DISABLED (always false)
- Raw PCM RMS eligibility gate: IMPLEMENTED, measured, stop-rule PASS

**Stop-rule results (2026-03-25, brRmsGate=0.015):**

| Capture | BR Events | RMS Eligible | Previous (no gate) |
|---------|-----------|-------------|-------------------|
| Settled idle | 0 | 0.0% | 11 false events |
| Music | 12 | 100.0% | 16 events |
| Post-pause | 0 | 0.0% | 26 false events |

**Measured RMS distributions:**
- Silence: p50=0.003, p95=0.007, max=0.009
- Music: p5=0.021, p50=0.042, max=0.128
- Gate at 0.015: 2.3x above silence max, 1.4x below music p5

**What is NOT done:**
- Validation matrix (quiet music, ambient noise, longer soak, varied content)
- Production readiness declaration
- Committing changes (all uncommitted)
- Disabling per-group absFloor values (currently 0.01, harmless but redundant)

### Decisions made (and why)

1. **Raw RMS gate over absolute band floors.** Four boot cycles proved AGC-normalised silence energy varies 0.13–2.19 across boots and overlaps with music energy (0.08–1.14). No fixed floor can separate them. Raw PCM RMS has 10x+ separation (silence 0.009 vs music 0.021).

2. **Gate at 0.015.** Measured from K1v2 traces. Silence max=0.009, music p5=0.021. Gate sits at the midpoint with margin on both sides.

3. **Simple veto, not state conditioning.** The gate does NOT: scale BR energies, rewrite threshold history, inject zeros, or adapt dynamically. It is a boolean eligibility check. This avoids the state-poisoning class of bugs that plagued the original onset architecture.

4. **Per-group absFloor values kept at 0.01.** Effectively harmless as a secondary safety net. The RMS gate does all the real work. These can be removed later if desired.

### Failed approaches (do NOT retry these)

(All previous entries from the prior handoff remain valid, plus:)

5. **Per-group absolute energy floors as primary defence.** Measured silence energies across four K1v2 boots: kick max 0.13→1.04, snare max 0.34→1.93, hihat max 1.00→2.19. AGC convergence varies per boot — no fixed floor works. Floors high enough to reject silence also kill music recall. The domain is structurally unsuitable for absolute thresholding.

### Next steps (validation matrix)

1. **Quiet music** — soft passages, low volume. Verify RMS stays above 0.015.
2. **Ambient non-music noise** — speech, tapping, fan. Verify gate doesn't admit too much.
3. **Longer soak** — 1-5 min silence, 1-5 min music, multiple pause/resume cycles.
4. **Varied content** — kick-heavy, hihat-heavy, sparse/ambient, low-volume.
5. **If all pass** → commit, update quarantine doc, declare Candidate V1 production-ready.
6. **If quiet music fails** → consider lowering gate (margin to silence allows it) or Option 1 (per-bin noise subtraction).

### Files modified (uncommitted)

All paths relative to `firmware-v3/`:

| File | Change |
|------|--------|
| `src/audio/onset/OnsetDetector.h` | Unchanged from prior session |
| `src/audio/onset/OnsetDetector.cpp` | Unchanged from prior session |
| `src/audio/AudioActor.h` | BandRatioConfig: added brRmsGate=0.015, per-group absFloors=0.01, bandRatioDetect takes absFloor param |
| `src/audio/AudioActor.cpp` | rawHopRms hoisted to shared scope, BR block gated by rmsEligible veto, trace counters for br_raw_rms and br_rms_eligible added |
| `src/audio/backends/esv11/EsV11Adapter.cpp` | Old crude percussion triggers disabled (snareTrigger/hihatTrigger = false) |
| `test/test_onset/test_onset_detector.cpp` | Unchanged from prior session |
| `test/test_onset_corpus/test_onset_music_corpus.cpp` | Unchanged from prior session |

### Constraints encountered

- **K1 is AP-ONLY** — never enable STA mode
- **Centre origin** — all effects from LED 79/80 outward
- **No heap in render()** — BR detector runs in AudioActor (Core 0), not render
- **British English** in all comments/docs/logs
- **ALWAYS verify MAC before flashing** — K1 MAC: `b4:3a:45:a5:87:f8`
- **Research-backed parameters only** — brRmsGate=0.015 from measured traces, not guessed
- **Opening ESP32-S3 USB CDC serial port RESETS the board** — affects all trace captures

### Key architectural insight

The fundamental problem was asking one signal to do two jobs. AGC-normalised band energy cannot answer "is there real acoustic energy?" because the AGC amplifies silence into the music energy range. Raw PCM RMS preserves the physical silence/music distinction (10x+ separation). The band-ratio detector is correct for event classification once it is protected by a physical presence gate.
