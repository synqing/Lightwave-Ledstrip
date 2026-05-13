# Implementation Diff Summary

RBDO label: GROUNDED

## Files Changed For Director V1

- `firmware-v3/src/core/songaware/SongAwareDirector.h`
- `firmware-v3/src/core/songaware/SongAwareDirector.cpp`
- `firmware-v3/src/core/actors/RendererActor.cpp`
- `firmware-v3/src/core/actors/ShowDirectorActor.cpp`
- `firmware-v3/src/serial/SerialCLI.cpp`

## Core Runtime Changes

- Added mode semantics: `off`, `parameter`, and `director`.
- Kept `on` as a serial-compatible alias for `parameter`.
- Added song states: `silence`, `ambient`, `steady`, `build`, `drop`, `breakdown`, `dense`, `transition`, `unknown`.
- Added a fixed Director V1 allowlist and state-to-visual-language policy.
- Added switch gates: minimum dwell 8 seconds, cooldown 20 seconds, maximum 2 switches per 60 seconds, confidence floor, health counters, manual ownership, show ownership, and allowlist validation.
- Added runtime status telemetry: selected effect, visual language, family, switch reason, dwell/cooldown remaining, switch count, health counters, and audio summary.
- Preserved parameter overlays as support behaviour, not the completion criterion.

## Renderer Integration

Director evaluation runs before single-effect render and after audio-context preparation. The pixel/effect render path consumes already-selected state/parameters; it does not scan the registry or make uncontrolled decisions inside effect render loops.

Source anchors:

- External/manual ownership marking: `firmware-v3/src/core/actors/RendererActor.cpp:653`.
- Director evaluation and switch application: `firmware-v3/src/core/actors/RendererActor.cpp:1961`.
- Parameter overlay application remains before effect render: `firmware-v3/src/core/actors/RendererActor.cpp:2125`.

## Serial Surface

Implemented or preserved:

- `songaware status`
- `songaware off`
- `songaware on`
- `songaware mode parameter`
- `songaware mode director`
- `songaware switching on`
- `songaware switching off`
- `songaware reset`

Source anchors:

- Status fields: `firmware-v3/src/serial/SerialCLI.cpp:101`.
- Commands: `firmware-v3/src/serial/SerialCLI.cpp:524`.

## Boundary Checks

- No production default change was added.
- No REST/WS dependency is required for basic validation.
- No NVS save route was added or invoked.
- No family morphing was enabled.
- No unconstrained effect roulette was introduced.
