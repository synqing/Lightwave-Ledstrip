# LightwaveOS Backlog

Prioritised engineering backlog. Items are tagged by category and roughly ordered by impact.

---

## Critical — Upstream Calibration Debt

Per the RBDO Gate (`CLAUDE.md` top), these upstream facts are unresolved. Until each is resolved or explicitly accepted under DEGRADED-MODE with disclosed risk, every tactical output that depends on them must be labelled DEGRADED-MODE or REFUSED. New tactical outputs MUST NOT add a fourth dependent to any URGENT row without resolving it first.

### C-1 — Microphone-domain operating envelope (URGENT)
What mic-domain RMS / peak / silentScale-trip range was the firmware tuned against?
- **Blocks:** LUFS target for any audio test sweep; AFS v2 silentScale validation; any "tuned-regime sign-off" claim.
- **Affected outputs:** ≥ 3.
- **Priority:** URGENT.
- **Revisit trigger:** Captain-allocated 30–60 min hardware envelope characterisation pass, OR audit of `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md` confirms it is already documented there.

### C-2 — Feature × effect × dwell coverage matrix (HIGH)
Which AFS v2 features × which Phase 5 effects × what minimum dwell each phenomenon needs to manifest visually.
- **Blocks:** sign-off sweep duration; per-clip dwell minimums; rubric anchor points.
- **Priority:** HIGH.
- **Revisit trigger:** Phase 5 sign-off authorisation moment, or any new audio-reactive effect requiring fixture validation.

### C-3 — Clip licence status + K1 repo public-status (HIGH)
Are the hybrid-beat-tracker corpus clips licensed for inclusion or path-reference in K1 firmware artefacts? What is the K1 repo's public-status at launch (open-source, public-on-release, private)?
- **Blocks:** clip pool composition for sign-off sweep; calibrated WAV storage policy; any third-party music reference in this repo.
- **Priority:** HIGH (legal exposure if assumed wrong).
- **Revisit trigger:** Captain answers (a) repo public-status at launch, (b) hybrid-beat-tracker licence applicability for commercial-product testing, (c) presence/absence of a Captain-licensed audiophile reference library.

### C-4 — First sign-off purpose (MEDIUM — DECIDED)
**Decision (current):** First Phase 5 sign-off is a **diagnostic baseline**, not a ship gate, not a regression detector.
- **Reason:** no calibrated baseline or timestamped observables exist yet (C-5 unresolved; C-1 unresolved).
- **Priority:** MEDIUM (decided; pending re-audit when C-5 lands).
- **Revisit trigger:** When C-5 produces ratifiable observables, the next sign-off cycle can be promoted to ship-gate (cycle 2) or regression-detector (cycle 3+).

### C-5 — Per-effect timestamped observables (MEDIUM)
What exactly does the operator look for, anchored to (clip, timestamp, measurable phenomenon), per Phase 5 effect (RTS / PVF / BPS)?
- **Blocks:** final rubric contents regardless of rubric shape (Y/N, 1–5, freeform).
- **Depends on:** C-2.
- **Priority:** MEDIUM.
- **Revisit trigger:** After C-2 lands; pre-flight to any sign-off harness build.

### F-1 — Contract authority (HIGH — DECIDED 2026-05-01)
Is the YAML at `docs/protocol/k1-{rest,ws}-contract.yaml` source-of-truth, or has it drifted past usability? Audit found ~50 REST routes + ~40 WS commands in firmware are absent from the contract; 5 WS commands in YAML have firmware handlers commented out (`WsFilesystemCommands.cpp:21-25`).
- **Decision (current):** Firmware is source-of-truth at runtime. Contract YAML is a *regeneratable documentation artefact*, NOT a lock-and-conform document. iOS aligns to firmware reality directly; YAML reconciliation is a separate, deferrable docs task (regenerate from firmware route registry when needed).
- **Reason:** Firmware is the running code; YAML drift does not break clients. Lock-and-conform creates a permanent governance burden that nobody owns. Pull-from-firmware is sustainable; iOS Phase 2 work targets firmware behaviour, not the contract artefact.
- **Affected outputs:** ≥ 3 (Phase 2 iOS scope unblocked).
- **Priority:** HIGH (decided; revisit if a third-party iOS client or compliance regime re-elevates contract authority).
- **Revisit trigger:** External-consumer onboarding requiring a stable published contract, OR firmware route surface stabilises and contract regeneration becomes mechanical.

### F-2 — Effect production cohort (HIGH — DECIDED 2026-05-01)
Of the 25+ new effects landed since iOS anchor `569d3e4b` (commits `f91619bf` 20 LGP AR variants, `99ca01a2` 11 K1 parity / Bloom V2, `9d068612`+`7a9ebd1a` 7 SB Waveform/Spectral incl. 0x130E, `4dfadc7b` 5 Beat Prism Onset, `39406e6b` Phase 5 exemplars), which are PRODUCTION (user-facing) vs EXPERIMENTAL (A/B research, dev-only)?
- **Decision (current):** Resolved by the `isExperimental` metadata flag already shipped (CHANGELOG `### Added` 2026-04-26 entry — `PatternRegistry::isExperimental(EffectId)` emitted on `/api/v1/effects` and `effects.getMetadata`). iOS `EffectViewModel.filteredEffects()` already filters experimentals out of the default view. 13 effects tagged per Captain verdicts. Going forward, every new effect must be tagged at registration time.
- **Reason:** Flag-based filtering is durable and scales with new effects; ad-hoc cohort lists rot. The mechanism is in firmware AND iOS; Phase 2 picker work just needs to surface a "show experimental" toggle for power users.
- **Priority:** HIGH (decided; ongoing discipline to tag new effects at registration).
- **Revisit trigger:** Tab5 client filtering need (currently architecturally deferred — Tab5's uint8 effect-index array can't key 16-bit namespaced EIDs).

### F-3 — Path canonicalisation (MEDIUM — DECIDED 2026-05-01)
Firmware accepts both modern (`/effects/current`, `/palettes/current`) and legacy (`/effects/set`, `/palettes/set`) paths. iOS currently uses legacy. Standardise on which?
- **Decision (current):** KEEP LEGACY paths. Firmware supports both; iOS uses legacy; no behavioural difference; no breakage. Migration to modern paths is a future hygiene task with no current payoff.
- **Reason:** Refactor risk (across RESTClient + every callsite + every test) exceeds the cost of staying on legacy. Re-evaluate ONLY when firmware deprecates a legacy path.
- **Priority:** MEDIUM (decided; no-op for Phase 2).
- **Revisit trigger:** Firmware deprecation of a legacy path with a removal-by date.

### F-4 — Runtime parameter UX scope (HIGH — DECIDED 2026-05-01)
Is `effects.parameters.set` an end-user surface (sliders in effect detail view) or developer-only (debug overlay)? Effect 0x130E exposes `silenceGate`, `decayBase`, `decaySlope`, `onsetBoost` and similar — not consumer-friendly knobs.
- **Decision (current):** END-USER surface. Phase 2 ships an effect-detail parameter sheet that exposes every parameter with a `displayName` field via type-appropriate controls — FLOAT → slider, INT → stepper, BOOL → toggle, ENUM → picker, `unknown` → slider over [min, max]. The `parameterType` infrastructure (Phase 1, post-`4398af3b`) drives the control choice.
- **Reason:** 0x130E and similar parameters ARE tunable knobs that users will want for live performance contexts. Phase 1's decoder already classifies them; making them user-facing is the natural extension. Parameters without a `displayName` are treated as developer-only and hidden — that's the heuristic for "consumer-friendly".
- **Priority:** HIGH (decided; Phase 2 SSA executes).
- **Revisit trigger:** Captain UX feedback on the parameter sheet's first hardware test, OR firmware adds a `userFacing: bool` flag making the heuristic explicit.

---

## Performance

### [DONE] ~~RendererActor vTaskDelay(1) costs 10 ms per frame~~ — resolved in d943101a
- Original `vTaskDelay(1)` before `showLeds()` replaced with `vTaskDelay(0)` (equivalent to `taskYIELD()`)
- Pre-show delay removed entirely; renderer now self-clocked at 120 FPS via `esp_rom_delay_us` frame pacer
- FastLED.show() yields naturally via `xSemaphoreTake(gTX_sem)` during ~4.8ms RMT DMA transmission
- Watchdog fed explicitly via `esp_task_wdt_reset()` every 10 frames (no IDLE1 dependency)
- **Discovered:** 2026-02-27 | **Resolved:** d943101a (2026-02-27, stable-effect-ids integration)

### [DONE] ~~Frame pacer uses esp_rom_delay_us busy-wait~~ — resolved in 9a055687
- Replaced `esp_rom_delay_us()` CPU spin with `esp_timer` one-shot + `ulTaskNotifyTake()`
- Zero-overhead event-driven wait, falls back to `taskYIELD()` for <100us remainders
- Saves 0-2ms CPU spin per frame, yielded to IDLE1 instead
- **Discovered:** 2026-03-21 | **Resolved:** 9a055687 (2026-03-24)

---

## Observability

### [DONE] ~~Activate MabuTrace~~ -- completed 2026-02-27
- Phase 0: `TRACE_INIT(64)` in `main.cpp`, `trace` serial command, `esp32dev_audio_trace` build env
- Phase 1: 6 audio spans (i2s_dma_read, dc_agc_loop, rms_flux, tempo_update, controlbus_build, snapshot_publish)
- Phase 2: 12 renderer spans (render_frame, effect_render, zone_compose, colour_correction, pre_show_yield, show_leds, fastled_rmt_show, audio_snapshot_read + fps/frame_us counters + frame_drop/effect_change instants)
- System-wide `config/Trace.h` header, `AudioBenchmarkTrace.h` is now a redirect
- **Remaining:** Phase 3 (cross-core `TRACE_FLOW_OUT/IN` arrows) deferred to Future section

### [DONE] ~~MabuTrace capture guide~~ -- completed 2026-02-27
- 454-line guide at `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md`
- Covers: build, capture, Perfetto import, span reference, troubleshooting, licence isolation

---

## Content

### [P3] Hero photo/GIF for README
- **Status:** Placeholder in README. Owner needs to provide a photo or GIF of the Light Guide Plate in action.

---

## Future (no urgency)

### Investigate Perfetto-compatible tracing alternatives
- MabuTrace is GPL-3.0 (dev-only, never ships -- acceptable but not ideal)
- Alternatives researched:
  - Custom Chrome JSON tracer (~300 LOC, Apache-2.0 compatible)
  - Tonbandgeraet (MIT, native Perfetto protobuf output)
  - ESP-IDF `esp_app_trace` (Apache-2.0, but outputs SystemView format, NOT Perfetto)
  - SEGGER SystemView and Percepio Tracealyzer are NOT Perfetto-compatible
- If GPL-3.0 becomes a concern, the Chrome JSON approach is simplest (~300 lines of C++)
- Reference: `firmware-v3/docs/research/EMBEDDED_TRACING_RESEARCH_2026.md`

### ControlBusFrame hot/cold split
- The ~2 KB ControlBusFrame is copied atomically across cores via SnapshotBuffer
- If cross-core contention becomes measurable, split into hot (~100 B: RMS, flux, bands) and cold (~1.9 KB: full spectrum, waveform) sub-structs with independent update rates

### MabuTrace library risk
- 7 GitHub stars, 1 fork, single maintainer (mabuware/Matthias Buhlmann)
- Core is only ~15 KB of C -- could fork or reimplement under Apache-2.0 if abandoned
- Library is feature-complete and stable for current needs
