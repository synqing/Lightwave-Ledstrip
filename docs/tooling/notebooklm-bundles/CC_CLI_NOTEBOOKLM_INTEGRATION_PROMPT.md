# NotebookLM Integration — Lightwave-Ledstrip CC CLI Agent Prompt

> **Run this from a CC CLI session** in the Lightwave-Ledstrip project directory (`~/Workspace_Management/Software/Lightwave-Ledstrip/`). Requires `notebooklm-mcp` MCP server to be registered and available. If tools are not available, check `.mcp.json` configuration.

## Context

We have uploaded 127 sterilised sources to a NotebookLM notebook covering the entire Lightwave-Ledstrip project: firmware architecture, iOS app, Tab5 controller, protocol contracts, effect development standards, audio pipeline, WiFi constraints, and governance docs.

The goal is to wire this notebook into the CC CLI agent workflow as an architectural/conceptual knowledge oracle — the tool agents query BEFORE they start reading source files. This reduces cold-start token burn from ~30K (reading reference docs) to a single API call.

**Notebook ID:** `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4`
**Notebook title:** Lightwave-Ledstrip — K1 Project Knowledge Base (forensic-corrected 2026-05-04)

## Phase 1: Configure the notebook for agent consumption

Use `chat_configure` to set a custom system prompt that shapes ALL subsequent query responses for agent consumption rather than human-readable prose.

```
chat_configure(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    goal="custom",
    custom_prompt="You are answering questions from a software engineering agent (Claude Code) working on this ESP32-S3 LED controller codebase. The agent has tools to read source files and navigate code symbols — it does NOT need you to explain what code does. It needs you to explain WHY decisions were made, WHAT constraints apply, and WHERE to find things.\n\nResponse format (MANDATORY for every answer):\n\n1. ANSWER: Direct technical answer in 2-5 sentences. Be precise and terse. No preamble.\n\n2. CONSTRAINTS: List any hard constraints, invariants, or rules that apply to this topic. Include the source document name for each.\n\n3. KEY FILES: List the most relevant source file paths mentioned in the uploaded documents. Use exact paths as they appear in the sources.\n\n4. CROSS-REFS: If the question touches multiple subsystems (audio, effects, network, iOS, Tab5), list which other subsystems are affected and why.\n\n5. WARNINGS: Any tripwires, anti-patterns, or 'do NOT do this' rules relevant to the question. These are critical — agents that miss warnings produce regressions.\n\nRules:\n- British English always.\n- Never suggest STA WiFi mode as viable. K1 is AP-only in production. If asked about WiFi, always include the AP-only constraint.\n- Never suggest heap allocation in render paths.\n- Centre-origin (LED 79/80 outward) applies to ALL effects — mention it whenever effects are discussed.\n- If the sources do not contain enough information to answer confidently, say so explicitly. Do NOT fabricate.\n- Prefer quoting exact constraint language from the source documents over paraphrasing.",
    response_length="default"
)
```

**Verify:** After configuration, confirm the response with a brief status check. The tool should return success.

## Phase 2: Test queries (5 representative scenarios)

Run these 5 queries against the configured notebook. For each query, record: (a) the full response, (b) whether the response follows the 5-section format, (c) whether the cited files/constraints are accurate, (d) a quality grade (A/B/C/F).

**Quality rubric:**
- **A**: Correct, structured, actionable, cites real files, includes relevant warnings. An agent could act on this without reading any other docs.
- **B**: Correct but missing structure, or includes unnecessary prose, or misses a relevant constraint. Usable but not optimal.
- **C**: Partially correct, or cites wrong files, or misses critical constraints. Dangerous if agent trusts it.
- **F**: Wrong, fabricated, or contradicts canonical truth. Kill signal — reconfigure before proceeding.

### Query 1: Audio pipeline architecture (cross-subsystem)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="Describe the audio data flow from microphone input to LED output. What actors are involved, which cores do they run on, and what are the timing constraints?"
)
```

**Expected in response:** AudioActor (Core 0), RendererActor (Core 1), ESV11 backend at 32 kHz / 125 Hz frame rate, ControlBus as shared state, 2.0ms per-frame ceiling, no heap in render(). Should cite `CLAUDE.md`, `codebase-map.md`, and potentially `ControlBus.h` bundle.

### Query 2: Effect development constraints (safety-critical)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="I need to write a new audio-reactive LED effect. What are ALL the constraints, rules, and patterns I must follow? Include the centre-origin rule, heap restrictions, timing budget, and any anti-patterns."
)
```

**Expected in response:** Centre origin 79/80, no heap in render(), 2.0ms ceiling, no rainbows, zone bounds checking, dt-correction, inheriting EffectBase, implementing render(RenderContext&). Should cite `EFFECT_DEVELOPMENT_STANDARD.md`, `CLAUDE.md`, `audio-visual-semantic-mapping.md`.

### Query 3: WiFi constraints (sterilisation verification)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="Can K1 connect to a home WiFi network as a client (STA mode)? What's the current WiFi architecture and what are the constraints?"
)
```

**Expected in response:** MUST say K1 is AP-only in production. Should explain the `WIFI_AP_ONLY` build flag, the `m_forceApOnly` runtime lock. Should mention the historical context (STA has been attempted and failed). Must NOT suggest STA as a viable path. This is the sterilisation litmus test.

### Query 4: Protocol contract (precision test)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="What WebSocket commands does K1 expose? What is the contract for adding a new command? Where is the single source of truth for the WS protocol?"
)
```

**Expected in response:** Should cite `k1-ws-contract.yaml` as the single source of truth. Should mention the gate rule: update contract FIRST, then implement. Should list some actual commands from the contract.

### Query 5: Cross-project question (iOS ↔ firmware)

```
notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="How does the iOS companion app connect to K1? What network protocol does it use, what's the connection state machine, and what are the iOS-side architectural constraints?"
)
```

**Expected in response:** AP-mode connection to 192.168.4.1, WebSocket for real-time, REST for commands. iOS uses `RESTClient` and `WebSocketService` (both actors). Should mention `@MainActor @Observable` ViewModels, 150ms debounce on parameter sliders, `[weak self]` in task closures. Should cite iOS `codebase-map.md` and `fsm-reference.md`.

## Phase 3: Evaluate and report

After running all 5 queries, produce a structured report:

```
NOTEBOOKLM INTEGRATION TEST REPORT
===================================

Notebook: 92d45c0b-83c7-4971-aa9a-2c9ee13b06d4
Chat config: custom (agent-optimised)
Date: [today]

Query 1 — Audio pipeline:     [A/B/C/F] — [one-line reason]
Query 2 — Effect constraints:  [A/B/C/F] — [one-line reason]
Query 3 — WiFi (sterilisation):[A/B/C/F] — [one-line reason]
Query 4 — Protocol contract:   [A/B/C/F] — [one-line reason]
Query 5 — iOS cross-project:   [A/B/C/F] — [one-line reason]

Overall grade: [A/B/C/F]

Format compliance: [did responses follow the 5-section format? Y/N]
Citation accuracy: [did cited files/constraints match reality? Y/N + notes]
Sterilisation hold: [did WiFi query correctly enforce AP-only? Y/N]

RECOMMENDATION: [PROCEED / RECONFIGURE / ABORT]
- If PROCEED: move to Phase 4
- If RECONFIGURE: specify what to change in chat_configure
- If ABORT: explain what's fundamentally broken
```

## Phase 4: Draft CLAUDE.md integration (only if Phase 3 = PROCEED)

If the test queries pass (overall grade A or B), draft the additions to `CLAUDE.md`. Do NOT apply them — write to `docs/tooling/notebooklm-bundles/CLAUDE_MD_NOTEBOOKLM_ADDITION.md` for Captain review.

The draft should include:

### 4a. New entry in the context tools hierarchy

Insert after the existing claude-mem entries (currently items 2-4) and before "Current source truth" (currently item 5). Example placement:

```
4.5. `notebook_query(notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4", query="...")` — **NotebookLM** — pre-indexed architectural knowledge across 127 sterilised sources. Returns structured answers with file paths, constraints, warnings, and cross-references. Use for "what/why" questions about architecture, constraints, design decisions, and subsystem relationships. Do NOT use for code symbol navigation (use clangd), current file contents (use Read), or session history (use crispy/claude-mem).
```

### 4b. New routing table entries

Add to the existing "When to use" tables:

```
| I need to understand... | Call this | NOT this |
|---|---|---|
| A subsystem's architecture before touching code | `notebook_query` | Reading 5+ reference docs |
| What constraints apply to a change | `notebook_query` | Scanning CLAUDE.md sections |
| Why a design decision was made | `notebook_query` | Grepping commit history |
| Cross-project interactions (iOS↔firmware↔Tab5) | `notebook_query` | Spawning 3 subagents to read 3 codebase-maps |
| Where a C++ symbol is defined | clangd | ~~notebook_query~~ |
| What changed since last session | crispy/claude-mem | ~~notebook_query~~ |
| Current file contents | Read tool | ~~notebook_query~~ |
```

### 4c. Agent Readback Protocol addition

Add to the readback template:

```
- NotebookLM: [will I query the knowledge base before reading files? If yes, what question?]
```

### 4d. Notebook ID registry

Add a reference section:

```
### NotebookLM Knowledge Base

| Notebook | ID | Sources | Use when... |
|---|---|---|---|
| Lightwave-Ledstrip | `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4` | 127 | Architecture, constraints, design decisions, cross-subsystem questions |
```

### 4e. Cross-notebook query guidance (for cross-project work)

```
For questions spanning multiple projects (e.g., "how does iOS connect to K1?"), use `cross_notebook_query` with relevant notebook names. Available notebooks:
- Lightwave-Ledstrip, SpectraSynq.LandingPage, PRISM.studio, K1 Marketing, K1 Launch Planning, War Room, K1 Testbed
```

## Phase 5: Record notebook IDs for future reference

Write a `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md` file containing all 7 notebook IDs, their titles, source counts, and the date they were last synced. This becomes the reference for all future sync operations.

```markdown
# NotebookLM Notebook Registry

Last updated: [today]

| Notebook | ID | Sources | Last synced | Bundle path |
|---|---|---|---|---|
| Lightwave-Ledstrip | 92d45c0b-83c7-4971-aa9a-2c9ee13b06d4 | 127 | 2026-05-04 | docs/tooling/notebooklm-bundles/lightwave_ledstrip/ |
| K1 Testbed | 299713a2-a418-4904-9b7b-0e882f6d61a7 | 37 | 2026-05-04 | ~/Workspace_Management/Software/SpectraSynq.K1_Testbed/docs/tooling/notebooklm-bundles/k1_testbed/ |
| War Room | 93b68c8c-edcb-453c-8d4b-a26edf923bb0 | 93 | 2026-05-04 | ~/Workspace_Management/Software/Obsidian.warroom/docs/tooling/notebooklm-bundles/warroom_governance/ |
| K1 Launch Planning | 3400c77e-9687-424b-99cd-ab879545b264 | 47 | 2026-05-04 | ~/SpectraSynq_K1_Launch_Planning/docs/tooling/notebooklm-bundles/k1_launch_planning/ |
| K1 Marketing | 723ee917-9fe1-4c95-ae1a-d42b99f01682 | 91 | 2026-05-04 | ~/K1_Marketing/docs/tooling/notebooklm-bundles/k1_marketing/ |
| SpectraSynq.LandingPage | 3dea7471-3b3a-4a28-b9c1-8b97de6affb2 | 89 | 2026-05-04 | ~/SpectraSynq.LandingPage/docs/tooling/notebooklm-bundles/landing_page/ |
| PRISM.studio | 70ccd467-9e70-4b22-b891-153bc4367cd1 | 66 | 2026-05-04 | ~/Workspace_Management/Software/PRISM.studio/docs/tooling/notebooklm-bundles/prism_studio/ |
```

## Error handling

- If `chat_configure` fails, check that `notebooklm-mcp` is registered in `.mcp.json` and the server is running. Try `server_info()` to verify connectivity.
- If `notebook_query` returns empty or errors, try `notebook_get("92d45c0b-83c7-4971-aa9a-2c9ee13b06d4")` to verify the notebook exists and has sources.
- If queries take >60s, switch to async: `notebook_query_start` + poll `notebook_query_status`.
- If the custom prompt is rejected (>10K chars), trim the rules section. The current prompt is ~1,800 chars — well under the 10K limit.
- Report all failures per the Tool Failure Protocol in CLAUDE.md. Do not silently work around a broken MCP.
