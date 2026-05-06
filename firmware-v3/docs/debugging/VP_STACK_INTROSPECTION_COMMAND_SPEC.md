---
abstract: "Specification for a future read-only VP stack introspection command. Captures what the command must report, which source owners hold each field, and which visual-pipeline defaults it must not change."
---

# VP Stack Introspection Command Spec

**Status:** DRAFT SPEC ONLY - Captain-approved for documentation/tooling package on 2026-05-06. No firmware behaviour changes are made by this document.

**Authority anchors:**
- `firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md:36-55` defines the ordered render lifecycle.
- `firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md:24-45` defines VP validation non-negotiables and evidence ladder.
- `firmware-v3/src/core/actors/ActorSystem.cpp:850` and `:865-882` already print frame/status evidence through serial `s`.
- `firmware-v3/src/core/actors/RendererActor.cpp:1753-1773`, `:1776-1796`, and `:1811-1847` distinguish direct-strip, zone-composer, and unified render branches.
- `firmware-v3/src/core/actors/RendererActor.cpp:966-982` processes colour correction on `m_leds`; `:2067-2105` later handles unified or dual-channel output buffers.

## Purpose

The future command should answer one question:

> What visual-pipeline layers are active for the current frame path, and which buffers do they actually touch?

It is a read-only truth dump. It must not change renderer behaviour, correction defaults, EdgeMixer state, silence policy, gamma, dithering, or WiFi mode.

## Proposed Command Surface

Start with serial:

```text
vp stack
```

Optional future machine-readable form:

```text
{"type":"vp.stack.get","requestId":"..."}
```

Do not add REST or WS forms until `docs/protocol/k1-rest-contract.yaml` and/or `docs/protocol/k1-ws-contract.yaml` are updated first.

## Required Output Sections

### 1. Frame Identity

Report:

- current effect ID and name;
- palette ID/name if available;
- brightness and speed;
- FPS;
- frames rendered;
- frame drops;
- average/min/max render time;
- average/max LED show time;
- `showSkips`;
- renderer stack watermark.

Existing source anchors:

- serial `s` status via `ActorSystem::printStatus()`;
- renderer status counters in `RendererActor`.

### 2. Render Topology

Report one of:

| Topology | Meaning |
|---|---|
| `unified` | Effect authors `m_leds[0..319]`; later split to physical strips. |
| `zone_unified` | ZoneComposer writes into unified output before shared post stages. |
| `direct_strip` | Effect authors physical strip buffers directly. |

Also report whether the current effect is allowed to use direct dual-strip output by metadata or policy.

### 3. Buffer Ownership

Report:

| Field | Meaning |
|---|---|
| `authoredSurface` | `m_leds`, `strip1/strip2`, or mixed. |
| `correctionSurface` | Which surface colour correction processed. |
| `outputSurface` | Which surface was passed toward LED show. |
| `surfaceMismatch` | True if correction did not process the authored output surface. |

This field exists to prevent the known ambiguity where direct-strip frames can bypass assumptions made around `m_leds`.

### 4. Ordered VP Layers

Report each layer as `active`, `bypassed`, or `not_applicable`:

1. effect render;
2. capture tap A;
3. colour correction;
4. capture tap B;
5. tone map;
6. unified-to-strip split or direct-strip route;
7. silence gates;
8. EdgeMixer;
9. capture tap C;
10. LED driver / FastLED / RMT;
11. wire-time fence.

### 5. Colour Correction State

Report effective state, not just intended config:

- mode;
- enabled;
- auto-exposure enabled and target;
- gamma enabled and value;
- LUT generation ID;
- LUT samples at 0, 32, 64, 128, 192, 255;
- saturation boost;
- white guardrail;
- brown guardrail;
- value clamp.

Existing source anchors:

- REST helper fields in `ColorCorrectionHandlers.cpp:22-52`;
- SerialJSON mirrors in `SerialJsonGateway.cpp:87-115`.

### 6. Tone Map State

Report:

- whether the current effect needs tone mapping;
- tone-map mode;
- additive-output reason if known.

Use `EFFECTS_BEHAVIORAL_REFERENCE.md:35-54` as the behavioural anchor for existing tone-map gate meaning.

### 7. Silence Policy State

Report:

- global `silentScale`;
- whether the effect inherits global silence scaling;
- whether the effect has an explicit policy;
- whether the observed fade/dim is effect-authored or output-layer-applied.

This is required because ambient effects can look audio-reactive if global silence scaling is applied after render.

### 8. EdgeMixer State

Report:

- mode;
- spread;
- strength;
- spatial parameter;
- temporal parameter;
- whether MIRROR baseline is active.

Existing source anchors:

- WS status in `WsDeviceCommands.cpp:79-90`;
- serial `#` output in `SerialCLI.cpp:2384-2395`.

### 9. Capture/Tap State

Report:

- capture enabled;
- streaming state;
- format;
- tap labels;
- write/assemble timing;
- backpressure counters;
- drop counters;
- last effect/palette/frame captured.

Existing source anchors:

- `CaptureStreamer.cpp:647-679`;
- `docs/CAPTURE_PIPELINE_REFERENCE.md:90-97`.

### 10. Transport Guard State

Report:

- LED count;
- physical strip lengths;
- expected WS2812 wire time;
- observed show time;
- whether the fence is active;
- `showSkips`.

Do not weaken or bypass the fence. The VP audit and validation protocol treat wire-time protection as a correctness guard, not an aesthetic layer.

## Human Output Sketch

```text
=== VP Stack ===
effect: 0x2102 Beat Parity Sprite
topology: unified
authoredSurface: m_leds[320]
correctionSurface: m_leds[320]
surfaceMismatch: false

layers:
  effect_render: active p99_us=...
  colour_correction: active mode=BOTH gamma=on value=...
  tone_map: active reason=needsToneMap(effect)
  split: unified_to_strip
  silence: active silentScale=...
  edge_mixer: MIRROR
  led_show: active avg_us=... max_us=... showSkips=0
  wire_fence: active expected_us=...

capture:
  tap_a=...
  tap_b=...
  tap_c=...
```

## Non-Goals

- Do not add new correction policy.
- Do not move colour correction to direct-strip buffers.
- Do not alter gamma, saturation, white guardrail, EdgeMixer, or silence defaults.
- Do not add STA or REST dependency.
- Do not treat this command as a visual sign-off.

## Implementation Notes For Future Work

Preferred shape:

1. Add a small immutable snapshot struct owned by renderer/status code.
2. Populate it at frame-boundary or command time without allocation in render.
3. Print through SerialCLI first.
4. Add SerialJSON only if automated tooling needs it.
5. Add REST/WS only after protocol YAML updates.

Acceptance:

- command is read-only;
- no render-path heap allocation;
- no FPS regression;
- serial output includes topology, buffer ownership, correction state, EdgeMixer, capture, and transport guard state;
- hardware `s` still reports `showSkips=0` under normal load.
