# Director V1 Effect Allowlist

RBDO label: GROUNDED

## Allowlist

| Song state | Effect ID | Effect name | Family label | Visual language | Reason |
|---|---:|---|---|---|---|
| silence | `0x0202` | LGP Modal Resonance | `interference` | `modal_low_density_hold` | Quiet standing optical posture from the earlier cycle set. Switching effectively blocked by confidence/silence gates. |
| ambient | `0x0202` | LGP Modal Resonance | `interference` | `calm_modal_resonance` | Calm modal/standing optical language. |
| steady | `0x0201` | LGP Holographic | `interference` | `flagship_holographic_depth` | Baseline premium optical depth from the earlier cycle set. |
| build | `0x0204` | LGP Wave Collision | `interference` | `colliding_wave_pressure` | Rising wave-collision pressure from the earlier cycle set. |
| drop | `0x0407` | LGP Photonic Crystal | `advanced_optical` | `photonic_drop_texture` | Decisive advanced-optical texture from the earlier cycle set. |
| breakdown | `0x0900` | LGP Chromatic Lens | `advanced_optical` | `chromatic_space_release` | Reduced/space-making chromatic release language. |
| dense | `0x1B01` | KdV Soliton Pair | `mathematical` | `dense_soliton_pair` | High-fill mathematical soliton language from the earlier cycle set. |
| transition | `0x0901` | LGP Chromatic Pulse | `advanced_optical` | `chromatic_transition_pulse` | Chromatic bridging pulse from the earlier cycle set. |

## Source Basis

- Policy table in code: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:36`.
- Effect IDs exist in `firmware-v3/src/config/effect_ids.h`.
- Effects are registered in `firmware-v3/src/effects/CoreEffects.cpp`.
- Pattern metadata marks the selected families as centre-origin or K1/Sensory Bridge appropriate in `firmware-v3/src/effects/PatternRegistry.cpp`.
- Candidate source set comes from the earlier Phase 0/1 cycle evidence in `firmware-v3/docs/research/k1_medium_phase0_candidate_audit_2026-05-12.md`.

## Exclusions

Excluded categories:

- Experimental/quarantined or known performance-risk effects.
- Full hue-wheel/rainbow style effects.
- Unbounded random/roulette-like effects.
- Effects without a clear role in the state policy.

## Runtime Proof

The corrected controlled audio smoke selected only allowlisted target `0x0407 LGP Photonic Crystal` from initial `0x1302 K1 Waveform`.
