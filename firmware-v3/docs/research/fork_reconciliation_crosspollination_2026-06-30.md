---
abstract: "Forensic timeline + full planned/proposed-feature inventory of the dormant Lightwave-Ledstrip monorepo (firmware-v3, last commit 2026-05-19) vs the active SpectraSynq_K1_Firmware repo (last commit 2026-06-28), plus the cross-pollination matrix and 2nd/3rd-order considerations. LOAD-BEARING REFRAME: the two are NOT branches of one codebase — they are independent Sensory Bridge (GPL-3.0) descendants on the SAME board (ESP32-S3 N16R8) but different software stacks (IDF 4.4.7/RMT4/ESV11 vs IDF 5.4.1/RMT5/GDFT). No git merge is possible; only idea/methodology ports. The real decision is which line is the canonical K1 launch firmware. Read before any reconciliation, port, or 'resume development' work."
---

# Fork Reconciliation & Cross-Pollination — Lightwave-Ledstrip ⇄ SpectraSynq_K1_Firmware

**Authored:** 2026-06-30 · **Method:** 4-agent SSA swarm (main-timeline, main-proposals, fork-inventory, cross-repo-diff) + orchestrator verification of decision-critical claims.
**RBDO label:** GROUNDED on lineage/timeline/board/perceptual-status (orchestrator-verified); DEGRADED-MODE on the canonical-line *recommendation* (depends on two Captain product facts — see Part D).

---

## 0. Headline reframe (read first)

The Captain's framing — "we forked development into SpectraSynq_K1_Firmware, find cross-pollination" — is colloquially true but technically misleading, and the difference changes the whole answer:

- **`SpectraSynq_K1_Firmware` is NOT a git fork of this monorepo.** Root commit `ed56dfb` (2026-06-23) is a *clean-slate rebrand of the Sensory Bridge working tree* (`SensoryBridge-main 9`). Remote: `synqing/SpectraSynq_K1_Firmware`.
- **Both repos are independent descendants of Sensory Bridge** (Connor Nishijima / Lixie Labs, **GPL-3.0**). They share only the Goertzel/GDFT DSP ancestor, reached by two separate paths. The fork **deliberately discarded** the `Lightwave-Ledstrip/` sub-project that sat inside the SB tree.
- **Same hardware, different stack.** Both target `board = esp32-s3-devkitc1-n16r8` (ESP32-S3, 16 MB QIO-OPI PSRAM) — orchestrator-verified in both `platformio.ini` files. The divergence is entirely software:

| Axis | Monorepo `firmware-v3` (dormant 2026-05-19) | Fork `SPECTRASYNQ_K1_FIRMWARE/` (active 2026-06-28) |
|---|---|---|
| Board | ESP32-S3 N16R8 | ESP32-S3 N16R8 (**same**) |
| IDF / framework | ESP-IDF **4.4.7**, custom platform, RMT4 | ESP-IDF **5.4.1**, pioarduino/arduino-esp32 3.2.0, RMT5 |
| Audio | ESV11 @ **32 kHz / 125 Hz**, ControlBus shared struct | GDFT @ **133 Hz / 12.8 kHz / 96-sample**, AudioSemanticState snapshot |
| Concurrency | FreeRTOS **actor model** (AudioActor/RendererActor) | SB dual-core loop + `director/`+`control/` facade |
| Effects | **100+** class-based `EffectBase`/LGP* effects | ~30 SB `light_mode_*` + emerging `effects/framework` ZoneComposer |
| Control surface | ESP-NOW + WiFi + **REST/WS + iOS app** | **BLE-MIDI "Remoted"** + AP frontend |
| Process substrate | RBDO ledger, native_test_*, MabuTrace | **"harness IS the product"**: golden-master oracle + Gate-0 fault-injection + CI |
| Perceptual status | Mature, hardware-attested (F-6 Zone AGC) | **RED — shipping visual failed its own A/B (2026-06-21)** |

**Consequence:** a git cherry-pick is impossible in either direction (no shared history, disjoint layouts, different IDF major, different effect + concurrency models). **Every "port" is a manual re-implementation.** Cross-pollination is real only at the **idea / methodology / asset** level, not the code-merge level.

**The decision this forces (Captain's, not an agent's):** which line is the canonical K1 launch firmware? See Part D. Everything else is downstream of that.

---

## Part A — Forensic development timeline

### A.1 Monorepo `firmware-v3` arc (all dates 2026; HEAD `08532b0a`)

| Phase | Window | Theme | Key commits | Outcome |
|---|---|---|---|---|
| P1 | 05-03→05-07 | VP show-timing / palette | `7c867df4`, `0d2a8ded`, `627b0b14` | VP timing instrumented (peripheral) |
| P2 | 05-01→05-02 | Zone wire-format + heap foundation | `d53092ad` (zoneId 0→1), `2fe2f289` (per-zone pool), `0cc34478`/`56110887` (heap-shed latch) | Zone addressing migrated; heap-shed latch later implicated in heap pressure |
| P3 | 05-13→05-17 | SongAware→SynqMatrix + Director authority | `d1d7b807`, `06cc6e76` (authority inversion), `0e2e0778`, `f94a0ac9` (ZoneComposer under Director) | SynqMatrix Director shipped, mic-verified |
| P4 | 05-16→05-17 | **F-5 WiFi dual-mode** | `ab8ef0ae` (strip WIFI_AP_ONLY → runtime), `44d2f557`, `f8897cc2` (NVS boot mode), `0987ba67` | Dual-mode shipped; concurrent AP+STA still forbidden (IDF 4.4.7) |
| P5 | 05-06→05-19 | **F-6 ControlBus Zone-AGC 4→3** | `b28113ae` (audit), `53483f3a` (brief), `08a7c997` (**behavioural repartition**), `bb8dea8a` (hardware-attested) | 3-zone partition shipped + hardware-validated |
| P6 | 05-18→05-19 | **Heap-pressure witchhunt + Phase-1A** | `9fa8bd25` (5-SSA diagnosis), `3770863e` (survival bundle), `3e0e40d8` (disable 1 Hz periodic.scalar) | hex-ID NOT the cause; app heap structurally undersized; Phase-1A landed |

**Where it stopped (2026-05-19):** last three commits are docs/diagnostics, not behavioural firmware. Last *behavioural* commit = `08a7c997` (F-6 repartition). **Last flashed = `3e0e40d8`** (K1v2 MAC `b4:3a:45:a5:87:f8`). Last validated env = `esp32dev_audio_esv11_k1v2_32khz`. F-6 was closed *and* F-6.1 (perceptual calibration gate) opened in the same act.

**The literal unfinished edge:** the working tree is **dirty with uncommitted WIP** — a **Transition runtime-config / REST-transition-contract** subsystem (untracked `TransitionRuntimeConfig.cpp/.h`, `test/test_transition_contract/`, plus modified `TransitionTypes.h`, `TransitionHandlers.*`, `V1ApiRoutes.cpp`, `k1-rest-contract.yaml`, etc.). Never committed. This is the last-edited thread and is **absent from BACKLOG and CHANGELOG**.

### A.2 Fork `SpectraSynq_K1_Firmware` arc (2026-06-23→06-28, 102 commits; HEAD `a11f2ed`)

Git history is 5 days, but the working tree carries months of prior SB-K1 work (`progress.md` from 2026-05-25). **Gate-paced, not date-paced.**

| Phase | Theme | Key commits |
|---|---|---|
| 0 | Fork init + RATIFIED modernization program | `ed56dfb`, `85f0496` |
| F | **Fail-proof harness** — L1 golden-master oracle + Gate Fα self-test, L0 CI, multi-oracle registry | `02b7975`, `4fef040`, `d520e72`, `34ddbf6` |
| A·L1 | GDFT decomposition + int64 stabilisation | `8467d9c`, `bac2fa7`, `a94c069`, `03d74f3` |
| — | **Serial-menu strangler-fig** (6191-LOC god-header → modular TUs, 13 families, LOCK→EXTRACT gate, PRs #1–#12) | `94b7c8b`, `82ff727`, `ea6f3b5`/`f180eb1`, … |
| — | Bridge-FS CRC32 config + I2S-watchdog state lock | `4e18810`, `479701d`, `2a3b7e7` |
| — | **BLE Remoted control map** (newest, 06-26→06-28) | `50bd207`, `31e5736`, `476b4ec`, `a11f2ed` |
| — | **Gate-0 fault-injection** A/B harness + admission gates U1–U7 | `d4de505`, `1e19ddc`, `b5c38c5`, `4b2d35a` |

**Newest active workstream:** BLE Remoted control-map decoding. **Production verdict (2026-06-26 Reality-Checker):** engineering-stable baseline, **NOT production-grade** — of 8 dimensions only 1 green (harness/CI).

---

## Part B — Master planned/proposed feature & enhancement inventory

### B.1 Monorepo — open / proposed (DONE items omitted; full status in source)

**Work Blocks:** WB-1 naming/metric audit (PARTIAL) · WB-2 FastLED/RMT transport visibility (PARTIAL — needs hardware TX-boundary proof).

**Upstream Calibration Debt (RBDO ledger):** C-1 mic operating envelope (MEASURED-DEGRADED) · C-2 feature×effect×dwell matrix (DONE-DEGRADED) · C-5 per-effect observables (DONE-DEGRADED; RTS-4 poor, needs replacement) · C-7 LGP perceptual JND floor (MEASURED-DEGRADED). [C-3/C-4 resolved; no C-6.]

**F-series gates:** F-1..F-4 DECIDED · **F-5** dual-mode WiFi (T1/T2 DONE, **T3 AP captive-portal owed, T4 multi-router STA owed**) · F-6 DONE (hardware-attested) · **F-6.1 Zone-AGC output-intensity acceptance — OPEN P0 quality gate before FE launch** (protocol defined, not run).

**Performance / Observability (open):** Heap Phase 1B (6 soak-gated steps: AsyncWebSocketSharedBuffer→PSRAM, EXT_RAM_BSS statics, stack trim, PSRAM JsonDocument allocator — **soak never run**) · pre-existing render/audio P0 pathology (render p99 ~9.2 ms, audio_hop p99 ~17.9 ms) · MabuTrace Surface 6/9 + Tier-2 spans · ControlBusFrame hot/cold split.

**Synergy-Topology programme:** Phases 0–3 + Phase 4.1/4.2/4.4 DONE; **Phase 4.3 F3 "Liquid Stillness" OWED** (gated on Captain selecting 8–12 ambient programmes); Phase 5 exemplars DONE-DEGRADED (sign-off blocked by RTS-4 + missing Captain visual answers; BPS visually unsatisfactory).

**Pathmode programmes:** 05/07 DONE; 03 latency per-stage pending; 04 silentscale move pending; **06 render-contract-enforcement NOT STARTED · 09 reliability-core NOT STARTED · 10 manufacturing-and-OTA NOT STARTED**.

**Proposed / in-flight features:**
- **Transition System** (12 centre-origin transitions) — **UNCOMMITTED working-tree WIP**, undocumented in BACKLOG/CHANGELOG. *Newest, most at-risk.*
- **Zone Composer Instrument** — Phase 0 closed; D-1 per-zone pool prototype GATED (`K1_ZONE_INSTANCE_POOL_ENABLED=0`), hardware validation pending; D-3 blocked; 10 zone REST endpoints return HTTP 501; zoneId 0→1 wire migration backlogged.
- **SynqMatrix / Song-Aware Director** — director bugs RESOLVED; V1 tempo telemetry source owed; restore-point latch consolidation DEFERRED (**revisit-by date = today, 2026-06-30**).
- **Zone Mixer Controller** (auto-memory) — AtomS3+PaHub DJ-mixer, 26-SSA research, PROPOSED not built.
- **STM suite** (spectral-temporal modulation, 128-band, WS binary stream) — LANDED.
- **Async RMT 120 fps** (FastLED 3.10.0 RMT4 non-blocking, 119 FPS) — LANDED.
- **iOS parity** — Phase 1 landed; Phase 2 (effect picker, runtime param UI, STM/VRMS viz, presets/shows) unblocked by F-1..F-4 decisions.
- **Voice SR Gate 4** (auto-memory) — 2/4/8-command rollout via MultiNet5, PROPOSED.
- **WebServer refactor** Phases 4–6 (test mocks HIGH).
- **Landing page pipeline** — 5 variants, video spec, 6 Captain decisions pending.
- **Tab5** — PSRAM preset storage, NVS health, 3-zone centre-origin layout, EdgeMixer preset save/restore — LANDED.
- Inference task placement (DSP/heuristic/ML) — RESEARCH/DESIGN only · VP reform substrate (Layer 4/5) landed-but-unwired · TRAIL FADE param decision open.
- **TODO/FIXME (firmware-v3/src):** 11 real items — WS log/FFT streaming (×3), MessageBus for AudioActor HEALTH/PONG, MotionSemantics wiring (×2), P4 LED power-limiting, PluginManager↔WebServer UI, NarrativeHandlers NVS persistence, LGPSpectrumDetailEnhanced bins256 migration, zone regression gates I1/I2/I4 stubs.

### B.2 Fork — forward backlog (production-readiness lane, 2026-06-26)

Classed by autonomy (A host-complete · B build+device-proof · C Captain decision · D external-blocked):
- **N2 (B, P0 highest):** I2S `portMAX_DELAY` unrecoverable audio-core freeze + **no task watchdog anywhere** → dead unit needs physical power-cycle. Fix = bounded timeout + WDT subscribe/feed.
- **N1 (A, P1):** CONFIG-load integrity (raw memcpy, no magic/version/CRC) — partly addressed by `479701d` bridge-fs CRC32.
- **N3 (A, P1):** scrub `-DK1_CONTROL_TOKEN="k1-tab5"` fleet-wide static secret baked into prod binary.
- **N4 (A, P1):** manufacturing/provisioning — upload guard hard-codes 2 USB serials; no factory image / per-unit identity. **Not manufacturable.**
- **N5 (A, P2):** release engineering — 0 git tags, FIRMWARE_VERSION not tag-linked, no build provenance.
- **N6 (C, P0 product-existential):** **shipping visual failed its own perceptual A/B (2026-06-21).** Prepare A/B package only; Captain owns the look.
- **N7 (C):** OTA · **N8 (D):** hardware recovery buttons · **N9 (C):** GDFT int32-overflow promotion — **fix built + device-proven but HELD** ("no product value", 2026-06-21); overflow **zeros spectral bins on loud audio = the demo condition**, may share root with N6.
- **Modernization program forward gates:** Gate Fα achieved; Phase A god-header decomposition in progress; Phase P (IDF+CMake dual-build parity), device eyes-on gates, L6 retail (secure boot, flash encryption, OTA) PENDING.
- **Research-only:** full 16 kHz AP path blocked by `sb_compute_acf_salience()` cost (p95 loop >7.5 ms). Production baseline locked at 12800/96/d3.

---

## Part C — Cross-pollination matrix (idea/asset/methodology, NOT code-merge)

### C.1 Do-regardless-of-canonical-decision (HIGH value, LOW friction)

| # | Asset | Direction | Why it matters | Effort |
|---|---|---|---|---|
| 1 | **Device-build registry / chip-ID upload guard** (refuse cross-flash) | fork→mono | Directly fixes the monorepo's **documented wrong-device-flash incident (2026-03-24)**. Concept is near drop-in. | LOW |
| 2 | **"Harness IS the product"** — golden-master oracle + **Gate-0 fault-injection** + CI commit-gate | fork→mono | Monorepo's `native_test_*` is weaker; this mutation-proven anti-gaming harness is **reusable machinery**, host-side pytest, largely platform-agnostic. Biggest transferable safety asset. | MED |
| 3 | **Bridge-FS CRC32 config integrity** | fork→mono | Both persist config; pattern ports cleanly. | LOW |
| 4 | **Fork's IDF 5.4.1 success retires the monorepo's "IDF5 BLOCKED" memory** | fact | Monorepo treated IDF5 as a wall (I2C bug + API rewrites). The fork **runs IDF 5.4.1 on the same board** → the migration path is now de-risked. Update that stale memory either way. | n/a |

### C.2 Canonical-dependent (MED value, HIGH friction — manual re-implementation)

| Asset | Direction | Trigger |
|---|---|---|
| **100+ LGP effect library + F-6 Zone AGC + 12 transitions + perceptual-A/B methodology** | mono→fork | If fork is canonical — this is **the** port that closes the fork's N6 perceptual RED. |
| **BLE-MIDI "Remoted" stack** | fork→mono | If monorepo is canonical AND BLE is the chosen control surface. |
| **iOS companion app + REST/WS** | mono→fork | If fork is canonical AND app-control is wanted (conflicts with the BLE bet — see UF2). |

### C.3 Methodology / governance (port the discipline, not the bytes)

- **Autonomy taxonomy (Class A/B/C/D)** ⇄ monorepo's **RBDO + Founder Execution Boundary** — two governance systems that say the same thing; converge into one.
- **LOCK→EXTRACT strangler-fig** discipline — apply to the monorepo's god-files (WebServer refactor Phases 4–6).
- **Pacing by gates not dates** + behaviour-changing fixes on a separate ticketed track — already half-present in monorepo's hardware-test-before-commit rule.

---

## Part D — The canonical-line decision (Captain's call)

**Current state:** Two parallel Sensory-Bridge-derived K1 firmware lines on the **same board**, complementary in exactly their weaknesses. Fork = modern stack (IDF5) + safety/ops substrate + BLE + GPL-clean, but **perceptually RED and not production-grade**. Monorepo = perceptual/effect maturity + iOS app + bigger shipped surface, but **dormant, older stack (IDF4.4.7), weaker process substrate**.

**Upstream facts that make the decision decidable** (per the Captain-decision-menu rule — no menu before the facts):

- **UF1 — same hardware?** ✅ RESOLVED by orchestrator: **yes**, both `esp32-s3-devkitc1-n16r8`. No hardware blocker to consolidation.
- **UF2 — intended launch control surface: BLE-MIDI "Remoted" (fork bet) or iOS-app + REST/WS (monorepo bet)?** *Product-defining; unresolved.* This is the deepest divergence and the strongest determinant.
- **UF3 — does IDF 5 viability change the calculus?** The fork **proves IDF5 works on this board** → the monorepo's IDF4.4.7 is a launch-longevity liability, and migrating it is now de-risked (but still real work).
- **UF4 — GPL-3.0 commercial posture for Kickstarter.** The fork has **clean SB attribution**; the monorepo *buried* SB under a custom platform. For a commercial launch this is a latent IP/compliance question (legal — flag, do not resolve here).

**Options (decidable once UF2 is answered):**

- **Option A — Fork canonical, monorepo = perceptual+feature donor (recommended).** Keep building on the fork (modern stack, harness, BLE, GPL-clean — all expensive to retrofit into a dormant tree). Manually port the monorepo's effect library + Zone AGC + transitions + perceptual-calibration methodology onto the fork's golden-master-gated spine to close N6. *You keep the safety substrate and re-earn perceptual maturity under the harness.*
- **Option B — Monorepo canonical, fork = process+stack donor.** Revive the monorepo (more shipped surface: iOS, transitions, STM, Tab5, 100+ effects), back-port the harness discipline + the now-de-risked IDF5 migration. *Faster to feature-complete if BLE is NOT the product; but carries a weaker safety net into a fleet and an IDF migration.*
- **Option C — Both stay live (status quo).** **Not recommended** — two trees both claiming "K1 firmware" is the exact canonical-source false-propagation hazard the memory-discipline doctrine warns against (the PipelineCore anti-pattern at repo scale). Doctrine has *already* forked (two 44 KB AGENTS.md/CLAUDE.md). Drift compounds.

**Recommendation (DEGRADED-MODE — gated on UF2):** **Option A.** The fork already won the hard-to-rebuild things (modern stack on the same silicon, harness-is-product safety net, GPL clarity, BLE). The monorepo already won the *other* hard-to-rebuild thing (perceptual + effect maturity + the RBDO calibration ledger). The fork's single P0-existential gap (N6 perceptual) is precisely the monorepo's strength. For a *visual* product, perceptual maturity is the capital you least want to rebuild — so make it the donor asset, not the discarded one.
- **Unresolved assumption:** UF2 — that BLE-MIDI "Remoted" (not the iOS app) is the launch control surface. If the iOS app is the product, Option B's larger app-side surface tilts the call.
- **Risk if wrong:** porting effects fork-ward is wasted; the monorepo's iOS/REST investment is stranded.
- **Fallback:** Option B (monorepo canonical) — back-port harness + IDF5.
- **Revisit trigger:** Captain answers UF2 (control surface) and confirms UF4 (GPL posture).
- **Debt count:** this recommendation gates the cross-pollination *direction* of Part C.2 (3 assets).

**Default action if Captain does not override:** treat the **fork as go-forward**, execute only the **C.1 direction-agnostic ports** (device-build guard, harness, CRC32 config) plus update the stale IDF5 memory — and **hold all C.2 manual re-implementations** until UF2 is answered. No code is touched in either tree without that answer.

---

## Part E — Verification & confidence notes

- **Orchestrator-verified** (not single-agent): fork perceptual fail (`production-readiness-lane.md:23`), N9 GDFT-overflow-HELD smoking gun, fork root commit/lineage (`ed56dfb` + CHANGELOG), and the **board-equality correction** (cross-repo-diff agent wrongly claimed different boards; both are N16R8).
- **HIGH confidence:** all timelines, the lineage truth, the proposal inventories, the impossibility of git-level merge.
- **MEDIUM confidence:** fine-grained DSP-constant portability between ESV11-32 kHz and GDFT-133 Hz (not diffed line-by-line); the six monorepo auto-memory project files (read via MEMORY.md index hooks, per the standing memory-trust-freeze caveat).
- **Stale-source flag:** BACKLOG.md F-5 still says "production ships AP-only via WIFI_AP_ONLY" — **contradicted** by current source + CLAUDE.md hard constraint (dual-mode shipped 2026-05-17, `ab8ef0ae`). Current source is authoritative; BACKLOG F-5 line should be corrected.

---

## Update — 2026-06-30: two de-risking spikes + the corrected launch picture

Two follow-on SSAs (DSP root-cause + effect-port feasibility) + orchestrator verification materially **corrected** the earlier framing. Net effect: the canonical call hardens, the launch-visual fix gets far cheaper, and the "monorepo effect harvest" moves *off* the launch critical path.

### Correction 1 — the int64 overflow (N9) is a RED HERRING for the perceptual RED (was: flagged as the "smoking gun")
> **Superseded** — Part B.2 / Part D quoted the lane's "N9 may share a root with N6" hypothesis. It is **disproven on-device.** A 4-leg device A/B (chip B489A500) shows the "louder→dimmer" failure present in **both** the legacy and int64-corrected legs — fixing the overflow does **nothing** to the visual. The "no perceptual value" HELD label is correct. **Keep the int64 fix HELD; couple its re-test to the AGC lane below.**

### Correction 2 — the real cause is a single-scalar broadband AGC defect (cheap, self-contained, in the fork)
`k1_gdft_core.cpp:376` computes **one global** `target_gain`; `:384` applies that one scalar to **every** bin → loud broadband content crushes the whole field ("louder→dimmer"). Per-band gain is **scaffolded but unused at `:411`.** Telemetry agrees (gain 0.067 loud vs ~0.15 moderate). **Fix = reshape one gain path. No effect harvest, no migration.**

### Correction 3 — the "fork effects look worse" judgement is CONFOUNDED
The 12201-GDFT-vs-2101-v3 perceptual loss was measured **with the AGC defect intact**, conflating "worse effect design" with "AGC dimming the bench build." **You cannot conclude you need the monorepo's effects until you re-A/B *after* the AGC fix**, against the 2026-06-15 "show is good" baseline (not against effect-framework-v3).

### Finding 4 — the taste-donor path is already PROVEN and underway (independent validation of the canonical call)
The fork built a v3-compatible `K1AudioContext`/`sb_audio_snapshot` adapter exposing the **same accessor surface** monorepo effects consume, and **already ported 5 hero effects** through it (`effect_lgp_harmonic_tide`, `effect_beat_pulse_resonant`, …). The chroma A↔C origin bug is already found+fixed with static-asserts. Audio-feature mapping: most accessors are **DROP-IN or RE-MAP**; the genuinely expensive gaps (`bins256` linear FFT, `saliency`, `sceneParameters`, `styleConfidence`) have no fork equivalent → **scope the first 10 heroes to beat-reactive + geometry/palette effects to avoid that tier.** Budget the adapter gap-fill (`bpm`, `heavy_*`, `silentScale`, `hopSequence`) **once** in the adapter, not per-effect. Per-effect: beat-reactive ≈0.5–1 d; LGP-organic/quantum ≈1–2 d (re-tune dominated by GDFT-note-spaced vs Goertzel-octave spectral shape); translation/bins256 effects = multi-day, **exclude**.

### Verification — production V2 channels CONFIRMED
`[env:k1_hardware]` build flags define `-DSB_ONSET_V2`, `-DSB_CHORD_V2`, `-DSB_TEMPO_CONF_V2`, `-DSB_TEMPO_FLYWHEEL_V2` (orchestrator-verified in fork `platformio.ini`). Onset (kick/snare/hihat), chord, and tempo-V2 channels are **live in the shipping build** → effect-port fidelity is not gutted; the per-band data the AGC fix needs is present.

### The flip condition is RETIRED — fork-as-canonical is now robust
Earlier the call could flip to "revive Lightwave-Ledstrip" *if* porting effects to the fork were intractable. **It is tractable and already underway.** Therefore the flip cannot trigger regardless of UF2. **Decision: build forward on `SpectraSynq_K1_Firmware`; archive `Lightwave-Ledstrip` as a read-only taste-donor.** (If UF2 = app-primary, the fork *adds* a REST/WS+app path — a fork enhancement, not a reason to revive the monorepo.)

### Corrected two-lane plan
- **Lane 1 — LAUNCH CRITICAL PATH (cheap):** fix broadband AGC → per-band/reshaped gain (`:376/:384/:411`) → re-A/B vs the 2026-06-15 baseline. Makes the fork's *own* visuals acceptable. Needs: Captain approval of an AGC lane + audio-source approval for the A/B. **STATUS 2026-06-30 — PREPARED & COMMITTED:** fork branch `feat/n6-agc-perband` commit `9c19b1f` (not pushed; Captain-gated), flag `SB_AGC_PERBAND_V1` **default OFF**, production byte-neutral (gdft golden reproduces; 574 host tests pass; `k1_hardware` builds OFF *and* ON, +56 B RAM/+284 B flash), RED→GREEN property test + device A/B protocol at fork `docs/architecture/n6-agc-perband-prepare-package.md`. **Awaiting Captain hardware A/B (SPH0645).** Host proves the *mechanism* (per-band gains diverge: 0.863/0.770/0.431/0.246 vs OFF's uniform 3.26× collapse); perceptual *magnitude* is deliberately the device gate (Goertzel leakage may partially equalise bands).
- **Lane 2 — RICHNESS (parallel, NOT launch-gating):** continue the v3-effect harvest via `K1AudioContext`; gap-fill the adapter once; first 10 = beat-reactive + geometry/palette; exclude `bins256`/`saliency`/`sceneParameters`.
- **Settled (no Captain input):** fork canonical; int64 HELD (re-test coupled to AGC); monorepo archived as donor.
- **Captain decisions:** (1) AGC per-band lane — ✅ **GREEN-LIT 2026-06-30**; (2) re-A/B audio source — ✅ **SPH0645 MEMS mic (live capture), 2026-06-30**; (3) **UF2 RESOLVED 2026-06-30 — BLE-MIDI is the go-forward control surface; WiFi is dropped entirely**; (4) confirm the ~10 heroes when Lane 2 ramps.

### Update 2 — 2026-06-30: BLE-only decision (UF2 resolved) + consequences
Captain ruling: **BLE-MIDI on `SpectraSynq_K1_Firmware` is the control surface; we are no longer using WiFi.** Consequences:
- **Monorepo donor value collapses to *effects only*.** The monorepo's WiFi dual-mode (F-5), REST/WS API, and the entire iOS companion app are now **product-deprecated** — not cross-pollination candidates. The only remaining donor asset is the curated effect taste (Lane 2), which is itself deferred and possibly unneeded for launch. **`Lightwave-Ledstrip` is effectively fully archived.**
- **Fork simplification (2nd-order, favourable):** dropping WiFi retires an entire attack/complexity surface — the AP-frontend path, and it cleans up **N3** (the fleet-wide static `K1_CONTROL_TOKEN="k1-tab5"` baked into the binary was a WiFi/Tab5 control token; with no WiFi the WiFi-control path may be removable outright, not just scrubbed).
- **N4 manufacturing/provisioning becomes BLE-centric** (per-unit BLE identity/pairing, not WiFi credentials).

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-06-30 | agent:claude-opus-4-8 (orchestrator + 4-SSA swarm) | Created. Forensic timeline + proposal inventory + cross-pollination matrix + canonical-line decision frame. Reframed "fork" as independent Sensory Bridge sibling (not a branch); verified board-equality, perceptual-fail, and lineage first-hand. |
| 2026-06-30 | agent:claude-opus-4-8 (+ 2 de-risk SSAs) | Update: int64/N9 disproven as N6 cause (red herring); real cause = single-scalar broadband AGC (`k1_gdft_core.cpp:376/384`, per-band scaffold at :411); "fork effects worse" judgement confounded by AGC; taste-donor adapter already built + 5 effects ported (canonical call validated); production V2 flags verified; flip condition retired; corrected two-lane plan. |
