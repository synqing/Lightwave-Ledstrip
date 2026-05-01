# K1 Effect Repair Workbench — Phase 0.5 Prediction Fidelity

**Label: DEGRADED-MODE**

- **Unresolved assumption:** No firmware LED-byte capture was available in this turn, so predicted-vs-actual MAE could not be measured.
- **Risk if wrong:** A prediction engine may later be viable, but this run cannot prove it. This is an unanswered probe, not a failed prediction test.
- **Fallback:** Build Track B capture-only v0: directive authoring, rules audit, byte-strip capture replay, no prediction claims.
- **Revisit trigger:** A paired predicted `.bin` and actual firmware capture `.bin` for the same deterministic scenario exists.
- **Debt count / affected outputs:** 1 — Track B omits prediction/scenario panels.

## Result

Phase 0.5 cannot honestly produce MAE without an actual captured LED byte stream. Prediction was **not tested**; it is unanswered, not failed. The workbench therefore selects **Track B** for v0 because Track B does not depend on prediction fidelity.

## Evidence

- Capture tooling exists: `firmware-v3/testbed/evaluation/capture_cli.py`.
- Existing parser supports serial v2/v4 capture frames: `firmware-v3/testbed/evaluation/frame_parser.py`.
- Web LED stream docs mention 961-byte and 966-byte LED frames.
- No `.bin` LED capture was found in the repository during this run.

## Gate Decision

**Track B — capture-only v0.**

Prediction engine, deterministic scenarios, and browser mic/Web Audio remain deferred until measured frame fidelity exists.

Track A remains revisitable once a paired predicted `.bin` and actual firmware capture `.bin` exist for the same deterministic scenario. If a later MAE check returns `<= 20%`, Track A can be planned as an upgrade rather than treated as rejected by this report.
