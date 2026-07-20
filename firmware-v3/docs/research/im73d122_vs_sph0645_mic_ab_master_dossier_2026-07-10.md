---
abstract: "MASTER DOSSIER v1.5 — IM73D122 (bench K1) vs SPH0645 (main K1) microphone A/B. CONSOLIDATED + DECISIONS CLOSED. Contains: (1) STM forensic (firmware-v3 record; STM ABSENT on DUT — P6 dropped); (2) device/repo reality (DUT firmware = SpectraSynq_K1_Firmware de-SB fork @ 12.8kHz/hop96/133.33Hz, NOT firmware-v3); (3) six recon lanes + two corrected lanes (A2/B2: 'AP' = Audio Processing, measurement channel = SERIAL, rich telemetry is probe-build-only) + two independent verification passes; (4) integrated confound analysis (digital-gain confound fixable via P0 calibration; N=1-per-model ceiling ACCEPTED per D1); (5) committed battery: CORE P0/P1/P4/P7, P3 contingent on D4, P2/P5 optional, P8 re-pointed to serial transport, all serial-channel; (6) execution plan. CAPTAIN DECISIONS CLOSED 2026-07-10: D5 = Audio-Processing/serial · D1 = (c) unit-pair framing (two units as configured, no swap, no second unit) · D2 = playback APPROVED (hybrid-beat-tracker corpus + 1kHz cal tone; per-session specifics still stated before play) · D3 = SPL meter AVAILABLE (absolute dB claims permitted). ALL GATES CLOSED. D0 CLOSED 2026-07-10 on LIVE hardware (both K1s' RUNTIME_TIMING_GUARD banner captured: sample_rate=12800/hop96/declared_ap_hz=133.333; bench DC=-2 IM73D vs main DC=-4545 SPH0645 = live gain-confound confirmation). Only outstanding step: the controlled acoustic capture (music→mics, SPL-metered) — physical, human-at-bench. D4 CLOSED 2026-07-10 — corpus VERIFIED at hybrid-beat-tracker/tests/benchmark (21/21 wav+beats pairs, 2091 beats, clips/+annotations/ layout). All results MUST be framed as Unit-Bench(IM73D122) vs Unit-Main(SPH0645) — never as mic models. Read before scoping, building, or running any K1 mic A/B."
---

# IM73D122 vs SPH0645 — K1 Microphone A/B Master Dossier

**Version:** 1.3 · **Date:** 2026-07-10 · **Authors:** research-agent (orchestrator + 6-lane SSA swarm + corrected lanes A2/B2); independent verification + consolidation by review agent (Cowork remote session, read-only mounted-repo re-reads); decisions by Captain · **Repo of record for this doc:** Lightwave-Ledstrip/firmware-v3 · **Repo of record for the DEVICE-UNDER-TEST firmware:** SpectraSynq_K1_Firmware (§3).

---

## 0. HOW TO READ THIS / RBDO STATUS BANNER

**v1.4 is the consolidated, decision-closed edition.** v1.1 (independent verification pass), v1.2 (corrected device-telemetry recon), and v1.3 (Captain-decision consolidation) are fully propagated through the body — no section below contradicts another. **The plan is now fully runnable — ALL GATES CLOSED.** D0 was executed on live hardware 2026-07-10 (both K1s fingerprinted over read-only serial); D4 (corpus) closed the same day. The only outstanding step is the physical acoustic capture (human-at-bench), which no terminal agent can perform:

- **D4 — locate the hybrid-beat-tracker `.beats` corpus. ✅ CLOSED 2026-07-10 (orchestrator-VERIFIED on local filesystem).** Corpus is present at `/Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark`: **21/21 complete `.wav`+`.beats` pairs**, 2091 ground-truth beats, tempo 60–207 BPM, 18 hard / 3 medium, zero missing, zero empty. **Layout the scorer must use:** `.wav` in `clips/`, `.beats` in `annotations/`, `manifest.json` uses **bare filenames** → resolve as `clips/<clip_file>` + `annotations/<annotation_file>` (NOT manifest-root-relative). `.beats` format = one beat timestamp (seconds) per line. P3 (onset) and full P4 (beat-F) ground truth now available (P3 uses beats-as-percussive-onset proxy; a dedicated onset annotation would sharpen it). Procedure record: §13.0.
- **D0 — serial-fingerprint each K1. ✅ CLOSED 2026-07-10 (executed on live hardware, read-only serial).** Both K1s are USB-connected; a DTR reset captured the boot banner VERBATIM from BOTH units: `RUNTIME_TIMING_GUARD: timing_ok=1 sample_rate=12800 samples_per_chunk=96 tempo_decim=3 declared_ap_hz=133.333 declared_nov_hz=44.444 ... ap_core=0 vp_core=1 core_ok=1`. Identity **triple-confirmed**: (1) USB MAC descriptors (`…89:B4` bench / `…87:F8` main); (2) SB-fork-only telemetry format (`[AP]`/`[VP]`/`SSL`/`cal_source` — absent in firmware-v3); (3) live mic **DC-signature — bench `DC=-2` (IM73D122 linear) vs main `DC=-4545` (SPH0645 affine pedestal)** = live on-hardware confirmation of the Lane C gain confound. Both units already stream `[AP]` per-frame `bpm/conf/lock/phase/beat/onset` at **~1.4 Hz** (tempo-VALUE metrics live now; beat-F still needs the ~20 Hz `TEMPO,` probe build). **NO GATES REMAIN.** The only outstanding step is the controlled acoustic capture (music→mics, SPL-metered) — physical, human-at-bench.

### CAPTAIN DECISIONS — CLOSED 2026-07-10
| ID | Decision | Ruling |
|---|---|---|
| **D5** | "AP tests" meaning | **Audio-Processing pipeline, measured over serial.** Not WiFi. Battery is serial-primary; the firmware-v3 WiFi surface (§5, historical) plays no role. |
| **D1** | Mic-vs-unit isolation | **(c) — the two K1 units as configured** (bench = IM73D122, main = SPH0645). No mic swap, no second unit. Consequence (binding): every result is framed **Unit-Bench vs Unit-Main**; no claim about IM73D122-vs-SPH0645 *as models* may be made from this data. Battery scope = **lean core** (§12). |
| **D2** | Acoustic playback | **APPROVED** — hybrid-beat-tracker corpus + 1 kHz calibration tone. The per-session literal constraint still applies: exact file, output path, SPL, duration, and stop-command stated before anything plays. |
| **D3** | Absolute calibration | **SPL meter available.** P1 reports absolute dB SNR / noise-floor / sensitivity figures (R²≥0.98 gate on the sensitivity regression). |

Labels used: **GROUNDED** (traced to file:line, datasheet URL, or measurement), **VERIFIED** (independently re-confirmed by a second agent against the repos), **PROVISIONAL** (recon prose not independently re-run), **CONTRADICTED** (failed re-check), **CLOSED** (Captain ruling). §16 records exactly which claims moved and on what evidence. **Zero hardware actions have occurred in any pass to date.**

**Provenance honesty note (cumulative).** Corrections on the record: (a) v1.0 initially claimed "IM73D122 does not exist" — false, it is real bench hardware; (b) v1.0 assumed the mic firmware lived in firmware-v3 — false, separate repo (§3); (c) v1.0 claimed the hybrid-beat-tracker corpus was "present on disk" — did not survive independent re-check (§10, D4); (d) v1.0–v1.1 treated "AP" as WiFi Access Point and 125 Hz as the DUT hop rate — both wrong for the DUT ("AP" = Audio Processing; rate = 133.33 Hz; corrected lanes A2/B2, propagated throughout in v1.3).

---

## 1. EXECUTIVE SUMMARY

- The **bench K1** physically carries an **Infineon IM73D122** (PDM) microphone; the **main/production K1** carries a **Knowles SPH0645LM4H** (I2S). The SPH0645 was **physically removed** from the bench for the eval — an SPH0645 build on the bench reads silence. **[VERIFIED — `k1_bench_im73d` env comment states "SPH0645 removed" and pins the bench MAC `B4:3A:45:A5:89:B4`.]**
- Datasheets predict the IM73D122 is a materially better transducer: **+8 dB SNR (73 vs 65 dB-A), −6 dB self-noise floor, +8 dB dynamic range, +2 dB AOP, identical sensitivity (−26 dBFS @ 94 dB SPL)** (§9). The advantage concentrates in **quiet / far-field / low-level / weak-transient** content and the **noise floor** — exactly the regime where naive raw comparisons are most confounded.
- **There is currently NO measured IM73D-vs-SPH0645 A/B in any repo.** The only on-hardware tempo numbers (bpm=125, conf 0.91-0.93 lock=1 on Avicii-Levels) proved a firmware *migration* was byte-identical — they control for the mic, they do not quantify it.
- **Measurement channel = SERIAL (USB CDC)** — the DUT's only rich audio-telemetry channel (§5A, VERIFIED). Rich per-hop DSP telemetry is **probe-build-only**; a **composite build** (mic env + `ENABLE_TEMPO_STREAM` / AP-frontend capture) is a named build-scope item (§13.2). DUT frame rate = **133.33 Hz** (12800/96).
- **Confounds, both bounded:** (1) different firmware digital-gain maps **[VERIFIED at `i2s_audio.h:420-424`]** → raw-level metrics need the P0 common-SPL calibration (SPL meter available per D3); (2) N=1 unit per mic model → **unit-pair framing is mandatory** (D1 CLOSED). Downstream DSP is byte-identical between the two builds, so once gain is calibrated out, only transducer + front-end + board identity remain entangled — and D1 accepts that entanglement explicitly.
- **Committed battery (post-D1):** CORE = **P0 (calibration gate), P1 (front-end, absolute dB), P4 (tempo/beat headline), P7 (silence floor)**; P3 contingent on D4; P2/P5 optional extensions; P8 re-pointed at serial-transport integrity; P9 parity check. ~22 core metrics, paired, steady-state-only, BH-FDR (§11, §12).

---

## 2. WHAT STM IS (forensic — GROUNDED for firmware-v3; **RESOLVED ABSENT on the DUT**)

STM = **Spectro-Temporal Modulation** — a second-order descriptor of *how* the sound is changing, on two orthogonal axes, published for the renderer. Not "more spectrum": it sits on top of the FFT.

- **Temporal modulation `stmTemporal[16]`** — "how fast is each mel band pulsing?" 16 mel bands (50 Hz–7 kHz), 16-frame history (~128 ms), a Goertzel filter tuned to **4 Hz** (rhythmic-pulse rate). High = that band throbs at a musical rate.
- **Spectral modulation `stmSpectral[42]`** — "how ripply/rough is the spectral envelope?" A 128-band mel spectrum → FFT *across frequency* (cycles-per-octave) → bins 0-41. Low = smooth timbre; high = dense harmonic combing.
- **Energies** `stmTemporalEnergy` = mean(stmTemporal), `stmSpectralEnergy` = mean(stmSpectral). `stmReady` warms after 16 frames.

**Producer chain (firmware-v3, GROUNDED):** `AudioActor.cpp:739-803` builds a 512-pt rectangular-window rFFT → 256 peak-normalised bins → `STMExtractor::process()` (`src/audio/pipeline/STMExtractor.cpp`), smoothed by `ControlBus::applyStmSmoothing()` (`ControlBus.cpp:543-579`, asymmetric attack/release EMA). Constants: mel HTK `2595·log10(1+hz/700)`, `kTemporalTargetHz=4.0`, `TEMPORAL_FRAMES=16`, `MEL_BANDS=16`, `SPECTRAL_MEL_BANDS=128`, `SPECTRAL_BINS=42`, `FFT_SIZE=512`, zero heap, `sizeof(STMExtractor)≤8192`. Runs on AudioActor/Core 0, not render().

**RESOLUTION (VERIFIED, twice independently):** recursive greps of the entire `SpectraSynq_K1_Firmware/SPECTRASYNQ_K1_FIRMWARE/` tree for `stmReady`/`stm_ready`/`STMExtractor`/`spectro-temporal` returned **zero hits** in both the v1.1 review pass and the v1.2 orchestrator pass. The de-SB fork contains **no STM code at all**. P6 is DROPPED (§12); this section is a firmware-v3 record only; the §15 risk row is CLOSED as confirmed-absent.

---

## 3. DEVICE & REPO REALITY (D0 — CLOSED: repo + on-device both VERIFIED on live hardware 2026-07-10)

**VERIFIED (lane C reads + independent re-reads).** The IM73D122 mic code and the `k1_bench_im73d` / `k1_hardware` build envs **do not exist in firmware-v3**:
- `firmware-v3/src/config/audio_config.h` — `enum class MicType : uint8_t { SPH0645, IM69D130 }` (no IM73D122); `constexpr MicType MICROPHONE_TYPE = MicType::SPH0645;` **[re-read verbatim]**
- `firmware-v3/platformio.ini` — no `k1_hardware` / `k1_bench_im73d` env (env list re-enumerated: `esp32dev_audio_*`, `native_test_*` families only).
- `grep IM73` in `firmware-v3/src` → **zero hits** (prose hits only under `docs/research/`).

**The DUT firmware lives in the SEPARATE repo `/Users/spectrasynq/SpectraSynq_K1_Firmware`** — a de-SB'd (Sensory-Bridge→K1 renamed) fork. **[VERIFIED: HEAD `1f270967609ddf312e4fd7df28acf48a20530b4d` on branch `lane/dual-sync-phase0`; further branch `lane/im73d-pdm-eval` (`628f69b`) exists.]** The fork runs **12.8 kHz / hop 96 → 133.33 Hz frame rate** (**VERIFIED:** `DEFAULT_SAMPLE_RATE 12800` at `config_types.h:37`, `DEFAULT_SAMPLES_PER_CHUNK 96` at `:41`), not firmware-v3's 32 kHz / 256 / 125 Hz.

**Consequence:** original lanes A/B audited **firmware-v3** and describe *its* surface (retained as §5/§6 historical records); corrected lanes A2/B2 (§5A/§6A) describe the **real** DUT surface. Repo evidence (MAC-pinned envs, version constants) strongly indicates the K1s run `SpectraSynq_K1_Firmware`, but **only the serial banner read closes D0** — resolver in §13.1, now trivially cheap (`V` command → `FIRMWARE_VERSION 40103`).

### Device inventory (GROUNDED from research docs + env comments; re-confirm at D0)
| Role | Chip ID | MAC | Mic | Mic pins | LED pin | Env |
|---|---|---|---|---|---|---|
| **Bench** | B489A500 | `b4:3a:45:a5:89:b4` | **IM73D122 (PDM)** | CLK 13 / DIN 12 / LR 14 | GPIO 4 | `k1_bench_im73d` |
| **Main** | (…`87:f8`) | `b4:3a:45:a5:87:f8` | **SPH0645 (I2S)** | STD I2S (pinmap-dependent) | GPIO 6 | `k1_hardware` |

Notes: bench mic wiring is **pinmap-independent** (PDM pins identical in both pinmap blocks); the `SB_K1_BENCH_REFERENCE_PINMAP` flag's only live effect between the two A/B builds is the **LED pin (4 vs 6)** — audio-neutral. Handover "io4/io5" = bench LED pins, not the mic. Radio-free `k1_bench_im73d` only for measurement — never the `_ble` or `_dsr16` variants (DSR-16 is a *third* variable, +2 dB SNR).

**`k1_prod_im73d` (VERIFIED, added 2026-07-06):** main/prod GPIO map + `K1_MIC_IM73D_PDM_V1`, guard-mapped to the main MAC. Was the firmware half of a 2×2 mic-swap; **not used in this battery per D1 (no swap)** — retained on record for any future model-level campaign.

---

## 4. RECON LANES — provenance

Six read-only SSAs (parallel, 2026-07-10) + two corrected lanes (A2/B2) + two independent verification passes. Evidence paths in §17. Consumption status per lane in §16.

| Lane | Agent | Question | Status after v1.3 |
|---|---|---|---|
| A | network-api-engineer | Audio telemetry over the (WiFi) AP? | historical — audited firmware-v3; **superseded by A2** |
| A2 | corrected recon | The DUT's real telemetry channel | **VERIFIED** (orchestrator grep + independent re-check) |
| B | embedded-system-engineer | Scoreable consumers of the mic signal | historical (firmware-v3); **superseded by B2**; F6/STM resolved ABSENT |
| B2 | corrected recon | The DUT's real scoreable fields | received; key items VERIFIED, field-level detail PROVISIONAL until D0 frame dump |
| C | deep-technical-analyst | Is the mic the only variable? | **VERIFIED** |
| D | search-specialist | Datasheet head-to-head | PROVISIONAL (sourced; as-mounted ≠ datasheet) |
| E | test-automator | Corpora + tooling + approval | **PARTIALLY CONTRADICTED** (primary corpus unlocated — D4); harmonixset VERIFIED present |
| F | python-pro | Metrology + stats methodology | PROVISIONAL (methodology; no data yet) |

---

## 5. LANE A — firmware-v3 WiFi TELEMETRY SURFACE (**HISTORICAL — DOES NOT EXIST ON THE DUT**)

> **SUPERSEDED (v1.2/v1.3).** Everything below describes **firmware-v3 (LightwaveOS)**. The DUT fork's WiFi surface is a coarse on-request `k1.state` (mode/palette/tempo.bpm+locked) gated `SB_K1_WIRELESS_ENABLED`, probe-only, **absent from the shipping `k1_hardware` build**. None of these endpoints, streams, or rates exist on the device under test. **WiFi-based mic testing is not viable on the DUT and is out of scope per D5.** Retained solely as the firmware-v3 record. The DUT's real channel is §5A.

REST polls (client-paced, ≤20 Hz cap — `RateLimiter.h:53-59`): `/api/v1/audio/fft` (`AudioHandlers.cpp:1686` — rms, flux, bands[8], chroma[12], bins64[64], bins64Adaptive[64]) · `/api/v1/audio/stm` (`:1733`) · `/api/v1/audio/state` (`:469`) · `/api/v1/audio/tempo` (`:517`). WS/UDP: "AUD" binary 464 B @ **15 FPS** (`AudioStreamConfig.h:64`; "30 FPS" comment stale), max 4 subscribers, UDP variant heap-gated; STM binary 250 B @ 30 FPS; `beat.event` JSON ≤20 Hz (`WebServerBroadcast.cpp:687`). Fidelity ceiling: 15/30 FPS vs the 125 Hz firmware-v3 hop = 8×/4× undersampling; backpressure **drops** frames; `m_lowHeapShed` kill-switch silently disables all audio broadcast; FFT WS stream is a no-op stub.

## 5A. THE DUT MEASUREMENT CHANNEL — SERIAL (corrected lane A2, **VERIFIED**)

- **"AP" = Audio Processing on this codebase, not WiFi Access Point** (`APCadence`, `declared_ap_hz`, `ap_frontend`; pervasive — orchestrator grep 145 files, independent re-grep 219 files, difference is search-scope only). The fork has a dedicated **`serial/k1_ap_capture_telemetry.{h,cpp}`** subsystem **[VERIFIED: files exist]**. Per **D5 (CLOSED)**: "AP tests" = Audio-Processing over serial.
- **Serial (USB CDC) is the only rich audio channel on the DUT.** All battery capture is serial.
- **Rich per-hop DSP telemetry is PROBE-BUILD-only:**
  - `k1_tempo_probe` (**VERIFIED `platformio.ini:490`**, `-DENABLE_TEMPO_STREAM=1`): `TEMPO,bpm/phase/conf/beat/lock/str` @ ~20 Hz + `TEMPO_DBG`.
  - `k1_ap_frontend_probe` (**VERIFIED `platformio.ini:544`**, also `ENABLE_TEMPO_STREAM=1`): `apcad`/`apsoak`/`nov` — per-hop `APCadenceCaptureSample` (winner_bpm, v2_conf, novelty, i2s bytes, elapsed_us).
  - Production `k1_hardware` serial is coarse: `dump`, `smart_status` (SC_SAFE — novelty/spectral_energy/onset/bass_onset/beat_confidence/kick/snare/hihat + levels), `vp_status`, `fps`.
- **BUILD-SCOPE item (named):** the mic-A/B envs (`k1_bench_im73d`/`k1_hardware`) and the telemetry envs are **different builds**. The battery needs a **composite build per unit** — mic env + `ENABLE_TEMPO_STREAM` (and AP-frontend capture where P-phase requires per-hop data). Feasible (bench envs extend the harness chain) — build + verify in §13.2, with a P9 parity check that telemetry flags don't perturb DSP timing.
- **Frame rate = 133.33 Hz** (12800/96 = `declared_ap_hz`). All battery rate/latency arithmetic uses 133.33 Hz, not firmware-v3's 125.
- **D0 fingerprint (VERIFIED):** boot banner `RUNTIME_TIMING_GUARD: … sample_rate=12800 samples_per_chunk=96 declared_ap_hz=133.333 …` (emitted from the `.ino`); `V` serial command → **`FIRMWARE_VERSION 40103`** (`SPECTRASYNQ_K1_FIRMWARE.ino:3` — "Try V on the Serial port for this!").
- **`lwos_benchmark` is unusable for mic scoring** (device-perf WS A/B + t-test only; no wav/ground-truth/F1 scorer; targets an absent firmware-v3 WS endpoint). The live scorer (§13.2) must consume serial `APCadence`/`TEMPO` frames.

**Battery consequence:** single-channel serial capture rig; the old "AP + serial dual-channel" framing is retired. Transport integrity of the serial telemetry itself is tested in P8.

---

## 6. LANE B — MIC-SIGNAL CONSUMER MAP (**HISTORICAL — firmware-v3**; DUT fields in §6A)

> **SUPERSEDED for scoring purposes.** The ~90-field / 8-family map below (`ControlBus`, `es_*`, `bins64`, MusicalGrid, HF semantics, etc.) belongs to firmware-v3 and **must not be used to score the DUT**. Retained as the firmware-v3 record and for the two traps at the end, which remain fully applicable.

F1 front-end/level (12: `rms`, `rmsUngated`, `es_vu_level_raw`, …) · F2 spectrum (8: `bands[8]`, `bins64[64]`, `bins256`, …) · F3 pitch/harmony (~14: `chroma[12]`, `chordState`, …) · F4 onset/percussion (12: `onsetFlux/Env/Event`, `kickTrigger`, `snareEnergy`, `hihatEnergy` 6-12 kHz, …) · F5 tempo/beat (~20: `es_bpm`, MusicalGrid `bpm_smoothed`/`beat_phase01`, …) · F6 STM (ABSENT-ON-DUT, §2) · F7 saliency/derived (~9) · F8 HF semantics (7, `#if FEATURE_AUDIO_HF_SEMANTICS`).

## 6A. THE DUT SCOREABLE-FIELD LIST (corrected lane B2 — gate each field at the D0 frame dump)

- **Level/VU:** VU / `peak_scaled` / `vu_level` / silence state (`i2s_audio.h`) — plus the raw-twin telemetry guardrail before gain/sensitivity/DC (`i2s_audio.h:361`) for mic-intrinsic P1 metrics.
- **Spectrum:** GDFT spectrogram **[80 notes, A1=55 Hz → ~13.3 kHz]** (`k1_gdft_core.cpp`) — the DUT's frequency-response surface (replaces firmware-v3 `bins64`).
- **Pitch/harmony:** `chroma_strength` + `chroma_pc[12]` (`sb_audio_snapshot.cpp`); `SBChordState` root/type/confidence (`sb_chord_detect.cpp`).
- **Tempo/beat:** `SBTempoEvent` bpm/phase01/confidence/locked/beat_tick (`sb_tempo.cpp`); `TEMPO` stream fields (§5A).
- **Onset/percussion:** `SBOnsetBeatEvent` onset/bass_onset/beat/transient/kick/snare/hihat + strength/level (`sb_onset_beat.cpp`).
- **Saliency/semantics:** `SBSaliencyAxisFrame` harmonic/rhythmic/timbral/dynamic-novelty (`sb_musical_saliency.cpp`); `SBMusicState` enum.

**Top-4 mic-A/B metrics (re-ranked for the DUT):** (1) front-end SNR/floor from raw-twin VU under P0 calibration; (2) GDFT[80] log-spectral distance + top-note-band energy (hihat/air analogue — note SPH0645's higher 15 kHz cutoff vs IM73D 10 kHz, §9); (3) tempo = `SBTempoEvent`/`TEMPO`-stream bpm error + phase error vs ground truth; (4) onset P/R/F + latency from `SBOnsetBeatEvent`.

**Two traps (GROUNDED, unchanged and binding):** (a) **populated-vs-struct-present** — dump a live frame under signal AND silence per unit (D0 step 3) before locking any field into the battery; scoring a zero field = comparing noise. (b) **post-AGC contamination** — SSL/AGC/follower normalise away exactly the sensitivity/floor differences the A/B wants; use pre-gain raw twins for mic-intrinsic metrics, post-chain fields only for ratio/tracking metrics.

---

## 7. LANE C — CONFOUND / BUILD-PARITY AUDIT (**VERIFIED**, the REAL repo)

*(All claims independently re-read against `SpectraSynq_K1_Firmware` and held.)*

**`k1_bench_im73d` = `k1_hardware` + exactly TWO build flags** (lineage confirmed: `k1_bench_im73d` → `k1_bench_reference` → `k1_hardware`):
1. `K1_MIC_IM73D_PDM_V1` — the mic front-end (the intended variable). **Audio-relevant.** Env comment: "the ONLY build that defines K1_MIC_IM73D_PDM_V1, so k1_hardware and k1_bench_reference are byte-identical to before." (`k1_prod_im73d` now also defines it — unused per D1.)
2. `SB_K1_BENCH_REFERENCE_PINMAP=1` — swaps LED pin (4 vs 6) only. **Audio-neutral.**

**Shared identically (both builds):** sample rate 12.8k, hop 96, `response_gain=1.0`, sensitivity, DC-offset cal, SSL/AGC, follower, VU, GDFT, tempo (`sb_tempo`), onset (`sb_onset_beat`), chord (`sb_chord_detect`), semantic state, full `SB_*_V2` flag set. **No per-env DSP divergence.** Same `build_src_filter`.

**Mic ingestion divergence — single file `audio/i2s_audio.h`, gated `#ifdef K1_MIC_IM73D_PDM_V1`:**
- Driver (`:220-291`): PDM RX (16-bit, DSR_8S, clk 819.2 kHz, slot LEFT) vs SPH0645 STD I2S (32-bit, School-A slot).
- Read (`:308-312`): `int16` 192 B vs `int32` 384 B.
- **Pre-conditioning transform — THE confound core [VERIFIED at `:420-424`]:**
  - SPH0645 (`:424`): `sample = (raw_i32 * 0.000512) + 56000 − 5120; sample >>= 2;` → effective **affine `raw*0.000128 + 12720`** (baked DC pedestal).
  - IM73D122 (`:422`): `sample = im73d_samples_i16 * K1_MIC_IM73D_INPUT_GAIN`, **`INPUT_GAIN = 16.0f`** (**VERIFIED `system/constants.h:70`**) → pure linear, no pedestal, hand-characterised.
  - **Unrelated functions of raw counts** — G=16 was tuned to sit in the working domain, NOT matched to the SPH0645 output level.
- PDM-only NaN/cold-boot follower guards (`:608-623`).

**Gain-confound verdict (VERIFIED):**
- **INVALID without P0 calibration:** raw `im73d_raw_i16_rms` vs SPH0645 raw counts, absolute `max_waveform_val_raw`, VU floor magnitude, pre-convergence transients.
- **VALID (self-normalised at steady state):** tempo (bpm/conf/lock), onset, chord, AGC/SSL/follower-ratio metrics — the chain removes the DC pedestal (per-boot DC cal) and normalises level (SSL + per-band AGC).

**Control (now resourced per D3):** P0 common-SPL calibration tone with the SPL meter before any raw/absolute comparison; report raw AND gain-corrected deltas; one acoustic variable per capture at matched SPL/position.

---

## 8. THE INTEGRATED CONFOUND PICTURE — **SCOPE NOW DECIDED**

Two independent confounds stack:

1. **Digital-gain confound (VERIFIED):** different pre-conditioning maps → raw-level metrics are gain artefacts. **Fixed by P0** (common-SPL tone, SPL meter available per D3); report raw + gain-corrected.
2. **Unit-vs-model ceiling — ACCEPTED per D1 (CLOSED).** Bench and main are two specific boards, N=1 per mic model; unit identity and mic model are perfectly aliased. Captain's ruling: test the two units as configured — no swap, no second unit. **Binding consequence:** all results are framed **"Unit-Bench(IM73D122) vs Unit-Main(SPH0645)"**. Any statement about the mic *models* from this data is prohibited (RBDO). The rejected escape routes — (a) 2×2 swap (`k1_prod_im73d` firmware half exists, on record for a future campaign) and (b) second same-model unit — are documented, not scheduled.

Since §7 shows the downstream DSP is byte-identical, once gain is calibrated out the *pipeline* is not a confound — the transducer + front-end + board identity remain entangled, and that entanglement is now an accepted, disclosed property of the study design rather than an open decision.

**Committed scope (follows from D1):** **lean core P0 → P1 → P4 → P7**; P3 contingent on D4; P2/P5 optional; P8/P9 infrastructure checks (§12).

---

## 9. LANE D — MIC DATASHEET ENVELOPE (GROUNDED, sourced) — the hypothesis the battery tests

| Spec | IM73D122 | SPH0645LM4H | Delta | Interpretation |
|---|---|---|---|---|
| SNR (dB-A) | **73** | 65 | **+8** | IM73D 6.3× quieter |
| Noise floor (dB SPL-A) | **31** | 37 | **−6** | IM73D floor 6 dB lower |
| AOP (dB SPL) | **122** | 120 | +2 | marginal headroom edge |
| Dynamic range (dB) | **91** | 83 | **+8** | 8 dB more usable headroom |
| Sensitivity (dBFS@94dB SPL) | −26 | −26 | **0** | identical output level |
| LF roll-off (Hz) | 20 | 45 | −25 | IM73D deeper low-end |
| Upper −3 dB (Hz) | 10 000 | **15 000** | +5 000 | **SPH0645 extends higher (treble)** |
| Interface | PDM | I2S 24-bit | — | IM73D needs decimation |
| Active current (µA) | 980 | **600** | −380 | SPH0645 lower power |

**Sources:** Infineon IM73D122 datasheet v01_00; Knowles SPH0645LM4H-B (DigiKey/Adafruit). THD & PSRR for IM73D122 **UNVERIFIED** (not in accessible datasheet text).

**Predicted downstream (falsifiable):** IM73D122 tempo-lock floor ~3-4 dB SPL lower; onset envelopes visible −3 to −6 dB below steady-state (vs +4-5 dB for SPH0645); chroma/band-energy cleaner at quiet levels. **Counter-note:** SPH0645's higher upper cutoff (15 vs 10 kHz) means the top-octave/GDFT-high-note result is **not a foregone IM73D win**. Per D1, all findings attach to the *units*, not the models.

---

## 10. LANE E — CORPORA + TOOLING + PLAYBACK (**D2 CLOSED: APPROVED**; D4 open)

### Corpora
- **`hybrid-beat-tracker/tests/benchmark` — ✅ LOCATED + VERIFIED (D4 CLOSED 2026-07-10, orchestrator filesystem re-run).** Actual contents (manifest-driven verification): **21 tracks, 21/21 complete `.wav`+`.beats` pairs, 2091 ground-truth beats, tempo 60.0–206.9 BPM, 18 hard / 3 medium, zero missing, zero empty.** (v1.0–v1.3 said "20 tracks / not found" — the not-found was a mounted-root limitation of the remote pass; the local filesystem check found it.) **Directory layout (scorer-critical):** `<root>/clips/*.wav` + `<root>/annotations/*.beats`; `manifest.json` (`version`/`source`/`tracks[]`, each track = `id`,`clip_file`,`annotation_file`,`clip_start_sec`,`clip_duration_sec`,`difficulty`,`characteristics[]`,`madmom_beats`,`madmom_tempo`) uses **bare filenames** → the scorer resolves `clips/<clip_file>` and `annotations/<annotation_file>`, NOT manifest-root-relative. `.beats` = one beat time (seconds) per line. It is the only candidate with real beat ground truth AND standing source-approval, and **D2 playback approval covers it** (per-session file/SPL/duration/stop-cmd still stated before play).
- **`firmware-v3/test/music_corpus/harmonixset/esv11_benchmark` — VERIFIED present** (`audio_12k8/`, `audio_32k/`, `manifest.tsv`, `summary.json`): 36 tracks, 6 BPM buckets (60-220), BPM point-estimate + difficulty only — **no beat timestamps**. Fallback if D4 fails: P4 downgrades to BPM-accuracy-only, P3 dies. Playback of harmonixset is NOT covered by D2 — separate approval needed if ever used acoustically.

### Tooling (firmware-v3/tools/ — none of it scores a live mic)
`serial_capture.py`/`_inject.py` (raw dump / command injection, no scoring) · `capture_trace.py` (MabuTrace perf, not audio) · `analyse_harmonixset_metrics.py`, `build_esv11_benchmark_pack.py` (offline corpus) · `test_esv11_music_corpus` (native regression gate, WAV-in, not live) · **`lwos_benchmark` — RESOLVED (v1.2): unusable for mic scoring** (device-perf WS A/B + t-test; no ground-truth scorer; targets an absent firmware-v3 endpoint).

**Build gap (confirmed):** no live-mic-vs-ground-truth scorer exists anywhere. §13.2 builds one that consumes **serial `TEMPO`/`APCadence` frames** and scores against `.beats` ground truth. Gated on D4.

### Playback approval status
- hybrid-beat-tracker corpus + 1 kHz calibration tone: **APPROVED (D2 CLOSED, 2026-07-10)** — with the standing literal constraint: each session states exact file, output path, SPL, duration, stop-command before playing.
- harmonixset: **unapproved** for acoustic playback.
- Usage-rights metadata (YouTube-sourced) unconfirmed for both — separate legal flag, unchanged.

---

## 11. LANE F — DEFINITIVE METHODOLOGY (GROUNDED; "definitive" now means unit-pair-definitive per D1)

**Three simultaneous conditions:** (1) paired-on-stimulus; (2) confound bounding — gain via P0, unit-aliasing via the D1 framing rule; (3) every tracking metric excludes its own convergence transient (prior K1 finding: headline **3.82× collapsed to 2.88×** once the ~8-10 s tempo convergence window was excluded — mandatory clause).

### Metric formulae (~22 in the committed core; ~28 with optional phases)
- **Audio metrology (8, P1/P0 — absolute dB permitted per D3):** SNR = `10·log10((P_sig−P_noise)/P_noise)`; noise floor (dBFS AND dB SPL via meter, in controlled silence); sensitivity = regression slope dBFS-vs-SPL (R²≥0.98 gate); dynamic range = `dBFS_clip_onset − dBFS_floor`; THD = `sqrt(Σ V_k²)/V_1`; freq-response flatness over GDFT[80] = `stdev(Δ_k dB)` + ripple; spectral similarity (coherence only if simultaneous dual capture is ever rigged — not planned); inter-trial CV.
- **MIR beat/tempo (12, mir_eval / Raffel 2014, P4):** BPM Accuracy-1 (±4%), Accuracy-2 (octave-tolerant), F-measure (±70 ms), CMLc/CMLt, AMLc/AMLt, P-score, information gain, time-to-lock, % frames locked (post-convergence), confidence distribution, octave-error rate, drift. *(F-measure family requires `.beats` — gated on D4; BPM family works on harmonixset fallback.)*
- **Onset (5, P3 — contingent on D4):** P/R/F (±50 ms), median + P95 latency, silence false-positive rate.
- **Chroma/chord (1-2, optional P5):** cosine similarity to pitch-class template / `mir_eval.chord`.
- **Silence detection (2, P7):** false-trigger rate, transition latency.

### Statistics
Paired (stimulus × repeat), interleaved A-B-A-B (never blocked); **N≥20** per stimulus/metric (**≥30** timing); mandatory steady-state exclusion per-trial via measured time-to-lock; report mean AND median + bootstrap 95% CI (≥2000 resamples); Cohen's **d_z** / rank-biserial; **Wilcoxon signed-rank**; **BH-FDR** q=0.05 across all reported metrics. **Decision gate:** a metric counts as a real unit-pair difference only if (a) BH-significant AND (b) |d_z|>0.5 (or rank-biserial equivalent). *(The §8 unit-variation null clause is void — D1 rejected the second unit; its absence is exactly why claims stay at unit-pair level.)*

---

## 12. THE TEST BATTERY — committed spec (serial-channel, 133.33 Hz, unit-pair-framed)

**Channel: SERIAL for everything** (§5A). Composite telemetry builds per §13.2. All metrics paired, steady-state, BH-FDR. **CORE = P0/P1/P4/P7** (runs regardless) · **CONTINGENT = P3** (D4) · **OPTIONAL = P2/P5** (run if capture time allows) · **INFRA = P8/P9**.

| Phase | Tier | Purpose | Stimulus | Key metrics | Note |
|---|---|---|---|---|---|
| **P0** | CORE (GATE) | Confound calibration | 1 kHz tone @ SPL-meter-verified level + controlled silence | per-unit gain offset (maps the §7 divergence to a measured constant) | runs FIRST; bounds everything |
| **P1** | CORE | Electro-acoustic front-end | pink noise + stepped-sine sweep + silence | SNR, noise floor (dBFS + dB SPL), sensitivity slope, dynamic range, GDFT flatness, THD | absolute dB per D3; raw-twin fields only |
| **P2** | OPTIONAL | Spectral integrity | tone sweep / pink | GDFT[80] log-spectral distance, per-note variance, spectral similarity | |
| **P3** | CONTINGENT | Onset | annotated percussion tracks | P/R/F (±50 ms), latency, silence FP rate | **gated on D4**; top-octave note: SPH0645 cutoff higher |
| **P4** | CORE (headline) | Tempo/beat | hybrid-beat-tracker `.beats` corpus | BPM acc-1/2, beat F, CMLt/AMLt, P-score, info-gain, time-to-lock, %locked, conf dist, octave-error, drift | AGC-normalised → front-end-attributable; **beat-F gated on D4** (harmonixset fallback = BPM-only) |
| **P5** | OPTIONAL | Pitch/harmony | known-key/known-chord material | chroma cosine-sim, chord accuracy/confidence | |
| ~~P6~~ | DROPPED | STM | — | — | STM absent from DUT (§2, VERIFIED twice) |
| **P7** | CORE | Silence detection | true-silence + low-level | false-trigger rate, transition latency | mic floor drives this — the +8 dB SNR envelope's natural stage |
| **P8** | INFRA | **Serial transport integrity** (re-pointed v1.3) | any active telemetry under load | sustained frame rate vs 133.33 Hz / ~20 Hz TEMPO, gap/drop count, timestamp jitter, buffer-overrun events | the channel is a DUT too; replaces the retired WiFi-transport phase |
| **P9** | INFRA | System/timing parity | any effect under signal | per-hop DSP µs, frame-rate stability | must be EQUAL both builds — **also validates the composite telemetry build didn't perturb timing** |

**Deliverable:** per-metric Unit-Bench−Unit-Main delta table with raw + gain-corrected values, CI, effect size, BH-adjusted p — framed as unit-pair per D1, with the datasheet envelope (§9) as interpretive context only.

---

## 13. ONWARD EXECUTION PLAN

### 13.0 D4 corpus location (LAST non-hardware gate; needs local-filesystem access)
1. Check `/Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark` (outside current session roots — add the folder to a session, or check in a local terminal: `ls …/tests/benchmark/*.beats | wc -l`).
2. If present: enumerate the 20 `.wav`/`.beats` pairs, confirm madmom timestamp format, record the absolute path here. D4 closes; P3 + full P4 unlock.
3. If absent: escalate — restore from backup, re-generate annotations (madmom re-run), or accept harmonixset fallback (P3 dies; P4 = BPM-only).

### 13.1 D0 hardware-fingerprint (30 seconds per unit; needs hardware-authorised session)
1. Enumerate serial ports; MAC-verify each K1 (`b4:3a:45:a5:89:b4` bench / `…87:f8` main) **before any write** — verify-MAC-before-flash hard rule.
2. Read the boot banner (`RUNTIME_TIMING_GUARD: … sample_rate=12800 samples_per_chunk=96 declared_ap_hz=133.333 …`) or send `V` → expect `FIRMWARE_VERSION 40103`. Confirms repo + build per unit.
3. Dump one live telemetry frame under signal + silence per unit (via `smart_status` on prod builds): confirm which §6A fields are non-zero (closes the populated-vs-struct trap).
4. Record findings here. D0 closes.

### 13.2 Execution swarm (post-D0/D4; NOT read-only — sandboxing + hardware auth)
- **Build lane 1 — composite telemetry envs:** `k1_bench_im73d` + `ENABLE_TEMPO_STREAM`(+AP-frontend capture) for the bench; `k1_hardware` + same for the main. Byte-parity of the DSP path verified in P9. Flash MAC-guarded, orchestrator only.
- **Build lane 2 — live scorer:** serial front-end consuming `TEMPO`/`APCadence`/`smart_status` frames, timestamping device output against `.beats` ground truth; mir_eval + metrology pipeline (§11); durable/re-runnable harness. Parallel-agent-sandboxing rule applies.
- **Capture lane:** serial per-hop rig; P0 first (SPL-metered 1 kHz tone); then core phases interleaved A-B-A-B.
- **Orchestrator keeps:** the commit, the flash, and personally re-runs every decision-critical metric before it becomes fact (SSA flagship rule).

### 13.3 Sequencing
`D4 corpus check ∥ D0 fingerprint → composite builds + live scorer → P0 (gate) → P1/P4/P7 core (+P3 if D4; P2/P5 optional) → P8/P9 infra logs throughout → score → synthesis, unit-pair-framed per D1.`

---

## 14. DECISIONS

| ID | Decision | Status | Ruling / Next action |
|---|---|---|---|
| **D0** | Which firmware is on each K1? | **CLOSED 2026-07-10** | Executed on LIVE hardware (read-only serial) — RUNTIME_TIMING_GUARD banner captured from BOTH units (12800/hop96/declared_ap_hz=133.333, core_ok=1); identity triple-confirmed (USB MAC + SB-fork telemetry format + DC-signature −2 IM73D / −4545 SPH0645). Both run `SpectraSynq_K1_Firmware` (de-SB fork). |
| **D1** | Mic-vs-unit isolation | **CLOSED 2026-07-10** | **(c) two units as configured** — no swap, no second unit. Unit-pair framing mandatory on all outputs. Lean-core scope. |
| **D2** | Playback approval | **CLOSED 2026-07-10** | **APPROVED** — hybrid-beat-tracker corpus + 1 kHz cal tone; per-session file/SPL/duration/stop-cmd still stated before play |
| **D3** | Absolute calibration | **CLOSED 2026-07-10** | **SPL meter available** — P1 reports absolute dB; R²≥0.98 sensitivity gate |
| **D4** | Locate the `.beats` corpus | **CLOSED 2026-07-10** | **VERIFIED present** — 21/21 wav+beats pairs at `hybrid-beat-tracker/tests/benchmark` (`clips/` + `annotations/`, 2091 beats). Harmonixset fallback no longer needed. |
| **D5** | "AP tests" meaning | **CLOSED 2026-07-10** | **Audio-Processing / serial.** WiFi out of scope; §5 historical only |

---

## 15. RISK REGISTER

| Risk | Severity | Status / Mitigation |
|---|---|---|
| Primary beat corpus missing → P3 dead, P4 downgraded | CLOSED | D4 CLOSED 2026-07-10 — corpus VERIFIED (21/21 pairs, 2091 beats); scorer must use `clips/`+`annotations/` bare-filename layout |
| Scored a struct-only/zero field → noise as signal | HIGH | §13.1 step 3 live-frame dump per unit before battery lock |
| Raw-level metrics attributed to mic front-end, actually digital gain | HIGH | P0 gain-cal (SPL meter per D3); report raw + corrected (confound VERIFIED §7) |
| Results overstated as mic-model claims | HIGH (framing) | D1 CLOSED: unit-pair framing is mandatory boilerplate on every output table |
| Wrong firmware assumed (repo mix-up) | MED | repo half VERIFIED; residual closed by the 30-sec D0 banner read |
| **Composite telemetry build perturbs DSP timing** | MED (NEW v1.3) | P9 parity check between telemetry-on and shipping builds; `ENABLE_TEMPO_STREAM` already ships in probe envs |
| **Serial capture overrun/backpressure at per-hop rate** | MED (NEW v1.3, replaces WiFi heap-shed) | P8 logs sustained rate/gaps/jitter; capture rig sized for 133.33 Hz × frame size |
| Convergence-transient inflation | MED | mandatory steady-state exclusion (§11) |
| Corpus licensing (YouTube-sourced) | LOW-MED (legal) | separate from the D2 gate; flag before external use |
| ~~STM absent on device firmware~~ | CLOSED | confirmed absent (two independent greps); P6 dropped |
| ~~WiFi heap-shed truncates capture~~ | RETIRED | WiFi channel out of scope per D5; superseded by the serial-capture risk above |

---

## 16. DELEGATION LEDGER (SSA-management §10)

| ID | Lane | Claim | Class | Evidence | CONSUMED AS (after v1.3) |
|---|---|---|---|---|---|
| A | WiFi-AP telemetry (firmware-v3) | decimated 15/30 FPS, heap-shed | historical | SSA-A_ap_telemetry.md | **superseded by A2** — surface does not exist on DUT |
| A2 | DUT telemetry channel | "AP"=Audio Processing; serial-only; probe-build telemetry; 133.33 Hz; fingerprint string | decision-critical | SSA-A2_device_repo_telemetry.md | **VERIFIED** — orchestrator grep + independent re-check (files exist; envs at `platformio.ini:490/:544`; `12800/96` at `config_types.h:37/41`; `FIRMWARE_VERSION 40103` at `.ino:3`; AP-grep 145 vs 219 files = scope diff only) |
| B | consumer map (firmware-v3) | ~90 fields; two traps | historical | SSA-B_consumer_map.md | **superseded by B2** for scoring; the two traps carry over as binding |
| B2 | DUT scoreable fields | SB* structs / GDFT[80] / chroma_pc[12] | load-bearing | SSA-B2_device_repo_consumers.md | **provisional at field level** — gate each field at the D0 frame dump; channel-level items VERIFIED |
| C | confound/parity | mic-only variable + gain-map confound; DIFFERENT REPO | decision-critical | SSA-C_confound_parity.md | **VERIFIED** — HEAD `1f27096` exact; two-flag lineage; `INPUT_GAIN=16.0f` @ `constants.h:70`; gain maps @ `i2s_audio.h:420-424`; firmware-v3 absence exact |
| D | datasheets | +8 dB SNR envelope | load-bearing | SSA-D_mic_datasheets.md | **provisional** (sourced; as-mounted ≠ datasheet; unit-pair framing applies regardless) |
| E | corpus/tooling | corpus "present on disk"; no live scorer | load-bearing | SSA-E_corpus_tooling.md | **PARTIALLY CONTRADICTED** — corpus unlocated (D4); harmonixset VERIFIED; "no live scorer" + "lwos_benchmark unusable" confirmed |
| F | methodology | metrics; N=1 ceiling; steady-state | load-bearing | SSA-F_methodology.md | **provisional**; N=1 ceiling converted from open decision to accepted design property (D1) |

**Verification provenance:** v1.1 pass (independent review agent — lane C promotion, STM grep, corpus contradiction, `k1_prod_im73d`); v1.2 pass (orchestrator — lanes A2/B2, grep-verified); v1.3 (independent re-check of every v1.2 claim: all held; consolidation + Captain decisions folded in). **Zero hardware actions across all passes.**

---

## 17. APPENDICES

### 17.1 Evidence file paths
```
scratchpad/im73d_recon/SSA-A_ap_telemetry.md
scratchpad/im73d_recon/SSA-A2_device_repo_telemetry.md
scratchpad/im73d_recon/SSA-B_consumer_map.md
scratchpad/im73d_recon/SSA-B2_device_repo_consumers.md
scratchpad/im73d_recon/SSA-C_confound_parity.md
scratchpad/im73d_recon/SSA-D_mic_datasheets.md
scratchpad/im73d_recon/SSA-E_corpus_tooling.md
scratchpad/im73d_recon/SSA-F_methodology.md
```
(session scratchpad root: `/private/tmp/claude-501/-Users-…-Lightwave-Ledstrip/5e36156a-…/scratchpad/`)

Verification passes: read-only re-reads via mounted repos (Cowork remote sessions, 2026-07-10). Key commands: verbatim reads of `audio_config.h` / `platformio.ini` (both repos) / `constants.h` / `i2s_audio.h` / `config_types.h`; `grep IM73` (firmware-v3/src → 0); STM grep (DUT tree → 0, run independently twice); `.beats` find (all three roots → 0); harmonixset `ls` (present); git HEAD resolution; `k1_ap_capture_telemetry` file check; `FIRMWARE_VERSION 40103` at `.ino:3`; probe envs at `platformio.ini:490/:544`; `12800/96` at `config_types.h:37/41`.

### 17.2 Datasheet sources
- Infineon IM73D122 v01_00 — infineon.com/dgdl/Infineon-IM73D122-DataSheet-v01_00-EN.pdf
- Knowles SPH0645LM4H-B — media.digikey.com/pdf/Data Sheets/Knowles Acoustics PDFs/SPH0645LM4H-B.pdf

### 17.3 Key firmware anchors (all VERIFIED unless noted)
- **DUT fingerprint:** `SPECTRASYNQ_K1_FIRMWARE.ino:3` — `#define FIRMWARE_VERSION 40103` ("Try V on the Serial port for this!"); `RUNTIME_TIMING_GUARD` banner emitted from the `.ino`.
- **DUT rate constants:** `system/config_types.h:37` (`DEFAULT_SAMPLE_RATE 12800`), `:40-41` (`DEFAULT_SAMPLES_PER_CHUNK 96`) → 133.33 Hz.
- **Mic front-end:** `audio/i2s_audio.h:220-291, 308-312`; transform `:420-424`; raw-telemetry guardrail `:361`; `system/constants.h:70` (`K1_MIC_IM73D_INPUT_GAIN 16.0f`).
- **Build envs (`SpectraSynq_K1_Firmware/platformio.ini`):** `k1_hardware:18`, `k1_bench_reference:194`, `k1_bench_im73d:216`, `k1_bench_im73d_dsr16:226`, `k1_prod_im73d:240`, `k1_tempo_probe:490`, `k1_ap_frontend_probe:544`.
- **DUT telemetry:** `serial/k1_ap_capture_telemetry.{h,cpp}`; scoreable-field sources per §6A (`k1_gdft_core.cpp`, `sb_audio_snapshot.cpp`, `sb_chord_detect.cpp`, `sb_tempo.cpp`, `sb_onset_beat.cpp`, `sb_musical_saliency.cpp`).
- **firmware-v3 (historical):** STM producer `AudioActor.cpp:739-803`, `STMExtractor.cpp`, `ControlBus.cpp:543-579`; WiFi telemetry `AudioHandlers.cpp:469/517/1686/1733`, `AudioStreamConfig.h:64`.

### 17.4 Glossary
STM = spectro-temporal modulation · AOP = acoustic overload point · SSL = spectral silence level · AGC = automatic gain control · d_z = paired Cohen's d · BH-FDR = Benjamini-Hochberg false-discovery-rate · CMLt/AMLt = correct/allowed metrical level (total) · DUT = device under test · **AP = Audio Processing (this codebase; NOT WiFi Access Point)** · GDFT = Goertzel discrete Fourier transform bank.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-10 | research-agent (orchestrator + 6-lane SSA swarm) | Created — full consolidation: STM forensic, device/repo reality (D0), 6 recon lanes, integrated confound analysis, 10-phase battery spec, execution plan, decisions D0-D3. DEGRADED-MODE; no hardware runs. |
| 2026-07-10 | agent:claude (Cowork review pass) | v1.1 — independent verification against both repos. Lane C PROMOTED TO VERIFIED (HEAD 1f27096; two-flag lineage; INPUT_GAIN 16.0f; gain maps @ :420-424, anchor corrected). STM confirmed ABSENT on DUT → P6 DROPPED. hybrid-beat-tracker corpus NOT FOUND → NEW GATE D4. k1_prod_im73d surfaced. Lean-core recommendation added. |
| 2026-07-10 | research-agent (orchestrator, corrected lanes A2/B2) | v1.2 — corrected DEVICE TELEMETRY CHANNEL (§0 block): "AP" = Audio Processing (serial), not WiFi; firmware-v3 WiFi surface absent on DUT → decision D5; rich telemetry probe-build-only → composite-build scope; 133.33 Hz not 125; D0 fingerprint string (RUNTIME_TIMING_GUARD / FIRMWARE_VERSION 40103); corrected DUT field list; lwos_benchmark unusable. |
| 2026-07-10 | agent:claude (Cowork consolidation) | v1.3 — CONSOLIDATED + DECISIONS CLOSED. All v1.2 claims independently re-verified (all held; .ino:3, platformio :490/:544, config_types.h:37/41, k1_ap_capture_telemetry files; AP-grep 219 vs 145 = scope diff). v1.2 findings propagated through body: §5 marked historical + new §5A (DUT serial channel), §6 historical + new §6A (DUT field list), §12 rebuilt serial-only with tiers (CORE P0/P1/P4/P7 · CONTINGENT P3 · OPTIONAL P2/P5 · INFRA P8 re-pointed to serial transport + P9), §14/§16/§17 updated, version header fixed (v1.2 had left it at 1.1). CAPTAIN DECISIONS RECORDED: D5=Audio-Processing/serial; D1=(c) two units as configured (unit-pair framing mandatory, lean-core scope); D2=playback APPROVED (per-session specifics still required); D3=SPL meter available (absolute dB permitted). Unit-null clause voided per D1. New risks: composite-build timing perturbation (P9 gate), serial capture overrun (P8). Remaining gates: **D4 + D0 only.** |
| 2026-07-10 | research-agent (orchestrator, local-filesystem re-run) | v1.4 — **D4 CLOSED.** Orchestrator verified the hybrid-beat-tracker corpus on the LOCAL filesystem (the remote v1.1/v1.3 passes could not reach the path): manifest-driven check = **21/21 complete `.wav`+`.beats` pairs, 2091 ground-truth beats, tempo 60.0–206.9 BPM, 18 hard / 3 medium, zero missing/empty.** Corrected the v1.0 "20 tracks/not found" claim. Documented scorer-critical layout: `clips/*.wav` + `annotations/*.beats`, manifest bare filenames (resolve `clips/<clip_file>`+`annotations/<annotation_file>`), `.beats` = one beat-time-in-seconds per line. Updated §0 banner, §10 corpus, §14 (D4 row → CLOSED), §15 (corpus-missing risk → CLOSED), abstract (REMAINING GATE: D0 only). **Only D0 (hardware serial fingerprint) now blocks execution.** |
| 2026-07-10 | research-agent (orchestrator, live-hardware execution) | v1.5 — **D0 CLOSED on live hardware (read-only serial).** Both K1s found USB-connected; DTR-reset captured the RUNTIME_TIMING_GUARD banner verbatim from BOTH (sample_rate=12800/hop96/declared_ap_hz=133.333/core_ok=1). Identity triple-confirmed: USB MAC (`…89:B4`/`…87:F8`) + SB-fork telemetry format + **live DC-signature (bench −2 IM73D / main −4545 SPH0645) = on-hardware confirmation of the Lane C gain confound.** Both units live-stream `[AP]` `bpm/conf/lock/phase/beat/onset` at ~1.4 Hz (tempo-VALUE metrics live; beat-F still needs the ~20 Hz TEMPO probe build). **Scorer parser corrected** to the REAL keyed `[AP]`/`TEMPO,t=` formats (the prior positional-CSV parser was wrong — caught by reading the emit code + live capture); Gate-0 self-test re-proven **26/26 GREEN** on real formats and validated against captured hardware logs (16/17 real frames each, DC-signature extracted). Updated §0 banner + §3 header + §14 (D0 → CLOSED) + abstract. **ALL GATES CLOSED — only the physical acoustic capture (human-at-bench) remains; no terminal agent can perform it.** Evidence: `scratchpad/im73d_recon/live_telemetry_{bench_im73d,main_sph0645}.log`; `firmware-v3/tools/mic_ab_scorer/`. |
