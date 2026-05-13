# Director V1 Visual Review Setup

RBDO label: GROUNDED

Date: 2026-05-12

## Boundary

- No firmware/source edits were made during this visual-review run.
- No tuning was performed.
- No new effects were added.
- No NVS save command was used.
- No REST or WebSocket control was used for validation.
- Director Mode only was used.

## Device

- Serial port: `/dev/cu.usbmodem1101`
- Device identity from PlatformIO: USB VID:PID `303A:1001`, serial `B4:3A:45:A5:87:F8`

## Fixed Starting State

- Starting effect: `0x1302 K1 Waveform`
- Brightness: `160`
- Speed: `27`
- Intensity: `128`
- Saturation: `128`
- Complexity: `128`
- Variation: `0`
- Palette: `10 Vintage 01`
- EdgeMixer: `mirror`, spatial `uniform`, temporal `static`

## Director Preflight

Serial preflight confirmed:

- `songaware status` works.
- `songaware mode director` works.
- `songaware switching on` works in Director Mode.
- `familyMorphing=false`
- `constrainedSwitching=true`
- `automaticEffectSwitches=0` before the audio pass.
- Hard counters were zero before the audio pass:
  - `show_skips=0`
  - `failures=0`
  - `rmt_errors=0`
  - `underruns=0`

## Track Availability

The exact Captain trigger track was not identifiable from local evidence available in this session. A local build/drop electronic file was available and used for serial-only Director runtime evidence:

`/Users/spectrasynq/Music/Music/Media.localized/Music/Kiro tv/Unknown Album/Tech House drums Loop - 124 BPM  + Bass.mp3`

This run cannot satisfy the visual product gate because no phone/camera video source was available to this agent.
