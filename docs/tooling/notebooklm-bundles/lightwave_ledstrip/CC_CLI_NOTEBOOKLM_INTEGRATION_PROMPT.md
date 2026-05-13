---
abstract: "CC CLI agent prompt for re-syncing and re-configuring the Lightwave-Ledstrip NotebookLM. Rev 4 (2026-05-13) -- bundle expanded to 163 sources. Custom system prompt and 5 test queries updated to surface the SynqMatrix rename PHASE WARNING (alongside the existing AP-only WiFi sterilisation gate). Includes a dedicated test query for the MusicAware audit and a test query confirming STA-NEVER policy still holds. Use this when re-uploading the Rev 4 bundle to NotebookLM."
---

# NotebookLM Integration -- Lightwave-Ledstrip CC CLI Agent Prompt (Rev 4)

> **Run this from a CC CLI session** in the Lightwave-Ledstrip project directory (`~/Workspace_Management/Software/Lightwave-Ledstrip/`). Requires `notebooklm-mcp` MCP server to be registered and available. If tools are not available, check `.mcp.json` configuration.

## Context

The Rev 4 bundle (2026-05-13) covers the entire Lightwave-Ledstrip project: firmware architecture, iOS app, Tab5 controller, protocol contracts, effect development standards, audio pipeline (including the new MusicAware audit and Phase 1B runtime evidence), WiFi constraints, SynqMatrix migration in-flight review, and governance docs.

**163 sterilised sources.** Net delta vs Rev 3: +36 sources (MusicAware audit, Phase 1B evidence, SynqMatrix rename review tree, VP audits, INFERENCE_TASK design briefs, selected research summaries).

**Notebook ID:** `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4`
**Notebook title:** Lightwave-Ledstrip -- K1 Project Knowledge Base (forensic-corrected 2026-05-04, refreshed 2026-05-13)

## Phase 1: Re-upload sources

The previous 127 sources must be deleted and replaced with the Rev 4 set (163 sources). Use `UPLOAD_HELPER.md` in this directory for the ordered upload list.

## Phase 2: Re-configure the notebook for agent consumption

Use `chat_configure` to refresh the custom system prompt. The Rev 4 prompt adds the **SynqMatrix migration phase warning** alongside the existing AP-only WiFi sterilisation rule.

```
chat_configure(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    goal="custom",
    custom_prompt="You are answering questions from a software engineering agent (Claude Code) working on this ESP32-S3 LED controller codebase. The agent has tools to read source files and navigate code symbols -- it does NOT need you to explain what code does. It needs you to explain WHY decisions were made, WHAT constraints apply, and WHERE to find things.\n\nResponse format (MANDATORY for every answer):\n\n1. ANSWER: Direct technical answer in 2-5 sentences. Be precise and terse. No preamble.\n\n2. CONSTRAINTS: List any hard constraints, invariants, or rules that apply to this topic. Include the source document name for each.\n\n3. KEY FILES: List the most relevant source file paths mentioned in the uploaded documents. Use exact paths as they appear in the sources.\n\n4. CROSS-REFS: If the question touches multiple subsystems (audio, effects, network, iOS, Tab5), list which other subsystems are affected and why.\n\n5. WARNINGS: Any tripwires, anti-patterns, or 'do NOT do this' rules relevant to the question. These are critical -- agents that miss warnings produce regressions.\n\nRules:\n- British English always.\n- K1 is AP-only in production via the WIFI_AP_ONLY build flag. NEVER suggest concurrent AP+STA -- that hits a known ESP-IDF 802.11 driver bug per _FORENSIC_WIFI_REPORT.md. Dual-mode (AP OR STA, never together) is the documented goal-state, not the current state. If asked about WiFi, always cite the forensic report and the current-vs-goal-state distinction.\n- SYNQMATRIX MIGRATION PHASE WARNING: the source tree is mid-flight on the songAware -> synqMatrix rename. Internal C++ types are migrating to SynqMatrix* (SynqMatrixMode, SynqMatrixProfile, etc. in src/core/synqmatrix/SynqMatrix.h). Wire-level identifiers (REST routes /api/v1/songAware/*, WS commands songAware.*, JSON keys currentSongState etc.) REMAIN songAware until the contract rev lands. The MusicAware audit refers to SongAwareDirector because the audit inspected the runtime branch, not the rename branch. Treat songAware and synqMatrix as referring to the same audio-aware director subsystem during this window; flag any code change that touches identifiers as needing rename-branch coordination.\n- Never suggest heap allocation in render paths. 2.0ms per-frame ceiling.\n- Centre-origin (LED 79/80 outward) applies to ALL effects -- mention it whenever effects are discussed.\n- Audio backend: ESV11 at 32 kHz (the _32khz envs are canonical). 256-sample hop, 128-sample chunk, 120 FPS render target.\n- Actor model: AudioActor (Core 0) | RendererActor (Core 1) | ShowDirectorActor | CommandActor | PluginManagerActor.\n- If the sources do not contain enough information to answer confidently, say so explicitly. Do NOT fabricate.\n- Prefer quoting exact constraint language from the source documents over paraphrasing.",
    response_length="default"
)
```

**Verify:** After configuration, confirm the response with a brief status check. The tool should return success.

## Phase 3: Test queries (5 representative scenarios, Rev 4)

Run these 5 queries against the configured notebook. For each query, record: (a) the full response, (b) whether the response follows the 5-section format, (c) whether the cited files/constraints are accurate, (d) a quality grade (A/B/C/F).

**Quality rubric:**
- **A**: Correct, structured, actionable, cites real files, includes relevant warnings.
- **B**: Correct but missing structure, or includes unnecessary prose, or misses a relevant constraint.
- **C**: Partially correct, or cites wrong files, or misses critical constraints. Dangerous if agent trusts it.
- **F**: Wrong, fabricated, or contradicts canonical truth. Kill signal -- reconfigure before proceeding.

### Query 1: MusicAware audit content (NEW Rev 4 content test)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="What does the MusicAware audit say about the current production audio path on K1? Specifically: what is the reference build environment, what sample rate and hop size does it use, and what are the current SongAwareDirector decision gates?"
)
```

**Expected in response:** Cites `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md`. Identifies `esp32dev_audio_esv11_k1v2_32khz` as reference env. Specifies 32 kHz sample rate, 256 hop, 128 chunk, 120 FPS / 8333 us render cadence. Names key director gates (boot grace, post-enable grace, evaluation-period throttle, stable hold, dwell, cooldown, anti-thrash, health). MUST surface the SYNQMATRIX PHASE WARNING -- the audit refers to `SongAwareDirector` because that is the runtime branch identifier; the rename branch is migrating to `SynqMatrix*`. **This is the new-content litmus test.**

### Query 2: SynqMatrix migration PHASE WARNING (NEW Rev 4 sterilisation test)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="Is there a class called SynqMatrixDirector in the codebase? What is the difference between songAware and synqMatrix identifiers? Which one should I use when I add a new wire command?"
)
```

**Expected in response:** MUST surface the SYNQMATRIX PHASE WARNING. Should explain: (a) `src/core/synqmatrix/SynqMatrix.h` exists with `SynqMatrixMode`, `SynqMatrixProfile`, `SynqMatrixState`, etc.; (b) the runtime class name during the inspected branch is still `SongAwareDirector` per the MusicAware audit; (c) wire-level identifiers (REST routes, WS commands, JSON keys) remain `songAware.*` until the contract rev lands; (d) per the naming review, ~200+ touchpoints span 11 files. Must cite `docs/temporary/projects/synqmatrix-naming-review/00-INDEX.md` and `firmware-v3/src/core/synqmatrix/SynqMatrix.h`. **This is the in-flight-migration sterilisation test.**

### Query 3: WiFi constraints / STA policy (Rev 3 sterilisation litmus -- must still hold)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="Can K1 connect to a home WiFi network as a client (STA mode)? What's the current WiFi architecture and what are the constraints? Has the AP-only constraint ever been relaxed?"
)
```

**Expected in response:** MUST cite the forensic report (`_FORENSIC_WIFI_REPORT.md`). Must distinguish CURRENT STATE (AP-only via `WIFI_AP_ONLY` build flag + `m_forceApOnly` runtime lock) from GOAL STATE (dual-mode AP-OR-STA, never together). Must identify CONCURRENT AP+STA as the genuine ESP-IDF 802.11 driver bug, NOT STA-alone. Must NOT use the words "KNOWN BROKEN", "architecturally prohibited", or "6+ failed". Should reference Era 1 (Light Crystals 2025-06-24) and Era 3 (v2 STA-primary 2025-12-16) as evidence that STA-alone has worked historically. Must cite BACKLOG F-5 (dual-mode WiFi delivery) as the engineering scope item. **This is the WiFi-doctrine sterilisation test; the STA-NEVER strength-D wording must not resurface.**

### Query 4: Effect development constraints

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="I need to write a new audio-reactive LED effect. What are ALL the constraints, rules, and patterns I must follow? Include the centre-origin rule, heap restrictions, timing budget, and any anti-patterns. Reference both the V1 and V2 authoring standards."
)
```

**Expected in response:** Centre origin 79/80, no heap in render(), 2.0ms ceiling, no rainbows, zone bounds checking, dt-correction, inheriting `EffectBase`, implementing `render(RenderContext&)`. Should cite both `EFFECT_DEVELOPMENT_STANDARD.md` (V1) and `EFFECT_AUTHORING_STANDARD_V2.md` (V2 -- Rev 4 addition). Should mention `audio-visual-semantic-mapping.md`.

### Query 5: Cross-subsystem audio data flow

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="Describe the audio data flow from microphone input to LED output. What actors are involved, which cores do they run on, what are the timing constraints, and how does the SongAwareDirector / SynqMatrix sit in the chain?"
)
```

**Expected in response:** AudioActor (Core 0) -> EsV11Backend -> EsV11Adapter -> ControlBusFrame -> SnapshotBuffer -> RendererActor (Core 1). ESV11 32 kHz, 256 hop, 128 chunk. 120 FPS / 2.0ms ceiling. SongAwareDirector / SynqMatrix sits between AudioActor and RendererActor (parameter modulation via `apply()` + switching decision via `evaluateDirector()`). Should cite `MusicAware_Audit_And_Gap_Analysis.md`, `AUDIO_SYSTEM_ARCHITECTURE.md`, `codebase-map.md`, and the `_BUNDLE_firmware_actors.txt` bundle.

## Phase 4: Evaluate and report

After running all 5 queries, produce a structured report:

```
NOTEBOOKLM INTEGRATION TEST REPORT -- REV 4
============================================

Notebook: 92d45c0b-83c7-4971-aa9a-2c9ee13b06d4
Chat config: custom (agent-optimised, Rev 4)
Date: [today]
Bundle: 163 sources (+36 vs Rev 3)

Query 1 -- MusicAware audit content:       [A/B/C/F] -- [one-line reason]
Query 2 -- SynqMatrix PHASE WARNING:       [A/B/C/F] -- [one-line reason]
Query 3 -- WiFi / STA-NEVER hold:           [A/B/C/F] -- [one-line reason]
Query 4 -- Effect constraints (V1 + V2):    [A/B/C/F] -- [one-line reason]
Query 5 -- Audio data flow:                 [A/B/C/F] -- [one-line reason]

Overall grade: [A/B/C/F]

Format compliance: [did responses follow the 5-section format? Y/N]
Citation accuracy: [did cited files/constraints match reality? Y/N + notes]
WiFi-doctrine sterilisation hold: [did Query 3 correctly enforce evidence-grounded dual-mode framing? Y/N]
SynqMatrix PHASE WARNING surfaced: [did Queries 1 + 2 surface the migration warning? Y/N]
MusicAware content discoverable: [did Query 1 return the audit content? Y/N]

RECOMMENDATION: [PROCEED / RECONFIGURE / ABORT]
- If PROCEED: bundle is ready for production agent queries
- If RECONFIGURE: specify what to change in chat_configure (likely the system prompt)
- If ABORT: explain what's fundamentally broken
```

## Phase 5: Update NOTEBOOK_REGISTRY.md

After successful re-upload + verification, update `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md` -- bump the Lightwave-Ledstrip row's Last-synced date to today and source count to 158.

## Error handling

- If `chat_configure` fails, check that `notebooklm-mcp` is registered in `.mcp.json` and the server is running. Try `server_info()` to verify connectivity.
- If `notebook_query` returns empty or errors, try `notebook_get("92d45c0b-83c7-4971-aa9a-2c9ee13b06d4")` to verify the notebook exists and has sources.
- If queries take >60s, switch to async: `notebook_query_start` + poll `notebook_query_status`.
- If the custom prompt is rejected (>10K chars), trim the rules section. The current prompt is ~3,100 chars -- well under the 10K limit.
- Report all failures per the Tool Failure Protocol in CLAUDE.md.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:NotebookLM-orchestrator | Created -- Rev 1-3 covering the initial 127-source bundle. |
| 2026-05-13 | agent:NotebookLM-orchestrator | Rev 4 -- updated for 158-source rebuild; added SynqMatrix PHASE WARNING to custom prompt; replaced query set to cover MusicAware audit content (Q1), SynqMatrix migration (Q2), WiFi-doctrine hold (Q3), V1+V2 effect standards (Q4), audio data flow with director chain (Q5). |
