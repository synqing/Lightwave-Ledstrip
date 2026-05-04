---
abstract: "Registry of all SpectraSynq NotebookLM notebooks — IDs, source counts, last-sync dates, and bundle paths. The reference for cross_notebook_query and re-sync operations. Read when invoking NotebookLM tools or planning a bundle refresh."
---

# NotebookLM Notebook Registry

Last updated: 2026-05-05

This registry is the single source of truth for SpectraSynq NotebookLM notebook IDs. Use these IDs with `mcp__notebooklm-mcp__notebook_query`, `mcp__notebooklm-mcp__cross_notebook_query`, and any future sync tooling.

| Notebook | ID | Sources | Last synced | Custom prompt | Bundle path |
|---|---|---|---|---|---|
| Lightwave-Ledstrip | `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4` | 127 | 2026-05-04 | ✓ 2026-05-04 (polished) | `docs/tooling/notebooklm-bundles/lightwave_ledstrip/` |
| K1 Testbed | `299713a2-a418-4904-9b7b-0e882f6d61a7` | 37 | 2026-05-04 | ✓ 2026-05-05 | `~/Workspace_Management/Software/SpectraSynq.K1_Testbed/docs/tooling/notebooklm-bundles/k1_testbed/` |
| War Room | `93b68c8c-edcb-453c-8d4b-a26edf923bb0` | 93 | 2026-05-04 | ✓ 2026-05-05 | `~/Workspace_Management/Software/Obsidian.warroom/docs/tooling/notebooklm-bundles/warroom_governance/` |
| K1 Launch Planning | `3400c77e-9687-424b-99cd-ab879545b264` | 47 | 2026-05-04 | ✓ 2026-05-05 | `~/SpectraSynq_K1_Launch_Planning/docs/tooling/notebooklm-bundles/k1_launch_planning/` |
| K1 Marketing | `723ee917-9fe1-4c95-ae1a-d42b99f01682` | 91 | 2026-05-04 | ✓ 2026-05-05 | `~/K1_Marketing/docs/tooling/notebooklm-bundles/k1_marketing/` |
| SpectraSynq.LandingPage | `3dea7471-3b3a-4a28-b9c1-8b97de6affb2` | 89 | 2026-05-04 | ✓ 2026-05-05 | `~/SpectraSynq.LandingPage/docs/tooling/notebooklm-bundles/landing_page/` |
| PRISM.studio | `70ccd467-9e70-4b22-b891-153bc4367cd1` | 66 | 2026-05-04 | ✓ 2026-05-05 | `~/Workspace_Management/Software/PRISM.studio/docs/tooling/notebooklm-bundles/prism_studio/` |

All 7 notebooks now have agent-optimised custom prompts mandating the 5-section response format (ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS) with project-specific safety rails (FROZEN core/, Stage 5 Captain gate, $369 floor, Direction C lock, centre-origin discipline, BigInt-only VM arithmetic, etc.).

## Usage

**Single-notebook query:**
```
mcp__notebooklm-mcp__notebook_query(
    notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4",
    query="..."
)
```

**Cross-notebook query** (e.g. iOS↔firmware questions, marketing↔technical alignment):
```
mcp__notebooklm-mcp__cross_notebook_query(
    query="...",
    notebook_names="Lightwave-Ledstrip, SpectraSynq.LandingPage"
)
```

## Sync discipline

When a bundle is regenerated:

1. Update `Sources` count and `Last synced` date in this table.
2. If the notebook ID has changed (rare — only on re-creation), update it everywhere it is referenced (CLAUDE.md, this registry, any saved skills/scripts).
3. Verify connectivity with `mcp__notebooklm-mcp__server_info` before broadcasting the new bundle.

## Notebook configuration

All 7 SpectraSynq notebooks are configured with custom agent-optimised system prompts that mandate the 5-section response format: ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS. Each prompt encodes that project's hard constraints and sterilisation rules — see each project's `docs/tooling/notebooklm-bundles/CC_CLI_NOTEBOOKLM_INTEGRATION_PROMPT.md` (or equivalent) for the verbatim prompt content. Re-run `chat_configure` only if a response loses structure or breaches sterilisation.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Seeded with 7 SpectraSynq notebooks and IDs from Phase 5 of the NotebookLM integration prompt. |
| 2026-05-05 | Claude (claude-opus-4-7) | Cross-project replication complete. All 6 sibling notebooks (K1 Testbed, War Room, K1 Launch Planning, K1 Marketing, SpectraSynq.LandingPage, PRISM.studio) now have project-specific custom prompts configured. Test results: 12/12 queries graded A, 6/6 verdicts PASS. Added "Custom prompt" column to the registry table. CLAUDE.md NotebookLM Knowledge Base sections applied to all 6 sibling project CLAUDE.md files. |
