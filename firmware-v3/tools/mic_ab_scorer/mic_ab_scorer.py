#!/usr/bin/env python3
"""
mic_ab_scorer — the verification oracle for the IM73D122-vs-SPH0645 K1 mic A/B.

Doctrine: the harness IS the product (autonomous-agentic-build). It must be
FAULT-EVIDENT before any capture lane runs — see test_scorer_fault_evident.py.
This module scores DEVICE SERIAL telemetry against ground-truth beat annotations
and computes the mic-A/B metric families + paired statistics.

HARD FRAMING RULE (D1 CLOSED): every result is Unit-Bench vs Unit-Main. This module
will REFUSE to emit "IM73D122 vs SPH0645" model-level language (see frame_label()).

Channel: SERIAL only (D5 CLOSED — "AP" = Audio Processing, not WiFi). DUT = 12.8 kHz /
hop 96 / 133.33 Hz frame rate. Corpus layout (D4 VERIFIED): <root>/clips/*.wav +
<root>/annotations/*.beats, manifest.json bare filenames, .beats = one beat-time (s)/line.

Pure stdlib. mir_eval is used for CMLt/AMLt/P-score/info-gain IF installed (optional);
the decision-critical metrics (beat P/R/F, BPM acc-1/2, time-to-lock, %locked,
phase-error, octave-error) are implemented here and always available.
"""
from __future__ import annotations
import json, math, os, re, statistics, random, argparse, sys
from dataclasses import dataclass, field
from typing import Optional

DUT_FRAME_HZ = 133.333            # 12800 / 96 (VERIFIED config_types.h:37/41)
BEAT_WINDOW_S = 0.070             # mir_eval beat F-measure default (±70 ms)
BPM_TOL = 0.04                    # mir_eval tempo accuracy default (±4%)
OCTAVE_FACTORS = (1/3, 0.5, 1.0, 2.0, 3.0)

# ----------------------------------------------------------------------------- I/O
def load_beats(path: str) -> list[float]:
    """One beat timestamp (seconds) per line. Blank lines ignored."""
    out = []
    with open(path) as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            out.append(float(s))
    return sorted(out)

def load_corpus(root: str) -> list[dict]:
    """Resolve the D4-verified layout: clips/<clip_file> + annotations/<annotation_file>."""
    man = json.load(open(os.path.join(root, "manifest.json")))
    tracks = man["tracks"] if isinstance(man, dict) else man
    resolved = []
    for t in tracks:
        wav = os.path.join(root, "clips", t["clip_file"])
        beats = os.path.join(root, "annotations", t["annotation_file"])
        if not (os.path.exists(wav) and os.path.exists(beats)):
            raise FileNotFoundError(f"corpus pair missing for {t.get('id')}: {wav} / {beats}")
        resolved.append({**t, "_wav": wav, "_beats": beats})
    return resolved

# --------------------------------------------------------------- serial frame parser
_KV = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=(-?\d+(?:\.\d+)?)")
def parse_frames(lines) -> list[dict]:
    """Parse REAL device telemetry frames (formats VERIFIED on hardware 2026-07-10). Handles:
      slow [AP] poll (~1.4 Hz):
        '[AP] SSL=179 DC=-2 max_raw=659 follower=1012 peak_scaled=0.537 ... silence=0
         cal_source=persisted_profile cal_valid=1 ... | bpm=107.0 conf=0.33 lock=0
         phase=0.51 beat=0 bstr=0.82 | onset=1 bass=0 ostr=0.00'
      fast TEMPO stream (~20 Hz, k1_tempo_probe / ENABLE_TEMPO_STREAM build):
        'TEMPO,t=<ms>,bpm=..,phase=..,conf=..,beat=..,lock=..,str=..'
    Extracts one normalised frame per tempo-bearing line. Non-numeric key=value tokens
    (cal_source=persisted_profile, cal_reason=none) are ignored by design.
    RATE CONTRACT: the [AP] poll supports tempo-VALUE metrics (bpm/lock/conf/time-to-lock/
    %-locked); beat-time F-measure/CMLt need the fast TEMPO stream's beat_tick edges."""
    frames, skipped = [], 0
    for line in lines:
        s = line.strip()
        if not (s.startswith("[AP]") or s.startswith("TEMPO,")):
            continue
        kv = {k: float(v) for k, v in _KV.findall(s)}
        if "bpm" not in kv:
            skipped += 1
            continue
        frames.append({
            "bpm": kv["bpm"], "conf": kv.get("conf", 0.0),
            "locked": int(kv.get("lock", 0)), "phase01": kv.get("phase", 0.0),
            "beat_tick": int(kv.get("beat", 0)),
            "strength": kv.get("bstr", kv.get("str", 0.0)),
            "onset": int(kv.get("onset", 0)),
            "t_ms": kv.get("t"),                        # present only on TEMPO frames
            "peak_scaled": kv.get("peak_scaled"), "max_raw": kv.get("max_raw"),
            "dc": kv.get("DC"), "ssl": kv.get("SSL"), "silence": int(kv.get("silence", 0)),
            "src": "TEMPO" if s.startswith("TEMPO,") else "AP",
        })
    parse_frames.skipped = skipped
    return frames

parse_tempo_stream = parse_frames   # backward-compatible alias

def frames_to_beat_times(frames, frame_hz: float = DUT_FRAME_HZ) -> list[float]:
    """Reconstruct device beat-onset times (s) from beat_tick edges. Uses t_ms if present,
    else the hop index / frame_hz."""
    times = []
    for i, fr in enumerate(frames):
        if fr["beat_tick"]:
            t = (fr["t_ms"] / 1000.0) if fr.get("t_ms") is not None else (i / frame_hz)
            times.append(t)
    return times

# ------------------------------------------------------------------ beat/tempo metrics
def beat_prf(ref: list[float], est: list[float], window: float = BEAT_WINDOW_S) -> dict:
    """mir_eval-style greedy one-to-one beat matching within ±window. Returns P/R/F."""
    if not ref and not est:
        return {"P": 1.0, "R": 1.0, "F": 1.0, "n_ref": 0, "n_est": 0, "matched": 0}
    if not ref or not est:
        return {"P": 0.0, "R": 0.0, "F": 0.0, "n_ref": len(ref), "n_est": len(est), "matched": 0}
    used_est = [False] * len(est)
    matched = 0
    j0 = 0
    for r in ref:
        best, best_d = -1, window + 1e-9
        j = j0
        while j < len(est) and est[j] < r - window:
            j += 1
        j0 = j
        while j < len(est) and est[j] <= r + window:
            if not used_est[j]:
                d = abs(est[j] - r)
                if d < best_d:
                    best, best_d = j, d
            j += 1
        if best >= 0:
            used_est[best] = True
            matched += 1
    P = matched / len(est)
    R = matched / len(ref)
    F = (2 * P * R / (P + R)) if (P + R) > 0 else 0.0
    return {"P": P, "R": R, "F": F, "n_ref": len(ref), "n_est": len(est), "matched": matched}

def bpm_accuracy(ref_bpm: float, est_bpm: float, tol: float = BPM_TOL) -> dict:
    """acc-1 (within tol of ref) and acc-2 (within tol of any octave multiple)."""
    if ref_bpm <= 0 or est_bpm <= 0:
        return {"acc1": 0, "acc2": 0, "rel_err": None, "octave_error": 0}
    rel = abs(est_bpm - ref_bpm) / ref_bpm
    acc1 = 1 if rel <= tol else 0
    acc2 = 0
    octave_hit = None
    for fmac in OCTAVE_FACTORS:
        if abs(est_bpm - ref_bpm * fmac) / (ref_bpm * fmac) <= tol:
            acc2 = 1
            octave_hit = fmac
            break
    octave_error = 1 if (acc2 and not acc1) else 0
    return {"acc1": acc1, "acc2": acc2, "rel_err": rel, "octave_error": octave_error,
            "octave_factor": octave_hit}

def time_to_lock(frames, ref_bpm: float, tol: float = BPM_TOL,
                 hold_frames: int = 3, frame_hz: float = DUT_FRAME_HZ) -> Optional[float]:
    """Seconds to first sustained (>=hold_frames) window within tempo tolerance."""
    run = 0
    for i, fr in enumerate(frames):
        ok = fr["locked"] and ref_bpm > 0 and abs(fr["bpm"] - ref_bpm) / ref_bpm <= tol
        run = run + 1 if ok else 0
        if run >= hold_frames:
            return (i - hold_frames + 1) / frame_hz
    return None

def pct_locked_steady(frames, ref_bpm, t_conv_s: float, tol=BPM_TOL, frame_hz=DUT_FRAME_HZ) -> Optional[float]:
    """% of POST-convergence frames within tempo tolerance (steady-state only)."""
    start = int(math.ceil(t_conv_s * frame_hz)) if t_conv_s is not None else None
    if start is None or start >= len(frames):
        return None
    steady = frames[start:]
    if not steady:
        return None
    good = sum(1 for fr in steady
               if fr["locked"] and ref_bpm > 0 and abs(fr["bpm"] - ref_bpm) / ref_bpm <= tol)
    return good / len(steady)

# ------------------------------------------------------------------------- metrology
def snr_db(signal_rms: float, noise_rms: float) -> float:
    """Noise-subtracted SNR (power). P = rms^2. Guards P_signal<=P_noise."""
    ps, pn = signal_rms ** 2, noise_rms ** 2
    if pn <= 0:
        return float("inf")
    if ps <= pn:
        return 0.0
    return 10.0 * math.log10((ps - pn) / pn)

def dbfs(rms: float, full_scale: float = 1.0) -> float:
    if rms <= 0:
        return -float("inf")
    return 20.0 * math.log10(rms / full_scale)

def noise_floor_rms(silence_series: list[float]) -> float:
    return statistics.median(silence_series) if silence_series else 0.0

# ---------------------------------------------------------------------------- statistics
def cohens_dz(diffs: list[float]) -> Optional[float]:
    if len(diffs) < 2:
        return None
    sd = statistics.pstdev(diffs)
    return (statistics.fmean(diffs) / sd) if sd > 0 else float("inf")

def bootstrap_ci(diffs: list[float], n: int = 2000, alpha: float = 0.05,
                 seed: int = 1) -> tuple[float, float]:
    if not diffs:
        return (float("nan"), float("nan"))
    rng = random.Random(seed)
    k = len(diffs)
    means = []
    for _ in range(n):
        means.append(statistics.fmean(diffs[rng.randrange(k)] for _ in range(k)))
    means.sort()
    lo = means[int((alpha / 2) * n)]
    hi = means[min(n - 1, int((1 - alpha / 2) * n))]
    return (lo, hi)

def wilcoxon_signed_rank(diffs: list[float]) -> Optional[float]:
    """Two-sided p-value, normal approximation with continuity correction. Zeros dropped."""
    nz = [d for d in diffs if d != 0]
    n = len(nz)
    if n < 6:
        return None  # too few for the normal approximation; report descriptive only
    ranks = _avg_ranks([abs(d) for d in nz])
    w_plus = sum(r for d, r in zip(nz, ranks) if d > 0)
    mean_w = n * (n + 1) / 4
    sd_w = math.sqrt(n * (n + 1) * (2 * n + 1) / 24)
    if sd_w == 0:
        return None
    z = (abs(w_plus - mean_w) - 0.5) / sd_w
    return 2 * (1 - _norm_cdf(z))

def benjamini_hochberg(pvals: dict[str, float], q: float = 0.05) -> dict[str, bool]:
    """Return {metric: significant_after_BH}. None p-values are treated as not-significant."""
    items = [(k, v) for k, v in pvals.items() if v is not None]
    m = len(items)
    if m == 0:
        return {k: False for k in pvals}
    items.sort(key=lambda kv: kv[1])
    sig = {k: False for k in pvals}
    max_i = -1
    for i, (k, p) in enumerate(items, start=1):
        if p <= (i / m) * q:
            max_i = i
    for i, (k, p) in enumerate(items, start=1):
        if i <= max_i:
            sig[k] = True
    return sig

def _avg_ranks(vals):
    order = sorted(range(len(vals)), key=lambda i: vals[i])
    ranks = [0.0] * len(vals)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and vals[order[j + 1]] == vals[order[i]]:
            j += 1
        avg = (i + j) / 2 + 1
        for k in range(i, j + 1):
            ranks[order[k]] = avg
        i = j + 1
    return ranks

def _norm_cdf(z):
    return 0.5 * (1 + math.erf(z / math.sqrt(2)))

# --------------------------------------------------------------------------- framing
def frame_label(unit: str) -> str:
    """Enforce unit-pair framing (D1). Reject any mic-model label."""
    u = unit.strip().lower()
    if u in ("bench", "unit-bench", "im73d", "im73d122"):
        return "Unit-Bench(IM73D122)"
    if u in ("main", "unit-main", "sph0645", "sph"):
        return "Unit-Main(SPH0645)"
    raise ValueError(f"unit must be bench/main (unit-pair framing per D1); got {unit!r}")

MODEL_CLAIM_BANNED = re.compile(r"IM73D122\s+(is|beats|outperforms|better)\b", re.I)
def guard_no_model_claim(text: str) -> str:
    if MODEL_CLAIM_BANNED.search(text):
        raise ValueError("D1 violation: model-level claim from a 2-unit study is prohibited")
    return text


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="mic A/B scorer (serial telemetry vs .beats)")
    ap.add_argument("--corpus", required=True, help="hybrid-beat-tracker/tests/benchmark root")
    ap.add_argument("--selftest", action="store_true", help="run the fault-evident self-test")
    args = ap.parse_args()
    if args.selftest:
        import test_scorer_fault_evident as t
        sys.exit(t.run())
    tracks = load_corpus(args.corpus)
    print(f"corpus OK: {len(tracks)} tracks resolved (clips/ + annotations/).")
