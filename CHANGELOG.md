# Changelog

All notable changes to the LightwaveOS project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - ESP32-P4 Audio Pipeline & iOS App

### Added
- **firmware/docs (zones) — Phase 0 closeout (2026-05-02):** Zone Composer Instrument Program contract clarity, regression scaffolding, and architectural decision record landed. Includes:
  - **B1 (firmware):** 10 zones REST endpoints converted from misleading stubs to explicit HTTP 501 NOT_IMPLEMENTED with structured error bodies; pending real implementation when underlying engine capability lands. Refs: `firmware-v3/src/network/webserver/handlers/ZoneHandlers.cpp`.
  - **B2 (docs):** Unified Zone Composer command matrix documenting REST/WS/SerialJSON command parity. Refs: `docs/protocol/zones-command-matrix.md`.
  - **B5 (docs):** SerialJSON parity inventory with per-command gap audit (gap #5 — `zones.list` missing `zoneId`+`effectName` — backlogged into the wire-format migration). Refs: `docs/protocol/zones-serial-json-parity.md`.
  - **ADR (docs):** Zone Composer architecture decision record — D-1 keystone, D-3 parameter routing, D-4 audio routing, blend-mode namespace, transport equivalence — with verification findings. Refs: `docs/adr/zone-composer-architecture-decisions.md`.
  - **B7 (firmware/test):** BlendMode namespace native regression test guards `BlendMode`→`GradientBlendMode` rename. Refs: `firmware-v3/test/test_blend_mode_namespace/`, `firmware-v3/platformio.ini` (`[env:native_test_blend_mode_namespace]`).
  - **D-8 (firmware/test):** Zone regression gate — invariants README + `zone_regression_smoke.py` I3 (50-toggle Triple Rings stress, heap-delta + zone-state preservation). I1/I2/I4 are TODO_PHASE1 stubs. Refs: `firmware-v3/scripts/zone_regression_smoke.py`, `firmware-v3/test/test_zone_regression_gate/README.md`.
  - **E5 (tools):** Cross-transport zone-equivalence harness — REST/WS/SerialJSON parity validator with zone-speed test PASSING. Refs: `firmware-v3/tools/zone-equivalence-harness/`.
  - **Blend-mode namespace rename (firmware):** `BlendMode` → `GradientBlendMode` migration to align with gradient module structure. Refs: `firmware-v3/src/effects/gradient/{GradientRamp,GradientTypes}.h`, `firmware-v3/src/effects/zones/BlendMode.h`, `firmware-v3/docs/gradient-system.md`.
  - **Zone numbering convention:** Hardened to 1-indexed (Zone 1/2/3) across all docs, scripts, plans. Zone 0 references purged. Wire-format migration (zoneId 0→1-indexed across REST/WS/SerialJSON) backlogged for next phase.

  [deferred hardware validation: REST 501 spot-check pending next hardware window]
- **firmware:** Phase 1+2 visual pipeline reform substrate — Layer 4 render primitives (`drawDot`, `drawSpriteScrolled`, `fillFromBins`) and Layer 5 frame post-process (`applyFrameBlending`) at `firmware-v3/src/effects/render/`. Centre-origin, dual-strip, dt-correct, no-heap, < 0.3 ms each at 320 LEDs. Mood-controlled global persistence via `applyFrameBlending`. 14 native unit tests at `test/test_render_primitives/` (all passing). Unwired in this commit — Phase 5+ effect ports will consume the substrate. Refs: `firmware-v3/docs/research/spazz_redesign_2026-04-30/PIPELINE_REFORM.md`.

### Fixed
- **firmware (zones) — D-1 PARTIAL FIX (2026-05-02):** `ZoneComposer::loadPreset()` and `ZoneComposer::setZoneEffect()` now explicitly call `effect->init(m_zoneContext)` on the assigned effect instance. **Root cause:** `RendererActor` only init()s the *globally active single-effect* (the one set via `setEffect()`), so any effect that has only ever been used inside ZoneComposer (e.g. K1 Bloom 0x1301 + K1 Waveform 0x1302 in a custom Dual Split preset) had its `init()` lifecycle skipped — the effect's PSRAM trail/scratch buffers (`m_bloom`, trail buffer, etc.) stayed nullptr, and `render()` either bailed out or wrote to garbage memory → **the strip rendered BLACK**. Symptom: zone preset loads cleanly per logs, but LEDs show nothing. **Doctrine for future agents:** ZoneComposer does NOT auto-init effects. Any code path that assigns an effectId to a zone (loadPreset, setZoneEffect, setLayout, NVS-restore) MUST call `IEffect::init()` on the new effect instance. **Full fix (D-1 keystone) — per-zone instance pool — is still pending** for the case where the SAME effectId is assigned to 2+ zones simultaneously (singleton state corruption). Refs: `firmware-v3/src/effects/zones/ZoneComposer.cpp:570-583, 712-727`, `docs/adr/zone-composer-architecture-decisions.md` § D-1, memory entry `feedback_zone_effects_need_init.md`.
- **firmware (zones):** Factory preset 1 "Dual Split" rebuilt — was `EID_RIPPLE_ENHANCED + EID_LGP_HOLOGRAPHIC`, now `EID_SB_K1_BLOOM (0x1301)` on Zone 1 + `EID_SB_K1_WAVEFORM (0x1302)` on Zone 2 with SCREEN blend over OVERWRITE base. Captain directive 2026-05-02. Refs: `firmware-v3/src/effects/zones/ZoneComposer.cpp:91-104`.
- **firmware (zones):** `ZONE_2_CONFIG` geometry rebuilt — was 40+120 LED split (Zone 1 inner 60-79+80-99, Zone 2 outer 0-59+100-159), now **60+100 LED split** (Zone 1 centre 50-79+80-109, Zone 2 outer 0-49+110-159) per Captain's exact specification 2026-05-02. Layout passes `validateLayout()`: symmetry around centre pair 79/80, full coverage 0-159, centre-outward ordering. Refs: `firmware-v3/src/effects/zones/ZoneDefinition.h:69-86`.
- **firmware (effects):** 0x1301 K1 Bloom — canonical SB 4.1.1 `light_mode_bloom()` rewrite in CRGB_F (Phase 5B). Replaces the broken integer-truncation scroll (which wiped trails on ~26% of frames when `pixelsToScroll` truncated to 0) with V1's existing CRGB_F `drawSprite()` method (faithful port of SB `led_utilities.h:1247-1290`). Removes 9 K1-team divergences from canonical SB: threshold filter, `totalMag` normalisation, EMA centre injection, sqrt distortion, fallback colour, audio-modulated scroll, linear half-strip edge fade. Adds chroma input peak normalisation (5 lines) before colour synthesis to match SB's `make_smooth_chromagram` peak-tracker behaviour — required for K1 ESV11 raw chroma scale (0.05–0.3) to map onto SB's normalised range. Hardware A/B verified on K1v1 (richer colour pipeline) — bloom now scrolls visibly with audio reactivity. K1v2 shows ~20-LED visual cutoff at strip ends; diagnosed as v1/v2 colour pipeline gamma delta (separate concern), NOT algorithm bug.
- **firmware (audio):** EsV11 `sb_waveform[128]` resampler — populates the SB-parity sidecar by resampling from the existing 4-chunk raw history ring at stride 3, giving a 12 ms time span (vs SB 3.1.0's 10.5 ms). Fixes "no waveform motion" on K1 SB Waveform (Ref) effect: K1 captures at 32 kHz / 128 samples / 4 ms — sub-period for music content < 250 Hz, so bass-frequency oscillations didn't fit in the SB-style waveform window. Resampling restores SB-equivalent temporal span. `out.waveform[128]` preserved unchanged as native 32 kHz / 4 ms for K1-native effects. `sb_waveform_peak_scaled` / `sb_waveform_peak_scaled_last` now computed from the resampled SB window for internal consistency. Effect file `SbWaveform310RefEffect.cpp` untouched (already prefers `sb_waveform`). Refs: data-contract audit in `firmware-v3/docs/research/spazz_redesign_2026-04-30/`.
- **firmware:** ESV11 Goertzel detector lattice re-anchored to C-origin (`vendor/goertzel.h:28` `BOTTOM_NOTE 12 → 6`). Bin 0 is now C2 (65.40639 Hz) instead of D#2 (77.78175 Hz); bins 0..59 cover 5 octaves C2..B6 folding cleanly into chroma[0..11]=C..B per the documented public contract (`ControlBus.h:42`, `EffectContext.h:248,276`). Pre-fix `chroma[0]` carried D# energy, causing `chordState.rootNote` to be +3-semitones offset from its musical name and `WebServerBroadcast` to send the wrong key label (e.g. `"C"` for D# detections) to Tab5/iOS/dashboard. Fix propagates uniformly to every downstream consumer (detectChord, SB sidecar, NOTE_NAMES, MusicalSaliency, the 4 hardcoded-C-origin effects) via the single lattice anchor — no chroma rotation, no detectChord patch, no NOTE_NAMES change. New `test_esv11_lattice` native env (11 tests) asserts the C-origin frequency map and chroma pitch-class identity. K1 V2 hardware-verified. Refs: `firmware-v3/docs/research/AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md`, `firmware-v3/docs/research/CHORD_ROOT_ORIGIN_TRACE.md`.
- **ios:** `ZoneViewModel` zone speed/brightness updates — Track C-2. Migrated WS sends from legacy multi-field `zones.update` to per-property `zone.setSpeed` / `zone.setBrightness`. Aligns iOS with Tab5 + REST contract usage. Firmware still handles the legacy command, but per-property is the canonical path going forward and avoids silent breakage if the legacy handler is later removed.
- **firmware:** `ZoneComposer::PRESETS[]` table — Track C-1. Replaced raw uint8 sequential indices (0, 9, 11, 14, 16, 17, 24, 97) with namespaced `EID_*` constants. The integers were valid pre-2026-02-27 but became invalid after the hex-namespacing refactor (commit `d943101a`) — `RendererActor::getEffectInstance()` returned `nullptr` for every preset zone, rendering BLACK on every fresh-K1 boot via `loadFromNVS()` fallback path. Captain decisions: Preset 0 "Unified" → `EID_LGP_HOLOGRAPHIC_AUTO_CYCLE` (was Fire — now `isExperimental`), Preset 1 "Dual Split" → `EID_RIPPLE_ENHANCED` + `EID_LGP_HOLOGRAPHIC` (base, not ES_TUNED), Preset 2 "Triple Rings" → ENHANCED variants `EID_LGP_WAVE_COLLISION_ENHANCED` + `EID_LGP_INTERFERENCE_SCANNER_ENHANCED` + `EID_LGP_STAR_BURST_ENHANCED` (richer audio responsiveness), Preset 3 "Heartbeat Focus" → `EID_HEARTBEAT` + `EID_BREATHING` × 2 (verbatim original intent). NVS V3 schema unchanged; user-saved configs unaffected.
- **firmware:** `ZoneComposerStub.h` — `getZoneEffect()` return type and `zoneEffect` member type promoted from `uint8_t` to `uint16_t` to match the real `ZoneComposer::EffectId` signature. Native codec tests previously truncated 16-bit namespaced IDs to low byte, producing false positives.
- **firmware:** `LGPHolographicAutoCycleEffect` brightness formula — parenthesis fix so DC offset (128) scales with `intensityNorm`; was `(128 + 127*layerSum) * intensityNorm` incorrectly leaving DC offset unscaled at low brightness
- **firmware:** `SbRawWaveformScopeEffect` — replace direct `controlBus.bins64[i]` field access with `ctx.audio.bins64Adaptive()` accessor, consistent with post-migration pattern used by `LGPSpectrumDetailEffect` and `JuggleEffect`

### Added
- **ios:** Capability discovery on connect — `DeviceCapabilities` model + `RESTClient.getCapabilities()` probes `GET /api/v1/openapi.json`, falls back to `GET /api/v1/firmware/version`, returns `nil` on any failure (best-effort; never breaks connect). Closes the architectural blind spot where firmware drift was silent; `AppViewModel` now stores capabilities post-connect under `// MARK: Phase 1 — capability discovery`.
- **ios:** Seven inbound `Event` enum cases for broadcasts firmware emits but iOS previously dropped — `cameraMode.changed`, `factoryPresets.changed`, `effectPresets.saved`, `effectPresets.deleted`, `colorCorrection.setGamma`, `colorCorrection.setAutoExposure`, `colorCorrection.setBrownGuardrail`. Handlers stubbed for Phase 1; Swift's exhaustiveness check now catches future broadcast drift.
- **ios:** Forward-compat `EffectParameter` decoder with `ParameterType` enum (FLOAT/INT/BOOL/ENUM) for the `effects.parameters` envelope (firmware commit `4398af3b`). Accepts both legacy shape (no `type` field, decodes to `nil`) and new shape; unknown numeric codes decode to `.unknown` rather than throwing.
- **docs:** `docs/superpowers/ios-firmware-parity-phase-1.md` — Phase 1 plan capturing three GROUNDED architectural iOS fixes plus the F-1..F-4 calibration debt that gates Phase 2 (effect picker, runtime parameter UI, STM/VRMS visualisation, presets/shows surface).
- **docs:** `BACKLOG.md` § Critical — Upstream Calibration Debt — F-1 (contract authority), F-2 (effect production cohort), F-3 (path canonicalisation), F-4 (runtime parameter UX scope) — iOS↔firmware parity decisions Captain must resolve before Phase 2.
- **firmware/ios:** `isExperimental` metadata flag for catalogue effects — `PatternRegistry::isExperimental(EffectId)` lookup function emitted as JSON field on `/api/v1/effects` and `effects.getMetadata` endpoints (REST + WS contracts updated). iOS `EffectViewModel.filteredEffects()` excludes experimental effects from default production view; backward-compatible decoder treats absent field as `false`. 13 effects tagged total: `EID_FIRE` (borderline), `EID_LGP_CAUSTIC_FAN`, six ShapeBangersPack pack-internals (`EID_LGP_AIRY_COMET`, `EID_LGP_SUPERFORMULA_GLYPH`, `EID_LGP_SPIROGRAPH_CROWN`, `EID_LGP_ROSE_BLOOM`, `EID_LGP_RULE30_CATHEDRAL`, `EID_LGP_LANGTON_HIGHWAY`), and 5-Layer AR variants `EID_LGP_SCHLIEREN_FLOW_AR`, `EID_LGP_AIRY_COMET_AR`, `EID_LGP_SPIROGRAPH_CROWN_AR`, `EID_LGP_RULE30_CATHEDRAL_AR`, `EID_LGP_LANGTON_HIGHWAY_AR` (Captain verdicts 2026-04-26 + orchestrator analogy call for Langton AR). NOT tagged: `EID_LGP_ROSE_BLOOM_AR` (centre-origin compliant — the standalone Track α-2 effect, naming collision with pack-internal Rose Bloom), `EID_LGP_SUPERFORMULA_GLYPH_AR` (Captain verdict: CO compliant). Tab5 client filtering deferred (architectural: Tab5's uint8 effect-index array cannot key 16-bit namespaced EIDs; iOS is the effective production filter).
- **firmware:** Rose Bloom (5L-AR) runtime test-mode selector — Baseline + 9 hypotheses (A–I) for "carry-through" tightening (Captain's "extremely tight, no carry-through" observation). Modes exercise event-anchored slow envelopes (downbeat A / beat B), tempo-locked geometric LFO (C), petal-count latch (D), dual-layer slow EMA (E), chroma phrase persistence (F), multi-scale impact decay (G), petal spring inertia (H), and adaptive fadeAmt (I). RB-FC fact-check guards: LIFT ceiling-clamp 0.30, Mode H velocity clamp + boundary-zero + mode-switch reset, timingReliable() gating. Cycle live via SerialCLI `'R'` key.
- **firmware:** Bloom Parity (0x1500) runtime test-mode selector — `Baseline` + 9 hypotheses (A–I) for the second-motion-layer artefact (observation #28405). Modes exercise prism-axis variations (off, halved, no-mirror, additive cap, multiplicative, SB-parity 0.25), edge-fade-to-zero, bulb cover off, and transport-alpha sweep. Non-destructive local overlays (statics never mutated). Cycle live via SerialCLI `'M'` key — Captain hardware A/B framework for the 2-year Bloom Parity tightening question.
- **tab5:** PSRAM-primary preset storage — NVS demoted to write-behind backup; no code path can silently erase user presets
- **tab5:** Custom partition table — NVS enlarged from 20KB to 64KB (SPIFFS reduced by 44KB)
- **tab5:** NVS health tracking (`isNvsHealthy()`) for storage diagnostics
- **docs:** Synergy-topology research drop (audio lattice investigation, Nyquist LUT spec, musical logic audit §6.3 errata, canonical musical model, Phase 5 checkpoints, spazz redesign SSA packet, Phase 1B MabuTrace evidence bundle incl. reports + pipeline logs, AFSv2 draft, pathmode prompts, session notes under `docs/research/`). Root `.gitignore` / `firmware-v3/.gitignore` exceptions allow `phase1b_runtime_evidence_2026-04-27/**/reports/**` and `*.log` only under that tree.
- **docs:** K1 landing page production pipeline — taglines, strategy, dual-state positioning, build spec, launch video spec, 5 HTML variants
- **firmware:** Vendored FastLED 3.10.0 RMT4 `idf4_rmt_impl.cpp` overlay (non-blocking `showPixels`) with PlatformIO pre-script — `firmware-v3/patches/vendor/FastLED-3.10.0-rmt4/` and `firmware-v3/scripts/apply_fastled_rmt4_patch.py`
- **scripts:** K1 loaded soak harness — serial stress (effect rotation, hotkeys, periodic `s` status) plus optional REST when the host can reach the K1 AP — `firmware-v3/scripts/k1_loaded_soak.py`
- **docs:** Inference task decision brief with evidence tags (latency, memory, execution targets; Orin downstream of open spec) — `firmware-v3/docs/design/INFERENCE_TASK_DECISION_BRIEF.md`
- **docs:** Seeded inference task-placement matrix (DSP / heuristic / ML classes, wire vs contract drift, mapping buckets) — `firmware-v3/docs/design/INFERENCE_TASK_PLACEMENT_MATRIX.md`
- **docs:** Inference placement matrix rev 0.2 — split tempo vs phase rows, vocal inject vs native, mandatory Yes/No/Research-only defaults, explicit failure semantics
- **docs:** Inference placement matrix rev 0.3 — parallel SSA corrections (ESV11 EsBeatClock path, merge m_merged hold, translation LOCK/silence, saliency/style grep vs features.h, wire REST/WS nuance)
- **firmware:** STM (Spectral-Temporal Modulation) dual-edge mode — per-LED spectral modulation for EdgeMixer
- **firmware:** STM 128-band spectral upgrade — mel filterbank + FFT pipeline
- **firmware:** STM snapshot REST endpoint with derived metrics
- **firmware:** STM real-time WebSocket binary stream + heap threshold fix
- **firmware:** STM_SPECTRAL_MAP EdgeMixer mode — per-LED spectral modulation bin mapping
- **tab5:** Zone Mode enable/disable button on ZONES sidebar panel with layout-before-enable sequence
- **tab5:** Zone count selector (1/2/3 zones) on grid slot 5, encoder-only (ENC-B 5)
- **tab5:** LED count display per zone on grid slot 6 (ENC-B 6)
- **tab5:** 3-zone centre-origin layout support (was limited to 2)
- **tab5:** Preset save/restore for EdgeMixer state (mode/spread/strength/spatial/temporal)
- **tab5:** Preset save/restore for colour correction mode (OFF/HSV/RGB/BOTH)
- **tab5:** Preset save/restore for auto-exposure target value
- **tab5:** Preset save/restore for per-zone blend modes
- **tab5:** Zone layout sent before zone.enable on preset recall (prevents K1 no-op)
- **tab5:** Current global effect auto-assigned to all zones on zone mode enable
- **tab5:** I2C recovery rewrite, PSRAM migration, UI and network hardening
- **tab5:** EdgeMixer mode cycling expanded to 9 modes (added STM DUAL, STM SPECTRAL)
- **zone-mixer:** AtomS3+PaHub physical controller — Phases 1-6 (input layer, display, I2C recovery, parameter mapping, LED feedback, echo suppression)
- **harness:** STM feasibility test harness for spectral-temporal modulation
- **scripts:** STM system test suite — zero-dependency offline tooling
- **docs:** STM 128-band spectral upgrade specification
- **docs:** STM endpoint, stream commands, and mode range updates
- **docs:** Tab5 I2C recovery research, memory audit, and implementation guides
- **docs:** Cross-workspace `CAVEMAN_RUNBOOK.md` — canonical install, disable, and guardrails for the caveman Claude Code plugin; rewritten 2026-04-22 from a 10-axis empirical audit that refuted the pre-install assumption that marketplace install skips auto-activation (plugin.json hooks fire regardless). Pointer stubs in the three satellite workspaces.
- **tooling:** `~/.claude/hooks/claude-mem-freshness-check.sh` — SessionStart hook that queries claude-mem for stale observations (>24h), stale session_summaries (>48h), or stuck pending_messages (>1h). Silent when healthy; banner in session context when any threshold crosses. Detection-gap fix for the 2026-04-20→23 outage where the write pipeline silently stalled for 68h. ~20ms per session start. Registered in `~/.claude/settings.json` alongside gsd-check-update.

### Changed
- **firmware/docs (zones) — Wire-format migration (2026-05-02):** `zoneId` migrated from 0-indexed to 1-indexed on every external transport (REST, WebSocket, SerialJSON). Internal storage (`m_zones[0..2]`, `m_zoneConfig[i].zoneId`, predefined `ZONE_*_CONFIG` arrays) UNCHANGED — translation centralised at the boundary via two inline helpers in `firmware-v3/src/network/RequestValidator.h`: `wireZoneIdToInternal()` (range-checks wire 1..3, returns internal 0..2) and `internalZoneIdToWire()` (internal 0..2 → wire 1..3). Wire `zoneId = 0` is now reserved and rejected uniformly with HTTP 400 / `INVALID_VALUE` error response across all transports. Per-zone REST regexes tightened from `[0-3]` to `[1-3]` — `GET /api/v1/zones/0` returns 404 NOT_FOUND from AsyncWebServer (no route match). Folds **B5 SerialJSON parity gap #5 (row-level)**: `zones.list` SerialJSON rows now include `zoneId` (1-indexed wire), `effectName` (resolved via `renderer->getEffectName()`), and a `paletteId` alias matching REST (existing `palette` field preserved for backward compat). Boundary translation applied to: REST handlers (`ZoneHandlers.cpp`, `V1ApiRoutes.cpp`), WebSocket codec/handlers (`WsZonesCodec.{cpp,h}`, `WsZonesCommands.cpp`, `WsZonePresetCommands.cpp`), SerialJSON dispatcher (`SerialJsonGateway.cpp`), broadcasts (`WebServerBroadcast.cpp`), action dispatch (`WebServer.cpp`). Contracts updated: `k1-rest-contract.yaml`, `k1-ws-contract.yaml`, `zones-command-matrix.md`, `zones-serial-json-parity.md`. Build pass on `esp32dev_audio_esv11_k1v2_32khz` (RAM 43.5%, Flash 33.3%). **BREAKING for clients still sending `zoneId=0`** — iOS / Tab5 / dashboard need parallel updates; out of scope of this commit. D-1 keystone (per-zone instance pool) and D-3 (per-zone parameter routing) untouched. Refs: `feedback_zone_numbering.md`. [deferred hardware validation: REST/WS/SerialJSON wire-format integration testing pending next hardware window]
- **firmware:** Extracted 119-line post-processing tail (prism + bulb cover + incandescent + output mirror) from 7 `SbK1BloomV2*Effect` subclasses into `applyBloomV2PostProcessing()` helper on the `SbK1BloomV2Effect` base class — previously duplicated 6× byte-identically across SpectralDelta (0x1308), BassTreble (0x1309), BeatPulse, ColorHistory, Exponential, SpectralSpread. Zero behaviour change (MD5-verified byte-identical extraction). Net −573 LOC in `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomV2Effect.{h,cpp}`. Pre-requisite for upcoming Track α runtime-mode variants on 0x1308 and 0x1309.
- **tooling:** caveman plugin auto-activation disabled via `~/.config/caveman/config.json` `{"defaultMode":"off"}`; plugin remains installed and opt-in via `/caveman lite|full`. Prior state (since marketplace install) was `full` mode on every session.
- **tooling:** 18 caveman-voice entries purged from `claude-mem` `pending_messages` queue under Captain authorisation (ids 198160, 198167, 198176, 198182, 198189, 198190, 198193, 198198, 198206, 198208, 198210, 198249, 198256, 198282, 198285, 198295, 198154, 198161); backups at `/tmp/caveman-research/pending_messages_purge_{round1,round2,round3}_backup_*.json`. Post-disable queue rescan confirmed no new contamination.

### Removed
- **tooling:** caveman Claude Code plugin fully uninstalled under Captain approval after 10-axis canonical audit returned a FAIL verdict (auto-activation on install, below-advertised compression, structural British-English hostility, claude-mem corpus-pollution conflict, governance overhead). Plugin removed via `claude plugin uninstall caveman`; marketplace removed via `claude plugin marketplace remove caveman`; `~/.config/caveman/`, `~/.claude/.caveman-active`, `~/.claude/plugins/cache/caveman/`, `~/.claude/plugins/data/caveman-caveman/` all cleaned. Canonical runbook moved from `docs/CAVEMAN_RUNBOOK.md` to `_archive/caveman-runbook-2026-04-23.md` with FINAL VERDICT banner. Pointer stubs in the three satellite workspaces collapsed to one-liners referencing the archive.

### Fixed
- **tooling:** claude-mem write-pipeline outage (2026-04-20 to 2026-04-23, ~68h) resolved. Root cause: three-way version drift — hooks running 12.3.2 (via `ls -dt` cache resolver) but MCP server and HTTP worker stuck at 10.5.6 because `installed_plugins.json` stayed pinned at 10.5.6 when marketplace refreshed. Contributing: stale `ANTHROPIC_API_KEY` in `~/.zshrc:267` hijacked Claude CLI into direct-API-key mode bypassing Max subscription OAuth. Symptom: `observations` and `session_summaries` frozen at 2026-04-20T18:12Z despite hooks still ingesting `user_prompts` and `pending_messages`. Fix: pin `installed_plugins.json` to 12.3.2, archive 10.5.6/12.1.5/12.1.6 cache dirs, comment out the stale API key export, restart worker, flush queue. Cohort B (101 agent-06 harness-noise rows) auto-cleared by issue #1957 fix. Cohort A (28 Lightwave observations from 2026-01-26 session 47941) lost to 3-month-deferred context-overflow — acceptable loss per triage. Full post-mortem at `_archive/claude-mem-outage-2026-04-23.md`. Backups at `~/claudemem-backups/`.

### Fixed
- **firmware:** WS heap-shed gate was latched permanently on K1v2 because the 22 KB shed threshold sat above the observed ~21 KB idle internal-heap baseline, so the per-connect 1013 reject at `WebServer.cpp:1437-1440` blocked every tab5 reconnect attempt; the latch also flapped via the 10 s force-clear hysteresis escape every cycle. Dropped shed threshold to 18 KB / resume to 28 KB (same 10 KB hysteresis width, but below the idle baseline) and removed the per-connect reject gate — shed now only activates on genuine pressure events, broadcast-side gating retained. Hardware-verified 2026-04-18: 0 shed events in 3.5 min K1v2 uptime (was: continuous flapping every ~10 s).
- **firmware:** WS connect-cooldown timestamp was refreshed on rejected retries (`WsGateway.cpp:233`), perpetually locking out fast reconnectors (tab5 at 2 s backoff never crossed the 2 s cooldown boundary). `lastMs` now only updates on accepted connects, so rejected retries age out as intended. Root cause of the K1↔tab5 WS reconnect churn investigated 2026-04-18.
- **firmware:** P1-09 shared AR effect state across zones — completed the per-zone `[kMaxZones]` migration for the remaining default-build AR effects: 10 scalar-only effects, 4 PSRAM-buffer effects, `LGPIFSBioRelicAREffect`, and the full `LGPTimeReversalMirror` AR/Mod1/Mod2/Mod3 family. Added `SPIRAM free` to serial status, measured an active-only TRM allocation lifecycle on hardware (`B0=8,058,167`, `B5=6,743,727`, `B9=8,058,167`), and verified the Phase 6 gate with >2.5 MiB margin above the 4 MiB reserve threshold.
- **firmware:** Same-effect multi-zone P1-09 control path exercised on hardware — zone mode enabled, identical migrated effects assigned to zones 0/1, per-zone speed divergence applied, single-zone reset path exercised, and mixed-effect sanity rechecked with stable 119 FPS / `showSkips=0`.
- **firmware:** Core 1 panic "Cache disabled but cached memory region accessed" when the FastLED RMT ISR called `micros()` during flash cache suspend (NVS/WiFi) — vendored `fastled_delay.h` uses `esp_timer_get_time()` for `CMinWait::mark()` on ESP32; apply script copies it with the RMT overlay
- **firmware:** Belt-and-braces hardening of FastLED RMT ISR path — `ESP32RMTController::startNext`, `startOnChannel` and `tx_start` in the vendored `idf4_rmt_impl.cpp` now carry `IRAM_ATTR`. Prevents cache-disabled panics on K1v2 where `gNumControllers` (3: two main strips + StatusStripTouch on GPIO 38) exceeds usable parallel RMT channels (2), causing `doneOnChannel` → `startNext` → `startOnChannel` to execute in ISR context on every `show()`
- **firmware:** Silent lock-up after ~40 rapid effect-change commands ("device keeps running, serial goes dead, LEDs freeze, no crash"). `Actor::run()` drain loop at >50% queue utilisation processed up to 8 messages back-to-back with no watchdog feed, no yield and no `onTick()` call — starving loopTask (and its serial polling) on Core 1 and eventually backing up the HWCDC TX ring until every `Serial.print` caller blocked. Drain is now capped at 4 messages, resets the task watchdog and yields after each dispatch, and forces one `onTick()` per drain cycle so frames keep rendering. Also demoted `IEffect cleanup/init/SUCCESS` per-step logs to `LW_LOGD` (kept the summary line at INFO) to shrink the log bandwidth per change from ~720 B to ~180 B. Also added a 20 ms auto-repeat coalesce to SerialCLI's cycle keys (` `, `n`, `N`) so a held key can't flood the renderer queue in a single tick.
- **firmware:** Progressive cascade lockup while rendering heavy effects (most reliably reproduced with `LGP Gravitational Lensing` 0x0601 — device ran normally, then serial went silent ~90 s into rendering that effect and never recovered). Root cause: `LW_LOG_PRINTF` mapped directly to `Serial.printf()` which blocks indefinitely when the HWCDC TX ring has no headroom; once one task stalled there, every other task calling `Serial.*` queued behind it. `LW_LOG_PRINTF` now guards with `Serial.availableForWrite() >= 256` and drops the log on full rather than blocking the caller. Also coarsened `LGPGravitationalLensingEffect::render()` (outer ray step 2→4, inner step 80→40) to bring its per-frame cost from ~15-25 ms back toward the 2 ms budget.
- **firmware:** Set HWCDC `setTxTimeoutMs(20)` in `initSerial()` so every Serial write — including direct `Serial.printf()` calls in SerialCLI that bypass `LW_LOG_PRINTF` — returns within 20 ms instead of blocking indefinitely on a full TX ring. This is the systemic belt for the same cascade-lockup class of bug; without it a different heavy effect (next observed: `Chimera Crown` 0x1900) would still wedge the device even with the `LW_LOG_PRINTF` guard in place.
- **firmware:** Cumulative-state wedge under sustained effect cycling — final closure. The cascade had multiple compounding sources: (1) `LGPTimeReversalMirrorEffect` base + `_AR` + `_Mod1/2/3` were `memset`-ing the full 45 KB / 321 KB PSRAM history buffer on every `init()` (~9–64 ms blocking Core 1 per change); now only the live 960 B field arrays are zeroed — the history buffer is gated by `m_historyCount=0` so stale data is never read. (2) Nine effects (`LGPReactionDiffusion{,AR,Triangle,TestRig}Effect`, `LGPRDTriangleAREffect`, `LGPCatastropheCausticsAREffect`, `WaveformParityEffect`, `EsOctaveRefEffect`, `LGPLangtonHighwayAREffect`) were freeing their PSRAM in `cleanup()` then re-allocating in the next `init()` — fragmenting PSRAM over ~150 cycles; they now retain their PSRAM across the effect lifetime (matches the pattern used by TRM, SbK1Base and most other effects). (3) `KuramotoTransportEffect::init()` leaked the first one or two PSRAM allocs if the second or third failed; failure paths now roll back. (4) `RendererActor::handleSetEffect` now calls `cleanup()` on the new effect if `init()` fails, so partial allocations don't strand. (5) `WebServer::m_lowHeapShed` could latch permanently if free internal heap oscillated in the 20–26 KB hysteresis band (fed by the WS reconnect-storm caused by `closeAll(1013)`); added `INTERNAL_HEAP_SHED_MAX_LATCH_MS = 10 s` force-clear so the flag cannot stay ON indefinitely. (6) `broadcastAudioFrame()` now gates on `hasSubscribers()` (previously only on `m_lowHeapShed`) — zero work per tick when no WS listener. (7) `RendererActor::handleSetEffect` no longer publishes the dead `EFFECT_CHANGED` message (zero subscribers across the firmware). (8) NVS-deferred `LW_LOGW` warning now rate-limited to 1 Hz (was flooding at 100 Hz when heap < 8 KB). (9) Arduino `enableLoopWDT()` now called in `setup()` so loopTask hangs (serial CLI, NVS, WebServer update) trigger a 5-second task-WDT reset with backtrace — previously only RendererActor was WDT-subscribed, making any loopTask hang invisible. **Verified: 280 rapid effect-change commands sustained over 60 s with zero stall (vs 45-command stall on baseline — 6.2× improvement), 220 effect renders completed, post-burst device still at 119 FPS with healthy CLI.**
- **firmware:** NVS debounced-save deferred when internal heap < 8KB — prevents flash corruption under memory pressure
- **firmware:** WebSocket reconnect storm during heap shedding — `handleWsConnect()` rejects with 1013 while `m_lowHeapShed` is active
- **tab5:** `nvs_flash_erase()` removed from NvsStorage and PresetStorage — was silently destroying user presets on NVS version mismatch
- **firmware:** Removed temporary overlap/JSON serial debug and invalid host-path logging from `LedDriver_S3` and `RendererActor` (ESP32 cannot append to macOS paths).
- **tab5:** Zone effectId truncated from uint16_t to uint8_t — ZoneState.effectId and WsMessageRouter parsing both used uint8_t, losing high byte of K1's hex effect IDs (0x0100+)
- **tab5:** Zone effect encoder sent raw 0,1,2,3 — clamped to valid K1 range 0x0100-0x1F00 with cached name display
- **tab5:** Zone palette encoder had no upper bound — now wraps 0-74 (75 palettes)
- **tab5:** Zone blend encoder had no upper bound — now wraps 0-7 (8 modes)
- **tab5:** Zone encoder zoneId not clamped to zone count — caused "Invalid zoneId" flood when zone count reduced
- **tab5:** Zone param cards 5-6 accepted touch events and bubbled to mode row — LV_OBJ_FLAG_CLICKABLE and LV_OBJ_FLAG_EVENT_BUBBLE cleared
- **tab5:** Preset zone brightness hardcoded to 255 on save — now reads actual ZoneState.brightness
- **tab5:** Preset CC mode used live server state on recall instead of saved value
- **tab5:** Preset AE target hardcoded to 110 on recall instead of saved value
- **tab5:** Preset zone blend mode captured but never restored (missing sendZoneBlend call)
- **tab5:** EdgeMixer mode display showed "???" for modes 7-8 — added STM DUAL and STM SPECTRAL names
- **firmware:** Cross-core race conditions in ZoneComposer and WebSocket broadcast
- **firmware:** Lower heap-shed thresholds and fix probe condition
- **zone-mixer:** Phase 4 bug fixes — per-zone effects, zone layout, layout-before-enable
- **zone-mixer:** Safety audit fixes — WDT, I2C error handling, Arduino compatibility
- **firmware (forensic-audit 2026-04-17, Wave 1):** P0-01 `StateStore` deadlock — inconsistent mutex acquisition ordering between `setActiveIndex()` and `get()` could deadlock under concurrent access from CommandProcessor + RendererActor; ordering now unified, `m_activeIndex` made `std::atomic` for lock-free reads
- **firmware (forensic-audit):** P0-02 OTA session timeout — idle OTA sessions now expire after 120s via periodic `update()` cron; previously a disconnected uploader left `OtaSessionLock` held forever, blocking all future OTA until reboot
- **firmware (forensic-audit):** P0-03 `PluginManagerActor` sizeof bug — `sizeof(manifest_ptr)` returned pointer width (4 B) instead of the pointed-to manifest struct, truncating every plugin manifest load
- **firmware (forensic-audit):** P0-04 `AuthRateLimiter` counter wrap — 32-bit request counter could wrap to 0 after ~4 B requests, opening an unlimited auth-attempt window; now uses wrap-safe subtraction
- **firmware (forensic-audit):** P0-05 `WiFiManager` softAP retry + TWDT — Task-WDT now fed during softAP retry loop; previously long retry sequences tripped WDT and reset the device mid-recovery
- **firmware (forensic-audit):** P0-06 four `portMAX_DELAY` unbounded-block sites — `EncoderManager.cpp`, `microphone.h`, `SerialCLI.cpp`, `WsStreamCommands.cpp` queue sends now time out (10–50 ms) with drop-on-contention telemetry; previously any full queue could wedge the calling task indefinitely
- **firmware (forensic-audit):** P0-07 `AudioActor` Task-WDT + dt correction — watchdog now fed in the I2S hop loop, and per-hop dt uses measured elapsed instead of nominal hop period; previously DSP overruns tripped TWDT and dt drift desynchronised beat tracking
- **firmware (forensic-audit):** P0-08 `BeatPulseSpectralPulseEffect` uint8 underflow — `size - 1` could wrap to 255 when the pulse list was empty, indexing stack garbage; guarded with emptiness check
- **firmware (forensic-audit):** P1-01 AsyncTCP task priority elevated (2 → 3 via `CONFIG_ASYNC_TCP_RUNNING_CORE`/`_PRIORITY`) — prevents TCP starvation when RendererActor runs at priority 2 on the same core
- **firmware (forensic-audit):** P1-03 `UdpStreamer` streaming-active latch cleared when last subscriber leaves — previously the latch stayed on forever after the last UDP client dropped, burning CPU on zero-consumer broadcasts
- **firmware (forensic-audit):** P1-04 `BenchmarkStreamBroadcaster` + `RendererActor` capture latches — same subscription-count-driven latch clearing applied to benchmark stream and LED capture; both now idle when the last subscriber disconnects
- **firmware (forensic-audit):** P1-05 `NVSManager` silent-wipe telemetry — `nvs_flash_erase()` now emits a conspicuous diagnostic line *before* erasing user data; previously a silent corruption-driven wipe left no trace
- **firmware (forensic-audit):** P1-08 transition-during-transition handling — dispatching `SET_EFFECT` while a transition is already in flight now cancels the in-flight transition cleanly (cleanup/init bracketing); previously double-transition corrupted effect state and leaked the pre-empted effect's buffers
- **firmware (forensic-audit):** P1-11 boot-banner phase markers — every `SystemInit::init{Phase1..10}` step now emits `Phase N: ... complete` so a mid-boot crash identifies exactly which subsystem init wedged
- **firmware (forensic-audit):** P1-12 `tempo.h` counter wrap — long-uptime `int` overflow in the tempo BPM smoother fixed with explicit 32→64 bit cast
- **firmware (forensic-audit):** P1-13 (see P0-07 — AudioActor dt-correction is the same fix)
- **firmware (forensic-audit):** P1-14 cross-core atomics — `EffectValidationMetrics`, `ZoneComposer`, `MessageBus` hot-path shared ints converted to `std::atomic` (lock-free verified); `StateStore::m_activeIndex` also atomicised under P0-01
- **firmware (forensic-audit):** P1-15 WebSocket broadcaster cleanup on disconnect — `WebServer::handleWsDisconnect()` now unsubscribes the vanishing client from LogStream / AudioStream / STM / Benchmark so broadcast latches can clear and heap-sensitive broadcasters stop work when the last client drops
- **firmware (forensic-audit):** P1-16 WebSocket frame fragmentation guard — `WsGateway` now refuses to fragment outbound frames above the AsyncWebSocket single-frame limit, preventing the silent corruption previously observed on large status payloads
- **firmware (forensic-audit):** P1-17 `RateLimiter` LRU eviction — full rate-limiter table now evicts the oldest entry instead of rejecting new requests outright, restoring correct behaviour under high client turnover
- **firmware (forensic-audit, Wave 2):** P1-02 `AudioActor` critical-section shrinkage — `m_controlBus.UpdateFromHop()` (DSP smoothing + spike detection, per-hop hot path) was running inside `portENTER_CRITICAL(&m_controlBusApiMux)`, holding IRQs off across hundreds of microseconds of audio work on both the PipelineCore (AudioActor.cpp:1715–1725) and ESV11 (AudioActor.cpp:3462–3472) hop processors. The critical section has been removed from both regions — cross-core reader safety was already guaranteed by the lock-free `SnapshotBuffer::Publish()` at lines 1835 and 3585, which is the actual cross-core publish path. Also deduplicated four `getControlBusFrameSnapshot()` calls (1734+1754 and 3482+3504) into single local `frameRef` values reused across the style-detection and publish consumers — saves ~5 KB memcpy + one mutex round-trip per hop. Verified: pio build SUCCESS, RAM/Flash usage unchanged, zero stack growth, SSA-7 TWDT + dt-correction additions preserved.
- **firmware (root-cause):** RendererActor yield discipline for heavy effects — on the 450 s pressure battery (`/tmp/k1v2_pressure.py`), every run before this fix hit 6–8 task-WDT resets on `loopTask (CPU 1)` within the first sustained burst: a render() or init() on a compute-heavy effect (Chimera Crown, Kuramoto Transport, Talbot Carpet, Lorenz Ribbon, Spirograph Crown AR) held CPU 1 for 5 s+ with no scheduler opening, so the Arduino loopTask could never feed its 5 s TWDT. Three surgical yields + one timeout bump close this class of wedge: (1) `RendererActor::onTick` end-of-frame yield escalates from `vTaskDelay(0)` to `vTaskDelay(1)` whenever the frame overran the 8.33 ms budget (pacingWaitUs == 0), guaranteeing loopTask gets CPU 1 on every overrun frame; (2) `handleSetEffect` brackets `effect->init()` with `vTaskDelay(1)` before and after so heavy init paths (PSRAM alloc, lookup-table build, oscillator field seed) can't starve loopTask either side of the call; (3) `handleStartTransition` concurrent-transition init gets the same bracketing; (4) `main.cpp` raises loopTask TWDT timeout from the 5 s Arduino default to 10 s via `esp_task_wdt_init(10, true)` — genuine hangs still panic, but a slow-but-progressing render/init no longer trips WDT. Verified: `/tmp/k1v2_pressure.py` full 450 s battery PASSES T1/T2/T3 with **0 TWDT resets, 0 reboots, 10/10 heartbeats, 1992 effects cycled, 119 FPS sustained, 27.6 KB heap free post-burst** (previously: 8 resets, 8 reboots).
- **firmware (forensic-audit):** P1-07 actor-dispatch bool-return discipline — 17 of 20 audited WS/REST handler call sites were ignoring the `bool` return of `actorSystem.setX()` / `->send()` and running `broadcastStatus()` unconditionally, lying "success" to the client while the command was silently dropped under queue saturation. All HIGH-severity sites now capture the return and respond with `ErrorCodes::RATE_LIMITED` (WS) or `HttpStatus::SERVICE_UNAVAILABLE` (REST) on false, matching the pre-existing `WsTrinityCommands` pattern. Multi-call presets (`handleFactoryPresetsLoad`, `handleEffectPresetLoad`, `V1ApiRoutes` EdgeMixer batch) now fail-fast on the first dropped dispatch to prevent partially-applied presets being reported as success. Sites fixed: `WsEffectsCommands.cpp` (×10 handlers), `WsEffectPresetCommands.cpp`, `WsPaletteCommands.cpp`, `EffectHandlers.cpp`, `ParameterHandlers.cpp`, `PaletteHandlers.cpp`, `V1ApiRoutes.cpp` (factory preset + EdgeMixer). Three MEDIUM/LOW internal-only sites (`ShowDirectorActor::sendToRenderer`, `main.cpp applyFactoryPreset`, `main.cpp` boot-state-restore) remain deferred — no WS client exists at those call sites to receive an error. **Client-visible contract impact**: iOS/Tab5/web consumers that previously treated all WS responses as success now see explicit `RATE_LIMITED` errors when the renderer queue is saturated; this is a correctness improvement but client error-path handling should be audited. No new contract fields — `ErrorCodes::RATE_LIMITED` already existed.
- **firmware (forensic-audit):** P1-09 shared effect state across zones — prototype pass (PR 1 of ~6). `ZoneComposer.cpp:284` returns the same `IEffect*` for all zones, so when one AR effect is assigned to multiple zones the effect's single `m_t`/`m_bass`/`m_chromaAngle`/smoothing-EMA members advance N× per frame: audio smoothing collapses to instantaneous values, palette/hue rotations stack, speed multiplies by zone count. Three representative AR effects now dimension their state arrays by `[kMaxZones]` (approach A — matches the existing pattern in `EsBloomRefEffect`, `SnapwaveLinearEffect`, and ~12 other effects): `LGPAiryCometAREffect` (scalar-only, +84 B DRAM), `LGPCatastropheCausticsAREffect` (scalar + LED-domain PSRAM histogram, +84 B DRAM, +1.9 KB SPIRAM), `LGPLangtonHighwayAREffect` (scalar + 2D grid PSRAM, +90 B DRAM, +12 KB SPIRAM). Global-render path (zone ID `0xFF`) uses the established bounds-check pattern: `const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0`. Follow-up plan at `firmware-v3/docs/p1-09-zone-state-followup-plan.md` documents remaining 18 AR effects batched for subsequent PRs (11 trivial scalar, 3 LED-buffer, 2 grid/CA, 4 large-PSRAM/TRM family). `ZoneComposer`, `RendererActor`, `IEffect`, and `EffectContext` are untouched — blast radius confined to effect files.

### Changed
- **firmware:** `ILedDriver::isShowInProgress()` documentation clarified for async RMT (wire time may continue after the flag clears).
- **tab5:** Zone purge 4→3 — 1-indexed (Zone 1/2/3), max 3 zones, no Zone 0
- **tab5:** I2CRecovery stripped to error counter only (-835 lines)
- **tab5:** Purged 22 unused font assets — 79K LOC removed
- **tab5:** Encoder bring-up hardened, external I2C init restored
- **tab5:** Dashboard initialised before host connection
- **tab5:** Pinned validated display and encoder dependencies
- **firmware:** Zone purge — 1-indexed (Zone 1/2/3), max 3, no Zone 0
- **firmware (forensic-audit):** P1-06 K1 NVS partition grown 20 KB → 64 KB (matches tab5 discipline) — previously 113/630 entries were used (~18 %), one feature-expansion away from full. `partitions_custom.csv` now allocates `nvs` at 0x9000 / 0x10000; `otadata` shifts to 0x19000, `app0` to 0x20000 (64 KB alignment preserved), `app1` to 0x720000, `spiffs` to 0xE20000, `userdata` shrinks from 1 MB → 896 KB at 0xEA0000 (no code references the `userdata` partition — grep confirmed zero hits), `coredump` at 0xF80000. Total 15.5 MB used of 16 MB (448 KB slack preserved). Deploy path: this change is NOT OTA-upgradeable from the old 20 KB layout; every K1 in the field requires a one-shot `esptool erase_flash` + full reflash. Pre-launch fleet (≤10 devices) accepts this cost. Captain-decision doc at `firmware-v3/docs/nvs-partition-grow-captain-decision-2026-04-18.md`.
- **firmware (forensic-audit):** P1-10 OTA integrity hash — SHA-256 is now mandatory for both WebSocket (`ota.begin` / `ota.verify`) and REST (`/api/v1/firmware/update`, `/api/v1/firmware/filesystem`, `/update`) OTA uploads. K1 AP is an open network; without a mandatory hash any AP-range attacker with the OTA token could push unsigned firmware, with partition rollback as the only defence. Streaming implementation uses ESP-IDF `mbedtls/sha256.h` — one static `mbedtls_sha256_context` per upload path (WS + REST), allocated in `.bss` (no heap in the chunk-streaming hot path), fed chunk-by-chunk, finalised on commit, constant-time-compared to the client-supplied digest. MD5 is retained as a deprecated legacy path for one release runway so clients can ship SHA-256 support without a synchronised deploy — MD5 will be removed in the next firmware release. Protocol contracts updated: `docs/protocol/k1-ws-contract.yaml` (`ota.begin`/`ota.verify` gain `sha256` field, mark `md5` deprecated) and `docs/protocol/k1-rest-contract.yaml` (`X-OTA-SHA256` required header; `X-OTA-MD5` deprecated). **Client migration required**: iOS (`lightwave-ios-v2`) uses `CryptoKit.SHA256`, Tab5 (`tab5-encoder`) uses mbedtls SHA-256 (already linked), web dashboard (`lightwave-dashboard`) uses `crypto.subtle.digest('SHA-256', ...)`. Do NOT flash the new K1 firmware to production before client fan-out — unsigned legacy clients will fail the new MD5-or-SHA256 requirement cleanly but users will see OTA errors. Phase-2 signed-boot (`CONFIG_SECURE_BOOT` + `CONFIG_SECURE_SIGNED_APPS` + eFuse burn + offline key custody) documented but deferred as a separate captain-gated project — notes in `firmware-v3/docs/p1-10-ota-hash-captain-decision-2026-04-18.md`.

### Removed
- **firmware:** Quarantined four heavy-compute effects from the registry and cycle order pending a render-budget rewrite — each was reproducibly triggering task-WDT resets on `loopTask (CPU 1)` under sustained 20 Hz effect cycling before the root-cause yield discipline landed. Source files are retained so the effects can be re-registered once their render/init paths fit the 2.0 ms budget:
  - `LGPChimeraCrownEffect` (EID 0x1900) — Kuramoto-Sakaguchi nonlocal coupling render busts the budget; 8 × TWDT resets in a 300 s burst
  - `KuramotoTransportEffect` (EID 0x1501) — 80-oscillator RK2 integration + nonlocal coupling per-frame; 7 × TWDT resets in one round
  - `LGPLorenzRibbonEffect` (EID 0x1903) — Lorenz ODE trail + radial projection
  - `LGPTalbotCarpetEffect` (EID 0x1800) — Fresnel harmonic sum render overruns; 8 × TWDT resets in one round
- Post-root-cause-fix (RendererActor yield discipline + 10 s loopTask TWDT), these effects no longer crash the system. They can be re-enabled individually by uncommenting the `renderer->registerEffect` call in `CoreEffects.cpp` and restoring their entries in `display_order.h` + `PatternRegistry.cpp` once their render paths are profiled and brought under 2.0 ms/frame.

### Fixed (previous)
- **firmware:** REST EdgeMixer mode validation rejected Triadic (5) and Tetradic (6) — `V1ApiRoutes.cpp` validated `mode > 4` instead of `mode > 6`
- **firmware:** WS speed validation capped at 50 instead of 100 — `WsEffectsCodec.cpp` `decodeSetSpeed` and `parameters.set` used stale range (1-50) while REST and RendererActor use extended range (1-100)
- **firmware:** `ZoneConfigManager.h` `MAX_SPEED` was 50, misaligned with `RendererActor.h` and `ZonePresetManager.h` (both 100)
- **firmware:** SerialCLI `zs` command validated speed 1-50 instead of 1-100
- **docs:** `api-v1.md` EdgeMixer section documented 5 modes (0-4); now documents all 7 modes (0-6) including Triadic and Tetradic
- **docs:** `api-v1.md` effects pagination documented `start`/`count` params; actual implementation uses `page`/`limit`/`offset`
- **docs:** `api-v1.md` transition `toEffect` documented as uint8 (0-46); corrected to uint16
- **docs:** `api-v1.md` parameters section missing `hue`, `mood`, `fadeAmount` fields; added with correct ranges
- **docs:** `api-v1.md` device status example showed STA mode (`apMode: false`, `192.168.1.100`); corrected to AP-only reality
- **docs:** `api-v1.md` presets section falsely claimed handlers are stubs returning NOT_IMPLEMENTED; disclaimer removed
- **docs:** `api-v1.md` speed range documented as 1-50; corrected to 1-100
- **docs:** `k1-ws-contract.yaml` speed ranges updated from 1-50 to 1-100 for `setSpeed` and `parameters.set`
- **docs:** `k1-rest-contract.yaml` EdgeMixer section expanded with parameter ranges and all 7 mode descriptions

### Added
- **firmware:** Shared gradient rendering kernel — GradientRamp, GradientCoord, GradientTypes under `effects/gradient/`. Centre-origin, dual-edge, 8-stop ramps with clamp/repeat/mirror modes, linear/eased/hard-stop interpolation, stack-allocated (~35 bytes)
- **firmware:** Retrofitted LGPPerceptualBlendEffect with 3-stop eased gradient ramp — proves multi-stop palette composition
- **firmware:** LGPGradientField effect (0x1F00) — operator-surfaced gradient proof with 6 parameters: basis, repeatMode, interpolation, spread, phaseOffset, edgeAsymmetry. Dirty-flag ramp rebuild.
- **firmware:** Colour correction skip for LGP_GRADIENT_FIELD (gradient ramps require uncorrected output)
- **firmware:** K1 coordinate helpers — uCenter(), uSigned(), uLocal(), edgeId(), halfIndex(), writeCentrePairDual() with correct 79.5 midpoint
- **firmware:** Colour correction skip for gradient-sensitive effects (EID_LGP_PERCEPTUAL_BLEND)
- **docs:** Gradient system design doc — coordinate model, API reference, validation scenes, forbidden patterns
- **ios:** EdgeMixer card on Play tab — 7 colour harmony modes, spread/strength sliders, spatial mask and temporal modulation pickers
- **ios:** EdgeMixer WebSocket integration — `edge_mixer.get/set/save` commands with 150ms debounce and echo prevention
- **ios:** EdgeMixer status broadcast sync — passive multi-client state updates from periodic device status
- **firmware:** FFT onset detector — 1024-point spectral flux (Bello/Dixon/Boeck), activity-gated, ~400us/hop, zero heap [quarantined]
- **firmware:** Band-energy ratio detector — variance-adaptive percussion triggers on grouped bands (Patin/WLED) [WIP]
- **firmware:** First-class onset API surface — OnsetContext with 6 semantic channels (beat, downbeat, transient, kick, snare, hihat) [WIP]
- **firmware:** OnsetSemantics module — extracted onset channel tracking from RendererActor into testable free functions
- **firmware:** AudioReactivePolicy TriggerMode — 7 modes with metronome fallback for effect trigger routing
- **firmware:** Onset capture telemetry — CaptureStreamer v2 bytes [26:32] carry onsetEnv, onsetEvent, percussion triggers
- **firmware:** EsV11Backend sample history accessor — getSampleHistory()/getSampleHistoryLength() for raw PCM access
- **tools:** Onset capture tooling — led_capture.py onset parsing, analyze_beats.py onset statistics, 2 capture suite scripts
- **docs:** Onset detector spec, quarantine matrix, ADR, WLED/aubio/essentia research references
- **tools:** Integrated RTK v0.34.2 token compression proxy — compresses Bash command output (git, builds, file listings) before reaching LLM context, reducing token consumption by 50-80% on CLI operations

### Reverted
- **firmware:** LGPRadialRippleEffect restored to original — gradient retrofit replaced sin16() brightness with triangular ramp and collapsed hue range from 64 to 4 steps
- **firmware:** LGPChromaticShearEffect restored to original — gradient retrofit reduced centre dimming from 50% to 30% with different curve shape

### Changed
- **firmware:** Silence gate uses raw PCM RMS instead of post-AGC band average — fixes false gate reopening during silence
- **firmware:** Silence hysteresis reduced from 8000ms to 150ms — responsive musical drops with ~550ms silence-to-black
- **firmware:** AGC noise floor lowered from 0.01 to 0.001 — only disables during electrical silence, not quiet passages
- **firmware:** Waveform effects (SbK1Waveform, SbK1WaveformHybrid) — RMS brightness scaling, silentScale gating, accelerated trail decay
- **firmware:** LittleFS.begin(true) — auto-format on blank/corrupted partition instead of silent failure
- **firmware:** BeatPulseBloom, BeatPulseBreathe, RippleEsTuned migrated to onset API trigger routing [WIP]

### Added
- **firmware:** EdgeMixer two new colour harmony modes — Triadic (120° hue shift) and Tetradic (90° hue shift); spread controls saturation blend (0=full sat, 60=70% sat)
- **tab5:** EdgeMixer mode button cycles through all 7 modes including Triadic and Tetradic
- **ESP32-P4 Platform Support**: Full audio capture and LED control on Waveshare ESP32-P4-WIFI6
  - ES8311 audio codec integration via I2S standard driver
  - Dual WS2812 LED strips (320 LEDs total) via RMT peripheral
  - ESP-IDF v5.5.2 toolchain with RISC-V GCC
  - Build scripts: `build_with_idf55.sh`, `flash_and_monitor.sh`

### Changed
- **Audio Pipeline Cadence Alignment** (ESP32-P4): Fixed timing mismatch between hop rate and scheduler
  - Changed HOP_SIZE from 128 to 160 samples (8ms → 10ms) to match FreeRTOS 100Hz tick
  - Hop rate now perfectly aligned: 160 samples @ 16kHz = 10ms = 100 Hz = 1 tick
  - Eliminates DMA timeouts, watchdog triggers, and erratic capture rates
  - See `docs/AUDIO_PIPELINE_CADENCE_FIX.md` for full technical analysis

- **DC Blocker Coefficient**: Corrected from 0.9922 to 0.992176 using proper formula
  - Formula: R = exp(-2π × fc / fs) where fc=20Hz, fs=16000Hz
  - Ensures accurate DC removal without affecting audio content

- **ES8311 Microphone Gain**: Added explicit 24dB gain setting
  - Signal levels were ~0.2% of full scale (-54dB), now properly amplified
  - Available gains: 0dB to 42dB in 6dB steps

- **Spike Detection Improvements**:
  - Added noise floor check (0.005 threshold) to skip detection on quiet signals
  - Raised warning threshold from 5.0 to 10.0 spikes/frame (20 bins checked per frame)
  - Prevents false warnings from noise-floor fluctuations

- **I2C Probe for ES8311**: Changed to low-level ACK check using `i2c_cmd_link` API
  - Fixes intermittent probe failures with `i2c_master_write_read_device()` 0-length read

### Fixed
- **AudioActor FreeRTOS Tick Conversion**: Added `LW_MS_TO_TICKS_CEIL_MIN1()` macro
  - `pdMS_TO_TICKS(8)` returns 0 at 100Hz ticks (8ms < 10ms tick period)
  - New macro uses ceiling division with minimum of 1 tick

- **Self-clocked Mode Stability** (retained but unused): Actor.cpp now supports tickInterval=0
  - Caused watchdog triggers due to IDLE0 starvation - not recommended for audio
  - Kept for potential future use cases with proper yield points

---

## [Previous Unreleased] - Light Guide Plate Feature

### Added
- **Light Guide Plate Mode**: Revolutionary optical waveguide display system
  - Dual-edge LED injection into 329mm acrylic plate
  - Advanced interference pattern effects using wave physics
  - Depth illusion system with volumetric display capabilities
  - Physics simulations: plasma fields, magnetic field lines, particle collisions
  - Interactive features: proximity sensing, gesture recognition, touch-reactive surfaces
  - Advanced optical effects: holographic patterns, energy transfer visualization

### Technical Documentation Added
- `docs/LIGHT_GUIDE_PLATE.md`: Comprehensive 240+ line technical specification
  - Physical configuration and hardware specifications
  - Optical theory and wave interference mathematics
  - 5 major effect categories with detailed implementations
  - Performance optimization strategies and memory management
  - 6-phase implementation roadmap with weekly milestones
  - Testing and validation frameworks
  - Future enhancement possibilities

### Architecture Enhancements
- Light guide effect base classes and coordinate mapping systems
- Interference calculation framework with real-time optimization
- Edge-to-center coordinate transformation algorithms
- Specialized synchronization modes for optical effects
- Performance-optimized interference pattern storage

### Configuration Extensions
- New feature flags for light guide mode detection
- Hardware configuration for dual-edge LED control
- M5Stack encoder integration for light guide parameters
- Compile-time optimization for optical calculations

### Effect Categories Planned
1. **Interference Pattern Effects**
   - Standing wave patterns with mathematical precision
   - Moiré interference from overlapping frequencies
   - Constructive/destructive zone visualization

2. **Depth Illusion Effects**
   - Volumetric display simulation with apparent 3D objects
   - Parallax-like effects using edge intensity control
   - Z-depth mapping with atmospheric perspective

3. **Physics Simulation Effects**
   - Plasma field visualization with realistic physics
   - Magnetic field line rendering using dipole equations
   - Particle collision chamber with momentum conservation
   - Wave tank simulation with proper propagation physics

4. **Interactive Applications**
   - Proximity detection using light occlusion analysis
   - Gesture recognition from shadow pattern changes
   - Touch-reactive surfaces with ripple effects
   - Real-time data visualization framework

5. **Advanced Optical Effects**
   - Edge coupling resonance with feedback loops
   - Energy transfer visualization with conservation laws
   - Holographic interference pattern generation
   - Interactive light-based games (optical pong, light tennis)

### Performance Specifications
- Maintained 120 FPS target with complex interference calculations
- Optimized memory usage with efficient pattern storage
- Real-time interference calculation using ESP32-S3 dual cores
- Temporal coherence optimization for smooth animations

### Future Integration Points
- Machine learning for advanced gesture recognition
- Multi-unit synchronization for large installations
- Camera integration for computer vision applications
- Advanced physics: quantum mechanics and relativistic effects

### Changed
- Web control plane refactor (robustness-first):
  - Replaced ESPAsyncWebServer/AsyncTCP web stack with ESP-IDF `esp_http_server` backend (REST + WebSocket) in WiFi environments.
  - Default build (`esp32dev`) no longer compiles any web stack dependencies.
  - JSON handling remains cJSON-only.
  - Feature flags: `FEATURE_WEB_SERVER`, `FEATURE_WEBSOCKET`, `FEATURE_OTA_UPDATE` now default to OFF unless enabled via PlatformIO env build flags.

### Fixed
- Eliminated cross-platform dependency leakage in ESP32 builds (no ESP8266/RP2040 async TCP libraries pulled into ESP32-S3 builds).
- **Audio Gate Responsiveness**: Fixed activity gate closing on valid audio signals
  - Lowered default `gateStartFactor` from 1.5 to 1.0 (more permissive threshold)
  - Fixed hardcoded noise floor rise rate (now uses tunable `noiseFloorRise` parameter)
  - Increased SNR threshold from 2.0 to 3.0 to prevent floor drift during active audio
  - Added automatic recovery mechanism: forces noise floor down when gate stuck with signal present
  - Audio-reactive effects now respond correctly to normal audio levels
  - See `docs/audio-visual/audio-gate-fix-2025-01.md` for comprehensive documentation

---

## Previous Releases

### [Phase 1] - LED Strips Mode Implementation
- Added dual 160-LED strips infrastructure
- M5Stack 8encoder I2C integration
- 12 strip-specific advanced effects
- Propagation and synchronization modes

### [Base System] - Core Architecture
- Modular effect system with base classes
- PlatformIO project structure
- Performance monitoring and optimization
- 16 core visual effects
- 33 color palettes
- Smooth transition system