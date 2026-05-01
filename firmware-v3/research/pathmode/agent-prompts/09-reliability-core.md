You are a Senior Engineer. Implement the following Intent Spec using a sequential workflow.

CRITICAL: This intent BUNDLES four foundational reliability workstreams. They share infrastructure (NVS persistence, REST endpoints, telemetry events). Implement in dependency order.

## Phase 1: Orientation
Likely-affected files: firmware-v3/platformio.ini (brownout config), src/core/SystemInit.{h,cpp}, src/core/persistence/NVSManager.h, src/core/system/OtaBootVerifier.h, src/audio/backends/esv11/vendor/microphone.h, src/network/webserver/V1ApiRoutes.cpp, src/core/system/HeapMonitor.h, src/network/WsGateway.cpp, tab5-encoder/src/ui/ (System tab → Diagnostics).

Read existing patterns: telemetry event format, REST API route structure, NVS access pattern. Use Glob/Grep/Read.

## Phase 2: Plan
Suggested order:
1. Brownout detector (platformio.ini config + NVS flag + safe-mode boot).
2. Panic / WDT log NVS ring buffer (write path + REST GET endpoint).
3. Mic placement + SNR specification doc (no firmware changes; placement guide).
4. Tab5 diagnostics panel REST endpoint /api/system/diagnostics.
5. Tab5 UI tab consuming the endpoint (read-only, 5 s refresh).

Use TodoWrite per outcome.

## Phase 3: Implement
Dependency order. NVS ring buffer feeds REST endpoint feeds Tab5 panel. After each: verify no breakage. Respect constraints (no destructive ops in diagnostics; no personal data in logs).

## Phase 4: Validate
**E2E**: Variable-PSU bench down to 4.5 V — assert recovery + brownout telemetry. Tab5 manual usage — diagnostics payload < 1 s.
**Unit**: Mock-hang triggers WDT — panic surfaces in /api/system/panic-history.
**Manual**: Calibrated SPL meter measurement in reference room — confirm SNR floor ≥ 40 dB. Field-troubleshoot dry-run using only the Tab5 diagnostics panel (no USB).

# Intent Spec: Reliability core: brownout detection + panic log persistence + mic SNR specification + Tab5 diagnostics panel

**ID**: `a6a991ec-28cd-4f4d-b214-83fefdf60f93` | **Status**: validated

## Objective

K1 lives in critical-listening environments and ships without cloud diagnostics. Today the firmware has no brownout detector, no persistent panic log, no documented mic SNR specification, and no local diagnostics surface for field troubleshooting. The brand promise ('permanent fixture for critical listening') demands robustness in the presence of mains sag, panics, and field-troubleshoot scenarios. This intent bundles four foundational reliability workstreams.

## Success Outcomes

- [ ] **Brownout detector** enabled at ≥ 2.8 V threshold; brownout event persisted to NVS flag and logged to Serial telemetry `{"event":"sys.brownout.detected","ts_mono_ms":...}`; safe-mode boot loads factory preset (Prism) when brownout flag is set; recovery 100% autonomous within 2 s; AP live within 5 s.
- [ ] **Persistent panic / WDT log** to NVS ring buffer (256 B/event × 5 slots); REST GET /api/system/panic-history returns JSON `{timestamp, reason, free heap, task name}`; ring overwrites oldest when full; survives reboot.
- [ ] **SPH0645 microphone placement + SNR floor specification** published (firmware-v3/docs/audio/k1-mic-placement.md): distance from speaker ≥ 30 cm, broadside angle, mounting orientation diaphragm-to-listener; SNR floor ≥ 40 dB A-weighted in typical hi-fi room (1 kHz, 94 dB SPL ref); AGC explicitly disabled by design (silence gate handles quiet passages); frequency response 50 Hz–16 kHz ± 3 dB confirmed empirically; spec applies to K1v1 and K1v2 GPIO variants.
- [ ] **Tab5 local diagnostics panel**: REST /api/system/diagnostics returns uptime, free heap + fragmentation %, last 5 panics, AP SSID + connected clients, audio snapshot (RMS, silence state, onset), OTA partition + rollback availability, encoder positions, NVS usage; Tab5 'System' → 'Diagnostics' renders this read-only at 5 s refresh.

## Constraints & Constitution

- [!] No false brownout positives during peak LED current draw (~5 A transient).
- [!] Panic-log writes < 100 ms latency; survives reboot; NVS-corruption fallback writes 'panic_nvs_failed' sentinel.
- [!] Diagnostics panel is read-only — no destructive operations (no factory reset, no firmware erase).
- [!] No personal data logged anywhere (no IPs, no usernames).
- [!] Mic placement spec must NOT propose hardware redesign — placement and software only.
- [!] British English.
- [!] Precedence: Constraint > Standard > Pattern.

## Edge Cases
- **Mains sag during NVS commit** → Brownout flag persists; NVS rolls back; factory preset on next boot.
- **Repeating brownout (every 30 s)** → Distinctive amber LED indicator during power-stabilisation phase.
- **Five panics in 60 s** → Most recent 5 retained; oldest erased.
- **NVS corrupted on panic-log write** → Write fails silently; boot logs 'panic_nvs_failed'.
- **NVS at 98% capacity** → Diagnostics dashboard warns + suggests restart.
- **Mic placed on speaker cabinet** → Placement spec warns of feedback risk; recommends ≥ 30 cm clearance.
- **Untreated reflective room** → Spec notes 3–6 dB SNR degradation expected; suggests soft furnishings.
