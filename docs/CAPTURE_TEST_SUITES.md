# Capture Test Suites Reference

> Operational test profiles for the LED capture pipeline. Each suite has fixed
> parameters, pass/fail gates, and a defined use case. Do not freestyle capture
> parameters — pick the right suite.

## Suite Definitions

| Suite | Tap | FPS | Format | Duration | When |
|-------|-----|-----|--------|----------|------|
| **Reference** | B | 30 | v2 | 20s | Every serious effect check |
| **Stress** | B | 40 | v2 | 15s | Before merge / milestone |
| **Isolation** | A, B, C | 30 | v2 | 10s each | Debugging colour/output path |
| **Soak** | B | 30 | v2 | 120s | Flagship effects, nightly, pre-release |

### Default capture mode

For almost everything: **B / 30 FPS / v2 / 20s** (Reference suite).

---

## Pass/Fail Gates

### Reference (B / 30 / v2 / 20s)

| Metric | Gate |
|--------|------|
| Actual FPS | At target (30.0 ± 0.5) |
| Frame drops | 0 |
| Show time p99 | No regression from baseline |
| Heap trend | Flat or bounded (< ±5 B/f) |
| Analyser metrics | Compare against stored baseline |

At 30 FPS with esp_timer pacing, this mode must be boringly clean.

### Stress (B / 40 / v2 / 15s)

| Metric | Gate |
|--------|------|
| Actual FPS | Near target (≥ 38) |
| Frame drops | 0 or < 0.5% |
| Show time p99 | Stable (< 8 ms) |
| Heap trend | No persistent creep |
| Parser errors | 0 suspect frames |

This is the canary — catches issues Reference misses.

### Isolation (A/B/C / 30 / v2 / 10s each)

Run the same effect, params, and audio across all three taps. Compare:

| Comparison | What it reveals |
|------------|-----------------|
| A vs B | Colour correction delta |
| B vs C | Pre-WS2812 output path delta |
| A alone | Whether the effect itself is wrong |

No automated gate — visual/analytical comparison only.

### Soak (B / 30 / v2 / 120s)

| Metric | Gate |
|--------|------|
| Heap trend | No ratchet (monotonic growth) |
| Show skips | No accumulation |
| FPS stability | No decay over time |
| Analyser drift | Metrics stable across 30s windows |

---

## Effect Classes

### Class 1 — Continuous / flowing

Examples: wave, breathing, caustics, interference motion.

Run: Reference + Stress + Soak (for flagship).

Key metrics: temporal smoothness, hue velocity, spatial spread, temporal
gradient energy, long-run stability.

### Class 2 — Beat / onset / transient-driven

Examples: BeatPulse family, beat-flash, pulse, onset-reactive triggers.

Run: Reference + Stress + event-sensitive audio clip. Optional Soak.

Key metrics: beat flag alignment, response latency, brightness delta around
beat windows, onset-trigger contrast, missed-trigger rate.

These benefit most from 40 FPS stress mode.

### Class 3 — Pipeline / colour / output verification

Use case: changing gamma, correction, palettes, output path, capture plumbing.

Run: Isolation + Reference.

Key metrics: A→B deltas, B→C deltas, brightness mapping, hue preservation,
clipping/compression artefacts.

---

## Audio Fixtures

Use a fixed clip pack for repeatability. Do not test against random music.

| Fixture | Purpose |
|---------|---------|
| Silence / near-silence | False triggers, idle stability, floor behaviour |
| Metronome / click track | Beat/onset timing truth |
| Steady 4/4 electronic beat | Regular pulse behaviour |
| Broadband / dense music | Stress spectral reactivity |
| Dynamic / sparse passage | Sensitivity to low-energy transitions |

For beat/transient effects: click track and steady 4/4 are mandatory.
For continuous effects: dense and dynamic clips matter more.

---

## Saved Outputs

For any run worth keeping, save all four:

| Output | Purpose |
|--------|---------|
| Raw session (.npz) | Ground truth data |
| Dashboard PNG | Metric summary |
| Waterfall PNG | Temporal pattern reading |
| Strip GIF/MP4 | Human sanity check |

Metrics without pictures can lie. Pictures without metrics can seduce.

---

## Automation Workflows

### Quick check (constant use)

One effect, one clip: **B / 30 / 20s**. Outputs dashboard + waterfall.

```bash
python tools/led_capture/capture_suite.py --serial PORT --suite reference
```

### Pre-merge / milestone

One effect or shortlist: Reference + Stress, compare against baseline.

```bash
python tools/led_capture/capture_suite.py --serial PORT --suite reference,stress
```

### Nightly / release validation

Flagship effects: Soak + selected Isolation runs.

```bash
python tools/led_capture/capture_suite.py --serial PORT --suite soak
```

---

*Last updated: 2026-03-08. Operational parameters validated against
esp_timer-paced async capture task (commit 2e338fe4).*
