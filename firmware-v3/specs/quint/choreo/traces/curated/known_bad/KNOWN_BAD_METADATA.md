# Known-Bad Traces

This directory contains **synthetic traces that violate invariants** to verify the conformance checker detects violations correctly.

These traces are **expected to FAIL** conformance checks. They serve as regression tests: if a trace in this directory passes conformance, the checker is broken.

## Trace Files

### violates_handshake_strict.itf.json

**Violated Invariant:** `HandshakeStrict`

**Scenario:** State 1 has `connState="CONNECTED"` but `handshakeComplete=false`.

**Expected Failure:**
```
Invariant violation: HandshakeStrict failed at state 1
  State: connState='CONNECTED', connEpoch=1, handshakeComplete=False, lastAppliedParams.keys()=[]
```

**Purpose:** Verifies checker detects `CONNECTED` state without completed handshake.

---

### violates_no_early_apply.itf.json

**Violated Invariant:** `NoEarlyApply`

**Scenario:** State 1 has `handshakeComplete=false` but `lastAppliedParams` contains `brightness=128`.

**Expected Failure:**
```
Invariant violation: NoEarlyApply failed at state 1
  State: connState='CONNECTING', connEpoch=1, handshakeComplete=False, lastAppliedParams.keys()=['brightness']
```

**Purpose:** Verifies checker detects parameter application before handshake completion.

---

### violates_epoch_resets.itf.json

**Violated Invariant:** `EpochResetsHandshake`

**Scenario:** State 1 transitions from `CONNECTED` (handshakeComplete=true) to `DISCONNECTED` but `handshakeComplete` remains `true`.

**Expected Failure:**
```
Invariant violation: EpochResetsHandshake failed at state 1
  State: connState='DISCONNECTED', connEpoch=1, handshakeComplete=True, lastAppliedParams.keys()=['brightness']
```

**Purpose:** Verifies checker detects `DISCONNECTED` state with `handshakeComplete=true` (handshake should reset on disconnect).

---

## Usage in CI

The CI workflow (`.github/workflows/conformance_check.yml`) runs these traces with `--expect-violation` flag:

```bash
python3 tools/check_trace_conformance.py --expect-violation violates_handshake_strict.itf.json
```

This **inverts the exit code**: the checker exits 0 (success) if a violation is detected, and exits 1 (failure) if the trace conforms.

## Notes

- These traces are **synthetic** (manually crafted), not captured from hardware
- They violate **one invariant each** for clarity
- They serve as **regression tests** for the conformance checker itself
