---
abstract: "Source-anchored first-pass inventory for BACKLOG WB-1: renderer and VP timing metric names whose labels can mislead tactical decisions."
---

# WB-1 Metric Accountability Inventory - 2026-05-16

## RBDO Label

GROUNDED.

## Scope

This is the first read-only inventory for `BACKLOG.md` WB-1, limited to the renderer/frame/timing surfaces named in the Work Block trigger:

- `frameDrops` / serial `Drops`;
- `cpuPercent` / serial `CPU` / `vp stack` `cpu=...`;
- trace span `show_leds`;
- LED transport health fields `showSkips`, failures, RMT errors, and underruns where they are used to disambiguate frame-budget overruns from hardware output suppression.

This artefact does not rename code, change protocol contracts, alter firmware behaviour, or claim full repo-wide metric coverage.

## Inventory

| Metric / label | Current source definition | Likely misread | Operational risk | Source anchors | Proposed action |
|---|---|---|---|---|---|
| `RenderStats::frameDrops` | Count of rendered frames whose raw pre-pacing work exceeded `LedConfig::FRAME_TIME_US`; incremented when `rawFrameTimeUs > LedConfig::FRAME_TIME_US`. | Dropped, skipped, or lost output frames. | Agents or operators may treat high values as LED transport failure even when every frame still reaches the LEDs. | `RendererActor.h:131`; `RendererActor.cpp:2608-2612`; `ap_vp_contract_frame_drop_investigation_2026-05-14.md:187-215`. | Rename internal field to `framesOverBudget` and keep a compatibility note only where historic evidence uses `Drops`. Behaviour change not required. |
| Serial status label `Drops:` | `ActorSystem::printStatus()` prints `rs.frameDrops` beside `Frames`. | Hardware frame skips or RMT output drops. | Serial evidence can be misclassified as AP/VP coupling, render failure, or LED suppression. | `ActorSystem.cpp:842-849`; `ap_vp_contract_frame_drop_investigation_2026-05-14.md:74-81,231-240`. | Change label to `OverBudget:` and include a percentage derived from `framesRendered`. Keep `LED show ... skips=...` separate. |
| `RenderStats::cpuPercent` | `(avgFrameTimeUs * 100) / LedConfig::FRAME_TIME_US`, clamped to 100. It is renderer frame-budget occupancy based on the rolling frame-time average. | Whole-device CPU utilisation. | `CPU: 100%` can be read as system saturation when it only means the renderer is occupying the 8.33 ms frame budget. | `RendererActor.h:132-136`; `RendererActor.cpp:2622-2641`; `ActorSystem.cpp:843`; `SerialCLI.cpp:463-471`. | Rename or relabel user-facing text to `frameBudgetPct` / `budget=...%`; document numerator, denominator, and smoothing window. Protocol fields need migration analysis before any wire rename. |
| REST/WS `cpuPercent` | Device/system and audio surfaces expose `cpuPercent` under multiple meanings. Renderer-derived device fields are copied from `RenderStats`; audio `cpuLoadPercent` is a separate audio metric. | A single uniform CPU metric across REST, WS, audio, and device status. | Client dashboards can compare unrelated percentages as if they share one owner and denominator. | `DeviceHandlers.cpp:47`; `SystemHandlers.cpp:44`; `WsDeviceCommands.cpp:76,130`; `AudioHandlers.cpp:1540`; `docs/protocol/k1-ws-contract.yaml:2340,2505`. | Do not rename blindly. Add protocol descriptions first: owner, numerator, denominator, timing window. If renamed later, alias old fields with deprecation. |
| Trace span `show_leds` | `RendererActor::onTick()` traces the renderer `showLeds()` wrapper. The wrapper includes post-correction output preparation plus the LED-driver show call. | FastLED-only or RMT wire-only duration. | Causal timing claims can wrongly attribute output-prep time to FastLED/RMT or miss the protective wire fence boundary. | `RendererActor.cpp:1059-1061`; `SerialCLI.cpp:477-482`; `VP_STACK_INTROSPECTION_COMMAND_SPEC.md:58-66`; `k1_waveform_hybrid_serial_evidence_2026-05-07.md:793`. | Keep span only if documented as wrapper timing, or add clearer sibling names: `renderer_show_wrapper`, `output_prep`, `led_driver_show`. |
| `led_show` serial/driver stats | `LedDriverStats` surface reporting driver show timing and `showSkips`; `LedDriver_S3::show()` increments `showSkips` only on RMT mutex acquisition timeout. | Pure FastLED wire time, or the same thing as trace `show_leds`. | Healthy LED transport can be mistaken for premature return if one timing field is read alone. | `ActorSystem.cpp:847-849`; `LedDriver_S3.cpp:147-153`; `LedDriver_S3.cpp:156-167`; `VP_STACK_INTROSPECTION_COMMAND_SPEC.md:65-66`. | Leave field name but define inclusion/exclusion boundary in docs and status output; interpret beside `expected_wire_us`, `wire_fence`, and `showSkips`. |
| SynqMatrix `health.showSkips` | SynqMatrix mirrors renderer LED health counters and treats any non-zero show skips/failures/RMT errors/underruns as degraded. | SynqMatrix-local failure count or musical-state diagnostic. | A rendering/transport health event can be misread as SynqMatrix algorithm failure. | `SynqMatrix.cpp:576`; `SynqMatrix.cpp:1532-1533`; `SynqMatrixHandlers.cpp:63`; `WsSynqMatrixCommands.cpp:90`; `docs/protocol/k1-rest-contract.yaml:305-310`; `docs/protocol/k1-ws-contract.yaml:419,533,687,825`. | Keep as health mirror, but document owner as LED transport / renderer health. Do not use it as music-state evidence. |

## Immediate Corrections Recommended

1. Rename serial-only `Drops:` to `OverBudget:` first. It is the lowest compatibility-risk correction because the existing forensic audit found no REST/WS `frameDrops` exposure.
2. Add a short metric-definition table to the VP stack spec and hardware-validation prompt pack before broader code renames. This should define owner, numerator, denominator, timing window, inclusion/exclusion boundary, and whether the field is evidence of visible output suppression.
3. Treat protocol `cpuPercent` as migration-sensitive. The same field name appears on multiple REST/WS surfaces with different owners, so code changes should wait for a client-compatibility plan.
4. Keep `showSkips` as the LED transport suppression counter. The problem is not the name; the problem is when readers confuse it with renderer frame-budget overruns.

## Non-Goals

- No protocol field rename in this pass.
- No runtime behaviour change.
- No timing-threshold change.
- No FastLED/RMT transport conclusion; that remains WB-2.
- No SynqMatrix algorithm work.

## Verification

Read-only source and documentation checks used:

```text
sed -n '1,260p' BACKLOG.md
sed -n '1,260p' firmware-v3/docs/debugging/VP_STACK_INTROSPECTION_COMMAND_SPEC.md
sed -n '1,260p' firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md
grep -RInE "frameDrops|cpuPercent|CPU percent|cpu=|show_leds|framesRendered|avgFrameTimeUs|showSkips" --include='*.cpp' --include='*.h' firmware-v3/src/core firmware-v3/src/network firmware-v3/src/serial firmware-v3/src/hal firmware-v3/src/codec
grep -RInE "frameDrops|cpu=|show_leds|showSkips|deadline|budget occupancy|FastLED-only|wire_fence|led_show|avgFrameTimeUs" --include='*.md' firmware-v3/docs docs
grep -RInE "framesRendered|frameDrops|cpuPercent|avgFrameTimeUs|showSkips" --include='*.yaml' docs/protocol firmware-v3/docs
```

`rg` was unavailable in the active shell, so the text-search fallback was `grep`. No C++ symbol/reference/call-hierarchy claim in this document depends on grep alone; source references above are direct file/line reads.

## Changelog

| Date | Agent | Note |
|---|---|---|
| 2026-05-16 | codex:gpt-5.5 | Created first-pass WB-1 inventory from current `BACKLOG.md`, VP stack docs, renderer/serial/driver source, protocol YAML, and the existing AP-VP frame-drop forensic audit. |
