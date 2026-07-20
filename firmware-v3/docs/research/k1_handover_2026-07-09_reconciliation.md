---
abstract: "Handover after the K1 firmware EdgeMixer → fork-wide de-SB → origin/main divergence reconciliation (2026-07-09). Everything LANDED on origin/main @ 0a518e5 (green: 697 pass + k1_hardware build; 26 origin commits preserved via fast-forward). Read for: current shipped state, repo/worktree/device map, outstanding cleanup + 3 Captain-deferred calls, the de-SB tooling + its 3 traps, and the next-milestone question. Firmware lives in the SEPARATE repo github.com/synqing/SpectraSynq_K1_Firmware, NOT this Lightwave-Ledstrip repo."
---

# K1 Firmware — Reconciliation Handover (2026-07-09, ~23:00 GMT+8)

## TL;DR — it's done and shipped
`origin/main @ 0a518e5` (github.com/synqing/**SpectraSynq_K1_Firmware**) is **reconciled, green, and pushed**: **697 passed / 1 skipped / 0 failed** + `pio run -e k1_hardware` **SUCCESS**. It now contains, on one branch, everything that was split across a 150-commit divergence — with **all 26 of Captain's PR-reviewed commits preserved** (landed as a clean fast-forward, not a force-push).

## What just happened (the arc)
1. **EdgeMixer superiority programme** — OKLab/split/masked colour differentiator, enable-by-default at BALANCED intensity. COMPLETE, gate-2 passed (Captain plate verdict), hardware-attested.
2. **Fork-wide de-SB → K1 rename** — every Sensory Bridge reference (identifiers AND prose) struck to K1/k1. ~4,800 refs. Hardware-attested (beat-locked Avicii @ 125 BPM, conf 0.93 on bench B489A500 with live IM73D mic).
3. **origin/main divergence** — turned out to be **base-drift**: our 93-commit work sat on a June-26 base while `main` advanced via PRs #14–#22 (N2/N2b/N3/N4a hardening). A 4-SSA audit proved: feat already re-did N2/N2b/N3 (no production-safety regression); only N4a + build-config + commit-gate were genuinely missing (dev/CI only); **origin/main itself shipped 2 pre-existing red tests** (forward-refs to feat-only work); the de-SB rename is regenerable machinery.
4. **Reconciliation (rebuild-forward)** — landed on `origin/main`:
   - **`2142220`** de-SB rename of origin/main (rename-first → both sides `k1_` → no collision storm), via the **hardened `migrate.py`**.
   - **`a152efd`** 3-way merge of feat's features onto it; 24 conflicts resolved (feat's validated source wins, origin's N4a guard kept, `k1_edgemixer_lite`→`k1_edgemixer` per no-lite, platformio = feat superset). Correct because merge-base predates N4a → origin's dev-lanes preserved as added-on-one-side.
   - **`0a518e5`** fixups: populated the N4a manifest `k1_device_identities.json` with the full 49-env set (**this cleared origin's 2 pre-existing reds** + registered feat's im73d/custom envs), pointed 3 feat tests at the manifest.

## Repo / worktree / device map (VERIFY before acting)
- **Firmware repo:** `~/SpectraSynq_K1_Firmware` (git store), github.com/synqing/SpectraSynq_K1_Firmware. **This is where firmware work happens — NOT the Lightwave-Ledstrip repo** (which holds these handover docs + the de-SB tooling).
- **Worktrees (in /tmp — ephemeral, but commits are durable in the home-dir store):**
  - `/private/tmp/k1_rebuild_main` → branch `rebuild/desb-main` @ `0a518e5` (= what's on origin/main).
  - `/private/tmp/k1_edgemixer_on_im73d` → branch `feat/edgemixer-on-im73d` @ `565b95f` (superseded; landed).
- **Stale/prune candidates:** local `main` @ `57f6236` (update to 0a518e5); `feat/edgemixer-on-im73d`, `wip/fork-desb-complete @ 1dfc9b0`, `rebuild/desb-main` (all landed → prunable).
- **Devices (two K1s on the bus — MAC-VERIFY before ANY flash, never trust the port name):**
  - **Bench B489A500** = MAC `b4:3a:45:a5:89:b4`, currently `usbmodem1401`. Physically has the **IM73D122 PDM mic** (SPH0645 was removed for the eval — an SPH0645 build reads silence). Currently flashed with a de-SB IM73D diagnostic build (temp env `k1_bench_im73d_ap_probe`, which was reverted from the repo — the flashed firmware persists but the env def is gone).
  - **Main F887A500** = MAC `b4:3a:45:a5:87:f8`, currently `usbmodem12401`. Untouched.
  - MAC-read (non-destructive): `~/.platformio/penv/bin/python -m esptool --chip esp32s3 --port <cu.port> read_mac`.
  - **N4a upload guard is live** — `pio run -e <env> -t upload --upload-port <port>` refuses a wrong device/env pairing via `scripts/platformio/k1_device_identities.json`.

## Outstanding (the "next" list)
**Cheap cleanup (agent-resolvable, do on request):**
- Prune the 3 landed branches; fast-forward local `main` to `0a518e5`.
- Reflash the bench off the diagnostic build to a canonical env (e.g. `k1_bench_im73d` or `k1_bench_reference` — mind the IM73D mic: only `k1_bench_im73d*` envs drive it).
- Dismiss the idle recon SSAs.

**Captain-deferred calls (defaulted; reversible; surface, don't force):**
- **Preset magic** — kept `'SBPS'` on-disk bytes (macro is `K1_PRESET_SLOTS_MAGIC = 0x53504253`). Zero data loss. Migrate to `'K1PS'` = format-version bump + read-old/write-new.
- **`bridge_fs.h`** — persistence filename left as-is (deep codec + golden coupling). Rename is its own task.
- **Phase-2 de-SB** — CODE is 100% `k1_`; `docs/` + `AGENTS.md` **prose** still say "Sensory Bridge". Deferred per the mandate's own sequencing. Sweep is safe (prose only).

**Optional quality gate:** fresh eyes-on smoke of the EXACT landed build (covered by equivalence: DSP goldens unchanged + build links + already-attested at 125 BPM).

**The real next milestone is Captain's to name** — this whole arc (EdgeMixer → de-SB → reconciliation) is closed. K1 default workstream is K1 → FE → Kickstarter → mass adoption. Pull `BACKLOG.md` / launch plan for the next target.

## De-SB tooling + the 3 traps (canonical, reusable)
Location: `firmware-v3/docs/research/desb-migration-2026-07-09/` — `migrate.py` (the transform, NOW HARDENED with `assertNotIn` guard protection), `desb_verify.py` (the 0-residual acceptance test — use THIS, not grep), `regen_golden.py`, `README.md`.
Traps the tooling encodes (each would ship past a green gate): (1) **zsh grep false-passes** — unquoted `--include=*.cpp` globs expand → searches nothing → "0 residual" on a full-SB tree; use the Python verifier. (2) **`-D[Dd]sb_` flags evade `\bSB_`** — the `-D` glues a word-char before SB → flags stay SB while source `#ifdef`s rename → beat-tracking silently compiled out with a green build. (3) **`assertNotIn("SB_…")` legacy guards** — the SB token must STAY SB (it's the forbidden token); now protected in migrate.py + sanctioned in desb_verify.

## Standing constraints (load-bearing)
no-lite (K1 equals-or-exceeds, never stripped); **K1/k1 ONLY — zero Sensory Bridge in code** (prose Phase-2); hardware-test-before-commit; MAC-verify before flash; British English (guard hook blocks US spellings); commit/push only when asked (Captain gave full autonomy for this reconciliation); the repo pre-commit gate = `scripts/hooks/pre-commit` (framework-safety + `pytest tests/ -q` ≥1-passed-guard + `pio run -e k1_hardware`); serial-open resets the board; single-char serial hotkeys (`a` toggles `[AP]` telemetry → `bpm/conf/lock`); RTK corrupts `edge`→`in` in grep stdout (use Read/Python); audio-safety = approved corpus (Avicii-Levels / deadmau5-Ghosts'n'Stuff in ~/Downloads + hybrid-beat-tracker/tests/benchmark) @ ~30%, state params before playback.

## Full detail
Prior programme record: `firmware-v3/docs/research/edgemixer_oklab_stm_vp_handover_2026-07-08.md` (reconciliation update block on top). Session transcript: `5e36156a-6939-4ce7-9d00-2691e3989d3a.jsonl`.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-09 | agent:opus-4.8 | Created — handover after the origin/main divergence reconciliation landed on 0a518e5. |
