---
abstract: "F-6 validation rollup — 2026-05-19 closure of the Zone AGC repartition attestation gap. P2/D-revised executed end-to-end: source proof + consumer enumeration + bench A/B PASS. Zero FE-active Zone AGC consumers; runtime gate toggle changes rendered bands[] by 3.82x mean energy. Closes the DEGRADED-MODE caveat from commit 08a7c997. No firmware behaviour change shipped — code already correct; this validates it on hardware."
---

# F-6 Validation Rollup — 2026-05-19

**Status:** ✅ CLOSED — no DEGRADED-MODE caveat remaining.

F-6 (`CONTROLBUS_NUM_ZONES` 4→3, explicit partition tables) shipped in commit `08a7c997` on 2026-05-17 under a self-disclosed DEGRADED-MODE attestation. Captain selected **P2** on 2026-05-19 — do not accept DEGRADED-MODE for FE launch. This validation closes the gap.

## Attestation chain

| Tier | Artefact | Verdict |
|---|---|---|
| 1 — Source proof | [SOURCE_PROOF.md](SOURCE_PROOF.md) | ✅ PASS — exact 3-zone execution proven at file:line against HEAD `3e0e40d8` |
| 2 — Consumer enumeration | [CONSUMER_TABLE.md](CONSUMER_TABLE.md) | ✅ PASS — zero FE-active Zone AGC consumers; zero effect-path consumers; FE blast radius is `m_frame.bands[]`/`m_frame.chroma[]` only |
| 3 — Bench A/B hardware capture | [BENCH_A_B.md](BENCH_A_B.md) | ✅ PASS — per-band ON/OFF differential 58.9–85.1%; per-zone aggregate 68–78%; mean OFF/ON energy ratio 3.82× on canonical K1v2 ESV11 |

## What this validates

- Canonical K1v2 ESV11 `_32khz` build calls `ControlBus::UpdateFromHop` every audio hop (`AudioActor.cpp:1955`).
- `UpdateFromHop` loops `CONTROLBUS_NUM_ZONES = 3` times on both band partition (`ControlBus.cpp:375`) and chroma partition (`:440`).
- The compile-time `static_assert` at `ControlBus.h:56–63` guarantees the partition tables cover all 8 bands and 12 chroma bins exactly once.
- All four runtime gates default ON (`ControlBus.h:643,650,654-655` + `BenchRegistry.cpp:34-35`), so production K1v2 exercises the partition every hop unless explicitly disabled.
- Toggling the bench gates `audio.zone_agc` + `audio.chroma_zone_agc` OFF at runtime measurably changes `m_frame.bands[]` (3.82× energy ratio), confirming the gates' production effect.
- REST `GET /api/v1/audio/zone-agc` and WS `audio.zone-agc.get` are intentionally disabled on ESV11 builds (`AudioHandlers.cpp:~1106`, `WsAudioCommands.cpp:~507` — both return `FEATURE_DISABLED`). This is product design, not a bug. The disabled endpoints are not the FE validation surface; bands[] is.

## What this does NOT change

- No firmware behaviour modified. F-6's behavioural commit was `08a7c997` (2026-05-17).
- No effect file touched. No render path touched. No REST/WS handler touched.
- No `#if FEATURE_AUDIO_BACKEND_ESV11` guards lifted (Captain's hard stop).
- No new code added beyond evidence-capture script.

## Hardware test receipt

K1v2 MAC `b4:3a:45:a5:87:f8`, port `usbmodem1301`, commit `3e0e40d8` flashed, music played externally into mic range, agent ran `capture_zone_agc_ab.py` start to end without panic, WDT reset, RMT error, or shed-latch event. Both gates restored ON at end of session.

## Closure pointers

- BACKLOG.md F-6 row → resolved (Task 2/3/4 DONE).
- `feedback_serial_bench_uppercase.md` memory written (lowercase `bench` SerialCLI collision with single-char `b` handler — operating quirk discovered 2026-05-19).
- NotebookLM infographic audio-AGC restoration → backlogged (unblocked but post-FE-launch).

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-19 | agent:claude-opus-4-7 | Created. Rollup of three-tier attestation chain (source + consumer + bench A/B) closing F-6 without DEGRADED-MODE caveat per Captain's D-revised P2 protocol. |
