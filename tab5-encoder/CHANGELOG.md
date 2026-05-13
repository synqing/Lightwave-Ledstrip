---
abstract: "Tab5-encoder changelog following Keep a Changelog conventions. Created during Track B forensic-audit remediation (2026-04-18). Tracks all firmware changes to the M5Stack Tab5 companion controller. Subsystem prefixes: build, network, ota, ui, input, storage, docs."
---

# Tab5-Encoder Changelog

All notable changes to the M5Stack Tab5 companion controller firmware are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to the repository-wide versioning scheme.

## [Unreleased]

### Added

### Changed

- **tab5 (network):** Migrate WebSocket zone commands to 1-indexed wire `zoneId` per K1 firmware B2 contract (2026-05-02). Internal C++ storage stays 0-indexed (0..2); translation occurs only at the WS boundary. Outbound: `WebSocketClient::sendZoneEffect/Brightness/Speed/Palette/Blend` now send `zoneId = internal + 1` (1..3); `sendZonesSetLayout` translates each segment's `zoneId`; `processSendQueue()` translates queued zone parameters. Inbound: `WsMessageRouter::handleZoneStatus` and `handleZonesList` (segments + zones[] + sidebar pass) decode wire 1..3 and reject wire 0 (RESERVED) and out-of-range values before mapping to internal `ZoneState[3]` / `ZoneSegment.zoneId` / `ParameterId::ZoneN`.

### Fixed

<!-- (reverted 2026-04-18) AsyncTCP priority/core/WDT flags destabilised WS on ESP32-P4 - needs different approach. -->


- **tab5 (network):** Enlarge the USB-CDC TX ring to 4 KB and cap the TX timeout at 20 ms so Serial writes cannot stall loopTask when no host terminal drains the pipe. (forensic-audit-2026-04-18 P0-17)
- **tab5 (input):** Rate-limit the two hot `[Param]` diagnostic prints in `ParameterHandler::onEncoderChanged` to 10 Hz to prevent encoder-spin flood of the CDC ring. Parameter updates themselves remain unthrottled. (forensic-audit-2026-04-18 P0-17)
- **tab5 (ui):** Rename LVGL 8 `LV_DISP_DEF_REFR_PERIOD` to LVGL 9 `LV_DEF_REFR_PERIOD` in `src/ui/lv_conf.h` so the intended 16 ms (~60 Hz) refresh period is honoured instead of the silently-applied 30 Hz default. (forensic-audit-2026-04-18 P1-20)
- **tab5 (ota):** Mark the running image valid via `esp_ota_mark_app_valid_cancel_rollback()` after a 30 s clean-uptime soak in `loop()`, so a bad OTA that survives `setup()` but crashes later rolls back instead of wedging the device. (forensic-audit-2026-04-18 P0-02)
- **tab5 (ota):** Require and verify a SHA-256 integrity header (`X-OTA-SHA256`, 64 hex) on every OTA upload — stream the hash alongside `Update.write()`, constant-time compare before `Update.end(true)`, hard-reject missing/malformed headers and mismatched images. (forensic-audit-2026-04-18 P0-03)
- **tab5 (ota):** Add a 30 s time-since-last-data session watchdog (`OtaHandler::loop()`) that aborts wedged uploads on client disconnect, releases the SHA-256 context and resets session state so subsequent uploads succeed. (forensic-audit-2026-04-18 P0-04)

### Removed

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-18 | agent:opus-4.7-1M (session by captain:elroy) | Created. Scaffold for Track B forensic-audit remediation. Replaces ad-hoc commit-message documentation. |
| 2026-05-02 | agent:opus-4.7-1M (session by captain:elroy) | Logged Tab5 1-indexed wire `zoneId` migration aligning with K1 B2 commit `d53092ad`. |
