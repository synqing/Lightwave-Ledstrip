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

### Fixed

<!-- (reverted 2026-04-18) AsyncTCP priority/core/WDT flags destabilised WS on ESP32-P4 - needs different approach. -->


- **tab5 (network):** Enlarge the USB-CDC TX ring to 4 KB and cap the TX timeout at 20 ms so Serial writes cannot stall loopTask when no host terminal drains the pipe. (forensic-audit-2026-04-18 P0-17)
- **tab5 (input):** Rate-limit the two hot `[Param]` diagnostic prints in `ParameterHandler::onEncoderChanged` to 10 Hz to prevent encoder-spin flood of the CDC ring. Parameter updates themselves remain unthrottled. (forensic-audit-2026-04-18 P0-17)
- **tab5 (ui):** Rename LVGL 8 `LV_DISP_DEF_REFR_PERIOD` to LVGL 9 `LV_DEF_REFR_PERIOD` in `src/ui/lv_conf.h` so the intended 16 ms (~60 Hz) refresh period is honoured instead of the silently-applied 30 Hz default. (forensic-audit-2026-04-18 P1-20)

### Removed

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-18 | agent:opus-4.7-1M (session by captain:elroy) | Created. Scaffold for Track B forensic-audit remediation. Replaces ad-hoc commit-message documentation. |
