---
abstract: "The costly scar behind ssa-management (2026-06-04, K1 tempo). The fan-out bought coverage, but EVIDENCE CONSUMPTION failed: multiple fluent subagent claims (V-OCTAVE 'ACF flat', T2 'v3 Goertzel-only', T4 '53% ceiling') were treated as findings before their methods and decisive artifacts were re-run, and one nearly drove a revert of a committed, hardware-validated +15.6pp gain. The fix was not better agents — it was the orchestrator's own re-run + adversarial cross-validation + contradiction resolution. Loaded on demand from the ssa-management SKILL."
---

# Case study — the V-OCTAVE near-disaster (2026-06-04, K1 tempo)

**Setup.** Recovering a tempo primitive (`sb_tempo`, ACF-salience). Three subagent swarms were
deployed — recon, de-risk, adversarial validation — each well-contracted and parallelised. Coverage
was excellent. *That was not the problem.*

**The failure — consumption, not fan-out.** One validator (V-OCTAVE) returned, fluently and with
numbers, that the recovered ACF signal was *"flat (std 0.029), barely functioning."* The orchestrator
**relayed it as fact** and recommended **reverting the committed primitive** — which was a real,
**+15.6pp Acc2** gain, later hardware-validated (128 + 144 BPM lock on device). Two *other* relayed
claims were also wrong: **T2** (*"v3 is Goertzel-only"* — false; v3 ships an octave arbiter +
a BeatTracker ACF) and **T4** (*"53% is the ceiling"* — it is 56.2%; 53% is a sub-ceiling).

**The catch.** The Captain forced an adversarial **back-test** swarm; a probe's 4-way ablation —
**which the orchestrator then re-ran himself** — showed std ≈ 0.28 and the ACF as the dominant signal
(`acf_only` 40.6% vs `prior_only` 15.6%). The revert was withdrawn in the same turn the claim was
refuted.

**The honest diagnosis (not the heroic one).** It is tempting to conclude *"the swarms were flawless;
the only failure was believing them."* That is too clean. **Three fluent claims were wrong**, which
points at real gaps across the chain — brief framing, validator method, the absence of a forced
return schema, weak contradiction handling, and above all **re-run timing**: decision-critical claims
were consumed *before* their methods and decisive artifacts were re-run. **The fan-out bought
coverage; the consumption laundered prose into fact.**

**What fixed it (the doctrine, in one scar):**
- the orchestrator's **own re-run** of the decisive artifact (SKILL §1, §6 step 5);
- **adversarial cross-validation** — probes tasked to *refute*, not confirm (SKILL §4 default verdict);
- **contradiction resolution instead of averaging** (SKILL §6 step 6, §8);
- and **auditing the validator's method**, not just its conclusion — V-OCTAVE's "std 0.029" was a
  harness artefact, a bent scale (see `load-bearing-edges`).

Not "better agents." Better **evidence-consumption discipline**. Full corrected record:
`docs/research/validation/2026-06-04-RECONCILIATION.md`.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-06-04 | agent:CTO | Extracted from ssa-management SKILL.md §11 into a reference (progressive disclosure); reframed from "flawless swarms / only failure was belief" to the accurate "fan-out bought coverage, consumption failed." |
