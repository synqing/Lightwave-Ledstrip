# Decision Register

## Current decisions
- Device control contracts use REST for commands and WebSocket for streaming/state. ([O28423@2026-02-09T15:59:51.150Z][Lightwave-Ledstrip] [O28394@2026-02-09T15:21:47.057Z][Lightwave-Ledstrip] [O28376@2026-02-09T15:08:19.742Z][Lightwave-Ledstrip])
  - citations: [O28423@2026-02-09T15:59:51.150Z][Lightwave-Ledstrip] [O28394@2026-02-09T15:21:47.057Z][Lightwave-Ledstrip] [O28376@2026-02-09T15:08:19.742Z][Lightwave-Ledstrip]
- Connectivity strategy keeps AP/STA fallback behaviour for resilience. ([O28339@2026-02-09T14:35:25.424Z][Lightwave-Ledstrip] [O28327@2026-02-09T14:31:03.742Z][Lightwave-Ledstrip] [O28326@2026-02-09T14:30:47.110Z][Lightwave-Ledstrip])
  - citations: [O28339@2026-02-09T14:35:25.424Z][Lightwave-Ledstrip] [O28327@2026-02-09T14:31:03.742Z][Lightwave-Ledstrip] [O28326@2026-02-09T14:30:47.110Z][Lightwave-Ledstrip]
- Zone/segment boundaries are fixed with explicit composer/state contracts. ([O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28239@2026-02-08T21:43:06.938Z][Lightwave-Ledstrip] [O28136@2026-02-08T17:34:04.290Z][Lightwave-Ledstrip])
  - citations: [O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28239@2026-02-08T21:43:06.938Z][Lightwave-Ledstrip] [O28136@2026-02-08T17:34:04.290Z][Lightwave-Ledstrip]
- Storage/persistence decisions lock in LittleFS and recovery paths. ([O28395@2026-02-09T15:21:59.709Z][Lightwave-Ledstrip] [O28136@2026-02-08T17:34:04.290Z][Lightwave-Ledstrip] [O27538@2026-02-07T18:40:58.097Z][Lightwave-Ledstrip])
  - citations: [O28395@2026-02-09T15:21:59.709Z][Lightwave-Ledstrip] [O28136@2026-02-08T17:34:04.290Z][Lightwave-Ledstrip] [O27538@2026-02-07T18:40:58.097Z][Lightwave-Ledstrip]
- Centre-origin propagation from LEDs 79/80 is locked as a rendering invariant. ([O28235@2026-02-08T21:42:04.041Z][Lightwave-Ledstrip] [O28119@2026-02-08T14:55:46.369Z][Lightwave-Ledstrip] [O27713@2026-02-07T21:19:55.535Z][Lightwave-Ledstrip])
  - citations: [O28235@2026-02-08T21:42:04.041Z][Lightwave-Ledstrip] [O28119@2026-02-08T14:55:46.369Z][Lightwave-Ledstrip] [O27713@2026-02-07T21:19:55.535Z][Lightwave-Ledstrip]
- RendererActor scheduling and buffer ownership decisions are locked for Core 1 execution. ([O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28068@2026-02-08T13:46:48.619Z][Lightwave-Ledstrip] [O28056@2026-02-08T13:35:42.420Z][Lightwave-Ledstrip])
  - citations: [O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28068@2026-02-08T13:46:48.619Z][Lightwave-Ledstrip] [O28056@2026-02-08T13:35:42.420Z][Lightwave-Ledstrip]
- IP-first discovery with mDNS as hint/fallback is favoured for deterministic connection. ([O27573@2026-02-07T18:50:51.764Z][Lightwave-Ledstrip] [O27133@2026-02-06T17:34:26.610Z][Lightwave-Ledstrip] [O26726@2026-02-06T15:57:15.186Z][Lightwave-Ledstrip])
  - citations: [O27573@2026-02-07T18:50:51.764Z][Lightwave-Ledstrip] [O27133@2026-02-06T17:34:26.610Z][Lightwave-Ledstrip] [O26726@2026-02-06T15:57:15.186Z][Lightwave-Ledstrip]
- Audio capture chain decisions nail mic/interface selection and hop/latency budgets. ([O28063@2026-02-08T13:44:11.715Z][Lightwave-Ledstrip] [O28056@2026-02-08T13:35:42.420Z][Lightwave-Ledstrip] [O28055@2026-02-08T13:35:32.828Z][Lightwave-Ledstrip])
  - citations: [O28063@2026-02-08T13:44:11.715Z][Lightwave-Ledstrip] [O28056@2026-02-08T13:35:42.420Z][Lightwave-Ledstrip] [O28055@2026-02-08T13:35:32.828Z][Lightwave-Ledstrip]
- Watchdog-related timing guardrails are treated as mandatory stability constraints. ([O27641@2026-02-07T19:20:47.595Z][Lightwave-Ledstrip] [O26219@2026-02-06T05:40:46.986Z][Lightwave-Ledstrip] [O26214@2026-02-06T05:35:37.199Z][Lightwave-Ledstrip])
  - citations: [O27641@2026-02-07T19:20:47.595Z][Lightwave-Ledstrip] [O26219@2026-02-06T05:40:46.986Z][Lightwave-Ledstrip] [O26214@2026-02-06T05:35:37.199Z][Lightwave-Ledstrip]
- Renderer cadence and effect code are budgeted around 120 FPS operation. ([O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28118@2026-02-08T14:54:32.369Z][Lightwave-Ledstrip] [O28056@2026-02-08T13:35:42.420Z][Lightwave-Ledstrip])
  - citations: [O28338@2026-02-09T14:34:00.302Z][Lightwave-Ledstrip] [O28118@2026-02-08T14:54:32.369Z][Lightwave-Ledstrip] [O28056@2026-02-08T13:35:42.420Z][Lightwave-Ledstrip]
- Rainbow cycling and full hue-wheel sweeps are intentionally excluded. ([O28371@2026-02-09T15:06:39.797Z][Lightwave-Ledstrip] [O28358@2026-02-09T14:56:27.180Z][Lightwave-Ledstrip] [O27650@2026-02-07T19:21:41.689Z][Lightwave-Ledstrip])
  - citations: [O28371@2026-02-09T15:06:39.797Z][Lightwave-Ledstrip] [O28358@2026-02-09T14:56:27.180Z][Lightwave-Ledstrip] [O27650@2026-02-07T19:21:41.689Z][Lightwave-Ledstrip]
- LED silicon choice and dual-channel brightness caps are locked for current HW. ([O27579@2026-02-07T18:52:26.628Z][Lightwave-Ledstrip] [O27577@2026-02-07T18:51:28.049Z][Lightwave-Ledstrip] [O27565@2026-02-07T18:47:54.030Z][Lightwave-Ledstrip])
  - citations: [O27579@2026-02-07T18:52:26.628Z][Lightwave-Ledstrip] [O27577@2026-02-07T18:51:28.049Z][Lightwave-Ledstrip] [O27565@2026-02-07T18:47:54.030Z][Lightwave-Ledstrip]
- Render-path heap allocation is prohibited in favour of static buffers. ([O27641@2026-02-07T19:20:47.595Z][Lightwave-Ledstrip] [O27564@2026-02-07T18:47:51.083Z][Lightwave-Ledstrip] [O27544@2026-02-07T18:42:26.268Z][Lightwave-Ledstrip])
  - citations: [O27641@2026-02-07T19:20:47.595Z][Lightwave-Ledstrip] [O27564@2026-02-07T18:47:51.083Z][Lightwave-Ledstrip] [O27544@2026-02-07T18:42:26.268Z][Lightwave-Ledstrip]
- PSRAM-enabled builds are required to avoid runtime memory failures. ([O27641@2026-02-07T19:20:47.595Z][Lightwave-Ledstrip] [O26767@2026-02-06T16:09:22.349Z][Lightwave-Ledstrip] [O26531@2026-02-06T14:38:25.184Z][Lightwave-Ledstrip])
  - citations: [O27641@2026-02-07T19:20:47.595Z][Lightwave-Ledstrip] [O26767@2026-02-06T16:09:22.349Z][Lightwave-Ledstrip] [O26531@2026-02-06T14:38:25.184Z][Lightwave-Ledstrip]
- System limits are maintained via single-source constants with validation checks. ([O28238@2026-02-08T21:43:02.979Z][Lightwave-Ledstrip] [O28028@2026-02-08T13:22:31.323Z][Lightwave-Ledstrip] [O27933@2026-02-08T11:56:02.004Z][Lightwave-Ledstrip])
  - citations: [O28238@2026-02-08T21:43:02.979Z][Lightwave-Ledstrip] [O28028@2026-02-08T13:22:31.323Z][Lightwave-Ledstrip] [O27933@2026-02-08T11:56:02.004Z][Lightwave-Ledstrip]
- Actor boundaries and explicit state ownership are the primary concurrency model. ([O27133@2026-02-06T17:34:26.610Z][Lightwave-Ledstrip] [O26087@2026-02-05T23:04:35.088Z][Lightwave-Ledstrip] [O24122@2026-01-31T14:46:15.037Z][Lightwave-Ledstrip])
  - citations: [O27133@2026-02-06T17:34:26.610Z][Lightwave-Ledstrip] [O26087@2026-02-05T23:04:35.088Z][Lightwave-Ledstrip] [O24122@2026-01-31T14:46:15.037Z][Lightwave-Ledstrip]

## Contested / superseded appendix
- [CONTESTED] Control plane choices keep AP/STA, IP-first discovery, and portable-mode recoveries deterministic. ([O28376@2026-02-09T15:08:19.742Z][Lightwave-Ledstrip] [O28339@2026-02-09T14:35:25.424Z][Lightwave-Ledstrip] [O28327@2026-02-09T14:31:03.742Z][Lightwave-Ledstrip])
  - citations: [O28376@2026-02-09T15:08:19.742Z][Lightwave-Ledstrip] [O28339@2026-02-09T14:35:25.424Z][Lightwave-Ledstrip] [O28327@2026-02-09T14:31:03.742Z][Lightwave-Ledstrip]
- [CONTESTED] Hardware topology and modularity boundaries separate device, hub, and tooling responsibilities. ([O27569@2026-02-07T18:49:28.799Z][Lightwave-Ledstrip] [O27304@2026-02-07T13:39:00.571Z][Lightwave-Ledstrip] [O27288@2026-02-07T13:34:49.380Z][Lightwave-Ledstrip])
  - citations: [O27569@2026-02-07T18:49:28.799Z][Lightwave-Ledstrip] [O27304@2026-02-07T13:39:00.571Z][Lightwave-Ledstrip] [O27288@2026-02-07T13:34:49.380Z][Lightwave-Ledstrip]
- [CONTESTED] Palette/preset control decisions are enforced through API handlers with validation. ([O28399@2026-02-09T15:33:08.292Z][Lightwave-Ledstrip] [O28398@2026-02-09T15:31:51.123Z][Lightwave-Ledstrip] [O28395@2026-02-09T15:21:59.709Z][Lightwave-Ledstrip])
  - citations: [O28399@2026-02-09T15:33:08.292Z][Lightwave-Ledstrip] [O28398@2026-02-09T15:31:51.123Z][Lightwave-Ledstrip] [O28395@2026-02-09T15:21:59.709Z][Lightwave-Ledstrip]
