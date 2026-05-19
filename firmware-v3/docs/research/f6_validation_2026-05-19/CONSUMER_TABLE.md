---
abstract: "F-6 Zone-AGC consumer enumeration evidence table. Result: 10 consumers found across firmware-v3 + tab5-encoder + iOS + dashboard + composer; ZERO FE-launch-relevant consumers active on canonical K1v2 ESV11 (all 7 runtime consumers gated #if FEATURE_AUDIO_BACKEND_ESV11 → FEATURE_DISABLED). No effect-path consumer. Confirms F-6 FE blast radius is via m_frame.bands[]/m_frame.chroma[] smoothing path only. Companion to SOURCE_PROOF.md."
---

# F-6 Zone-AGC Consumer Enumeration

**Date:** 2026-05-19
**Source:** Phase 1 + Phase 2 dispatched Explore SSA against firmware-v3 + lightwave-ios-v2 + tab5-encoder + lightwave-dashboard + k1-composer
**Tier:** Step 1 of D-revised protocol (consumer surface evidence for bench A/B scoping)
**Confidence:** HIGH (exhaustive grep + clangd cross-check; effects under `src/effects/` verified clean)

---

## Consumer evidence table

| # | Consumer (file:line) | Function/method | Reads Zone-AGC output? (Y/N + symbol) | ESV11-active? (Y/N + reason if N) | FE-launch-relevant? | Implication for hardware validation |
|---|---|---|---|---|---|---|
| 1 | `ControlBus.h:545–556` | `getZoneFollower`, `getZoneMaxMag`, `getChromaZoneFollower`, `getChromaZoneMaxMag` | Y — direct reads of `m_zones[z].max_mag_follower` and `.max_mag` | Y — accessors compiled unconditionally | N — internal API, not exposed | Foundational accessor; not directly user-visible |
| 2 | `AudioActor.h:415–425` | `ZoneAgcSnapshot` struct + `getZoneAgcSnapshot()` declaration | Y — contains `followers[]` and `maxMags[]` | N — inside `#if !FEATURE_AUDIO_BACKEND_ESV11` at `AudioActor.h:387` | N | Not compiled on canonical K1v2 |
| 3 | `AudioActor.cpp:375–390` | `AudioActor::getZoneAgcSnapshot()` body | Y — calls `m_controlBus.getZoneFollower(z)` + `getZoneMaxMag(z)` | N — entire function inside `#if !FEATURE_AUDIO_BACKEND_ESV11` | N | Not compiled on canonical K1v2 |
| 4 | `AudioHandlers.cpp:1097–1127` | REST `handleZoneAGCGet` (`GET /api/v1/audio/zone-agc`) | Y (in !ESV11 branch) — reads `snapshot.enabled`, `.lookaheadEnabled`, `.followers[]`, `.maxMags[]` | **N** — returns `HTTP 501 FEATURE_DISABLED` on ESV11 (`:1108–1111`) | N | Disabled-on-ESV11; observation surface unavailable on production |
| 5 | `AudioHandlers.cpp:1129–1186+` | REST `handleZoneAGCSet` (POST) | N — writes only via `setZoneAgcEnabled`, `setLookaheadEnabled`, `setZoneAGCRates` | **N** — `FEATURE_DISABLED` on ESV11 | N | Disabled-on-ESV11 |
| 6 | `WsAudioCommands.cpp:500–527` | WS `handleAudioZoneAgcGet` (`audio.zone-agc.get`) | Y (in !ESV11 branch) — reads same snapshot fields as row 4 | **N** — `FEATURE_DISABLED` on ESV11 (`:507–509`) | N | Disabled-on-ESV11; WS observation surface unavailable on production |
| 7 | `WsAudioCommands.cpp:529–565` | WS `handleAudioZoneAgcSet` | N — writes only | **N** — `FEATURE_DISABLED` on ESV11 | N | Disabled-on-ESV11 |
| 8 | `WsAudioCommands.cpp:566–594` | WS `handleAudioSpikeDetectionGet` | Y (in !ESV11) — reads `snapshot.lookaheadEnabled` only | **N** — `FEATURE_DISABLED` on ESV11 (`:573–575`) | N | Spike-detection tied to lookahead flag; disabled-on-ESV11 |
| 9 | `AudioHandlers.cpp:1180–1217` | REST `handleSpikeDetectionGet` | Y (in !ESV11) — reads `snapshot.lookaheadEnabled` only | **N** — `FEATURE_DISABLED` on ESV11 (`:1194–1198`) | N | Same as row 8 |
| 10 | `test_control_bus_bench_toggles.cpp:134–167` | Zone AGC unit tests | Y — verifies follower/maxMag math via accessor calls | N/A — native test only | N — tests only | Native unit-level coverage (already passing per commit `08a7c997`) |

---

## Summary

- **Total consumers found:** 10 (7 runtime API surface + 1 struct/accessor definition + 1 method definition + 1 test).
- **FE-launch-relevant consumers (ESV11-active on canonical K1v2 production):** **ZERO**.
- **Disabled-on-ESV11 consumers:** 6 (rows 4, 5, 6, 7, 8, 9 — all REST + WS Zone-AGC and spike-detection handlers).
- **Compile-time-excluded consumers:** 2 (rows 2, 3 — declaration and body inside `#if !FEATURE_AUDIO_BACKEND_ESV11`).
- **Effect-path consumers:** ZERO. Verified by SSA grep across `firmware-v3/src/effects/` and `firmware-v3/src/visuals/`. Effects consume only `m_frame.bands[]` / `m_frame.chroma[]` (the post-partition smoothed outputs), not Zone-AGC follower/maxMag accessors.
- **Cross-repo consumers:** ZERO. iOS, dashboard, composer, tab5-encoder do not reference Zone-AGC symbols directly — they consume the REST/WS endpoints (rows 4, 6) which return `FEATURE_DISABLED` on production K1v2.

---

## Implication for Step 2 attestation

1. **No effect-path consumer surprise** → the plan's premise holds: F-6's blast radius on canonical K1v2 ESV11 is exclusively via `m_frame.bands[]` / `m_frame.chroma[]` smoothing output.
2. **No external API consumer is active on production** → the bench A/B captures should NOT also poll REST/WS Zone-AGC endpoints (they would return `FEATURE_DISABLED` and waste effort).
3. **Bench A/B target is `bands[]` differential under gate ON vs gate OFF.** Observable via `adbg spectrum` over serial. This is the only positive runtime attestation available without lifting the `#if FEATURE_AUDIO_BACKEND_ESV11` guards (which Captain has explicitly rejected).

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-19 | agent:claude-opus-4-7 | Created. Phase 2 SSA enumeration of every Zone-AGC consumer across the SpectraSynq codebase. Cleared the "no effect-path consumer" confidence gate; confirmed all 7 runtime consumers are ESV11-disabled. Step 2 bench A/B target is the `m_frame.bands[]` differential under gate ON/OFF. |
