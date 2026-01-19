# Curated Hardware Traces

This directory contains real hardware-captured traces from the ESP32-S3 LightwaveOS device. These traces are used as "golden fixtures" for protocol conformance testing.

## Trace Files

### happy_path.jsonl / happy_path.itf.json
- **Scenario:** Connect -> status.get -> parameters.set (brightness) -> parameters.set (speed) -> effects.setCurrent -> status.get -> disconnect
- **Firmware SHA:** a824c3741b68e8b77341def4364bc2bc95be3544
- **Spec SHA:** a824c3741b68e8b77341def4364bc2bc95be3544
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 7 (ws.connect, msg.recv x5, ws.disconnect)
- **Purpose:** Baseline happy path protocol flow verification
- **Conformance:** PASS (verified against lightwave_step.qnt)
- **Last Verified:** 2026-01-19

### validation_negative.jsonl / validation_negative.itf.json
- **Scenario:** Invalid JSON, oversized payload, unknown method, malformed params
- **Expected Behaviour:** All msg.recv events have result="rejected" with appropriate reason
- **Firmware SHA:** a824c3741b68e8b77341def4364bc2bc95be3544
- **Spec SHA:** a824c3741b68e8b77341def4364bc2bc95be3544
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 10 (includes multiple rejection scenarios)
- **Purpose:** Verify rejection telemetry and error handling
- **Conformance:** PASS (verified against lightwave_step.qnt - rejected messages don't change state)
- **Last Verified:** 2026-01-19

### reconnect_churn.jsonl / reconnect_churn.itf.json
- **Scenario:** 3 rapid connect/disconnect cycles
- **Firmware SHA:** a824c3741b68e8b77341def4364bc2bc95be3544
- **Spec SHA:** a824c3741b68e8b77341def4364bc2bc95be3544
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 7 (multiple ws.connect/ws.disconnect cycles)
- **Purpose:** Verify clientId increments, ws.disconnect logged correctly, connection state tracking
- **Conformance:** PASS (verified against lightwave_step.qnt)
- **Last Verified:** 2026-01-19

### zones_happy_path.jsonl / zones_happy_path.itf.json
- **Scenario:** Connect → getStatus (handshake) → zones.get → zones.update → observe zones.list + zones.changed
- **Firmware SHA:** *[To be captured]*
- **Spec SHA:** *[To be captured]*
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** *[To be captured]*
- **Events:** *[To be captured]*
- **Purpose:** Baseline zones protocol flow verification (happy path)
- **Conformance:** *[To be verified]*
- **Last Verified:** *[To be verified]*

### zones_reconnect_mid_update.jsonl / zones_reconnect_mid_update.itf.json
- **Scenario:** zones.update then disconnect **before** zones.changed, reconnect, complete flow (reconnect-mid-update)
- **Firmware SHA:** *[To be captured]*
- **Spec SHA:** *[To be captured]*
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** *[To be captured]*
- **Events:** *[To be captured]*
- **Purpose:** Prove idempotency / message-soup tolerance for zones (disconnect before response)
- **Conformance:** *[To be verified]*
- **Last Verified:** *[To be verified]*

### zones_reconnect_churn.jsonl / zones_reconnect_churn.itf.json
- **Scenario:** Connect → start zones flow → disconnect → reconnect (epoch increments) → complete zones flow
- **Firmware SHA:** *[To be captured]*
- **Spec SHA:** *[To be captured]*
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** *[To be captured]*
- **Events:** *[To be captured]*
- **Purpose:** Prove epoch/handshake discipline holds for zones protocol too
- **Conformance:** *[To be verified]*
- **Last Verified:** *[To be verified]*

## Capture Process

### Core traces (color.* flow)

These traces were captured using:
1. Direct serial capture from `/dev/cu.usbmodem1101` (ESP32-S3 device)
2. Parallel WebSocket client execution via Python
3. JSONL extraction using `tools/extract_jsonl.py`
4. ITF conversion using `tools/jsonl_to_itf.py`
5. ADR-015 validation using `tools/validate_itf_bigint.py`

### Zones traces (zones.* flow)

**Capture script:** `tools/capture_zones_trace_direct.py`

**Usage:**
```bash
# Capture zones_happy_path
python3 tools/capture_zones_trace_direct.py zones_happy_path traces/curated/

# Capture zones_reconnect_mid_update
python3 tools/capture_zones_trace_direct.py zones_reconnect_mid_update traces/curated/

# Capture zones_reconnect_churn
python3 tools/capture_zones_trace_direct.py zones_reconnect_churn traces/curated/
```

The capture script:
1. Captures raw serial output (with ESP_LOG prefixes) from `/dev/cu.usbmodem1101` at 115200 baud
2. Runs WebSocket scenarios via `ws://lightwaveos.local/ws`
3. Post-processes raw capture through `tools/extract_jsonl.py` to extract clean JSONL
4. Outputs `zones_*.jsonl` files ready for ITF conversion and conformance checking

**Post-capture steps (per trace):**
1. Convert to ITF: `python3 tools/jsonl_to_itf.py traces/curated/zones_*.jsonl traces/curated/zones_*.itf.json`
2. Validate ADR-015: `python3 tools/validate_itf_bigint.py traces/curated/zones_*.itf.json`
3. Check conformance: `python3 tools/check_conformance.py traces/curated/zones_*.jsonl`

## Validation

All ITF files in this directory pass ADR-015 compliance checks (bigint encoding required for all integers).

## Conformance Checking

Traces are checked for model conformance using `tools/check_trace_conformance.py`, which verifies:
- All invariants from `lightwave_properties.qnt` hold for every state in the trace:
  - `NoEarlyApply`: Node never applies params before handshake complete
  - `HandshakeStrict`: CONNECTED requires handshakeComplete
  - `ConnEpochMonotonic`: connEpoch >= 0
  - `EpochResetsHandshake`: If not CONNECTED, handshake must be false
  - `ZonesNoEarlyApply`: *(After first zones trace)* Node never applies zones before handshake complete
  - `ZonesIdempotentUnderDup`: *(After first zones trace)* Identical zone updates do not change resulting zone state on replay

Conformance status is verified automatically in CI (see `.github/workflows/conformance_check.yml`).

## Known-Bad Traces

The [`known_bad/`](known_bad/) subdirectory contains **synthetic traces that violate invariants** to verify the conformance checker detects violations correctly.

These traces are **expected to FAIL** conformance checks. They serve as regression tests: if a trace in this directory passes conformance, the checker is broken.

### Purpose

Known-bad traces verify that:
- The conformance checker (`tools/check_trace_conformance.py`) correctly detects invariant violations
- CI gates fail appropriately when traces violate invariants
- Future changes to the checker don't break violation detection

### Usage

In CI (`.github/workflows/conformance_check.yml`), known-bad traces are checked with `--expect-violation`:

```bash
python3 tools/check_trace_conformance.py --expect-violation known_bad/violates_handshake_strict.itf.json
```

The checker exits 0 (success) if a violation is detected, and exits 1 (failure) if the trace conforms.

See [`known_bad/KNOWN_BAD_METADATA.md`](known_bad/KNOWN_BAD_METADATA.md) for details on each known-bad trace.

## Notes

- Hardware traces are **real captures**, not synthetic
- Known-bad traces are **synthetic** (manually crafted to violate invariants)
- When protocol changes, re-capture hardware traces and update SHAs
- Keep trace files synchronized with their .jsonl source files
- Synthetic traces (if any) should be prefixed with `synthetic_`
