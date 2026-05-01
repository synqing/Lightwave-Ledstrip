# Entry Prompts — Synergy Topology Extraction Protocol

Two entry prompts: one for Claude Code Max (CC CLI), one for GPT Codex.
Copy-paste the relevant one into the terminal/chat to begin.

---

## CC CLI Entry Prompt (Claude Code Max)

```
Read firmware-v3/docs/research/synergy-topology/K1_VP_SYNERGY_TOPOLOGY_PROMPT.md in full.

This is a 4-pass strategic analysis protocol for mapping the synergy topology of the K1 visual pipeline. It is NOT a code task — no source files will be modified. All outputs are analytical documents.

Before doing anything else:
1. Execute the Verification step in the Operational Wrapper — confirm all 10 source documents are readable and list each filename with its line count.
2. Create the output directory: firmware-v3/docs/research/synergy-topology/
3. Output your READBACK block per CLAUDE.md.

Then execute the protocol:
- Pass 1: parallelise via /dispatching-parallel-agents using the 4 subagent scopes defined in the Execution Model. Merge returns and write PASS_1_DOMAIN_REGISTRY.md.
- Passes 2, 3, 4: execute sequentially in main context. Write each pass output to disk before starting the next.
- After Pass 4: generate EXECUTIVE_SUMMARY.md.

This same protocol is being run independently on GPT Codex for cross-model validation. Produce your best independent analysis.

Notify me when all 5 output files are written.
```

---

## Codex Entry Prompt (GPT Codex)

```
Read firmware-v3/docs/research/synergy-topology/K1_VP_SYNERGY_TOPOLOGY_PROMPT_codex.md in full.

This is a 4-pass strategic analysis protocol for mapping the synergy topology of the K1 visual pipeline. It is NOT a code task — no source files will be modified. All outputs are analytical documents.

Before doing anything else:
1. Execute the Verification step in the Operational Wrapper — confirm all 10 source documents are readable and list each filename with its line count.
2. Create the output directory: firmware-v3/docs/research/synergy-topology/codex/

Then execute the protocol:
- Read all 10 source documents (2 primary + 8 secondary SSA outputs) before beginning Pass 1.
- Execute all 4 passes sequentially. Write each pass output to disk before starting the next.
- After Pass 4: generate EXECUTIVE_SUMMARY.md.

This same protocol is being run independently on Claude Code Max for cross-model validation. Your independent analytical perspective is the point — do not try to anticipate what Claude would produce. Produce your own analysis from the source material.

Notify me when all 5 output files are written.
```

---

## Pre-Flight Checklist (for Captain)

Before launching either agent:

1. [ ] Place `K1_VP_SYNERGY_TOPOLOGY_PROMPT.md` at `firmware-v3/docs/research/synergy-topology/K1_VP_SYNERGY_TOPOLOGY_PROMPT.md`
2. [ ] Place `K1_VP_SYNERGY_TOPOLOGY_PROMPT_codex.md` at `firmware-v3/docs/research/synergy-topology/K1_VP_SYNERGY_TOPOLOGY_PROMPT_codex.md`
3. [ ] Confirm all 10 source documents exist at their expected paths:
   - `firmware-v3/docs/research/SB_ES_MOTION_MECHANICS_TAXONOMY_2026-04-26.md`
   - `firmware-v3/docs/research/SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26.md`
   - `.claude/recovered_ssa_outputs/SSA-Physics-a35c14b375a166bcc.md`
   - `.claude/recovered_ssa_outputs/SSA-AudioDriver-a797472436462f7f9.md`
   - `.claude/recovered_ssa_outputs/SSA-Geometry-a92a95c6330daeedb.md`
   - `.claude/recovered_ssa_outputs/SSA-Persistence-a5795be9e307b5798.md`
   - `.claude/recovered_ssa_outputs/SSA-Composition-a2dd422ad30c85b59.md`
   - `.claude/recovered_ssa_outputs/SSA-CrossLineage-ac1431e4e287e4dfc.md`
   - `.claude/recovered_ssa_outputs/SSA-K1Specific-a12ec6509e9f8e474.md`
   - `.claude/recovered_ssa_outputs/SSA-ProductStrategy-ac38d0188df830e8e.md`
4. [ ] Codex Custom Instructions are configured (paste contents of `firmware-v3/docs/research/synergy-topology/CODEX_CUSTOM_INSTRUCTIONS.md` into Codex project settings)
5. [ ] Both agents can be launched — Codex first (sequential, will run longer) or simultaneously

## Post-Run: Convergence Analysis

After both runs complete, the outputs sit in parallel directories:

```
firmware-v3/docs/research/synergy-topology/
├── PASS_1_DOMAIN_REGISTRY.md          (CCM)
├── PASS_2_SYNERGY_TOPOLOGY.md         (CCM)
├── PASS_3_KILL_ORDER.md               (CCM)
├── PASS_4_ADVERSARIAL_STRESS_TEST.md  (CCM)
├── EXECUTIVE_SUMMARY.md               (CCM)
└── codex/
    ├── PASS_1_DOMAIN_REGISTRY.md      (Codex)
    ├── PASS_2_SYNERGY_TOPOLOGY.md     (Codex)
    ├── PASS_3_KILL_ORDER.md           (Codex)
    ├── PASS_4_ADVERSARIAL_STRESS_TEST.md (Codex)
    └── EXECUTIVE_SUMMARY.md           (Codex)
```

Compare pass-by-pass. The highest-value analysis is:

- **Pass 1 divergence**: Different domain boundaries or domains one model identified that the other missed entirely.
- **Pass 2 divergence**: Different hub/bridge/cluster assignments — this reveals where the synergy classification is model-dependent.
- **Pass 3 divergence**: Different kill order sequencing — this reveals which strategic assumptions are contestable.
- **Pass 4 convergence**: Where both adversarial tests reach the same findings — high-confidence structural critique that survives independent attack from two reasoning lineages.
