# NotebookLM Integration — Lightwave-Ledstrip CC CLI Agent Prompt

> **Run this from a CC CLI session** in the Lightwave-Ledstrip project directory (`~/Workspace_Management/Software/Lightwave-Ledstrip/`). Requires `notebooklm-mcp` MCP server registered and available.

## Context

We have uploaded 127 sterilised sources covering the entire Lightwave-Ledstrip project to NotebookLM. The notebook is live and queryable. This prompt configures the notebook for agent consumption, tests response quality, and — if quality passes — drafts the CLAUDE.md wiring to make NotebookLM a first-class tool in every future CC CLI session on this project.

**Notebook ID:** `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4`
**Notebook title:** Lightwave-Ledstrip — K1 Project Knowledge Base (forensic-corrected 2026-05-04)
**Source count:** 127

## Phase 1: Configure the notebook for agent consumption

Use `chat_configure` to set a custom system prompt that shapes all subsequent query responses for agent use — not human prose.

```
chat_configure(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    goal="custom",
    custom_prompt="""You are answering questions from a software engineering agent (Claude Code CLI) working on the Lightwave-Ledstrip codebase. The agent operates under strict constraints documented in CLAUDE.md.

Response format — MANDATORY for every answer:

## Answer
Direct technical answer. Be precise and terse. Do not explain what code is — the agent can read it. Explain WHY decisions were made and WHAT constraints exist.

## File paths
List the specific source files relevant to this answer (use the paths from the uploaded documents, e.g. firmware-v3/src/audio/contracts/ControlBus.h). The agent will use these paths to navigate directly to the code.

## Constraints
List any hard constraints, invariants, or "never do this" rules that apply to this topic. Pull these from CLAUDE.md, CONSTRAINTS.md, EFFECT_DEVELOPMENT_STANDARD.md, or any other authority document in the sources.

## Cross-references
If this topic connects to other subsystems, name them and explain the coupling. For example: "Audio chain changes affect RendererActor via ControlBus — test both."

Rules:
- British English always (centre, colour, behaviour, initialise).
- K1 WiFi: current shipping firmware is AP-only. STA-only is a goal-state requiring Captain coordination. Concurrent AP+STA is the bug surface — NEVER suggest it.
- If a question touches a topic where the sources contain conflicting information, say so explicitly and cite both sources. Do not silently pick one.
- If the sources do not contain enough information to answer confidently, say "SOURCES INSUFFICIENT" and state what is missing.
- Prefer quoting exact constraint text from authority documents over paraphrasing.
- When referencing effects or render code, always note: centre origin (LED 79/80 outward), no heap in render(), 2.0ms per-frame ceiling.
""",
    response_length="default"
)
```

**Verify:** After configuration, the response should confirm the custom prompt was applied. If it fails, check `server_info()` for auth status.

## Phase 2: Test queries — 5 representative scenarios

Run these 5 queries against the notebook. For each, record: (a) the full response, (b) whether file paths are accurate, (c) whether constraints are correctly cited, (d) a quality grade (PASS / PARTIAL / FAIL).

### Query 1: Audio chain architecture (subsystem understanding)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="Explain the complete audio data flow from microphone input to LED output. Include the actor model, which cores they run on, the backend, sample rate, frame rate, and the key data structures at each stage."
)
```

**Ground truth to verify against:**
- Microphone → I2S DMA → AudioActor (Core 0) → ESV11 backend → PipelineAdapter → ControlBus → RendererActor (Core 1) → Effects → FastLED → RMT → WS2812 LEDs
- ESV11 at 32 kHz, 125 Hz frame rate
- 64-bin Goertzel + 12-note chroma + 8-band octave + tempo/beat tracking
- ControlBus is the shared audio state contract
- Should mention `_32khz` envs as canonical

### Query 2: Effect development constraints (constraint retrieval)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="I need to write a new LED effect. What are ALL the constraints, rules, and requirements I must follow? Include the development standard, render constraints, and any anti-patterns."
)
```

**Ground truth to verify against:**
- Centre origin: LED 79/80 outward (or inward to 79/80). No linear sweeps.
- No heap alloc in render() or any transitive call (no new/malloc/String)
- 120 FPS target, 2.0ms per-frame ceiling
- No rainbows / no full hue-wheel sweeps
- Effects inherit EffectBase (actually IEffect.h), implement render(RenderContext&)
- Zone-indexed access must bounds-check: (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0
- dt-correction for frame-rate independence
- British English in comments
- Should reference EFFECT_DEVELOPMENT_STANDARD.md

### Query 3: WiFi mode — the critical safety question

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="What is the current WiFi mode for K1? Can I implement STA mode? What's the history of WiFi mode changes and what went wrong?"
)
```

**Ground truth to verify against:**
- Current: AP-only via WIFI_AP_ONLY build flag
- Goal-state: dual-mode (AP OR STA, never both concurrently)
- Concurrent AP+STA is the bug surface (ESP-IDF 802.11 driver-level auth corruption)
- ~3,418 LOC of STA infrastructure is PRESENT-DISABLED
- _sta_validation build env exists for re-validation
- STA-alone DID work in Era 1 and Era 3
- The "STA never worked" doctrine was an over-correction from Era 5 Portable Mode failure
- MUST NOT suggest concurrent AP+STA

### Query 4: WebSocket protocol contract (cross-cutting reference)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="What is the WebSocket command contract for K1? What rules govern adding or modifying WebSocket commands? Where is the contract defined?"
)
```

**Ground truth to verify against:**
- Contract: docs/protocol/k1-ws-contract.yaml
- Gate rule: read the contract before adding/modifying any WS command
- Update the contract FIRST, then implement
- Do NOT add WS commands without updating the YAML
- Do NOT consume commands from Tab5/iOS without verifying field names match the contract
- Should mention the REST contract too (k1-rest-contract.yaml)

### Query 5: Cross-notebook query — iOS ↔ firmware interface

```
cross_notebook_query(
    query="How does the iOS companion app communicate with the K1 firmware? What protocols, endpoints, and state machines are involved on both sides?",
    notebook_names="Lightwave-Ledstrip — K1 Project Knowledge Base (forensic-corrected 2026-05-04)"
)
```

**Note:** This tests `cross_notebook_query`. The answer should primarily come from the Lightwave-Ledstrip notebook since iOS sources are bundled there. If the tool can also reach the other notebooks, even better — but verify it at least works with a single targeted notebook.

**Ground truth to verify against:**
- WiFi AP mode, iOS connects to K1 at 192.168.4.1
- REST API for commands (api-v1.md or api-v2.md)
- WebSocket for real-time state/streaming
- iOS side: RESTClient, WebSocketService, UDPStreamReceiver (all actors)
- ConnectionState FSM on iOS side
- Parameter slider debounce: 150ms minimum

## Phase 3: Grade and report

For each query, assign a grade:

| Grade | Criteria |
|-------|----------|
| **PASS** | Answer is technically correct, file paths are accurate, constraints are cited with specific text, cross-references are useful. Agent could act on this without reading additional files. |
| **PARTIAL** | Answer is directionally correct but missing key details, file paths, or constraints. Agent would need to supplement with file reads. |
| **FAIL** | Answer is wrong, contradicts CLAUDE.md, suggests STA as viable without caveats, or fabricates file paths. |

**Pass threshold for the integration:** 4/5 PASS, 0 FAIL. If Query 3 (WiFi safety) is anything other than PASS, the integration does NOT proceed until the notebook is re-sterilised.

## Phase 4: Draft CLAUDE.md additions (only if Phase 3 passes)

If the test passes, draft a CLAUDE.md patch that adds NotebookLM to the tool hierarchy. The patch should:

### 4a. Add to "Context tools available" section

Insert as a new tier between the current #4 (auggie codebase-retrieval) and #5 (episodic-memory). Use this template:

```markdown
4. `notebook_query(notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4", query="...")` — **NotebookLM project oracle** — semantic Q&A over 127 sterilised project sources (architecture, constraints, decisions, protocol contracts, subsystem docs). Returns structured answers with file paths, constraints, and cross-references. Best for "what are the rules for X?", "how does subsystem Y work?", "what constraints apply to Z?". Does NOT replace clangd (code symbols), crispy (session transcripts), or claude-mem (extracted observations). Use for architectural/conceptual questions BEFORE opening files.
   - Follow-up queries: pass `conversation_id` from prior response to chain questions.
   - Cross-project queries: `cross_notebook_query(query="...", all=True)` searches all 7 SpectraSynq notebooks.
```

### 4b. Add to the tool routing table

Add these rows to the "I need to..." table:

```markdown
| Understand a subsystem's architecture | `notebook_query` with descriptive question | ~~reading 5+ reference docs~~ |
| Check what constraints apply to a change | `notebook_query` asking about constraints | ~~re-reading CLAUDE.md sections~~ |
| Understand why a design decision was made | `notebook_query` for architectural context | ~~grepping for comments~~ |
| Cross-project question (iOS ↔ firmware) | `cross_notebook_query` | ~~reading two CLAUDE.md files~~ |
```

### 4c. Add to the Agent Readback Protocol

Add `NotebookLM query` to the readback block:

```markdown
- NotebookLM query: [what architectural/constraint question I'll ask first, or "N/A — single-file edit"]
```

### 4d. Add notebook ID reference

Add a small reference section:

```markdown
### NotebookLM Reference

| Notebook | ID | Sources | Last synced |
|----------|----|---------|-------------|
| Lightwave-Ledstrip | `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4` | 127 | 2026-05-04 |

**Query patterns:**
- Subsystem understanding: "Explain the [audio chain / WiFi stack / effect system / WebSocket protocol]"
- Constraint check: "What constraints apply to [modifying render() / adding a WS command / touching WiFi code]?"
- Decision archaeology: "Why is [K1 AP-only / ESV11 the audio backend / BigInt used in VM]?"
- Cross-subsystem coupling: "How does [AudioActor affect RendererActor / Tab5 consume REST endpoints]?"

**When NOT to use NotebookLM:**
- Finding a specific symbol definition → clangd
- Checking what changed in the last session → crispy / claude-mem
- Verifying a function signature → clangd get_hover
- Running a build or test → pio / bash
```

## Phase 5: Write the CLAUDE.md patch

**Do NOT apply the patch directly to CLAUDE.md.** Write the patch to `docs/tooling/notebooklm-bundles/CLAUDE_MD_NOTEBOOKLM_PATCH.md` so Captain can review and apply it.

The patch file should contain:
1. The exact text to insert
2. The exact location (after which line/section)
3. A brief rationale for each addition

## Phase 6: Report

Output a summary:

```
NOTEBOOKLM INTEGRATION TEST — LIGHTWAVE-LEDSTRIP
---
Notebook configured: YES/NO
Custom prompt applied: YES/NO

Query results:
  Q1 (Audio chain):      [PASS/PARTIAL/FAIL] — [one-line summary]
  Q2 (Effect constraints): [PASS/PARTIAL/FAIL] — [one-line summary]
  Q3 (WiFi safety):      [PASS/PARTIAL/FAIL] — [one-line summary]
  Q4 (WS contract):      [PASS/PARTIAL/FAIL] — [one-line summary]
  Q5 (Cross-notebook):   [PASS/PARTIAL/FAIL] — [one-line summary]

Overall: [X/5 PASS, Y PARTIAL, Z FAIL]
Integration gate: [PASS — CLAUDE.md patch written / FAIL — details]

Patch location: docs/tooling/notebooklm-bundles/CLAUDE_MD_NOTEBOOKLM_PATCH.md
---
```

## Error handling

- If `chat_configure` fails: check `server_info()` for auth. If auth is expired, run `refresh_auth()`. Report if still broken.
- If `notebook_query` returns empty or errors: try `notebook_query_start` + `notebook_query_status` (async path) as fallback. The notebook has 127 sources which may need async queries.
- If `cross_notebook_query` fails: fall back to a regular `notebook_query` on the same notebook. Note the failure — cross-notebook may not work with only this notebook's name.
- Do NOT silently fall back. Report every failure per the Tool Failure Protocol in CLAUDE.md.
