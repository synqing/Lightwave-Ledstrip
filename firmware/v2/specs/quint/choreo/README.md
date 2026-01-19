# Choreo Protocol Specifications for LightwaveOS

Formal specifications for the v2 Hub ↔ Tab5 Node WebSocket control plane using Choreo-style message soup patterns.

**Status:** Phase 0-3 in progress (environment contract, protocol map, telemetry contract, minimal step model)

---

## Directory Structure

```
choreo/
├── ENVIRONMENT_CONTRACT.md     # Fault model + trace semantics
├── PROTOCOL_MAP.md             # Message inventory + idempotency rules
├── choreo.qnt                  # Minimal Choreo pattern library
├── lightwave_types.qnt         # Shared types (Node, Message, Event, State)
├── lightwave_step.qnt          # Fast step model (Phase 3)
├── lightwave_properties.qnt    # Invariants (3-5 core properties)
├── traces/
│   ├── curated/                # Hardware captures (JSONL + ITF)
│   └── sim/                    # Quint-generated ITF traces
└── tools/
    ├── collect_trace.py        # Hardware scenario driver
    ├── extract_jsonl.py         # Serial log → JSONL filter
    ├── jsonl_to_itf.py          # JSONL → ITF converter
    └── validate_itf_bigint.py   # ADR-015 validator
```

---

## Core Flow (Phase 3 Scope)

**Focus:** connect → hello/status → parameter change

1. **Connection:** Tab5 connects to v2 → `ws.connect` event → `connEpoch` increments
2. **Handshake:** Tab5 sends `getStatus` → v2 responds with `status` → `handshakeComplete := true`
3. **Parameter Update:** Tab5 sends `parameters.set` → v2 updates state → broadcasts `parameters.changed`

---

## Usage

### Typecheck

```bash
cd firmware/v2/specs/quint/choreo
quint typecheck lightwave_types.qnt
quint typecheck lightwave_step.qnt
quint typecheck lightwave_properties.qnt
```

### Convert JSONL to ITF

```bash
python3 tools/jsonl_to_itf.py input.jsonl output.itf.json
```

**ADR-015 Compliance**: All integers are encoded as `{"#bigint": "num"}` format. Validate with:

```bash
python3 tools/validate_itf_bigint.py output.itf.json
```

### Capture curated hardware traces (Phase 4)

**Terminal 1 (serial capture):**
```bash
pio device monitor -p /dev/cu.usbmodem1101 -b 115200 | \
  python3 firmware/v2/specs/quint/choreo/tools/extract_jsonl.py > \
  firmware/v2/specs/quint/choreo/traces/curated/<scenario>.jsonl
```

**Terminal 2 (scenario execution):**
```bash
python3 firmware/v2/specs/quint/choreo/tools/collect_trace.py \
  --host lightwaveos.local \
  --scenario <scenario>
```

Scenarios:
- `happy_path` - connect → getStatus → enableBlend → getStatus
- `validation_negative` - missing required field (rejected frame)
- `reconnect_churn` - connect → disconnect → reconnect → getStatus

After capture, convert and validate:
```bash
python3 tools/jsonl_to_itf.py traces/curated/happy_path.jsonl traces/curated/happy_path.itf.json
python3 tools/validate_itf_bigint.py traces/curated/happy_path.itf.json
```

### ITF Trace Viewer

For visual inspection of ITF traces, install the **ITF Trace Viewer** VS Code extension:
- Extension ID: `informal.itf-trace-viewer`
- Marketplace: https://marketplace.visualstudio.com/items?itemName=informal.itf-trace-viewer

After installation, open any `*.itf.json` file in VS Code to view state progression and diffs between consecutive states.

### Run Step Model (when complete)

```bash
quint run lightwave_step.qnt --invariant=Invariants --max-steps=20
```

---

## Invariants (Phase 3)

From `lightwave_properties.qnt`:

1. **NoEarlyApply**: Node never applies parameters before `handshakeComplete == true`
2. **HandshakeStrict**: Connected state requires handshake complete or in progress
3. **ConnEpochMonotonic**: Connection epoch never decreases

---

## Next Steps

- Phase 4: Capture curated traces from firmware
- Phase 5: Expand coverage, add micro-step model if needed
