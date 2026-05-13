# C-5 Phase 5 Timestamped Observables

**Date:** 2026-05-06
**RBDO label:** GROUNDED for the C-2 feature-to-effect mapping; DEGRADED-MODE for clip suitability and visual pass status until Captain runs these windows on hardware.
**Scope:** Close `BACKLOG.md` C-5 by binding the Phase 5 exemplar effects to redacted private clip labels, timestamp windows, expected visual phenomena, trace evidence, and Captain-facing pass questions.

## Evidence Boundary

This is not a ship-quality visual sign-off. It is the operator matrix that makes the next hardware run decidable.

| Item | Evidence |
|---|---|
| Phase 5 cohort | The three covered effects are `0x2100` Radial Time Scope, `0x2101` Attack-Only Pitch Velocity Field, and `0x2102` Beat Parity Sprite (`firmware-v3/docs/audit/phase_5_visual_sign_off_2026-04-28.md:23-29`). |
| Feature/dwell source | C-2 maps each effect to the audio feature rows, fixture archetypes, and minimum dwell lower bounds (`firmware-v3/docs/research/c2_feature_effect_dwell_matrix_2026-05-06.md`). |
| Mic-domain regime source | C-1 measured current K1v2 ESV11 32 kHz firmware-domain idle, quiet, normal, dense, and stop-recovery envelopes (`firmware-v3/docs/research/c1_mic_domain_envelope_capture_2026-05-06.md:20-47`). |
| Privacy boundary | Captain resolved C-3: the repo is public and clips stay private (`BACKLOG.md` C-3). This document therefore uses redacted private corpus labels only. |
| Candidate selection | Private labels and windows were selected from a non-playback local metric scan of Captain-authorised local material. The private mapping lives outside the public repo. |

## DEGRADED-MODE Label

- **Unresolved assumption:** The selected private windows are musically adequate and visually discriminating on the physical K1 LGP.
- **Risk if wrong:** A row may fail to exercise the intended phenomenon, making a visual pass ambiguous rather than proving effect quality.
- **Fallback:** Replace the private label/window for that row and keep the effect under diagnostic-baseline status until a discriminating window is found.
- **Revisit trigger:** First hardware run of this matrix, a change to the private corpus, a changed audio backend/sample-rate profile, or a changed Phase 5 effect implementation.
- **Debt count / affected outputs:** This artefact feeds the next Phase 5 sign-off cycle and any harness derived from it. It does not by itself promote the effects to ship-gate status.

## Private Corpus Labels

Do not expand these labels into public file paths, dataset IDs, or track names in repo artefacts.

| Label | Archetype | Window | Primary use |
|---|---|---:|---|
| `PC-QS-01` | quiet sparse | 60s-90s | Silence-boundary and sparse-energy behaviour. |
| `PC-NF-01` | normal full-spectrum | 89s-119s | Baseline working-band behaviour. |
| `PC-DB-01` | dense bright/loud | 16s-46s | Saturation, haze, and high-density stress. |
| `PC-ST-01` | sustained tonal | 63s-93s | PVF standing-wave and BPS colour stability. |
| `PC-HC-01` | harmonic changes | 31s-61s | PVF/BPS chroma morphing. |
| `PC-KT-01` | regular kick train | 40s-70s | BPS kick-spawn and RTS onset-spike coverage. |
| `PC-SY-01` | syncopated/percussion-light | 4s-34s | BPS no-tempo-lock fallback. |
| `PC-HF-01` | HF bright stress | 26s-56s | Guard row only; do not use for Phase 5 primary claims. |
| `PC-BH-01` | bass-heavy | 90s-120s | Low-band guard row. |
| `SIL-IDLE` | no playback | 0s-10s | False-event and stale-state clear. |
| `STOP-20S` | hard stop | 0s-20s playback, 20s-30s post-stop | Stop recovery and stale-spawn check. |

## `0x2100` Radial Time Scope

| Row | Label/window | Feature rows | Trace evidence | Expected visual | Captain question |
|---|---|---|---|---|---|
| RTS-1 | `SIL-IDLE` 0s-10s | A0, A1 | `rts_audio_conf`, `rts_silent_scale`, `rts_ring_count` | Near-dark or dim bed with no stale off-centre activity. | Does the scope stay quiet without random flashes or old history? |
| RTS-2 | `PC-QS-01` 60s-90s | A0, A1, A2 | `rts_fast_rms`, `rts_onset_flux`, `rts_onset_env_pushed` | Freshest activity sits at LEDs 79/80 and ages outward. | Is "now" visibly locked to the centre pair? |
| RTS-3 | `PC-NF-01` 89s-119s | A1, A2, A3 | `rts_pushes`, `rts_ring_count`, `rts_fast_rms` | Full-spectrum activity reads as a radial time axis, not random shimmer. | Can you read centre-newer / edge-older motion without explanation? |
| RTS-4 | `PC-DB-01` 16s-46s | A1, A2 | `rts_fast_rms`, `rts_onset_flux`, `rts_audio_conf` | Onset spikes add punch without white haze or saturated smear. | Does dense material stay colourful and structured? |
| RTS-5 | `PC-KT-01` 40s-70s | A2, A5 guard | `rts_onset_flux`, `rts_onset_env_pushed` | Kick/onset teeth appear as centre-origin emphasis only. | Are the spikes centre-origin rather than off-centre flashes? |
| RTS-6 | `STOP-20S` 0s-30s | A0 | `rts_silent_scale`, `rts_audio_conf`, `rts_ring_count` | After the 20s stop, no new activity appears; old history fades. | Does stop recovery feel clean within the C-1 envelope? |

## `0x2101` Attack-Only Pitch Velocity Field

| Row | Label/window | Feature rows | Trace evidence | Expected visual | Captain question |
|---|---|---|---|---|---|
| PVF-1 | `SIL-IDLE` 0s-10s | A0 | `pvf_audio_conf`, `pvf_silent_scale`, `pvf_field_zero` | Field collapses cleanly with no bright stale chroma latch. | Does silence clear the field rather than freezing a pattern? |
| PVF-2 | `PC-ST-01` 63s-93s | A4 | `pvf_field_max`, `pvf_field_min`, `pvf_topk_count` | Sustained radial standing-wave continuum. | Does it read as a continuous field rather than three dots? |
| PVF-3 | `PC-HC-01` 31s-61s | A4, A3 | `pvf_topk_count`, `pvf_field_max` | Harmonic movement morphs smoothly across the LGP. | Do chord changes glide instead of snapping or smearing? |
| PVF-4 | `PC-QS-01` 60s-90s | A0, A1, A4 | `pvf_audio_conf`, `pvf_silent_scale`, `pvf_field_max` | Low bed, visible harmonic structure, no percussion expectation. | Is quiet harmony still visible without looking falsely beat-driven? |
| PVF-5 | `PC-DB-01` 16s-46s | A4, A1 | `pvf_field_max`, `pvf_field_min`, `pvf_topk_count` | Dense material stays chromatic and avoids rainbow drift. | Does it remain saturated and musical, not pastel or noisy? |
| PVF-6 | `PC-BH-01` 90s-120s | A1 guard, A4 | `pvf_audio_conf`, `pvf_field_max` | Bass-heavy material does not overpower chroma. | Does low-band force flatten the pitch field? |

## `0x2102` Beat Parity Sprite

| Row | Label/window | Feature rows | Trace evidence | Expected visual | Captain question |
|---|---|---|---|---|---|
| BPS-1 | `SIL-IDLE` 0s-10s | A0 | `bps_silent_scale`, `bps_active_sprites`, `bps_silence_gate` | No sprite spawns and no stale centre emissions. | Does silence produce zero new sprites? |
| BPS-2 | `PC-KT-01` 40s-70s | A5, A0 | `bps_kick_trigger`, `bps_kick_fired`, `bps_sprite_spawn` | Kick events spawn centre-origin sprites consistently. | Do visible kicks create sprites without missed or extra pulses? |
| BPS-3 | `PC-SY-01` 4s-34s | A5, A6 | `bps_kick_trigger`, `bps_tempo_conf`, `bps_sprite_spawn` | Sparse/syncopated kicks still spawn without tempo lock. | Does it avoid becoming a metronome when timing is irregular? |
| BPS-4 | `PC-HC-01` 31s-61s | A4 | `bps_active_sprites`, `bps_sprite_spawn` | Sprite colour follows harmonic context without hue-wheel cycling. | Do sprite colours feel stable and intentional? |
| BPS-5 | `PC-NF-01` 89s-119s | A1, A5, A6 | `bps_audio_conf`, `bps_beat_in_bar`, `bps_active_sprites` | Baseline bloom/breath with parity accents, not dot-strobe. | Does it read as bloom-breath rather than mechanical flashing? |
| BPS-6 | `STOP-20S` 0s-30s | A0 | `bps_silent_scale`, `bps_silence_gate`, `bps_active_sprites` | Spawning stops quickly; existing sprites fade without new centre pulses. | Does stop recovery prevent new sprites after the hard stop? |

## Hardware Run Record

Use one record per row during the next hardware run:

```text
effect_id:
row_id:
private_label:
window:
trace_file:
observed_visual:
trace_counter_summary:
captain_answer:
result: PASS | FAIL | DEGRADED-PASS
notes:
```

Minimum pass conditions:

| Effect | Must be true |
|---|---|
| RTS | Centre-origin time axis remains legible, no off-centre flashes, no dense-material haze regression. |
| PVF | Field reads as a continuum, no rainbow drift, no stale bright latch after silence. |
| BPS | Kick spawns are event-driven, not tempo-only; sprites originate from centre; no post-stop spawns. |

## Decision

C-5 is **DONE-DEGRADED**:

- The per-effect `(private label, timestamp window, observable)` matrix now exists.
- The matrix is public-safe: it does not commit audio, absolute paths, dataset IDs, or track names.
- It is sufficient to build and run the next Phase 5 hardware sign-off harness.
- It does not prove clip adequacy, visual quality, or ship-gate readiness until Captain runs the matrix on K1 hardware.
