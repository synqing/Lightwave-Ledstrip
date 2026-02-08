# Trinity → Lightwave Bridge (Host-Side)

This tool creates a practical bridge between **PRISM/Trinity** (audio producer) and **LightwaveOS** (visual consumer).

It reads **Trinity WebSocket messages** as JSON Lines (`.jsonl`) from stdin and forwards them to a Lightwave device WebSocket (`/ws`). Optionally, it applies **segment-driven semantic mappings** (e.g. `chorus` → change effect + brightness).

## Why this exists

- PRISM/Trinity emits rich, *semantic* information (`trinity.segment`, `trinity.beat`, `trinity.macro`, `trinity.sync`).
- Lightwave firmware already supports beat/macro/sync ingestion; this repo now adds **`trinity.segment`** support as well.
- This bridge lets you combine:
  - **Music transport** (`trinity.sync`)
  - **Beat clock** (`trinity.beat`)
  - **Macro curves** (`trinity.macro`)
  - **Structure/sections** (`trinity.segment`)
  - …and map them into Lightwave control actions.

## Run

Forward Trinity messages to a device:

```bash
cat messages.jsonl | node tools/trinity-bridge/trinity-bridge.mjs --device lightwaveos.local
```

Forward + apply semantic mappings:

```bash
cat messages.jsonl | node tools/trinity-bridge/trinity-bridge.mjs \
  --device ws://192.168.0.10/ws \
  --mapping tools/trinity-bridge/example-mapping.json
```

Use a mapping exported from PRISM Dashboard V4 (“k1_mapping_v1”):

```bash
cat messages.jsonl | node tools/trinity-bridge/trinity-bridge.mjs \
  --device ws://192.168.0.10/ws \
  --mapping /path/to/k1_mapping_v1.json
```

Dry-run (prints what would be sent):

```bash
cat messages.jsonl | node tools/trinity-bridge/trinity-bridge.mjs --device lightwaveos.local --dry-run
```

## Input format

JSON lines where each object includes a `type` field, e.g.:

```json
{"type":"trinity.sync","action":"start","position_sec":0,"bpm":120}
{"type":"trinity.segment","index":3,"label":"chorus","start":45.12,"end":67.90}
{"type":"trinity.beat","bpm":120,"beat_phase":0.0,"tick":true,"downbeat":true,"beat_in_bar":0,"beat_index":128}
{"type":"trinity.macro","t":12.345,"energy":0.8,"vocal_presence":0.6,"bass_weight":0.7,"percussiveness":0.5,"brightness":0.9}
```

## Mapping file format (JSON)

Two supported formats:

### A) `segmentRules` (simple)

`segmentRules` is a list of match → actions rules:

- `match`:
  - `{ "pattern": "chorus|drop", "flags": "i" }` (regex), or
  - `"chorus"` (exact match)
- `actions`: a list of Lightwave WebSocket commands to send (must include `type`).

See `tools/trinity-bridge/example-mapping.json`.

### B) `k1_mapping_v1` (PRISM export)

If the JSON contains `"schema": "k1_mapping_v1"`, the bridge will:

- Match incoming `trinity.segment` by `id` (preferred) or `index`
- Send:
  - `effects.setCurrent` when `motion_id` is present
  - `palettes.set` when `palette_id` is present
- Use `macros.segment_fade_ms` as the transition duration for `effects.setCurrent`

Optional: add alias maps inside the JSON if `palette_id`/`motion_id` are strings:

```json
{
  "lightwave": {
    "paletteAliases": { "neon": 3, "ocean": 7 },
    "effectAliases": { "beat_pulse": 69, "bass_breath": 71 }
  }
}
```

## Notes

- Commands are sent using Lightwave’s WebSocket envelope key `type` (not `cmd`).
- Use a firmware build with `FEATURE_AUDIO_SYNC=1` (e.g. `esp32dev_audio`) for Trinity ingestion.
