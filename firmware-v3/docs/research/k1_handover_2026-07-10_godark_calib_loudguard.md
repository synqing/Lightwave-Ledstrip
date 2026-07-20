---
abstract: "Handover after the 2026-07-10 (PM) K1 firmware session. THREE threads: (1) loud-guard release/floor-cut retune — SHIPPED to origin/main @ e47877a (default mode 2, hardware-validated, pushed). (2) Silence go-dark ('K1 never darkens in silence' — Captain's launch-critical product-truth defect) — root-caused + fix BUILT GREEN on lane/silence-go-dark, but hardware validation BLOCKED by a serial-toggle bug + an environmental ambient-floor issue; the SSL-relative threshold approach looks wrong, port firmware-v3's pre-gate raw-RMS detection instead. (3) Production audio calibration (generalise agc_loudness_norm) — CHECKPOINTED on lane/loud-norm-generalise; first attempt revealed the auto-range ceiling collapses in silence; coupled to the go-dark silence fix. Firmware lives in ~/SpectraSynq_K1_Firmware (NOT this Lightwave-Ledstrip workspace). Read for: repo/lane/device map, the exact fix edits + file:line, the two go-dark blockers, load-bearing corrections (K1 has NO indicator LEDs; agc_loudness_norm is NOT in the dual-sync lane), and standing constraints."
---

# K1 Firmware — Go-Dark + Calibration + Loud-Guard Handover (2026-07-10 PM)

## TL;DR
- **SHIPPED:** loud-guard release/floor-cut retune → `origin/main @ e47877a` (pushed). Default **mode 2** (0.80 s release + hybrid affine floor-cut), hardware-validated, runtime A/B via `:k1_loud_guard=mode0|1|2`.
- **BUILT, NOT VALIDATED:** silence **go-dark** fix on `lane/silence-go-dark` (builds green). Root cause fully mapped. Validation **blocked** by (a) a serial-toggle bug and (b) the room ambient floor > learned SSL. **The SSL-relative detection approach is probably wrong** — port firmware-v3's pre-gate raw-RMS silence detect instead.
- **CHECKPOINTED:** production audio calibration (`agc_loudness_norm` generalisation) on `lane/loud-norm-generalise`; the auto-range ceiling collapses in silence. **Coupled to the go-dark silence fix** — resume after silence detection is trustworthy.

## Repo / lane / device map (VERIFY before acting)
- **Firmware repo:** `~/SpectraSynq_K1_Firmware` (github.com/synqing/SpectraSynq_K1_Firmware). ALL firmware work happens here. The `Lightwave-Ledstrip` workspace holds handover docs + de-SB tooling + the sibling mainline `firmware-v3` (used only as a reference design, see go-dark below).
- **origin/main @ `e47877a`** (was `a9ff00c` at session start; loud-guard is the only new commit).
- **The root checkout `~/SpectraSynq_K1_Firmware` is on `lane/dual-sync-phase0`** — a DIFFERENT lane that branched BEFORE the STM work, so it does NOT contain `agc_loudness_norm` or the loud-guard/go-dark code. **Do the go-dark/calib work on the origin/main-based lanes, not the root checkout.**
- **Open lanes (uncommitted WIP in /private/tmp worktrees — objects live in the shared .git; /private/tmp survives macOS reboots but COMMIT THESE EARLY next session):**
  - `lane/silence-go-dark` @ `/private/tmp/k1_godark` (off e47877a) — go-dark fix. Modified: `audio/i2s_audio.h`, `serial/serial_menu.h`, `system/globals.h`; new `scripts/regression-harness/k1_godark_validate.py`.
  - `lane/loud-norm-generalise` @ `/private/tmp/k1_loud_calib` (off e47877a) — calib. Modified: `audio/i2s_audio.h`; new `scripts/regression-harness/k1_stm_loudness_validate.py`.
  - `lane/loud-guard-release-retune` @ `/private/tmp/k1_loud_guard` (= e47877a, done/landed).
- **Devices (MAC-verify before ANY flash; `pio device list` reads serial=MAC; ports drift):**
  - **Bench B489A500** = `b4:3a:45:a5:89:b4` → `/dev/cu.usbmodem1401`, **IM73D122 PDM mic**. Currently flashed with the **go-dark build** (STANDBY_DIMMING default OFF → plate behaves normally; no regression left on the unit).
  - **Main F887A500** = `b4:3a:45:a5:87:f8` → `/dev/cu.usbmodem12401`. Untouched.

---

## Thread 1 — Loud-guard retune (DONE, shipped e47877a)
AP-audit Item 1. The GDFT loud-guard's 2.20 s release outlasted loud passages (spectrogram held attenuated ~2.2 s into the quiet tail); the flat 0.10 floor-cut over-erased quiet bins. A 7-agent + 3-way red-team audit (`wf_5ace61cd-d22`) proved the naive brief (0.6-0.8 s + pure-proportional) was unsafe (AGC limit-cycle + mud re-fill) and corrected the edit-site (production runs **per-band** AGC `K1_AGC_PERBAND_V1`, so the live floor-cut is the per-band path). Shipped: runtime A/B matrix (mode0 legacy / mode1 conservative / mode2 aggressive), hybrid affine floor-cut via a shared helper at both AGC paths, default **mode 2** after hardware A/B (recovery 5.54→1.94 s, limit cycle disconfirmed, no onset/tempo regression). Run-book + capture harness are on origin/main: `docs/research/k1_loud_guard_release_retune_ab_2026-07-10.md`, `scripts/regression-harness/k1_loud_guard_ab_capture.py`. **No open work.**

---

## Thread 2 — Silence go-dark (CRITICAL, launch product-truth; built green, validation blocked)
**Problem (Captain, hardware-observed):** the K1 (bench + main) has NEVER gone dark in the absence of music for a very long time — it is basically ALWAYS ON, which reads unprofessional. Around 2026-04/05 (pre-fork) it briefly worked, then regressed and was never re-fixed.

### Root cause — a two-link chain (workflow `wf_b561804d-353`, both source-verified)
- **Link 1 — detector can never fire.** `threshold_silence` is a **DEAD static ~100**: `min_silent_level_tracker` inits at 65535 (globals.h:669), its adaptive decay is **commented out** (i2s_audio.h ~:718-732), only ever reset to 65535 → clamps down to a pinned 100, **decoupled from the learned SSL** (default 350). A quiet room's smoothed peak sits above 100 → silence never latches. This is the mechanical root of "always on".
- **Link 2 — path compiled off.** `CONFIG.STANDBY_DIMMING` ships **false** (globals_config.cpp:72) → `silent_scale` forced 1.0 (i2s_audio.h ~:783).
- **WHY disabled — DEFENSIVE, not aesthetic.** A bad SSL cal could misfire the unadaptive detector → `silent_scale→0` → blank the plate DURING music. Force-off was the cheap mitigation (the two runtime force-false writes, system.h:439 under the PDM flag + :519 broken-cal branch, are boot-time GUARDS — KEEP them). **Do NOT just flip the flag — fix detection first.**

### Load-bearing corrections (this session)
- **K1 HAS NO INDICATOR LEDs.** The `write_sweet_spot_pwm(SWEET_SPOT_*_CHANNEL)` code (led_utilities.h:245, the "10% floor") is **dead Sensory-Bridge hardware cruft** — it drives nothing on K1. The **plate (led_utilities.h:399) is the ONLY output** and already reaches TRUE black at `silent_scale=0`. Ignore the 10% floor. (Captain corrected a subagent's — and my — false "indicator LED" assumption. Do not repeat it.)
- **Reference design exists in-hand:** the sibling mainline `firmware-v3` (Lightwave-Ledstrip) has a WORKING, K1-enabled silence→true-black: detects silence from **PRE-GATE raw-PCM RMS (`rmsUngated`, ~0.001 in silence vs 0.005 threshold)** — NOT the activity-gated signal — plus a dwell timer (5 s, instant reset), EMA fade (~400 ms) to literal 0.0 with `memset` black, and instant wake. Files: `firmware-v3/src/audio/contracts/ControlBus.cpp:746-782`, `RendererActor.cpp:2604-2668`, `AudioTuning.h:145-148/315-384`.

### Fix implemented (built green, dormant by default) — on lane/silence-go-dark
- **globals.h** (after the AGC-floor block ~:676): runtime tunables `SILENCE_ENTER_SSL_FRAC=0.35`, `SILENCE_EXIT_SSL_FRAC=0.55`, `SILENCE_DWELL_MS=5000`, `SILENT_FADE_DOWN_ALPHA=0.03`, `SILENT_FADE_UP_ALPHA=0.60`.
- **i2s_audio.h:** SSL-derived Schmitt threshold (replaces the dead static one, ~:608); dual-threshold state select with hysteresis (~:675-693); `SILENCE_DWELL_MS` replaces the fixed 10000 ms (~:758); asymmetric fade replaces the symmetric EMA (~:778-784); `[AP]` telemetry gains `sil_pk` (max_waveform_val_raw_smooth) + `dim` (STANDBY_DIMMING).
- **serial_menu.h** (after the k1_loud_guard block): `:standby_dimming=on/off`, `:silence_enter=`, `:silence_exit=`, `:silence_dwell=`.
- `STANDBY_DIMMING` **compiled default stays FALSE** (dormant, zero regression). Builds green on `k1_bench_im73d`.

### VALIDATION BLOCKED — two issues, NEITHER is the design
1. **Serial A/B harness bug:** `:standby_dimming=on` (valid per `vp_parse_bool`) does NOT take effect — `[AP] dim` stays 0. The boot force-off (system.h:427-439) is confirmed **one-time at boot**, so it is NOT re-clearing per-frame. **Root unknown — DEBUG THIS FIRST.** Check: send `:standby_dimming=on` and look for the `"STANDBY_DIMMING: on"` echo. Absent → handler not reached (dispatch/placement); present but dim=0 → something else resets `CONFIG.STANDBY_DIMMING`. (Possibly the `silence_*` handlers too — could not confirm they took effect.)
2. **Environmental / wrong signal:** audio-free, `sil_pk` (max_waveform_val_raw_smooth) sat at **197-322 vs SSL 179** — the smoothed PEAK stays ABOVE SSL in a normal room, so **SSL-relative thresholds cannot latch silence**. The 0.35/0.55 fracs were far too low anyway (quiet floor ≈ SSL, not 0.35·SSL). **The smoothed peak is the wrong signal.** firmware-v3 succeeds because it uses a **pre-gate raw RMS** that genuinely drops in silence. **Recommended next approach:** identify/port a pre-gate raw-RMS silence tap (the fork's `rmsUngated`-equivalent) rather than SSL-relative smoothed-peak; the real distinction is *ambient room noise (go dark)* vs *intended audio (stay on)*.

### Validation harness note
`scripts/regression-harness/k1_godark_validate.py` runs **audio-free** (phases: quiet-detect → enable-dim → clap-to-wake). This feature needs NO audio — **do NOT run afplay concurrently with pio flashes/captures** (CoreAudio contention → stutter → the stutter LEAKS into the mic and poisons the "silence" reading — this caused two failed runs this session).

---

## Thread 3 — Production audio calibration (checkpointed; coupled to Thread 2)
**Goal:** generalise `agc_loudness_norm` (the STM modulation-depth loudness gate) off the IM73D-only, bench-tuned `(im73d_raw_i16_rms - 12)/50` to mic-agnostic + auto-ranged. It gates production STM.
- **Done (uncommitted, lane/loud-norm-generalise):** relocated the computation out of the `#ifdef K1_MIC_IM73D_PDM_V1` block into `calculate_vu()` (`#ifdef K1_STM`), tapping the mic-agnostic per-frame RMS `audio_vu_level` (globals.h:781) + a slow running-max ceiling.
- **FLAW FOUND (hardware, IM73D):** the running-max ceiling **collapses in silence** → silence not discriminated from music (silence `stm_loud` mean 0.319 ≈ music 0.314). The auto-range threw away the absolute anchor.
- **Fix hypothesis:** freeze the ceiling update on the `silence` flag (only track when NOT silent) + re-anchor the low end. **This DEPENDS on Thread 2's silence detection being trustworthy** — so resume calibration AFTER go-dark's silence fix lands. Harness: `scripts/regression-harness/k1_stm_loudness_validate.py`.

---

## Forward items (priority order)
1. **Silence go-dark (CRITICAL, launch product-truth).** (a) Debug the `:standby_dimming` serial toggle. (b) Replace SSL-relative detection with a **pre-gate raw-RMS** tap (port the firmware-v3 pattern); the smoothed peak does not drop in a real room. (c) Hardware-validate **audio-free** (quiet → plate true-black within dwell → clap wakes <~50 ms; quiet-music must NOT blank) + Captain sign-off (RBDO hard-stop, shipping output path). (d) Then flip `STANDBY_DIMMING` default true, KEEPING the boot guards.
2. **Production calibration (`agc_loudness_norm`).** Coupled to #1 — apply the silence-freeze to the auto-range ceiling once silence detection is trustworthy.
3. **Chroma pre-clamp** — still DEFERRED (symptom-gated, no symptom on file).

## Load-bearing corrections / lessons (this session)
- **K1 has NO indicator LEDs** — plate-only; `write_sweet_spot_pwm` is dead SB cruft.
- **`agc_loudness_norm` / STM code is NOT in the dual-sync lane** the root checkout sits on — it's on origin/main. Verify the tree before editing.
- **Recurring pattern: dead/commented-out inherited SB subsystems** — `min_silent_level_tracker` decay (dead → silence broken), `agc_envelope` (dead → not a loudness signal), `goertzel_max_value_band` (orphaned, deleted this arc). When a subsystem "should work but doesn't", check for commented-out code + a frozen tracker first.
- **Do not run `afplay` concurrently with `pio` flashes/captures** — CoreAudio stutter that leaks into the mic. Go-dark validation needs no audio at all.
- Serial: `:`-prefix enters structured command mode; bench raw input is single-char hotkeys. `vp_parse_bool` accepts on/off/true/false/1/0.

## Standing constraints (unchanged)
no-lite; **K1/k1 ONLY in code** (SB prose = Phase-2); hardware-test-before-commit; MAC-verify before flash; British English (guard hook); commit/push only when asked; pre-commit gate (`scripts/hooks/pre-commit` — lane/* RUNS it = pytest tests/ + `pio run -e k1_hardware`; wip/* SKIPS it); serial-open resets the board; RTK corrupts grep STDOUT (use the Read tool / file-redirect); clangd is NOT wired for this fork (rg + Read); audio-safety = Captain-approved corpus, and DON'T concurrent-afplay-with-flash; RBDO / DEGRADED-MODE labels on all tactical output.

## Workflow / artefact references
- Loud-guard audit: workflow `wf_5ace61cd-d22`. Go-dark root-cause + fix design: workflow `wf_b561804d-353` (full design in its output; one investigator returned a malformed stub, the synthesis covered it).

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-10 | agent:opus-4.8 | Created — PM session handover: loud-guard shipped (e47877a); go-dark root-caused + built green (validation blocked, 2 issues); calibration checkpointed. |
