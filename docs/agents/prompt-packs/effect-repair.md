# Prompt Pack: Effect Repair

Use this when repairing, tuning, or reviewing LightwaveOS effects.

## First Contact Prompt

```text
GROUNDED:

You are repairing or reviewing K1 LightwaveOS effects. Start by reading:
- CLAUDE.md top RBDO gate.
- AGENTS.md hard constraints.
- firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md.
- firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md.
- firmware-v3/docs/EFFECT_AUTHORING_STANDARD_V2.md.
- firmware-v3/docs/GOOD_LIGHT_SHOW_TAXONOMY.md.
- firmware-v3/docs/EFFECTS_BEHAVIORAL_REFERENCE.md.
- firmware-v3/docs/audio-visual/AUDIO_FEATURE_SURFACE_V2_CONTRACT.md for audio-reactive effects.
- firmware-v3/docs/audio-visual/audio-visual-contract-surface.md for current audio accessors.

Hard constraints to read back:
- centre-origin LED 79/80 outward;
- no rainbow/full hue-wheel sweeps;
- no heap allocation in render or render-called paths;
- 2.0 ms effect-code ceiling;
- dt-correct smoothing;
- sub-8 ms audio-to-visual latency;
- British English.

Task:
1. Verify current branch, HEAD, and dirty tree.
2. Identify the target effect ID(s), class(es), metadata, and render topology.
3. Fill the Effect Authoring Standard v2 intake sheet.
4. Classify the effect with the Good Light Show Taxonomy.
5. Verify that production effects use named semantic/musical accessors before raw/debug substrates.
6. Locate the smallest repair surface.
7. Do not tune global VP layers to hide an effect bug.
8. If changing firmware behaviour, run the scoped native/build checks and identify the hardware gate before commit.

Forbidden:
- linear sweeps;
- free-running hue wheel/rainbow;
- raw audio directly to pixels;
- new production `bins256` scanning;
- new render-path heap;
- changing WiFi mode;
- claiming visual quality from trace/build evidence alone.

Return using docs/agents/prompt-packs/_shared/return-contract.md.
```
