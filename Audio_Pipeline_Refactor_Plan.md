 Audio Pipeline Comprehensive Audit Plan                                        

 Context

 While fixing the silence gate bug in RendererActor (es_vu_level_raw AGC contamination), we discovered a fundamental architectural problem: the ES v1.1 backend bypasses the ControlBus processing pipeline entirely.
 EsV11Adapter::buildFrame() writes directly to a ControlBusFrame and publishes it (AudioActor.cpp:235→251), skipping all 7 ControlBus processing stages. This means every field in ControlBusFrame has different processing
 characteristics depending on which backend is active, and effects consuming these fields may be making incorrect assumptions.

 The audit must systematically uncover every erroneous assumption about signal characteristics, AGC amplification, noise gating, timing semantics, and broken functions/connections before any further development proceeds.

 ---
 Phase 1: Signal Identity & Backend Parity Audit (FOUNDATION)

 Goal: For every field in ControlBusFrame, document what populates it in each backend path, and identify all semantic mismatches.

 Files (6):
 - src/audio/contracts/ControlBus.h — ControlBusFrame field declarations (lines 80-164)
 - src/audio/contracts/ControlBus.cpp — UpdateFromHop() 7-stage pipeline (lines 278-564)
 - src/audio/backends/esv11/EsV11Adapter.cpp — buildFrame() direct population (lines 39-258)
 - src/audio/backends/esv11/EsV11Backend.h — EsV11Outputs struct (ES raw signal definitions)
 - src/audio/AudioActor.cpp — ES path (line 235: buildFrame→Publish) vs LWLS path (line 1407: UpdateFromHop→GetFrame→Publish)
 - src/audio/AudioTuning.h — Tuning presets and defaults

 Already-confirmed critical findings:

 ┌────────────────┬─────────────────────────────────────────────┬─────────────────────────────────────────┬──────────┐
 │     Field      │                  LWLS Path                  │                 ES Path                 │  Status  │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ rms            │ clamp → EMA(0.35)                           │ sqrt(vu_level) * 1.25 (no smoothing)    │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ flux           │ clamp → EMA(0.12)                           │ clamp(novelty_norm_last) (no smoothing) │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ fast_rms       │ unsmoothed clamp                            │ = rms (identical)                       │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ fast_flux      │ unsmoothed clamp                            │ = flux (identical)                      │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ bands[0..7]    │ clamp → despike → ZoneAGC → attack/release  │ autorange(bins64) → mean(8 bins)        │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ heavy_bands    │ same + heavy attack/release                 │ simple EMA(0.05)                        │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ chroma[0..11]  │ clamp → despike → ChromaZoneAGC → smoothing │ autorange follower → clamp              │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ bins64         │ clamp passthrough                           │ autorange normalised                    │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ bins64Adaptive │ separate adaptive normalisation             │ = bins64 (identical)                    │ MISMATCH │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ silentScale    │ rmsUngated → hysteresis → fade              │ Always 1.0 (never set)                  │ MISSING  │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ isSilent       │ derived from silence detection              │ Always false (never set)                │ MISSING  │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ saliency.*     │ computeSaliency()                           │ All zeros                               │ MISSING  │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ chordState     │ detectChord()                               │ All zeros                               │ MISSING  │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ liveliness     │ tempo + flux computation                    │ Always 0.0                              │ MISSING  │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ currentStyle   │ StyleDetector                               │ UNKNOWN                                 │ MISSING  │
 ├────────────────┼─────────────────────────────────────────────┼─────────────────────────────────────────┼──────────┤
 │ snare/hihat*   │ GoertzelAnalyzer                            │ Always 0.0/false                        │ MISSING  │
 └────────────────┴─────────────────────────────────────────────┴─────────────────────────────────────────┴──────────┘

 Deliverable: Complete Signal Identity Matrix with backend parity verdict per field.

 ---
 Phase 2: AGC Contamination & selectChroma12() Audit

 Goal: Find every place AGC-amplified signals are used for threshold decisions, and fix the systematic selectChroma12() bias.

 2a: selectChroma12() pattern (8 non-reference effects)

 The pattern (esSum > lwSum + 0.001f) ? es_chroma_raw : chroma appears in 8 effects. This systematically prefers AGC-amplified ES raw chroma because AGC increases total energy even when musical content is identical.

 Files (8 effects + 2 reference effects that are intentional):
 1. src/effects/ieffect/AudioBloomEffect.cpp:36-41
 2. src/effects/ieffect/LGPBeatPulseEffect.cpp:27-30
 3. src/effects/ieffect/LGPBassBreathEffect.cpp:26-29
 4. src/effects/ieffect/LGPSpectrumBarsEffect.cpp:26-29
 5. src/effects/ieffect/HeartbeatEsTunedEffect.cpp:30-33
 6. src/effects/ieffect/LGPHolographicEsTunedEffect.cpp:38-41
 7. src/effects/ieffect/LGPPhotonicCrystalEffect.cpp:136-139
 8. src/effects/ieffect/AudioWaveformEffect.cpp:38-41
 9. src/effects/ieffect/RippleEsTunedEffect.cpp:123 (direct access, different pattern)
 10. src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp:101 (direct access)
 - Intentional (ES reference parity): esv11_reference/EsOctaveRefEffect.cpp:48, esv11_reference/EsBloomRefEffect.cpp:67, esv11_reference/EsSpectrumRefEffect.cpp:55

 Action: For each non-reference effect, determine whether es_chroma_raw adds value or just introduces AGC bias. If the normalised chroma from ControlBus/EsV11Adapter is sufficient, remove the fallback to raw.

 2b: es_vu_level_raw consumers

 Already fixed: RendererActor.cpp silence gate (switched to post-gate rms/flux)
 Intentional: EsAnalogRefEffect.cpp:40 (ES reference parity)
 Debug only: RendererActor.cpp:1265 (debug log, harmless)

 2c: ES rms derivation risk

 rms = sqrt(vu_level) * 1.25 where vu_level is AGC-amplified. Does this mean rms stays elevated in silence? Need to verify: does the ES backend's AGC have its own noise gate or floor?

 File: src/audio/backends/esv11/EsV11Backend.cpp — trace what vu_level represents and whether it has noise floor handling.

 Deliverable: AGC contamination report with fix recommendations per file.

 ---
 Phase 3: Silence Gate & Effect silentScale Audit

 Goal: Map all silence/gate mechanisms and identify where they fail in the ES path.

 3a: Three-tier gate architecture

 ┌───────────────────────┬─────────────────────────────┬─────────────────────┬──────────────────────────────────────────────────────────────┐
 │         Gate          │          Location           │        Input        │                        ES Path Status                        │
 ├───────────────────────┼─────────────────────────────┼─────────────────────┼──────────────────────────────────────────────────────────────┤
 │ ControlBus silence    │ ControlBus.cpp:532-563      │ rmsUngated          │ BYPASSED (ES never calls UpdateFromHop)                      │
 ├───────────────────────┼─────────────────────────────┼─────────────────────┼──────────────────────────────────────────────────────────────┤
 │ RendererActor global  │ RendererActor.cpp:1545-1553 │ silentScale         │ NON-FUNCTIONAL (always 1.0 in ES path)                       │
 ├───────────────────────┼─────────────────────────────┼─────────────────────┼──────────────────────────────────────────────────────────────┤
 │ RendererActor high-ID │ RendererActor.cpp:1558-1613 │ rms, flux, isSilent │ PARTIALLY WORKING (rms/flux work, but isSilent always false) │
 └───────────────────────┴─────────────────────────────┴─────────────────────┴──────────────────────────────────────────────────────────────┘

 3b: Effects with their own silentScale usage (7 effects)

 These effects read silentScale directly, creating effect-level gating ON TOP OF the RendererActor global gate. In the ES path, silentScale is always 1.0, so these effect-level gates never activate:

 1. LGPBeatPulseEffect.cpp:190-192 — multiplies pulseIntensity by silentScale
 2. HeartbeatEsTunedEffect.cpp:117-243 — multiplies lubIntensity, dubIntensity, core by silentScale
 3. LGPSpectrumBarsEffect.cpp:70 — multiplies band target by silentScale
 4. AudioBloomEffect.cpp:182-200 — multiplies chroma bins by silentScale
 5. LGPBassBreathEffect.cpp:76-126 — multiplies targetBreath by silentScale
 6. AudioWaveformEffect.cpp:161 — multiplies currentAmp by silentScale
 7. LGPHolographicEsTunedEffect.cpp:105-213 — multiplies g1-g4 by silentScale
 - BeatPulseBloomEffect.cpp:90 — explicitly notes "renderer handles it globally" (correct pattern)

 Action: Determine whether these effects should rely on RendererActor's global gate exclusively (like BeatPulseBloomEffect does), or whether per-effect gating serves a distinct purpose.

 Deliverable: Gate architecture diagram + recommendation for ES silence detection implementation.

 ---
 Phase 4: dt-Correctness Sweep

 Goal: Verify every effect that uses deltaTime for audio-coupled maths uses signalDt()/getSafeRawDeltaSeconds() (unscaled by SPEED), not visualDt()/getSafeDeltaSeconds() (SPEED-scaled).

 Rule:
 - Audio-coupled maths (envelope followers, EMA on audio signals, beat decay, physics): MUST use rawDeltaTimeSeconds
 - Visual motion (position transport, animation speed, scrolling): SHOULD use deltaTimeSeconds

 Files using AudioReactivePolicy correctly (10 — verify signalDt vs visualDt usage):
 - KuramotoTransportEffect, LGPExperimentalAudioPack, BeatPulseVoidEffect, BeatPulseCore, SnapwaveLinear, SbWaveform310Ref, BeatPulseBloom, WaveformParity

 Files using deltaTimeSeconds directly (~58 effects): Grep for deltaTimeSeconds|rawDeltaTimeSeconds|getSafeDelta across all effects and verify each usage.

 Deliverable: dt-correctness report per effect file.

 ---
 Phase 5: Full Consumer Audit (All Audio-Reactive Effects)

 Goal: For each of the ~57 audio-consuming effects, verify signal usage assumptions hold under both backends.

 Check per effect:
 1. What audio signals does it read? (rms, flux, bands, chroma, bins64, etc.)
 2. Does it assume rms and fast_rms differ? (Identical in ES path)
 3. Does it assume bins64 and bins64Adaptive differ? (Identical in ES path)
 4. Does it use saliency/chord/style features? (All zeros in ES path)
 5. Does it use snare/hihat onset detection? (Always 0/false in ES path)
 6. Does it make threshold decisions that depend on a specific signal range?

 Approach: Split effects into batches of ~20, audit with parallel agents. Focus first on effects with ID >= 140 (high-ID reactive gate applies).

 Deliverable: Consumer compatibility matrix.

 ---
 Phase 6: Cross-Cutting Verification

 Files:
 - src/plugins/api/EffectContext.h:65-336 — Verify all AudioContext convenience accessors correctly map to ControlBusFrame fields
 - src/effects/zones/ZoneComposer.cpp — Verify audio context passes through unchanged
 - src/core/actors/RendererActor.cpp:1277-1309 — Verify AudioContext population is identical for Trinity vs live path
 - src/effects/ieffect/AudioReactivePolicy.h — Verify signalDt/visualDt contract documentation
 - src/effects/ieffect/BeatPulseRenderUtils.h — Verify dt usage in transport/decay functions

 ---
 Expected P0 Fixes (from audit)

 1. ES path has no silence detection. Must implement silence detection in EsV11Adapter or route ES frames through ControlBus for silence/saliency stages.
 2. selectChroma12() AGC bias in 8 effects. Sum-of-energy comparison systematically prefers AGC-amplified data. Remove or fix the heuristic.
 3. ES rms may not reach zero in silence if vu_level is AGC-amplified. Verify and fix.

 Expected P1 Fixes

 4. Missing saliency/chord/style/liveliness/onset detection in ES path — effects using MIS features see all zeros.
 5. bins64 and bins64Adaptive identical in ES path — effects expecting distinct spectra get the same data.
 6. Per-effect silentScale gating is non-functional in ES path.

 Execution

 Phases 1-3 are the critical path. Phase 1 establishes the signal identity contracts that all subsequent phases verify against. Phases 4-6 can run in parallel after Phase 1 completes. Recommend parallel agent teams for Phase 5 (57
  files).