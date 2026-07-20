---
abstract: "Verification oracle for the IM73D122-vs-SPH0645 K1 mic A/B battery. Scores DEVICE SERIAL telemetry (TEMPO/APCadence frames) against hybrid-beat-tracker .beats ground truth: beat P/R/F, BPM acc-1/2, time-to-lock, %-locked (steady-state), octave-error, plus SNR/dBFS metrology and paired stats (bootstrap CI, Wilcoxon, Cohen's d_z, BH-FDR). Fault-evident per autonomous-agentic-build doctrine (Gate-0 self-test, 23/23 GREEN). Enforces D1 unit-pair framing. Built 2026-07-10; blocked only by D0 (hardware fingerprint) before real captures."
---

# mic_ab_scorer — the mic A/B verification oracle

The **verification harness for the IM73D122-vs-SPH0645 K1 microphone A/B** (see the master
dossier: `firmware-v3/docs/research/im73d122_vs_sph0645_mic_ab_master_dossier_2026-07-10.md`).

Per the autonomous-agentic-build doctrine: **the harness is the product.** It is stood up and
proven **fault-evident BEFORE any capture lane runs**, so that when D0 (the hardware serial
fingerprint) clears, scoring is instant and already trusted.

## What it does
Scores **device serial telemetry** (the DUT's only rich audio channel — D5: "AP" = Audio
Processing, not WiFi) against **hybrid-beat-tracker** `.beats` ground truth (D4 VERIFIED).

- **Beat/tempo (P4):** beat P/R/F (±70 ms, mir_eval-style greedy match), BPM accuracy-1 and
  octave-tolerant accuracy-2, time-to-lock, %-frames-locked (steady-state only), octave-error rate.
- **Metrology (P1):** SNR (noise-subtracted dB), dBFS, noise floor from the VU series.
- **Statistics:** paired bootstrap 95% CI, Wilcoxon signed-rank, Cohen's d_z, Benjamini-Hochberg FDR.
- **D1 framing guard:** refuses model-level ("IM73D122 is better…") claims; all output is
  **Unit-Bench(IM73D122) vs Unit-Main(SPH0645)**.

`CMLt/AMLt/P-score/information-gain` use `mir_eval` if installed (optional); everything above is
pure stdlib and always available.

## Fault-evident guarantee (Gate 0)
```
python3 test_scorer_fault_evident.py      # 23/23 → GREEN → cleared for capture scoring
```
The self-test injects a fault battery — shifted beats, random beats, **blank/empty output** (the
"harness passes on blank data" anti-pattern this whole investigation kept hitting), octave errors,
wrong BPM, never-locks, null-vs-real statistical effects — and asserts the scorer **detects each
one**. If any injected fault is not caught, the verdict is **RED** and the harness must not be used.
Current status: **26/26 GREEN** (2026-07-10) — re-proven against the REAL keyed `[AP]`/`TEMPO,t=` device formats and validated on captured hardware logs.

## Usage
```
python3 mic_ab_scorer.py --corpus /Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark
python3 mic_ab_scorer.py --selftest
```
Corpus layout (D4 VERIFIED): `<root>/clips/*.wav` + `<root>/annotations/*.beats`, `manifest.json`
uses bare filenames → resolved as `clips/<clip_file>` + `annotations/<annotation_file>`.

## Status / what's left
- **DONE:** scoring core + metrology + stats + framing guard + Gate-0 fault battery (GREEN);
  end-to-end verified against the real 21-track corpus (loaded beat counts match `madmom_beats`).
- **DONE (2026-07-10):** **D0 CLOSED** — both K1s fingerprinted on live hardware (RUNTIME_TIMING_GUARD
  banner captured; identity triple-confirmed incl. the live DC-signature −2 IM73D / −4545 SPH0645).
  **Parser finalised** to the REAL keyed `[AP]`/`TEMPO,t=` formats (read from the device emit code +
  live capture) and validated against captured hardware logs. The prior integration TODO is resolved.
- **PENDING — the one remaining step (physical, human-at-bench):** flash the composite probe build
  (mic env + `ENABLE_TEMPO_STREAM`, for the ~20 Hz beat-tick stream that beat-F needs — the ~1.4 Hz
  `[AP]` poll the shipping build already streams covers BPM/lock/conf), then play the approved corpus
  into each mic at an SPL-metered level and capture serial. No terminal agent can do the acoustic
  delivery; run-book: dossier §13. Feed the capture logs straight to this scorer.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-10 | research-agent (orchestrator) | Created — fault-evident mic A/B scorer + Gate-0 self-test (23/23 GREEN); end-to-end verified vs real corpus. Build-lane-2 of the dossier §13.2, unblocked by D4 closure. |
