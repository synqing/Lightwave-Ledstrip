# Findings: PRISM/Trinity ↔ Lightwave Integration

## Quick Summary
- Lightwave already has an internal DSP “ControlBus” (ControlBusFrame) consumed by effects via `EffectContext`.
- Lightwave already implements a WebSocket Trinity ingestion surface (behind `FEATURE_AUDIO_SYNC`):
  - `trinity.sync` (start/stop/pause/resume/seek + position + optional bpm, optional `_id` ACK)
  - `trinity.beat` (bpm, beat_phase, tick/downbeat flags, beat_in_bar)
  - `trinity.macro` (energy, vocal_presence, bass_weight, percussiveness, brightness) in `[0,1]`
- RendererActor can switch audio source between “live” ControlBus frames and Trinity proxy frames (`TrinityControlBusProxy`), with 250ms staleness protection.
- PRISM Dashboard V4 canonicalises segment labels into stable tokens: `intro`, `verse`, `chorus`, `bridge`, `breakdown`, `drop`, `solo`, `inst`, `start`, `end`.
- PRISM Dashboard V4 can export a `k1_mapping_v1` JSON containing per-segment `palette_id` and `motion_id` assignments + `segment_fade_ms`.

## Lightwave-Ledstrip: Relevant Entry Points
- Docs to read first:
  - `docs/shows/README.md`
  - `firmware/v2/docs/PHASE4_SHOWDIRECTOR_ACTOR.md`
  - `firmware/v2/src/core/README.md` (Show System section)
  - `docs/audio-visual/audio-visual-semantic-mapping.md`

## PRISM/Trinity: Relevant Entry Points
- `SpectraSynq.trinity/Trinity.s3hostside/trinity_contract/contract_types.ts` defines the canonical “Trinity Contract”:
  - Contract meta includes `rhythm` (beats/downbeats), `structure.segments` (labelled sections), and optional `blobs`.
  - WebSocket message types include **`trinity.segment`** in addition to beat/macro/sync.
- `SpectraSynq.trinity/Trinity.s3hostside/trinity_contract/trinity_loader.ts` shows a reference host implementation that streams:
  - `trinity.beat` on beat/downbeat boundaries
  - `trinity.segment` on section changes
  - `trinity.macro` at fixed FPS

## Protocol / Mapping Notes
- (Schema ideas, parameter naming, expected ranges, smoothing strategies.)

### Trinity WS Schema (as implemented in firmware)
- `trinity.sync` required fields: `action` ("start"|"stop"|"pause"|"resume"|"seek"), `position_sec` (>=0). Optional: `bpm` (default 120), `_id` (int64; if present, server replies `{_type:"ack", _id:<same>}`).
- `trinity.beat` required fields: `bpm` (30–300), `beat_phase` ([0,1)). Optional: `tick` (bool), `downbeat` (bool), `beat_in_bar` (int; packed to 2 bits in message).
- `trinity.macro` optional floats (clamped to [0,1]): `energy`, `vocal_presence`, `bass_weight`, `percussiveness`, `brightness`.

### Gap Identified (Isolation Point)
- Trinity contract/loader emits `trinity.segment` (section/structure). This was previously missing from Lightwave firmware, so messages were rejected as unknown types.

### Resolution Implemented
- Added firmware support for `trinity.segment` and a host-side bridge that can consume PRISM’s `k1_mapping_v1` export to drive Lightwave effect/palette selection on segment changes.

## Decisions & Rationale (Expanded)
- (Keep design rationale here once the approach stabilises.)

## References
- Paths, commands, links:
  - `docs/api/api-v1.md`
  - `firmware/v2/src/network/WebServer.cpp`
