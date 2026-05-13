# Director V1 Runtime Decision

RBDO label: GROUNDED

## Required Runtime Evidence

| Requirement | Result | Evidence |
|---|---|---|
| Build passes | PASS | `pio run -e esp32dev_audio_esv11_k1v2_32khz` succeeded. |
| Upload passes | PASS | Upload to `/dev/cu.usbmodem1101`, MAC `b4:3a:45:a5:87:f8`, succeeded. |
| `songaware status` works | PASS | Status printed enabled/mode/state/effect/health/audio telemetry. |
| `songaware mode director` works | PASS | Serial status reported `mode=director`. |
| `songaware switching on` only works in Director | PASS | Rejected while off, accepted after Director mode. |
| `songaware off` disables cleanly | PASS | Final status: `enabled=false mode=off constrainedSwitching=false suppressed=disabled`. |
| At least one visual-language/effect decision occurs | PASS | Corrected medium-candidate policy produced `0x1302 K1 Waveform -> 0x0407 LGP Photonic Crystal`, reason `drop_impact`. |
| Switch target is allowlisted and reason-logged | PASS | Target `0x0407` is allowlisted; serial log included state/confidence/previous/target/family/language/reason. |
| Manual ownership suppresses Director | PASS | After manual `setEffect`, status reported `owner=manual suppressed=manual_owner`. |
| Show ownership suppresses Director | SOURCE PASS | ShowDirector marks show ownership while playing before renderer messages. Source: `ShowDirectorActor.cpp:344`, `ShowDirectorActor.cpp:801`; no active show was present in smoke (`Has show: NO`). |
| Low confidence suppresses switching | PASS | Status before audio run showed `suppressed=low_confidence` / silence-low-confidence conditions. Source gate: `SongAwareDirector.cpp:259`. |
| Health counters suppress switching if degraded | SOURCE PASS | Source gate suppresses if `showSkips`, `failures`, `rmtErrors`, or `underruns` are nonzero. Source: `SongAwareDirector.cpp:283`, `SongAwareDirector.cpp:612`. Runtime counters remained zero. |
| No NVS save path called | PASS | No Song-Aware NVS route exists; no save command invoked. |
| No production default changes | PASS | Feature remains runtime-controlled and final status restored `songaware off`; no defaults/NVS changed. |
| No unconstrained effect roulette | PASS | Fixed policy table only; no random selection. |
| No product PASS claimed | PASS | This decision is runtime-ready-for-visual-review only. |

## Decision

PASS_DIRECTOR_V1_RUNTIME_READY_FOR_VISUAL_REVIEW

## Scope Boundary

This is not final product approval. It proves runtime support and live bounded Director decisions. Optical A/B review remains required before product-quality claims.
