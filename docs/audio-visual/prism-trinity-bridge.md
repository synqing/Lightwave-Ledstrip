# PRISM/Trinity → Lightwave Semantic Bridge

This document describes the **real-time bridge** between the PRISM/Trinity analysis stack (audio producer) and LightwaveOS firmware (visual consumer).

The bridge is intentionally **event + intent** oriented:
- **Music timing** (tempo/beat/phase) drives the renderer beat clock.
- **Macro features** drive the `ControlBusFrame` via `TrinityControlBusProxy` so existing audio-reactive effects work in “offline ML sync mode”.
- **Semantic structure** (segments such as verse/chorus/drop) modulates `NarrativeEngine` and applies smooth global parameter sweeps via `ShowDirectorActor`.

## Transport

LightwaveOS consumes PRISM/Trinity messages over **WebSocket**:
- Endpoint: `ws://<device-ip>/ws`
- Envelope: JSON messages MUST include a `type` string (not `cmd`).

Example:
```json
{ "type": "trinity.macro", "energy": 0.8 }
```

## Message Contract (v1)

### 1) `trinity.sync` (transport control)

```json
{
  "type": "trinity.sync",
  "action": "start|stop|pause|resume|seek",
  "position_sec": 0.0,
  "bpm": 120.0
}
```

### 2) `trinity.beat` (beat clock injection)

```json
{
  "type": "trinity.beat",
  "bpm": 120.0,
  "beat_phase": 0.5,
  "tick": true,
  "downbeat": false,
  "beat_in_bar": 2
}
```

### 3) `trinity.macro` (offline macro features)

```json
{
  "type": "trinity.macro",
  "t": 12.345,
  "energy": 0.8,
  "vocal_presence": 0.6,
  "bass_weight": 0.7,
  "percussiveness": 0.5,
  "brightness": 0.9
}
```

### 4) `trinity.segment` (semantic structure marker)

```json
{
  "type": "trinity.segment",
  "index": 5,
  "label": "chorus",
  "start": 42.0,
  "end": 58.0
}
```

Notes:
- `label` should be a **canonical lower-case token** (e.g., `intro`, `verse`, `chorus`, `drop`, `bridge`, `breakdown`, `inst`, `start`, `end`).
- Firmware hashes the label (FNV-1a folded to 16-bit) for compact transport/storage.
- Extra fields are permitted and will be ignored by current firmware handlers.

## Firmware Behaviour

### Beat + macros (RendererActor)

- `trinity.beat` injects an external beat clock (`MusicalGrid` / `EsBeatClock`).
- `trinity.macro` updates `TrinityControlBusProxy`, which maps PRISM macro features into `ControlBusFrame` fields.
- When `trinity.sync` is active and the proxy is fresh, effects consume the proxy frame instead of live audio.

### Segments (semantic → narrative + sweeps)

When `trinity.segment` changes, the firmware publishes a `TRINITY_SEGMENT` message on `MessageBus`. `ShowDirectorActor` subscribes to this event and, **when no show is currently playing**, applies a default semantic mapping:

| Segment label | Narrative phase | Typical tension | Brightness target | Speed target | Mood |
|--------------|-----------------|----------------|-------------------|--------------|------|
| `start` | REST | low | low | very low | smooth |
| `intro` | BUILD | low–mid | low | low | smooth |
| `verse` | BUILD | mid | mid | low–mid | smooth |
| `chorus` | HOLD | high | high | high | reactive |
| `drop` | HOLD | very high | max | very high | very reactive |
| `bridge` | RELEASE | mid | mid | mid | mixed |
| `breakdown` / `inst` | REST | low–mid | low | low–mid | smooth |
| `end` | REST | none | off | low | very smooth |

Implementation notes:
- `NarrativeEngine` tempo still follows tension (higher tension → faster tempo).
- Narrative phase duration prefers the PRISM segment length (clamped).
- Brightness and speed are applied via `ParameterSweeper` (no render-thread allocations).
- If a choreographed show is playing, segment mapping is ignored to avoid conflicting control planes.

## Debugging Checklist

On the device serial log, you should see:
- `WsTrinity`: `trinity.segment: ...`
- `RendererActor`: `TRINITY_SEGMENT: ...`
- `ShowDirector`: `Trinity segment intent: ...` (only when no show is playing)

