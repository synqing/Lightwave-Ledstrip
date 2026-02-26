# K1 Composer Wireframe v2 (Dashboard-as-Brains)

## Visual hierarchy

The live code visualiser is the primary panel. Timeline, transport, and stream telemetry support it.

```text
+--------------------------------------------------------------------------------------------------+
| K1 Lightwave Composer | Device: Connected | Lease: Observe/Exclusive | Stream: Idle/Live/Lost   |
| [Connect] [Take Control] [Release] [Reacquire] [Emergency Stop]                                  |
+--------------------------------------------------------------------------------------------------+
| Timeline + Transport                                      | Lease + Stream Session               |
| [Play] [Pause] [Reset] [0.25x][0.5x][1x][2x]             | owner, remainingMs, heartbeat        |
| ---- scrubber ---- markers ---- loop ----                | contractVersion, queue, counters     |
+--------------------------------------------------------------------------------------------------+
| Live Code Visualiser (PRIMARY)                                                                   |
| effect selector, real firmware source listing, active line highlights, confidence badge, trace  |
+--------------------------------------------------------------------------------------------------+
| LED Preview (dual-strip centre-origin)                    | Algorithm Flow / Active Stage        |
| strip A + strip B from same local framebuffer            | input -> mapping -> modulation ->    |
| being streamed to K1                                       | render -> post -> output             |
+--------------------------------------------------------------------------------------------------+
| Parameter Tuner (lease-gated)                             | Stream Diagnostics + Event Log        |
| speed/intensity/saturation/complexity/variation           | lease + stream + backpressure events |
+--------------------------------------------------------------------------------------------------+
```

## UX state machine

| State | Entry | Posture | Exit |
|---|---|---|---|
| Observe | connected, no owned lease | controls read-only, stream idle | acquire success -> Exclusive, foreign owner -> Locked |
| Exclusive | owned lease + token | controls enabled, heartbeat running, stream may run | release -> Observe, heartbeat fail -> Lease Lost, WS disconnect -> Lease Lost |
| Locked | active lease owned by another client | controls disabled, owner + countdown visible | lease expiry/release -> Observe |
| Lease Lost | previously Exclusive, ownership gone | fallback to Observe, show reason toast | reacquire success -> Exclusive |

## Behaviour model

- Dashboard executes effect logic locally (JS now, WASM parity tier rollout).
- Dashboard streams only framebuffer bytes to K1.
- K1 remains deterministic renderer with stale timeout fallback to internal mode.
- Explainability remains local: code lines, variables, and branch traces are not streamed.
- Visualiser sources come from generated firmware extraction artefacts (`generated/index.json` + per-effect files).
- Diagnostics panel shows queue pressure (`now/p95/max`), send cadence/jitter, and drop ratio (`10s/60s`).
