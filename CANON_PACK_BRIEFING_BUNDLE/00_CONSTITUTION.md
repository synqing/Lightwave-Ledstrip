# Constitution

## Mission
- The project mission is an audio-reactive LED controller experience with web/app control. ([O28425@2026-02-09T16:01:39.531Z][Lightwave-Ledstrip] [O28424@2026-02-09T16:01:31.824Z][Lightwave-Ledstrip] [O28417@2026-02-09T15:56:19.430Z][Lightwave-Ledstrip])

## Invariants
1. Effects originate from centre LEDs 79/80 and propagate outward/inward, not linear sweeps. (citations: [O28415@2026-02-09T15:54:01.001Z][Lightwave-Ledstrip] [O28246@2026-02-08T21:45:07.109Z][Lightwave-Ledstrip] [O28235@2026-02-08T21:42:04.041Z][Lightwave-Ledstrip])
2. Linear sweep rendering is rejected in favour of centre-origin motion semantics. (citations: [O28421@2026-02-09T15:59:10.592Z][Lightwave-Ledstrip] [O28419@2026-02-09T15:56:29.928Z][Lightwave-Ledstrip] [O28256@2026-02-09T07:10:37.472Z][Lightwave-Ledstrip])
3. Rainbow cycling and full hue-wheel sweeps are disallowed. (citations: [O28393@2026-02-09T15:21:35.350Z][Lightwave-Ledstrip] [O28371@2026-02-09T15:06:39.797Z][Lightwave-Ledstrip] [O28358@2026-02-09T14:56:27.180Z][Lightwave-Ledstrip])
4. Render paths avoid dynamic heap allocation and use static buffers. (citations: [O28387@2026-02-09T15:12:29.525Z][Lightwave-Ledstrip] [O28285@2026-02-09T07:30:48.198Z][Lightwave-Ledstrip] [O28286@2026-02-09T07:30:37.411Z][Lightwave-Ledstrip])
5. Fixed/static render buffers are a non-negotiable requirement for runtime stability. (citations: [O27727@2026-02-07T21:22:34.944Z][Lightwave-Ledstrip] [O27614@2026-02-07T19:17:13.192Z][Lightwave-Ledstrip] [O27564@2026-02-07T18:47:51.083Z][Lightwave-Ledstrip])
6. PSRAM is mandatory for runtime stability and memory budgets. (citations: [O28123@2026-02-08T14:59:11.275Z][Lightwave-Ledstrip] [O28002@2026-02-08T12:45:13.689Z][Lightwave-Ledstrip] [O27979@2026-02-08T12:08:10.154Z][Lightwave-Ledstrip])
7. The renderer targets 120 FPS and strict per-frame execution budgets. (citations: [O28339@2026-02-09T14:35:25.424Z][Lightwave-Ledstrip] [O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28123@2026-02-08T14:59:11.275Z][Lightwave-Ledstrip])
8. Watchdog-safe scheduling and timing behaviour are mandatory system guardrails. (citations: [O28340@2026-02-09T14:40:46.573Z][Lightwave-Ledstrip] [O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28331@2026-02-09T14:32:06.270Z][Lightwave-Ledstrip])
9. REST plus WebSocket contracts are stable control-plane invariants for device control and streaming. (citations: [O28423@2026-02-09T15:59:51.150Z][Lightwave-Ledstrip] [O28407@2026-02-09T15:51:30.569Z][Lightwave-Ledstrip] [O28406@2026-02-09T15:51:06.946Z][Lightwave-Ledstrip])
10. AP/STA fallback behaviour is preserved as a resilience invariant. (citations: [O28425@2026-02-09T16:01:39.531Z][Lightwave-Ledstrip] [O28424@2026-02-09T16:01:31.824Z][Lightwave-Ledstrip] [O28421@2026-02-09T15:59:10.592Z][Lightwave-Ledstrip])
11. Discovery prioritises deterministic IP paths with mDNS as hint/fallback. (citations: [O28416@2026-02-09T15:56:17.412Z][Lightwave-Ledstrip] [O28375@2026-02-09T15:08:08.248Z][Lightwave-Ledstrip] [O28360@2026-02-09T14:58:01.637Z][Lightwave-Ledstrip])
12. Actor boundaries and explicit state ownership are maintained as concurrency invariants. (citations: [O28417@2026-02-09T15:56:19.430Z][Lightwave-Ledstrip] [O28399@2026-02-09T15:33:08.292Z][Lightwave-Ledstrip] [O28398@2026-02-09T15:31:51.123Z][Lightwave-Ledstrip])
13. System limits are enforced through single-source constants with validation checks. (citations: [O28424@2026-02-09T16:01:31.824Z][Lightwave-Ledstrip] [O28238@2026-02-08T21:43:02.979Z][Lightwave-Ledstrip] [O28090@2026-02-08T14:00:34.123Z][Lightwave-Ledstrip])
14. Dual-strip 320-LED WS2812 topology is a fixed product invariant. (citations: [O28421@2026-02-09T15:59:10.592Z][Lightwave-Ledstrip] [O28419@2026-02-09T15:56:29.928Z][Lightwave-Ledstrip] [O28393@2026-02-09T15:21:35.350Z][Lightwave-Ledstrip])

