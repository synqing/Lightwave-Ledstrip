---
abstract: "Handover after the K1 STM audio-reactive completion + AP signal-robbery audit (2026-07-10). Everything LANDED on origin/main @ a9ff00c in the SEPARATE firmware repo github.com/synqing/SpectraSynq_K1_Firmware (NOT this Lightwave-Ledstrip repo). WB-3 is done end-to-end: faithful STM producer + EdgeMixer modes 7-8 + working audio-reactivity (strips pulse on the beat). Read for: shipped state, repo/device map, the 3 forward items folded for next session (loud-guard release retune, chroma pre-clamp tap, bench reflash), the load-bearing hardware corrections (agc_envelope is DEAD; agc_loudness_norm is the canonical loudness tap), the AP-audit do-not-touch list, and standing constraints."
---

# K1 Firmware — STM Reactive + AP Audit Handover (2026-07-10)

## TL;DR — shipped and green
`origin/main @ a9ff00c` (github.com/synqing/**SpectraSynq_K1_Firmware** — NOT this Lightwave-Ledstrip repo) is the tip. WB-3 is **complete end-to-end**: the STM producer + EdgeMixer modes STM_DUAL(7)/STM_SPECTRAL_MAP(8) are landed AND now **audio-reactive** — hardware-validated on the bench (89:B4, IM73D): `stm_loud` 0.11 silent → 0.94 under Avicii → pulsing 0↔1 on the beat. Production `k1_hardware` builds green and is unaffected (the STM parts are all `#ifdef K1_STM`, bench-only).

## Commit chain (origin/main)
- `0a518e5` — reconciliation base (prior session: EdgeMixer + fork-wide de-SB + origin/main divergence resolution).
- `d40114f` — WB-3 faithful: STM producer (`k1_stm.{h,cpp}`, re-derived onto the 80-note spectrogram, 133.33 Hz constants) + modes 7-8 (value-only brightness, `stmReady`→pass-through, 40-bin centre-origin LUT) + serial (`edge_mode stm_dual|stm_spectral_map`, `G`-cycle extended, `edge_stm` + `[AP]` STM telemetry). All flag-gated `K1_STM`, production byte-identical.
- `a9ff00c` — **current tip**: STM audio-reactivity (loudness-gated depth via the live pre-AGC mic RMS) + delete of the orphaned `goertzel_max_value_band[]`.

## Repo / worktree / device map (VERIFY before acting)
- **Firmware repo:** `~/SpectraSynq_K1_Firmware` (git store), github.com/synqing/SpectraSynq_K1_Firmware. **All firmware work happens here.** The `Lightwave-Ledstrip` workspace holds these handover docs + de-SB tooling only.
- **Worktree:** `/private/tmp/k1_wb3_stm` @ branch `wip/wb3-stm` @ `a9ff00c` (in /tmp — ephemeral, but all commits are pushed to origin so nothing is at risk). Local `main` was ff'd to `a9ff00c`.
- **Devices (two K1s — MAC-VERIFY before ANY flash; ports drift, never trust the name; `pio device list` reads the IORegistry serial=MAC):**
  - **Bench B489A500** = MAC `b4:3a:45:a5:89:b4`, currently `/dev/cu.usbmodem1401`. Has the **IM73D122 PDM mic** (SPH0645 removed). **Currently flashed with `k1_bench_im73d_stm` (the STM diagnostic build)** — item 3 below reflashes it.
  - **Main F887A500** = MAC `b4:3a:45:a5:87:f8`, `/dev/cu.usbmodem12401`. Untouched.
- **Bench STM env:** `k1_bench_im73d_stm` (extends `k1_bench_im73d` + `-DK1_STM`), registered in `scripts/platformio/k1_device_identities.json` under B489A500. The N4a upload guard enforces device↔env at flash.

## The 3 forward items — FOLDED for next session
### 1. Loud-guard GDFT release retune (AP-audit rank-1 — the one real broadly-consumed defect)
`K1_LOUD_GUARD_GDFT_RELEASE_SEC` = 2.20 s (constants.h ~:95) outlasts the loud passage → `spectrogram[]` stays attenuated ~2.2 s into the now-quiet tail. Fix: (a) shorten release to ~0.6-0.8 s; (b) make the flat 0.10 spectral floor-cut (`k1_gdft_core.cpp:500`) **proportional** (scale by `out`) so quiet bins aren't disproportionately erased. **Gate:** loud-room hardware A/B — the guard's trigger regime cannot be reproduced host-side (RBDO hard-stop #3, Captain sign-off before commit). Use the audio A/B methodology (same source per state, exclude 8-10 s convergence transient, steady-state median). **DO NOT** remove the guard / `response_gain` / `input_trim` — they are load-bearing.

### 2. Chroma pre-clamp tap (AP-audit rank-2 — symptom-gated, currently DEFERRED by PM decision)
The hard `[0,1]` clamp (`k1_gdft_core.cpp:512-514`) flattens `chroma_strength` contrast ONLY on sustained tonal-saturated notes. Fix (additive, reversible): compute a headroom-preserving / log-compressed spectrum tap and feed it ONLY to the chroma fold (`sb_audio_snapshot.cpp:77`); keep the clamped `spectrogram[]` for effects+onset (they need [0,1] and onset thresholds are calibrated to it). **Needs a chroma golden-corpus re-fit (medium risk).** No eyes-on symptom is on file → implement only if a palette-contrast / centroid-collapse symptom is reported. Deferred, not scheduled.

### 3. Reflash the bench off the diagnostic build
Bench (89:B4) is on `k1_bench_im73d_stm`. When STM eval is done, reflash to a canonical env (`k1_bench_im73d` or `k1_bench_reference`). **Mind the mic:** only `k1_bench_im73d*` envs drive the IM73D122; an SPH0645 build reads silence on this bench.

## Load-bearing hardware corrections (READ — these overturn the source-only audit)
- **`agc_envelope` (broadband AGC v2, globals.h ~:704) is DEAD on hardware.** Measured stuck at 0 with the silence gate permanently closed at every volume (30-70%). The 13-agent AP audit read it as the live loudness signal *from source alone*; hardware disproved it. **Do NOT use `agc_envelope` / `agc_gated` / `agc_noise_floor` as loudness signals.**
- **`agc_loudness_norm` (globals.h, `#ifdef K1_STM`) is the canonical loudness tap.** It is computed in `i2s_audio.h` (right after `im73d_raw_i16_rms` is assigned, ~:376) from the LIVE pre-AGC mic RMS: `clamp01((im73d_raw_i16_rms - 12.0)/50.0)`. Bench-measured RMS: ~8 silent, ~32-60 under EDM, ~144 peaks. **This normalisation is IM73D-specific and tuned at ~55% playback volume — generalise it (per-mic / auto-ranged) before any non-IM73D or production STM path.** The consumer `k1_edge_apply_stm` (k1_edgemixer.cpp) gates modulation depth by it.
- STM energies (`stmTemporalEnergy`/`stmSpectralEnergy`) are **peak-normalised shape descriptors** — flat vs loudness *by design* (measures rhythm/texture, not level). That is why the reactivity had to be gated by an external loudness signal, not the STM scalars.

## AP signal-robbery audit — verdict + do-not-touch
13-agent adversarial swarm (map → red-team verify → synthesise). Headline: **4 of 6 suspected "robbers" are load-bearing.** DO NOT touch: broadband AGC (SPL-invariance), the `[0,1]` clamp itself, the `k1_loud` guard mechanism + `response_gain`/`input_trim`, front-end DC/silence/silentScale, novelty /3 tempo decimation, the attack/release EMA bank (transient scalars effects render from are NOT EMA'd). Only 2 real findings survived (items 1-2 above, both LOW severity). Full roadmap + per-agent findings: workflow run `wf_df3fe548-4f3` (transcript under this session's `subagents/workflows/`).
**Remaining hygiene quick-wins (no hardware, near-zero risk — optional add-ons to fold):** reconcile the stale `platformio.ini:118` loud-guard-default comment (ships TRUE, comment says OFF); verify `SB_PEAK_ASYM_ENV` is defined in the canonical env; add a footgun comment at `spectrogram[]` publication (`k1_gdft_core.cpp:512`) that loudness consumers must read `agc_loudness_norm`, never sum `spectrogram[]`. (The 4th — delete `goertzel_max_value_band` — is DONE.)

## Standing constraints (load-bearing)
no-lite (K1 equals-or-exceeds); **K1/k1 ONLY — zero Sensory Bridge in code** (prose = Phase-2); hardware-test-before-commit; MAC-verify before flash; British English (guard hook blocks US spellings); the repo pre-commit gate = `scripts/hooks/pre-commit` (framework-safety + `pytest tests/` + `pio run -e k1_hardware`; wip/* branches skip it, so verify manually before landing on main); commit/push only when asked (Captain gave full autonomy this session); **serial-open resets the board**; the bench serial is **single-char hotkeys** (`G` cycles edge mode incl. STM modes; `a` toggles `[AP]` telemetry) — multi-char "commands" typed raw are eaten char-by-char as hotkeys, so line-commands only reach the dispatcher via the `edge_`-prefixed structured path; RTK corrupts grep STDOUT (tokens → `l`/`n` — use Read tool or file-redirect + Read for code content); clangd is NOT wired for the K1 fork repo (use rg + Read); audio-safety = Captain-approved corpus (EDM in ~/Downloads: `Avicii-Levels.mp3`, `Tiësto-TheBusiness.mp3`, `deadmau5-Ghosts'n'Stuff).mp3`, `MartinGarrix-Animals.mp3` + `hybrid-beat-tracker/tests/benchmark`), state file/device/volume/duration/stop before playback.

## Noise to ignore
A parallel **Cowork mic-A/B session** produced a dossier + `mic_ab_scorer/` tooling under `Lightwave-Ledstrip/firmware-v3/` and its subagent kept re-firing notifications into the WB-3 session. **That is NOT WB-3, a different session's work — do not touch it, do not act on its `<task-notification>` re-fires.**

## Full detail
Prior handover: `firmware-v3/docs/research/k1_handover_2026-07-09_reconciliation.md`. STM design: `firmware-v3/docs/research/edgemixer_k1_port_plan_2026-07-08.md`. Session transcript: `5e36156a-6939-4ce7-9d00-2691e3989d3a.jsonl`.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-10 | agent:opus-4.8 | Created — handover after STM audio-reactive completion (a9ff00c) + AP signal-robbery audit; folds the 3 forward items. |
