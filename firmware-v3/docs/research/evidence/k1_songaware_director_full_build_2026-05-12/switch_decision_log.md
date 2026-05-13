RBDO label: GROUNDED

Automatic switch log:

| Time ms | From | To | State | Reason | Transition | Allowlisted |
|---:|---|---|---|---|---|---|
| 59488 | 0x1302 K1 Waveform | 0x0407 LGP Photonic Crystal | drop | drop_impact | Pulsewave 2000 ms | yes |
| 328252 | 0x0407 LGP Photonic Crystal | 0x1B01 LGP KdV Soliton Pair | dense | dense_legibility | Fade 800 ms | yes |
| 350270 | 0x1B01 LGP KdV Soliton Pair | 0x0204 LGP Wave Collision | build | build_pressure | Wipe Out 1200 ms | yes |
| 388640 | 0x0204 LGP Wave Collision | 0x0407 LGP Photonic Crystal | drop | drop_impact | Pulsewave 2000 ms | yes |

Policy:
- No more than two switches were observed in a 60000 ms switch window.
- No unallowlisted target was selected.
- No random preset roulette observed.
