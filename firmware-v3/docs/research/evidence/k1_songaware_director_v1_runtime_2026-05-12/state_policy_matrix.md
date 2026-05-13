# State Policy Matrix

RBDO label: GROUNDED

## State Detection Inputs

Director V1 uses existing runtime fields only:

- RMS / fast RMS
- flux / fast flux
- `isSilent` / `silentScale`
- BPM / tempo confidence
- beat strength / onset event
- overall saliency
- liveliness

Source anchor: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:491`.

## Policy

| State | Runtime posture | Switching posture | Parameter support |
|---|---|---|---|
| silence | Low-density hold/fade posture | No aggressive switching; confidence gate suppresses | Drive falls toward zero. |
| ambient | Calm sparse slow motion | May select LGP Modal Resonance after dwell/cooldown | Lower speed/complexity envelope. |
| steady | Stable readable movement | May select LGP Holographic after gates | Moderate overlay. |
| build | Rising pressure | May target LGP Wave Collision after gates | Added speed/intensity pressure. |
| drop | Decisive impact | May target LGP Photonic Crystal after gates and confidence >= 0.60 | Added intensity impact. |
| breakdown | Reduced density/release | May target LGP Chromatic Lens | Lower complexity/speed. |
| dense | High-fill but legible | May target KdV Soliton Pair if health is clean | Complexity capped relative to generic high-energy overlay. |
| transition | Bridge posture | May target LGP Chromatic Pulse | Bridge language without roulette. |
| unknown | No visual-language claim | No switching | No claim. |

## Gate Constants

- Evaluation period: 500 ms.
- Stable-state hold: 1200 ms.
- Drop fast hold: 250 ms at high confidence.
- Minimum dwell before switch: 8000 ms.
- Cooldown after switch: 20000 ms.
- Rate limit: 2 switches per 60000 ms.
- Manual suppression: 15000 ms.
- Show suppression heartbeat: 1000 ms.

Source anchors:

- Constants: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:17`.
- Gates: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:259`.
- Dwell/cooldown/rate gates: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:301`.
- Health gate: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:283`, `firmware-v3/src/core/songaware/SongAwareDirector.cpp:612`.
