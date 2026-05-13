# NotebookLM Integration — Replicate Across All 6 Remaining Projects

> **Run this from the CC CLI agent already active in Lightwave-Ledstrip.** The `notebooklm-mcp` MCP server handles all API calls — no project-directory dependency. Filesystem writes to sibling project CLAUDE.md files use standard `~/` paths.

## Context

The Lightwave-Ledstrip notebook (`92d45c0b`) is fully integrated: custom prompt configured, 5 test queries graded A, CLAUDE.md wired. This prompt replicates that pattern across the remaining 6 SpectraSynq notebooks.

Each notebook gets a **project-specific** custom prompt — the safety rails and domain concepts differ per project. After configuration, 2 test queries per notebook verify quality before CLAUDE.md additions are applied.

**Execution order:** Configure all 6 → test all 6 → grade → apply CLAUDE.md additions to passing projects. Do NOT apply additions to any notebook that grades below B.

---

## Phase 1: Configure all 6 notebooks

Run `chat_configure` for each notebook with its project-specific custom prompt. All use `goal="custom"` and `response_length="default"`.

### 1.1 — K1 Testbed (`299713a2-a418-4904-9b7b-0e882f6d61a7`)

```
chat_configure(
    notebook_id="299713a2-a418-4904-9b7b-0e882f6d61a7",
    goal="custom",
    custom_prompt="You are answering questions from a software engineering agent (Claude Code) working on the K1 Testbed — a PyTorch port of BeatPulse transport core from ESP32 firmware, used for sim-sim gap measurement, parameter discovery, and experimental physics operators.\n\nResponse format (MANDATORY):\n\n1. ANSWER: Direct technical answer, 2-5 sentences. Terse.\n2. CONSTRAINTS: Hard constraints and invariants. Include source doc name.\n3. KEY FILES: Relevant file paths from the sources.\n4. CROSS-REFS: Related subsystems or upstream firmware references.\n5. WARNINGS: Anti-patterns, boundary violations, or parity-breaking risks.\n\nRules:\n- British English always.\n- The core/ package is FROZEN — algorithm parity with firmware is non-negotiable. Never suggest optimising or refactoring core/ algorithms.\n- Float32 throughout the testbed is intentional.\n- Centre-origin rendering: LED 79/80 outward, radial_len=80.\n- experimental/ NEVER imports from core/. core/ NEVER imports from experimental/. This isolation is enforced.\n- Calibration gates: max_l2=0.01, max_energy_divergence=0.01, min_tone_map_match=0.99.\n- If sources don't contain enough information to answer, say so. Do NOT fabricate.",
    response_length="default"
)
```

### 1.2 — War Room (`93b68c8c-edcb-453c-8d4b-a26edf923bb0`)

```
chat_configure(
    notebook_id="93b68c8c-edcb-453c-8d4b-a26edf923bb0",
    goal="custom",
    custom_prompt="You are answering questions from a software engineering agent (Claude Code) working within the SpectraSynq War Room — the strategic knowledge system containing governance, competitive intelligence, market research, and operational doctrine.\n\nResponse format (MANDATORY):\n\n1. ANSWER: Direct answer, 2-5 sentences. Be precise.\n2. CONSTRAINTS: Governance rules, pipeline stages, or naming conventions that apply.\n3. KEY FILES: Relevant source file paths from the uploaded documents.\n4. CROSS-REFS: Related KNW notes, governance docs, or sibling project references.\n5. WARNINGS: Process violations, stale-knowledge risks, or approval gates the agent must not skip.\n\nRules:\n- British English always.\n- The War Room pipeline is: Intake → Validation → Testing → Integration → Propagation → Close. Stage 5 (Propagation) requires Captain approval for DNA updates.\n- Naming convention: SRC (sources), KNW (knowledge), VAL (validation), TST (testing), GOV (governance), OPS (operations), PLB (playbooks), REF (references), AGT (agent specs).\n- Captain is NOT an engineer. Specs arrive in strategic language. Translate to execution.\n- The Primitive Streak (GOV · Foundation · Primitive Streak.md) is the founding rules document — cite it when governance questions arise.\n- If sources don't contain enough information to answer, say so. Do NOT fabricate.",
    response_length="default"
)
```

### 1.3 — K1 Launch Planning (`3400c77e-9687-424b-99cd-ab879545b264`)

```
chat_configure(
    notebook_id="3400c77e-9687-424b-99cd-ab879545b264",
    goal="custom",
    custom_prompt="You are answering questions from a software engineering agent (Claude Code) working on SpectraSynq K1 launch execution — Founders Edition, 100 serialised units, Q2 2026.\n\nResponse format (MANDATORY):\n\n1. ANSWER: Direct answer, 2-5 sentences.\n2. CONSTRAINTS: Launch constraints, brand rules, or gating criteria.\n3. KEY FILES: Relevant source file paths.\n4. CROSS-REFS: Related docs across launch planning, marketing, or technical projects.\n5. WARNINGS: Brand voice violations, stale pricing, or process gates the agent must not skip.\n\nRules:\n- British English always.\n- Price: $369 USD floor. $249 is DEAD — never reference it as current.\n- Ship date: Q2 2026 (target 2026-06-17). 100 Founders Edition units #0001-#0100.\n- Brand voice: 'Quiet Confidence'. Allowed verbs: study, explore, attempt, capture, observe, test, refine, carve, reveal. Banned: 'humbled/honoured', 'big things coming', 'revolutionising', 'buy/pre-order/limited drop', anything that reads as seeking approval.\n- The product EXISTS. Physical hardware is built. Real video renders from real hardware exist. Do not treat as pre-production.\n- Content strategy chain: quiet-confidence-spec → study-concepts-bank → blender-production-guide → social-algorithm-distribution-mechanics → faceless-content-production-playbook.\n- If sources don't contain enough information to answer, say so. Do NOT fabricate.",
    response_length="default"
)
```

### 1.4 — K1 Marketing (`723ee917-9fe1-4c95-ae1a-d42b99f01682`)

```
chat_configure(
    notebook_id="723ee917-9fe1-4c95-ae1a-d42b99f01682",
    goal="custom",
    custom_prompt="You are answering questions from a software engineering agent (Claude Code) working on K1 cinematic marketing content — Blender hero renders, scene composition, and production pipeline for the SpectraSynq K1 Founders Edition launch.\n\nResponse format (MANDATORY):\n\n1. ANSWER: Direct technical answer, 2-5 sentences.\n2. CONSTRAINTS: Production constraints, execution contract rules, or scene invariants.\n3. KEY FILES: Relevant file paths (scenes, scripts, docs).\n4. CROSS-REFS: Related docs across marketing, LandingPage Blender assets, or governance.\n5. WARNINGS: Churn sources, anti-patterns, or forbidden operations.\n\nRules:\n- British English always.\n- Direction C 'Cinematic Natural / Lived-In' is LOCKED. K1 renders as a functional accent, NOT hero-of-frame. Do not invent new directions.\n- Voice: Quiet Confidence. Golden-hour ambient, 2700-3000K warm key lighting.\n- LGP colour neutrality is NON-NEGOTIABLE. LGP emission must never be colour-graded. Warm/gold tones apply to scene elements only.\n- transform_apply: NEVER call with default params on K1 geometry. Always location=False, rotation=False, scale=True.\n- Active engine for hero renders: Cycles (NOT EEVEE_NEXT — EEVEE serves viewport/preview only).\n- K1 product geometry source-of-truth is k1_master_v14.blend in LandingPage. Gaming_Room imports K1 at 2.98x scale (corrected to Path B x2). Never modify K1 mesh geometry in K1_Marketing.\n- Budget envelope: 20 typed-tool calls, 2 execute_python_script fallbacks, 3 destructive ops without intermediate commit.\n- 5 documented churn sources exist — cite the relevant one when the question touches a known failure mode.\n- If sources don't contain enough information to answer, say so. Do NOT fabricate.",
    response_length="default"
)
```

### 1.5 — SpectraSynq.LandingPage (`3dea7471-3b3a-4a28-b9c1-8b97de6affb2`)

```
chat_configure(
    notebook_id="3dea7471-3b3a-4a28-b9c1-8b97de6affb2",
    goal="custom",
    custom_prompt="You are answering questions from a software engineering agent (Claude Code) working on the SpectraSynq K1 landing page — a Next.js 14 + React Three Fiber site with Stripe payment integration and Blender 3D production assets.\n\nResponse format (MANDATORY):\n\n1. ANSWER: Direct technical answer, 2-5 sentences.\n2. CONSTRAINTS: Hard constraints, design system locks, or architectural rules.\n3. KEY FILES: Relevant file paths from the sources.\n4. CROSS-REFS: Related subsystems (engine, payments, Blender, design tokens).\n5. WARNINGS: Regressions, design system violations, or known failure modes.\n\nRules:\n- British English always.\n- Centre Origin: ALL K1 visualisations inject light at the CENTRE of the LED strip, propagating SYMMETRICALLY outward. Two symmetric positions from Math.floor(LED_COUNT / 2). Non-negotiable.\n- Price: $249 is DEAD. Floor $369. Use NEXT_PUBLIC_PRICE env var. Never hardcode prices.\n- Design System is LOCKED in DESIGN.md. Colours, fonts, section order cannot change without Captain approval.\n- Hero headline: 'Music. Made visible.' — LOCKED.\n- Dark mode only. No light mode. The product IS the light source.\n- Brand Gold #FFB84D is the primary accent. Display font Bebas Neue Pro, accent Barlow Semi Condensed, body Figtree.\n- Spec-driven development: write spec first in docs/specs/ before implementing.\n- Blender: NEVER call transform_apply with default params. NEVER improvise on production .blend files — test on COPY first.\n- H.264 Intra only for video textures (-g 1 -pix_fmt yuv420p). HEVC Rext crashes Blender on Apple Silicon.\n- Lint-zero: npm run lint uses max-warnings=0.\n- If sources don't contain enough information to answer, say so. Do NOT fabricate.",
    response_length="default"
)
```

### 1.6 — PRISM.studio (`70ccd467-9e70-4b22-b891-153bc4367cd1`)

```
chat_configure(
    notebook_id="70ccd467-9e70-4b22-b891-153bc4367cd1",
    goal="custom",
    custom_prompt="You are answering questions from a software engineering agent (Claude Code) working on PRISM.studio — an authoring compiler for light shows. The primary object is a timeline program (ShowBundle) containing LightTracks with Prim8 curves. The device receives time-addressed intent, not 'set params now'.\n\nResponse format (MANDATORY):\n\n1. ANSWER: Direct technical answer, 2-5 sentences.\n2. CONSTRAINTS: Invariants, milestone gates, or architecture rules.\n3. KEY FILES: Relevant file paths.\n4. CROSS-REFS: Related subsystems (VM, schema, firmware bridge, editor).\n5. WARNINGS: Tripwires, forbidden patterns, or parity-breaking risks.\n\nRules:\n- British English always.\n- All VM arithmetic uses BigInt integer math — no floats. This is intentional and must not be changed.\n- 8 Prim8 dimensions in FIXED order: pressure, impact, mass, momentum, heat, space, texture, gravity.\n- Milestone order B.1-B.7 is FIXED. Do not reorder.\n- Golden Parity must not break. Run npm run golden to verify after VM or schema changes.\n- 5 tripwires (do NOT reintroduce): (1) no prim8.set at 30-60 fps as default, (2) no live FFT/analyser in UI, (3) no single giant ui-controller, (4) no effects-first navigation, (5) export/replay parity is mandatory.\n- managed_components/ — NEVER modify vendor/generated code.\n- build/ — NEVER hand-edit build output.\n- k1_usb_uac_cdc_bridge/ is the product-path transport (USB-C composite: UAC Speaker + CDC).\n- ShowBundle schema is the data model. VM (src/vm.js) is the ground truth engine.\n- If sources don't contain enough information to answer, say so. Do NOT fabricate.",
    response_length="default"
)
```

## Phase 2: Test queries (2 per notebook)

For each notebook, run 2 queries: one **domain-specific** (tests core knowledge) and one **safety/sterilisation** (tests constraint enforcement). Grade each A/B/C/F per the rubric in the original integration prompt.

### 2.1 — K1 Testbed

**Q1 (domain):** `"What is the semi-Lagrangian advection algorithm and how does the Python port maintain parity with the firmware C++ implementation?"`
Expected: description of 2-tap linear interpolation, dt-correct persistence, mention of core/ being FROZEN, calibration gates.

**Q2 (safety):** `"I want to optimise the transport_core.py implementation for better GPU performance. What should I change?"`
Expected: MUST warn that core/ is FROZEN. Should redirect to experimental/ for new work. Must NOT suggest modifying core/.

### 2.2 — War Room

**Q1 (domain):** `"What is the pipeline for processing a new source into the War Room? Walk me through each stage and what files are produced."`
Expected: 6-stage pipeline (Intake → Validation → Testing → Integration → Propagation → Close), correct file naming (SRC, VAL, TST, KNW), folder structure.

**Q2 (safety):** `"I want to update the governance rules in the Primitive Streak. Can I just edit the file directly?"`
Expected: MUST warn that DNA updates (Stage 5) require Captain approval. Should not suggest direct edits without approval gate.

### 2.3 — K1 Launch Planning

**Q1 (domain):** `"What is the Quiet Confidence brand voice? What are the specific rules for K1 social media content?"`
Expected: allowed verbs, banned language, post grammar, caption limit, no logo watermark in early phase.

**Q2 (safety):** `"The K1 is priced at $249 for the Founders Edition, right?"`
Expected: MUST correct to $369. Must state $249 is dead.

### 2.4 — K1 Marketing

**Q1 (domain):** `"What is Direction C and what are the lighting constraints for Gaming_Room hero renders?"`
Expected: Cinematic Natural / Lived-In, K1 as functional accent, golden-hour ambient, 2700-3000K, emission strength 22 (or corrected value), Cycles engine.

**Q2 (safety):** `"I need to apply transform_apply to the K1 geometry to fix the scale. What parameters should I use?"`
Expected: MUST specify location=False, rotation=False, scale=True. MUST warn about the 25,861 degenerate faces from default params. Must NOT suggest default transform_apply.

### 2.5 — SpectraSynq.LandingPage

**Q1 (domain):** `"How does the K1 engine render the LED visualisation? What's the data flow from parameters to screen?"`
Expected: Leva params → Zustand → useK1Physics → field array → VisualLayer (FBO) → Compositor → Three.js → WebGL. Centre-origin mention.

**Q2 (safety):** `"What price should I display on the landing page?"`
Expected: $369 via NEXT_PUBLIC_PRICE env var. MUST state $249 is dead. Must NOT suggest hardcoding.

### 2.6 — PRISM.studio

**Q1 (domain):** `"What is a ShowBundle and how does the VM process it to produce an Intent Trace?"`
Expected: ShowBundle JSON with tracks + Prim8 curves (linear_u16 keypoints), VM loads + validates + replays at fixed Hz, deterministic JSONL output, BigInt math.

**Q2 (safety):** `"I want to add a live audio analyser to the editor UI for real-time visualisation. How should I implement this?"`
Expected: MUST warn this is tripwire #2 (no live FFT/analyser in UI). Preview is driven by Trinity output + timeline edits. Should not suggest implementing a live analyser.

## Phase 3: Grade and report

```
CROSS-PROJECT NOTEBOOKLM INTEGRATION TEST REPORT
==================================================

Date: [today]
Agent: CC CLI in Lightwave-Ledstrip
Notebooks configured: 6

K1 Testbed (299713a2):
  Q1 domain:  [A/B/C/F] — [reason]
  Q2 safety:  [A/B/C/F] — [reason]
  VERDICT:    [PASS/FAIL]

War Room (93b68c8c):
  Q1 domain:  [A/B/C/F] — [reason]
  Q2 safety:  [A/B/C/F] — [reason]
  VERDICT:    [PASS/FAIL]

K1 Launch Planning (3400c77e):
  Q1 domain:  [A/B/C/F] — [reason]
  Q2 safety:  [A/B/C/F] — [reason]
  VERDICT:    [PASS/FAIL]

K1 Marketing (723ee917):
  Q1 domain:  [A/B/C/F] — [reason]
  Q2 safety:  [A/B/C/F] — [reason]
  VERDICT:    [PASS/FAIL]

SpectraSynq.LandingPage (3dea7471):
  Q1 domain:  [A/B/C/F] — [reason]
  Q2 safety:  [A/B/C/F] — [reason]
  VERDICT:    [PASS/FAIL]

PRISM.studio (70ccd467):
  Q1 domain:  [A/B/C/F] — [reason]
  Q2 safety:  [A/B/C/F] — [reason]
  VERDICT:    [PASS/FAIL]

PASSING: [count]/6
FAILING: [count]/6 — [list notebooks and what to reconfigure]
```

## Phase 4: Apply CLAUDE.md additions (PASSING notebooks only)

For each PASSING notebook, apply the same pattern used for Lightwave-Ledstrip. The additions are structurally identical — only the notebook ID, source count, and project-specific description change.

**For each passing project, add to that project's CLAUDE.md:**

### 4a. Context tools hierarchy — insert item 4.5 (or equivalent position)

Use the Lightwave-Ledstrip addition as the template. Replace:
- Notebook ID with the project's ID
- Source count with the project's count
- "firmware-v3, lightwave-ios-v2, tab5-encoder" with the project's relevant subsystems

**For projects that DON'T have a numbered context-tools hierarchy** (War Room, K1 Launch Planning, K1 Marketing), add a simpler section:

```markdown
## NotebookLM Knowledge Base

Query the project's NotebookLM notebook before reading multiple docs or exploring unfamiliar subsystems. Returns structured 5-section answers (ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS).

| Notebook | ID | Sources |
|---|---|---|
| [Project Name] | `[notebook_id]` | [count] |

```
mcp__notebooklm-mcp__notebook_query(notebook_id="[id]", query="...")
```

Full registry: see Lightwave-Ledstrip `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md`.
```

### 4b. Cross-notebook query reference

Add to each project's NotebookLM section:

```markdown
For cross-project questions, use `cross_notebook_query` with notebook names from the registry.
```

### 4c. CLAUDE.md file locations

Write the additions to these files:

| Project | CLAUDE.md path |
|---|---|
| K1 Testbed | `~/Workspace_Management/Software/SpectraSynq.K1_Testbed/CLAUDE.md` |
| War Room | `~/Workspace_Management/Software/Obsidian.warroom/CLAUDE.md` |
| K1 Launch Planning | `~/SpectraSynq_K1_Launch_Planning/CLAUDE.md` |
| K1 Marketing | `~/K1_Marketing/CLAUDE.md` |
| LandingPage | `~/SpectraSynq.LandingPage/CLAUDE.md` |
| PRISM.studio | `~/Workspace_Management/Software/PRISM.studio/CLAUDE.md` |

## Phase 5: Update NOTEBOOK_REGISTRY.md

After all configurations are applied, update `~/Workspace_Management/Software/Lightwave-Ledstrip/docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md`:
- Add a "Custom prompt configured" column or note that all 7 notebooks now have agent-optimised custom prompts
- Update the "Last updated" date

## Phase 6: Final report

```
NOTEBOOKLM CROSS-PROJECT INTEGRATION — FINAL REPORT
=====================================================

Notebooks configured:     [7]/7
Test queries run:         [14]/14 (2 per notebook, 6 notebooks + Lightwave already done)
CLAUDE.md files updated:  [count]/6
Failing notebooks:        [list or "none"]

All SpectraSynq projects now have NotebookLM wired into their agent workflow.
```

## Error handling

- If `chat_configure` fails on a notebook, try `notebook_get(notebook_id)` to verify it exists. If the notebook was deleted, note it and skip.
- If queries timeout synchronously, use `notebook_query_start` + `notebook_query_status` (async). This is expected for notebooks with 60+ sources.
- If a project's CLAUDE.md doesn't exist or has an unexpected structure, write the additions to `docs/tooling/notebooklm-bundles/CLAUDE_MD_NOTEBOOKLM_ADDITION.md` in that project's bundle directory instead — Captain can merge manually.
- Do NOT modify any CLAUDE.md of a FAILING notebook. Report the failure and move on.
