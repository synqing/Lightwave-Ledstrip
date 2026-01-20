# Curated Hardware Traces

This directory contains real hardware-captured traces from the ESP32-S3 LightwaveOS device. These traces are used as "golden fixtures" for protocol conformance testing.

## Trace Files

### happy_path.jsonl / happy_path.itf.json
- **Scenario:** Connect -> status.get -> parameters.set (brightness) -> parameters.set (speed) -> effects.setCurrent -> status.get -> disconnect
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
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
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 10 (includes multiple rejection scenarios)
- **Purpose:** Verify rejection telemetry and error handling
- **Conformance:** PASS (verified against lightwave_step.qnt - rejected messages don't change state)
- **Last Verified:** 2026-01-19

### reconnect_churn.jsonl / reconnect_churn.itf.json
- **Scenario:** 3 rapid connect/disconnect cycles
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 7 (multiple ws.connect/ws.disconnect cycles)
- **Purpose:** Verify clientId increments, ws.disconnect logged correctly, connection state tracking
- **Conformance:** PASS (verified against lightwave_step.qnt)
- **Last Verified:** 2026-01-19

### zones_happy_path.jsonl / zones_happy_path.itf.json
- **Scenario:** Connect → getStatus (handshake) → zones.get → zones.update → observe zones.list + zones.changed
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c (to be updated after commit)
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c (to be updated after commit)
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 19 (ws.connect x2, msg.recv x16 including zones.get, zones.update, ws.disconnect x1)
- **Purpose:** Baseline zones protocol flow verification (happy path)
- **Conformance:** PASS (verified against lightwave_step.qnt with zones actions)
- **Last Verified:** 2026-01-19

### zones_reconnect_mid_update.jsonl / zones_reconnect_mid_update.itf.json
- **Scenario:** zones.update then disconnect **before** zones.changed, reconnect, complete flow (reconnect-mid-update)
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c (to be updated after commit)
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c (to be updated after commit)
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 10 (connect/disconnect cycles with zones.get, zones.update)
- **Purpose:** Prove idempotency / message-soup tolerance for zones (disconnect before response)
- **Conformance:** PASS (verified against lightwave_step.qnt with zones actions)
- **Last Verified:** 2026-01-19

### zones_reconnect_churn.jsonl / zones_reconnect_churn.itf.json
- **Scenario:** Connect → start zones flow → disconnect → reconnect (epoch increments) → complete zones flow
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c (to be updated after commit)
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c (to be updated after commit)
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 9 (connect/disconnect/reconnect with zones.get, zones.update)
- **Purpose:** Prove epoch/handshake discipline holds for zones protocol too
- **Conformance:** PASS (verified against lightwave_step.qnt with zones actions)
- **Last Verified:** 2026-01-19

### ota_ws_happy_path.jsonl / ota_ws_happy_path.itf.json
- **Scenario:** Connect → getStatus (handshake) → ota.check → ota.begin → stream chunks → ota.verify (device reboots)
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 57 (WebSocket OTA happy path with full upload flow, device rebooted)
- **Purpose:** Baseline OTA WebSocket protocol flow verification (happy path)
- **Conformance:** PASS (ADR-015, JSONL conformance, ITF invariants)

### ota_ws_reconnect_mid_transfer.jsonl / ota_ws_reconnect_mid_transfer.itf.json
- **Scenario:** ota.begin → send some chunks → disconnect WS → reconnect (epoch increments) → restart ota.begin → complete
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 68 (reconnect mid-transfer with epoch-scoping validation)
- **Purpose:** Prove OTA session epoch-scoping: disconnect aborts session, reconnect requires restart
- **Conformance:** PASS (ADR-015, JSONL conformance, ITF invariants)

### ota_ws_abort_and_retry.jsonl / ota_ws_abort_and_retry.itf.json
- **Scenario:** ota.begin → send some chunks → ota.abort → begin again → complete
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 56 (abort and retry flow validation)
- **Purpose:** Prove explicit abort and retry flow works correctly
- **Conformance:** PASS (ADR-015, JSONL conformance, ITF invariants)

### ota_rest_happy_path.jsonl / ota_rest_happy_path.itf.json
- **Scenario:** POST /api/v1/ota/upload with valid X-OTA-Token header → full upload completes → device reboots → new firmware running
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 13 (ota.rest.begin, ota.rest.progress x10, ota.rest.complete, telemetry.boot - complete successful OTA flow)
- **Purpose:** Baseline OTA REST endpoint flow verification (true happy path with auth) - validates dual-bucket rate limiting + Update API fixes (aligned maxSketchSpace, error diagnostics)
- **Conformance:** PASS (ADR-015, JSONL conformance, ITF invariants - REST OTA exempted from handshake requirement)
- **Notes:** 
  - Complete successful OTA upload (1,533,264 bytes) with progress events (10% increments)
  - Update API fixes: aligned maxSketchSpace calculation, Update.printError() diagnostics, partition/flash context logging
  - Dual-bucket rate limiting validated: no 429 errors, auth passed, bucket separation confirmed

### ota_rest_invalid_token.jsonl / ota_rest_invalid_token.itf.json
- **Scenario:** POST /api/v1/ota/upload with invalid/missing X-OTA-Token header → expect 401/403 + ota.rest.failed telemetry
- **Firmware SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Spec SHA:** aaabc3c8373909b1c66fbb4f946325734926581c
- **Device:** ESP32-S3 LightwaveOS v2
- **Build Environment:** esp32dev_audio (PlatformIO)
- **Captured:** 2026-01-19
- **Events:** 1 (ota.rest.failed with Unauthorized - validates auth_fail bucket)
- **Purpose:** Verify auth correctness: invalid token produces correct HTTP status and telemetry
- **Conformance:** PASS (ADR-015, JSONL conformance, ITF invariants - REST OTA exempted from handshake requirement)

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

### OTA traces (ota.* flow)

**WS capture script:** `tools/capture_ota_ws_trace_direct.py`

**REST capture script:** `tools/capture_ota_rest_trace_direct.py`

**Usage:**
```bash
# Capture WS OTA traces
python3 tools/capture_ota_ws_trace_direct.py ota_ws_happy_path traces/curated/
python3 tools/capture_ota_ws_trace_direct.py ota_ws_reconnect_mid_transfer traces/curated/
python3 tools/capture_ota_ws_trace_direct.py ota_ws_abort_and_retry traces/curated/

# Capture REST OTA traces
python3 tools/capture_ota_rest_trace_direct.py ota_rest_happy_path traces/curated/ lightwaveos.local LW-OTA-2024-SecureUpdate
python3 tools/capture_ota_rest_trace_direct.py ota_rest_invalid_token traces/curated/ lightwaveos.local LW-OTA-2024-SecureUpdate
```

The capture scripts:
1. Capture raw serial output (with ESP_LOG prefixes) from `/dev/cu.usbmodem1101` at 115200 baud
2. Run OTA scenarios via WebSocket (`ws://lightwaveos.local/ws`) or HTTP POST (`http://lightwaveos.local/api/v1/ota/upload`)
3. Post-process raw capture through `tools/extract_jsonl.py` to extract clean JSONL
4. Output `ota_*.jsonl` files ready for ITF conversion and conformance checking

**Post-capture steps (per trace):**
1. Convert to ITF: `python3 tools/jsonl_to_itf.py traces/curated/ota_*.jsonl traces/curated/ota_*.itf.json`
2. Validate ADR-015: `python3 tools/validate_itf_bigint.py traces/curated/ota_*.itf.json`
3. Check legality: `python3 tools/check_conformance.py traces/curated/ota_*.jsonl`
4. Check ITF invariants: `python3 tools/check_trace_conformance.py traces/curated/ota_*.itf.json`

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
  - `NoOtaBeforeHandshake`: *(After first OTA trace)* OTA state transitions require handshake complete (WebSocket OTA only; REST OTA exempted)
  - `OtaMonotonicProgress`: *(After first OTA trace)* OTA progress bytesReceived does not decrease within a session

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
