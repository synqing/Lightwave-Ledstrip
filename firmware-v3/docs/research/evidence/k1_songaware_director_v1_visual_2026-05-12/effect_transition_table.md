# Effect Transition Table

RBDO label: GROUNDED

| Timestamp ms | From | To | Song state | Confidence | Family | Visual language | Reason | Allowlisted | Notes |
|---:|---|---|---|---:|---|---|---|---|---|
| 208446 | `0x1302 K1 Waveform` | `0x0202 LGP Modal Resonance` | `ambient` | 1.000 | `interference` | `calm_modal_resonance` | `ambient_posture` | yes | First automatic Director switch. |
| 239706 | `0x0202 LGP Modal Resonance` | `0x0407 LGP Photonic Crystal` | `drop` | 1.000 | `advanced_optical` | `photonic_drop_texture` | `drop_impact` | yes | Second automatic Director switch. |

## Policy Safety

- Automatic switch count reached `2`.
- The Director V1 policy limit is no more than `2` automatic switches per rolling minute.
- No effect roulette or rapid back-and-forth thrash was observed in serial logs.
- No unallowlisted effect was selected.

## Visual Limitation

This table proves runtime transitions and reasons only. It does not prove visual fit because no optical capture was available.
