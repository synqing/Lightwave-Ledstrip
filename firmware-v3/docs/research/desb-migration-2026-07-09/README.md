---
abstract: "Reproducible tooling + evidence for the 2026-07-09 fork-wide Sensory-Bridge→K1 migration of the SpectraSynq_K1_Firmware repo (4,800 refs / 168 files, zero behaviour change). desb_verify.py is the canonical acceptance test — use it, NOT grep. Read before touching SB→K1 naming or re-running the migration."
---

# Fork-wide de-SB → K1 migration — tooling + evidence (2026-07-09)

Applied to `SpectraSynq_K1_Firmware` (worktree `/private/tmp/k1_edgemixer_on_im73d`), banked as
`wip/fork-desb-complete @ 1dfc9b0` off `feat/edgemixer-on-im73d @ 0d83729`. Host gate green:
0-residual + 667 pytest + `pio run -e k1_hardware` SUCCESS. Full narrative: the ✅ UPDATE block
at the top of `../edgemixer_oklab_stm_vp_handover_2026-07-08.md`.

## Files
| File | What |
|------|------|
| `desb_verify.py` | **The canonical acceptance test.** Python `re` — catches `sb_`, `SB_`, `SB[caps]`, `-[Dd]sb` flags, and `sensory bridge` prose; sanctions 3 residual classes (preset magic bytes, external real path `SensoryBridge-main 9`, `assertNotIn("SB_…")` legacy guards). `python3 desb_verify.py <worktree>` → `ACCEPTANCE: PASS (0 residual)`. |
| `migrate.py` | The content transform (ordered word-boundary subs + `-[Dd]` flag lookbehind + magic/path protection + branding sweep). Idempotent, reversible. Run AFTER the `git mv` phase. |
| `regen_golden.py` | Re-bless drifted goldens; flags name-only vs value drift. |
| `desb_before.txt` / `desb_after2.txt` | Verifier output before (4,800 residual) and after (0). |

## Three traps this migration hit (do NOT repeat)
1. **grep is a false-pass generator here.** The master plan's `grep -rniE "…\bsb_…" --include=*.cpp` under **zsh** filename-expands the unquoted `--include` globs and searches nothing → prints "0 residual" on a fully-SB tree. Use `desb_verify.py`.
2. **`-DSB_` flags evade `\bSB_`.** The `-D` glues a word char before `SB`, so a `\b`-anchored rename misses compiler-define flags — source `#ifdef`s rename to `K1_` while the `-D` flags stay `SB_`, silently compiling out the beat-tracking with a green build. Rename `-[Dd]sb_` explicitly; the 0-residual test (which now sees `-[Dd]sb`) is the guarantee.
3. **Don't rename `assertNotIn("SB_…")` legacy guards.** Those negative assertions PROVE the old prefix is gone — their SB token must stay SB.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-09 | agent:opus-4.8 | Created — tooling + evidence for the fork-wide de-SB→K1 migration. |
