# CC Audit Protected Instruction Map

**Label:** GROUNDED. Line references are to the current working tree on 2026-05-05.

**Purpose:** Classify always-loaded or commonly routed instruction surfaces before any future compression/remediation work. This map is an audit artefact only; it does not edit or remove instructions.

---

## Summary

The protected core is not optional: RBDO, READBACK, hard constraints, root governance, memory routing, clangd/QMD/Context7 routing, and tool-failure discipline must remain immediately visible.

The compression opportunity is in detailed operational registries, stale routing snapshots, duplicate reference lists, and contradictions. Those should be repaired or moved behind references only after a patch review.

---

## Protected Inline Content

| Source | Lines | Classification | Reason |
|---|---:|---|---|
| `CLAUDE.md` RBDO gate | 3-39 | `KEEP inline` | Canonical tactical-output gate; `AGENTS.md` points back to it. |
| `CLAUDE.md` context/delegation | 43-66 | `KEEP inline` | Defines single-target vs multi-target and delegation triggers. |
| `CLAUDE.md` session start + memory routing | 68-96 | `KEEP inline` | Preserves `$RECALL_CLI`, claude-mem `mcp-search`, NotebookLM/current-source boundaries. |
| `CLAUDE.md` READBACK | 98-133 | `KEEP inline` | Mandatory per-agent pre-work protocol. |
| `CLAUDE.md` clangd gate | 143-161 | `KEEP inline` | Protected C++ symbol-navigation rule. |
| `CLAUDE.md` QMD gate | 163-174 | `KEEP inline` | Protected documentation-search routing. |
| `CLAUDE.md` NotebookLM gate | 176-196 | `KEEP inline` | Architecture oracle routing plus current-source caveat. |
| `CLAUDE.md` Context7 gate | 198-208 | `KEEP inline` | Protected external API/library fact rule. |
| `CLAUDE.md` tool failure + pre-commit gates | 232-256 | `KEEP inline` | Stop-on-tool-fail and pre-commit confidence gates. |
| `CLAUDE.md` hard constraints | 258-266 | `KEEP inline` | Core firmware/audio/WiFi/British-English constraints. |
| `CLAUDE.md` root allowlist/workspace rules | 268-306 | `KEEP inline` | Prevents root-file sprawl and orphan artefacts. |
| `CLAUDE.md` parallel sandboxing | 427-440 | `KEEP inline` | Supports RBDO hard stop around sandbox-to-integration loss. |
| `AGENTS.md` RBDO pointer | 3-5 | `KEEP inline` | Short pointer to canonical RBDO. |
| `AGENTS.md` memory routing | 9-11 | `KEEP inline` | Concise current claude-mem route and generated-context warning. |
| `AGENTS.md` workflow discipline R1-R5 | 38-50 | `KEEP inline` | Referenced by RBDO as protected governance. |
| `AGENTS.md` visual pipeline guardrails | 52-64 | `KEEP inline` | Operational safety invariants not fully duplicated elsewhere. |

---

## Keep Referenced

| Source | Lines | Classification | Reason |
|---|---:|---|---|
| `CLAUDE.md` protocol/LVGL gates | 135-141 | `KEEP referenced` | Task-specific gates; important but not always global. |
| `CLAUDE.md` lifecycle + parallel execution | 210-230 | `KEEP referenced` | Useful workflow doctrine; can sit behind routing docs. |
| `CLAUDE.md` architecture/build/tracing | 308-361 | `KEEP referenced` | Useful source-truth context; too detailed for hot inline rules. |
| `CLAUDE.md` iOS/subagent protocol | 363-425 | `KEEP referenced` | Important for iOS/subagent scopes, not every session. |
| `CLAUDE.md` RTK | 442-457 | `KEEP referenced` | Operational detail; not a protected invariant. |
| `CLAUDE.md` further docs + NotebookLM KB | 481-528 | `KEEP referenced` | Registry/reference material; needs AP-only wording repair. |
| `CLAUDE.md` autocontext/gstack/Crispy | 530-624 | `KEEP referenced` | Tooling-specific routing; preserve as referenced operational material. |
| `docs/WORKFLOW_ROUTING.md` purpose | 1-18 | `KEEP referenced` | Explains routing doc role. |
| `docs/WORKFLOW_ROUTING.md` clangd table | 43-57 | `KEEP referenced` | Detailed companion to inline clangd gate. |
| `docs/WORKFLOW_ROUTING.md` QMD/Context7 | 59-72 | `KEEP referenced` | Detailed companion to inline QMD/Context7 gates. |
| `docs/WORKFLOW_ROUTING.md` RTK | 164-166 | `KEEP referenced` | Matches `CLAUDE.md` summary. |
| `docs/WORKFLOW_ROUTING.md` anti-patterns/decision tree | 193-245 | `KEEP referenced` | Preserve clangd/QMD/memory anti-patterns; prune stale rows separately. |

---

## Duplicates, Stale Sections, And Contradictions

| Source | Lines | Classification | Reason |
|---|---:|---|---|
| `CLAUDE.md` allowlist count labels | 276-278 | `CONTRADICTION` | Count labels do not match listed entries; entries should stay, counts need correction. |
| `CLAUDE.md` handoff protocol | 459-479 | `CONTRADICTION` | Requires `.claude/handoff.md` next steps; conflicts with `AGENTS.md` ban on forward task handoffs. |
| `CLAUDE.md` NotebookLM AP-only wording | 505 | `CONTRADICTION` | AP-only-ever wording conflicts with current dual-mode/pure-STA doctrine in `CLAUDE.md:260`. |
| `AGENTS.md` build block | 13-28 | `CONTRADICTION` | Calls PipelineCore primary; root `CLAUDE.md` says ESV11 `_32khz` paths are canonical. |
| `AGENTS.md` hard constraints | 30-36 | `STALE` | Subset only; omits WiFi/audio safety detail now in root `CLAUDE.md`. |
| `AGENTS.md` further docs | 66-77 | `DUPLICATE` | Mostly overlaps root further-doc registry. |
| `.claude/CLAUDE.md` generated context | 1-13 | `STALE` | Jan 26 generated claude-mem block only; generated context should not be edited casually. |
| `docs/WORKFLOW_ROUTING.md` phase 0 | 20-38 | `STALE` | Lacks `$RECALL_CLI` and NotebookLM routing now in root `CLAUDE.md`; Auggie remains not configured. |
| `docs/WORKFLOW_ROUTING.md` memory table | 73-83 | `STALE` | Older/partial memory path versus root session-start route. |
| `docs/WORKFLOW_ROUTING.md` browser/design/planning/tooling | 84-162 | `UNKNOWN` | Many rows are removed/not configured; needs live tool availability audit before edits. |
| `docs/WORKFLOW_ROUTING.md` Ralph | 179-189 | `STALE` | Says not active but still gives invocation guidance. |
| `docs/WORKFLOW_ROUTING.md` history | 249-256 | `STALE` | Header says last updated 02 May 2026, history stops 31 Mar 2026. |

---

## Compression Recommendation

Do not compress by deleting protected gates. The safe order is:

1. Repair contradictions.
2. Move detailed registries to referenced docs.
3. Collapse duplicate further-doc lists.
4. Update stale workflow-routing rows from live tool state.
5. Re-run word counts and compare against the `12,112` word baseline.
