# AS-IS Architecture Atlas

## Components
- RendererActor is the core visual engine with exclusive frame ownership and high-frequency scheduling. ([O28396@2026-02-09T15:28:51.860Z][Lightwave-Ledstrip] [O28381@2026-02-09T15:09:36.033Z][Lightwave-Ledstrip] [O28380@2026-02-09T15:09:13.887Z][Lightwave-Ledstrip])
- Audio pipeline computes analysis features (tempo/chroma/flux) and feeds render-time controls. ([O28426@2026-02-09T16:01:54.129Z][Lightwave-Ledstrip] [O28421@2026-02-09T15:59:10.592Z][Lightwave-Ledstrip] [O28418@2026-02-09T15:56:26.177Z][Lightwave-Ledstrip])

## State ownership
- State ownership is actor-mediated with explicit boundaries between render, audio, and control layers. ([O28417@2026-02-09T15:56:19.430Z][Lightwave-Ledstrip] [O28409@2026-02-09T15:52:48.212Z][Lightwave-Ledstrip] [O28399@2026-02-09T15:33:08.292Z][Lightwave-Ledstrip])

## Timing model
- Timing model couples audio hop cadence with render cadence under deterministic frame budgets. ([O28339@2026-02-09T14:35:25.424Z][Lightwave-Ledstrip] [O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28334@2026-02-09T14:33:07.926Z][Lightwave-Ledstrip])

## Control planes
- Control plane combines REST endpoints and WebSocket streams with fallback handling. ([O28423@2026-02-09T15:59:51.150Z][Lightwave-Ledstrip] [O28394@2026-02-09T15:21:47.057Z][Lightwave-Ledstrip] [O28380@2026-02-09T15:09:13.887Z][Lightwave-Ledstrip])

## Boundaries
- Network behaviour includes AP/STA modes with discovery and IP fallback strategies. ([O28425@2026-02-09T16:01:39.531Z][Lightwave-Ledstrip] [O28424@2026-02-09T16:01:31.824Z][Lightwave-Ledstrip] [O28421@2026-02-09T15:59:10.592Z][Lightwave-Ledstrip])
- iOS/Tab5/dashboard clients are boundary consumers of firmware-owned state and contracts. ([O28360@2026-02-09T14:58:01.637Z][Lightwave-Ledstrip] [O28357@2026-02-09T14:50:45.810Z][Lightwave-Ledstrip] [O28336@2026-02-09T14:33:33.081Z][Lightwave-Ledstrip])

## Implemented vs aspirational
Aspirational (requires follow-up):
- RendererActor is the core visual engine with exclusive frame ownership and high-frequency scheduling.
- Audio pipeline computes analysis features (tempo/chroma/flux) and feeds render-time controls.
- State ownership is actor-mediated with explicit boundaries between render, audio, and control layers.
- Timing model couples audio hop cadence with render cadence under deterministic frame budgets.
- Control plane combines REST endpoints and WebSocket streams with fallback handling.
- Network behaviour includes AP/STA modes with discovery and IP fallback strategies.
- iOS/Tab5/dashboard clients are boundary consumers of firmware-owned state and contracts.
