# Captain Review Table

| Gate | Evidence | Result |
|---|---|---|
| Serial support works | `songaware status`, `songaware off`, `songaware on`, and `songaware mode balanced` all returned structured status over USB serial. | PASS |
| Song-aware can be disabled cleanly | Final state after run: `enabled=false mode=off effectiveMode=off owner=none suppressed=disabled`. | PASS |
| Family morphing off | Every status sample reported `familyMorphing=false`. | PASS |
| Constrained switching off | Every status sample reported `constrainedSwitching=false`. | PASS |
| Automatic effect switching off | `automaticEffectSwitches=0` across preflight, A, B, and final state. | PASS |
| Effect id stable | `0x1302 K1 Waveform` start/end and compact poll samples for all A/B runs. | PASS |
| A baseline fixed | A parameter delta was 0 on all tracks. | PASS |
| B autonomous parameter activity | B parameter deltas: 25,560, 26,741, 30,210. | PASS |
| B health worse than A | No. A and B both had zero hard error counters. | PASS |
| NVS save avoided | No `saveEdgeMixer`, `S`, `}`, `Csave`, WiFi save, preset save, or REST/WS persistence route used. | PASS |
| REST/WS avoided | All controls and captures used USB serial. | PASS |
| Optical visible gain captured | No optical camera or human visual feed available to this agent session. Serial state changes are available, visible LED subjective gain is not proven. | BLOCKED |
| Per-frame overlay outputs exposed | Persisted `vp stack` controls remained fixed by design; current serial status does not expose post-overlay per-frame speed/intensity/complexity values. | LIMITED |

## Practical read

The serial/runtime sub-gate passed: parameter mode is active, autonomous, fixed-effect, non-persistent, and health-neutral in this A/B run.

The optical review gate did not pass because the session lacked a visual capture path. Do not treat this evidence as proof of visible improvement on the physical LEDs.

