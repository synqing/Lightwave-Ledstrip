You are a Senior Engineer. Implement the following Intent Spec using a sequential workflow.

CRITICAL: This intent BUNDLES three manufacturing/OTA workstreams. The OTA validation extension depends on `/api/system/perf-snapshot` from Slot 3 (latency programme) — if that's not yet implemented, document the dependency and either coordinate or stub appropriately.

## Phase 1: Orientation
Likely-affected files: src/test_strip_hw.cpp (existing demo, expand to full QA), src/main.cpp (factory mode entry), platformio.ini (FEATURE_FACTORY_TEST flag, FastLED.setMaxPowerInVoltsAndMilliamps update), src/network/webserver/V1ApiRoutes.cpp (factory-test-result endpoint), src/core/system/OtaBootVerifier.h (VALIDATION_TIMEOUT_MS), hardware design doc (PSU + NTC inrush spec).

## Phase 2: Plan
Suggested order:
1. Factory test mode (FEATURE_FACTORY_TEST=1, gated): LED sweep, mic loopback, encoder sweep, JSON serial output, persisted result.
2. PSU specification publication + NTC inrush + FastLED power cap update.
3. OTA validation extension to 90 s + secondary perf check.

Use TodoWrite per outcome.

## Phase 3: Implement
Dependency order. Factory test must remain disabled in production builds. After each: verify no breakage. Respect constraints (factory test exits gracefully without AP; OTA must not fail boot for benign causes).

## Phase 4: Validate
**E2E**: Pre-prod K1 with known-good hardware passes factory test; LED unplug triggers FAIL. Variable-PSU load test with calibrated ammeter — assert 3 A continuous rating. OTA a heavy build; monitor perf_check telemetry; verify 90 s window respected.
**Unit**: Simulated render stall triggers OTA warning telemetry.
**Manual**: Walk a factory floor through the 2-minute test cycle; confirm throughput is acceptable.

# Intent Spec: Manufacturing & OTA: factory test mode + PSU specification & inrush limiting + OTA validation extension

**ID**: `2ca2f44a-958c-4afa-b877-42b14e88f704` | **Status**: validated

## Objective

K1 ships through a factory floor and updates over-the-air with no cloud rollback. Today: test_strip_hw.cpp is a demo, not comprehensive QA; power budgeting is firmware-only with no documented PSU spec and no hardware inrush limiting; OTA validation runs 30 s with only basic heap / AP / WebServer checks. This intent bundles the three manufacturing/OTA workstreams that make the device robust to factory variance, electrical edge cases, and bad firmware drops.

## Success Outcomes

- [ ] **Factory test mode** (`FEATURE_FACTORY_TEST=1`, disabled in production by default): entered via 5-second encoder press once AP is live (or Serial command); LED sweep across 320 LEDs × R/G/B; mic loopback (2 s white noise, RMS > -40 dB); encoder full-rotation sweep (80 position reports); JSON output to Serial with PASS/FAIL + serial number + timestamp; result persisted at /api/system/factory-test-result for QR-code/label printing; total runtime ≤ 2 minutes.
- [ ] **PSU specification published**: 5 V @ 3 A continuous, 5 A transient (20 ms peaks), 85–264 VAC input, wall-mount or internal brick; NTC thermistor (10 Ω @ 25 °C, 1206/1210 footprint) in series with main 5 V rail for inrush limiting; firmware brightness cap retained as secondary safeguard (max 160/255 in RendererActor); `FastLED.setMaxPowerInVoltsAndMilliamps(5, 3000)` updated to reflect 3 A continuous rating; hardware design doc updated.
- [ ] **OTA validation window extended** from 30 s to 90 s (`VALIDATION_TIMEOUT_MS`); secondary perf check at 60 s queries /api/system/perf-snapshot (from latency programme, Slot 3); telemetry: `{"event":"ota.boot.perf_check","frame_drops":N,"latency_avg_ms":X,"status":"ok|warning"}`; if frame drops > 0 in 60 s OR avg render > 4 ms → warning logged but does NOT auto-rollback (allows human intervention).

## Constraints & Constitution

- [!] Factory test exits gracefully if AP not yet live ('WiFi not ready' failure mode).
- [!] Factory test must NOT be available in production builds (gated by FEATURE_FACTORY_TEST=1).
- [!] NTC thermistor drop ≤ 0.1 V under full LED load.
- [!] Factory-test full-white sweep must not trigger thermal trip on PSU.
- [!] Async OTA perf checks must not delay AP startup.
- [!] OTA validation must not fail boot for benign causes (heavy USB logging during dev).
- [!] British English.
- [!] Precedence: Constraint > Standard > Pattern.

## Edge Cases
- **LED #N open-circuit during factory test** → Reports 'LED N FAIL (no response)' and halts.
- **Mic under-range during factory test** → Reports 'MIC FAIL (RMS < -40 dB, check connection)'.
- **Customer plugs in undersized 1 A PSU** → LEDs dim instead of flickering; no crash.
- **PSU short-circuit** → Inrush NTC limits fault current; PSU thermal shutdown engages safely.
- **First-effect startup overhead causes 1–2 frame drops during OTA validation** → Warning logged; boot continues.
- **Pathological firmware hangs every 70 s (out of 90 s OTA validation window)** → Validation passes; field unit caught by panic log (Slot 9 Reliability Core) instead.
