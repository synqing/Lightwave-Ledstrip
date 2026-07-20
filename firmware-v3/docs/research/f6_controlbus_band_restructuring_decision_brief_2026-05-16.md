# F-6 ControlBus Band Restructuring Decision Brief - 2026-05-16

RBDO: DEGRADED-MODE

- Unresolved assumption: Captain has not selected the product/engineering restructuring approach for reducing `CONTROLBUS_NUM_ZONES` from 4 to 3.
- Risk if wrong: choosing a partition prematurely can suppress high-frequency/chroma content, change legacy/non-ES AGC behaviour, or encode a fourth-zone workaround that violates the 3-zone hard rule.
- Fallback: this brief is decision-enabling analysis only; no firmware refactor, protocol change, or hardware claim is authorised by this document.
- Revisit trigger: Captain selects option A, B, or C; audio Zone AGC is needed for a public/product surface; or a hardware-in-the-loop F-6 session is opened.
- Debt count / affected outputs: one output, F-6 Task 2 decision readiness.

## Source Facts Before Decision

| Fact | Source anchor | Decision impact |
|---|---|---|
| Current source still defines `CONTROLBUS_NUM_ZONES = 4`. | `firmware-v3/src/audio/contracts/ControlBus.h:20-22` | A fix must change source, not merely documentation. |
| Current 8-band AGC partition is four 2-band buckets: `0-1`, `2-3`, `4-5`, `6-7`. | `firmware-v3/docs/research/f6_controlbus_num_zones_audit_2026-05-06.md` | A mechanical `4 -> 3` would leave bands `6-7` uncovered if range logic remains `z * 2`. |
| Current 12-chroma AGC partition is four 3-bin buckets: `0-2`, `3-5`, `6-8`, `9-11`. | `firmware-v3/docs/research/f6_controlbus_num_zones_audit_2026-05-06.md` | A 3-zone chroma path needs an explicit 4/4/4 grouping, disabling, or a new strategy. |
| Production K1v2 ESV11 calls `applyDerivedFeatures()` rather than the Zone AGC stage. | `firmware-v3/docs/research/f6_controlbus_num_zones_audit_2026-05-06.md`; `firmware-v3/src/audio/AudioActor.cpp:1041` | The defect is source/doc debt for production K1v2 today, but remains live in legacy/non-ES paths. |
| Legacy/non-ES paths still call `UpdateFromHop()` and therefore still exercise Zone AGC. | `firmware-v3/docs/research/f6_controlbus_num_zones_audit_2026-05-06.md` | Future validation must target a path that actually runs the changed AGC logic. |
| REST/WS Zone AGC is feature-disabled in ESV11 backend builds. | `firmware-v3/docs/research/f6_controlbus_num_zones_audit_2026-05-06.md` | API behaviour changes are not expected for production ESV11 unless F-6 deliberately changes that boundary. |
| WS and REST contracts both mark Zone AGC as PipelineCore/legacy-only and ESV11-disabled after the 2026-05-16 protocol follow-up. | `docs/protocol/k1-ws-contract.yaml`; `docs/protocol/k1-rest-contract.yaml`; commit `e5b29a61` | Future F-6 implementation must still revisit contract wording after the source boundary is selected; this brief does not close implementation or hardware validation. |
| The hard rule is three user-facing zones only. | `BACKLOG.md` F-6 | Do not document the fourth internal AGC zone as canonical product truth. |

## Decision Criteria

| Criterion | Why it matters |
|---|---|
| Full coverage | Every band `0-7` and chroma bin `0-11` must be covered exactly once, unless a path is explicitly retired. |
| Musical balance | Bass, mid, and high content should remain normalised enough that one region does not dominate the others. |
| Product semantics | The internal model should not reintroduce a fourth user-facing zone by another name. |
| Testability | The chosen partition must be unit-testable without hardware, then hardware-testable on the path that exercises Zone AGC. |
| Migration risk | REST/WS docs and any non-ES consumers must be updated only after the source behaviour is chosen. |

## Options

| Option | Meaning | Engineering notes | Main risk | Validation required before commit |
|---|---|---|---|---|
| A - Drop the high zone | Keep low/mid coverage and omit current top band/chroma group. | Lowest source churn if implemented deliberately. | Highest audio/visual risk: hats, cymbals, brilliance, and upper chroma classes can lose normalisation. | Unit test proves the omission is intentional; hardware/reference audio confirms no unacceptable high-frequency collapse. |
| B - Merge adjacent zones | Keep all inputs but combine two neighbouring regions into one follower group. | Moderate churn; likely candidates are low merge (`0-2 / 3-5 / 6-7`) or high merge (`0-1 / 2-4 / 5-7`) for bands. | The merge choice changes spectral balance and can bias bass/mid/high response. | Unit tests for exact coverage; hardware A/B against reference material with percussion and mid-heavy content. |
| C - Explicit three musical buckets | Replace derived `z * 2` / `z * 3` ranges with named tables for three buckets. | Best long-term model because the implementation can encode product semantics directly. | Highest implementation and validation cost; requires clear band/chroma tables and hardware evidence. | Unit tests for every bucket table; production build; flash relevant path; reference audio validation before commit. |

## Engineering Recommendation

Do not implement F-6 until Captain selects a restructuring direction. If the goal is to preserve musical coverage while aligning source with the 3-zone rule, option C is the strongest engineering target because it replaces accidental four-way arithmetic with explicit three-bucket ownership. That recommendation does not authorise code changes by itself.

The future implementation should first add explicit band/chroma boundary tables, then test coverage before changing runtime behaviour. A direct constant-only edit is unsafe.

## Required Future Gates

1. Captain selects A, B, or C.
2. Source implementation replaces derived `z * 2` and `z * 3` logic with explicit coverage.
3. Unit tests prove every retained band/chroma input is covered exactly once.
4. REST/WS/docs are updated only after the implementation boundary is known, preserving the ESV11-disabled Zone AGC boundary unless Captain explicitly changes it.
5. Hardware validation runs on a build/path that exercises Zone AGC.
6. Commit only after hardware evidence records AGC behaviour against the approved reference audio corpus.

## Forbidden Claims

- Do not claim F-6 is fixed from this brief.
- Do not claim production K1v2 ESV11 visual behaviour changes.
- Do not claim any 3-zone partition is selected.
- Do not claim hardware validation has happened.
- Do not claim REST/WS contract cleanup is complete.
- Do not expose a fourth product/user zone in public docs as a workaround.





