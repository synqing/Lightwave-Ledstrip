---
abstract: "Turnkey operator run-book for the ONE remaining step of the IM73D122-vs-SPH0645 K1 mic A/B: the physical acoustic capture (human-at-bench). Everything upstream (all decisions D0-D5, corpus, scorer) is CLOSED/proven. This is the speaker→mic measurement procedure + the exact capture/score commands. Unit-pair framing (D1) is mandatory on all outputs."
---

# Mic A/B — Physical Capture Run-book (the one human-at-bench step)

**Status going in:** all gates CLOSED. D0 fingerprinted on hardware; D1=unit-pair; D2=playback approved
(hybrid-beat-tracker corpus + 1 kHz tone); D3=SPL meter available; D4=corpus verified; D5=Audio-Processing/serial.
Scorer (`mic_ab_scorer.py`) is fault-evident (26/26 GREEN) and format-matched to the real device frames.
**This is the only step no terminal agent can do — it needs a person, a speaker, an SPL meter, and the two K1s in a room.**

**Golden rule (D1):** every number you produce is **Unit-Bench(IM73D122)** vs **Unit-Main(SPH0645)** — never
"IM73D122 vs SPH0645 as mic models" (N=1 per model; the scorer's `guard_no_model_claim()` enforces this).

---

## 0. Bench setup (once)
- Both K1s on the same USB hub (they are, as of D0). One loudspeaker. One SPL meter.
- Fixed jig: each K1's mic port at a **reproducible distance/angle** from the speaker. Same position for both
  units (interleaved swap) — position is a confound, hold it constant. Log ambient SPL.
- Quiet room. Same session/time-of-day where possible.

## 1. Confirm identity (D0 re-check — 30 s)
```
python3 capture_serial.py --unit bench --seconds 3 --out /tmp/id_bench.log
python3 capture_serial.py --unit main  --seconds 3 --out /tmp/id_main.log
```
The tool REFUSES if the USB MAC doesn't match. Sanity-glance the log: bench shows `DC=-2`, main shows `DC≈-4545`.

## 2. Flash the capture build (only if you need beat-F / P4 timing)
The shipping build already streams `[AP]` at ~1.4 Hz → enough for **BPM / lock / conf / time-to-lock / %-locked**.
For **beat-F-measure / CMLt** you need the ~20 Hz `TEMPO,` stream → flash the composite build **per unit, MAC-guarded**:
```
# bench (IM73D):  env with K1_MIC_IM73D_PDM_V1 + ENABLE_TEMPO_STREAM
~/.local/bin/pio run -d /Users/spectrasynq/SpectraSynq_K1_Firmware -e k1_tempo_probe \
    -t upload --upload-port /dev/cu.usbmodem1401     # VERIFY MAC 89:B4 first
# main (SPH0645): same telemetry env, main GPIO map
~/.local/bin/pio run -d /Users/spectrasynq/SpectraSynq_K1_Firmware -e k1_tempo_probe \
    -t upload --upload-port /dev/cu.usbmodem12401    # VERIFY MAC 87:F8 first
```
(Or add `-DENABLE_TEMPO_STREAM=1` to the mic env for a true composite. Run P9 parity: confirm the telemetry
flag didn't perturb DSP timing — compare `RUNTIME_TIMING_GUARD` `timing_ok=1` before/after.)

## 3. P0 — calibration gate (RUN FIRST, both units)
Play the **1 kHz reference tone** at an **SPL-meter-verified** level (state exact file, dBSPL, duration, stop-command
before playing — D2 literal constraint). Capture per unit:
```
python3 capture_serial.py --unit bench --seconds 20 --out captures/p0_bench.log
python3 capture_serial.py --unit main  --seconds 20 --out captures/p0_main.log
```
Compute each unit's gain offset (maps the −2 vs −4545 DC / gain-map divergence to a measured constant). All
subsequent raw-level (P1) comparisons apply this offset. **Without P0, raw-level deltas are gain artefacts, not acoustics.**

## 4. P1 / P4 / P7 — the core battery (interleaved A-B-A-B, N≥20)
For each stimulus (P1: pink noise + stepped sine + silence; P4: hybrid-beat-tracker corpus clips; P7: true silence),
state file/SPL/duration/stop, then **alternate units** (never block all-bench-then-all-main):
```
python3 capture_serial.py --unit bench --seconds <clip_len> --out captures/<phase>_bench_trialNN.log
python3 capture_serial.py --unit main  --seconds <clip_len> --out captures/<phase>_main_trialNN.log
# repeat, interleaved, N>=20 (N>=30 for time-to-lock)
```
Corpus playback (P4): use `hybrid-beat-tracker/tests/benchmark/clips/<clip_file>.wav`; ground truth =
`annotations/<annotation_file>.beats`.

## 5. Score (software — the proven oracle)
```
python3 mic_ab_scorer.py --selftest        # re-confirm 26/26 GREEN before trusting any run
# then, in Python / a driver: for each trial pair,
#   frames = parse_frames(open(log).read().splitlines())
#   beats  = frames_to_beat_times(frames)         # P4 (needs TEMPO stream)
#   beat_prf(ref_beats, beats); bpm_accuracy(ref_bpm, est); time_to_lock(frames, ref_bpm)
#   snr_db(signal_rms, noise_rms)                 # P1 (from peak_scaled/VU under P0 calibration)
#   then paired stats: bootstrap_ci / wilcoxon_signed_rank / cohens_dz / benjamini_hochberg
```
Steady-state only (exclude each trial's measured time-to-lock window). Report mean+median+95% CI+effect size+BH-p.

## 6. Deliverable
Per-metric **Unit-Bench − Unit-Main** delta table (raw + gain-corrected), with CI, effect size, BH-adjusted p.
Datasheet envelope (dossier §9: IM73D +8 dB SNR etc.) is **interpretive context only** — the claim ceiling is unit-pair (D1).

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-10 | research-agent (orchestrator) | Created — turnkey physical-capture run-book; the one human-at-bench step after all gates closed. Pairs with capture_serial.py + mic_ab_scorer.py. |
