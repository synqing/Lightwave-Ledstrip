# K1 Song-Aware Director Control Surface Schema - 2026-05-12

RBDO label: GROUNDED.

## Scope

This document defines a contract-only product control model using existing REST/WS surfaces. It does not add endpoints, change firmware, save NVS, alter production defaults, or author implementation logic.

Grounding:

- The execution plan explicitly forbids firmware changes, VP substrate changes, timing/cadence work, NVS saves, and production default changes for this phase (`firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:17-25`).
- Lane C requires modes and tunables for Off, Subtle, Balanced, High Energy, and Song-Aware Director (`firmware-v3/docs/research/k1_song_aware_director_agent_execution_plan_2026-05-12.md:86-103`).
- Current REST supports current effect, effect parameters, global parameters, effects metadata/families, audio state, shows, narrative, stimulus, and EdgeMixer (`docs/protocol/k1-rest-contract.yaml:96-166`, `:222-269`, `:388-402`, `:601-638`, `:829-843`).
- Current WS supports current effect, effect metadata/families, effect parameters, global parameters, EdgeMixer, narrative, shows, stimulus, audio subscriptions, and status telemetry (`docs/protocol/k1-ws-contract.yaml:115-293`, `:1039-1102`, `:1290-1424`, `:1499-1544`, `:1673-1700`, `:1930-1962`).

## Contract Stance

There is no current `songAware.*` REST or WS command in the supplied contracts. Therefore, for this research phase, `songAware` is a **runtime policy schema** for clients/tests to reason about existing setters and telemetry. Any later firmware implementation must update the protocol contract first.

## Product Modes

| Mode | Meaning | Allowed automatic action | Effect-ID switching |
|---|---|---|---|
| Off | Director disabled. | None. | None. |
| Subtle | Low-amplitude parameter shaping. | Global/effect parameter updates only. | Prohibited. |
| Balanced | Default research target. | Parameter updates; family posture only after validation. | Prohibited. |
| High | Stronger motion/intensity envelope. | Parameter updates; family posture only after validation. | Prohibited unless later constrained-switching gate passes. |
| Director | Full opt-in umbrella. | Parameter mode first, family morphing second, constrained switching only after proof. | Disabled until separate Lane D PASS and Captain decision. |

## Runtime Policy Schema

```yaml
songAware:
  enabled: boolean
  mode: off | subtle | balanced | high | director
  sensitivity: 0.0-1.0
  intensityScalar: 0.0-1.0
  motionScalar: 0.0-1.0
  silencePolicy: hold | fade_to_ambient | manual_hold
  minDwellMs: uint32
  switchCooldownMs: uint32
  confidenceFloor: 0.0-1.0
  allowedFamilies: uint8[]
  prohibitedFamilies: uint8[]
  allowedEffects: uint16[]
  prohibitedEffects: uint16[]
  familyMorphing: boolean
  constrainedSwitching: boolean
```

## Existing Surface Mapping

| Policy need | Current surface | Evidence |
|---|---|---|
| Fixed effect baseline | REST `/api/v1/effects/current`, `/api/v1/effects/set`; WS `effects.setCurrent` | `docs/protocol/k1-rest-contract.yaml:96-115`; `docs/protocol/k1-ws-contract.yaml:115-130` |
| Global parameter modulation | REST `/api/v1/parameters`; WS `parameters.set` | `docs/protocol/k1-rest-contract.yaml:147-166`; `docs/protocol/k1-ws-contract.yaml:248-293` |
| Effect parameter modulation | REST `/api/v1/effects/parameters`; WS `effects.parameters.set` | `docs/protocol/k1-rest-contract.yaml:126-139`; `docs/protocol/k1-ws-contract.yaml:216-246` |
| Family allow/prohibit lists | REST `/api/v1/effects/families`; WS metadata/family queries | `docs/protocol/k1-rest-contract.yaml:117-145`; `docs/protocol/k1-ws-contract.yaml:172-214` |
| Narrative status/config | REST `/api/v1/narrative/status`, `/api/v1/narrative/config`; WS `narrative.getStatus`, `narrative.config` | `docs/protocol/k1-rest-contract.yaml:627-638`; `docs/protocol/k1-ws-contract.yaml:1290-1319` |
| Show ownership | REST `/api/v1/shows/current`, `/api/v1/shows/control`; WS `show.*` and `show.cue.inject` | `docs/protocol/k1-rest-contract.yaml:618-624`; `docs/protocol/k1-ws-contract.yaml:1320-1424` |
| Stimulus validation | REST `/api/v1/stimulus/*`; WS `stimulus.*` | `docs/protocol/k1-rest-contract.yaml:388-402`; `docs/protocol/k1-ws-contract.yaml:1499-1544` |
| EdgeMixer baseline | REST `/api/v1/edgeMixer`; WS `edge_mixer.*` | `docs/protocol/k1-rest-contract.yaml:829-843`; `docs/protocol/k1-ws-contract.yaml:1039-1102` |
| Telemetry | REST `/api/v1/device/status`; WS `status.subscribe`, `device.getStatus`, `status` | `docs/protocol/k1-rest-contract.yaml:43-46`; `docs/protocol/k1-ws-contract.yaml:1673-1700`, `:1772-1787`, `:1930-1962` |

## Ownership Precedence

1. **Show**: active show playback or cue injection wins. Cue execution can change effect, parameters, zones, transitions, narrative, and palette (`firmware-v3/docs/reference/fsm-reference.md:112-125`).
2. **Manual**: direct user commands win when no show/cue owns the surface.
3. **Director**: may act only when enabled, above confidence floor, outside dwell/cooldown, health gates pass, and lists allow the target family/effect.

## Telemetry Schema

```yaml
songAwareTelemetry:
  effectiveMode: off | subtle | balanced | high | director
  owner: show | manual | director | none
  suppressedReason: disabled | show_active | manual_hold | low_confidence | dwell | cooldown | prohibited | health_gate
  currentSongState: silence | ambient | steady | build | drop | breakdown | dense | transition | unknown
  confidence: 0.0-1.0
  dwellRemainingMs: uint32
  cooldownRemainingMs: uint32
  lastDecisionAtMs: uint32
  lastAction: none | parameter_update | family_morph | constrained_switch | rollback
  effectiveAllowedFamilies: uint8[]
  effectiveProhibitedFamilies: uint8[]
  activeEffectId: uint16
  activeEffectName: string
  activeFamilyId: uint8
  health:
    fps: float
    frameTimeUs: uint32
    showSkips: uint32
    failures: uint32
    rmtErrors: uint32
    underruns: uint32
    freeHeap: uint32
```

## Explicit Non-Goals

- No `songAware.*` endpoint in this phase.
- No NVS saves or preset writes.
- No production default changes.
- No render-path decision logic.
- No unconstrained effect switching.
- No AP/STA or network mode changes.
