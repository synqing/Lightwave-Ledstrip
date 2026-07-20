# Fork-wide de-SB → K1 — MASTER EXECUTION PLAN (synthesised from 4 recon SSAs)

Repo: `/private/tmp/k1_edgemixer_on_im73d`, branch `feat/edgemixer-on-im73d` (HEAD `0d83729`).
Standard: **NO `sb_`/`SB` naming — K1/k1 only.** ZERO behaviour change. The EdgeMixer is ALREADY
`k1_edgemixer` (done) — do NOT re-touch it (skip `k1_edge*`/`K1EdgeMixer*`/`K1_EDGE*`).

Live source = `SPECTRASYNQ_K1_FIRMWARE/`. **EXCLUDE from all edits:** `.pio/build/**` (generated),
`SPECTRASYNQ_K1_FIRMWARE.ino.cpp` (generated Arduino artefact), `**/__pycache__/**`. **Leave**
`beat_aware_director`/`bad_director_*` (no SB branding — outside the mandate).

**MANDATE (Captain 2026-07-09):** COMPLETE migration OFF Sensory Bridge — *any and all* Sensory Bridge
references STRUCK and eliminated, to K1/k1. This means **identifiers AND human-readable text**: every
`Sensory Bridge` / `SensoryBridge` string, comment, serial help/log line → `K1`. Not just `sb_`/`SB_`.
- **Code migration = PHASE 1 (this execution, gate-validated).** Docs (`docs/architecture/*.md`, `.claude/*`,
  `progress.md`) = PHASE 2 of the SAME mandate, sequenced after code lands green (not excluded — deferred).
- **FLAGGED (Captain's call — surfaced separately):** persisted preset-file magic `0x53504253`='SBPS' bytes,
  and `docs/forensics/**` SB-named snapshots (historical evidence). See the FLAGGED-DECISIONS at the bottom.

## CONVENTIONS (word-boundary anchored — mandatory)
- `\bsb_` → `k1_` · `\bSB_` → `K1_` · `\bSB\([A-Z]\)` → `K1\1` (macOS sed: `sed -i '' -E`).
- Word-boundary is LOAD-BEARING: naive `SB_` corrupts `ARDUINO_USB_MODE` (mid-word). Anchoring fixes it.
- **TEXT/BRANDING sweep (the complete-migration requirement):** strike every `Sensory Bridge` /
  `SensoryBridge` → `K1` in comments, string literals, serial help/log text, and doc prose. Serial
  help/log text is USER-FACING (~80+ hits in serial_menu.h + serial_cmd_handlers.cpp) — those especially
  must read K1, not SB.

## ORDER OF OPERATIONS (each step avoids a red/silent build)

### STEP 0 — double-k1 collapse FIRST (before the general sed, or it produces k1_k1_)
Across all in-scope files: `s/\bsb_k1_/k1_/g` and `s/\bSB_K1_/K1_/g`.
Targets: `sb_k1_control_facade`→`k1_control_facade`, `sb_k1_control_*`→`k1_control_*`,
`sb_k1_wireless*`→`k1_wireless*`, `SB_K1_WIRELESS_*`→`K1_WIRELESS_*`, `SB_K1_HARDWARE`→`K1_HARDWARE`,
`SB_K1_BENCH_REFERENCE_PINMAP`→`K1_BENCH_REFERENCE_PINMAP`, `SB_K1_BLE_REMOTED`→`K1_BLE_REMOTED`,
`SB_K1_WS_TASK_CORE`→`K1_WS_TASK_CORE`.
⚠ VERIFY no pre-existing `K1_HARDWARE` symbol collision first (board env is literally named `k1_hardware`
— that's a build ENV name in `[env:k1_hardware]`, NOT a C macro, so no clash, but grep-confirm).

### STEP 1 — git mv the source files (+ the two double-k1 → single-k1)
audio/: `sb_audio_snapshot`, `sb_semantic_state`, `sb_musical_saliency`, `sb_i2s_capture_types`(h),
`sb_tempo`, `sb_onset_beat`, `sb_chord_detect`(cpp, no h) → `k1_*` (.cpp/.h as present).
director/: `sb_smart_director`, `sb_mode_selection`, `sb_visual_hooks` → `k1_*`.
control/: `sb_effect_queue`, `sb_wireless_control`, `sb_noise_cal_arm` → `k1_*`; **`sb_k1_control_facade` → `k1_control_facade`** (single k1).
network/: **`sb_k1_wireless` → `k1_wireless`** (single k1).
diag/: `sb_trace.h` → `k1_trace.h`.
harness/tests (python): `scripts/regression-harness/sb_trace_capture.py`→`k1_trace_capture.py`,
`scripts/regression-harness/sb_trace_l1_gate.py`→`k1_trace_l1_gate.py`,
`tests/test_sb_trace_l1_gate.py`→`test_k1_trace_l1_gate.py`,
`tests/test_sb_wireless_control_static.py`→`test_k1_wireless_control_static.py`.
(`k1_edgemixer.*` already renamed — skip. `beat_aware_director.*` — LEAVE.)
Confirm no target k1_*.cpp pre-exists before each mv.

### STEP 2 — general anchored sed across ALL in-scope files (source + oracles + tests + host stubs)
Apply the 3 CONVENTIONS globally to `SPECTRASYNQ_K1_FIRMWARE/**` (excl generated .ino.cpp),
`scripts/regression-harness/**` (oracles + stubs), `tests/**`. This renames: all identifiers, all
`#include "sb_*.h"`→`"k1_*.h"`, and all oracle hardcoded PATH strings (`"audio/sb_chord_detect.cpp"`
→ `"audio/k1_chord_detect.cpp"`, `MODULE_CPPS=["director/sb_smart_director.cpp",...]`, `FACADE_REL`,
`serial_replay_host_stubs.h`) — the path fixes ride the same sed since `\bsb_` matches inside the strings.
⚠ **PRESERVE** the persisted-format comment: `SB_PRESET_SLOTS_MAGIC = 0x53504253` — rename the MACRO NAME
(→`K1_PRESET_SLOTS_MAGIC`, the sed does this) but the numeric value `0x53504253` is untouched (it's a
number). The comment token `'SBPS'` (the on-disk bytes) → the `\bSB[A-Z]` sed would corrupt it to `'K1PS'`;
RESTORE it to `'SBPS'` (it documents the actual persisted bytes, which are unchanged). Optionally append
`// (bytes still spell 'SBPS' — on-disk compat with saved preset files)`.

### STEP 3 — platformio.ini
- `s/-DSB_/-DK1_/g` (the `-D` blocks `\bSB_` from matching flags — this line is REQUIRED and separate).
- `s/\bSB_/K1_/g` for any standalone SB_ (comments, non-flag). Double-k1 already collapsed in STEP 0.
- **Build-filter fixes (the EdgeMixer trap, at scale):**
  - line 44 base filter: `+<control/sb_*.cpp>` → `+<control/k1_*.cpp>`; DELETE now-dead `+<audio/sb_*.cpp>`
    and `+<director/sb_*.cpp>` (audio/ + director/ already carry `+<*/k1_*.cpp>`).
  - line ~358 (wireless bench env): `+<network/sb_*.cpp>` → **`+<network/k1_wireless.cpp>`** (EXPLICIT —
    a `k1_*` glob would wrongly drag in `k1_ble_midi_decoder.cpp`).
- The ~25 `-DSB_*` feature flags (SB_TEMPO_CONF_V2, SB_TEMPO_FLYWHEEL_V2, SB_ONSET_V2, SB_CHORD_V2,
  SB_SEMANTIC_STATE, SB_AGC_PERBAND_V1, SB_PEAK_ASYM_ENV, SB_VIVID_PRECOMP_V1, SB_TEMPO_ACF_SPREAD_V1,
  SB_TEMPO_NOVELTY_DECIMATION, SB_LED_TASK_CORE, SB_I2S_DMA_DESC_NUM_VALUE, SB_TEMPO_AP_FRAME_HZ,
  SB_ALLOW_AP_VP_SAME_CORE_FOR_PROBE, SB_DROP_CUT_V1, SB_CHORD_HUE_V1, …) get renamed by STEP 2 (source
  `#ifdef`) AND STEP 3 (`-D` flag) — the atomicity is guaranteed because BOTH passes run before any build.

### STEP 4 — goldens + manifest
After all renames: re-run every oracle that host-compiles renamed source; `diff` vs committed golden;
for any that changed → `python3 scripts/regression-harness/golden/oracle_NAME.py > tests/golden/NAME.golden.jsonl`;
then recompute the changed lines in `tests/golden/MANIFEST.sha256` (`shasum -a 256 <f>`).
`serial_struct.golden.jsonl` WILL drift (serialised field names). Oracles with hardcoded paths (chord,
onset_beat, serial_replay, ble_midi_map, smart_director, tempo) had their path strings fixed in STEP 2 —
confirm they no longer FileNotFoundError. Run `harness_selftest.py` → Gate F-alpha must stay all-[PASS].

## VERIFICATION (the safety net — the gate does NOT catch a silent #ifdef disable)
1. **0-RESIDUAL grep (the key guarantee + the complete-migration acceptance test):**
   `grep -rniE "sensory.?bridge|\bsb_|\bSB_|\bSB[A-Z]" SPECTRASYNQ_K1_FIRMWARE scripts tests platformio.ini`
   — must be EMPTY except the FLAGGED items (persisted-magic value/comment; forensic snapshots if kept
   archived). Zero residual ⇒ (a) Sensory Bridge fully eliminated per the mandate, AND (b) every `#ifdef`
   flag matches its `-D` ⇒ NO feature silently disabled (protects the celebrated beat-tracking).
2. **Gate:** commit → the repo pre-commit gate runs `pytest tests/ -q` (667/1) + `pio run -e k1_hardware`
   (SUCCESS). Also build the wireless-bench/harness envs that exercise control/ + network/ globs
   (`pio run -e <that env>`) to prove TRAP #2 is fixed. Do NOT `--no-verify`.
3. Report to team-lead for the **hardware boot smoke + optional beat-lock audio check** (team-lead owns
   the bench flash, MAC-gated).

## COMMIT
One coherent commit (or a small logical few) on `feat/edgemixer-on-im73d`:
`refactor(fork): de-SB → K1 naming across audio/beat/director/control/network (fork-wide, zero behaviour change)`
Message must note: file mv list, build-filter fixes (control/ + network/), the persisted-magic value
preserved, beat_aware_director + docs left as separate scope, and the 0-residual verification result.

## FLAGGED DECISIONS (the only 2 places "eliminate ALL SB" collides with a constraint — Captain's call)
1. **Persisted preset magic `SB_PRESET_SLOTS_MAGIC = 0x53504253` (='SBPS' on disk).** The MACRO renames
   free (→`K1_PRESET_SLOTS_MAGIC`), but the 4 bytes written into saved preset files spell 'SBPS'. Striking
   SB from DISK = a format migration (new K1 magic + version bump + read-old/write-new). DEFAULT: rename
   the macro, KEEP the byte value (SB gone from all code; 4 legacy bytes remain only inside already-saved
   files) — zero data loss. Full 'K1PS' migration is a tracked follow-up unless Captain wants it in-pass.
2. **`docs/forensics/**` SB-named snapshots** (historical evidence of the SB-origin code). DEFAULT: archive
   with a provenance note rather than strike — eliminates SB from the LIVE tree, preserves history. Captain
   may instead order a full strike.

## RECON SOURCES (detail)
Beat-tracking full symbol inventory: `scratchpad/beat_tracking_desb_symbol_inventory.md` (recon-beat).
Director/control/wireless + infra/build/golden maps: recon-director + recon-infra reports (team-lead has them).
