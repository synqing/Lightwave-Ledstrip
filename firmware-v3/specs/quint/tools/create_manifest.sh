#!/bin/bash
# Create run manifest for reproducibility

OUTPUT_DIR="$1"
MAX_STEPS="$2"
TRACE_COUNT="$3"
SEED="${4:-123}"

QUINT_VERSION=$(quint --version 2>/dev/null || echo "unknown")
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

cat > "$OUTPUT_DIR/run_manifest.json" << EOF
{
  "timestamp": "$TIMESTAMP",
  "quint_version": "$QUINT_VERSION",
  "git_commit": "$GIT_COMMIT",
  "max_steps": $MAX_STEPS,
  "trace_count": $TRACE_COUNT,
  "seed": $SEED,
  "spec_file": "beat_tracker.qnt",
  "command": "quint run --max-steps=$MAX_STEPS --n-traces=$TRACE_COUNT --seed=$SEED --mbt --out-itf=traces/ --main=beat_tracker beat_tracker.qnt",
  "thresholds": {
    "min_traces_per_tuning": 3,
    "max_false_lock_rate": 0.20
  },
  "scoring_weights": {
    "lock_success": 1000.0,
    "time_to_lock": 0.1,
    "post_lock_mae": 10.0,
    "false_lock": 500.0,
    "thrash": 50.0,
    "drift": 100.0,
    "jitter": 50.0
  }
}
EOF

echo "Created manifest: $OUTPUT_DIR/run_manifest.json"
