# Quint Specifications for LightwaveOS

Formal specifications and model-based testing infrastructure for the beat tracker and audio pipeline.

## Quick Start

```bash
cd firmware/v2/specs/quint/tools

# Typecheck the spec
make typecheck

# Run quick test (10 traces, 100 steps)
make quick-test

# Run full witness check (40 traces, 500 steps)
make witness-lock

# Run full nightly sweep with analysis
make speed2-nightly
```

## Directory Structure

```
specs/quint/
├── beat_tracker/
│   ├── beat_tracker.qnt       # Main Quint specification
│   ├── beat_tracker.qnt.backup # Backup of original spec
│   └── winning_tunings.json   # Validated winning tunings
├── tools/
│   ├── Makefile               # Build targets
│   ├── analyze_itf.py         # Trace analysis and ranking
│   ├── check_quint_sync.py    # Doc-sync gate
│   ├── check_witnesses.py     # Witness property checker
│   └── create_manifest.sh     # Run manifest generator
└── README.md                  # This file
```

## Two-Speed Verification Pipeline

### Speed 1: PR Check (Fast)

```bash
make speed1-pr-check
```

- Typecheck
- Smoke traces (10 traces, 100 steps)
- Bounded verification of critical invariants

### Speed 2: Nightly/Full Verification

```bash
make speed2-nightly
```

- Full tuning sweep (80 traces, 500 steps)
- Run manifest for reproducibility
- Comprehensive analysis with trust gates
- Results written to `reports/beat_tracker/<timestamp>/`

## Witness Properties (Phase 2)

The spec defines witness properties that verify lock behaviour:

- **LockAchieved**: At least one trace achieves lock
- **LockWithin5Seconds**: Lock achieved within 250 ticks
- **AccurateLock**: Lock with <10 BPM error
- **StableLock**: Lock maintained for 100+ ticks

Run witness checks:

```bash
make witness-lock
```

## Trust Gates

The analysis applies three trust gates:

1. **Coverage Gate**: Minimum traces per tuning (default: 3)
2. **Safety Gate**: Maximum false-lock rate (default: 20%)
3. **Evidence Gate**: Scenario bucket coverage (when available)

## Post-Lock Metrics

The analyzer reports post-lock-only metrics to separate convergence from steady-state performance:

- **post_lock_mae**: Mean absolute error after lock
- **drift_rate**: BPM change per second while locked
- **lock_jitter**: Standard deviation of BPM while locked

## Winning Tunings

Validated tunings are stored in `beat_tracker/winning_tunings.json`:

```json
{
  "recommended": "winner_2026_01_18",
  "tunings": [
    {
      "id": "winner_2026_01_18",
      "parameters": {
        "refractory_ms": 140,
        "conf_gate_float": 0.8,
        "alpha_attack_float": 0.20,
        "alpha_release_float": 0.10,
        "hold_ms": 1000,
        "octave_mode": "aggressive",
        "phase_nudge_float": 0.10
      },
      "metrics": {
        "lock_success_rate": 1.0,
        "time_to_lock_mean_ms": 4428
      }
    }
  ]
}
```

## Quint Mode Compliance

The spec follows Quint's mode rules:

- No braces `{...}` in `pure def` bodies (except record literals)
- No `.filter()`, `.forall()`, `.map()` on lists (use `.select()`, `.foldl()`)
- No `oneOf()` in `pure def` (use `nondet` in `action init`)

Run the doc-sync gate to verify compliance:

```bash
make doc-sync
```
