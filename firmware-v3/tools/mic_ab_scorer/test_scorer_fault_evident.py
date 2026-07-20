#!/usr/bin/env python3
"""
Gate-0 fault-evident self-test for mic_ab_scorer (autonomous-agentic-build doctrine).

The harness is only trustworthy if it PROVABLY DETECTS faults. This injects a battery
of known-bad device outputs and asserts the scorer flags each one. If any injected
fault is NOT detected, the harness is RED and must not be used on real captures.

No hardware. No audio playback. Synthetic device output only.
Run: python3 test_scorer_fault_evident.py   (exit 0 = harness fault-evident)
"""
from __future__ import annotations
import math
import mic_ab_scorer as S

PASS, FAIL = "PASS", "FAIL"
results = []

def check(name, condition, detail=""):
    results.append((name, PASS if condition else FAIL, detail))

def mk_frames(bpm, n, beat_period_frames, locked=1, conf=0.9, start_lock_frame=0):
    """Synthesize a TEMPO stream: a beat_tick every beat_period_frames, lock after start."""
    frames = []
    for i in range(n):
        frames.append({
            "bpm": bpm, "phase01": (i % beat_period_frames) / beat_period_frames,
            "conf": conf, "beat_tick": 1 if (i % beat_period_frames == 0) else 0,
            "locked": 1 if (locked and i >= start_lock_frame) else 0,
            "strength": 1.0, "t_ms": i / S.DUT_FRAME_HZ * 1000.0,
        })
    return frames

def run() -> int:
    # ---- ground truth: 120 BPM = beat every 0.5 s, 20 beats over 10 s ----
    ref = [i * 0.5 for i in range(20)]

    # F1 PERFECT: est == ref -> F ~ 1.0 (harness must reward a correct result)
    est_perfect = list(ref)
    r = S.beat_prf(ref, est_perfect)
    check("perfect_beats_score_high", r["F"] > 0.99, f"F={r['F']:.3f}")

    # F2 SHIFTED: all beats +150 ms -> outside ±70 ms -> F must collapse
    est_shift = [b + 0.150 for b in ref]
    r = S.beat_prf(ref, est_shift)
    check("shifted_beats_detected", r["F"] < 0.2, f"F={r['F']:.3f} (must be low)")

    # F3 RANDOM: garbage estimates -> F near 0
    est_rand = [0.013, 0.29, 0.71, 1.13, 3.9, 7.7, 8.05, 9.61]
    r = S.beat_prf(ref, est_rand)
    check("random_beats_detected", r["F"] < 0.35, f"F={r['F']:.3f}")

    # F4 BLANK (the load-bearing anti-pattern): empty est must score 0, NOT crash/pass
    r = S.beat_prf(ref, [])
    check("blank_est_scores_zero_not_pass", r["F"] == 0.0 and r["n_est"] == 0, f"F={r['F']}")

    # F5 BLANK-vs-BLANK edge: both empty is a defined case (no signal both sides)
    r = S.beat_prf([], [])
    check("blank_vs_blank_defined", r["F"] == 1.0, "both empty = trivially matched (documented)")

    # F6 OCTAVE ERROR: est BPM = 2x ref -> acc1 fails, acc2 passes, octave_error flagged
    a = S.bpm_accuracy(120.0, 240.0)
    check("octave_error_flagged", a["acc1"] == 0 and a["acc2"] == 1 and a["octave_error"] == 1,
          f"acc1={a['acc1']} acc2={a['acc2']} oct={a['octave_error']}")

    # F7 BPM CORRECT: within tol -> acc1 passes
    a = S.bpm_accuracy(120.0, 121.0)
    check("correct_bpm_accepted", a["acc1"] == 1, f"rel_err={a['rel_err']:.4f}")

    # F8 BPM WRONG (non-octave): 120 vs 137 -> both acc fail
    a = S.bpm_accuracy(120.0, 137.0)
    check("wrong_bpm_rejected", a["acc1"] == 0 and a["acc2"] == 0, "137 vs 120")

    # F9 TIME-TO-LOCK: never locks -> None; locks at frame 40 -> ~0.30 s @133.33Hz
    frames_nolock = mk_frames(90.0, 200, beat_period_frames=89, locked=0)
    check("no_lock_detected", S.time_to_lock(frames_nolock, 120.0) is None, "unlocked stream")
    frames_lock = mk_frames(120.0, 400, beat_period_frames=67, locked=1, start_lock_frame=40)
    ttl = S.time_to_lock(frames_lock, 120.0)
    check("lock_time_measured", ttl is not None and 0.25 < ttl < 0.40, f"ttl={ttl}")

    # F10 STEADY-STATE EXCLUSION: %locked computed only after convergence window
    pct = S.pct_locked_steady(frames_lock, 120.0, t_conv_s=(ttl or 0.3))
    check("steady_pct_high_when_locked", pct is not None and pct > 0.95, f"pct={pct}")

    # F11 SNR metrology: signal==noise -> ~0 dB; signal 10x noise -> ~20 dB
    check("snr_zero_when_signal_eq_noise", abs(S.snr_db(0.1, 0.1) - 0.0) < 1e-9, "eq")
    hi = S.snr_db(1.0, 0.1)
    check("snr_high_when_signal_dominates", 19.5 < hi < 20.5, f"{hi:.2f} dB")

    # F12 STATS — identical pairs => NOT significant; clearly separated => significant.
    same = [0.0] * 12
    diff = [0.9, 1.1, 0.8, 1.2, 1.0, 0.95, 1.05, 0.85, 1.15, 0.9, 1.1, 1.0]
    p_same = S.wilcoxon_signed_rank(same)
    p_diff = S.wilcoxon_signed_rank(diff)
    check("null_effect_not_significant", (p_same is None) or (p_same > 0.05), f"p={p_same}")
    check("real_effect_significant", p_diff is not None and p_diff < 0.05, f"p={p_diff}")
    dz = S.cohens_dz(diff)
    check("effect_size_large_for_real_effect", dz is not None and dz > 0.8, f"dz={dz:.2f}")
    lo, hi = S.bootstrap_ci(diff)
    check("ci_excludes_zero_for_real_effect", lo > 0, f"CI=({lo:.2f},{hi:.2f})")

    # F13 BH-FDR: a basket with 1 tiny p among nulls -> only the real one survives
    sig = S.benjamini_hochberg({"m1": 0.001, "m2": 0.6, "m3": 0.7, "m4": 0.9})
    check("bh_keeps_real_drops_null", sig["m1"] and not sig["m2"], str(sig))

    # F14 D1 FRAMING GUARD: model-level claim must be rejected; unit labels accepted
    try:
        S.guard_no_model_claim("IM73D122 is better than SPH0645")
        check("model_claim_blocked", False, "guard did NOT raise")
    except ValueError:
        check("model_claim_blocked", True)
    ok_label = S.frame_label("bench") == "Unit-Bench(IM73D122)"
    check("unit_pair_label_enforced", ok_label)
    try:
        S.frame_label("nonsense"); check("bad_unit_rejected", False)
    except ValueError:
        check("bad_unit_rejected", True)

    # F15 SERIAL PARSER on REAL device frame formats (captured from hardware 2026-07-10).
    real_ap = ("[AP] SSL=179 DC=-2 max_raw=659 follower=1012 peak_scaled=0.537 "
               "response_gain=1.000 silent_scale=1.000 silence=0 cal_source=persisted_profile "
               "cal_valid=1 cal_reason=none | bpm=107.0 conf=0.33 lock=0 phase=0.51 beat=0 "
               "bstr=0.82 | onset=1 bass=0 ostr=0.00")
    real_ap_sph = ("[AP] SSL=215 DC=-4545 max_raw=475 follower=667 peak_scaled=0.387 "
                   "silence=0 cal_source=config | bpm=127.0 conf=0.54 lock=1 phase=0.92 "
                   "beat=1 bstr=0.08 | onset=0 bass=0 ostr=0.00")
    real_tempo = "TEMPO,t=12345,bpm=120.00,phase=0.500,conf=0.900,beat=1,lock=1,str=0.750"
    lines = [real_ap, "garbage line", "TEMPO,junk", real_tempo, real_ap_sph,
             "[VP] profile=custom fix=111011 chroma_seq=2145110"]
    fr = S.parse_frames(lines)
    check("parser_accepts_real_frames", len(fr) == 3, f"n={len(fr)} (2 AP + 1 TEMPO)")
    check("parser_reads_ap_tempo_fields",
          fr[0]["bpm"] == 107.0 and abs(fr[0]["conf"] - 0.33) < 1e-9 and fr[0]["locked"] == 0,
          f"bpm={fr[0]['bpm']}")
    check("parser_reads_tempo_beat_tick",
          fr[1]["beat_tick"] == 1 and fr[1]["locked"] == 1 and fr[1]["t_ms"] == 12345.0,
          f"beat={fr[1]['beat_tick']}")
    check("parser_extracts_frontend_dc_confound",
          fr[0]["dc"] == -2 and fr[2]["dc"] == -4545,
          f"IM73D DC={fr[0]['dc']} vs SPH0645 DC={fr[2]['dc']} (the gain confound)")
    check("parser_skips_junk_visibly", S.parse_frames.skipped == 1,
          f"skipped={S.parse_frames.skipped}")

    # ---- verdict ----
    n_fail = sum(1 for _, st, _ in results if st == FAIL)
    print("=" * 68)
    print("GATE-0 FAULT-EVIDENT SELF-TEST — mic_ab_scorer")
    print("=" * 68)
    for name, st, detail in results:
        print(f"  [{st}] {name}" + (f"  — {detail}" if detail else ""))
    print("-" * 68)
    total = len(results)
    print(f"  {total - n_fail}/{total} checks passed.")
    if n_fail == 0:
        print("  VERDICT: HARNESS IS FAULT-EVIDENT (GREEN). Cleared for capture scoring.")
    else:
        print(f"  VERDICT: RED — {n_fail} fault(s) NOT detected. DO NOT use on real data.")
    print("=" * 68)
    return 1 if n_fail else 0


if __name__ == "__main__":
    raise SystemExit(run())
