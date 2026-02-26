# K1 Composer + Firmware v2 Implementation Path (Dashboard-as-Brains)

## Phase 1: Contract lock

1. Finalise lease contract (`control.*`) and render stream contract (`render.stream.*`).
2. Lock binary frame header v1 (`K1F1`, 16-byte header, 960-byte payload).
3. Expose capability discovery fields for render stream support.

## Phase 2: Firmware stream transport core

1. Add JSON command handlers: `render.stream.start|stop|status`.
2. Add fragmentation-safe binary WS ingest.
3. Validate contract fields and count invalid/blocked frames.

## Phase 3: Renderer external mode

1. Add `ExternalStream` render source mode to renderer.
2. Add 2-slot latest-frame mailbox and overwrite counters.
3. Add stale timeout fallback to internal renderer.

## Phase 4: Lease-coupled enforcement

1. Reuse the same lease authority for JSON mutations and binary frames.
2. Stop stream on lease release/expiry/owner disconnect.
3. Keep observe/read-only paths always available.

## Phase 5: Status and telemetry surfaces

1. Extend `device.status` and REST status payloads with stream state/counters.
2. Broadcast `render.stream.stateChanged` and `control.stateChanged`.
3. Preserve token secrecy across logs, telemetry, and broadcasts.

## Phase 6: Dashboard transport runtime

1. Implement lease lifecycle and heartbeat client.
2. Implement stream lifecycle with negotiated contract handling.
3. Implement sender pacing via `bufferedAmount` + drop-old policy.

## Phase 7: Live code visualiser stack

1. Generate effect source catalogue from firmware `ieffect/*.h` + `ieffect/*.cpp`.
2. Render source view with line highlighting driven by generated phase ranges.
2. Show execution variables and trace log per tick/scrub.
3. Bind timeline transport to local effect execution and streamed framebuffer.

## Phase 8: Browser diagnostics

1. Sample `bufferedAmount`, send cadence, and drop attempts in the transport loop.
2. Compute rolling windows (`10s`, `60s`) for drop ratio and jitter.
3. Render diagnostics panel and emit throttled diagnostics telemetry events.

## Phase 9: WASM parity rollout

1. Add Emscripten build path and JS loader.
2. Tier 1: curated effect set with deterministic frame parity checks.
3. Tier 2/3: progressively expand effect coverage without protocol change.

## Phase 10: Hardening and rollout

1. Burn-in tests with mixed clients (many observers, one owner).
2. Validate stale recovery and disconnect behaviour under network impairment.
3. Promote feature defaults once latency and lock correctness metrics are stable.
