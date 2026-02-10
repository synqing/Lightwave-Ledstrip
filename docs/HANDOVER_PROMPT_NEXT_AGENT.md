# Handover Prompt for Next Agent

## Context: OTA Protocol Conformance Phase - COMPLETE ✅

You're taking over after we completed the **OTA protocol slice** under the conformance checking loop. All 5 OTA traces are golden fixtures, telemetry pipeline is hardened, and the foundation is set for expanding protocol coverage.

## What Was Just Completed

1. **Dual-bucket rate limiting** - Separated auth_fail (5/sec) and auth_ok (20/sec) buckets to prevent invalid token attempts from DoS'ing legitimate uploads
2. **REST OTA write fixes** - Aligned maxSketchSpace calculation + error diagnostics (`Update.getError()`, `Update.printError()`)
3. **Origin header validation** - WebSocket security (monitor-only, can be tightened)
4. **Telemetry "no heroics"** - Removed extractor repair fallback, CI enforces valid JSONL
5. **All 5 OTA traces PASS** - Complete validation (ADR-015, conformance, invariants)

## Current State

**OTA Protocol:** ✅ Complete and locked in
- 3 WebSocket traces (`ota_ws_happy_path`, `ota_ws_reconnect_mid_transfer`, `ota_ws_abort_and_retry`)
- 2 REST traces (`ota_rest_happy_path`, `ota_rest_invalid_token`)
- All traces in `firmware/v2/specs/quint/choreo/traces/curated/`
- All pass ADR-015, conformance, and invariant checks

**Next Recommended:** Expand protocol coverage to `effects.*` (high user value) or implement security hardening (connection limits, idle timeouts)

## Critical Patterns (Don't Break)

### OTA Update Pattern
```cpp
// ✅ CORRECT: Use aligned maxSketchSpace
size_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
Update.begin(maxSketchSpace, U_FLASH);  // NOT firmware size!

// ❌ WRONG: Don't use firmware size directly
Update.begin(s_updateTotal, U_FLASH);  // Will fail with write errors
```

### Error Diagnostics Pattern
```cpp
// Always capture error code and print human-readable error
if (!Update.begin(...) || Update.write(...) != len || !Update.end(true)) {
    int errorCode = Update.getError();
    Update.printError(Serial);  // Human-readable explanation
    emitTelemetry("ota.rest.failed", "failed", bytes, total, reason, nullptr, errorCode);
}
```

### Dual-Bucket Rate Limiting Pattern
```cpp
// Check auth FIRST, then rate limit based on result
bool authValid = checkOTAToken(request);
if (!authValid) {
    if (!rateLimiter.checkHTTPAuthFail(clientIP)) {
        // Rate limited on auth_fail bucket (brute-force protection)
    }
} else {
    if (!rateLimiter.checkHTTPAuthOk(clientIP)) {
        // Rate limited on auth_ok bucket (legitimate uploads)
    }
}
```

### Telemetry Emission Pattern
```cpp
// Always escape payloadSummary before embedding in JSONL
char payloadSummaryEscaped[256];
escapeJsonString(payloadSummaryRaw, payloadSummaryEscaped, sizeof(payloadSummaryEscaped));
// Use payloadSummaryEscaped in snprintf (never raw JSON)
```

## File Locations

**OTA Handlers:**
- `firmware/v2/src/network/webserver/handlers/OtaHandlers.cpp` - REST OTA implementation
- `firmware/v2/src/network/webserver/ws/WsOtaCommands.cpp` - WebSocket OTA implementation

**Rate Limiting:**
- `firmware/v2/src/network/webserver/RateLimiter.h` - Dual-bucket implementation

**Telemetry & Validation:**
- `firmware/v2/specs/quint/choreo/tools/extract_jsonl.py` - JSONL extractor (no repair fallback)
- `firmware/v2/specs/quint/choreo/tools/check_trace_conformance.py` - Invariant checker
- `.github/workflows/telemetry_jsonl_validity.yml` - CI gate for JSONL validity

**Traces:**
- `firmware/v2/specs/quint/choreo/traces/curated/` - All golden traces
- `firmware/v2/specs/quint/choreo/traces/curated/TRACE_METADATA.md` - Trace documentation

## Quick Start Commands

```bash
# Validate a trace
cd firmware/v2/specs/quint/choreo
python3 tools/validate_itf_bigint.py traces/curated/ota_rest_happy_path.itf.json
python3 tools/check_conformance.py traces/curated/ota_rest_happy_path.jsonl
python3 tools/check_trace_conformance.py traces/curated/ota_rest_happy_path.itf.json

# Capture a new trace
python3 tools/capture_ota_rest_trace_direct.py ota_rest_happy_path traces/curated/ lightwaveos.local LW-OTA-2024-SecureUpdate

# Build & upload firmware
cd firmware/v2 && pio run -e esp32dev_audio_esv11 -t upload --upload-port /dev/cu.usbmodem1101
```

## Key Constraints

- **Centre origin only**: Effects must originate from LEDs 79/80
- **No rainbows**: No rainbow color cycling
- **No heap allocation in render loops**: No `new`, `malloc`, `std::vector` growth
- **120+ FPS target**: Keep render loops predictable
- **British English**: Use `centre`, `colour`, `initialise`

## Known Issues / Future Work

1. **Update.runAsync()**: Not available in ESP32 Arduino core (removed from implementation)
2. **Origin validation**: Currently monitor-only (can be tightened to full rejection)
3. **Connection limits**: Not yet implemented (OWASP recommendation)
4. **Idle timeouts**: Not yet implemented (OWASP recommendation)

## Recommended Next Steps

1. **Expand `effects.*` protocol** (highest ROI) - Same conformance loop pattern
2. **Security hardening sprint** - Connection limits + idle timeouts
3. **OTA edge cases** - Duplicate chunks, out-of-order progress

## Full Documentation

See `docs/HANDOVER_OTA_PHASE_COMPLETE.md` for complete details, architecture, troubleshooting, and code references.
