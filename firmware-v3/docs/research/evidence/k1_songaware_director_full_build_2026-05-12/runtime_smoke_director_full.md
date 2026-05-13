RBDO label: GROUNDED

Runtime smoke method:
- USB serial control.
- No REST/WS dependency.
- Uploaded K1 build to `/dev/cu.usbmodem1101`.
- Used local audio material plus generated controlled broadband audio from `/tmp`, not app/cloud metadata.

Director decisions observed:
- 59488 ms: 0x1302 K1 Waveform -> 0x0407 LGP Photonic Crystal, state=drop, reason=drop_impact.
- 328252 ms: 0x0407 LGP Photonic Crystal -> 0x1B01 LGP KdV Soliton Pair, state=dense, reason=dense_legibility.
- 350270 ms: 0x1B01 LGP KdV Soliton Pair -> 0x0204 LGP Wave Collision, state=build, reason=build_pressure.
- 388640 ms: 0x0204 LGP Wave Collision -> 0x0407 LGP Photonic Crystal, state=drop, reason=drop_impact.

Suppression proof:
- same_effect after 0x0407 active.
- low_confidence after playback stopped.

Restore proof:
- final `enabled=false mode=off`
- final effect 0x1302 K1 Waveform
- final hard counters zero.
