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

### [DONE] ~~MabuTrace Surfaces 1/2/3/4/5/7 canonical Tier 1 + Surface 8 analyser~~ -- completed 2026-04-27/28
- Spec: `firmware-v3/docs/debugging/TRACE_INSTRUMENTATION_SPEC.md` + 9 detail files in `trace_spec_sections/` — 4,448 lines documenting the Phase 0/1/2 instrumentation that shipped 2026-02-27 (above) and codifying it into Surfaces 1–9 / Tiers 1–4
- Surface 1 (render budget) Tier 1: 5/5 names — RendererActor.cpp 893/955/970 + 1404
- Surface 2 (audio→render handoff) Tier 1: 5/5 names — RendererActor.cpp 1418/1434/1450/1465 + onStart 566
- Surface 3 (audio DSP Core 0) Tier 1: 6/6 names — AudioActor.cpp 728 + post-Publish block
- Surface 4 (network): 15/15 names — WiFiManager dual-branch, WsGateway 316/464, WsOtaCommands 330/733/876/1037 (4 OTA names landed in 016853b7)
- Surface 5 (memory/thermal): 12/12 names — main.cpp 1 Hz health task with hysteresis
- Surface 7 (bench framework): 8/8 toggles registered — `firmware-v3/src/utils/BenchRegistry.{h,cpp}` + `serial/SerialCLI.cpp` `bench {list/begin/split/end/toggle/reset}`
- Surface 8 (analyser): `firmware-v3/tools/analyse_trace.py` 1,597 LOC stdlib-only + `test_native/test_analyse_trace.py` 12-test matrix + 4 K1 V2 baselines in `firmware-v3/tools/baselines/`
- Captain decision Q3 ENCODED in spec: ControlBusFrame → internal DRAM relocation (5 KB cost approved); Tier 1 measurement contract is shipped, architectural relocation is Future-section work
- **Refs:** commits fc122a25 (tooling) → 19007888 (Phase 1B) → 016853b7 (Surface 4 OTA closure) → 929e6817 (baselines seed) → f2ab8671 (spec abstract reconciliation)

---

## Synergy-Topology Programme

The active feature branch is `feature/synergy-topology-phase-0-1`. Phase moves land here; merge to `main` is gated on completion of selected programme work.

### [DONE] ~~Phase 1 — Infrastructure substrates (Moves 1.1–1.6)~~
- Move 1.1 PersistenceHelpers — commit 6907404c
- Move 1.2 EffectRoleFlags substrate — commit 7a077701
- Move 1.3 FramebufferLPF — commit 00628fe7
- Move 1.4 LayerStack composer — commit d2a7499f
- Move 1.6 sinLUT256 + CFLSubstepGate — commit b62cc5d7
- Move 1.5: not yet planned (gap left intentional)

### [DONE] ~~Phase 1B — AFS v2 instrumentation + ControlBus contract lock~~
- Phase 1B instrumentation — commit 19007888
- AFS v2 effect API lock — commit c2dc7d26
- AFS v2 contract docs — commit c7bc6d28
- Phase 1B follow-up (Surface 3 hop-timing decomposition + WS gateway accessor) — commit 8b31e9f6

### [DONE] ~~Phase 2 Move 2.1 — PSRAMScalarRing substrate~~ -- commit f8b52bce

### [DONE] ~~Phase 4 — Audio substrates + cinematic boot~~
- Move 4.1 AUD-21 VoiceMusicClassifier — commit 5021d96a
- Move 4.2 PER-18 AudioGatedConditionalDecay — commit ba816631
- Move 4.4 F6 First-Light Ignition (cinematic boot effect) — commit 4d12edc5 (Captain hardware visual confirmed in commit body)
- Move 4.3: not yet planned

### Phase 5 — Synergy-Topology effect exemplars (3 of 7+ moves) — DONE-DEGRADED
- Move 5.4 RadialTimeScopeEffect (EID 0x2100) — committed in 39406e6b; **DEGRADED-MODE attested 2026-04-28**
- Move 5.6 AttackOnlyPitchVelocityFieldEffect (EID 0x2101) — committed in 39406e6b; **DEGRADED-MODE attested 2026-04-28**
- Move 5.7 BeatParitySpriteEffect (EID 0x2102) — committed in 39406e6b; **DEGRADED-MODE attested 2026-04-28**
- **Native test harness:** 130/130 PASS in 1.97 s — commit f49b4d6a; gated by `pio test -e native_test_phase5` in `firmware-v3_build_check.yml` since 632132e4
- **Hardware traces:** 8 captures committed in `firmware-v3/tools/baselines/` totalling ~21,000 events; `bps_kick_fired` → `bps_sprite_spawn` 1:1 ratio confirmed
- **B.4 DONE-DEGRADED:** Phase 5 sign-off attested under DEGRADED-MODE per Captain authorisation 2026-04-28. Attestation: `firmware-v3/docs/audit/phase_5_visual_sign_off_2026-04-28.md`. Diagnostic-baseline only — does NOT claim hardware visual sign-off, does NOT promote to ship-quality. Cycle 2 sign-off (calibrated, hardware-validated, ship-gate purpose) requires C-1/C-2/C-5 resolution first.

### Pathmode programmes — IntentSpecs feeding device + Pathmode product manifest

These are NOT phases; they are validated engineering intents that update both firmware and Pathmode. See `firmware-v3/research/k1-spec-recommendations-2026-04.md` for the full set; per-programme briefs in `firmware-v3/research/pathmode/agent-prompts/`.

| Programme | Status |
|---|---|
| 03-audio-to-photon-latency (REC-PERF-1) | observability infra shipped (MabuTrace + analyse_trace); per-stage instrumentation pending |
| 04-silentscale-framework-enforcement (REC-PERF-4) | Captain decision encoded 2026-04-26; framework move pending |
| 05-audio-backend-consolidation (REC-PERF-5) | DONE — ESV11 sole production path; PipelineCore deprecated |
| 06-render-contract-enforcement | not started |
| 07-beattracker-correctness-lock (REC-PERF-2) | DONE — comb-tooth algo locked 2026-03-20 in commit fab1802d |
| 09-reliability-core | not started |
| 10-manufacturing-and-ota | not started |

---

## Content

### [P3] Hero photo/GIF for README
- **Status:** Placeholder in README. Owner needs to provide a photo or GIF of the Light Guide Plate in action.

---

## Future (no urgency)

### Surface 6 — Effect lifecycle Tier 1 instrumentation (forward roadmap)
- Spec: `firmware-v3/docs/debugging/TRACE_INSTRUMENTATION_SPEC.md` §6 + `trace_spec_sections/06_effect_lifecycle.md`
- Scope: `effect_init_<eid>`, `effect_cleanup_<eid>`, `effect_render_first_frame_<eid>` instants + `effect_init_us` / `effect_cleanup_us` / `effect_psram_alloc_bytes` counters
- Owner sites: `RendererActor.cpp:570/587/597-600/619-629` + each effect's `init()` for `effect_psram_alloc_<eid>`
- Trigger to revisit: when an effect-switch regression or cold-cache cost investigation lands

### Surface 9 — Causal markers Tier 4 catalogue (forward roadmap)
- Spec: `TRACE_INSTRUMENTATION_SPEC.md` §9 + `trace_spec_sections/09_causal_markers.md`
- Scope: 18 net-new boot-phase + audio-regime hysteresis markers (`audio_silence_entered/exited`, `audio_loud_entered`, `tempo_lock_acquired/lost`, `chord_change`, `boot_phase_*`, `heap_pressure_entered/exited`, `firmware_version`)
- Joint with Surface 5 health regime markers (1 Hz health task hosts `heap_pressure_*`)
- Trigger to revisit: when narrative segmentation in Perfetto becomes worth the trace event budget

### Surface 2 / 3 Tier 2 decomposition spans (gated, opt-in)
- Spec: `TRACE_INSTRUMENTATION_SPEC.md` §2 + §3 Tier 2 tables
- Surface 2 spans (`bus_copy_memcpy`, `bus_retry_check`, `audio_ctx_populate_us`, `motion_engine_tick_us`, `motion_shaper_tick_us`) gated on `FEATURE_TRACE_AUDIO_HANDOFF`
- Surface 3 spans (`i2s_dma_read`, `stm_rfft_256`, `onset_detect_span`, `band_ratio_detect`, `controlbus_publish`) gated on `FEATURE_TRACE_AUDIO_DSP`
- Captain decision deferred per spec Q3: implement only if `audio_snapshot_read` p99 stays > 300 µs after the DRAM relocation (below) lands

### ControlBusFrame → internal DRAM relocation (Captain Q3 RESOLVED in spec, implementation pending)
- Captain-approved 2026-04-27 architectural change: relocate `SnapshotBuffer<ControlBusFrame>` from PSRAM to internal DRAM (5 KB cost approved)
- Expected outcome: 5–10× speedup on `audio_snapshot_read` (current p99 836 µs → target <200 µs)
- The Tier 1 measurement contract (`audio_snapshot_age_us`, `hop_seq_lag`, `size_bytes`, `snapshot_read_retries_total`) is SHIPPED — before/after baseline diffing via `firmware-v3/tools/analyse_trace.py --baseline tools/baselines/k1v2_0x2102_2026-04-27.json --strict` is mechanical
- Strategy options surfaced by the SSA-PHASE-A audit (2026-04-27): (1A) override `AudioActor::operator new` to force `MALLOC_CAP_INTERNAL` — lowest risk, ~50–100 KB cost; (1B) convert `m_controlBusBuffer` to a heap-allocated pointer — closer to 5 KB envelope but ~10 KB minimum for double-buffer; (1C) verify-first via `esp_ptr_in_dram` boot diagnostic before committing budget
- Owner: Captain decision required (1A vs 1B vs 1C); agent applies once authorised

### Audio-side bench toggle wiring (Surface 7 follow-up)
- BenchRegistry framework + 8 toggle registrations + `render.color_correction` consumer wiring SHIPPED
- Audio-side toggles (`audio.lookahead`, `audio.zone_agc`, `audio.chroma_zone_agc`) registered as visibility stubs; per-hop observer call site needs a small AudioActor change (one line at hop entry)
- `render.async_rmt` and `render.dual_strip_parallel`: also stubs; require LedDriver disentanglement (not in current scope)
- `effect.fade_to_black`: per-effect opt-in via a thin `fadeToBlackByGated` helper (not yet authored)
- Trigger to revisit: when Captain asks for runtime A/B of any specific toggle

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

### 0x130E SB Spectral Envelope — deferred concepts (post-baseline 2026-04-30)
Validated baseline at tag `0x130E-validated-solid-8` (Captain hardware verdict: solid 8/10). Three concepts surfaced during repair but deliberately not implemented; do NOT attempt during the current 30-40 effect repair sweep.

1. **Softened end-trail / no-audio fade.** Current build cuts trails extremely quickly when audio drops below active range. Captain's verdict: keep as-is because it reinforces the "audio is the engine" lock-in feel. Revisit only if later user testing says the cut feels too abrupt or anti-climactic. The lever is the no-audio branch in `SbSpectralEnvelopeEffect.cpp` (the `fadeToBlackByDt(..., 16, dt)` path, currently around line 124), not `decayBase`. A new param `m_silenceFadeAmt` defaulting to 16 would let Captain runtime-tune the no-audio decay without recompiling.
2. **Saturation-aware trail-buffer blend.** The current additive `+=` accumulation across overlapping scrolled hues causes pastel/white wash on devices with hot audio input (K1v1 with mic-on-speaker geometry). A saturation-aware blend (screen blend, max blend, or controlled alpha blend) would prevent the additive overflow without lowering boost. Parked because it changes visual character and may affect every other effect that uses similar trail accumulation. Treat as a render-primitive-level investigation, not effect-local.
3. **Per-device runtime tuning via NVS.** The K1v1 / K1v2 acoustic delta (3.5x bass on K1v1 from physical setup) means the same `onsetBoost` doesn't render identically on both. NVS-persisted per-effect parameter overrides would let each device store its own calibrated values. Rejected for now — too much operational overhead while 30-40 effects remain to repair. The serial setter (`effects.parameters.set`) covers the immediate workflow; persistence can be added later if production units ship with varying mic placements.
