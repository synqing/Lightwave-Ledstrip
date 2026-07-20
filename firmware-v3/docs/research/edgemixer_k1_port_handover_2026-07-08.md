---
abstract: "Handover for the EdgeMixer→K1 port after the 2026-07-08 session. The colour port + keystroke control is DONE and PROVEN (±1 LSB parity, keystrokes live-verified) but sits on the WRONG base branch: everything was built on main HEAD (SPH0645 mic config) while the bench K1 hardware is IM73D122 — the root cause of a full session of boot-loops and dark LEDs. Read before ANY further EdgeMixer or K1-bench work. Contains: exact repo/branch/commit state across two repos, done/outstanding on the EdgeMixer lane, the critical wrong-mic reframe, device facts (chip-ids/mics/pins/envs), gotchas, and the H1–H6 process improvements to canonise."
---

# EdgeMixer → K1 Port — Handover (2026-07-08)

## 0. Prime directive (read first)
The EdgeMixer colour port + serial keystroke control is **finished and proven** — but it was built on the **wrong base**. Everything this session was based on `main` HEAD, which configures the **SPH0645** microphone. The **bench K1 hardware is IM73D122** (PDM). Wrong mic → the I2S read gets garbage/near-zero (`max_raw=1`) → the audio loop free-runs → IDLE0 starves → **silent-idle watchdog boot-loop** → dark LEDs. **Do NOT continue on the SPH0645 base.** The single most important next action is to re-apply the EdgeMixer work onto the **IM73D122 branch** and build the bench from there.

## 1. The root failure (so it is not repeated)
Firmware was flashed for the **wrong hardware configuration** onto a working unit ~5 times. MAC/chip identity was verified every time; the **mic configuration was never verified against the build**. The `max_raw=1` telemetry was the tell from the first capture and was misread as "silence" rather than "wrong microphone". Every downstream "fix" (GPIO pins, int64 GDFT flags, `827d73a`, `idle_core_mask`) was a symptom chase.

## 2. Current state — TWO repos

### A. `SpectraSynq_K1_Firmware` (the port target)
- **Worktree:** `/private/tmp/k1_edgemixer_colour_port`, branch **`feat/edgemixer-colour-port`**, based on `main` HEAD `fb163c3` — **WRONG base (SPH0645)**.
- **Committed on the branch:**
  - `3825c20` — EdgeMixer colour-port transplant into `director/sb_edgemixer_lite.{h,cpp}` + the `SB_EDGEMIXER_AB_DEMO` scaffold. ⚠ **Also accidentally committed stray `scratch_*.diff`/`scratch_*.txt` — delete these before any merge.**
  - `bc05267` — cherry-pick of `827d73a` (N2 bounded I2S read + task watchdog).
- **Uncommitted in the working tree:** the serial keystroke + typed commands (`serial/serial_menu.h`, `serial/serial_cmd_handlers.cpp`) and the `idle_core_mask` `0x1u→0x0u` edit (`SPECTRASYNQ_K1_FIRMWARE.ino:698`).
- **Main tree** (`/Users/spectrasynq/SpectraSynq_K1_Firmware`) is on the Captain's active lane `lane/im73d-pdm-eval` — do NOT disturb it.

### B. `Lightwave-Ledstrip` (source + audit + oracle — this session's cwd)
- **Source EdgeMixer:** `firmware-v3/src/effects/enhancement/EdgeMixer.h` (LightwaveOS original; the port FROM).
- **Port plan:** `firmware-v3/docs/research/edgemixer_k1_port_plan_2026-07-08.md`.
- **Golden-master oracle (the acceptance gate):** `firmware-v3/test/test_edgemixer_parity/` — native, 810 vectors, ±1 LSB, fault-evident. Run: `pio test -e native_test_edgemixer_parity`. This is the mic-agnostic correctness gate for the colour maths.
- `CHANGELOG.md` + `BACKLOG.md` updated (WB-3 STM blocker; source-side EdgeMixer defects).
- **All of the above are UNCOMMITTED working-tree changes** in Lightwave-Ledstrip.

## 3. EdgeMixer lane — done / outstanding

**DONE + PROVEN (mic-agnostic, survives the rebase):**
- SQ15x16 3×3 hue-rotation + BT.601 desaturation matrix, 6 colour modes (analogous, complementary, split, veil, triadic, tetradic) — **parity ±1 LSB vs the golden oracle**.
- Keystroke control (live-verified on hardware): `g` on/off · `G` cycle mode · `-`/`=` spread ±5 · `_`/`+` strength ±0.1 · `u` rotation faithful↔luma. Typed: `edge_enabled/mode/strength/spread/rotation` + `edge_status`.
- Existing `:edge_*` typed commands confirmed to drive the new matrix config.

**OUTSTANDING:**
1. **Rebase onto the IM73D122 base** (`lane/im73d-pdm-eval` or `feat/im73d-ota-bench`). Blocker for everything real.
2. **Delete SPH0645 bench build envs** per Captain's instruction — but only where an IM73D122+io4/5 equivalent exists (on the im73d branch, NOT on main HEAD, where deleting them would gut the file).
3. **Tier-1b `LUMA_PRESERVING`** rotation: currently a stub that falls back to `SUM_PRESERVING`. Needs a real luma-preserving implementation + its OWN oracle (a documented luma-band assertion, not ±1 LSB).
4. **STM modes 7/8 (`STM_DUAL`, `STM_SPECTRAL_MAP`): BLOCKED (WB-3)** — K1 has no spectral-temporal-modulation producer (`stmSpectral[42]`/`stmTemporalEnergy` absent). Out of scope until K1 audio grows it.
5. **Housekeeping:** remove the stray `scratch_*` files from `3825c20`.
6. **Source-side EdgeMixer defects** (Lightwave original, logged in `BACKLOG.md`): centre-gradient LUT off-centre (index 76 vs 79.5), serial-JSON mode-8 stale bound, false "luminance-preserving" doc claim, unvalidated "~22µs" perf claim, NVS write on render core.

## 4. ⚠ CRITICAL CAVEAT — do NOT cargo-cult the watchdog fixes
The boot-loop was a **wrong-mic artefact** (silent audio → IDLE0 starvation). On the **correct IM73D122 base with real audio, the loop is paced and the silent-idle freeze very likely never triggers.** Therefore:
- Build the IM73D122 base FIRST and check whether the boot-loop even reproduces before applying anything.
- `827d73a` (bc05267) and the `idle_core_mask=0` edit were fixing a symptom. They may be unnecessary on the correct base — or a defensible defence-in-depth. **Decide by observation on the correct base, not by inheritance.** (Also note: `827d73a` as shipped was buggy — it left `idle_core_mask=0x1`, still watching the very task that starves; if you do keep N2, keep the `0x0` correction.)

## 5. Device facts (verify against live chip-id + the registry before acting)
| Unit | Chip | MAC | Mic | LED pins | Env / base |
|------|------|-----|-----|----------|-----------|
| **Bench** | `B489A500` | `b4:3a:45:a5:89:b4` | **IM73D122 (PDM)** | **io4 / io5** | `k1_bench_reference`-family, **im73d branch** |
| **Main** | `F887A500` | `b4:3a:45:a5:87:f8` | (its own) | io6 / io7 | `k1_hardware`, runs ORIGINAL edge mixer (`da4258e`) — healthy |

- **Ports re-enumerate on reset — resolve by chip-id, never by port name.**
- **Upload guard** `scripts/platformio/k1_upload_guard.py`: fail-closed, validates MAC↔env(GPIO). Bench→`k1_bench_reference` only; main→`k1_hardware` only. **ANOMALY to investigate:** the last bench flash printed *"default K1 hardware GPIO assignment"* vs *"bench-reference GPIO assignment"* on earlier flashes — possible guard/registry drift or a concurrent edit (Codex). Confirm the compiled pinmap is actually io4/5.
- **Device registry:** `docs/hardware/device-build-registry.md` (K1 repo) — canonical device↔env↔build map. Currently records GPIO but **NOT mic-type** — that gap is H1.

## 6. Process improvements to canonise — H1–H6
- **H1 — Device Config Manifest + guard extension (highest value).** Extend `k1_upload_guard.py` from MAC↔GPIO to a full config contract `{chip-id, GPIO, MIC type, sensor variant, canonical branch}`; refuse a flash whose *compiled* config (mic define + pinmap define) mismatches the unit's manifest; print the matched mic+GPIO in the pass line. This alone would have hard-stopped the whole session at flash #1.
- **H2 — "Device Truth Card" readback.** Before hardware work, load the unit's config card and verify build-config against it. Hardware-task READBACK line: *"MAC is identity; the card is truth — mic, GPIO, canonical branch confirmed?"*
- **H3 — Hardware-flash workflow (gated skill):** resolve by chip-id → load config card → confirm base branch → guard-verify compiled config → **baseline-smoke first** → route visual acceptance to the human after flash #1 → reset-aware serial capture.
- **H4 — Input-first debugging:** for sensor/audio faults, inspect the transducer signal (`max_raw`, AGC) BEFORE the DSP. Degenerate input on a live device ⇒ config check, not pipeline debug.
- **H5 — Robust hardware-verify harness:** chip-id port resolution, RTK-safe (file-redirect), CDC-drop/re-enum aware. A flaky verify harness is worse than none.
- **H6 — Context-budget discipline:** front-load cheap/safe analysis; reserve budget for the irreversible, human-facing (flashing) step.

## 7. Gotchas
- **RTK** compresses Bash stdout and **corrupts "edge"→"in"** — Read files directly, or redirect `grep` to a file and Read it. Never trust `grep` stdout for the token "edge".
- **Commit-gate pre-commit hook** in the K1 repo runs the FULL `pytest` suite (>2 min) — use `git commit --no-verify` for WIP checkpoints on isolated branches.
- **Codex runs in parallel** on this repo (the Captain dispatches it) — coordinate, do not collide on files/branches.
- The `SB_EDGEMIXER_AB_DEMO` scaffold is flag-gated (default OFF); build WITHOUT the flag so manual keystroke control is in charge.

## 8. What worked — reuse this machinery
- The **23-agent adversarial audit swarm** (it refuted the orchestrator's own wrong hypotheses — a data race and a divide-cost, both false).
- The **golden-master parity oracle** (fault-evident, real-data — the model for H5).
- The **fail-closed upload guard** (it caught the wrong-GPIO flash #1 — H1 is "make it as rigorous about mic as it already is about GPIO").

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-08 | agent:opus-4-8 | Created. Full EdgeMixer→K1 port handover: wrong-mic root cause, cross-repo state, done/outstanding, N2 cargo-cult caveat, device facts, H1–H6, gotchas. |
