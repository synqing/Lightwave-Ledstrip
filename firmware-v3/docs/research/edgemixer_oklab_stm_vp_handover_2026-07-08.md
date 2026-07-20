---
abstract: "Handover for the EdgeMixer superiority programme (2026-07-08 session 2): OKLab perceptual rotation + STM Core-0 producer + the objective VP measurement tool. The colour engine is ported and running; three new lanes are in flight. IMMEDIATE next action: run the offline CIELAB analyser on the captured device sweep to get the OKLab-vs-luma verdict. Governing standard: the K1 EdgeMixer must equal-or-exceed the Lightwave original AND hit frame rate — never a lite/perf-limited version. Contains: exact branch/commit/worktree/device state, the three-lane status, the objective-tool method + why (the BT.601-Y circularity that made the eyeball A/B inconclusive), the prioritised outstanding-work list, gotchas, and the idle-agent roster. Read before any further EdgeMixer/OKLab/STM/VP work. Continues edgemixer_k1_port_handover_2026-07-08.md."
---

# EdgeMixer — OKLab / STM / Objective-VP-Tool — Handover (2026-07-08, session 2)

## 0. Prime directive (read first)
You inherit **full project-owner/PM ownership** (Captain green-lit `/autonomous-agentic-build`): decide and execute, escalate only for genuinely irreversible/destructive/high-blast-radius actions.

**Governing standard (locked, recorded in memory `feedback_k1_edgemixer_no_lite_standard.md`):** the K1 EdgeMixer must be the highest visual quality **and** hit frame rate — equal to or exceeding the Lightwave original. **Never** a stripped/subset/"lite"/performance-limited version. Performance is a constraint to *engineer around* (optimise, use headroom), **not** a reason to downgrade the visual.

This session continues `firmware-v3/docs/research/edgemixer_k1_port_handover_2026-07-08.md` (the port + boot-loop handover). That work landed: the full Rodrigues+BT.601 colour engine is ported to K1 `sb_edgemixer_lite` at SQ15x16 precision (higher than the original Q8.8), running on the bench.

## 1. THE IMMEDIATE NEXT ACTION

> ## ✅ UPDATE 2026-07-09 ~22:40 — ORIGIN/MAIN DIVERGENCE RECONCILED (rebuild-forward) — supersedes all below
>
> **The 150-commit divergence was base-drift, not corruption.** origin/main (154f716) advanced via PRs #14–#22 (N2/N2b/N3/N4a hardening) while our 93-commit EdgeMixer+de-SB effort sat on a June-26 base. 4-SSA audit findings: (a) feat already re-implemented N2/N2b/N3 (production safety NOT regressed); (b) genuinely missing = N4a device-identity guard + build-config drift-gate + commit-gate fix (dev/CI only); (c) origin/main itself ships **2 pre-existing red tests** (test_acf_spread + test_upload_guard assert feat-forward-refs); (d) the de-SB rename is regenerable machinery.
>
> **Resolution executed (Captain: rebuild-forward, "do everything necessary"):** built on branch **`rebuild/desb-main`** (worktree `/private/tmp/k1_rebuild_main`, off origin/main):
> - **PR-0 `2142220`**: de-SB rename of origin/main via the HARDENED `migrate.py` (the assertNotIn legacy-guard corruption ssa-transform caught is now fixed in the canonical tool). Rename-first → both sides k1_ → no sb_/k1_ collision. 0-residual; 647 pass + origin's 2 pre-existing reds.
> - **Merge `a152efd`**: 3-way merge of feat into renamed-main. 24 conflicts resolved — took feat (validated superset) for source/features/tests, kept origin's N4a guard, dropped k1_edgemixer_lite→k1_edgemixer (no-lite), platformio=feat superset. Correct-tree because merge-base predates N4a (origin's dev-lanes kept as added-on-one-side).
> - **Fixups `0a518e5`**: populated the N4a manifest with the full 49-env set (clears origin's pre-existing upload-guard reds + registers feat's im73d/custom envs); pointed 3 feat tests at the manifest (N4a single-source) instead of the pre-N4a guard source; regen serial_struct golden. **689 pass → 3 remaining fixed → running final full gate + k1_hardware build.**
>
> **REMAINING:** final full-gate confirm (running) → hardware re-smoke on bench B489A500 (IM73D) → **land to origin/main** (Captain's GitHub — the outward-facing step; reconciled branch preserves origin's history as ancestor, so it lands clean). Tooling/evidence: `firmware-v3/docs/research/desb-migration-2026-07-09/`.
>
> ---
>
> ## ✅ UPDATE 2026-07-09 ~20:20 — FORK-WIDE de-SB MIGRATION COMPLETE + HOST-GATE-GREEN (superseded by the reconciliation above)
>
> **Item 1 is DONE.** The fork-wide Sensory-Bridge→K1 migration was re-run CLEAN from `0d83729` (the parked `wip/fork-desb-partial` was NOT resumed — a half-applied sed is harder to finish than one coherent pass) and is now complete + host-gate-green, durably committed as a checkpoint on **`wip/fork-desb-complete` @ `1dfc9b0`** (worktree `/private/tmp/k1_edgemixer_on_im73d`; object store durable in `~/SpectraSynq_K1_Firmware/.git`). `feat/edgemixer-on-im73d` stays clean at `0d83729`.
>
> **Scope (true, ground-truthed — NOT the ~2,500 estimate):** 4,800 SB refs / 168 files. 33 file renames (incl. double-k1 collapse). 165 files content-transformed, symmetric 4047 ins/del = pure rename, zero behaviour change.
>
> **Verified:** **0-residual** across every class (`sb_`, `SB_`, `SB[caps]`, `-DSB`/`-dsb` flags, `Sensory Bridge` prose) · **667 passed / 1 skipped** · **`pio run -e k1_hardware` SUCCESS** · DSP replay goldens (tempo/onset/chord/smart_director) **UNCHANGED** = beat-tracking byte-identical (the crown-jewel guarantee). Reproducible tooling + before/after evidence banked at `firmware-v3/docs/research/desb-migration-2026-07-09/` (`desb_verify.py` = the corrected acceptance test, `migrate.py`, `regen_golden.py`).
>
> **THREE latent defects caught by ground-truthing (each would have shipped past a green gate):** (1) the master-plan acceptance **grep is a zsh false-pass generator** — unquoted `--include=*.cpp` globs get filename-expanded → searched nothing → "0 residual" on a fully-SB tree; the canonical acceptance test is now `desb_verify.py` (Python `re`), NOT grep. (2) **`-DSB_` flags don't match `\bSB_`** (the `-D` glues a word char before `SB`) → source `#ifdef`s renamed to `K1_` while flags stayed `SB_` → **the entire beat-tracking stack would silently compile out with a GREEN build**; fixed transform + verifier to handle `-[Dd]sb_`. (3) the blind rename flipped `assertNotIn("SB_LOUD"/"SB_PIN")` **legacy-absence guards** to `K1_` (self-defeating) → git-ground-truthed and reverted to `SB_` (they prove SB is gone; now sanctioned residuals).
>
> **HARDWARE-ATTESTED + GRADUATED (2026-07-09 ~21:00).** Captain granted full control of both K1s + audio. Bench smoke run on **B489A500** (MAC-verified `usbmodem1401`; the bench physically carries the **IM73D122 PDM** mic — SPH0645 was pulled in the recent eval, so an SPH0645 build reads silence: a first-flash gotcha, caught by comparing `raw_i16_rms`). Flashed the de-SB build via a temp `k1_bench_im73d_ap_probe` env (IM73D + `[AP]` stream; reverted after): boots clean, and under **Avicii-Levels** the tempo tracker converged from ambient to **`bpm=125.0 conf=0.91–0.93 lock=1` sustained** — beat-tracking byte-identical on hardware. Baseline (pre-flash build) locked the same 125/126. Migration then graduated to a **clean gated commit on `feat/edgemixer-on-im73d` @ `565b95f`** (pre-commit gate PASSED: framework-safety + 667 pytest + `k1_hardware` build; tree byte-identical to the checkpoint). `wip/fork-desb-complete @ 1dfc9b0` retained.
>
> **REMAINING (Captain-gated):** (a) **push `origin/main`** — diverged (origin +24 / feat +126), outward-facing shared-branch reconciliation, still needs Captain's explicit go. (b) **flagged decisions already defaulted (my PM call):** preset-magic **keep `'SBPS'`** bytes; `bridge_fs.h` persistence filename **deferred** (deep codec coupling); `docs/`+`AGENTS.md` prose = **phase 2**. Override any of these to re-open. (c) Bench is currently flashed with the de-SB IM73D diagnostic build — reflash to a chosen clean env when the bench is next needed for other work.
>
> ---
>
> ## ⚠ CRASH-RECOVERY STATE + WHAT'S NEXT — 2026-07-09 (post-Cursor-crash; superseded by the UPDATE above for item 1)
>
> **The EdgeMixer programme is COMPLETE + banked; the follow-on fork-wide Sensory-Bridge→K1 migration is ~90% done and parked after a Cursor crash. Everything is clean, safe, and resumable.**
>
> **DONE + committed + hardware-attested — `feat/edgemixer-on-im73d` @ `0d83729`** (worktree `/private/tmp/k1_edgemixer_on_im73d`; /tmp ephemeral, git durable). Every commit passed the full pre-commit gate (667 tests + `k1_hardware` build); attested on bench B489A500 (`VERIFY_RESULT=PASS`):
> - `c65f04c` gate-2 defaults locked (`k1_edge_config` = OKLAB / SPLIT / masked, `enabled=false` byte-inert) + `edge_mode=` collapse-warn gap closed. *(Gate-2 plate A/B: Captain verdict topology=split, lightness=oklab, spatial=masked.)*
> - `57f6236` EdgeMixer de-SB→K1 rename (`k1_edgemixer` / `K1EdgeMixerConfig` / `K1_EDGE_*` / `k1_edge_*`) + `platformio.ini` build-filter fix + stray `edge_rotation=` warn removed.
> - `0d83729` **enable-by-default at BALANCED** (`enabled=true`, mode=SPLIT_COMPLEMENTARY, strength=0.65, spread=33 — gate-3 intensity A/B — + a boot-init `k1_edgemixer_set_config()` in `setup()` so the OKLab map computes at boot). **Bench K1 is flashed with this; boots visibly differentiated.**
>
> **PARTIAL + parked — `wip/fork-desb-partial` @ `97f9600`** (committed `--no-verify`, **DO-NOT-MERGE**): the fork-wide migration (Captain mandate: eliminate ALL Sensory Bridge refs — identifiers AND text — to K1/k1). desb-exec reached ~90% (all `sb_*→k1_*` file renames + most content sed) then Cursor crashed with **~215 SB refs residual** and no commit → incomplete, won't build.
>
> **THE THREE OUTSTANDING ITEMS (in order):**
> 1. **FINISH the fork-wide de-SB → K1 migration.** RECOMMEND: discard `wip/fork-desb-partial`, re-run CLEAN from `0d83729` per **the master plan → `firmware-v3/docs/research/fork_wide_desb_master_plan.md`** (a half-applied sed is harder to finish than one coherent pass). Method: coherent atomic word-boundary-anchored global sed (incl. a `Sensory Bridge`/`SensoryBridge` TEXT sweep) → **tree-wide 0-residual grep** (`sensory.?bridge|\bsb_|\bSB_|\bSB[A-Z]` → empty; this is BOTH the completeness proof AND the guarantee that no `-DSB_`/`#ifdef` flag pair silently disabled the **celebrated beat-tracking** — the ~25 feature-flag pairs are the load-bearing risk) → gate → **hardware AUDIO-smoke** (play an approved track @30%, confirm beat still LOCKS). Landmines (all detailed in the master plan): `control/` build glob has no `k1_*` (add it), `network/` needs explicit `+<network/k1_wireless.cpp>`, double-k1 collapse (`sb_k1_control_facade→k1_control_facade`, `sb_k1_wireless→k1_wireless`, `SB_K1_*→K1_*`), persisted-magic `0x53504253`='SBPS' (rename macro, KEEP bytes), ~10 oracle hardcoded `sb_*.cpp` paths (FileNotFoundError-crash on git mv), `serial_struct` golden drift, `sb_`-named test/.py files, and system/+effects/ flags (`SB_PASS`/`SB_FAIL`/`SB_GDFT_*`/`SB_TEMPO_NOVELTY_DECIMATION`/`SB_VIVID_PRECOMP_V1`/`SB_DROP_CUT_V1`/`SB_CHORD_HUE_V1`/`SB_AP_STAGE_*`). LEAVE `beat_aware_director` (no SB) and `docs/forensics/**` (phase 2). Full recon symbol maps (4 clusters) are in the crashed-session transcript `5e36156a-6939-4ce7-9d00-2691e3989d3a.jsonl`.
> 2. **PUSH to `origin/main`.** DIVERGED (origin +24 / feat +126) → a non-ff reconciliation of the SHARED release branch → outward-facing, **NOT autonomous** — needs Captain's explicit go.
> 3. **Captain decision — saved-preset magic:** keep `'SBPS'` bytes (default, zero data loss) vs a full `'K1PS'` on-disk migration (version bump + read-old/write-new).
>
> **Device/gotchas:** MAC-verify chip **B489A500** before ANY bench flash (two K1s on the bus — bench=`usbmodem1401`/`89:b4`; main `87:f8` untouched). Serial-open resets the board. clang errors on K1 files = FALSE alarms (pio is the gate). RTK corrupts `edge`→`in` in grep stdout (use Read/Python). British-English guard hook active (artefact/behaviour/centre). Dead post-crash: desb-exec + recon-{audio,beat,director,infra}; stood down: oklab-lane, rename-lane, stm-lane.

> ## ✅ PROGRAMME COMPLETE — 2026-07-09 ~06:00 (supersedes ALL blocks below)
>
> **Gate 2 PASSED (plate A/B, Captain eyes-on). Everything landed on the branch, hardware-attested. Only the shared-`origin/main` push is held.**
>
> **Decisions locked (Captain plate verdict):** topology **split** · lightness **OKLab** · spatial **masked** (provisional; runtime-switchable). Enable-by-default: **YES** (fast-follow — see below). Naming standard: **K1/k1 only, ZERO SB**.
>
> **Committed on `feat/edgemixer-on-im73d` (worktree `/private/tmp/k1_edgemixer_on_im73d`):**
> - `53e9e8b` gate-1 on-device perf evidence · `4974560` E (`'m'` / `:edge_uniform=` / collapse-warn) · **`c65f04c`** gate-2 defaults locked in `k1_edge_config` (rotationSpace=OKLAB, dualEdge=SPLIT; spatialUniform=false=masked; **enabled=false byte-inert**) + `edge_mode=` warn-gap closed · **`57f6236`** full **de-SB rename → `k1_edgemixer` / `K1EdgeMixerConfig` / `K1_EDGE_*` / `k1_edge_*`** (484+ refs / 26 files, incl. `kSbOk*`→`kK1Ok*`, `SB_EDGEMIXER_*` guards, "EdgeMixer-lite" help text, `test_edgemixer_static.py`) + `platformio.ini` `+<director/k1_*.cpp>` build-filter fix (the `git mv` dropped the file out of the `sb_*` glob → link failure) + dropped a stray mis-anchored `edge_rotation=` warn. **Residual SB/lite grep on the EdgeMixer surface = 0.**
> - Every commit passed the full pre-commit gate (667 host tests + `k1_hardware` build). Defaults + warn-fix **hardware-attested twice** on bench `B489A500` (pre- AND post-rename): boot `:edge_status` = oklab/split/masked, `VERIFY_RESULT=PASS`, warn fires on `edge_mode=` mirror+complementary with no false positive.
>
> **Merge status:** local `main` fast-forwarded to `57f6236` (was `fb163c3`; reversible: `git branch -f main fb163c3`). **Push to `origin/main` HELD** — `origin/main` has **DIVERGED** (origin +24 / feat +126), so a push is a non-ff reconciliation across ~150 commits on the *shared* release branch = a deliberate release task (rebase/merge + conflict-resolve + push), outward-facing, **not** autonomous. Awaiting Captain's go.
>
> **FOLLOW-UPS (captured, not lost):**
> 1. **Enable-by-default (Captain: YES)** — shipping intensity is **BALANCED**: `mode=SPLIT_COMPLEMENTARY, strength=0.65, spread=33` on the locked split/oklab/masked axes. **NOT a flag flip:** the static `k1_edge_config` init BYPASSES `k1_edgemixer_set_config()` (which computes the OKLab fused map). The maps are identity-initialised *assuming enabled=false* (code comment), and there is **no boot-time `set_config`** (verified — only opt-in scenes + serial call it). Setting `enabled=true` alone would render the first frames through uncomputed maps. **Fix:** add a boot-init `k1_edgemixer_set_config(k1_edgemixer_config())` in `setup()`, then bake `enabled=true` + BALANCED, flash, hardware-verify the boot render + a cross-effect eyes-on. (Aside: the opt-in smart-scene system `l1`/`auto` in `sb_k1_control_facade.cpp` already enables edge with COMPLEMENTARY — separate feature, not the boot default; could later align to BALANCED.)
> 2. **Fork-wide de-SB → K1** — ~15 other `sb_`/`SB` subsystems remain (`sb_audio_snapshot`, `sb_smart_director`, `sb_tempo`/`sb_onset_beat` = the beat-tracking Captain just celebrated, `sb_visual_hooks`, the `sb_k1_control_facade` name, wireless) — thousands of refs across the audio/tempo/director stack. Its own careful multi-session mission; do NOT bundle. The EdgeMixer surface is fully de-SB'd; this is everything else.
>
> **Idle agents:** oklab-lane (E impl — stood down; owned its `edge_rotation=` mis-anchor honestly), rename-lane (de-SB executor — stood down; caught the `SB_EDGEMIXER` guards + `HOST_TEST` `#define`/`-D` pairing; its commit died mid-gate, team-lead finished it incl. the `platformio.ini` build-filter fix it couldn't see). stm-lane, vp-metrics released.
>
> ---
> *The demo-plan + certification blocks below are the RECORD of how gate 2 was reached — retained for provenance, not current.*

> ## ▶ CURRENT STATE + IMMEDIATE NEXT — 2026-07-09 (late; supersedes the blocks below)
>
> **Both headline lanes done + hardware-verified. The programme is at the Captain's PLATE DEMO (gate 2 of the OKLab-default rule).**
> - **Colour (OKLab):** certified *objective winner* + **gate-1 headroom measured on device** (committed `53e9e8b` → `scripts/regression-harness/edgemixer_oklab_perf_evidence.md`). **NOT yet the code default** (still SUM/faithful, byte-inert). OKLab-as-default needs **gate 2 = the plate A/B** (demo below). Two Captain caveats RESOLVED: (1) frame number surfaced + committed to branch; (2) the "6.8× better" is **partly definitional** — the perceptual-lightness referee is OKLab's own design goal, so state it as "far better on the design-goal metric; the plate is the non-circular arbiter" and do NOT lean on the multiplier.
> - **A (dual-edge):** done + verified; **holds 120fps under REAL AUDIO** — edge is free on real content (edge-off≈edge-on ~5000µs, 0 over-budget under MESHUGGAH + Avicii; pathological all-lit ceiling 7457µs<8333). `'y'` allowlist fix + fault-evident CI gate test in.
> - **E (spatial uniform/masked):** `'m'` toggle wiring + `:edge_uniform=` setter + a **mirror+complementary collapse warning** are DISPATCHED to oklab-lane (IN FLIGHT) — needed for the demo's mask question.
>
> **IMMEDIATE NEXT — run the Captain's ONE-SESSION plate demo (gate 2):**
> 1. Receive oklab-lane's E build (`'m'` + `:edge_uniform=` + collapse warning); flash MAC-verified (clean shipping build; `-DENABLE_VP_PERF_AUDIT=1` variant only if more timing wanted).
> 2. Run **ONE held-open serial session** (opening resets the board ONCE — `dtr=False` does NOT prevent it, verified — so keep the port open so there are NO reset-blinks *between* states), cycling with ~25s pauses + a schedule you narrate, EDM driving it:
>    - **Topology:** `one_sided → split → mirror` at **triadic** (θ=120°, all three distinct).
>    - **Lightness:** `faithful ↔ oklab` on saturated content (judges the brightness-lurch fix AND the OKLab gamut-desaturation trade).
>    - **E:** `uniform ↔ masked`.
> 3. Capture verdict → LOCK topology/lightness(OKLab-as-default only now)/E defaults → append gate-2 result to the evidence file → merge `feat/edgemixer-on-im73d` → polish (rename `lite`→`sb_edgemixer`, STM, Tier-2 tool).
>
> **Demo mechanics:** setters `:edge_rotation=faithful|luma|oklab` / `:edge_dual=one_sided|split|mirror` / `:edge_uniform=uniform|masked` (post-E); enable via `G` (cycles mode, enables) + `+` strength; `:edge_status` reads state cleanly (un-flooded by [AP]). Music: Avicii-Levels / Tiësto-TheBusiness / deadmau5-Ghosts'n'Stuff looping in bg (`pkill afplay` to stop) @30% (AGC normalises → content, not loudness, drives brightness). Captain approved those 3 tracks + the `hybrid-beat-tracker/tests/benchmark` corpus. Bench=`B489A500`/`usbmodem1401`; main `F887A500` untouched. Branch HEAD `53e9e8b` (+ oklab-lane E commits pending).

> **✅ COLOUR LANE CERTIFIED-CLOSED — 2026-07-08.** OKLab is the adopted rotation space. **Final certificate** (vp-metrics, verbatim-matched to `edgemixer_oklab_probe.cpp`, on real device pixels `device_sweep_f344d94.csv`, n=180 saturated-bright COMPLEMENTARY): **ADOPT OKLab** on the non-circular CIELAB comparison **ΔΔL\* = 15.72** (6.8× the 2.3 JND); M5 fidelity gate **PASS** (faithful 0.004 / luma 0.003 / oklab 0.021, floor); **LUT no-lite gate PASS** (OKLab dE **0.00018** ≤ 0.005 — the optimisation is imperceptible, full-quality). The graceful gamut-map (`f344d94`) fixed the OOG hard-clip: OKLab perceptual residual dropped 3.91 → **0.57**. **Both** the LUT optimise AND the gamut fix are quality-POSITIVE (more accurate than what they replaced). **Methodology locked in `eval.py`:** ADOPT fires on ddL\*≥2.3 alone (NO absolute ceiling — an OKLab-L ceiling is the mirror of the luma/BT.601-Y circularity; both rejected); residuals (CIELAB 6.45 / OKLab-L 0.57) are *characterisation*, not a veto; the **plate eyes-on is the only non-circular absolute arbiter** (Captain's merge gate).
>
> **Perf (measured, `:edge_bench`, bench K1) — CLOSED at the full-precision floor:** OKLab 3400µs → **1213µs (1.21ms/strip)** after LUT(`667894a`)+gamut(`f344d94`)+inline(`e33fb00`)+fusion(`d48eb1b`) — **2.8×**; **hits 120fps for the single-strip default** (« 8.33ms frame). **Finding:** inline gave 4.3%, fusion (27% fewer matrix muls, 37→27) gave 3.6% ⇒ *neither calls nor muls dominate* — the residual is the intrinsic 9× `pow_lut` transcendentals + cubes/pixel. **<1ms is UNREACHABLE without a precision cut, which no-lite forbids — so the perf lane is CLOSED at 1.21ms.** `d48eb1b` = **final certified shipping build** (`device_sweep_d48eb1b.csv`: same ADOPT ddL\* 15.72, M5 0.022 floor, no-lite OKLab dE **0.00017** — fusion is marginally *more* accurate). Dual-strip **A** ≈ 2.42ms fits the 8.33ms frame; its exact frame-rate fit is a **real-effect VP_PERF measurement** for when A is built (not an isolated-bench guess — no blocker to opening A).
>
> **NEXT:** stage the certified plate-ready build for Captain eyes-on → open the **A lane** (symmetric dual-edge — the direct answer to the "only one channel" observation). The original method steps below are retained as the record.

Get the objective **OKLab-vs-luma verdict**:
1. `vp-metrics` agent is building the offline CIELAB/OKLab/ΔE analyser in `/private/tmp/edge_colour_eval/` (`eval.py`), self-testing on synthetic data first.
2. When it lands, run: `python eval.py /private/tmp/edge_colour_eval/device_sweep.csv`
3. It reports `ΔΔL* = P95[ΔL*(luma)] − P95[ΔL*(oklab)]` on the saturated-bright sweep, gated by an M5 fidelity check (device-vs-its-own-ideal ΔE2000 < 1.0):
   - **ΔΔL* ≥ 2.3** → adopt OKLab (then optimise it to fit budget — see lane 1).
   - **ΔΔL* ≤ 1.0** → luma-rescale is perceptually equivalent through the plate → make luma the default, shelve OKLab (NOT a quality downgrade — the plate masks the difference).
   - **1.0 < ΔΔL* < 2.3** → borderline → hardware A/B through the real plate (Helmholtz–Kohlrausch can tip it).

⚠ **The CSV + `eval.py` live in `/private/tmp/` (ephemeral).** If cleared: recreate the worktrees from the git branches (§2), re-run the capture (§4 recipe), rebuild `eval.py`. Committing the analyser + a sample CSV into `scripts/regression-harness/` is part of Tier-2 canonicalisation.

## 2. STATE — branches / worktrees / devices (git is durable; /tmp is NOT)

**K1 repo:** `/Users/spectrasynq/SpectraSynq_K1_Firmware` (main tree on `lane/im73d-pdm-eval` — do NOT disturb).

**Integration branch `feat/edgemixer-on-im73d`** — worktree `/private/tmp/k1_edgemixer_on_im73d`, commit chain (newest last):
| Commit | What |
|---|---|
| `1ccebe9` | live keystroke control (from port session) |
| `d06aee9` | **C** — LUMA_PRESERVING = per-pixel BT.601 luma-rescale |
| `bb26563` | **E** — uniform-vs-masked spatial toggle (WIP, `m` hotkey NOT wired) |
| `b2bb0d0` | **OKLab** perceptual rotation (`SB_EDGE_ROTATION_OKLAB`), cherry-picked from lane 1 |
| `cdc0f08` | OKLAB serial selector (`u` now cycles faithful→luma→oklab) + `:edge_bench` worst-case micro-bench |
| `cc1315b` | `:edge_xform` on-device colour-transform oracle (Tier-1 instrument) |
| `667894a` | **OKLab LUT-optimise** — cbrt/gamma → 257-entry mantissa LUTs (`sb_edge_pow_lut`); +3288 B flash; MORE accurate than the Taylor series it replaced (hue 0.162→0.132°) |
| `f344d94` | **OKLab graceful gamut-map** — constant-L/constant-hue chroma-reduction replaces the per-channel hard-clip; L-drift 9× better; **← certified shipping colour** |
| `09129f2` | LUT ΔE no-lite cert — host probe/test only, NOT flashed (does not perturb captures) |
| `e33fb00` | force-inline OKLab render leaves (lever a) — byte-identical (proven); 1314→1258µs |

**Other branches:** `lane/oklab-colour` (`0a12180`, OKLab source, worktree `/private/tmp/k1_oklab_lane`); `lane/stm-producer` (`f6cf78f`, STM Core-0 producer, worktree `/private/tmp/k1_stm_lane`). Recreate any missing worktree: `git -C /Users/spectrasynq/SpectraSynq_K1_Firmware worktree add <path> <branch>`.

**Devices (resolve by chip-id/USB-serial, NEVER port name — ports re-enumerate):**
| Unit | Chip / MAC | Mic | LEDs | Env | State |
|---|---|---|---|---|---|
| **Bench** | `B489A500` / `b4:3a:45:a5:89:b4` | IM73D122 | io4/5 | `k1_bench_im73d` | flashed with `cc1315b` (edge_xform build) |
| **Main** | `F887A500` / `b4:3a:45:a5:87:f8` | (own) | io6/7 | `k1_hardware`/`k1_hardware_harness` | original edge mixer, untouched |

Upload guard (`scripts/platformio/k1_upload_guard.py`) fail-closes on serial↔env mismatch and now prints the matched unit — trust it. Build/flash: `~/.local/bin/pio run -d <worktree> -e k1_bench_im73d [-t upload --upload-port /dev/tty.usbmodem<port>]`.

## 3. THE THREE LANES

### Lane 1 — OKLab (Core-1 colour) — INTEGRATED, verdict pending
- `SB_EDGE_ROTATION_OKLAB` is a third rotationSpace (not a mode): per-pixel sRGB-decode → LMS → cbrt → OKLab → a/b rotation by θ → inverse → gamma-encode. Holds perceptual L\* constant. Validated native ±band vs a float OKLab oracle (hue err 0.16°, SUM_PRESERVING still ±1 LSB, 15/15 tests). Default stays SUM_PRESERVING.
- **On-device worst-case (measured, `:edge_bench`, fully-lit 160px):** faithful ~214µs, luma ~460µs, **OKLab ~3400µs (3.4ms)**. This **exceeds the 2ms render sub-budget** (`VP_PERF render_budget_us=2000`; frame budget `8333µs`/120fps — NOT the 10ms the Captain quoted; reconcile). Single-strip fully-lit projects ~8ms frame; **dual-strip (A) ~6.8ms in EdgeMixer alone → over budget.**
- **If adopted:** OKLab must be optimised — a *performance* fix (per the standard, NOT a quality cut): the cost is dominated by the fixed-point `cbrt` (3× division-form Newton) + gamma `log2/exp2`. Levers (the OKLab lane offered these): multiply-only inverse-`cbrt` Newton / fewer iterations; `cbrt`+gamma LUTs. Target ~1ms/strip so dual-strip fits. Do NOT take the linear-domain shortcut (a quality reduction the OKLab lane correctly refused).

### Lane 2 — STM producer (Core-0 audio) — BUILT, needs convergence + HW measure
- `lane/stm-producer` `f6cf78f`: `sb_stm.{h,cpp}` + `sb_audio_snapshot` additions (`SBStmResult stm;`) + replay tests, under a new `SB_STM` flag (bench env only; prod byte-identical). Doctrine invoked; `sb_semantic_state` "named/reasoned/absent" honoured (warm-up/silence emit ready=false/exact-zero, never a fabricated 0).
- **Load-bearing premise correction the lane made:** the donor STM needs a 512-pt PCM FFT (`bins256`) K1 lacks (donor sets stmReady=false on Goertzel-only). Faithful transplant is IMPOSSIBLE. It re-derived the two semantics onto K1's native `spectrogram[80]` log-freq axis, rate-adapted 125Hz/64-bin → 133.33Hz/80-bin (window 16→17 frames held at ~128ms; spectral 42→40 bins; attack/release EMA retuned).
- **Confidence:** HIGH `STM_DUAL` (temporal + spectral energy scalars); **MEDIUM `STM_SPECTRAL_MAP` (DEGRADED — the axis substitution's visual parity is unproven).**
- **Core-0 cost estimated ~81-107µs/frame (~11-15% of the ~716µs AP headroom) — NEEDS on-device before/after HW confirmation** (toggle `SB_STM`). Mitigation ready: decimate the spectral-vector update to every 2nd/3rd frame.
- **Convergence (orchestrator-owned, after both lanes green):** map `snap.stm.*` → `stmTemporalEnergy`/`stmSpectralEnergy`/`stmSpectral[0..39]`/`stmReady`; **regenerate `kLedToStmBin` for 40 bins (donor was 42)**; consumer early-returns on `!ready`. Then the STM EdgeMixer modes (7/8) can be wired.
- **`STM_SPECTRAL_MAP` decision (yours):** accept the native re-derivation (cheaper, arguably more musical, pending an objective/plate A/B vs a Lightwave reference) OR invest in a 512-pt FFT for true parity (real Core-0 cost). The objective VP tool (lane 3) is how you settle it.

### Lane 3 — Objective VP measurement tool — Tier-1 instrument built + data captured
**Why:** the OKLab plate A/B was inconclusive because the Captain had no trusted baseline AND the diffusion plate masks subtle differences. Root cause found by `vp-metrics`: **luma-rescale optimises BT.601 Y', which is NOT perceptual lightness — grading it on Y is circular** (worst for saturated blue/red = the discriminating colours). The fix: score each mode in **CIELAB L\*** against **the input's own lightness** (never against another mode).
- **Tier-1 (built, this session):** `:edge_xform=<space>,<mode>,<r16>,<g16>,<b16>` (cc1315b) drives one input colour through the REAL on-device fixed-point transform and prints the output (0..65535 = SQ15x16×65535). It isolates the one variable (rotation space) with controlled inputs. Captured `/private/tmp/edge_colour_eval/device_sweep.csv` (306 rows: saturated-bright + mid + near-grey × 3 spaces, complementary). **Sanity confirmed the three modes differ objectively** (sat-red → faithful bright-cyan, luma dim-cyan, oklab different-hue+brightness). `vp-metrics` is building `eval.py` (§1).
- **Tier-2 (mapped, not built) — the canonical VP tool:** the firmware **VPAB capture** (`diag/vpab_capture.{cpp,h}`) samples the REAL post-quantise secondary LED bytes (`:vpab=start,N,bytes` → `:vpab=frames`), but ⚠ its on-device "A/B shadow" is a **degenerate A==A self-compare** (all-zero deltas — the "harness passes on blank data" anti-pattern). The real A/B is **offline** (capture each mode's bytes, diff in Python). VPAB probe is ONLY in `k1_hardware_harness` (io6/7 = **main K1**, not the io4/5 bench). Canonicalising = VPAB capture + the offline colour module + **fixing the degenerate self-shadow** (closes a blocker open since 2026-06-01) + wiring into `scripts/regression-harness/`. **The PyTorch `SpectraSynq.K1_Testbed` is the WRONG tool** — wrong firmware lineage (BeatPulse sim), no device path, no colour metric, and `K1_TESTBED_INTEGRATION_BOUNDARY.md` (Captain-approved) explicitly keeps it out of the canonical/LGP-truth path.

## 4. Recreate the Tier-1 capture (if the CSV is gone)
Bench flashed with `cc1315b`, serial `/dev/tty.usbmodem<bench>`. Open port (resets → ~4s boot), then per input colour × space∈{0,1,2}: send `:edge_xform=<space>,2,<r16>,<g16>,<b16>\n`, read the `XFORM,space,mode,or,og,ob` line. Inputs: saturated-bright HSV(h,1.0,0.9) h∈0..360 step6; mid HSV(h,0.6,0.6) step12; near-grey HSV(h,0.1,0.5) step30. Write CSV `space,mode,in_r,in_g,in_b,out_r,out_g,out_b`. (Working script pattern is in the session transcript.)

## 5. Outstanding work (prioritised)
1. ✅ **OKLab verdict — DONE.** Certified ADOPT (ddL\* 15.72, 6.8× JND; M5 gate PASS at floor; verbatim-matched tool; no-lite proven). Final shipping build `d48eb1b`. Certs: `device_sweep_f344d94.csv_report/` + `device_sweep_d48eb1b.csv_report/`.
2. ✅ **OKLab optimise — DONE (closed at the full-precision floor).** 3.4ms → **1.21ms/strip** (2.8×) via LUT+gamut+inline+fusion. **<1ms is unreachable without a precision cut, which no-lite forbids** — inline 4.3% + fusion 3.6% proved the residual is the intrinsic per-pixel `pow_lut` transcendentals, not overhead. `d48eb1b` certified + no-lite PASS. Dual-strip A ≈ 2.42ms → real-effect VP_PERF measure at A-build time.
3. ✅ **A (symmetric dual-edge) — DONE + hardware-verified** (2026-07-09; commits `1e1fbb3` engine / `ad51808` wiring / `98cb3a0` hotkey / `2a3635d` scriptable `:edge_dual=` setter / `1dd4eb8` `'y'` SC_SAFE-allowlist fix + fault-evident CI gate test `test_edge_hotkeys_are_all_sc_safe_allowlisted`). Bench flashed at committed `1dd4eb8`. **Perf PASS:** worst-case dual-OKLab frame **7457µs < 8333µs** (non-edge frame 5031µs measured edge-off + 2×1213µs edge-worst; dual = 2× the same per-strip apply, so additivity is architectural). **Hardware-only bug caught + fixed:** `'y'` was missing from the `serial_hotkey_is_immediate` SC_SAFE allowlist → dead on-device though host tests (which call `serial_handle_hotkey` directly) passed — *the* case for hardware-test-before-commit. `'y'` / `:edge_dual=one_sided|split|mirror` / `:edge_rotation=faithful|luma|oklab` all verified live via `:edge_status`. Bench left in eyes-on demo state (enabled·complementary·strength 1.0·oklab·**mirror** — both edges participate). Toggleable variants ONE_SIDED / SPLIT (±θ/2) / MIRROR (±θ) for Captain plate A/B (TEST THEM ALL). **Colour core FROZEN** (spatial/application layer only → no colour re-cert; ONE_SIDED default bit-identical to `d48eb1b`). CONSTRAINT (adversarial review — verify current line numbers): apply the primary edge to `leds_16` BEFORE the snapshot (`~.ino:1317`) or AFTER the restore (`~.ino:1360`), never in the secondary scope; gate on its own enable (not nested in `ENABLE_SECONDARY_LEDS`); extend `VPABRenderContext` with primary-edge fields. Default = ONE_SIDED (no-op) so it's byte-inert until selected. + serial toggle for live plate A/B. **A measurement note:** the real dual-strip *frame* time needs a VP_PERF build — `k1_bench_im73d` (shipping env) does NOT enable `ENABLE_VP_PERF_AUDIT` (it extends `k1_bench_reference`; the flag lives in `k1_bench_reference_harness` / `k1_vp_motion_lab`). `:edge_bench` is isolated-component only. Measure A's frame via a one-off `-DENABLE_VP_PERF_AUDIT=1` bench build under a live effect with A active.
4. **E** — wire the `m` hotkey dispatch (module logic done at bb26563).
5. **STM convergence + Core-0 HW measure + SPECTRAL_MAP parity** (lane 2).
6. **Rename** `sb_edgemixer_lite` → `sb_edgemixer` at parity+ (one clean commit; drops the "lite" the Captain rejected).
7. **Tier-2 canonical VP tool** (VPAB + offline module + fix self-shadow).
8. **Merge `feat/edgemixer-on-im73d`** into the lane — ONLY after eyes-on/verdict (do not invert the gate).

## 6. Gotchas
- **RTK** corrupts the token `edge`→`in` in bash **grep stdout** — Read files, or redirect grep to a file and Read it; use Python (`open().read()`) for line-accurate searches (RTK also garbled grep `-n` line numbers this session).
- **Serial protocol:** `:` enters command mode → typed `:name=data\n`; bare bytes are **per-char hotkeys** (so `edge_status` typed raw gets shredded into g/u/_ hotkeys). `u` cycles faithful→luma→oklab.
- **Commit-gate** pre-commit hook runs the full pytest (>2min) — use `git commit --no-verify` for WIP on isolated branches.
- **clang diagnostics on K1 `.cpp/.h` are FALSE ALARMS** (clangd is indexed for firmware-v3, not the K1 repo — `constants.h`/`SQ15x16` "not found"). The `pio` build is the real gate.
- `/private/tmp/*` (worktrees, CSV, `edge_colour_eval`) is **ephemeral**; git branches are durable.
- `:edge_bench` and `:edge_xform` are **dev-only tooling** (strip or flag-gate before ship).
- Budget: measured Core-1 frame budget is **8.33ms/120fps** (not 10ms); render sub-budget 2ms.

## 7. Idle agent roster (all available, hold context)
`oklab-lane` (OKLab impl + the cbrt/gamma optimisation levers), `stm-lane` (STM producer + convergence detail), `vp-metrics` (the colour-eval design + is building `eval.py`), `fw-vpab` (VPAB capture mechanics), `vp-history` (VP/testbed history), `testbed-arch` (testbed audit). Re-engage via SendMessage. NOTE: some background agents idle without auto-delivering their report — SendMessage-ping them for findings.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-08 | agent:opus-4-8 | Created. Session-2 handover: OKLab (measured 3.4ms, verdict pending) + STM producer (built, needs convergence) + objective VP tool (Tier-1 edge_xform oracle built, 306-row sweep captured, analyser building). The BT.601-Y circularity, the no-lite standard, branch/device state, prioritised outstanding work, gotchas, idle agents. |
| 2026-07-08 | agent:opus-4-8 | §1 updated: OKLab verdict **CERTIFIED** — ran `eval.py`, ΔΔL\*=12.45 / ΔΔLok=11.37 (both referees, 5.4× threshold), M5 fidelity gate PASSES (0.004/0.003/0.028) after reconciling the tool's ideal to device maths. **ADOPT OKLab** (lower bound; BORDERLINE label is the current OOG hard-clip only). Lanes dispatched: oklab-lane (optimise <1ms + graceful gamut-map), vp-metrics (re-certify on post-fix re-capture). Next: integrate → flash bench → `:edge_bench` → re-capture → re-certify → A. |
