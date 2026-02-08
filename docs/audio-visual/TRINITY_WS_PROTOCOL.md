# Trinity Sync WebSocket Protocol (PRISM → LightwaveOS)

This document describes the **Trinity sync** WebSocket message protocol used to feed PRISM/Trinity analysis into LightwaveOS firmware.

## Requirements

- Firmware build with `FEATURE_AUDIO_SYNC=1` (e.g. PlatformIO env `esp32dev_audio`).
- WebSocket endpoint: `ws://<device-host>/ws`
- All messages use the Lightwave WebSocket envelope key `type`:

```json
{ "type": "trinity.macro", "...": "..." }
```

## Message Types

### `trinity.sync`

Transport/playback control. Typically sent on start/stop/pause/resume/seek.

Required fields:
- `action`: `"start" | "stop" | "pause" | "resume" | "seek"`
- `position_sec`: number (>= 0)

Optional:
- `bpm`: number (defaults to 120 if omitted)
- `_id`: integer (if present and > 0, firmware replies `{ "_type": "ack", "_id": <same> }`)

Example:
```json
{ "type": "trinity.sync", "action": "start", "position_sec": 0.0, "bpm": 120.0, "_id": 1 }
```

### `trinity.beat`

External beat clock injection (BPM + phase + beat/downbeat ticks).

Required fields:
- `bpm`: number (30–300)
- `beat_phase`: number in `[0, 1)`

Optional:
- `tick`: boolean (beat boundary pulse)
- `downbeat`: boolean (bar boundary pulse)
- `beat_in_bar`: integer (packed to 2 bits in firmware; currently supports 0–3)
- `beat_index`: integer (ignored by firmware; safe to include)

Example:
```json
{ "type": "trinity.beat", "bpm": 120, "beat_phase": 0.0, "tick": true, "downbeat": true, "beat_in_bar": 0 }
```

### `trinity.macro`

Macro-level continuous controls, typically streamed at a fixed cadence (e.g. 10–30 FPS).

Firmware currently consumes (clamped to `[0,1]`):
- `energy`
- `vocal_presence`
- `bass_weight`
- `percussiveness`
- `brightness`

Other keys may be present (and are ignored by firmware today), allowing contract evolution.

Example:
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

### `trinity.segment`

Music structure / section change events (semantic layer).

Required fields:
- `index`: integer (0–255)
- `label`: string (1–64 chars)

Optional (recommended):
- `start`: number (seconds, >= 0)
- `end`: number (seconds, >= start)

Notes:
- Firmware hashes `label` to a compact `labelHash16` (FNV-1a folded) to avoid string storage in actor messages.
- Segment events are stored in `RendererActor` and also published on the `MessageBus` as `MessageType::TRINITY_SEGMENT`.

Example:
```json
{ "type": "trinity.segment", "index": 3, "label": "chorus", "start": 45.12, "end": 67.90 }
```

## Recommended Pipeline

1. Use `trinity.sync` to establish “external sync mode”.
2. Stream `trinity.beat` to drive smooth beat phase.
3. Stream `trinity.macro` for continuous audio-reactive control.
4. Send `trinity.segment` when structure/section changes.
5. Optionally, map segments into Lightwave control actions (effect/palette/parameters) using `tools/trinity-bridge/`.

