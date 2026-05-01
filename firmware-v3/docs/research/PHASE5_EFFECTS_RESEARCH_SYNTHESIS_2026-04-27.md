---
abstract: "Distilled synthesis of 20-SSA parallel research wave (2026-04-27) covering Phase 5 audio-reactive effect redesign for Moves 5.4/5.6/5.7. Captures the Effect Development Standard mandates, 5-layer audio model, canonical smoothing primitives with concrete tau values, audio-source field-reality table (which bus fields are continuous vs impulsive vs always-zero), the Feb-2026 argmax→circular-chroma migration, the proven impossibility of the original Move 5.7 spawn gate (tempoBeatTick + phase=0.65 cannot co-occur), brand-voice WHAT-IS-THAT criteria, PSRAM policy, mabutrace conventions, and reference-effect anatomy from 6 canonical AR implementations. Read this before redesigning, modifying, or reviewing any audio-reactive effect — it consolidates findings that were scattered across 9 docs and 30+ source files."
---

# Phase 5 Effects Research Synthesis (20-SSA wave, 2026-04-27)

This document consolidates the research findings from twenty parallel sub-agent investigations conducted on 2026-04-27. The wave was triggered by hardware failures of three new audio-reactive effects (Moves 5.4 / 5.6 / 5.7) where the implementations were research-paper-clean but visually catastrophic on hardware. The synthesis below identifies the systemic gaps that produced those failures and the canonical patterns that should have been used.

---

## 1. The Effect Development Standard mandates (Part 1: §2 + §3 + §4 + §6 + §7)

Every audio-reactive effect MUST meet these baselines per `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md`:

- **§2.1 Frame-rate independent timing** — `ctx.getSafeDeltaSeconds()` clamped to [0.0001, 0.05]. Hardcoded per-frame increments forbidden.
- **§2.2 Trail persistence via `fadeToBlackBy()`** — every animated effect MUST fade BEFORE drawing. Audio-reactive: dynamic fade `20 + 60*(1-energy)` so loud=short, quiet=long. Standard value across the 1A0x AR pack: **30** (≈12% per frame at 120 FPS).
- **§2.3 Centre-origin** — iterate `for d in [0, HALF_LENGTH)`; write via `SET_CENTER_PAIR(ctx, d, color)` or quartet manual writes. Linear sweep `for i in [0, 160)` FORBIDDEN.
- **§2.4 Palette via `ctx.palette.getColor(idx, brightness)`** — hardcoded `CHSV(...)` / `CRGB(r,g,b)` is "DEAD — ignores palette." Exception only for physics effects with `isLGPSensitive()=true`.
- **§2.5 Brightness via `scale8` / `nscale8_video`** — float divide-by-255 multiplies are perf regressions and gamma-naïve.
- **§2.6 Layered light via `qadd8` / `+=`** — raw `ctx.leds[i] = col` after fadeToBlackBy KILLS the trail (overwrites the faded base). Use `+=` (CRGB::operator+= is qadd-saturating per channel).
- **§7.1 No raw audio → pixel** — every audio value passes through a smoothing primitive before reaching colour or position.
- **§7.2 No constant-tweaking when the architecture is missing trails / subpixel / asymmetric envelopes** — fix the architecture FIRST.
- **§7.4 No dynamic alloc in render()** — static buffers only.

**Critical empirical violation rate in the 3 broken effects:** all three failed §2.4 (palette), all three failed §2.5 (scale8), 5.4 + 5.6 failed §2.6 (overwrite kills trail), all three failed §3.x (rolled own EMA instead of using `enhancement::AsymmetricFollower`), 5.6 + 5.7 failed §6.2 (integer position sampling = wagon-wheel aliasing).

---

## 2. The 5-layer audio model (mandatory per `NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md`)

Every audio-reactive effect decomposes into ALL FIVE layers — never bolt audio onto one layer.

| Layer | Source signals | Smoothing tau | Purpose |
|---|---|---|---|
| **L0 Bed** | rms, heavy_bands | 0.20–0.80 s | Ambient ground state. Keeps strip alive in silence. |
| **L1 Structure** | spectralFlux, fastFlux, timbralSaliency, rhythmicSaliency | 0.08–0.30 s | Quick response to texture and density change. |
| **L2 Impact** | beatStrength, isOnBeat(), kickTrigger, snareTrigger, hihatTrigger | 0.12–0.30 s decay | Discrete events. Spawn-class triggers. |
| **L3 Tonal** | rootNote, chordConfidence, heavy_chroma | 0.30–0.80 s + Schmitt hysteresis 0.40 open / 0.25 close | Colour anchor. Schmitt prevents chord-flicker. |
| **L4 Memory** | integrated impact + novelty | 0.40–1.60 s | Story tail. Persistence beyond per-event decay. |

**The 16-control schema** (every effect exposes the same knobs so iOS/Tab5 surfaces stay coherent): `audio_mix`, `beat_gain`, `attack_s`, `release_s`, `motion_rate`, `motion_depth`, `colour_anchor_mix`, `event_decay_s`, `memory_gain`, `silence_hold` (10 critical of 16).

**Two clocks, not one:** `AudioReactivePolicy::signalDt(ctx)` for envelope/hysteresis math; `AudioReactivePolicy::visualDt(ctx)` for pure visual drift.

**Audio presence + graceful silence:** `m_audioPresence = trackAudioPresence(...)`; below 0.001 → fadeToBlackBy(30) and degrade to internal motion (NEVER freeze).

---

## 3. ControlBus field reality table (from `audio-visual-semantic-mapping.md` + onset hardening session)

The most expensive failure modes in this session came from misreading WHICH bus fields are continuous vs impulsive vs always-zero.

| Field | Reality | Effect implication |
|---|---|---|
| `tempoBeatTick`, `es_beat_tick`, `tempoDownbeatTick`, `es_downbeat_tick` | **Impulsive** — true on 1 frame per beat (~60 frames silence between ticks @ 120 FPS / 120 BPM) | Use as edge trigger only. NEVER a primary modulation signal. |
| `tempoBeatStrength`, `es_beat_strength` | **Impulsive magnitude** — non-zero only on tick frames | Edge triggers only. |
| `kickTrigger`, `snareTrigger`, `hihatTrigger` | **Impulsive bool** — band-ratio detector, no tempo-lock dependency. Fires reliably on percussive content. | Best primary spawn source for sprite/event effects. **NOT** the FFT detector (FFT triggers were demoted to TRACE-only telemetry per AudioActor.cpp:748). |
| `onsetEvent` | **Impulsive magnitude** — 0 most frames, strength on event | Edge trigger with magnitude. |
| `onsetEnv` | **Mostly zero on non-percussive content** (chord/pad). Spikes briefly above median+1.0 threshold. Officially "advanced/debug-oriented" per ADR_2026-03-25. | Wrong choice for "audio drive level". Use `fast_rms` instead. |
| `onsetFlux` | **Continuous** raw log-spectral flux, sum 62 Hz – 4 kHz | Spike overlay on top of a continuous floor, not a primary continuous signal. |
| `chordState.confidence`, `styleConfidence` | **Slow-changing** — non-zero once detector warms (~few seconds) | Continuous gate with hysteresis. |
| `saliency.*Novelty` (raw) | **Spiky** — peaks during change events | Use `*Smooth` variants instead. |
| `saliency.*NoveltySmooth` | **Continuous** — asymmetric attack/release | Safe to read every frame. |
| `audioConfidence` | **Continuous, default 1.0** — fades over 200-500ms | Multiplier, NOT gate. |
| `silentScale` | **Continuous, default 1.0** — fades over 10s of silence | Multiplier. |
| `rms`, `fast_rms`, `flux`, `fast_flux` | **Continuous** | Primary continuous scalars. fast_rms is ~100ms reactive (α_fast=0.35). |
| `bands[8]`, `chroma[12]`, `heavy_*`, `bins64`, `bins256` | **Continuous arrays** | Primary continuous spectra. AGC-normalised per zone. |
| `tempoBpm`, `es_bpm`, `tempoConfidence`, `es_tempo_confidence` | **Continuous, slow** | Mood/palette use. Runtime gate threshold = 0.30 per `m2_adversarial/expected_results.md:127`. |
| **`es_phase01_at_audio_t`** | **Continuous [0,1)** — linear ramp 0→1 between beats. **The underutilised gem for smooth beat-synced motion.** | Use for "between-beat glide" — sprites/elements moving smoothly along beat phase. |
| **`es_beat_in_bar`** | **Continuous uint8** (latched until next beat) | The actual parity counter. `& 1` gives parity bit. |
| `timing_jitter`, `syncopation_level`, `pitch_contour_dir` | **Continuous, smoothed** | Stage-2 motion-semantic modulators. |

---

## 4. Tempo/beat/phase alignment — proven from source

Per `EsV11Backend.cpp:312-321`:

```cpp
m_beatPhase += phaseInc;
if (m_beatPhase >= 1.0f) {
    m_beatPhase -= floorf(m_beatPhase);  // wraps to ~0
    tick = true;                          // tick fires HERE
}
out.beat_tick = tick;
out.phase_radians = (m_beatPhase * 2 - 1) * π;  // computed AFTER wrap
```

**Critical implication:** on a tick frame, `es_phase01_at_audio_t ≈ 0` (the per-hop increment, ~0.008-0.02), NEVER ~0.65. The two events are separated by ~65% of a beat period (~325 ms at 120 BPM). **A spawn gate that requires `tempoBeatTick && phase01 ≈ 0.65` in the same frame is mathematically impossible.**

The original Move 5.7 spec (BeatParityBloomSpriteInjection) made this exact mistake. Its source (PASS_1_DOMAIN_REGISTRY.md:455-456) cites ES `beat_tunnel` doctrine which gates on **phase-crossing-0.65 ALONE**, NOT conjoined with a tick. K1 incorrectly conjoined them with AND. Result: the sprite never spawned.

**Correct alternatives (ranked):**
1. Use `bus.kickTrigger` as primary spawn source (band-ratio detector, no tempo-lock dependency) + tempoBeatTick as accent — matches LGPBeatPrismOnset family.
2. Phase-crossing-0.65 alone + `tempoConfidence ≥ 0.30` gate, with parity latched separately on each tick.
3. `AudioReactivePolicy::audioTrigger(HybridTempoTransient, ...)` — beat → kick → fallback metronome.

---

## 5. Smoothing infrastructure — exact taus from canonical effects

All four primitives live in `firmware-v3/src/effects/enhancement/SmoothingEngine.h` (no separate AsymmetricFollower.h / Spring.h / ExpDecay.h files exist — they're nested structs in this one header).

### `enhancement::AsymmetricFollower` — the most important AR primitive (per §3.2: "single most important smoothing primitive for audio-reactive effects")

Members: `value`, `riseTau` (default 0.05), `fallTau` (default 0.30). API: `update(target, dt)` or `updateWithMood(target, dt, moodNorm)`. dt-correct via `1 - expf(-dt/tau)`.

**Concrete tau values mined from canonical call sites:**

| Signal type | Rise tau | Fall tau | Source |
|---|---|---|---|
| Per-bin chroma envelope | 0.05–0.08 s | 0.20–0.30 s | LGPInterferenceScannerEnhanced, BreathingEnhanced, LGPPerlinVeil |
| RMS / sub-bass envelope | 0.03–0.08 s | 0.20–0.30 s | BreathingEnhanced, SnapwaveLinear |
| Spectral flux / beat strength | 0.05 s | 0.20 s | LGPPerlinVeil |
| Snapwave attack peak | 0.02 s | 0.20 s | SnapwaveLinear |
| Energy average (slow) | 0.20 s | 0.50 s | LGPInterferenceScannerEnhanced |
| Brightness envelope (chord) | 0.08 s | 0.25 s | LGPChordGlow |
| Beat-pulse centre intensity | 0.005–0.010 s | 0.10–0.20 s | (FAST attack — beats are snaps) |

### `enhancement::ExpDecay` — symmetric scalar smoothing
Members: `value`, `lambda` (default 5.0). Factory: `ExpDecay::withTimeConstant(tauSeconds)`. Lambda 2..50 (slow to snap). Use for confidence, hue, dominant-class index, root-note slot.

### `enhancement::Spring` — physics-based motion
Members: `position`, `velocity`, `stiffness`, `damping`, `mass`. **Canonical: `Spring{50, 1.0}` everywhere** (stiffness 50, mass 1, critically damped). Used for "speed" / "scroll rate" parameters that must glide, not snap. Critical-damping default prevents overshoot.

### `enhancement::SubpixelRenderer` — anti-aliased motion (mandatory per §6.2 for ALL moving elements)
Static methods only. `renderPoint(buffer, size, position_float, color, brightness)` distributes brightness across two adjacent LEDs proportional to fractional position via qadd8. `renderLine(...)` for anti-aliased segments. **Integer position rounding produces wagon-wheel aliasing on moving sprites or scrolling waves — always use SubpixelRenderer for motion.**

### `effects::chroma::circularChromaHueSmoothed` — canonical chroma → hue (per Feb 2026 migration)

Located at `firmware-v3/src/effects/ieffect/ChromaUtils.h:94-111`. Treats 12 chroma bins as 12 unit vectors at 30° spacing on a unit circle. Computes weighted vector mean `(c, s) = Σ chroma[i]·(cos_i, sin_i)`, takes `atan2f(s, c)` as hue. Applies circular EMA on shortest arc (wrap-aware). Output: continuous 0..255 hue that varies smoothly even when chroma distribution shifts.

**Caller persists `float m_chromaAngle = 0.0f` instance member.** Tau range: 0.20f standard, 0.25f slower (Heartbeat), 0.30f very stable (LGPHolographic). Silence decay: `m_chromaAngle *= powf(0.995f, rawDt * 60.0f)`.

**Migrated from `argmax(chroma) * 21` pattern Feb 21 2026** across 14+ effects (claude-mem #37140-37142). Argmax fails because two near-equal bins flip dominance and cause hue jumps of up to 7×21=147 units; circular EMA integrates all 12 continuously.

---

## 6. Persistence patterns — three dialects (mutually exclusive per effect)

| Pattern | When to use | Examples | Key idiom |
|---|---|---|---|
| **`fadeToBlackBy` direct LED** | Discrete sprites/spawns, simple effects | BeatPulseRipple, Confetti, BPMEnhanced | `fadeToBlackBy(ctx.leds, count, 25)` THEN `ctx.leds[i] +=` (additive over fade) |
| **PSRAM trail buffer** | Large-history effects with custom decay | SbK1Bloom, SbK1Waveform, BeatPulseBloom | `expf(-decayRate*dt)` per pixel in PSRAM buffer; copy to ctx.leds at end |
| **Substrate ring** | Time-axis-as-radial scopes | RadialTimeScope (LIN-06) | `PSRAMScalarRing<float, 80>` IS the persistence; minimal/no fadeToBlackBy |

**Memory tail tau** (L4 layer): 0.40–1.60 s. `event_decay_s` (control 14) drives impact tail decay; `memory_gain` (control 15) controls accumulation depth; `silence_hold` (control 16) controls persistence in silence.

---

## 7. Brand voice / WHAT-IS-THAT filter (PS-03)

Per `PASS_1_DOMAIN_REGISTRY.md:260` + `Topology_Reconciliation.md §3 C-3` + the SB_ES catalogue kill-list. **No canonical `BRAND_VOICE_POSTURE.md` file exists** — referenced by lint but not yet written. De-facto posture lives across PASS_1 PS- registry, PASS_4 T-01 tension, and the brainstorm catalogue kill-list.

### Passes (positive criteria)
- **WHAT-IS-THAT visceral-wow gate (PS-03):** layperson at ~1m must produce involuntary "what is that?" curiosity reflex on first sight, NOT analytical "neat lighting".
- Liquid centre-origin chiaroscuro on diffused glass — looks like a *liquid*, not a *graph*.
- Bounded palette-segment hue around `baseHue` (≤180° arc, anchored, not free-running).
- Continuum-class motion (heat-eq, ≥80-cell spring lattices reading as continuum) — allowed.
- Per-strip mirrored / centre-emitting topologies; LGP-fused soft gradients.

### Fails (banned, lint-enforced where possible)
- Rainbow cycling / full hue-wheel sweeps / `fill_rainbow` / hue++ / free-running CHSV(hue,...).
- "Looks like a graph / DSP test fixture / oscilloscope" — history-buffer-as-spatial-axis "scope" modes are KILLED. **Centre-origin radial variant LIN-06 is the SOLE exempt scope-class effect.**
- Multi-element fragmentation: PendulumChain, BoidSwarm, KuramotoOscillators, OscillatorChain — N≥8 discrete agents fragment the LGP, never unify.
- Per-bin tempo-bank rendering (`tempi[i]` indexed access in render path) — banned per lint check 9.
- GEO-06 CircularRing / GEO-10 AsymmetricDriftOrigin / `DriftOrigin` — break centre-origin invariant.
- "Smart lighting" / "AI-powered" / "instrument-grade" framings.

### Brand-voice fits for the 3 effects
- **5.4 LIN-06 RadialTimeScope: PASSES** — explicitly named "K1-native exemplar #1", sole exempt scope-class. Caveats: (a) substrate must be PSRAMScalarRing (320 B), not raw bins[] history; (b) display scalar must be EMOTIONAL (RMS/onset envelope) not ANALYTICAL (raw spectrum bin).
- **5.6 LIN-08 PitchVelocityField: AT RISK** — top-3-of-12 rendered as 3 *moving* foci risks "3 dots / 3 elements" (fragmentation lite). Must read as continuum drift, NOT discrete agents.
- **5.7 LIN-09 BeatParitySprite: AT RISK** — must read as "the bloom inhales harder on every other beat", NOT "a dot blinks on the beat" (metronome failure mode).

---

## 8. Synergy-topology core findings (PASS_1-4 + Topology_Reconciliation)

### Reconciled 8-phase kill order (Topology_Reconciliation.md §5)
V1.0 ships in Phase 4 (~1,560 LOC). Phase 5 adds the K1-native exemplars (LIN-06/08/09/10) and PSRAM substrate. Total V1.2+ trajectory ~3,720 LOC.

### Phase 5 effect prescriptions
- **Move 5.4 LIN-06 RadialTimeScope (~60 LOC):** uses INF-12 PSRAMScalarRing + AUD-04 onset-flux history. "K1-native exemplar #1; brand-defensibility headline."
- **Move 5.6 LIN-08 AttackOnlyPitchClassVelocityField (~80 LOC):** uses AUD-02 chroma + 12 followers. **"Render top-3 classes only"** — explicit prescription against 12-class fragmentation. Zero audio extension; ~0.5 ms render.
- **Move 5.7 LIN-09 BeatParityBloomSpriteInjection (~50 LOC):** PRIMARY uses INF-04 TempoPhaseContinuous; FALLBACK uses AUD-05 BeatTrack flag. Original spec to "modify SbK1Bloom" — Captain rejected this in hardware test (additive overlay on bright hero saturates). Standalone effect on black background is the correct architecture.

### Codex parallel-run divergences (4 ordering decisions Codex won)
1. PersistenceHelpers Phase 1 (not Phase 4).
2. ProductFilter as Phase 0 graph node, not meta-pillar.
3. Dual-strip moat as dedicated Phase 3.
4. Hub #3 = dual-strip, not tempo-phase.

### Adversarial mirages (PASS_4)
- **T-01 LGP fringe-coherence ABSOLUTE→STRONG:** "physical interference on LGP" demoted until empirical measurement. M1 measurement campaign required pre-V1.0.
- **T-04 PSRAM cache-latency:** profile under WiFi-AP + AudioActor contention before committing the multi-second history variant of LIN-06.
- **A-04 ESV11 32 kHz beat tracking:** unverified reliability. M2 adversarial test set (5 signal classes) required pre-Phase-2. Dry run shows 2/5 PASS (ambient, sine), 3 rule-caveat FAIL (rubato, syncopation, sub-bass) — none are firmware regressions, all need threshold ratification.

### Capability pillars (PASS_2)
8 platform hubs, 6 capability pillars, 5 bridges, 6 hidden synergy layers. Every absolute moat routes through {LGP, Dual-Strip, Centre-Origin}. Pillar D = K1-native exemplars (geometry-from-time effects exploiting centre-pair topology — the home of LIN-06/08/09/10).

---

## 9. PSRAM policy (per IEffect.h:39-50 + ieffect/CLAUDE.md)

**Any effect buffer > 64 bytes MUST be allocated from PSRAM via `heap_caps_malloc(MALLOC_CAP_SPIRAM)`.**

Pattern: `struct PsramData { ... }; PsramData* m_state = nullptr;`. Allocate in `init()` with placement-new; free in `cleanup()` with explicit destructor + `heap_caps_free`. Render guards `if (m_state == nullptr) return;`. Native build fallback via `#ifdef NATIVE_BUILD` to `std::malloc`/`std::free`.

**Canonical exemplar:** `RadialTimeScopeEffect.cpp:49-79`.

**Rationale:** internal DRAM is reserved for WiFi/lwIP/FreeRTOS/DMA. Large class members starve the system heap. Below 64 B threshold: instance value member is fine.

`docs/MEMORY_ALLOCATION.md` is referenced by IEffect.h:49 + ieffect/CLAUDE.md but **does not exist on disk** — the IEffect.h header block is the de-facto canonical policy.

---

## 10. Mabutrace conventions

**Status:** ZERO existing AR effects use TRACE_* macros. Canonical pattern is gated `Serial.printf` behind a debug flag (`g_bloomDebugEnabled` etc.). The TRACE_* infrastructure exists for system-level (AudioActor / RendererActor / FastLED show / RMT) profiling.

**Captain's directive on Phase 5 effects:** mabutrace coverage is mandatory for registration. Establishes a NEW convention for the Phase 5 K1-native effects.

### Conventions (from MABUTRACE_GUIDE.md + Trace.h + Captain demand)
- `TRACE_SCOPE("<prefix>_render")` at function entry (3-letter effect prefix: rts, pvf, bps).
- `TRACE_COUNTER("<prefix>_<metric>", value_int)` every frame for key state. Floats [0,1] × 1000 cast to int. Bitmasks packed into single counter.
- `TRACE_INSTANT("<prefix>_<event>")` on STATE TRANSITIONS only — never per-frame markers. Verbs/transitions: `*_silence_gate`, `*_beat_tick`, `*_dominant_switch`, `*_sprite_spawn`, `*_spawn_blocked`.
- NO heap allocation; NO `String`/`std::to_string` — pass int directly.
- TRACE_BEGIN/TRACE_END NOT used inside effects (file-local handle restriction per Trace.h:50-53).

### Capture pipeline
Build: `pio run -e esp32dev_audio_esv11_k1v2_32khz_trace` (the `_trace` variant). Flash. Serial monitor → type `trace` → copy JSON between markers → paste into ui.perfetto.dev. 64 KB ring covers ~2.5 s.

---

## 11. Brainstorm catalogue (SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26)

### 6 convergent themes
1. **Layered composition** (LayerStack + per-layer blend) — INF-01.
2. **Universal trail mechanism** (framebuffer LPF) — INF-02.
3. **Tempo-phase as primary motion driver** — INF-04.
4. **Persistent character at low cost** (fx_dots[12], hue ring) — LIN-* family.
5. **PersistenceHelpers as shared substrate** — INF-08.
6. **PSRAM frame ring + scalar ring** — INF-03 + INF-12.

### 6-piece engine roadmap
LayerStack → FramebufferLPF → tempo-phase → fx_dots → PersistenceHelpers → PSRAM rings.

### Top-12 priority shortlist (filtered through brand voice)
LIN-06 RadialTimeScope, LIN-08 PitchVelocity, LIN-09 BeatParityBloom, LIN-10 MotionBlurChromagram, LIN-04 RhythmLockedCubicPerlin, LIN-07 PrismHueDrift, INF-02 FramebufferLPF, INF-04 TempoPhaseContinuous, COM-08 ColorEngine cross-blend, GEO-13 InterStripPhaseDelay, PER-14 OpticalFlow, F4 Cross-Strip Wave Interference.

### Hub elevation
Codex pulled DualStrip from #3 to #1, above LayerStack/LPF/TempoPhase. Single most under-exploited K1 affordance.

---

## 12. Pathmode corpus (operational implementation orders)

`firmware-v3/research/pathmode/` is NOT separate research — it's the operational artefacts derived from `k1-spec-recommendations-2026-04.md` + Captain decisions ratified 2026-04-26. Treat as Captain-facing implementation orders.

### Captain decisions resolved 2026-04-26
1. Parallel RMT enabled (replaces serialised single-channel show).
2. BeatTracker locked at commit `fab1802d` behind regression gate.
3. PipelineCore audio backend deprecated — ESV11_32KHZ is sole canonical.
4. Paired-CI Inversion Bypass gate: `@spatial-mapping: inverted` comment AND `[INVERTED]` registry prefix both required.
5. silentScale framework multiplier — applied post-render uniformly. Effects opt out via `SilenceBehaviour::IntentionallyPersistent`.

### 7 senior-engineer implementation prompts (slots 03-07, 09-10)
Audio-to-photon latency programme; SilenceBehaviour framework enforcement; Audio backend consolidation (PipelineCore retirement); Render contract enforcement (per-effect budget + lint/CI + FrequencyMap); BeatTracker correctness lock; Reliability core (brownout + NVS + OTA + heap monitor); Manufacturing + OTA validation.

---

## 13. Reference-effect anatomy (canonical patterns from 6 production AR effects)

| Effect | Audio coupling | Smoothing chain | Trail | Palette | Speed |
|---|---|---|---|---|---|
| **BeatPulseBloom** | rms + onset.transient.level01 + beatStrength + chordConf + rootNote | HybridTempoTransient trigger; BeatPulseHTML; BeatPulseTransportCore advection | persistencePerFrame60 = lerp(0.995, 0.90, fade01) | `palette.getColor(paletteShift + ctx.gHue)` | `lerp(0.70, 1.50, speed/100)` |
| **AudioBloom** | hopSequence + bands[0] + chordState + chroma + motionFluidity | dtDecay sub-bass; selectChroma12; hop-gated even-frame | fadeToBlackBy(25) + saturation +24 + fadeTopHalf | NOTE_OFFSETS[12] musical anchors; chord warmth ±32; rootHueShift = root*21*conf*0.5 | `(0.3 + speed/50*2.2) * fluidityMod` LEDs/hop |
| **BPMEnhanced** | heavyBands(1)/(2) + beatStrength + chord + chroma + isOnBeat + isSnareHit + beatPhase | AsymmetricFollower::updateWithMood × 16; Spring(50,1.0) for speed; circularChromaHueSmoothed; PLL P-only beatPhase tau=0.1 | fadeToBlackBy(ctx.fadeAmount) variable | `palette.getColor(gHue + chromaHueOffset + dist/3)`; complementary +128 strip2 | `clamp(0.6 + 0.8*heavyEnergy, 0.3, 1.6) * speed/50` |
| **SbK1Bloom** | sb_chromagram_smooth (base) + rms + saturation | base chroma smoothing; centre-blend tau=30ms; sub-pixel scroll | sprite scroll outward (no fadeToBlackBy); linear edge fade | cyan-offset (+0.5); huePosition only chromaticMode; force_saturation HSV roundtrip | NOT used (mood param instead) |
| **SbK1Waveform** | chromaSmooth + wfPeak follower + audioConfidence + silentScale | base chroma + peak EMA tau=23ms; trail expf(-decayRate*dt) | float trail buffer fade; adaptive decay rate | additive palette accumulation per chroma bin; soft-knee normalise | `150 * speed/10` LEDs/s |
| **BeatPulseSpectral** | bass()/mid()/treble() | per-band exp smoothing rate=`1-pow(0.85/0.88/0.92, dt*60)`; m_beatBoost decays `pow(0.90, dt*60)` | NO fadeToBlackBy (recompute every frame) | BASS_PALETTE=40, MID=128, TREBLE=200 bounded | NOT used (fallbackPhase only) |

### Common audio-coupling pattern
Read `ctx.audio.controlBus` directly OR helper accessors → hop-gate target updates (`if hopSequence != m_lastHopSeq`) → smooth via `enhancement::AsymmetricFollower::updateWithMood(target, rawDt, moodNorm)` → render via centre-origin loop with `ctx.palette.getColor()` and `+=`/qadd8.

### `beatStrength()` is the default brightness modulation
Per Effect Standard §4.4: `beatMod = 0.4f + 0.6f * ctx.audio.beatStrength()`. Already EsBeatClock-smoothed; do NOT re-smooth. **Prefer over `isOnBeat()` for any visible parameter** — `isOnBeat()` is single-frame at 8.33 ms = invisible flash, only valid for spawning particles.

---

## 14. Audio-visual semantic mapping principles (from `audio-visual-semantic-mapping.md`)

### The Binding Trap
Rigid `bass→expansion / treble→shimmer / chord→hue / snare→burst` is FORBIDDEN. Same input ≠ same output. Audio is DRIVER, not modifier.

### Five principles
1. Saliency analysis — what's *interesting* in the audio right now.
2. Style-adaptive response — rhythm-driven music gets pulse, harmony-driven gets drift.
3. Behaviour selection (not value mapping) — `if saliency.harmonic > saliency.rhythmic { HARMONIC_DRIFT } else { RHYTHMIC_PULSE }`.
4. Temporal context awareness — what happened in the last 5 seconds shapes now.
5. Non-deterministic variation — same input twice should not look identical.

### GEMS framework (emotional reliability)
Vitality / Joyful Activation / Tension / Sadness reliably mapped from RMS / tempo / onset / centroid. Sublimity needs heuristics. **15-second warm-up rule** — emotion signals unreliable for first ~15s.

### Music-type → strategy
EDM/Hip-hop = rhythm pulse; Jazz/Classical = harmony drift; Vocal Pop = melody shimmer; Ambient = texture flow; Orchestral = dynamics build/release.

---

## 15. The three Move 5.x effects — final design verdict

### 5.4 RadialTimeScope (LIN-06)
**Brand voice: PASSES.** Substrate is right (PSRAMScalarRing<float, 80>). Implementation needs surgical fixes per Effect Standard:
- Source: `max(fast_rms, onsetFlux*4)` not `onsetEnv`
- Wrap source in AsymmetricFollower{0, 0.05, 0.30}
- Replace CHSV with ctx.palette.getColor + nscale8_video
- Multiplicative silence (audioConf × silentScale per pixel) not early-return
- Drop double-fade (ring IS persistence)
- Add Bed layer (rms-driven floor)
- Hop-gate target updates

### 5.6 PitchVelocityField (LIN-08)
**Brand voice: AT RISK** if rendered as 3 discrete agents. Must read as continuum drift. Major rewrite:
- Add fadeToBlackBy(30) at top, switch to `+=` blend
- Replace argmax + custom EMA with AsymmetricFollower[12] + circularChromaHueSmoothed
- Keep top-3 follower-driven spatial frequency selection (effect's signature) BUT decouple from hue
- SubpixelRenderer for radial wave (eliminates wagon-wheel)
- ctx.palette.getColor with bounded dispersion shifts ≤ ±32
- Speed knob via ctx.speed/50 on phase drift
- Bed layer; hop-gate

### 5.7 BeatParitySprite (LIN-09)
**Brand voice: AT RISK** if reads as metronome dot. Must read as bloom-breath-on-odd-beats.
- **Spawn gate redesign:** drop impossible `tempoBeatTick && phase01≈0.65` conjunction. Use `bus.kickTrigger` PRIMARY (band-ratio detector, no tempo-lock dependency, fires reliably even on chord-only music). `tempoBeatTick && tempoConfidence ≥ 0.30` SECONDARY for downbeat accents.
- Latch `m_lastBeatInBar` on tick frames separately for parity tracking. Read parity at injection time.
- **PSRAM compliance:** move `Sprite m_sprites[8]` (128 B) into `PsramData` struct. Match RadialTimeScope's pattern verbatim.
- Round-robin spawn (overwrite oldest), not drop-on-full.
- Continuously running `circularChromaHueSmoothed`; sample `m_chromaAngle` AT spawn for stable hue (not argmax).
- ctx.palette.getColor + SubpixelRenderer for sprite radius.
- Bed layer (rms-driven background glow so silence ≠ black).
- Lower kTempoLockGate 0.4 → 0.30 per m2_adversarial doctrine.

---

## 16. Execution gates (commission-before-V1.0)

Two empirical measurements gate V1.0 marketing/launch and have not been commissioned:

1. **M1 LGP fringe-coherence:** `firmware-v3/docs/measurement_protocols/m1_lgp_fringe.md`. 8-12 viewer photometer session, ≥75%/≥4-of-5 offsets identification rate. Gates Phase 3 F4 launch + "physical interference" marketing copy.
2. **M2 ESV11 adversarial test set:** `firmware-v3/tools/m2_adversarial/`. Dry run 2/5 PASS, 3 rule-caveat FAIL. Captain ratification needed on rubato re-acquire criterion / syncopation phase coherence formula / sub-bass confidence-gating. Gates Phase 2 doctrine + LIN-09 ship.

---

## 17. Open questions

1. **`docs/MEMORY_ALLOCATION.md`** referenced by IEffect.h:49 + ieffect/CLAUDE.md but doesn't exist. Reinstate as canonical or repoint references at IEffect.h header block.
2. **`BRAND_VOICE_POSTURE.md`** referenced by lint engine §3.3/§3.5/§3.6/§4.3/§4.5 but doesn't exist. Captain may want to commission as single source of truth split out of SB_ES catalogue + PASS_1 PS-registry.
3. **Move-numbering ambiguity:** PASS_3 uses 5.1/5.4/5.5 for LIN-06/08/09; Topology_Reconciliation uses 5.4/5.6/5.7. Reconciled is canonical going forward.
4. **INF-04 surfacing:** PASS_1:60 flagged status conflict — verify `bus.es_phase01_at_audio_t` is actually populated on the canonical ESV11_32KHZ build. Affects LIN-09 primary path.
5. **TRACE_* in production AR effects:** Captain's directive establishes new convention for Phase 5 — should be back-applied to existing AR effects? Or kept as Phase-5-specific?

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-27 | claude-opus-4-7 | Created. Synthesises 20 parallel SSA returns covering effect dev standard, 5-layer audio model, ControlBus field reality, tempo/phase alignment proof, smoothing infrastructure with concrete tau values, persistence patterns, brand voice, synergy-topology phase moves, PSRAM policy, mabutrace conventions, brainstorm catalogue, pathmode operational orders, and 6 reference-effect anatomies. Direct distillation requested by Captain after the redesign-handoff doc was written. |
