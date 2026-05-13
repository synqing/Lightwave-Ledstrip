# SongAware Director Repair Evidence - 2026-05-13 12:12:35 AWST

## RBDO

Label: DEGRADED-MODE.

Unresolved assumption: clangd MCP diagnostics in this Codex session returned `Transport closed`; this repair is verified by source inspection, grep/static checks, native tests, harness matrix, and production PlatformIO compile, not semantic clangd.

Risk if wrong: a C++ symbol-level issue could be missed despite compile/test success.

Fallback: treat this as a repair candidate until a fresh Codex session can run clean clangd diagnostics and until hardware/Lane D validation confirms musical taste.

Revisit trigger: first fresh-session clangd smoke, first hardware upload, or first Lane D real-track validation run.

Debt count / affected outputs: 1 output, this evidence pack and the associated repair pass.

## Files Changed

- `firmware-v3/src/core/songaware/SongAwareDirector.h`
- `firmware-v3/src/core/songaware/SongAwareDirector.cpp`
- `firmware-v3/test/test_song_aware_director/test_song_aware_director.cpp`
- `firmware-v3/src/serial/SerialCLI.cpp`
- `firmware-v3/src/serial/SerialJsonGateway.cpp`
- `firmware-v3/src/network/webserver/handlers/SongAwareHandlers.h`
- `firmware-v3/src/network/webserver/handlers/SongAwareHandlers.cpp`
- `firmware-v3/src/network/webserver/ws/WsSongAwareCommands.cpp`
- `firmware-v3/src/network/webserver/V1ApiRoutes.cpp`
- `docs/protocol/k1-rest-contract.yaml`
- `docs/protocol/k1-ws-contract.yaml`
- `docs/evidence/songaware_repair_20260513_121235.md`

## Behavioural Summary

Before:
- Mode taxonomy mixed authority and intensity: `Off`, `Parameter`, `On = Parameter`, `Subtle`, `Balanced`, `High`, `Director`.
- `MusicalGridSnapshot` was discarded in `evaluateDirector()` and `apply()`.
- Policy rows were exposed as a fixed read-only "allowlist".
- Serial/API/WS payloads exposed no separate profile.

After:
- Operational mode is `off | assist | director`.
- Intensity profile is `subtle | balanced | high`.
- Legacy `on` and `parameter` map to Assist; legacy `subtle | balanced | high` mode values set profile and map mode to Assist.
- `MusicalGridSnapshot` drives Director boundary gating through downbeat, beat, or conservative phase fallback.
- Status/debug now expose `intent`, `actionPlan`, `boundaryGate`, `boundaryReady`, `waitingForBoundary`, and `boundaryConfidence`.
- Assist applies parameter and bounded colour-control modulation without effect switching.
- Director can request effect switches only when switching is enabled, gates pass, and a musical boundary is ready.
- Runtime allowlist is mutable; disabled rows suppress with `allowlist_disabled`.
- Serial hotkeys: `D` cycles mode, `G` cycles profile, `Q` prints compact SongAware status.
- Serial readback is consolidated under `sa dbg 0..4`; mutating commands remain separate.

## Before/After Command Map

Before:
- `songaware status`, `songaware debug`, `songaware policy`, `songaware allowlist`, `songaware health`
- `songaware mode off|parameter|on|subtle|balanced|high|director`
- `songaware switching on|off`
- `songaware reset`

After:
- `sa` / `songaware`: compact status
- `sa dbg 0`: compact
- `sa dbg 1`: full status
- `sa dbg 2`: policy table and allowlist
- `sa dbg 3`: health
- `sa dbg 4`: trace/debug
- `sa mode off|assist|director`
- `sa profile subtle|balanced|high`
- `sa switch on|off`
- `sa allow <state> on|off`
- `sa allow reset`
- `sa off`
- `sa wipe`
- `sa counters reset`

Compatibility aliases kept for one release:
- `songaware on`: maps to Assist with deprecation text.
- `songaware mode on|parameter`: maps to Assist.
- `songaware mode subtle|balanced|high`: sets profile and maps mode to Assist.
- `songaware reset`: maps to wipe with deprecation text.
- `songaware policy|health|debug`: maps to `sa dbg` levels.

## Commands Run

Baseline before repair:
- `pio test -e native_test_song_aware_director`
  - PASS: 19 tests succeeded.

Final verification:
- `pio test -e native_test_song_aware_director`
  - PASS: 25 tests succeeded in 0.953 s.
- `python3 scripts/check_native_harness_routes.py`
  - PASS: no stale broad native_test routes found.
- `python3 scripts/native_harness_matrix.py`
  - PASS: 17 matrix rows passed, including `song-aware director policy`, codec aggregate, phase5 native substrate, audio benchmark, and WS router benchmark.
- `pio run -e esp32dev_audio_esv11_k1v2_32khz`
  - PASS: production ESV11 K1v2 32 kHz build succeeded in 43.124 s.
  - RAM: 126700 / 327680 bytes, 38.7%.
  - Flash: 2519949 / 7340032 bytes, 34.3%.
  - Warning observed: existing `WS_MAX_QUEUED_MESSAGES` redefinition warning.
- `rg -n '\(void\)grid|SongAwareMode::(On|Parameter|Subtle|Balanced|High)|On = Parameter|allowlistSnapshot|copyAllowlist|fixed Song-Aware|fixed state policy allowlist|\[off, on|parameter-only|automaticEffectSwitches remains zero' firmware-v3/src firmware-v3/test/test_song_aware_director docs/protocol || true`
  - PASS: no matches.

## Acceptance Evidence

- Off, Assist, and Director are distinct: `test_song_aware_mode_taxonomy_collapsed`.
- Assist modulates controls and never emits a switch: `test_song_aware_assist_mode_changes_controls_without_switching`, `test_song_aware_assist_handles_build_drop_without_effect_switch`.
- Director can emit a switch when all gates pass: `test_song_aware_successful_switch_request_and_applied_counters`.
- Profile scalars are real: `test_song_aware_profile_scalars_are_real`.
- Grid boundary gates switching: `test_song_aware_grid_boundary_gates_switching`.
- Mutable allowlist disables policy selection: `test_song_aware_mutable_allowlist_disables_state_policy`.
- Health recovery uses current clean-window state: `test_song_aware_health_gate_uses_current_health_not_stale_counters`.
- Legacy parsing maps safely: `test_song_aware_mode_parsing_and_names_cover_public_modes`.

## Known Limitations

- Real-track musical taste validation remains unproven.
- No hardware upload was run.
- EdgeMixer and ZoneComposer are represented in action-plan intent telemetry; this pass does not add new renderer actor messages to directly mutate those subsystems.
- Policy timing constants remain compile-time constants.
- Compatibility aliases still exist for one release and should be removed only after clients stop sending legacy mode values.
