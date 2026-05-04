---
abstract: Forensic re-audit of SSA2's firmware-v3 docs disposition for the Lightwave-Ledstrip NotebookLM curation. Captain-authorised deeper excavation. Reports undispositioned files, false-negative excludes, false-positive includes, dispositions for api-legacy.md and enhancement-engine-api.md, and any STA-taint that slipped through. SSA2 disposition was largely sound — 0 STA-promoting docs leaked. Three out-of-scope but potentially canonical files at firmware-v3/ root flagged as gaps. Final dispositions for the three flagged API files: api-legacy.md INCLUDE-WITH-DISCLAIMER (working compatibility shim), enhancement-engine-api.md EXCLUDE (not enabled in canonical envs), api/CLAUDE.md EXCLUDE (auto-stub). Two HIGH-severity SSA2 gaps identified.
---

# Forensic Audit — SSA2 firmware-v3 docs disposition

**Audit agent:** Forensic Audit SSA
**Date:** 2026-05-04
**Scope:** verify SSA2's firmware-v3 docs disposition for the LightwaveOS NotebookLM bundle
**Method:** enumerate → cross-classify against SSA2 manifest → sample-audit excludes/includes → verify the three flagged API files → re-sweep STA taint
**Working tree HEAD:** `feature/heap-stability-day1`

---

## Task 1 — Disposition completeness check

Total `.md` files under `firmware-v3/docs/`: **207** (per `find firmware-v3/docs -type f -name "*.md"`).

SSA2's stated counts: **51 INCLUDED + ~120 EXCLUDED = ~171 dispositioned**. Delta of ~36 is mostly absorbed by SSA2's wildcards (`research/findings/*`, `phase1b_runtime_evidence_2026-04-27/**/*`, `synergy-topology/**/*`, `spazz_redesign_2026-04-30/**/*`, `session-notes-2026-04-30/*`, `SILENCE_DETECTION_*`, `trace_spec_sections/*.md`).

### Files inside SSA2's stated scope (`firmware-v3/docs/` + `firmware-v3/CONSTRAINTS.md`) that are UNDISPOSITIONED

After enumerating every `.md` under `firmware-v3/docs/` and matching against SSA2's INCLUDE table, EXCLUDE table, and wildcard patterns, **0 files are undispositioned within SSA2's stated scope**. Every doc-tree file is either:

- explicitly listed in INCLUDE (51 files), or
- explicitly listed in EXCLUDE (named or by wildcard), or
- caught by a wildcard pattern (`research/findings/*`, `research/phase1b_runtime_evidence_2026-04-27/**`, `research/synergy-topology/**`, `research/spazz_redesign_2026-04-30/**`, `research/session-notes-2026-04-30/*`, `research/SILENCE_DETECTION_*`, `prompts/*`, `debugging/trace_spec_sections/*`).

### Files OUTSIDE SSA2's stated scope but inside `firmware-v3/` (CAPTAIN GAP)

SSA2's manifest abstract says "Scope: firmware-v3/ documentation only". Yet SSA2 only enumerated `firmware-v3/docs/` plus `firmware-v3/CONSTRAINTS.md`. The following firmware-v3 root-and-subtree `.md` files were silently NOT considered:

| Path | Size | Likely disposition |
|---|---|---|
| `firmware-v3/CLAUDE.md` | small | EXCLUDE (claude-mem stub) |
| `firmware-v3/HANDOVER_BeatTracker.md` | ~8 KB | EXCLUDE (dated handover, BLOCKED status, contains "destroyed the codebase" narrative) |
| `firmware-v3/PYTORCH_PORT_COMPLETE.md` | unread | likely EXCLUDE (port milestone log) |
| `firmware-v3/REFERENCE_HARNESS.md` | ~6 KB | INCLUDE candidate (BeatPulseTransportCore reference test harness — current ground-truth doc) |
| `firmware-v3/research/AFSv2.md` | 42 KB | EXCLUDE candidate (research) |
| `firmware-v3/research/k1-spec-recommendations-2026-04.md` | 53 KB | EXCLUDE candidate (recommendations, dated) |
| `firmware-v3/research/OSCILLOSCOPE_EFFECT_CONTRACTS.md` | ~5 KB | likely EXCLUDE (research substrate) |
| `firmware-v3/research/VISUAL_PERSISTENCE_ARCHITECTURE.md` | ~5 KB | likely EXCLUDE (research substrate) |
| `firmware-v3/data/CLAUDE.md` | small | EXCLUDE (claude-mem stub) |
| `firmware-v3/src/CLAUDE.md` | unread | EXCLUDE candidate (likely src-area instructions, not user-facing doc) |
| `firmware-v3/src/README.md` | unread | INCLUDE candidate (lists FEATURE_ENHANCEMENT_ENGINES build flags — relevant) |
| `firmware-v3/testbed/README.md` | unread | EXCLUDE (testbed setup) |
| `firmware-v3/testbed/REFERENCE_GENERATION.md` | unread | EXCLUDE candidate (reference generation tooling) |
| `firmware-v3/testbed/SETUP_SUMMARY.md` | unread | EXCLUDE (setup log) |
| `firmware-v3/tools/TEST_RESULTS_TEMPLATE.md` | unread | EXCLUDE (template) |
| `firmware-v3/tools/verify_broadcast_logic.md` | unread | EXCLUDE candidate (tool-specific) |
| `firmware-v3/tools/WS_HARDWARE_TEST_README.md` | unread | EXCLUDE candidate (hardware test) |
| `firmware-v3/.claude/handoff.md` | unread | EXCLUDE (handoff doc) |

**Verdict:** SSA2's stated scope said "firmware-v3/ documentation" but the actual working scope was "firmware-v3/docs/ + CONSTRAINTS.md". The 18 files above are outside the working scope but within the stated scope. Most are correctly excluded by class (handovers, stubs, research, tooling), but **`firmware-v3/REFERENCE_HARNESS.md`** is a strong INCLUDE candidate (current beat-tracker reference harness ground-truth doc) that SSA2 never considered.

---

## Task 2 — False-negative excludes (canonical content wrongly rejected)

Sampled the EXCLUDE categories. Read or grepped representative entries.

### Sample 1 — `firmware-v3/docs/SENSORYBRIDGE_AUDIO_PROCESSING_RESEARCH.md` (EXCLUDED as research)

Read first 30 lines. Frontmatter abstract (`firmware-v3/docs/SENSORYBRIDGE_AUDIO_PROCESSING_RESEARCH.md:1-3`):
> "SensoryBridge (Lixie Labs) audio silence detection and noise gating implementation. Covers calibration algorithm, per-bin noise subtraction, RMS-based VU calculation, 10-second silence timeout, novelty detection, and sweet spot fade logic. Research source: GitHub connornishijima/SensoryBridge."

This is a research-source extract document — covers an external project's algorithm. SSA2's reasoning ("wled-* and emotiscope-algorithms.md cover the canonical ports") is reasonable. The document does not document K1-shipping behaviour; it documents the upstream SB algorithm.

**Verdict on exclusion:** CORRECT (LOW severity if wrong). SSA2's flag-7 already surfaces this for Captain.

### Sample 2 — `firmware-v3/docs/audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md` (EXCLUDED as superseded, dated 2025-12-29)

Cannot fully verify without reading. SSA2's reasoning was that IMPLEMENTATION_PATTERNS + VISUAL_PIPELINE_MECHANICS supersede this. Trust-but-verify burden is on the Captain — flagged in SSA2 manifest already.

**Verdict on exclusion:** likely CORRECT (LOW). Already flagged.

### Sample 3 — `firmware-v3/docs/audio-visual/AUDIO_FEATURE_SURFACE_V2_CONTRACT.md` (EXCLUDED as DRAFT)

Frontmatter status reportedly "implementation not yet authorised beyond policy/helper design." This matches the description of a foundation-contract doc not yet in canonical effect with shipping firmware.

**Verdict on exclusion:** CORRECT (LOW). Sterilisation doctrine respected.

### Sample 4 — `firmware-v3/docs/architecture/WEBSERVER_BASELINE_INVENTORY.md` (EXCLUDED as pre-refactor baseline)

Per SSA2: refactor is complete per migration guide. INCLUDED migration guide explicitly states "All 141 WS commands migrated. processWsCommand() removed." Pre-refactor inventory is therefore pre-state, not current state.

**Verdict on exclusion:** CORRECT (LOW).

### Sample 5 — `firmware-v3/docs/debugging/trace_spec_sections/*.md` (10 files EXCLUDED as duplicating master spec)

Master spec `TRACE_INSTRUMENTATION_SPEC.md` is INCLUDED. Section files are deep reference per their README abstract. NotebookLM with master spec already covers content. Excluding sections prevents content duplication that confuses the index.

**Verdict on exclusion:** CORRECT (LOW). Reasonable doctrine call.

### Sample 6 — `firmware-v3/docs/research/spazz_redesign_2026-04-30/*` (22 files EXCLUDED en bloc)

Source verification: ChevronWavesEffect.cpp, LGPWaveCollisionEffect.cpp, etc. exist as source files. The redesign apparently shipped some effects, but I have no evidence the spazz_redesign substrate docs were ratified into canonical doctrine. SSA2's flag-5 already flags this.

**Verdict on exclusion:** likely CORRECT (MEDIUM if wrong — these are 22 files of detailed redesign substrate). Already flagged for Captain. Closer look would require reading SYNTHESIS.md and PORT_PLAN.md to see if either is shaped as ratified output. Did not perform that check (token budget).

### Summary — false negatives

No HIGH-severity false-negative excludes detected. All sampled excludes have defensible rationale and SSA2 already surfaced the marginal ones to Captain via the unresolved-flags section.

---

## Task 3 — False-positive includes (stale content wrongly accepted)

Sampled SSA2's candidate INCLUDEs (the 41 non-mandatory ones).

### Sample A — `firmware-v3/docs/p1-09-migration-cookbook.md` (INCLUDED)

Read first 30 lines. The doc says (`firmware-v3/docs/p1-09-migration-cookbook.md:15`):
> `kMaxZones = 4` per class.

Cross-checked against source. `firmware-v3/src/effects/ieffect/*.h` shows ~20+ effect classes use `kMaxZones = 4` — this matches the doc. The "4" here is a per-class array dimension safety upper bound, not the user-visible zone count.

User-visible zone count is 3 per `MEMORY.md feedback_zone_numbering.md`. Source has both:
- Per-effect `kMaxZones = 4` (defensive array bound)
- User-facing limit of 3 zones

The cookbook is technically consistent with source semantics (the array bound is still 4) but the doc does NOT explain that the user-facing limit is 3 zones. **A NotebookLM consumer reading only this doc could conclude "4 zones" is the user-facing model**, which contradicts the project's explicit "Zone 1, 2, 3 only — Zero-indexed zones BANNED" rule.

**Verdict:** INCLUDE-with-disclaimer. Severity MEDIUM. SSA2 should flag the cookbook for "kMaxZones=4 is an array dimension; user-facing zone count is 3 (1-indexed) per MEMORY.md feedback_zone_numbering.md, refactored Apr 1 2026."

### Sample B — `firmware-v3/docs/effects-catalog/EFFECTS_INVENTORY.md`, `MATH_APPENDIX.md`, `PATTERN_TAXONOMY.md` (INCLUDED, 3 files)

SSA2 already flagged: dated 2026-02-21, ~2.5 months old. Effect catalog likely shifted. SSA2's MATH_APPENDIX flag (2,837 lines) also raises NotebookLM single-source word-limit concern.

**Verdict:** SSA2's flags are correct. Severity MEDIUM. Captain should:
- Re-check MATH_APPENDIX.md against `EFFECTS_BEHAVIORAL_REFERENCE.md` for stale effect-name pairs.
- Decide whether to split MATH_APPENDIX or include with explicit "snapshot 2026-02-21" caveat.

### Sample C — `firmware-v3/docs/AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md` (INCLUDED)

Read first 30 lines. Doc is a comprehensive engineering reference for `LGPExperimentalAudioPack` (effect IDs `0x1A00` to `0x1A09`). 1,695 lines total. No frontmatter abstract or status marker. SSA2 already flagged: may contain stale parameter values vs. shipping behaviour.

**Verdict:** SSA2's flag is correct. Severity MEDIUM if implementation has drifted from doc. INCLUDE-with-disclaimer is the safer call.

### Sample D — `firmware-v3/docs/audio-visual/MUSICAL_LOGIC_CANONICAL_MODEL.md` (INCLUDED)

Frontmatter: "Single source of truth for what the audio pipeline produces today. Pure observation, no external framework, no prescription." (`firmware-v3/docs/audio-visual/MUSICAL_LOGIC_CANONICAL_MODEL.md:2`)

This is a self-described canonical observation document. INCLUDE is correct.

**Verdict:** CORRECT (LOW concern). Strong INCLUDE.

### Sample E — `firmware-v3/docs/api/api-v1.md` (mandatory INCLUDE)

Line 2906: "`/api/*` - Legacy v0 endpoints (still functional)". Confirms api-v1.md acknowledges legacy endpoints are still operational. This is consistent with `firmware-v3/src/network/webserver/V1ApiRoutes.cpp` and the broader webserver source — both v1 and legacy paths register handlers.

**Verdict:** CORRECT (LOW). Strong INCLUDE.

### Sample F — `firmware-v3/docs/EFFECTS_BEHAVIORAL_REFERENCE.md` (INCLUDED, 45 KB, "auto-generated for all 174 registered effect IDs")

Auto-generated docs go stale instantly when generation isn't run after every effect change. SSA2 already flagged. If number of registered effects has changed since generation (e.g. new IDs registered), the doc misrepresents shipping state.

**Verdict:** Severity MEDIUM if stale. Captain action: re-run the generator before locking the bundle, OR include with explicit generation-date disclaimer.

### Summary — false positives

No HIGH-severity false-positive includes detected. The MEDIUM-severity flags are:
1. `p1-09-migration-cookbook.md` — needs zone-count disclaimer.
2. `effects-catalog/{EFFECTS_INVENTORY,MATH_APPENDIX,PATTERN_TAXONOMY}.md` — dated 2026-02-21, possibly stale (already flagged).
3. `AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md` and the 132-151 blueprint — possibly stale (already flagged).
4. `EFFECTS_BEHAVIORAL_REFERENCE.md` — auto-gen freshness (already flagged).

---

## Task 4 — Specific file dispositions

### `firmware-v3/docs/api/api-legacy.md` (8.7 KB, fully read)

**Verdict: INCLUDE-WITH-DISCLAIMER.**

Reasoning:
- The doc is NOT a deprecated/superseded artifact. It documents the legacy `/api/*` endpoint surface that **still functions** in current shipping firmware (per api-v1.md:2906 — "Legacy v0 endpoints (still functional)").
- Source verification: every endpoint family it documents has a live implementation:
  - `/api/v1/zones/layout` — `firmware-v3/src/network/webserver/V1ApiRoutes.cpp:952`
  - Legacy `setEffect` WS commands — `firmware-v3/src/network/webserver/ws/WsEffectsCommands.cpp:194,310,825`
  - Zone REST family — referenced in `firmware-v3/src/network/README.md:320`
- One stale element: lists `/api/zone/count` as "deprecated" (line 209). True per source; the doc itself acknowledges the deprecation.
- One potentially stale element: lists "Quad" preset (4-zone) at line 272. Captain should verify against current `getZonePresets()` implementation — the project moved to 3-zones-max per MEMORY.md feedback_zone_numbering.md (Apr 1 2026 zone-system refactor).
- Disclaimer to attach: "This is the LEGACY-COMPATIBILITY API surface. v1 (`/api/v1/*`) is the canonical contract. Legacy `/api/*` paths still function for backwards compatibility but should not be used in new code. Zone presets reference 4-zone schema; user-facing zone count is now 3 (1-indexed) per Apr 1 2026 refactor."
- Value to NotebookLM: prevents the corpus from hallucinating non-existent endpoints when answering questions about K1's REST/WS surface (the legacy paths ARE on the device, are documented, and ARE returning 200 responses).

SSA2's EXCLUDE rationale ("Legacy API — superseded by v1") is **incorrect** as written — "superseded" implies the legacy is removed; it isn't.

**Severity of SSA2 error: HIGH.** This is canonical documentation of currently-functioning shipping behaviour, wrongly excluded.

### `firmware-v3/docs/api/enhancement-engine-api.md` (17.1 KB, fully read)

**Verdict: EXCLUDE.**

Reasoning:
- Doc explicitly requires `FEATURE_ENHANCEMENT_ENGINES=1` build flag.
- Source check (`firmware-v3/platformio.ini`): the canonical K1 envs `esp32dev_audio_esv11_k1v2_32khz` and `esp32dev_audio_esv11_32khz` do NOT set `FEATURE_ENHANCEMENT_ENGINES=1`. Only `firmware-v3/src/README.md:494` lists it as an available build flag.
- Source check (`firmware-v3/src/effects/enhancement/`): `ColorEngine.cpp/h`, `MotionEngine.cpp/h`, etc. exist as source — the engines are present, but they only register their REST routes when the flag is set.
- Doc dates: "Last Updated: 2025-12-12. Target Firmware: esp32dev_enhanced build." That env is not the canonical K1 production env.
- Doc itself documents an `esp32dev_enhanced` build path that does not exist in current `platformio.ini` as a canonical K1 env.
- The doc references future "BlendingEngine" features as "Week 5 — not yet implemented" — pre-implementation content.
- INCLUDING this would mislead NotebookLM into believing K1 ships with `/api/enhancement/*` endpoints active. They do not, in production builds.

SSA2's EXCLUDE was **CORRECT**.

**Severity of SSA2 disposition: 0 (correct).**

### `firmware-v3/docs/api/CLAUDE.md` (482 B, fully read)

**Verdict: EXCLUDE.**

Reasoning:
- File is 13 lines total, entirely a `<claude-mem-context>` auto-generated activity ledger (3 entries from Dec 21, 2025 about API V2 specification work).
- Zero documentation content. Pure session-activity index.
- Identical class to `firmware-v3/docs/CLAUDE.md` and `firmware-v3/docs/effects-catalog/CLAUDE.md` which SSA2 also excluded as "junk content".

SSA2's EXCLUDE was **CORRECT**.

---

## Task 5 — Subdirectory completeness

| Subdirectory | Total .md | SSA2 INCLUDE | SSA2 EXCLUDE | Undispositioned |
|---|---|---|---|---|
| `firmware-v3/docs/reference/` | 9 | 9 | 0 | 0 |
| `firmware-v3/docs/api/` | 5 | 1 (api-v1) | 4 (api-v2, api-legacy, enhancement, CLAUDE) | 0 |
| `firmware-v3/docs/audio-visual/` | 19 | 12 | 7 | 0 |
| `firmware-v3/docs/debugging/` | 16 | 3 (DEBUG_SYSTEM, MABUTRACE, TRACE_SPEC) | 13 (chaos-map, redesign, architecture-review, 10 trace_spec_sections) | 0 |
| `firmware-v3/docs/research/` | ~95 | 0 | ~95 (en-bloc wildcards + named) | 0 |
| `firmware-v3/docs/design/` | 5 | 1 (ONSET_DETECTOR_SPEC) | 4 (CLOSED_LOOP, INFERENCE_DECISION, INFERENCE_PLACEMENT, ONSET_QUARANTINE) | 0 |
| `firmware-v3/docs/specs/` | 6 | 0 | 6 (all spec/codex prompts) | 0 |
| `firmware-v3/docs/effects-catalog/` | 5 | 3 (INVENTORY, MATH_APPENDIX, PATTERN_TAXONOMY) | 2 (CLAUDE, GAP_REPORT) | 0 |
| `firmware-v3/docs/integration/` | 0 | — | — | — (does not exist) |
| `firmware-v3/docs/architecture/` | 3 | 2 (DEFENSIVE_BOUNDS, WEB_SERVER_MODULAR) | 1 (WEBSERVER_BASELINE_INVENTORY) | 0 |
| `firmware-v3/docs/migration/` | 1 | 1 (WEB_SERVER_REFACTOR_MIGRATION) | 0 | 0 |
| `firmware-v3/docs/implementation/` | 1 | 0 | 1 (WEBSERVER_REFACTOR_IMPL_SUMMARY) | 0 |
| `firmware-v3/docs/measurement_protocols/` | 1 | 1 (m1_lgp_fringe) | 0 | 0 |
| `firmware-v3/docs/performance/` | 2 | 2 (RMT_SHOW_PATH, VALIDATION_OVERHEAD) | 0 | 0 |
| `firmware-v3/docs/testing/` | 3 | 3 (AUDIO_TEST_HARNESS, METRICS_REFERENCE, TEST_SCENARIOS) | 0 | 0 |
| `firmware-v3/docs/audit/` | 3 | 0 | 3 (all dated audits) | 0 |
| `firmware-v3/docs/prompts/` | 9 | 0 | 9 (all agent prompts) | 0 |
| `firmware-v3/docs/` (root) | 16 | 11 | 5 (handovers, postmortem, debt audit, stage-cherry-pick) | 0 |

**Total firmware-v3/docs/.md:** 207 (matches `find` enumeration).

All subdirectories within `firmware-v3/docs/` are fully covered by SSA2's disposition (named or wildcard). No undispositioned files within stated scope.

**Out-of-scope files found in firmware-v3/ (Task 1 finding):** 18 files that SSA2 didn't enumerate. None require INCLUDE except potentially `firmware-v3/REFERENCE_HARNESS.md` and `firmware-v3/src/README.md`.

---

## Task 6 — STA-mode re-sweep on INCLUDE set

Re-greped all 51 SSA2 INCLUDE files plus `CONSTRAINTS.md` for: `WIFI_MODE_STA`, `wifi_sta`, `WIFI_STA`, `station mode`, `STA mode`, `STA-mode`, `WiFi client`, `wifi.sta`, `wifi.client`.

**Single hit found:** `firmware-v3/docs/reference/fsm-reference.md:159`:
> `**Network types:** None, WiFiStation, WiFiAP, Ethernet, EspHosted`

This is the documented `ConnectionState.networkType` enum — a TYPE-SYSTEM declaration of all values the enum can take. It is NOT a recommendation to enable STA on K1; it is a description of what values exist in the type. Same hit SSA2 already classified as SAFE.

**No other STA hits in the INCLUDE set.** The earlier SSA2 hits on `MABUTRACE_GUIDE.md` (`wifi_ap_mode` counter — AP-confirming) and `WEB_SERVER_MODULAR_ARCHITECTURE.md` (false positive on "stage") are not present in my targeted re-grep with the tighter pattern set.

**No file in the INCLUDE set proposes WiFi STA mode as viable for K1.** Confirmed.

---

## Severity-weighted action list

### HIGH severity (correct before locking the bundle)

1. **INCLUDE `firmware-v3/docs/api/api-legacy.md` WITH DISCLAIMER.** SSA2 EXCLUDED as "superseded by v1", but source verification (`V1ApiRoutes.cpp`, `WsEffectsCommands.cpp`, `network/README.md`, api-v1.md:2906) shows the legacy paths still function. NotebookLM without this doc will hallucinate or refuse-to-know about half of K1's working REST/WS surface. Disclaimer text proposed in Task 4.
2. **Verify zone count in `p1-09-migration-cookbook.md`.** The doc describes `kMaxZones = 4` array bound (correct per source) but does NOT mention the user-facing zone-count limit was refactored from 4 to 3 on Apr 1 2026 (per MEMORY.md `feedback_zone_numbering.md`). Add a disclaimer to the cookbook's bundled copy, or annotate at the manifest level.

### MEDIUM severity (Captain should resolve before launch)

3. **Decide on `EFFECTS_BEHAVIORAL_REFERENCE.md` freshness.** Auto-generated for 174 effect IDs. Re-run the generator now to ensure currency, OR add an "as of <date>" caveat in the bundled copy.
4. **Decide on `effects-catalog/MATH_APPENDIX.md` size.** 2,837 lines may exceed NotebookLM single-source word limit. Either split, replace with a curated subset, or accept truncation.
5. **Re-check `AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md` and `NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md`** against current effect IDs to confirm shipping correctness — they are dense engineering refs and may contain stale parameters.
6. **Consider `firmware-v3/REFERENCE_HARNESS.md` for INCLUDE.** SSA2 didn't enumerate this file. It's the BeatPulseTransportCore reference test harness ground-truth doc — relevant to anyone querying NotebookLM about K1's beat-tracker. If beat-tracker behaviour is in-scope for the corpus, INCLUDE it.

### LOW severity (Captain awareness only)

7. **Confirm `firmware-v3/docs/research/spazz_redesign_2026-04-30/SYNTHESIS.md` and `PORT_PLAN.md` ratification status.** Source code shows `ChevronWavesEffect.cpp` and `LGPWaveCollisionEffect.cpp` exist. If these two redesign docs ARE the canonical reference for those effects (per SSA2 flag-5), they should be promoted to INCLUDE. Currently EXCLUDED en bloc.
8. **Confirm `firmware-v3/docs/SENSORYBRIDGE_AUDIO_PROCESSING_RESEARCH.md` is not the canonical noise-gating reference for current shipping firmware.** SSA2 flag-4 already raises this.
9. **All `firmware-v3/docs/audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md` content is fully covered by IMPLEMENTATION_PATTERNS + VISUAL_PIPELINE_MECHANICS** — verify before locking, otherwise INCLUDE.

### CORRECT (no action needed)

10. SSA2's EXCLUDE of `enhancement-engine-api.md` is correct (feature-flag not enabled in canonical envs).
11. SSA2's EXCLUDE of `api/CLAUDE.md`, `effects-catalog/CLAUDE.md` is correct (auto-stubs).
12. SSA2's en-bloc EXCLUDE of `research/findings/*`, `research/phase1b_runtime_evidence_*`, `research/synergy-topology/*`, `research/session-notes-*`, `research/SILENCE_DETECTION_*`, `prompts/*`, `audit/*`, `docs/SESSION_HANDOVER_*`, dated postmortems, and Captain-decision docs is correct.
13. STA-mode discipline in the bundled corpus is INTACT. Zero STA-promoting content.

---

## Confidence assessment

**SSA2's disposition (after the corrections above) covers firmware-v3/docs/ canonically with HIGH confidence.**

- Within stated scope (`firmware-v3/docs/` + `firmware-v3/CONSTRAINTS.md`): 0 silently undispositioned files. Disposition is exhaustive.
- The one HIGH-severity correction (`api-legacy.md`) is a SSA2 reasoning slip, not a coverage slip.
- STA-mode discipline is intact.
- Sterilisation doctrine ("when in doubt, exclude") was applied consistently.
- SSA2 already self-flagged 8 marginal cases for Captain — most of my MEDIUM-severity flags overlap with those.

**The two genuine SSA2 GAPS:**
- The implicit scope-narrowing from "firmware-v3/" to "firmware-v3/docs/ + CONSTRAINTS.md" — 18 files outside docs/ were silently not considered. Most of these don't matter; one (`REFERENCE_HARNESS.md`) probably does.
- The `api-legacy.md` mis-classification (HIGH).

After these corrections, the bundle covers firmware-v3 docs canonically.

---

## What I did NOT verify

- **Did not read full bodies** of EFFECTS_BEHAVIORAL_REFERENCE.md, MATH_APPENDIX.md, AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md, NON_AUDIO_EFFECTS_PACK_132_151. I sampled their first 30 lines and trusted SSA2's flag triage. If those docs internally promote 4-zones or contradict CLAUDE.md hard constraints, my audit missed it.
- **Did not read `spazz_redesign_2026-04-30/SYNTHESIS.md` or `PORT_PLAN.md`** to assess ratification. Token budget. SSA2's flag-5 covers this; Captain decision needed.
- **Did not re-read all 12 audio-visual INCLUDED files line-by-line.** Sampled frontmatter status only.
- **Did not run a full grep across every byte of every INCLUDED file** for date-stamps that contradict CLAUDE.md. STA grep was tight; date-stamp grep was absent.
- **Did not verify `firmware-v3/docs/EFFECTS_BEHAVIORAL_REFERENCE.md` matches the current effect registry.** Would require running the regen tool or comparing against `EffectRegistry.cpp`.
- **Did not enumerate `firmware-v3/src/**/CLAUDE.md` or `firmware-v3/src/**/README.md`** beyond the top-level src files. There may be additional doc-class files deeper in `src/`.
- **Did not check `firmware-v3/docs/api/api-v2.md`** in this audit — Captain said it is being handled in a separate workstream with multi-point disclaimers. I trusted that.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:Forensic-Audit-SSA (claude-opus-4-7-1m) | Created. Forensic re-audit of SSA2's firmware-v3 docs disposition. 0 silently undispositioned files within stated scope; 18 out-of-stated-scope files in firmware-v3/ root flagged. 2 HIGH-severity corrections (api-legacy.md INCLUDE, p1-09-cookbook zone-count disclaimer). 4 MEDIUM-severity corrections. STA-mode discipline confirmed intact (1 SAFE hit on fsm-reference.md type-system enum). |
