---
abstract: "Boundary contract for using SpectraSynq.K1_Testbed with LightwaveOS VP and effect work. Defines what the testbed can filter, what it cannot prove, and how results enter the firmware workflow."
---

# K1_Testbed Integration Boundary

**Status:** DRAFT - Captain-approved for documentation/tooling package on 2026-05-06. No changes are made to the external K1_Testbed repo by this document.

**Authority anchors:**
- `firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md:152-168` says K1_Testbed is an optional pre-hardware filter, not physical LGP truth.
- `/Users/spectrasynq/Workspace_Management/Software/SpectraSynq.K1_Testbed/CLAUDE.md:1-15` defines the testbed as a PyTorch port of firmware transport/rendering utilities for sim-sim gap, parameter discovery, offline visual regression, and experimental operator work.
- `SpectraSynq.K1_Testbed/CLAUDE.md:40-50` freezes `core/` as firmware-parity reference and isolates `experimental/` as research sandbox.
- `SpectraSynq.K1_Testbed/CLAUDE.md:91-97` defines calibration gates: `max_l2=0.01`, `max_energy_divergence=0.01`, `min_tone_map_match=0.99`.
- `SpectraSynq.K1_Testbed/CALIBRATION_README.md:101-111` records an older calibration report where the current reference set failed those gates; do not assume pass status without a fresh run.
- A current `git status --short` on 2026-05-06 shows the K1_Testbed tree has many modified and untracked files. Treat it as externally owned unless Captain explicitly opens it for writes.

## Working Model

K1_Testbed is a pre-hardware reasoning and filtering tool. It is useful for asking:

- does a transport-core style algorithm preserve shape, energy, and tone-map behaviour?
- do parameter ranges collapse into dead output, runaway brightness, or unstable motion?
- does an experimental operator deserve firmware consideration?
- can a candidate be visualised offline before using K1 hardware time?

It cannot answer:

- does the real acrylic LGP fuse the strips into a good optical object?
- does the FastLED/RMT output path remain safe?
- does a physical strip buffer ownership issue exist?
- does Captain perceive the effect as product-grade?
- should global firmware visual defaults change?

## Allowed Uses

| Use | Allowed? | Boundary |
|---|---:|---|
| Transport-core parity checks | Yes | Only against represented algorithms. |
| Parameter discovery | Yes | Candidate filter, not product sign-off. |
| Offline visual previews | Yes | Must be labelled simulation. |
| Experimental operator exploration | Yes | Stays in K1_Testbed `experimental/`. |
| Firmware-effect distillation hints | Yes | Requires firmware implementation and hardware pass later. |
| LGP visual-default sign-off | No | Requires physical K1 hardware. |
| FastLED/RMT timing validation | No | Firmware hardware only. |
| Colour-correction default changes | No | Must follow VP validation protocol. |
| Editing K1_Testbed from LightwaveOS session | No by default | Requires explicit Captain write approval. |

## Evidence Ladder

When K1_Testbed is used in a firmware decision:

1. Record the testbed commit or dirty-state summary.
2. State which testbed domain was used: `core`, `calibration`, `vis`, `experimental`, `training`, or `workbench`.
3. State whether the run is parity, exploration, or visual preview.
4. Record gate values: L2, energy divergence, tone-map match.
5. If gates fail, classify the failure as simulation-only unless firmware reproduction confirms it.
6. If gates pass, treat that as permission to run firmware capture, not as firmware pass.
7. Run firmware trace/serial validation.
8. For visual defaults or production promotion, get Captain hardware visual judgement.

## Calibration Gates

Use these as hard simulation thresholds when the represented model matches the firmware candidate:

| Gate | Threshold | Meaning |
|---|---:|---|
| L2 | `< 0.01` | Pixel-level shape similarity. |
| Energy divergence | `< 0.01` | Brightness/energy conservation. |
| Tone-map match | `> 0.99` | Tone-map fidelity. |

If the testbed reference set is stale or failing, do not use it as a pass/fail authority. Use it only as a qualitative exploration tool until recalibrated.

## Workflow

### 1. Intake

Before using the testbed, classify the firmware task:

| Firmware Task | Testbed Role |
|---|---|
| BeatPulse transport parameter tuning | Strong candidate filter. |
| VP colour-correction default | Weak or no role. |
| Direct-strip buffer ownership | No proof role; firmware trace/hardware required. |
| New physics operator | Strong exploration role. |
| Phase 5 effect visual polish | Useful for parameter search only if the effect maps to testbed operators. |
| Silence policy | Limited; hardware perception required. |

### 2. External Repo Discipline

- Do not edit `/Users/spectrasynq/Workspace_Management/Software/SpectraSynq.K1_Testbed` from a LightwaveOS task unless Captain explicitly opens that repo for writes.
- If writing is approved, start by reading K1_Testbed `CLAUDE.md`.
- Do not modify `core/` without verifying corresponding firmware source parity.
- Keep `experimental/` claims separate from firmware claims.
- Do not import experimental operators into frozen core.

### 3. Report Format

Every testbed-assisted LightwaveOS report should include:

```text
K1_Testbed role: parity | exploration | visual preview | parameter search
K1_Testbed state: commit or dirty summary
Represented firmware surface: file/function/effect
Unrepresented firmware surfaces: FastLED/RMT/LGP/VP layers/etc.
Gates: L2=..., EDiv=..., ToneMatch=...
Result: pass | fail | exploratory only
Firmware next step: trace | serial | hardware visual | no action
```
## Hard Refusals

Refuse or stop if asked to:

- claim K1_Testbed alone proves production visual quality;
- change firmware defaults from simulation-only evidence;
- edit K1_Testbed `core/` without firmware parity verification;
- use stale failing calibration as a pass;
- skip hardware after a visual-pipeline or colour-default change;
- cross-contaminate K1_Testbed `experimental/` and `core/`;
- hide private media paths or clips inside public repo artefacts.

## Practical Integration Point

For the current Phase 5 / VP work, K1_Testbed should be used narrowly:

- parameter search for BeatPulse-style transport behaviours;
- offline preview of candidate motion grammars;
- visual regression screenshots for candidate comparison;
- rejection filter for obviously dead or unstable parameter ranges.

The final gate remains:

```text
K1_Testbed candidate pass
-> firmware build/trace/serial pass
-> K1 hardware visual pass
-> Captain promotion decision
```
