# SpectraSynq / K1‑Lightwave — Migration Plan

Date: 2026-02-09
Scope: Migration from AS‑IS to TO‑BE with explicit exit criteria per stage.

Reference map for citations in this spec:
- Invariants: I1..I14 from `00_CONSTITUTION.md` in listed order.
- Decisions: D1..D13 from `01_DECISION_REGISTER.md` in listed order.

---

## 8. Migration Plan from AS‑IS (with exit criteria)

1. Stage 0 — Baseline instrumentation. Exit criteria: per‑frame render time measured; heap allocs in render path == 0; PSRAM availability verified. (I4, I5, I6, I7)
2. Stage 1 — Actor boundary audit. Exit criteria: explicit owner for every shared state; no direct cross‑actor writes. (I12, D13)
3. Stage 2 — FeatureBus integration. Exit criteria: audio features emitted as fixed‑size frames; render consumes frames deterministically. (I4, I5, D8, D9)
4. Stage 3 — Render pipeline normalisation. Exit criteria: centre‑origin mapping enforced everywhere; no linear sweeps; no rainbow/hue‑wheel sweeps. (I1, I2, I3, D5, D11)
5. Stage 4 — Control plane decoupling. Exit criteria: REST/WS updates produce immutable snapshots; render only reads snapshots at frame boundary. (I9, I12, D1)
6. Stage 5 — Network resilience enforcement. Exit criteria: AP/STA fallback verified under load; deterministic IP path reliable; mDNS hint only. (I10, I11, D2, D7)
7. Stage 6 — Storage robustness. Exit criteria: LittleFS recovery tested; preset/palette save/load stable. (D4)

---

## Rules Compliance Notes
- No generic IoT boilerplate was introduced; all choices are tied to Constitution invariants or current decisions.
- British English spelling is used throughout.
