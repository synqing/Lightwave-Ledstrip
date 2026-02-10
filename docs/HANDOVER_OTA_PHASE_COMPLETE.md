# OTA Protocol Conformance Phase - Handover Document

**Date:** 2026-01-19  
**Status:** OTA slice complete - all 5 traces captured, validated, and locked in  
**Next Recommended Work:** Expand protocol coverage (effects.*) or security hardening (connection limits, timeouts)

---

## Executive Summary

We've successfully completed the **OTA protocol slice** under the conformance checking loop. All 5 OTA traces (3 WebSocket, 2 REST) are now golden fixtures with full validation gates. The telemetry pipeline has been hardened to enforce valid JSONL emission at the firmware level.

**Key Achievements:**
- ✅ Dual-bucket rate limiting (prevents auth_fail from DoS'ing auth_ok bucket)
- ✅ REST OTA write failures fixed (aligned maxSketchSpace + error diagnostics)
- ✅ Origin header validation for WebSocket security
- ✅ Telemetry "no heroics" lock-in (CI enforces valid JSONL)
- ✅ All 5 OTA traces PASS (ADR-015, conformance, invariants)

---

## What Was Implemented

### 1. Dual-Bucket Rate Limiting for REST OTA

**Problem:** Invalid OTA token attempts consumed the same rate limit quota as legitimate uploads, preventing `ota_rest_happy_path` trace capture.

**Solution:** Separate rate limit buckets:
- **Bucket A (auth_fail)**: `HTTP_AUTH_FAIL_LIMIT = 5` (brute-force protection)
- **Bucket B (auth_ok)**: `HTTP_AUTH_OK_LIMIT = 20` (legitimate uploads)

**Files Changed:**
- `firmware/v2/src/network/webserver/RateLimiter.h` - Added `httpAuthFailCount`, `httpAuthOkCount`, `checkHTTPAuthFail()`, `checkHTTPAuthOk()`
- `firmware/v2/src/network/webserver/handlers/OtaHandlers.cpp` - Check auth first, then apply appropriate bucket
- `firmware/v2/src/network/ApiResponse.h` - Added `bucket` parameter to `sendRateLimitError()`
- `firmware/v2/src/network/webserver/V1ApiRoutes.cpp` - Removed generic rate limit check (handled internally)

**Key Design Decision:** Auth is checked **before** rate limiting to determine which bucket to use. This is standard security practice (authenticate → authorize/rate-limit).

### 2. REST OTA Write Failure Fixes

**Problem:** `Update.write()` returning 0, causing OTA uploads to fail with "Write failed. Expected: X, Written: 0".

**Solution:**
- **Aligned maxSketchSpace calculation**: `(ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000` (ESP32 Update library requirement)
- **Error diagnostics**: All failure paths now capture `Update.getError()` and call `Update.printError(Serial)`
- **Telemetry enhancement**: Added `updateError` field to `ota.rest.failed` events
- **Partition/flash logging**: One-time boot log includes free space, maxSketchSpace, and firmware size

**Files Changed:**
- `firmware/v2/src/network/webserver/handlers/OtaHandlers.cpp` - Updated `Update.begin()` to use aligned maxSketchSpace, added error diagnostics
- `firmware/v2/src/network/webserver/handlers/OtaHandlers.h` - Added `updateError` parameter to `emitTelemetry()`

**Critical Pattern:** Always use `Update.begin(maxSketchSpace, U_FLASH)` with aligned space, not `Update.begin(firmwareSize, U_FLASH)`. This is the known-good ESP32 async OTA pattern.

### 3. WebSocket Origin Header Validation

**Problem:** OWASP guidance requires Origin header validation during WebSocket handshake to prevent Cross-Site WebSocket Hijacking (CSWSH).

**Solution:** Implemented Origin validation policy:
- **Allows**: Empty/missing Origin (for non-browser clients)
- **Allows**: `lightwaveos.local` and device IPs (local dashboard)
- **Rejects**: All other origins with telemetry logging

**Files Changed:**
- `firmware/v2/src/network/webserver/WsGateway.h` - Added `static bool validateOrigin(AsyncWebServerRequest* request)`
- `firmware/v2/src/network/webserver/WsGateway.cpp` - Implemented Origin validation logic
- `firmware/v2/src/network/WebServer.cpp` - Registered `validateOrigin` as handshake handler
- `firmware/v2/src/network/webserver/ws/WsOtaCommands.cpp` - Added `handleOtaClientDisconnect()` to abort OTA on client disconnect

**Current Policy:** Monitor-only (logs rejected origins but doesn't block yet). Can be tightened to full rejection once we've validated no legitimate clients are affected.

### 4. Telemetry "No Heroics" Lock-In

**Problem:** Extractor contained a repair fallback (`fix_payload_summary()`) that masked firmware telemetry bugs.

**Solution:**
- Removed `fix_payload_summary()` repair function from extractor
- Extractor now fails immediately on any JSON parse error (no repair attempts)
- Created CI workflow that enforces valid JSONL extraction

**Files Changed:**
- `firmware/v2/specs/quint/choreo/tools/extract_jsonl.py` - Removed repair function, strict JSON validation
- `.github/workflows/telemetry_jsonl_validity.yml` - New CI gate that validates all raw captures

**Result:** Telemetry correctness is now a hard contract - firmware must emit valid JSONL, extractor validates strictly, CI enforces.

### 5. OTA Trace Capture & Validation

**Captured 5 OTA traces:**
1. `ota_rest_happy_path` - Complete REST OTA upload (1.5MB, 13 events, successful reboot)
2. `ota_rest_invalid_token` - Auth failure validation
3. `ota_ws_happy_path` - Complete WebSocket OTA upload (57 events)
4. `ota_ws_reconnect_mid_transfer` - Epoch-scoping validation (68 events)
5. `ota_ws_abort_and_retry` - Explicit abort/retry flow (56 events)

**All traces PASS:**
- ✅ ADR-015 bigint compliance (`validate_itf_bigint.py`)
- ✅ JSONL conformance (`check_conformance.py`)
- ✅ ITF invariants (`check_trace_conformance.py`)

**Capture Scripts:**
- `firmware/v2/specs/quint/choreo/tools/capture_ota_rest_trace_direct.py` - REST OTA capture
- `firmware/v2/specs/quint/choreo/tools/capture_ota_ws_trace_direct.py` - WebSocket OTA capture

**Metadata:** All traces documented in `firmware/v2/specs/quint/choreo/traces/curated/TRACE_METADATA.md` with firmware SHA, spec SHA, capture dates, and validation status.

---

## Current State

### OTA Protocol Status: ✅ COMPLETE

- **WebSocket OTA**: Full protocol implemented with telemetry, epoch-scoping, client ownership validation
- **REST OTA**: Full protocol with dual-bucket rate limiting, error diagnostics, aligned Update.begin()
- **Security**: Origin validation (monitor-only), rate limiting with separate buckets
- **Telemetry**: All OTA events instrumented (`ota.ws.*`, `ota.rest.*`)
- **Traces**: All 5 golden traces captured, validated, and locked in

### Invariants Added

1. **NoOtaBeforeHandshake**: OTA state transitions require handshake complete (WebSocket OTA only; REST OTA exempted)
2. **OtaMonotonicProgress**: OTA progress bytesReceived does not decrease within a session

Both invariants are enforced in `check_trace_conformance.py`.

### Known Issues / Limitations

1. **REST OTA Update.runAsync()**: Attempted to add `Update.runAsync(true)` but this method doesn't exist in the ESP32 Arduino core. Current implementation works without it, but if async issues arise, this may need investigation.

2. **Origin Validation Policy**: Currently monitor-only. Consider tightening to full rejection after validating no legitimate clients are affected.

3. **Connection Limits**: OWASP recommends connection/resource limiting for WebSockets. Not yet implemented.

4. **Idle Timeouts**: OWASP recommends timeouts/keepalive for long-lived connections. Not yet implemented.

---

## Architecture Context

### Rate Limiting Design

The rate limiter uses a **sliding window** approach with per-IP tracking:
- Window size: 1 second
- Block duration: 5 seconds when limit exceeded
- LRU eviction when tracking table is full (8 IPs max)

**Bucket Structure:**
- Generic HTTP bucket: `httpCount` (20/sec limit)
- OTA auth_fail bucket: `httpAuthFailCount` (5/sec limit)
- OTA auth_ok bucket: `httpAuthOkCount` (20/sec limit)
- WebSocket bucket: `wsCount` (50/sec limit)

All buckets share the same `blockedUntil` timestamp (simpler, still achieves DoS protection).

### OTA Update Flow

**REST OTA:**
1. First chunk (`index == 0`): Check auth → rate limit (appropriate bucket) → `Update.begin(maxSketchSpace)` → emit `ota.rest.begin`
2. Subsequent chunks: `Update.write()` → progress telemetry (every 10%) → emit `ota.rest.progress`
3. Final chunk: `Update.end(true)` → emit `ota.rest.complete` → reboot

**WebSocket OTA:**
1. `ota.begin`: Check space → `Update.begin(size)` → bind to client ID/epoch → emit `ota.ws.begin`
2. `ota.chunk`: Base64 decode → `Update.write()` → progress telemetry → emit `ota.ws.chunk`
3. `ota.verify`: `Update.end(true)` → emit `ota.ws.complete` → reboot
4. `ota.abort`: `Update.abort()` → emit `ota.ws.abort` → reset state

**Error Handling:**
- All failure paths capture `Update.getError()` and call `Update.printError(Serial)`
- Telemetry includes `updateError` field for diagnosability
- Client disconnects abort active OTA sessions

### Telemetry Pipeline

**Firmware Emission:**
- All telemetry emitted via `Serial.println()` with valid JSONL format
- `payloadSummary` fields are always escaped using `escapeJsonString()` or equivalent
- Events include `schemaVersion: "1.0.0"` field

**Extraction:**
- `extract_jsonl.py` finds JSON lines (handles ESP_LOG prefixes)
- Strict JSON parsing (no repair fallback)
- Exits non-zero if any parse errors occur

**Validation:**
- CI workflow runs extractor on all `*_raw.txt` files
- Validates each extracted line is valid JSON
- Fails if any parse errors or invalid JSON detected

---

## Files Modified (Summary)

### Core Implementation
- `firmware/v2/src/network/webserver/RateLimiter.h` - Dual-bucket rate limiting
- `firmware/v2/src/network/webserver/handlers/OtaHandlers.cpp` - REST OTA fixes + error diagnostics
- `firmware/v2/src/network/webserver/handlers/OtaHandlers.h` - Telemetry signature update
- `firmware/v2/src/network/webserver/V1ApiRoutes.cpp` - Removed generic rate limit from OTA route
- `firmware/v2/src/network/ApiResponse.h` - Added bucket parameter to rate limit error
- `firmware/v2/src/network/webserver/WsGateway.h/cpp` - Origin validation
- `firmware/v2/src/network/webserver/ws/WsOtaCommands.cpp` - Client disconnect handling
- `firmware/v2/src/network/WebServer.cpp` - Origin validation registration

### Tooling
- `firmware/v2/specs/quint/choreo/tools/extract_jsonl.py` - Removed repair function
- `firmware/v2/specs/quint/choreo/tools/capture_ota_rest_trace_direct.py` - Uses real firmware.bin
- `firmware/v2/specs/quint/choreo/tools/check_trace_conformance.py` - OTA invariants + REST exemption
- `firmware/v2/specs/quint/choreo/traces/curated/TRACE_METADATA.md` - Updated with all 5 OTA traces

### CI/CD
- `.github/workflows/telemetry_jsonl_validity.yml` - New workflow for JSONL validity enforcement

---

## Testing & Validation

### Validation Commands

```bash
# Validate a specific trace
cd firmware/v2/specs/quint/choreo
python3 tools/validate_itf_bigint.py traces/curated/ota_rest_happy_path.itf.json
python3 tools/check_conformance.py traces/curated/ota_rest_happy_path.jsonl
python3 tools/check_trace_conformance.py traces/curated/ota_rest_happy_path.itf.json

# Capture a new trace
python3 tools/capture_ota_rest_trace_direct.py ota_rest_happy_path traces/curated/ lightwaveos.local LW-OTA-2024-SecureUpdate
python3 tools/capture_ota_ws_trace_direct.py ota_ws_happy_path traces/curated/

# Test extractor
python3 tools/extract_jsonl.py traces/curated/ota_rest_happy_path_raw.txt
```

### Build & Upload

```bash
# Build
cd firmware/v2 && pio run -e esp32dev_audio_esv11

# Upload to ESP32-S3
cd firmware/v2 && pio run -e esp32dev_audio_esv11 -t upload --upload-port /dev/cu.usbmodem1101

# Monitor serial
pio device monitor -p /dev/cu.usbmodem1101 -b 115200
```

**Device Ports:**
- ESP32-S3 (v2 firmware): `/dev/cu.usbmodem1101`
- Tab5 (encoder firmware): `/dev/cu.usbmodem101`

---

## Recommended Next Steps

### Option A: Expand Protocol Coverage (Recommended)

Bring the next message family under the conformance loop using the same pattern:

1. **`effects.*` protocol** (high user-facing value):
   - Add `effects.*` messages to protocol map
   - Add minimal Quint model actions
   - Add 2-3 invariants (e.g., "No effect apply before handshake")
   - Capture 2-3 curated traces (`effects_happy_path`, `effects_reconnect_mid_set`, etc.)
   - Run standard gates (ADR-015, conformance, invariants)

**Why:** Same machinery, expanding coverage. Effects are user-visible and high-value for regression protection.

### Option B: Security Hardening Sprint

Implement remaining OWASP WebSocket recommendations:

1. **Connection Limits**: Max concurrent WebSocket connections (global + per-IP if possible)
2. **Idle Timeouts**: Timeout idle connections after N seconds (with keepalive ping/pong)
3. **Origin Validation Tightening**: Switch from monitor-only to full rejection

**Why:** OWASP explicitly calls these out as baseline DoS/operational hygiene for long-lived connections.

### Option C: OTA Edge Cases (Robustness)

Capture additional OTA traces for edge cases:

1. `ota_ws_duplicate_chunk` - Duplicate chunk handling
2. `ota_ws_out_of_order_progress` - Out-of-order progress events
3. `ota_ws_reconnect_during_verify` - Reconnect during verify phase

**Why:** Increases robustness but lower priority than expanding protocol coverage.

---

## Important Constraints (Don't Break These!)

### Centre Origin Only
**ALL effects MUST originate from centre LEDs 79/80 and propagate OUTWARD** (or converge inward). No linear 0→159 sweeps.

### No Rainbows
Do not implement rainbow color cycling or full hue-wheel sweeps.

### No Heap Allocation in Render Loops
Forbidden in `render()` hot paths: `new`, `malloc`, `std::vector` growth, `String` concatenations.

### Performance Requirements
- Target: 120+ FPS
- Per-frame work must be predictable
- No heap allocations in render loops

### British English
Use British spelling: `centre`, `colour`, `initialise`.

---

## Troubleshooting

### REST OTA Write Failures

If `Update.write()` returns 0:
1. Check serial output for `Update.printError()` output
2. Verify `ota.rest.failed` telemetry includes `updateError` field
3. Confirm `maxSketchSpace` calculation: `(ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000`
4. Check partition table (OTA partition must be large enough)

### Rate Limiting Issues

If legitimate uploads are rate-limited:
1. Check which bucket fired: Look for `bucket: "auth_fail"` or `bucket: "auth_ok"` in telemetry
2. Verify auth is checked **before** rate limiting (to select correct bucket)
3. Check rate limit config: `HTTP_AUTH_FAIL_LIMIT = 5`, `HTTP_AUTH_OK_LIMIT = 20`

### Telemetry JSONL Parse Errors

If extractor fails with parse errors:
1. Check firmware telemetry emission: All `payloadSummary` must be escaped
2. Verify `escapeJsonString()` is called for all payloadSummary usages
3. Check CI workflow output for specific line numbers
4. Firmware must emit valid JSONL - no repair fallback exists

### Origin Validation Rejects Legitimate Clients

If Origin validation blocks legitimate clients:
1. Current policy is monitor-only (logs but doesn't block)
2. Check telemetry for `ws.connect` events with `result: "rejected"`, `reason: "origin_not_allowed"`
3. To tighten: Modify `WsGateway::validateOrigin()` to return `false` for non-allowlisted origins

---

## Code References

### Key Patterns

**Aligned OTA Space Calculation:**
```cpp
size_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
Update.begin(maxSketchSpace, U_FLASH);
```

**Error Diagnostics:**
```cpp
int errorCode = Update.getError();
Update.printError(Serial);
emitTelemetry("ota.rest.failed", "failed", bytes, total, reason, nullptr, errorCode);
```

**Dual-Bucket Rate Limiting:**
```cpp
bool authValid = checkOTAToken(request);
if (!authValid) {
    if (!rateLimiter.checkHTTPAuthFail(clientIP)) {
        // Rate limited on auth_fail bucket
    }
} else {
    if (!rateLimiter.checkHTTPAuthOk(clientIP)) {
        // Rate limited on auth_ok bucket
    }
}
```

### Documentation

- **Protocol Map**: `firmware/v2/specs/quint/choreo/PROTOCOL_MAP.md`
- **Trace Metadata**: `firmware/v2/specs/quint/choreo/traces/curated/TRACE_METADATA.md`
- **Conformance Checking**: `firmware/v2/specs/quint/choreo/tools/check_conformance.py`
- **Invariant Checking**: `firmware/v2/specs/quint/choreo/tools/check_trace_conformance.py`

---

## Questions for Next Agent

1. **Which protocol family to expand next?** (effects.*, transitions.*, audio.*, etc.)
2. **Should we implement connection limits and idle timeouts now?** (OWASP recommendations)
3. **Should Origin validation be tightened to full rejection?** (currently monitor-only)
4. **Any specific edge cases or robustness improvements needed?** (duplicate messages, out-of-order events, etc.)

---

**Status:** Ready for next phase. OTA slice is complete, validated, and locked in. All infrastructure is in place for expanding protocol coverage using the same conformance loop pattern.
