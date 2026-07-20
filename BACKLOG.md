# LightwaveOS Backlog

Prioritised engineering backlog. Items are tagged by category and roughly ordered by impact.

---

## Critical — Work Blocks

Work Blocks are critical tasks discovered while executing another mission. They must be scoped, logged, assigned out, and then the original mission must resume unless Captain explicitly re-scopes the session or an RBDO hard stop prevents continuation. Protocol: `instructions/work-blocking-protocol-v1.md`.

## Technical Debt

- **Restore-point latch consolidation (SynqMatrix)** — `SerialCLI.cpp`, `SerialJsonGateway.cpp`, and `WsSynqMatrixCommands.cpp` each carry an independent file-scope restore-point latch for SynqMatrix runtime state. Three-way drift risk if one surface is updated without the others. Consolidate into a single shared module under `core/synqmatrix/`. **Revisit by 2026-06-30** or when the next SynqMatrix surface (e.g. iOS direct) is added — whichever first. Deferred from PR #16 to keep blast radius bounded.
- **SynqMatrix V1 tempo telemetry source** — V0 exposes `missedPredictionCount` and `tempoWinnerChanges` as unavailable zero counters because the current SynqMatrix input path has no beat-prediction miss source or tempo winner/bin identity. Real population requires a separately scoped V1 music-timebase / tempo-bank source brief.
- **`[SynqMatrix] Director autonomous owner-clobber on effect switch — RESOLVED 2026-05-19 in current source.** `SynqMatrix::queueDirectorSwitch()` and `SynqMatrix::notifySwitchApplied()` now gate Director owner writes behind `currentOwner != Manual && currentOwner != Show`, so user/show authority survives queued and applied Director switches. The original symptom was recorded from 2026-05-18 zone-control-restore validation.
- **`[SynqMatrix] Director ZoneComposer clamp disabled capture mode — RESOLVED 2026-05-19 in current source.** `RendererActor` no longer treats active Director ownership as permission to force `ZoneComposer::setEnabled(false)`. Historical per-state `zoneEnabled=0` policy is now release-time cleanup only (`owner=None`), so Director can continue effect/palette/hue/EdgeMixer decisions while ZoneComposer remains enabled for multi-zone content capture. Hardware validation 2026-05-19: K1v2 stayed in STA, SynqMatrix owner=`director`, Triple Rings stayed enabled for >60 s, no clamp log reappeared, and health stayed clean (`shed=0`, `show_skips=0`, failures=0, RMT errors=0).
- **`[REST] Per-zone POST routes returned 404 — RESOLVED 2026-05-19 in current source.** `POST /api/v1/zones/{1-3}/effect|brightness|speed|palette|blend|enabled` now registers literal paths for zones 1-3 instead of relying on `registry.onPostRegex`, so the documented finite REST control paths work even when ESPAsyncWebServer regex support is not compiled in. Hardware validation 2026-05-19: uploaded to K1v2 `b4:3a:45:a5:87:f8`, moved K1 to saved STA over serial (`192.168.1.106`), verified all six setters return HTTP 200 for Zones 1-3 after applying the source-defined Triple Rings layout. WS `zone.setEffect/setPalette/setBlend/...` remains the canonical client path used by iOS, Tab5, and the fallback HTML controller.
- **`[WebServer] Low-heap shedding — RESOLVED 2026-05-19 in tag `phase-1a-survival-fix` (commit `3770863e`).** Single-client connect collapse fixed by AsyncTCP service-task stack trim 16 KB → 8 KB (`CONFIG_ASYNC_TCP_STACK_SIZE=8192`). Hardware-validated: pre-trim shed latched within 456 ms of first WS client; post-trim zero shed events across 600 s including a live client connect at t=52s. Region-1 largest free block under client load: 660 B → 9,716 B sustained. MabuTrace timing non-regressing (render p99 −11.5%, audio_snapshot_age p99 −8.2%). See [Phase 1B follow-up](#phase-1b-followup-2026-05-19) below for open headroom items.
- **`[WebServer] Low-heap shedding — DIAGNOSED 2026-05-18 (superseded by RESOLVED entry above).** Witchhunt complete: 5-SSA forensic audit + 2nd/3rd external opinions converged. Root cause is **structural internal-SRAM deficit** — application heap is ~28 KB at idle vs the 30 KB shed threshold; system is configured for ~400 KB of demand on 320 KB hardware. **Hex-ID architecture is NOT the cause** (PROGMEM `const char*` correctly pointer-aliased). Highest-leverage move per both external reviewers: PSRAM-allocate `AsyncWebSocketSharedBuffer` (+24-40 KB *retained* internal-DRAM relief at 4-8 clients). Reconciled 3-tier plan + diagnostic firmware on branch `ssa-w4-diagnostic-build` (`fc7ae2bf`) + soak protocol bundled in branch tree. Full context: `~/.claude/projects/<slug>/memory/project_k1_heap_pressure_witchhunt_2026_05_18.md`. Three open Qs gate Phase 1 dispatch: (1) does `uxTaskGetStackHighWaterMark` telemetry exist on K1 V2? (2) what ArduinoJson minor version (7.3+ requires `JsonString(ptr,true)` wrapping)? (3) nm/map output — are `PATTERN_METADATA` + name literals in `.flash.rodata` or `.dram0`? Captain's Phase 3 reframe: gate `ledTransport.*` behind a dev-only `stats.telemetry` topic — was designed as autonomous-agent backend dev-signal AND iOS preview novelty; production status broadcast drops the 12-field nested object. Witchhunt artefacts at `docs/research/heap-pressure-witchhunt-2026-05-18/`.
- **Original heap-shedding symptom note (superseded by the diagnosis above)**: Discovered 2026-05-18 during hardware validation of zone-control-restore on K1 V2. Serial reports `Low-heap shedding active (internal=8300, largest=1524, resume>28672, latched_ms=151399+)` after ~3 minutes of HTTP+WS activity. AsyncTCP needs 4-16 KB contiguous chunks to build responses; `largest_free_block=1524 B` rejects every new request. WebServer self-protects by shedding, but the `resume>28672` threshold is never met because the fragmenter (suspected: status JSON allocation in WebServerBroadcast every 5 s, or AsyncWebSocket queue retention) never releases enough contiguous memory. Symptom matches Captain's historical "webapp stability was absolutely dismal" framing exactly. **Closure**: identify the fragmentation source (heap-tracing under sustained HTTP), refactor the offender to a static buffer or pool-allocator. Until then K1 in AP mode + fresh reboot is the working test pattern.
- **`[SynqMatrix] /synqmatrix/engage owner transient resets to none — RESOLVED 2026-05-19 in current source.** `SynqMatrix::setSuppressed()` now maps Director-mode autonomous suppression (`boot_grace`, `no_audio`, `low_confidence`, etc.) to owner=`Director` instead of `None`, while preserving `Manual`/`Show` claims and keeping disabled/off as owner=`None`. Native regression: `test_synq_matrix_director_suppression_preserves_owner_for_capture`. Hardware validation 2026-05-19: `synqmatrix engage` under live music reported `owner=director` during `enable_grace`, then transitioned to `state=dense`, `confidence=1.00`, `boundary=downbeat_boundary`, `action=palette_shift`, with `show_skips=0`, failures=0, RMT errors=0, and no low-heap shed latch.
- **TRAIL FADE parameter — no firmware backing** — present as an active slider in the historical 2026-02-01 webapp screenshot, dropped from the V1 fallback controller restore on 2026-05-18 per Captain decision. No corresponding parameter exists in `V2Parameters`, REST `/api/v1/parameters`, WS `parameters.changed`, or either protocol contract. Decision: defer to the VP team's deep-dive review of the 23 Tier-2 effects (Captain framing 2026-05-18) — handle TRAIL FADE semantics alongside that review rather than as a standalone parameter add. Either drop permanently from product surface or restore with concrete effect-modulation semantic.

- **EdgeMixer (`enhancement/EdgeMixer.h`) — confirmed defects from the 2026-07-07 adversarial audit** (23-agent swarm; full plan `firmware-v3/docs/research/edgemixer_k1_port_plan_2026-07-08.md`). All source-side (LightwaveOS), independent of the K1 port. (1) **Centre-origin breach** — `kCentreGradient` LUT zeroes at index 76, not the physical centre 79.5, and is edge-asymmetric (251 low edge vs 255 high); manifests only when the non-default `CENTRE_GRADIENT` spatial mode is selected. Fix: regenerate `round(255*|i-79.5|/79.5)` or adopt an analytic mask. Firmware behaviour → hardware-test-before-commit. (2) **Serial-JSON mode-8 regression** — `SerialJsonGateway.cpp:771` bounds mode to `STM_DUAL` (0-7), rejecting `stm_spectral_map` (mode 8) which WS/REST/NVS all accept; a re-run of the #47224 stale-bound bug on a sibling surface. Fix: bound to `STM_SPECTRAL_MAP` + centralise the mode-max constant. (3) **Doc-accuracy** — the "luminance-preserving" claim (`EdgeMixer.h:20/480`) is false for the hue-rotation modes (TRIADIC ~2x brightness on red→green), and the "~22µs/160px" figure (`:22`) is an unvalidated estimate copied from a refactor prompt but stated as fact in shipping source. (4) **Minor** — `saveToNVS` does synchronous flash I/O on the render core; `m_spreadHue` and the post-boot `m_matrixDirty` safety-net branch are dead. **Refuted by the audit (do NOT act on): no cross-core data race (actor queue serialises config onto Core 1); no hidden divide cost (the compiler strength-reduces `/255`).**

<a id="phase-1b-followup-2026-05-19"></a>
- **Phase 1B follow-up — open items from the heap-pressure witchhunt closure (2026-05-19)** — Phase 1A (tag `phase-1a-survival-fix`, commit `3770863e`) shipped the survival fix; the items below are post-survival headroom + procedural follow-ups, none blocking K1 FE launch. Closure: bundle into Phase 1B only after an abusive multi-client soak surfaces the next failure mode — do not pre-engineer against guessed-at threats.

  1. **Phase 1B candidates (predicted region-1 deltas; ranked by leverage at the time of writing):**
     - **Step 1.5 — AsyncWebSocketSharedBuffer PSRAM relocation.** Highest leverage at multi-client load. Each connected WS client retains a `shared_ptr<vector<uint8_t>>` per broadcast frame until ACK; one client connect cost ~5.5 KB of persistent region-1 occupancy in the Branch (3) soak. PSRAM-relocate the per-client backing → predicted +24–40 KB region-1 retained relief at 4–8 clients. HIGH effort — touches ESPAsyncWebServer internals or wraps `makeSharedBuffer`. Verify pbuf DMA path keeps internal-DRAM eventually.
     - **Step 1.2 — `EXT_RAM_BSS_ATTR` on effect statics + `s_spectralMelBands` + `kParameters` tables.** Predicted ~+13.6 KB raw relocated out of region 1 (per `nm` inventory in `project_k1_heap_pressure_witchhunt_2026_05_18.md`); actual ΔregionLargest depends on consolidation. LOW effort. Sandbox SSA-C predicted ISR-safety risk is low for the named candidates (audio path Core 0 only, render path cache-friendly access pattern).
     - **Step 1.4 — Renderer/Audio stack trim.** Each 16 KB. Trim only after measuring hwm under realistic load with the new `dbg stack <name>` helper (already shipped in `phase-1a-survival-fix`). Predicted +10–16 KB combined if hwm allows.
     - **Step 1.6 — PSRAM `JsonDocument` allocator on network/codec paths.** ArduinoJson 7.0.4 (confirmed pre-7.3, zero-copy `const char*` holds) — no `JsonString(ptr, true)` wrapping required at current dependency. Trip-wire for future ArduinoJson bumps: verify before any 7.3+ upgrade. Predicted churn relocation, not survival.

  2. **Abusive multi-client soak protocol (3-bullet sketch — full design before next dispatch):**
     - 4 concurrent WS clients (Tab5 + iOS + 2 Python `websockets` clients from a co-network Mac), sustained 1 h, status+zones subscribed, ~1/s effect cycling + parameter dithering.
     - Capture cadence: `region1.{free,largest,min_free}` at 1 Hz scalar via the shipped `[HEAP-FORENSICS]` plane; `dbg stack async_tcp` injection every 5 min; `vp stack` every 10 min for renderer P99; per-topic broadcast byte rate via existing `WsGateway::Stats`.
     - Pass condition: identify the FIRST metric that crosses a meaningful threshold (region-1 largest collapse, shed latch, AsyncTCP hwm < 1.5 KB margin, P99 frame > existing baseline + 1 ms). That metric names the Phase 1B target. Do NOT pre-engineer; let the data choose.
     - **Observed 2026-05-19:** rapid STA REST zone churn (all six per-zone setters across Zones 1-3, repeated every ~3 s) triggered a transient WebServer low-heap shed after ~70 s. Evidence: `shed.enable` at uptime 1377 s, `region1.free=3408`, `region1.largest=2036`, `region1.min_free=1764`; shed released after ~801 ms. REST route semantics stayed correct, but this names the next Phase 1B stress target as network JSON/AsyncTCP churn under repeated zone writes.

  3. **Pre-existing render/audio P0 pathology — note for the record.** `render_frame_work_us` p99 ≈ 9.2 ms, `audio_hop_us` p99 ≈ 17.9 ms, `audio_snapshot_age_us` p99 ≈ 21 ms — all measured on K1 V2 at 2026-05-19 against effect 0x2102. These predate Branch (3) (pre-trim values were 10.4 / 18.1 / 23.4 ms — Branch (3) improved render by 11.5 % and snapshot age by 8.2 %; the trim did NOT cause this pathology). Documented as `pre_show_yield` 10 ms dominance in earlier WB-1 / WB-2 entries (above). Not blocking FE launch. Phase 2+ work. Do NOT investigate this in the same session as Phase 1B headroom work — distinct surface, distinct evidence.

  4. **Tab5 connection retry flakiness — open observation.** Across multiple K1 V2 soaks in this session, Tab5/iOS clients sometimes auto-reconnect to K1's SoftAP within 30 s of K1 reboot, sometimes only at minute 2–3, sometimes not at all in a 10-min window. No clear pattern. Could be Tab5-side WiFi retry backoff, K1-side AP advertisement timing, or some combination. Not blocking the FE single-client survival path. Investigation surface: Tab5 firmware's `WiFiClient` reconnect policy + K1's `WiFiManager` AP-start sequencing. Defer.

  5. **AsyncTCP default-stack community contribution opportunity.** The vendored AsyncTCP defaults `CONFIG_ASYNC_TCP_STACK_SIZE = 8192 * 2 = 16,384 B` for the `_async_service_task`. K1 V2's measured peak hwm under realistic 1-client connect load is 5,260 B — 32 % of the default. The library over-provisions ~10 KB per ESP32 user with a tight heap. Worth a writeup (GitHub issue on `me-no-dev/AsyncTCP` or successor fork, plus a SpectraSynq engineering note) explaining the measurement methodology + safe trim formula (peak × 1.30, 256-aligned). Improves the ecosystem; positions SpectraSynq as having earned this finding through forensic discipline. Non-urgent; queue for a quiet week.

  6. **Operating-contract addition — diagnostic-toolbox audit pre-flight.** This session's load-bearing finding was that `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` returns the CROSS-REGION max (dominated by RTC_FAST on K1 V2), not the main-heap-region-1 largest free block. The W3 analysis used the wrong number for weeks. The procedural lesson: **before trusting any diagnostic scalar, audit which region(s) the underlying capability mask actually spans on the specific MCU/IDF version**. Encode this rule in `MEMORY.md` (project memory) and the global CLAUDE.md operating-contract section: every new heap/timing diagnostic surfaces its underlying API contract (which region, which cap mask, which TTL) before any tactical output cites its value. Already partially captured by Captain's "do not invent" rule in this session — formalise as a permanent operating contract.

  **Closure rule for this entry:** when the abusive multi-client soak completes, replace this entry with a Phase 1B planning entry naming the actually-observed next failure mode. Do not advance Phase 1B sub-steps before the soak runs.

### WB-1 — Systemic naming, definition, and metric accountability audit (PARTIAL — FIRST-PASS INVENTORY + LOW-RISK CORRECTIONS LANDED)

- **Problem statement:** Recent visual-pipeline work exposed misleading names and descriptions around renderer metrics and timing surfaces. Examples include `frameDrops` reading as skipped output frames when it is currently deadline-miss accounting, `cpu=100%` reading as whole-device CPU utilisation when it is renderer frame-budget occupancy, and `show_leds` reading as FastLED-only timing when it included output preparation plus LED driver show.
- **Trigger / evidence:** K1v2 waveform/hybrid characterisation and VP Stack timing split, especially commits `6ec1d0b6` and `7c867df4`; docs: `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md`, `firmware-v3/docs/debugging/VP_STACK_INTROSPECTION_COMMAND_SPEC.md`.
- **Scope:** Audit high-risk metric names, debug labels, protocol fields, docs descriptions, and agent-facing terminology where the name can lead to wrong tactical conclusions. For each item, record actual definition, likely misread, operational risk, source anchor, and proposed action: rename, split, document, deprecate, or leave as-is with justification.
- **Non-goals:** Do not rename broad surfaces blindly. Do not break client contracts without migration. Do not pause the current effects-characterisation lane after this Work Block is logged.
- **Success conditions:** A source-anchored inventory exists; every proposed change has compatibility impact noted; every new or revised metric/debug field defines numerator, denominator, timing window, inclusion/exclusion boundary, and owner; approved corrections are reflected in docs and code where appropriate.
- **Progress 2026-05-16:** First-pass inventory committed at `firmware-v3/docs/research/wb1_metric_accountability_inventory_2026-05-16.md` (`829e8767`). Low-risk follow-ups landed: serial `Drops:` relabelled to `OverBudget:` with percentage (`3b86928f`), VP metric-definition/validation interpretation tables added (`cc18e711`), and REST/WS `cpuPercent` / `cpuLoadPercent` ownership descriptions added without wire renames (`36a2c4d8`). WB-1 remains open for repo-wide terminology audit and any migration-sensitive field rename plan.
- **Failure conditions:** Loose prose without source anchors; renames without migration plan; treating one corrected metric as proof that the wider naming/definition problem is solved.
- **Owner / pickup mode:** Separate governance/observability agent or team. Start from the cited docs and current renderer/serial status surfaces.
- **Resume rule for original mission:** Effects visual-quality work continues after this entry is logged. Do not turn waveform/PVF/BPS tuning into a repo-wide terminology audit in the same session.

### WB-2 — FastLED/RMT transport visibility and ownership study (PARTIAL — SOURCE MAP LANDED; HARDWARE INSTRUMENTATION STILL OPEN)

- **Problem statement:** FastLED is not currently proven broken. The problem is insufficient project visibility into the FastLED overlay, RMT driver behaviour, wire-time fencing, return semantics, and low-level LED transport configuration now that those details affect K1 visual-pipeline timing interpretation.
- **Trigger / evidence:** The VP Stack timing split showed the need to separate output preparation from LED driver show timing. Source anchors include `firmware-v3/src/hal/esp32s3/LedDriver_S3.cpp` and `firmware-v3/src/hal/esp32s3/LedDriver_S3.h`; docs and commits: `6ec1d0b6`, `7c867df4`, `firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md`, `firmware-v3/docs/debugging/VP_STACK_INTROSPECTION_COMMAND_SPEC.md`.
- **Scope:** Map the active LED transport from `LedDriver_S3` through the vendored FastLED RMT4 overlay into ESP-IDF RMT calls. Establish what blocks, what returns early, what the fixed wire fence covers, how dual 160-LED strip timing behaves, and what instrumentation would prove TX start/TX complete/reset-latch boundaries on K1v1 and K1v2.
- **Non-goals:** Do not declare FastLED broken without evidence. Do not remove or weaken the FastLED/RMT fence as part of this study. Do not rewrite LED transport inside the current effects-characterisation lane.
- **Success conditions:** A source-anchored transport map exists; timing diagram distinguishes CPU preparation, `FastLED.show()`, RMT TX, fixed fence, and latch/reset windows; hardware instrumentation plan or evidence is recorded; decision matrix compares keep-upstream, vendor-fork, project-owned transport wrapper, and full in-house LED driver options.
- **Progress 2026-05-16:** Source-anchored transport map and decision matrix committed at `firmware-v3/docs/research/wb2_fastled_rmt_transport_map_2026-05-16.md` (`f586b297`). WB-2 remains open for hardware instrumentation that proves physical TX-start/TX-complete/reset-latch boundaries on K1 hardware.
- **Failure conditions:** Library-blame without proof; generic rewrite proposal without test harness and safety gates; changing LED output behaviour before the study has hardware evidence.
- **Owner / pickup mode:** Separate low-level firmware/transport agent or team. Treat this as an investigation first, not an implementation pass.
- **Resume rule for original mission:** Effects visual-quality work continues after this entry is logged. FastLED/RMT ownership is not the next PVF/BPS/waveform tuning task unless Captain explicitly reopens it.

### WB-3 — K1 spectral-temporal-modulation (STM) producer for EdgeMixer audio-reactive modes (BLOCKER — scoped 2026-07-08, not started)

- **Problem statement:** EdgeMixer modes 7-8 (STM_DUAL, STM_SPECTRAL_MAP) consume `stmSpectral[42]`, `stmTemporalEnergy`, `stmSpectralEnergy`, `stmReady` — a spectral-temporal-modulation analysis product present in LightwaveOS `ControlBus.h:140-145` but with NO analogue anywhere in K1 (SensoryBridge) audio. Whole-repo grep of `/Users/spectrasynq/SpectraSynq_K1_Firmware` for those tokens = zero hits; `sb_semantic_state.h:47-91` produces only tempo/onset/kick-snare-hihat/chord and `sb_audio_snapshot.h:58-111` only vu/spectral-energy/spectrum/chroma. Until K1 grows this producer the two STM modes cannot be ported; a naive zero-fill drives the secondary strip to black AND breaches K1's `sb_semantic_state` "absent — do not fabricate 0" doctrine.
- **Trigger / evidence:** 2026-07-07 EdgeMixer adversarial audit, finding `stm-signals-absent` (HIGH, CONFIRMED). Full context: `firmware-v3/docs/research/edgemixer_k1_port_plan_2026-07-08.md`.
- **Scope:** Design and build (or decide against) a per-bin spectral-modulation-history + temporal-modulation-energy stage in K1 audio reproducing the LightwaveOS STM semantic. Establish bin count (source: 42 from a 128-band FFT), modulation-rate mapping, `stmReady` gating, and the SQ15x16 output contract.
- **Non-goals:** Do NOT block the Tier-1 colour-core port on this. Do NOT zero-fill absent signals. Do NOT port modes 7-8 until this lands and is hardware-validated (STM was never validated even on the source — validation covered only colour modes 0-4).
- **Success conditions:** K1 audio exposes an STM analogue with a documented contract; a hardware A/B shows STM_DUAL/STM_SPECTRAL_MAP behave equivalently to source; the compile flag gating modes 7-8 is removed only after that proof.
- **Owner / pickup mode:** Separate audio-DSP agent/team. Treat as a design investigation first (is STM worth building for K1, or should modes 7-8 be dropped from the K1 product surface?), not an implementation pass.
- **Resume rule:** The EdgeMixer colour-core port (Tier 1) proceeds independently and does not wait on this Work Block.

---

## Critical — Upstream Calibration Debt

Per the RBDO Gate (`CLAUDE.md` top), these upstream facts are unresolved. Until each is resolved or explicitly accepted under DEGRADED-MODE with disclosed risk, every tactical output that depends on them must be labelled DEGRADED-MODE or REFUSED. New tactical outputs MUST NOT add a fourth dependent to any URGENT row without resolving it first.

### C-1 — Microphone-domain operating envelope (HIGH — MEASURED-DEGRADED 2026-05-06)
What mic-domain RMS / peak / silentScale-trip range was the firmware tuned against?
- **Blocks:** No longer blocks current K1v2 firmware-domain tuning, AFS v2 silentScale validation, or tuned-regime sign-off work that uses the same ESV11 32 kHz profile and Captain-approved private playback chain. Still blocks SPL/LUFS, cross-room, K1v1/K1v2 parity, and production-acoustic claims.
- **Affected outputs:** Firmware-domain outputs can cite the measured-degraded envelope; absolute acoustic outputs remain DEGRADED-MODE.
- **Priority:** HIGH follow-up debt, not an URGENT hard stop for current K1v2 firmware-domain work.
- **Audit status:** 2026-05-06 audit of `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md` completed in `firmware-v3/docs/research/c1_mic_domain_envelope_audit_2026-05-06.md`; it narrowed the raw-hop RMS scale but did not close C-1 by itself.
- **Hardware evidence:** 2026-05-06 K1v2 capture completed in `firmware-v3/docs/research/c1_mic_domain_envelope_capture_2026-05-06.md`. Current measured raw-hop RMS envelope: idle p50/p95/p99 `0.001591/0.003477/0.005916`; quiet p50/p95 `0.005513/0.015687`; normal p50/p95 `0.017600/0.036282`; dense p50/p95 `0.026247/0.044652`; observed max `0.064463`. Stop recovery after a 20.0 s hard stop: `isSilent=true` at `0.266399 s`, `silentScale<0.2` at `1.128722 s`.
- **Captain approval:** 2026-05-06 hardware envelope pass completed with Captain-provided private tracks. Clip paths and audio material stay out of public repo artefacts per C-3.
- **Remaining debt:** No SPL/LUFS reference level, no calibrated acoustic room/output level, no K1v1 parity pass, and no production photometry tie-off.
- **Revisit trigger:** Microphone placement, enclosure acoustics, sample rate, silence-gate constants, playback chain, source corpus, or target hardware revision changes; or any request to claim SPL/LUFS/cross-device production acoustic validity.

### C-2 — Feature × effect × dwell coverage matrix (HIGH — DONE-DEGRADED 2026-05-06)
Which AFS v2 features × which Phase 5 effects × what minimum dwell each phenomenon needs to manifest visually.
- **Blocks:** no longer blocks C-5 authoring or sign-off harness planning. Still blocks final Phase 5 ship-gate claims until the C-5 hardware run validates the timestamped observables visually.
- **Priority:** HIGH follow-up debt, not an authoring hard stop.
- **Evidence:** `firmware-v3/docs/research/c2_feature_effect_dwell_matrix_2026-05-06.md` maps Phase 5 effects `0x2100`/`0x2101`/`0x2102` to source-backed audio feature rows, fixture archetypes, and minimum dwell lower bounds.
- **Remaining debt:** dwell minima are source-derived and DEGRADED-MODE until the C-5 hardware run validates them against the timestamped observable matrix.
- **Revisit trigger:** C-5 timestamped observable pass, Phase 5 sign-off authorisation moment, or any new audio-reactive effect requiring fixture validation.

### C-3 — Clip licence status + K1 repo public-status (HIGH)
Are the hybrid-beat-tracker corpus clips licensed for inclusion or path-reference in K1 firmware artefacts? What is the K1 repo's public-status at launch (open-source, public-on-release, private)?
- **Blocks:** clip pool composition for sign-off sweep; calibrated WAV storage policy; any third-party music reference in this repo.
- **Priority:** HIGH (legal exposure if assumed wrong).
- **Captain answer:** 2026-05-06: the repo is already public. Clips are to stay private. Captain can suggest several music tracks when the sign-off corpus is actually needed.
- **Current status:** Repo-public status and clip privacy are resolved. Do not commit clips or public path references to private clips. The exact reference-track list remains deferred until the sign-off corpus step.
- **Revisit trigger:** Captain answers (a) repo public-status at launch, (b) hybrid-beat-tracker licence applicability for commercial-product testing, (c) presence/absence of a Captain-licensed audiophile reference library.

### C-4 — First sign-off purpose (MEDIUM — DECIDED)
**Decision (current):** First Phase 5 sign-off is a **diagnostic baseline**, not a ship gate, not a regression detector.
- **Reason:** the first attestation was accepted before per-effect timestamped observables existed. C-1 is now measured-degraded for current K1v2 firmware-domain work, C-2 is done-degraded, and C-5 now supplies a ratifiable observable matrix, but no hardware visual run has executed against that matrix yet.
- **Priority:** MEDIUM (decided; pending hardware re-audit against C-5).
- **Revisit trigger:** When the C-5 observable matrix is run on hardware, the next sign-off cycle can be promoted to ship-gate (cycle 2) or regression-detector (cycle 3+), subject to C-3 corpus composition.

### C-5 — Per-effect timestamped observables (MEDIUM — DONE-DEGRADED 2026-05-06)
What exactly does the operator look for, anchored to (clip, timestamp, measurable phenomenon), per Phase 5 effect (RTS / PVF / BPS)?
- **Blocks:** no longer blocks final rubric authoring or sign-off harness planning. Still blocks ship-gate promotion until the hardware run records Captain visual answers for each row.
- **Depends on:** C-2 matrix landed; private corpus labels/windows selected from Captain-authorised local material. Exact source media mapping remains outside the public repo per C-3.
- **Evidence:** `firmware-v3/docs/research/c5_phase5_timestamped_observables_2026-05-06.md` binds RTS/PVF/BPS to redacted private labels, timestamp windows, trace counters, expected visual phenomena, and Captain visual questions.
- **Hardware sweep:** `firmware-v3/docs/research/c5_phase5_hardware_sweep_2026-05-06.md` records the first K1v2 serial execution. 18/18 rows produced trace evidence; all effect-specific render p99 values were under 2 ms; RMT wire time stayed around 6.1-6.3 ms. Captain gave one explicit visual judgement: `RTS-4` was an extremely poor effect/fixture choice.
- **Remaining debt:** clip/window adequacy and visual quality remain DEGRADED-MODE until row-level Captain PASS / FAIL / DEGRADED-PASS answers are captured. `RTS-4` specifically requires replacement, redesign, or explicit removal from the sign-off matrix.
- **Priority:** MEDIUM follow-up debt, not an authoring hard stop.
- **Revisit trigger:** Replacement/rerun of `RTS-4`, full row-level Captain PASS / FAIL capture, private corpus replacement, changed audio backend/sample-rate profile, or changed Phase 5 effect implementation.

### C-7 — K1 LGP perceptual JND floor (HIGH — MEASURED-DEGRADED 2026-05-05)
What is the minimum perceptible brightness/contrast change through K1's actual LGP at customer viewing distance and normal viewing conditions?
- **Blocks:** Phase 1 Move 1.7 PerceptualJND constants; INF-02 FramebufferLPF minimum cutoff bounds; PER-X minimum tau bounds; any claim that subtle motion/flicker thresholds are calibrated rather than inherited from ES/SB intuition.
- **Measurement:** Captain observed the fixed `test_brightness_floor` harness on K1 hardware after flashing `test_brightness_floor` to MAC `b4:3a:45:a5:87:f8` over `/dev/cu.usbmodem2101`; LEDs were only visible from test level 4 onward in both Phase 1 and Phase 2. Test level 4 is `8.0%` perceptual in `firmware-v3/test/test_brightness_floor/main.cpp`.
- **Accepted degraded constant:** use `8.0% perceptual` as the current minimum visible LGP brightness floor for Move 1.7 bounds until photometer data supersedes it.
- **Remaining debt:** viewing distance, ambient conditions, observer count, and photometer readings were not captured; this is good enough to unblock placeholder-free constants, not good enough for final production photometry claims.
- **Priority:** MEDIUM follow-up debt after Move 1.7; production photometry still owed, but placeholder-free constants are now unblocked.
- **Revisit trigger:** Photometer-backed K1 + LGP measurement campaign at customer viewing distance, or Captain reports a different visible threshold under normal customer ambient conditions.

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

### F-5 — K1 dual-mode WiFi delivery (HIGH — DECIDED + SCOPED 2026-05-04)
Captain has authorised reversing the "AP-only-EVER, STA never worked" doctrine after forensic excavation (`docs/tooling/notebooklm-bundles/lightwave_ledstrip/_FORENSIC_WIFI_REPORT.md`) established the doctrine was an over-correction from a single concurrent-AP+STA failure event in Era 5 (2026-02-05 → 2026-02-16). STA mode actually worked in Era 1 (2025-06) and Era 3 (2025-12). Goal-state: dual-mode (AP OR STA, never together — concurrent AP+STA hits a known ESP-IDF 802.11 driver bug).
- **Decision (current):** Deliver dual-mode K1 WiFi. Current shipping remains AP-only via `WIFI_AP_ONLY` build flag; the `_sta_validation` build env (`esp32dev_audio_esv11_k1v2_32khz_sta_validation`, restored on current main by commit `0987ba67`) is the entry point for re-validation work.
- **Engineering scope (4 tasks, in order):**
  1. **DONE 2026-05-16 — Replace `WIFI_MODE_APSTA` paths with AP OR STA semantics.** Commit `0987ba67` restored `esp32dev_audio_esv11_k1v2_32khz_sta_validation`, removed the active AP+STA connection/fallback paths from `WiFiManager`, kept production `esp32dev_audio_esv11_k1v2_32khz` AP-only, and recorded evidence at `firmware-v3/docs/research/f5_sta_validation_task1_2026-05-16.md`.
  2. **DONE 2026-05-16 — Add NVS mode-preference field.** This pass adds `mode` to the `wifi_creds` namespace, keeps production `WIFI_AP_ONLY` builds AP-forced, honours stored AP-or-STA preference only in the `_sta_validation` build, and records K1v2 hardware evidence at `firmware-v3/docs/research/f5_mode_preference_task2_2026-05-16.md`.
  3. **PARTIAL 2026-05-16 — Add first-boot provisioning UI.** Source/build/native/USB-serial/LAN-STA-refusal evidence exists at `firmware-v3/docs/research/f5_provisioning_task3_2026-05-16.md`: validation-build captive portal source, `/api/v1/network/provision`, STA-mode 503 refusal over existing LAN, production and `_sta_validation` builds, and upload to K1v2 MAC `b4:3a:45:a5:87:f8`. Remaining before full closure: AP captive portal root page, captive DNS wildcard, browser redirect, and AP-mode provision success path from a separate client/device because this Mac must not join `LightwaveOS-AP`.
  4. **PARTIAL 2026-05-16 — Hardware-test pure-STA validation build.** Commit `0987ba67` flashed `esp32dev_audio_esv11_k1v2_32khz_sta_validation` to K1v2 MAC `b4:3a:45:a5:87:f8`, connected to one router in pure STA mode, completed 30/30 one-minute HTTP 200 samples over `lightwaveos.local` with `apMode=false`, and confirmed IPv4 mDNS via `dscacheutil`, `ping`, and `curl -4`. Remaining validation before closing task 4: multiple-router coverage.
- **Reason:** The original requirement was dual-mode (Captain quote 2026-05-04: "the K1 + tab5.encoder are taken out into 'the wild' — there are scenarios that actually do not have wifi readily available. Therefore, the ability to connect via AP MUST be possible, but that got lost in translation to mean 'AP-only-ever', which in reality is actually not ideal as well. The system SHOULD and MUST be capable of functioning (without issues) in BOTH AP and STA mode."). The doctrine-driven over-correction blocked the goal-state for 76 days; reverting it and delivering dual-mode unblocks offline-AP, online-STA, OTA-from-router, and time-sync use cases.
- **Affected outputs:** ≥ 9 source-doctrine surfaces already corrected in this curation pass (CLAUDE.md, TOOLCHAIN_IMPLEMENTATION_GUIDE, tab5 PRODUCT_DECISION_PRINCIPLES, k1-rest-contract.yaml, CHANGELOG.md, 2 memory files, MEMORY.md index, ROADMAP Section 9). Future iOS / Tab5 client work needs to consume the `mode` preference + handle mode transitions gracefully.
- **Priority:** HIGH (decided; engineering execution pending hardware-in-the-loop session).
- **Revisit trigger:** Multiple-router pure-STA validation, F-5 Task 3 provisioning UI implementation, or an ESP-IDF 5.x upgrade that resolves the concurrent AP+STA bug at the driver level (current K1 is pinned at IDF 4.4.7; upgrade is blocked by an I2C bug + API rewrites per `firmware_build_envs.md`).

### [DONE] ~~F-6 — CONTROLBUS_NUM_ZONES violates 3-zone hard rule~~ — RESOLVED 2026-05-19 (hardware-attested)
`firmware-v3/src/audio/contracts/ControlBus.h:22` now defines `static constexpr uint8_t CONTROLBUS_NUM_ZONES = 3;` with explicit coverage-checked partition tables at `:29-39` (Option C of decision brief, commit `08a7c997` 2026-05-17). DEGRADED-MODE caveat shipped with that commit has been discharged 2026-05-19 via P2 / D-revised hardware-truth attestation per Captain directive.
- **Task 1 (audit-only): DONE.** Original 4-zone partition + consumers documented in `firmware-v3/docs/research/f6_controlbus_num_zones_audit_2026-05-06.md`.
- **Task 2 (decide band-restructuring approach): DONE 2026-05-17.** Option C (explicit three-bucket coverage-checked tables) selected and implemented in commit `08a7c997`. Decision brief: `firmware-v3/docs/research/f6_controlbus_band_restructuring_decision_brief_2026-05-16.md`. Partition: bands `{0,1}/{2,3,4}/{5,6,7}`, chroma `{0-3}/{4-7}/{8-11}`. Compile-time `static_assert` at `ControlBus.h:56-63` enforces contiguous coverage.
- **Task 3 (refactor consumers): DONE 2026-05-19.** Consumer audit via SSA enumeration confirmed zero hardcoded `[3]` zone indices in production paths; all consumers use `CONTROLBUS_NUM_ZONES` loop bounds or the zone-range tables. Evidence: `firmware-v3/docs/research/f6_validation_2026-05-19/CONSUMER_TABLE.md`. Finding: ZERO FE-launch-relevant ESV11-active Zone AGC consumers (REST/WS endpoints intentionally disabled on ESV11 by `#if FEATURE_AUDIO_BACKEND_ESV11` → `FEATURE_DISABLED`); ZERO effect-path consumers (effects consume only `m_frame.bands[]`/`m_frame.chroma[]`, never zone-AGC accessors).
- **Task 4 (hardware-test before commit): DONE 2026-05-19.** D-revised P2 protocol executed: source proof (`SOURCE_PROOF.md`) + bench A/B hardware capture (`BENCH_A_B.md`) on canonical `esp32dev_audio_esv11_k1v2_32khz` build at commit `3e0e40d8`. Captain played `James_Brown_-_Papa_s_Got_A_Brand_New_Bag.wav` from the approved corpus through an external speaker into K1v2's SPH0645 microphone range; agent toggled `audio.zone_agc` + `audio.chroma_zone_agc` OFF mid-run via the existing `BENCH TOGGLE` serial CLI. 8 `adbg spectrum` snapshots over 80 s captured. Per-band ON vs OFF differential 58.9–85.1%; per-zone aggregate 68.1% / 77.9% / 70.7% across zone 0/1/2; mean OFF/ON energy ratio 3.82×. Partition empirically active on production K1v2 ESV11. Validation rollup at `firmware-v3/docs/research/f6_validation_2026-05-19/SUMMARY.md`.
- **Closure note:** REST `GET /api/v1/audio/zone-agc` and WS `audio.zone-agc.get` remain intentionally disabled on ESV11 builds — this is product design, not technical debt. F-6 FE blast radius routes via `m_frame.bands[]` only (the post-partition smoothed output that every audio-reactive effect renders from). The 4→3 partition materially affects rendered behaviour, attested by the 3.82× energy ratio above.
- **NotebookLM infographic audio-AGC restoration:** unblocked but post-FE-launch. Action when unblocked: unstrike audio-AGC panel in `docs/tooling/notebooklm-bundles/lightwave_ledstrip_infographics/sources/06_PART_OUTLINES.md` with canonical 3-zone semantics (Zone 0 bass 20–250 Hz / chroma 0–3; Zone 1 mid 250 Hz–2 kHz / chroma 4–7; Zone 2 treble 2–20 kHz / chroma 8–11).
- **Revisit trigger:** none. F-6 is closed without DEGRADED-MODE caveat.

### F-6.1 — Zone AGC output intensity acceptance (QUALITY GATE — opened 2026-05-19)
F-6 closure proved the 3-zone partition is exercised and materially affects `m_frame.bands[]` (steady-state 45-80% per-band, 2.88× total energy OFF/ON). F-6.1 asks the downstream question: **are the post-AGC normalised bands/chroma visually acceptable for FE launch, or is the AGC over-compressing the bands effects render from?** This is a calibration question, not a correctness question.
- **Source of risk:** Effects consume `m_frame.bands[]` and `m_frame.chroma[]` (post-partition smoothed outputs), not raw Zone-AGC internals. If Zone AGC compresses too aggressively, effects may receive lower band values than their visual thresholds / gain curves expect, producing dimmer visuals, weaker beat punch, fewer activations, flatter motion, reduced bass impact — "technically stable but emotionally dead" output. Product-critical, not correctness-critical.
- **Scope:**
  - Do NOT modify partition logic (closed in F-6).
  - Do NOT lift REST/WS ESV11 guards (Captain's hard stop).
  - Do NOT touch effects unless acceptance proves visual under-drive.
  - Do NOT add broad observability surfaces.
- **Protocol:**
  1. Canonical K1v2 ESV11 `_32khz` build.
  2. Replay the **same fixed 30 s audio segment** for each run (NOT different windows of continuous playback — fixes the cross-window content-shift confound of F-6 BENCH_A_B.md).
  3. Run gate ON and gate OFF as **separate repeated captures** (not coupled in one log).
  4. Exclude first 8–10 s after gate transition from ratio calculations (follower convergence transient).
  5. Capture `adbg spectrum` bands[]; chroma output if available; video of representative FE effects.
  6. Test 3–4 representative tracks: bass-heavy; vocal/mid-heavy; treble/transient-heavy; normal full-mix reference.
  7. **One-page result** with median + P95 band sum ON/OFF, obvious clipping/saturation OFF, obvious dimness/deadness ON, visual verdict PASS / TUNE NEEDED.
- **Acceptance:** PASS if gate ON preserves visible punch while avoiding raw-band domination/saturation. TUNE NEEDED if gate ON is visibly under-driven or activation density drops materially.
- **If TUNE NEEDED — preferred fix order (do NOT default to partition retune):**
  - All effects too dim → post-AGC visual gain scalar / transfer curve
  - Beat pulses too weak → effect threshold / envelope sensitivity
  - Bass no longer drives enough → per-zone output weighting, NOT reverting zones
  - Chroma dull but bands fine → chroma gain curve only
  - Only one effect bad → effect-local calibration
- **Anti-pattern:** "3.82× feels like too much, so weaken Zone AGC globally." This may reintroduce the original bass-dominance problem the partition was built to solve.
- **Priority:** quality gate before FE launch (~3 weeks). Single-session work.
- **Revisit trigger:** any FE-launch visual review flags Zone-1 dimness, beat under-drive, or post-AGC saturation; OR Captain authorises the calibration session directly.

---

## Performance

### [DONE] ~~K1v2 SRAM/PSRAM reclaim pass~~ — completed 2026-05-06
- **Authority:** `firmware-v3/docs/research/k1v2_sram_psram_reclaim_handoff_2026-05-06.md`.
- **Trigger:** K1v2 Phase 5 testing exposed real low-heap pressure: WebServer low-heap shedding latched around 8.9-10.5 KB internal free heap and `RendererActor::handleSetEffect()` rejected effect switches below its 12 KB floor.
- **Current evidence:** commit `63a4b392` disabled production diagnostic monitors for K1v2 and restored ~6.9 KB static RAM. K1v2 `/dev/cu.usbmodem2101` / MAC `b4:3a:45:a5:87:f8` then booted with `17776` B free internal heap, `15848` B min free, `8180` B max alloc, `showSkips=0`, and accepted `0x2103` plus `0x0100` effect switches.
- **Goal:** recover enough additional internal DRAM/SRAM to keep K1v2 out of low-heap shedding during normal AP/effect-switch testing, preferably `>=22 KB` no-client boot free internal heap and at least `>=8 KB` largest alloc/free block.
- **First candidates:** cold/control-path SRAM consumers only: `CaptureStreamer` fallback/task buffers, `StaticAssetRoutes` 3 KB static buffer, `WsCommandRouter` handler table, and builtin effect registry metadata. Measure from the current ELF before patching.
- **Completed evidence:** `firmware-v3/docs/research/k1v2_sram_psram_reclaim_run_2026-05-06.md` records the Batch A patch, baseline/post-patch build deltas, symbol guard, K1v2 upload, and serial verification. Static internal RAM dropped from `136084` B to `125748` B. K1v2 booted with `28088` B free internal heap, `26160` B min free, `18420` B max alloc, `showSkips=0`, and accepted `0x2103`, `0x0100`, and `0x2102`; post-switch memory remained `27940` B free with `18420` B max alloc.
- **Hard stops honoured:** heap guard thresholds unchanged; `SnapshotBuffer<ControlBusFrame>` not moved; no render hot-path heap added; STA untouched; failed two-unit manual A/B not revived; unrelated dirty files not staged.
- **Return path:** after this pass is verified and committed, resume Phase 5 visual-quality tuning on promising effects (`0x2101`/`0x2102` etc.) with RTS/`0x2100` parked unless Captain explicitly reopens it.

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

Canonical plan is `firmware-v3/docs/research/synergy-topology/Topology_Reconciliation.md` §5, with the resume protocol preserved in `firmware-v3/docs/research/synergy-topology/RESUME_BRIEF_2026-05-05.md`. Do **not** implement against the superseded 6-phase `PASS_3_KILL_ORDER.md` ordering.

Original execution branch `feature/synergy-topology-phase-0-1` was folded into later work via merge `effa781d`. Current resume branch: `feature/synergy-topology-resume-2026-05-05`; keep new resume work isolated unless Captain redirects.

### [DONE] ~~Phase 0 — Baseline Guardrails~~
- Phase 0A kill-list lint shipped — commit b2cc2824
- Move 0.1 Product Signature Filter-as-code accepted from the Phase 0A kill-list lint — commit b2cc2824
- Move 0.2 centre-origin / brand-voice violation triage shipped — commit d4348f08
- Move 0.2 follow-up recovery after sandbox-to-integration loss shipped — commit 6b1a222f
- **Gate resolved 2026-05-05:** Captain accepted the existing Phase 0A + Move 0.2 commits as satisfying `Topology_Reconciliation.md` §5 Move 0.1 Product Signature Filter-as-code and Move 0.2 centre-origin audit pass for this resume branch.

### Phase 1 — Infrastructure substrates — DONE-DEGRADED (7 landed)
- Move 1.1 PersistenceHelpers — commit 6907404c
- Move 1.2 EffectRoleFlags substrate — commit 7a077701
- Move 1.3 FramebufferLPF — commit 00628fe7
- Move 1.4 LayerStack composer — commit d2a7499f
- Move 1.6 sinLUT256 + CFLSubstepGate — commit b62cc5d7
- Move 1.5 ControlBus render-side reuse refactor — shipped in resume-branch Phase Move commit; expands generic AudioEffectMapping sources using existing ControlBusFrame fields only
- Move 1.7 E-05 PerceptualJND calibration constants — shipped in resume branch; `8.0% perceptual` floor captured in `effects/PerceptualJND.h`, INF-02 lower cutoff bound named, isolated native test added; photometer-grade calibration remains C-7 follow-up debt.

### [DONE] ~~Phase 1B — AFS v2 instrumentation + ControlBus contract lock~~
- Phase 1B instrumentation — commit 19007888
- AFS v2 effect API lock — commit c2dc7d26
- AFS v2 contract docs — commit c7bc6d28
- Phase 1B follow-up (Surface 3 hop-timing decomposition + WS gateway accessor) — commit 8b31e9f6

### [DONE] ~~Phase 2 Move 2.1 — PSRAMScalarRing substrate~~ -- commit f8b52bce

### [DONE] ~~Phase 3 — Dual-Strip Moat~~
- Move 3.1 F5 Reflective Twin contract enforcement — shipped in resume branch; `ReflectiveTwinPolicy` gates direct dual-strip output so default/legacy effects stay on the mirrored unified path unless metadata declares `EffectRoleFlags::DUAL_CHANNEL`; native harness matrix includes the scoped policy test.
- Move 3.2 F4 Cross-Strip Wave Interference — shipped in resume branch after Captain LGP fringe-visibility sign-off; default `3pi/4` phase offset matched the strongest visible tooth/trough separation, and K1v2 hardware testing resolved white vertical flashes by enforcing full WS2812 wire-time after patched FastLED RMT `show()`.
- Move 3.3 GEO-13 InterStripPhaseDelay infrastructure — shipped in resume branch; `InterStripPhaseDelay` wraps paired PSRAM frame rings so future `DUAL_CHANNEL` effects can sample delayed strip A/B frames without render-path allocation; native harness matrix includes the scoped substrate test.
- Remaining Phase 3 work: none.

### Phase 4 — Audio substrates + cinematic boot — PARTIAL (3 landed, 1 owed)
- Move 4.1 AUD-21 VoiceMusicClassifier — commit 5021d96a
- Move 4.2 PER-18 AudioGatedConditionalDecay — commit ba816631
- Move 4.4 F6 First-Light Ignition (cinematic boot effect) — commit 4d12edc5 (Captain hardware visual confirmed in commit body)
- Move 4.3 F3 Liquid Stillness curation — planned in `firmware-v3/docs/research/synergy-topology/MOVE_4_3_LIQUID_STILLNESS_CURATION_2026-05-05.md`; Captain noted/approved the gate on 2026-05-06, and implementation remains gated on Captain selecting the final 8–12 ambient programmes from the audition slate.

### Next Synergy-Topology re-entry recommendation — CAPTAIN RATIFIED 2026-05-05
- Phase 0 ledger gate is resolved above.
- Move 1.5 and Move 1.7 are closed on this resume branch; C-7 remains as photometer-grade follow-up debt, not a Phase 1 blocker.
- Move 3.1 is closed on this resume branch.
- Move 3.3 is closed on this resume branch.
- Move 3.2 is closed on this resume branch after Captain hardware sign-off.
- Do not promote Phase 5 visual sign-off to ship-gate until the failed `RTS-4` row is resolved and Captain records row-level visual results for the C-5 matrix; C-1/C-2/C-5 are measured/done under DEGRADED-MODE and C-3 still gates final sign-off corpus composition.

### Phase 5 — Synergy-Topology effect exemplars (3 of 7+ moves) — DONE-DEGRADED
- Move 5.4 RadialTimeScopeEffect (EID 0x2100) — committed in 39406e6b; **DEGRADED-MODE attested 2026-04-28**
- Move 5.6 AttackOnlyPitchVelocityFieldEffect (EID 0x2101) — committed in 39406e6b; **DEGRADED-MODE attested 2026-04-28**
- Move 5.7 BeatParitySpriteEffect (EID 0x2102) — committed in 39406e6b; **DEGRADED-MODE attested 2026-04-28**
- 2026-05-07 BPS lane close-out — immediate silence/background repair is complete for this workstream and Captain confirmed silence is dark/unresponsive; BPS remains an allowed event-sprite class but is visually unsatisfactory and not the future Hybrid/V1 Waveform Pull-In class. See `docs/adr/lightweight-architecture-decision-ledger.md` ADL-009/010 and `firmware-v3/docs/research/lgp_beat_emotiscope_architecture_review_2026-05-06.md`.
- **Native test harness:** 130/130 PASS in 1.97 s — commit f49b4d6a; gated by `pio test -e native_test_phase5` in `firmware-v3_build_check.yml` since 632132e4
- **Hardware traces:** 8 captures committed in `firmware-v3/tools/baselines/` totalling ~21,000 events; `bps_kick_fired` → `bps_sprite_spawn` 1:1 ratio confirmed
- **B.4 DONE-DEGRADED:** Phase 5 sign-off attested under DEGRADED-MODE per Captain authorisation 2026-04-28. Attestation: `firmware-v3/docs/audit/phase_5_visual_sign_off_2026-04-28.md`. Diagnostic-baseline only — does NOT claim hardware visual sign-off, does NOT promote to ship-quality. Cycle 2 sign-off now has a trace-complete C-5 hardware sweep, but remains blocked by the failed `RTS-4` row, missing row-level Captain visual answers, and C-3 corpus composition; C-1/C-2/C-5 are measured/done under DEGRADED-MODE.

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

### SynqMatrix restore-point consolidation (DEFERRED — revisit by 2026-06-30)
- **Problem:** Three independent in-RAM restore-point latches duplicate state across transports:
  - `firmware-v3/src/serial/SerialJsonGateway.cpp` — `g_synqMatrixRestorePoint` + `g_synqMatrixRestorePointValid`
  - `firmware-v3/src/serial/SerialCLI.cpp` — same pair
  - `firmware-v3/src/network/webserver/ws/WsSynqMatrixCommands.cpp` — anonymous-namespace `g_restorePoint` + `g_restorePointValid`
- **Risk:** Each surface captures its own restore point on config-set and uses it for restore commands. Three-way drift if one surface is updated without the others.
- **Decision:** Consolidate into a single shared module under `core/synqmatrix/` (e.g. `SynqMatrixRestorePoint`). Deferred from Chunk 1.C of the SynqMatrix migration (2026-05-13/14) because consolidating mid-rename would widen blast radius from "internal renames" to "internal renames + new public API".
- **Trigger to revisit:** by 2026-06-30, OR when the next SynqMatrix transport surface is added (e.g. iOS REST/WS bindings, Tab5 control surface) — whichever first.
- **Anchor commits:** `0d354ca2` (1.A atomics) / `70d73e0a` (1.B struct fields) / `4cf745fa` (1.C helpers + globals).

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
- DONE: Surface 2 live-code spans (`bus_copy_memcpy`, `bus_copy_retry`, `bus_retry_check`, `audio_ctx_populate_us`) are gated on `FEATURE_TRACE_AUDIO_HANDOFF` and build via `esp32dev_audio_esv11_k1v2_32khz_trace_handoff`.
- NOT IMPLEMENTED: `motion_engine_tick_us` / `motion_shaper_tick_us` have no live update call site in `RendererActor`; activating them would change effect-facing motion semantics, so they remain parked until a Captain-approved product behaviour change exists.
- Surface 3 spans (`i2s_dma_read`, `stm_rfft_256`, `onset_detect_span`, `band_ratio_detect`, `controlbus_publish`) gated on `FEATURE_TRACE_AUDIO_DSP`
- DONE: Surface 3 opt-in DSP decomposition now has `esp32dev_audio_esv11_k1v2_32khz_trace_dsp`. Gated spans cover `i2s_dma_read`, `controlbus_build`, `stm_rfft_256`, `stm_extract`, `onset_detect_span`, legacy `onset_detect`, `band_ratio_detect`, `controlbus_update_stage_b`, `snapshot_publish`, and `controlbus_publish`; Tier 1 counters remain available in trace builds via `audio_hop_us`, `audio_hop_freq`, `audio_silence_scale`, `audio_rms_x1000`, and `audio_hop_count`.
- 2026-05-06 K1v2 handoff trace (`0x2102`, MAC `b4:3a:45:a5:87:f8`): `audio_snapshot_read` p99 687 µs; `bus_copy_memcpy` p99 247 µs; `audio_ctx_populate_us` p99 302 µs; retry only 2/98 reads. Retry contention is not the root cause.
- DONE: Renderer-side copy-count reduction now populates single-effect and independent-strip `EffectContext.audio` directly from the renderer-owned frame instead of first copying through `m_sharedAudioCtx`. The zone path keeps one compatibility context because `ZoneComposer` owns its own reusable context.
- 2026-05-06 K1v2 copy-reduction trace (`0x2102`, MAC `b4:3a:45:a5:87:f8`): `audio_snapshot_read` p99 460 µs; `bus_copy_memcpy` p99 251 µs; `audio_ctx_populate_us` p99 293 µs; `render_frame` p99 2755 µs; retry still 2/99 reads.
- Next gate: only graduate the ControlBusFrame hot/cold split below if sub-300 µs snapshot reads become a hard requirement. Do not tune retry policy unless a later trace shows retry frequency rising.

### ControlBusFrame → internal DRAM relocation (Captain Q3 RESOLVED in spec, implementation shipped)
- Captain-approved 2026-04-27 architectural change: relocate `SnapshotBuffer<ControlBusFrame>` from PSRAM to internal DRAM (5 KB cost approved)
- Expected outcome: 5–10× speedup on `audio_snapshot_read` (current p99 836 µs → target <200 µs)
- The Tier 1 measurement contract (`audio_snapshot_age_us`, `hop_seq_lag`, `size_bytes`, `snapshot_read_retries_total`) is SHIPPED — before/after baseline diffing via `firmware-v3/tools/analyse_trace.py --baseline tools/baselines/k1v2_0x2102_2026-04-27.json --strict` is mechanical
- DONE: 1C verify-first diagnostic is implemented. ActorSystem init now reports actor/snapshot payload memory region (`DRAM`, `PSRAM`, or `OTHER`) and trace counters `audio_actor_storage_region`, `audio_snapshot_storage_region`, `audio_snapshot_payload_bytes`.
- DONE: 1B narrow relocation is implemented. K1v2 hardware verification on `/dev/cu.usbmodem2101` / MAC `b4:3a:45:a5:87:f8` changed the boot diagnostic from `actor=PSRAM payload=PSRAM` to `actor=PSRAM payload=DRAM`; whole-actor 1A allocation was not used.
- DONE: 2026-05-06 post-relocation Tier 1 trace captured in `firmware-v3/docs/research/phase1b_runtime_evidence_2026-05-06/controlbus_dram_relocation_trace/`. `audio_snapshot_read` p99 stayed above the target (`647 µs`), so the gated Tier 2 handoff trace was implemented and captured.
- DONE: 2026-05-06 renderer copy-count reduction improved the same K1v2 handoff trace from `audio_snapshot_read` p99 687 µs to 460 µs and `render_frame` p99 3072 µs to 2755 µs, without changing the cross-core `SnapshotBuffer` safety copy.
- Strategy options surfaced by the SSA-PHASE-A audit (2026-04-27): (1A) override `AudioActor::operator new` to force `MALLOC_CAP_INTERNAL` — lowest risk, ~50–100 KB cost; (1B) convert `m_controlBusBuffer` to a heap-allocated pointer — closer to 5 KB envelope but ~10 KB minimum for double-buffer; (1C) verify-first via `esp_ptr_in_dram` boot diagnostic before committing budget
- Result: DRAM placement plus renderer copy-count reduction reduced but did not close the original `<200 µs` target. The remaining target is frame shape / hot-cold split, not allocation region or retry policy.

### Audio-side bench toggle wiring (Surface 7 follow-up)
- BenchRegistry framework + 8 toggle registrations + `render.color_correction` consumer wiring SHIPPED
- DONE: Audio-side toggles (`audio.lookahead`, `audio.zone_agc`, `audio.chroma_zone_agc`) are wired into the ControlBus `UpdateFromHop` backends through an AudioActor per-hop observer plus scoped native regression coverage.
- ESV11 caveat: production K1v2 builds construct a `ControlBusFrame` through `EsV11Adapter` and bypass ControlBus Stage A (`UpdateFromHop`), so equivalent ESV11 adapter A/B gates require a separate semantic change and are not part of this small Surface 7 follow-up.
- `render.async_rmt` and `render.dual_strip_parallel`: also stubs; require LedDriver disentanglement (not in current scope)
- DONE: `effect.fade_to_black` helper substrate (`effects/FadeOverride.h`) is authored and covered by `native_test_fade_override`; no effect call sites migrated yet, so default product visuals are unchanged.
- Trigger to revisit: when Captain asks for runtime A/B of any specific toggle

### VP render path audit follow-ups (2026-05-05)
- DONE: gamma LUT lifecycle/status correctness (`adacee3d`). NVS and runtime colour-correction config writes now route through `ColorCorrectionEngine::setConfig()`, and REST/WS/SerialCLI/SerialJSON expose `gammaEnabled`, `gammaValue`, `lutGenerationId`, and LUT proof samples.
- DONE: source-grounded VP frame lifecycle audit (`3978c167`). The audit documents one shared output path with a buffer-ownership fork, not two render pipelines.
- DONE: VP validation protocol drafted in `firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md`. Use it before buffer-ownership correction, silence-policy metadata, or colour-correction default changes.
- GATED: buffer-ownership correction. Current source applies `ColorCorrectionEngine::processBuffer()` to `m_leds` in `RendererActor::onTick()`, while direct dual-channel effects can author `m_strip1/m_strip2` and bypass the corrected surface before `showLeds()`. Patching this changes visible output for strip-authored effects, so do not implement until the protocol is followed and the run report exists.
- GATED: silence-policy metadata. Global `silentScale` is an output brightness policy and can make ambient/non-reactive effects appear audio-reactive. Add per-effect policy metadata only behind tests, protocol evidence, and explicit product approval; default changes are visible behaviour.
- SKIPPED: subjective two-unit/manual colour A/B. Do not revive the failed timed A/B workflow or use its observations as evidence. Any future visual-default change needs a new protocol first.

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
- 2026-05-06 evidence after duplicate renderer-copy removal: direct snapshot payload copy is still ~251 µs p99 and `audio_snapshot_read` is ~460 µs p99 on K1v2 `0x2102`. Hot/cold split is the next plausible lever, but it is a contract refactor touching stimulus, legacy inactive Trinity compatibility, debug/streaming, and effect compatibility; do not start it as a small patch.
- Trinity status note: Captain clarified on 2026-05-06 that Trinity has never been actively deployed or utilised; source hooks should be treated as dormant compatibility only. Evidence note: `firmware-v3/docs/research/trinity_inactive_status_note_2026-05-06.md`.

### MabuTrace library risk
- 7 GitHub stars, 1 fork, single maintainer (mabuware/Matthias Buhlmann)
- Core is only ~15 KB of C -- could fork or reimplement under Apache-2.0 if abandoned
- Library is feature-complete and stable for current needs

### 0x130E SB Spectral Envelope — deferred concepts (post-baseline 2026-04-30)
Validated baseline at tag `0x130E-validated-solid-8` (Captain hardware verdict: solid 8/10). Three concepts surfaced during repair but deliberately not implemented; do NOT attempt during the current 30-40 effect repair sweep.

1. **Softened end-trail / no-audio fade.** Current build cuts trails extremely quickly when audio drops below active range. Captain's verdict: keep as-is because it reinforces the "audio is the engine" lock-in feel. Revisit only if later user testing says the cut feels too abrupt or anti-climactic. The lever is the no-audio branch in `SbSpectralEnvelopeEffect.cpp` (the `fadeToBlackByDt(..., 16, dt)` path, currently around line 124), not `decayBase`. A new param `m_silenceFadeAmt` defaulting to 16 would let Captain runtime-tune the no-audio decay without recompiling.
2. **Saturation-aware trail-buffer blend.** The current additive `+=` accumulation across overlapping scrolled hues causes pastel/white wash on devices with hot audio input (K1v1 with mic-on-speaker geometry). A saturation-aware blend (screen blend, max blend, or controlled alpha blend) would prevent the additive overflow without lowering boost. Parked because it changes visual character and may affect every other effect that uses similar trail accumulation. Treat as a render-primitive-level investigation, not effect-local.
3. **Per-device runtime tuning via NVS.** The K1v1 / K1v2 acoustic delta (3.5x bass on K1v1 from physical setup) means the same `onsetBoost` doesn't render identically on both. NVS-persisted per-effect parameter overrides would let each device store its own calibrated values. Rejected for now — too much operational overhead while 30-40 effects remain to repair. The serial setter (`effects.parameters.set`) covers the immediate workflow; persistence can be added later if production units ship with varying mic placements.
