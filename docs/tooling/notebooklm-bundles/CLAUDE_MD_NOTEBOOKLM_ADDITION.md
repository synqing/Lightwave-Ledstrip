---
abstract: "Draft additions to firmware-v3/Lightwave-Ledstrip CLAUDE.md to wire the NotebookLM knowledge oracle into agent workflows. Phase 4 deliverable from the NotebookLM integration test (Phase 3 = PROCEED, overall grade A). Read before applying changes to CLAUDE.md."
---

# CLAUDE.md — NotebookLM Integration Additions (DRAFT, NOT YET APPLIED)

**Status:** DRAFT — produced by Phase 4 of the NotebookLM integration test. Phase 3 grade: **A** (PROCEED). Apply only after Captain review.

**Source notebook:** `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4` — Lightwave-Ledstrip — K1 Project Knowledge Base (sterilisation-corrected 2026-05-04, 127 sources).

**Test report summary:** All 5 representative queries graded A — audio pipeline, effect constraints, WiFi sterilisation, protocol contract, iOS cross-project. Format compliance Y (all 5 sections present), citation accuracy Y (no hallucinated paths), sterilisation hold Y (AP-only enforcement intact, STA never proposed as viable production path).

---

## 4a. Context tools hierarchy — insert as item 4.5

In the **Session Start (MANDATORY — every session, no exceptions)** section, insert this entry between current item 4 (`mcp__plugin_claude-mem_mcp-search__get_observations`) and the "Current source truth" item:

```markdown
4.5. `mcp__notebooklm-mcp__notebook_query(notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4", query="...")` — **NotebookLM knowledge oracle** — pre-indexed architectural knowledge across 127 sterilised sources. Returns structured answers in 5 mandatory sections: ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS. Use for "what/why" questions about architecture, constraints, design decisions, and subsystem relationships before reading reference docs. Cuts cold-start cost from ~30K tokens (reading 5+ reference docs) to one API call. Prefer `notebook_query_start` + `notebook_query_status` (async) for broad multi-section questions — synchronous calls may exceed the 60 s socket timeout on whole-corpus retrieval.

   **Do NOT use NotebookLM for:** code symbol navigation (use clangd), current file contents (use Read), git/session history (use Crispy `$RECALL_CLI` or claude-mem), or anything where freshness against today's HEAD matters (the notebook is a periodic snapshot — verify against current source before any code edit).
```

The numbering keeps the existing memory-order intent (crispy → claude-mem → NotebookLM → current source). Do NOT renumber the surrounding items; "4.5" preserves the hierarchy without churn elsewhere.

---

## 4b. Routing table additions

Add the following block to the existing **Tool Enforcement** section, immediately after the Documentation Search QMD table and before the Library APIs Context7 table. Header: `### Architectural Knowledge — NotebookLM FIRST, reference docs LAST`.

```markdown
### Architectural Knowledge — NotebookLM FIRST, reference docs LAST

**Gate rule:** When you need to understand a subsystem's architecture, why a decision was made, what constraints apply across multiple files, or how two subsystems interact — query NotebookLM before reading any `docs/reference/*.md`, `EFFECT_DEVELOPMENT_STANDARD.md`, or cross-project docs. The notebook returns the structured answer in one call; reading the equivalent docs costs ~30K tokens.

| I need to understand... | Call this | NOT this |
|---|---|---|
| A subsystem's architecture before touching code | `mcp__notebooklm-mcp__notebook_query` | ~~Reading 5+ reference docs~~ |
| What constraints apply to a planned change | `mcp__notebooklm-mcp__notebook_query` | ~~Scanning CLAUDE.md sections~~ |
| Why a design decision was made | `mcp__notebooklm-mcp__notebook_query` | ~~Grepping commit history~~ |
| Cross-subsystem interactions (iOS↔firmware↔Tab5) | `mcp__notebooklm-mcp__notebook_query` or `cross_notebook_query` | ~~Spawning 3 subagents to read 3 codebase-maps~~ |
| Where a C++ symbol is defined | `mcp__clangd__find_definition` | ~~notebook_query~~ |
| Current file contents | `Read` | ~~notebook_query~~ |
| What changed since last session | Crispy `$RECALL_CLI` / claude-mem | ~~notebook_query~~ |
| Whole-corpus broad question (likely >60 s) | `notebook_query_start` + `notebook_query_status` | ~~`notebook_query` (will time out)~~ |

**When NotebookLM is NOT acceptable:**
- Any code edit that depends on today's HEAD state — the notebook is a snapshot, not a live mirror. After NotebookLM gives you the architectural answer, verify with clangd / Read before writing code.
- Symbol navigation in C++ — clangd is canonical, NotebookLM is conceptual.
- Git history forensics — use `git log` / `git blame`, not NotebookLM.

**Tool failure:** If `notebook_query` times out or errors and the async fallback also fails, STOP per the Tool Failure Protocol — do NOT silently fall back to reading reference docs without flagging the degradation. The doc-read fallback is acceptable only after explicit Captain approval.
```

---

## 4c. Agent Readback Protocol addition

In the **Agent Readback Protocol (MANDATORY — every session, every agent)** section, add a new line to the readback template (insert after the `Tool routing:` line):

```markdown
- NotebookLM: [will I query the knowledge base before reading reference docs? If yes, the exact question I will ask. If no (e.g. trivial single-file edit), why not.]
```

The intent is to make the NotebookLM-first habit a forced choice in every readback, not a silent omission.

---

## 4d. Notebook ID registry section

Add this section to the **Further Docs** area of CLAUDE.md, immediately above the existing reference table (or in a new top-level `## NotebookLM Knowledge Base` section between `## Further Docs` and the existing autocontext section — orchestrator's call):

```markdown
## NotebookLM Knowledge Base

| Notebook | ID | Sources | Use when... |
|---|---|---|---|
| Lightwave-Ledstrip | `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4` | 127 | Architecture, constraints, design decisions, cross-subsystem questions for firmware-v3, lightwave-ios-v2, tab5-encoder, protocol contracts, audio pipeline, WiFi, governance |

Full SpectraSynq notebook registry (7 notebooks, IDs, source counts, bundle paths): [`docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md`](../docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md).

Custom system prompt (configured 2026-05-04) mandates a 5-section response format: ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS. The prompt enforces British English, AP-only-WiFi sterilisation, no-heap-in-render warnings, and centre-origin guidance on every effect-related answer. If a response loses the structure or breaches sterilisation, re-run `chat_configure` per `docs/tooling/notebooklm-bundles/CC_CLI_NOTEBOOKLM_INTEGRATION_PROMPT.md`.
```

---

## 4e. Cross-notebook query guidance

Add as a sub-section of the NotebookLM Knowledge Base block above:

```markdown
### Cross-notebook queries

For questions spanning multiple SpectraSynq projects (e.g. "how does the marketing positioning of K1 align with the technical AP-only constraint?", "where does PRISM.studio reference K1 protocol contracts?"), use:

```
mcp__notebooklm-mcp__cross_notebook_query(
    query="...",
    notebook_names="Lightwave-Ledstrip, SpectraSynq.LandingPage"
)
```

Available notebooks (see `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md` for IDs, sources, last-sync dates):

- **Lightwave-Ledstrip** — firmware/iOS/Tab5 codebase + protocol + governance (127 sources)
- **K1 Testbed** — testbed/dev hardware + capture rigs (37 sources)
- **War Room** — governance, doctrine, decision history (93 sources)
- **K1 Launch Planning** — launch checklist, demo plans, gating (47 sources)
- **K1 Marketing** — positioning, copy, banned language, audience (91 sources)
- **SpectraSynq.LandingPage** — landing-page Next.js + R3F site (89 sources)
- **PRISM.studio** — PRISM compositor/studio app (66 sources)

Cross-notebook is rate-limited — prefer single-notebook queries when one notebook clearly owns the answer.
```

---

## Optional polish — APPLIED 2026-05-04

Phase 3 SSA flagged one borderline observation in the Query 4 (protocol contract) response: the notebook surfaces a captured decision that frames the YAML contract as "regeneratable documentation, not lock-and-conform". This is a real Captain note from the corpus, but a weaker reader could interpret it as licence to skip the contract-first gate.

**Captain decision (2026-05-04):** APPLY. Cost is one sentence; risk of a future agent reading "regeneratable artefact" as permission to skip the gate is non-zero.

The following rule was added to the `chat_configure` custom prompt and is now LIVE on notebook `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4`:

> The WS contract-first gate (`docs/protocol/k1-ws-contract.yaml` updated BEFORE implementation) is non-negotiable. The "regeneratable artefact" framing applies to retrospective doc reconciliation, not to gate bypass during forward implementation. Always cite `k1-ws-contract.yaml` as the gate, even when surfacing the regeneratable-artefact decision.

A smoke re-test of Query 4 was run after re-configuration to verify the rule landed without regression elsewhere — see Document Changelog footer for the verification date and outcome.

---

## Application checklist (do NOT apply automatically)

Before merging these additions into the canonical `CLAUDE.md`:

- [ ] Captain review of all five sub-sections (4a–4e).
- [x] Decide on the optional polish (custom-prompt tightening for the WS contract gate). **APPLIED 2026-05-04.**
- [ ] Confirm the canonical placement of the NotebookLM Knowledge Base section (top-level vs sub-section of `## Further Docs`).
- [ ] After application, run a smoke query (e.g. Query 3 — WiFi sterilisation) to confirm the notebook still enforces AP-only after any subsequent `chat_configure` adjustments.
- [ ] Cross-link `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md` from the relevant `MEMORY.md` index entry under **Reference**.

---

## RBDO label

**GROUNDED** — every claim in this draft sourced either from the SSA Phase 3 report (graded against verbatim canonical truth in the dispatch prompt), the original integration prompt, or `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md` (Phase 5). No assumption left unresolved. The draft is presentational only — no firmware behaviour change is proposed.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Drafts CLAUDE.md additions per Phase 4 of the NotebookLM integration prompt — context-tools hierarchy entry (4a), routing table (4b), readback addition (4c), notebook ID registry section (4d), cross-notebook guidance (4e). Optional polish on contract-first gate noted. NOT yet applied to CLAUDE.md — awaits Captain review. |
| 2026-05-04 | Claude (claude-opus-4-7) | Optional polish APPLIED per Captain decision. Re-ran `chat_configure` with the contract-first rule appended to the Rules block (custom prompt now ~2,000 chars, well under 10K). Smoke-tested via Query 4 (WS protocol contract). VERIFIED: response now leads ANSWER, CONSTRAINTS, and WARNINGS with the contract-first gate verbatim; subordinates the regeneratable-artefact framing to retrospective doc reconciliation only; no regression to AP-only / centre-origin / no-heap-in-render answers. Application checklist updated. |
