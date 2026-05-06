# C-5 Phase 5 Hardware Sweep

**Date:** 2026-05-06
**RBDO label:** GROUNDED for serial/device/build/trace evidence; DEGRADED-MODE for visual sign-off because the sweep was run continuously and only one Captain visual judgement was captured.
**Scope:** Record the first C-5 hardware execution of the timestamped observable matrix for Phase 5 effects `0x2100`, `0x2101`, and `0x2102`.

## Evidence Boundary

This is not a ship-gate and not full visual sign-off. It proves that the matrix can be executed on K1v2 hardware, that trace evidence exists for every row, and that at least one visual row failed fixture/effect suitability.

| Item | Evidence |
|---|---|
| Device | K1v2 on `/dev/cu.usbmodem2101`, upload MAC `b4:3a:45:a5:87:f8`. |
| Firmware | `esp32dev_audio_esv11_k1v2_32khz_trace` flashed successfully before the sweep. |
| Control path | Serial only. No REST, STA, or WiFi dependency. |
| Audio privacy | Private local audio windows were used outside the repo. No source media, absolute paths, dataset IDs, or track names are committed here. |
| Trace coverage | 18/18 C-5 rows produced trace JSON and row metadata after rerunning the initial `RTS-1` trace-parse failure with the corrected private extractor. |
| Visual coverage | Captain gave one explicit visual judgement during the run: `RTS-4` was an extremely poor effect/fixture choice. Other rows have trace evidence but no captured PASS/FAIL visual answer. |

## DEGRADED-MODE Label

- **Unresolved assumption:** Rows without explicit Captain answers may or may not visually pass on the LGP.
- **Risk if wrong:** Technical trace health could be mistaken for visual quality, allowing poor-looking rows to survive.
- **Fallback:** Treat this sweep as trace-complete but visual-incomplete. Do not promote Phase 5 to ship-gate.
- **Revisit trigger:** Any attempt to claim Phase 5 ship-gate readiness, replace the failed `RTS-4` fixture/effect pairing, or capture row-level Captain PASS/FAIL answers.
- **Debt count / affected outputs:** This affects Phase 5 cycle-2 sign-off and any harness derived from C-5.

## Technical Summary

All effect-specific render spans stayed below the 2 ms effect-code ceiling in the final trace set. `fastled_rmt_show` remained around the expected physical wire-time layer for the dual 160-LED output.

| Row | Effect | Events | Effect p99 us | RMT p50/p99 us | BPS fired/spawn | Visual note |
|---|---:|---:|---:|---:|---:|---|
| `RTS-1` | `0x2100` | 5501 | 480 | 6149/6257 | 0/0 |  |
| `RTS-2` | `0x2100` | 5504 | 461 | 6159/6245 | 0/0 |  |
| `RTS-3` | `0x2100` | 5503 | 465 | 6156/6248 | 0/0 |  |
| `RTS-4` | `0x2100` | 5505 | 472 | 6157/6252 | 0/0 | FAIL: Captain said the effect/fixture choice was "extremely poor". |
| `RTS-5` | `0x2100` | 5505 | 470 | 6158/6249 | 0/0 |  |
| `RTS-6` | `0x2100` | 5502 | 483 | 6152/6266 | 0/0 |  |
| `PVF-1` | `0x2101` | 5506 | 939 | 6154/6251 | 0/0 |  |
| `PVF-2` | `0x2101` | 5506 | 1000 | 6160/6255 | 0/0 |  |
| `PVF-3` | `0x2101` | 5506 | 996 | 6155/6244 | 0/0 |  |
| `PVF-4` | `0x2101` | 5508 | 947 | 6161/6264 | 0/0 |  |
| `PVF-5` | `0x2101` | 5508 | 990 | 6161/6253 | 0/0 |  |
| `PVF-6` | `0x2101` | 5508 | 991 | 6154/6259 | 0/0 |  |
| `BPS-1` | `0x2102` | 5520 | 1434 | 6157/6247 | 0/0 |  |
| `BPS-2` | `0x2102` | 5509 | 1568 | 6161/6255 | 9/9 |  |
| `BPS-3` | `0x2102` | 5509 | 1573 | 6153/6253 | 10/10 |  |
| `BPS-4` | `0x2102` | 5506 | 1522 | 6160/6269 | 7/7 |  |
| `BPS-5` | `0x2102` | 5505 | 1523 | 6153/6259 | 2/2 |  |
| `BPS-6` | `0x2102` | 5520 | 1427 | 6161/6248 | 0/0 |  |

## Interpretation

- The C-5 matrix is executable end-to-end on K1v2 over serial.
- Trace capture is complete for every row.
- The effect-code hot path is not the immediate blocker: all final effect-specific p99 render spans were below 2 ms.
- The RMT/wire-time layer remains present and sane at roughly 6.1-6.3 ms in these traces.
- BPS kick-trigger rows show 1:1 `bps_kick_fired` to `bps_sprite_spawn` ratios in the captured trace windows where kicks occurred.
- `RTS-4` is not usable as a pass row in its current form. It needs replacement or redesign before Phase 5 can be promoted.

## Decision

C-5 hardware execution is **TRACE-COMPLETE / VISUAL-INCOMPLETE**:

- Keep `BACKLOG.md` C-5 as DONE-DEGRADED for observable authoring and matrix execution.
- Do not claim Phase 5 ship-gate readiness.
- Treat `RTS-4` as a failed row requiring a replacement fixture/effect pairing or a deliberate product decision that dense bright/loud material is not an RTS sign-off row.
- A future ship-gate run must capture explicit Captain PASS / FAIL / DEGRADED-PASS answers for every row, not only trace files.
