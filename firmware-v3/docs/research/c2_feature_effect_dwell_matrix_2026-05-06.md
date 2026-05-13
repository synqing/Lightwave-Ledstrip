# C-2 Feature x Effect x Dwell Matrix

**Date:** 2026-05-06
**RBDO label:** GROUNDED for source mappings; DEGRADED-MODE for dwell minima until C-5 timestamped observables and a calibrated hardware sign-off run exist.
**Scope:** Close `BACKLOG.md` C-2 by mapping the Phase 5 exemplar effects to the audio features they actually consume, the visible phenomena they need to show, and the minimum dwell each phenomenon needs before Captain visual sign-off can be meaningful.

## Evidence Boundary

This is not a ship-quality visual sign-off and not C-5. It answers "what must be covered and for how long" so C-5 can later pin those rows to exact private clip timestamps.

| Item | Evidence |
|---|---|
| Phase 5 cohort | `0x2100` / `0x2101` / `0x2102` are the three Phase 5 exemplars covered by the degraded attestation (`firmware-v3/docs/audit/phase_5_visual_sign_off_2026-04-28.md:23-29`). |
| AFS v2 field policy | Effects should consume named helpers and semantic controls, not raw spectrum scans (`firmware-v3/docs/audio-visual/AUDIO_FEATURE_SURFACE_V2_CONTRACT.md:18-21`, `55-57`, `219-243`). |
| Feature reality | `EffectContext::AudioContext` exposes the relevant accessors: `fastRms()`, `rms()`, `audioConfidence()`, `tempoBeatTick()`, `tempoBeatConfidence()`, `beatInBar()`, `hopSequence()`, `getChroma()`, `chroma()`, `silentScale()`, `onsetFlux()`, and `isKickHit()` (`firmware-v3/src/plugins/api/EffectContext.h:94-100`, `153-172`, `273-292`, `300-310`, `364-367`). |
| Current microphone-domain envelope | C-1 measured the current K1v2 ESV11 32 kHz firmware-domain regimes and stop recovery (`firmware-v3/docs/research/c1_mic_domain_envelope_capture_2026-05-06.md:20-47`). |
| Dwell caveat | C-5 still needs `(clip, timestamp, observable)` anchors per effect; this matrix only supplies the source-derived minimum coverage rows. |

## DEGRADED-MODE Label For Dwell Minima

- **Unresolved assumption:** The dwell durations below are source-derived lower bounds, not empirically proven perceptual thresholds.
- **Risk if wrong:** A phenomenon could be judged before it stabilises visually, or a failure could be missed because the fixture did not hold the right musical condition long enough.
- **Fallback:** If a C-5 timestamped run contradicts a dwell minimum, C-5 overrides this document and this matrix is revised.
- **Revisit trigger:** First C-5 timestamped observable pass, new Phase 5 effect, new audio backend/sample-rate profile, changed silence policy, or changed Phase 5 render source fields.
- **Debt count / affected outputs:** This C-2 artefact feeds C-5 and any calibrated Phase 5 sign-off harness. It must not be used as final visual evidence by itself.

## Feature Classes

| Feature class | Firmware-facing fields | Source status | Why it matters |
|---|---|---|---|
| A0 audio availability and silence | `audioConfidence`, `silentScale`, `isSilent` | `audioConfidence` is a 200-500 ms music-present envelope and `silentScale` is the brightness multiplier for silence (`ControlBus.h:237-247`). | Separates true effect behaviour from global output dimming. |
| A1 continuous activity | `fastRms`, `rms` | `fastRms()` and `rms()` are effect-facing accessors (`EffectContext.h:94-100`). RTS deliberately uses `fastRms`, while BPS/PVF use RMS-derived beds. | Drives soft beds, energy floors, and RTS's "now" sample. |
| A2 onset novelty | `onsetFlux`, `onsetEvent`, band flux helpers | `onsetFlux()` is explicitly advanced/debug scale-sensitive (`EffectContext.h:309-315`). | RTS overlays onset-flux spikes on `fastRms`; this must be tested with both sparse and dense transients. |
| A3 hop cadence | `hopSequence` | Effects can gate target capture to fresh audio hops (`EffectContext.h:273-274`). | Prevents render frames from re-sampling stale audio as if it were new. |
| A4 harmonic/chroma | `chroma[12]`, `getChroma(i)`, `chroma()` | `ControlBusFrame` carries `chroma[12]`; accessors expose single bins and pointer views (`ControlBus.h:135-138`, `EffectContext.h:276-287`). | PVF's primary structure and BPS sprite colour come from chroma. |
| A5 impact events | `isKickHit`, `kickTrigger`, semantic onset channels | `isKickHit()` combines the first-class onset surface and `ControlBusFrame::kickTrigger` (`EffectContext.h:364-367`). | BPS spawns sprites from kick events. |
| A6 tempo grid | `tempoBeatTick`, `tempoBeatConfidence`, `beatInBar` | Tempo helpers combine ControlBus, ESV11, and musical-grid sources (`EffectContext.h:156-172`). | BPS uses tempo only for beat-in-bar/parity and accents, not as its primary spawn gate. |
| A7 HF semantics | `hfEnergy`, `hfFlux`, `hatEvent`, `cymbalSustain`, `airEnergy` | The AFS v2 contract defines these but warns not to treat HF energy as a hat trigger (`AUDIO_FEATURE_SURFACE_V2_CONTRACT.md:146-200`). | Not directly consumed by the three Phase 5 effects; keep in fixture stress only to avoid false HF claims. |

## Effect x Feature Coverage

| Effect | Primary feature coverage | Secondary / guard coverage | Negative coverage |
|---|---|---|---|
| `0x2100` Radial Time Scope | A1 `fastRms` plus A2 `onsetFlux * 4` source; A3 hop-gated target capture; A0 confidence/silence multiplier. | 60 Hz ring history; palette-bound hue drift; bed layer. | Does not use `onsetEnv` as a continuous drive; does not scan raw `bins64` / `bins256`. |
| `0x2101` Attack-Only Pitch Velocity Field | A4 12-bin chroma followers; top-3 class radial velocity field; A1 `fastRms` bed. | A3 hop-gated chroma target capture; A0 confidence/silence multiplier; speed-scaled phase drift. | Does not use tempo ticks or impact events; do not validate it with percussion-only fixtures. |
| `0x2102` Beat Parity Sprite | A5 kick events spawn sprites; A6 tempo tick/confidence/beat-in-bar modulate parity/accent; A4 chroma snapshots sprite hue. | A1 `rms` bed; A0 silence gate and `silentScale`; speed scales sprite expansion only. | Kick spawn must not require tempo lock; parity must not suppress all kicks. |

## Per-Effect Matrix

### `0x2100` Radial Time Scope

Source mapping:

- `render()` reads `fastRms`, `onsetFlux`, `audioConfidence`, `silentScale`, and `hopSequence`; the source is `max(fastRms, onsetFlux * 4)` (`RadialTimeScopeEffect.cpp:209-237`).
- It updates the follower every render frame, but only refreshes the target on a new hop (`RadialTimeScopeEffect.cpp:223-231`, `248-251`).
- It pushes the smoothed value into an 80-slot ring at speed-scaled cadence and maps ring offset to centre-pair distance (`RadialTimeScopeEffect.cpp:267-289`, `370-404`).
- Its header defines the visual contract: freshest sample at LEDs 79/80, edge LEDs about 1.33 s old at the 60 Hz baseline, mirrored strip 2, and dim silence bed (`RadialTimeScopeEffect.h:18-32`, `44-74`).

| Phenomenon to cover | Feature rows | Minimum dwell | Pass anchor for C-5 |
|---|---|---:|---|
| Idle/silence floor and clear | A0, A1 | 10 s before playback and 8 s after hard stop | Bed fades to dark or near-dark without stale off-centre activity. |
| Quiet sparse activity | A0, A1, A2 | 20 s | Centre remains the freshest point; history radiates outward without flashes. |
| Normal full-spectrum activity | A1, A2, A3 | 30 s | Time-axis is legible: "now" at centre, older energy outward, mirrored on both strips. |
| Dense bright/loud material | A1, A2 | 30 s | Onset spikes add punch without turning the scope into saturated haze. |
| Hard stop recovery | A0 | 8 s after exact stop | Visual dimming follows C-1 stop recovery; no new activity appears after stop. |

### `0x2101` Attack-Only Pitch Velocity Field

Source mapping:

- `render()` captures all 12 chroma targets only on fresh audio hops, then smooths followers every render frame (`AttackOnlyPitchVelocityFieldEffect.cpp:222-249`).
- It derives a circular-mean chroma hue, builds a top-3 radial velocity field, and renders centre-origin quartet writes (`AttackOnlyPitchVelocityFieldEffect.cpp:251-278`, `327-352`).
- The bed is driven by `fastRms`, while the field brightness is multiplied by `audioConfidence * silentScale` (`AttackOnlyPitchVelocityFieldEffect.cpp:291-307`, `324-335`).
- The header says the signature must read as a standing-wave continuum, not fragmented pitch dots (`AttackOnlyPitchVelocityFieldEffect.h:31-40`).

| Phenomenon to cover | Feature rows | Minimum dwell | Pass anchor for C-5 |
|---|---|---:|---|
| Sustained tonal centre | A4 | 20 s | A stable radial standing wave forms and does not flicker between chroma bins. |
| Chord change sequence | A4, A3 | 45 s or at least 4 clear harmonic changes | Field morphs continuously; top-3 classes read as interference, not three separate dots. |
| Sparse quiet harmony | A0, A1, A4 | 25 s | Bed stays low; harmonic structure remains visible without false percussion expectations. |
| Dense harmonic material | A4, A1 | 30 s | Top-3 selection avoids smear/noise; no rainbow or whole-palette sweep. |
| Silence / no tonal evidence | A0 | 10 s | Field collapses cleanly; no stale chroma latches as a bright pattern. |

### `0x2102` Beat Parity Sprite

Source mapping:

- `render()` reads `audioConfidence`, `silentScale`, `rms`, `isKickHit`, `tempoBeatTick`, `tempoBeatConfidence`, `beatInBar`, and `chroma()` (`BeatParitySpriteEffect.cpp:105-128`).
- It samples chroma every frame for sprite hue, writes an RMS bed, then gates spawn/update when confidence or `silentScale` is too low (`BeatParitySpriteEffect.cpp:147-185`).
- Kick events are the primary spawn trigger; tempo/parity only modulates accent and diagnostics (`BeatParitySpriteEffect.cpp:187-236`).
- Sprite lifetime is 0.6 s, with up to 8 concurrent sprites (`BeatParitySpriteEffect.h:76-85`).

| Phenomenon to cover | Feature rows | Minimum dwell | Pass anchor for C-5 |
|---|---|---:|---|
| Regular kick train | A5, A0 | 30 s or at least 24 kick opportunities | Every visible kick should spawn a centre-origin sprite; trace should keep `bps_kick_fired` and `bps_sprite_spawn` aligned. |
| Tempo-locked downbeat accents | A5, A6 | 45 s or at least 8 bars after tempo confidence stabilises | Accents can strengthen sprites, but lack of tempo lock must not stop kick sprites. |
| Syncopated/percussion-light music | A5, A6 | 45 s | Sparse kicks still spawn; tempo/parity diagnostics do not create metronome-only behaviour. |
| Harmonic colour stability | A4 | 30 s across active sprites | Sprite hue is stable for each sprite lifetime and follows chroma over time without hue-wheel cycling. |
| Hard stop recovery | A0 | 10 s after exact stop | Spawn/update stops quickly; existing sprites fade naturally without new centre emissions. |

## Fixture Archetype Matrix

Do not commit source media, private file paths, or third-party track references. C-5 can bind these archetypes to Captain-approved private clips later.

| Archetype | Minimum duration per effect | Required effects | Coverage reason |
|---|---:|---|---|
| `idle_room` | 10 s pre and 10 s post | all | Silence floor, stale-state clear, no false events. |
| `quiet_sparse` | 25 s | all | C-1 shows quiet material sits near the silence boundary; tests hysteresis without over-driving. |
| `normal_full_spectrum` | 30 s | all | Baseline working band for current K1v2 firmware-domain tuning. |
| `dense_bright_loud` | 30 s | all | Saturation/haze stress and high-event-density stress. |
| `sustained_tonal` | 20 s | PVF, BPS | Chroma continuity and sprite colour stability. |
| `harmonic_changes` | 45 s | PVF, BPS | Chroma morphing and top-3 continuum behaviour. |
| `regular_kick_train` | 30 s or 24 kicks | BPS, RTS | Event spawn and RTS onset-spike overlay. |
| `syncopated_or_percussion_light` | 45 s | BPS | Proves kick-trigger fallback does not depend on tempo lock. |
| `hard_stop` | 20 s playback plus 10 s post-stop | all | Aligns with C-1 stop-recovery evidence and future C-5 timestamping. |

## C-5 Handoff

Each C-5 row should be one timestamped observable:

```text
effectId:
archetype:
private clip label:
timestamp start/end:
feature rows covered:
expected visual:
trace counters:
Captain visual question:
PASS / FAIL / DEGRADED-PASS:
```

Minimum trace counters for the C-5 harness:

| Effect | Counters / instants |
|---|---|
| RTS | `rts_fast_rms`, `rts_onset_flux`, `rts_audio_conf`, `rts_silent_scale`, `rts_onset_env_pushed`, `rts_pushes`, `rts_ring_count` |
| PVF | `pvf_audio_conf`, `pvf_silent_scale`, `pvf_field_max`, `pvf_field_min`, `pvf_topk_count`, `pvf_field_zero` |
| BPS | `bps_audio_conf`, `bps_tempo_conf`, `bps_beat_in_bar`, `bps_kick_trigger`, `bps_silent_scale`, `bps_active_sprites`, `bps_kick_fired`, `bps_sprite_spawn`, `bps_silence_gate` |

## Decision

C-2 is **DONE-DEGRADED**:

- The source-to-effect feature matrix is grounded in current firmware source and existing docs.
- The dwell matrix is sufficient to unblock C-5 authoring and a calibrated sign-off harness plan.
- It does not prove visual quality, clip adequacy, or timestamped observability.
- C-5 remains the next calibration debt item before any Phase 5 ship-gate sign-off.
