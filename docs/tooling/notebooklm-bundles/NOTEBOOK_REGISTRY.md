---
abstract: "Registry of all SpectraSynq NotebookLM notebooks — IDs, source counts, last-sync dates, and bundle paths. The reference for cross_notebook_query and re-sync operations. Read when invoking NotebookLM tools or planning a bundle refresh."
---

# NotebookLM Notebook Registry

Last updated: 2026-05-14

This registry is the single source of truth for SpectraSynq NotebookLM notebook IDs. Use these IDs with `mcp__notebooklm-mcp__notebook_query`, `mcp__notebooklm-mcp__cross_notebook_query`, and any future sync tooling.

| Notebook | ID | Sources | Last synced | Custom prompt | Bundle path |
|---|---|---|---|---|---|
| Lightwave-Ledstrip | `cafda274-4f59-40c0-b178-28a12ab121fc` | 167 | 2026-05-14 | ✓ 2026-05-14 | `~/Workspace_Management/Software/Lightwave-Ledstrip/docs/tooling/notebooklm-bundles/lightwave_ledstrip/` |
| K1 Testbed | `299713a2-a418-4904-9b7b-0e882f6d61a7` | 37 | 2026-05-04 | ✓ 2026-05-05 | `~/Workspace_Management/Software/SpectraSynq.K1_Testbed/docs/tooling/notebooklm-bundles/k1_testbed/` |
| War Room | `93b68c8c-edcb-453c-8d4b-a26edf923bb0` | 93 | 2026-05-04 | ✓ 2026-05-05 | `~/Workspace_Management/Software/Obsidian.warroom/docs/tooling/notebooklm-bundles/warroom_governance/` |
| K1 Launch Planning | `3400c77e-9687-424b-99cd-ab879545b264` | 47 | 2026-05-04 | ✓ 2026-05-05 | `~/SpectraSynq_K1_Launch_Planning/docs/tooling/notebooklm-bundles/k1_launch_planning/` |
| K1 Marketing | `fa31897d-d25e-4925-a266-928bf2bd109b` | 200 | 2026-05-14 | ✓ 2026-05-14 | `~/K1_Marketing/notebooklm_bundles/k1_marketing/` |
| SpectraSynq.LandingPage | `167557f4-606b-4b2e-8636-bfd38d982ef4` | 127 | 2026-05-14 | ✓ 2026-05-14 | `~/SpectraSynq.LandingPage/docs/tooling/notebooklm-bundles/landing_page/` |
| PRISM.studio | `70ccd467-9e70-4b22-b891-153bc4367cd1` | 66 | 2026-05-04 | ✓ 2026-05-05 | `~/Workspace_Management/Software/PRISM.studio/docs/tooling/notebooklm-bundles/prism_studio/` |
| Synesthesia | `8e36af95-7daa-4508-bd60-bca00ce61ee2` | 85 | 2026-05-13 | ✓ 2026-05-13 | `~/Workspace_Management/Software/Synesthesia/Docs/tooling/notebooklm-bundles/synesthesia/` |
| Hybrid Beat Tracker | `14f9f49c-addc-47a2-a299-f59b280686c3` | 28 | 2026-05-13 | ✓ 2026-05-13 | `~/Workspace_Management/Software/hybrid-beat-tracker/docs/tooling/notebooklm-bundles/hybrid_beat_tracker/` |
| K1 Visual Bible | `5ebfdb30-690f-4a90-8fc2-d1a6bc0e03ec` | 150 | 2026-05-14 | ✓ 2026-05-14 | `~/K1_Visual_Bible/docs/tooling/notebooklm-bundles/k1_visual_bible/` |
| SpectraSynq Doctrine & Architecture | `0648af9c-42f7-4c86-8cb1-b69a63c34c4d` | 38 | 2026-05-14 | ✓ 2026-05-14 | `~/Workspace_Management/Software/Agency.agents/docs/tooling/notebooklm-bundles/spectrasynq_doctrine/` |

All 11 notebooks now have agent-optimised custom prompts mandating the 5-section response format (ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS) with project-specific safety rails (FROZEN core/, Stage 5 Captain gate, $369 floor, Direction C lock, centre-origin discipline, BigInt-only VM arithmetic, Synesthesia multi-variant disambiguation + DSP hard constants, Hybrid Beat Tracker Auto-BPM-RE attribution + Phase-2 deleted-module disclosure + crossref Tier-2 demarcation, etc.).

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

All 11 SpectraSynq notebooks are configured with custom agent-optimised system prompts that mandate the 5-section response format: ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS. Each prompt encodes that project's hard constraints and sterilisation rules — see each project's `docs/tooling/notebooklm-bundles/CC_CLI_NOTEBOOKLM_INTEGRATION_PROMPT.md` (or equivalent) for the verbatim prompt content. Re-run `chat_configure` only if a response loses structure or breaches sterilisation.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Seeded with 7 SpectraSynq notebooks and IDs from Phase 5 of the NotebookLM integration prompt. |
| 2026-05-05 | Claude (claude-opus-4-7) | Cross-project replication complete. All 6 sibling notebooks (K1 Testbed, War Room, K1 Launch Planning, K1 Marketing, SpectraSynq.LandingPage, PRISM.studio) now have project-specific custom prompts configured. Test results: 12/12 queries graded A, 6/6 verdicts PASS. Added "Custom prompt" column to the registry table. CLAUDE.md NotebookLM Knowledge Base sections applied to all 6 sibling project CLAUDE.md files. |
| 2026-05-13 | Claude (claude-opus-4-7[1m]) | Synesthesia notebook created and configured (id `8e36af95-7daa-4508-bd60-bca00ce61ee2`, 85 sources). Bundle covers: complete Docs tree (Synesthesia.RE re-engineering doctrine, Agent.Output research, _ARCHIVE historical, root status/sweep), all firmware variants (src/main ESP-IDF, k1_bt_p4nano BT shield, Synesthesia_AP RE'd reference, root Tab5), Quint formal model + tuning sweep tooling. Custom prompt enforces 11-tier authority hierarchy, DSP hard constants (NFFT=2048, HOP=1024, FS=48kHz, bass biquad coefficients), and multi-variant disambiguation. Phase-5 tests: 3/3 critical queries graded A+, including detection of a real 48kHz/44.1kHz bass-coefficient divergence between CLAUDE.md and `src/main/main_tab5_es7210_synesthesia_parity.cpp`. |
| 2026-05-13 | Claude (claude-opus-4-7[1m]) | Hybrid Beat Tracker notebook created and configured (id `14f9f49c-addc-47a2-a299-f59b280686c3`, 28 sources). Bundle covers: cross-platform port of Auto BPM (iOS app `no.douzette.Auto-BPM` v2.9.4 by Andre Douzette, December 2025) reverse-engineering combined with madmom academic baseline + Tab5.DSP Goertzel efficiency reference. Sources: 12 in-repo docs (incl. 99 KB foundational RE technical analysis), 14 code bundles (C++ core dsp/features/onset/tempo/tracker, Python pybind11, iOS Swift framework + demo, tests + madmom parity baselines + 21 ground-truth `.beats` annotations, build config), plus 4 Tier-2 cross-project context docs (Lightwave-Ledstrip MusicAware audit, Synesthesia Family B beat-tracker, academic compass-artifact survey). Custom prompt enforces 10-tier authority hierarchy with explicit in-repo Tier-1 vs cross-project Tier-2 demarcation, ATTRIBUTION WARNING (every Auto-BPM-derived algorithm citation must name source app), PHASE WARNING (3 tempo modules deleted in working tree as of bundle date), CROSSREF WARNING, PARITY WARNING (madmom comparisons). Phase-5 tests: 3/3 graded A+ including correctly stating "no active implementation" for deleted ACF modules and firing both ATTRIBUTION + PHASE warnings verbatim. Tab5.DSP duplicate of RE doc excluded (md5 match). |
| 2026-05-14 | Claude (claude-opus-4-7[1m]) | Major refresh — 2 new notebooks created + 3 stale notebooks rebuilt. **NEW: K1 Visual Bible** (id `5ebfdb30-690f-4a90-8fc2-d1a6bc0e03ec`, 150 sources) — canonical K1 hero render production doctrine; 143 .md governance docs (PRODUCT_TRUTH / SCENE_IDEATION / BLENDER_RENDERS / LANDING_PAGE / ART_BIBLE / DECISIONS / SSA_CONTEXT) + 105 evidence run reports + 4 code bundles (state JSON, scripts, evidence scripts, 171 evidence manifests). Custom prompt enforces 7-point Approval Rule (not 1-9 gates as initially spec'd — corrected to project reality), banned vocabulary (`approved`/`production-ready`/`hero-quality`/`premium`/`complete`/`done` as terminal states), active scene authority lock (`gaming_room_hero_FUCKING_REFERENCE_K_Cycles.blend`, camera `SV_Left_Top_3/4_125mm`, Cycles K-Cycles 5.1.0, Path B x2 scale 2.060372), forensic-only v16_synthesised warning, LGP-face neutrality, gold-caps trust anchor. Smoke PASS. **NEW: SpectraSynq Doctrine & Architecture** (id `0648af9c-42f7-4c86-8cb1-b69a63c34c4d`, 38 sources) — cross-cutting white-space coverage: Founder Boundary, NEXUS strategy, Agent Doctrine (SSA protocol + handoff playbooks + 10 representative agent definitions), L1/L2 KB (9 L1 + 13 L2 + 3 KB orientation), global CLAUDE.md, Agency.agents strategy material (playbooks/runbooks/activation). Custom prompt enforces FOUNDER BOUNDARY WARNING (redirect work to asking agent, never push routine validation to Captain), AUTHORITY WARNING, NEXUS PHASE WARNING, STALENESS WARNING. Smoke PASS — correctly refused to push Captain to dashboard, cited L1_00 forbidden-escalations + X_ACCESS_POLICY conservative pricing fallback. **REBUILD: Lightwave-Ledstrip** (id changed `92d45c0b...` → `cafda274-4f59-40c0-b178-28a12ab121fc`, 127 → 167 sources, +40 delta) — added MusicAware_Audit_And_Gap_Analysis (40 KB, first-class), Phase 1B runtime evidence (controlbus DRAM relocation + AudioCtx copy reduction + handoff Tier 2), SynqMatrix naming-review tree (12 docs covering audio contracts → backends → pipeline → renderer → actors → plugin API → network routes → handlers → serial → HAL → core utilities), VP_RENDER_PATH_LAYER_AUDIT + VP_VALIDATION_PROTOCOL + VP_STACK_INTROSPECTION, EFFECT_AUTHORING_STANDARD_V2, GOOD_LIGHT_SHOW_TAXONOMY, 13 canonical research summaries. Preserved STA-NEVER, ESV11 32kHz, centre-origin, 120 FPS, actor model, British English. Smoke PASS — surfaced SynqMatrix phase warning spontaneously. **REBUILD: SpectraSynq.LandingPage** (id changed `3dea7471...` → `167557f4-606b-4b2e-8636-bfd38d982ef4`, 89 → 127 sources, +38) — added Higgsfield video pipeline research, press kit (README/fact-sheet/founder-bio/product-info), hero video G1-G5 gate runner artefacts, content-pipeline lo-fi shot list, press pitch template, K1-BLENDER agent failure debrief + onboarding + naming standard + restructuring, K1-CC-CLI render execution brief, soft-capture deployment checklist, PHASE_MATCH_V2 + Light Wrangler reference. Preserved LAUNCH_TRUTH anchor ($369 USD, "Music. Made visible.", "Reserve Your K1", 2026-06-17, 100 FE units, Direction C lock). One LandingPage duplicate (`K1-LIGHT-WRANGLER-DEPLOYMENT-REFERENCE.md`) detected post-upload and source_deleted to maintain 127 unique sources. Smoke PASS. **REBUILD: K1 Marketing** (id changed `723ee917...` → `fa31897d-d25e-4925-a266-928bf2bd109b`, 91 → 200 sources, +109) — added STRATEGIC_SURFACING (134 KB), v14 render manifest 9-shot landing page v1, SAFETY_STACK_MAP (39 KB), full Blender skill suite (34 files incl. checklists, examples, references, templates), K-Cycles programme reports (6), floor saga (8), hash breach trilogy, lookdev candidates 001-003 full brief+review+variant+final cycles, photorealism pass 001/001a, real-photo environment v1, 17 V15/V16/Wake-Up/destruction-cascade/multi-segment/ViewportGuard CHANGELOG entries. Preserved scene-authority rules, centre-origin, K-Cycles+Metal lock, 2026-05-20 hero deadline. Added 5 new warning classes: HERO RENDER DEADLINE WARNING, V15-V16 LINEAGE WARNING, SCENE-AUTHORITY WARNING, MEDIA-ONLY WARNING, K-CYCLES/METAL WARNING. Smoke PASS — correctly identified V15 baseline + canonical scene + fired all 5 mandatory warnings + surfaced LAUNCH_TRUTH vs CHANGELOG contradiction for re-anchoring. Phase-B execution: 3 old notebooks deleted, 5 new created, 678 total sources uploaded across 5 notebooks (3 SSAs hit usage limits mid-batch but were resumed after auth refresh; gaps identified via `notebook_get` diff and completed). 5/5 chat_configure success, 5/5 smoke tests PASS. |
