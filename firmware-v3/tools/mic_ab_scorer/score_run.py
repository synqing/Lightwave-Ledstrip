#!/usr/bin/env python3
"""Quick-look per-capture reader for one capture_serial.py log (the fast read during the
bench loop; full paired battery stats come after N trials). Parses [AP]/TEMPO frames and
prints the mic-A/B-relevant series + summary.
  P0 tone:   --spl <dBSPL>   -> dBFS + gain-offset readout
  P4 corpus: --ref-bpm <bpm> -> BPM accuracy-1/2 + lock-reached
"""
import argparse, statistics
import mic_ab_scorer as S

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--spl", type=float, help="measured SPL (dB) during a P0 tone capture")
    ap.add_argument("--ref-bpm", type=float, help="ground-truth BPM for a P4 corpus capture")
    a = ap.parse_args()
    fr = S.parse_frames(open(a.log).read().splitlines())
    if not fr:
        print("NO tempo-bearing frames parsed — check the capture / build / port."); return
    bpm = [f["bpm"] for f in fr]; conf = [f["conf"] for f in fr]
    lock = [f["locked"] for f in fr]
    ps = [f["peak_scaled"] for f in fr if f["peak_scaled"] is not None]
    dc = sorted({f["dc"] for f in fr if f["dc"] is not None})
    n_tempo = sum(1 for f in fr if f["src"] == "TEMPO")
    print(f"frames={len(fr)} (TEMPO={n_tempo}, [AP]={len(fr)-n_tempo}) | DC-signature={dc}")
    print(f"bpm    : median={statistics.median(bpm):6.1f}  range {min(bpm):.0f}-{max(bpm):.0f}")
    print(f"conf   : median={statistics.median(conf):6.2f}  max={max(conf):.2f}  "
          f"| locked {sum(lock)}/{len(lock)} frames")
    if ps:
        print(f"peak_sc: median={statistics.median(ps):6.3f}  max={max(ps):.3f}")
    if a.spl is not None and ps:
        level = statistics.median(ps)
        print(f"\nP0  SPL={a.spl} dB | median peak_scaled={level:.3f} | dBFS≈{S.dbfs(level):.1f} "
              f"| record this (unit gain offset = protocol target_dBFS − this dBFS).")
    if a.ref_bpm is not None:
        acc = [S.bpm_accuracy(a.ref_bpm, b) for b in bpm]
        a1 = sum(x["acc1"] for x in acc) / len(acc)
        a2 = sum(x["acc2"] for x in acc) / len(acc)
        oct_ = sum(x["octave_error"] for x in acc) / len(acc)
        print(f"\nP4  ref_bpm={a.ref_bpm} | acc-1={a1:.2f}  acc-2(oct-tol)={a2:.2f}  "
              f"octave-err={oct_:.2f} | lock reached: {'YES' if any(lock) else 'NO'}")
        if not any(lock):
            print("    NOTE: no lock — expected if the clip is short / quiet / mid-convergence. "
                  "Longer capture or louder SPL if the room allows.")

if __name__ == "__main__":
    main()
