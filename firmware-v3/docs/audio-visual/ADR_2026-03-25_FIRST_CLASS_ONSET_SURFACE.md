# ADR 2026-03-25: First-Class Onset Surface

## Status

Accepted

## Context

`ControlBusFrame` already carried onset-related transport fields, but effect code
 only had partial convenience accessors. That produced three recurring problems:

- new effect work had poor discoverability for onset semantics;
- raw `ctx.audio.controlBus.*` reach-through leaked transport details into
  plugin code;
- future work such as onset-backed BeatPulse variants and semantic runtime
  mappings had no stable effect-facing contract.

The renderer now constructs a dedicated `ctx.audio.onset` surface every frame.
This ADR freezes the semantics of that surface and the fallback rules that
allow older backends and wrappers to coexist during migration.

## Decision

Effects consume onset data through `ctx.audio.onset.*` plus compatibility
wrappers such as `isOnBeat()`, `beatStrength()`, `isKickHit()`,
`hasOnsetEvent()`, `onsetEnv()`, and `onsetEvent()`.

`ctx.audio.controlBus` remains a transport/debug object. New effect code must
not read onset state directly from `controlBus`.

## Surface Definition

`ctx.audio.onset` exposes six semantic channels:

- `beat`
- `downbeat`
- `transient`
- `kick`
- `snare`
- `hihat`

Each channel carries the same fields:

- `fired`
  - Single-frame or single-render pulse flag for event-driven logic.
- `strength01`
  - Instantaneous trigger strength, normalised into `[0, 1]`.
- `level01`
  - Continuous held/decayed level, normalised into `[0, 1]`.
- `ageMs`
  - Milliseconds since the last fire for that channel.
- `intervalMs`
  - Milliseconds between the two most recent fires.
- `sequence`
  - Monotonic fire counter for that channel.
- `reliable`
  - `true` when the channel comes from a trustworthy native source for the
    current backend/input mode; `false` when degraded, unavailable, or blocked.

`ctx.audio.onset` also carries timing metadata:

- `phase01`
- `bpm`
- `tempoConfidence`
- `timingReliable`

Raw detector diagnostics live under `ctx.audio.onset.raw`:

- `flux`
- `env`
- `event`
- `bassFlux`
- `midFlux`
- `highFlux`

These raw values are advanced/debug oriented. They are not the default API for
general effect authors and are not treated as stable runtime mapping sources.

## Mapping Matrix

| Onset field | Source | Notes |
|-------------|--------|-------|
| `beat.*` | `MusicalGridSnapshot.beat_*` | Semantic timing channel. |
| `downbeat.*` | `MusicalGridSnapshot.downbeat_tick` + held beat strength | Downbeat inherits beat-strength envelope. |
| `transient.*` | `ControlBusFrame.onsetEvent/onsetEnv` | Broadband onset semantics from detector. |
| `kick.*` | `ControlBusFrame.kickTrigger/onsetBassFlux` | Detector-backed percussion trigger. |
| `snare.*` | `ControlBusFrame.snareTrigger` + max(`snareEnergy`, `onsetMidFlux`) | Hybrid semantic level. |
| `hihat.*` | `ControlBusFrame.hihatTrigger` + max(`hihatEnergy`, `onsetHighFlux`) | Hybrid semantic level. |
| `phase01` | `MusicalGridSnapshot.beat_phase01` | Render-domain timing phase. |
| `bpm` | `MusicalGridSnapshot.bpm_smoothed` | Render-domain tempo estimate. |
| `tempoConfidence` | `MusicalGridSnapshot.tempo_confidence` | Tempo trust signal. |

## Reliability Rules

- `beat.reliable` and `downbeat.reliable` require live audio plus tempo
  confidence `>= 0.25`.
- Detector-backed channels (`transient`, `kick`, raw onset diagnostics) require
  live audio and no Trinity override.
- `snare` and `hihat` are marked unreliable under Trinity because their live
  detector semantics are not active there, even if fallback transport fields are
  non-zero.
- When `reliable=false`, effects may still inspect the channel, but shared
  trigger policy should prefer a higher-confidence fallback.

## Compatibility Rules

- Existing flat helpers remain for one release cycle.
- Flat helpers resolve to the new onset surface first, then fall back to legacy
  transport fields where needed.
- When runtime mappings are widened after the live validation gate passes, they
  must expose only semantic onset fields:
  - `ONSET_ENV`
  - `ONSET_EVENT`
  - `KICK_EVENT`
  - `SNARE_EVENT`
  - `HIHAT_EVENT`

Raw flux magnitudes remain debug/effect-only and must stay out of runtime
mapping until live detector scale is fully locked.

## Consequences

Positive:

- effect authors get a discoverable, backend-aware onset API;
- shared trigger policy can work against one semantic contract;
- runtime mapping expansion can target semantic onset channels rather than raw
  transport fields;
- renderer-side onset tracking becomes unit-testable through shared semantics
  helpers.

Negative:

- renderer now owns a small amount of per-channel onset bookkeeping;
- documentation and examples must be kept in sync with the new contract;
- compatibility wrappers increase short-term API surface until the migration
  window closes.
