# K1 Composer Dashboard (Standalone)

Standalone companion dashboard for firmware v2 where the dashboard is the effect brain and K1 is the deterministic renderer head.

## Implemented v1 capabilities

- hard-exclusive lease lifecycle (`control.acquire`, `control.heartbeat`, `control.release`, `control.status`)
- render stream lifecycle (`render.stream.start`, `render.stream.stop`, `render.stream.status`)
- binary frame streaming with `K1F1` header + RGB888 payload (`320 * 3 = 960` bytes)
- browser backpressure control using `WebSocket.bufferedAmount`
- latest-frame-wins sender policy (drop on backlog, never catch-up queueing)
- timeline transport (play/pause/reset + speed scaling)
- live code visualiser (phase highlighting, variable cards, trace log)
- dual-strip preview sourced from the same local frame payload that streams to K1
- parameter tuner that injects `X-Control-Lease` and `X-Control-Lease-Id` for REST mutations
- effect tunable discovery and patching via `/api/v1/effects/parameters`
- telemetry/event trace with token/header redaction

## v1.1 additions

- real effect source catalogue generated from firmware `ieffect` headers/cpps
- phase-range mapping derived from render source (with confidence/warnings)
- effect selector and live source metadata in code visualiser
- browser diagnostics panel for stream jitter/backpressure:
  - queue (`now`, `p95`, `max`)
  - send cadence and jitter
  - drop ratio over `10s` and `60s`

## Run

This scaffold is static HTML/CSS/JS.

```bash
cd tools/k1-composer-dashboard
python3 -m http.server 8080
# open http://localhost:8080
```

Set `Device Host` to your K1 host or IP, then click `Connect`.

## Generate source catalogue

```bash
cd tools/k1-composer-dashboard
python3 ./scripts/generate_effect_catalog.py
python3 ./scripts/validate_effect_catalog.py --strict
```

Run generation whenever firmware effect sources change.

## File map

- `index.html`: dashboard layout and state panels
- `styles.css`: visual system and state styling
- `app.js`: lease + stream runtime, local simulation, binary transport, visualiser logic
- `telemetry.schema.json`: token-safe telemetry schema
- `wasm/`: Emscripten scaffold (optional Tier 1 parity path)
- `src/code-catalog/`: generated effect source mappings and fallback seed
- `scripts/generate_effect_catalog.py`: firmware source extractor
- `scripts/validate_effect_catalog.py`: strict catalogue validator

## Runtime state model

- `Observe`: connected, no owned lease, controls read-only
- `Exclusive`: lease owned, heartbeat active, mutating controls enabled
- `Locked`: lease held by another client, controls disabled, countdown visible
- `Lease Lost`: previously exclusive, now downgraded to observe

## Transport notes

- v1 transport uses binary WebSocket only.
- Queue policy is freshness-first: if `bufferedAmount` is above threshold, frame is dropped.
- Dashboard retains explainability locally; K1 receives only framebuffer data plus standard control/status traffic.

## Diagnostics interpretation

- High `queueP95` indicates browser/network backpressure (risk of visible latency).
- Rising `sendJitterMs` indicates unstable send cadence.
- `dropRatio10s` is the immediate health indicator; `dropRatio60s` shows longer trend.

## Effect tunables API contract

- Discovery: `GET /api/v1/effects/parameters?id=<effectId>`
- Update: `POST` or `PATCH /api/v1/effects/parameters`
- Required body shape for updates:

```json
{
  "effectId": 1201,
  "parameters": {
    "forward_sec": 1.4
  }
}
```

- Tunable metadata fields available per parameter:
  - `name`, `displayName`, `min`, `max`, `default`, `value`
  - `type` (`float|int|bool|enum`), `step`, `group`, `unit`, `advanced`
- Persistence fields available in GET response:
  - `persistence.mode` (`nvs|volatile`)
  - `persistence.dirty` (debounced write pending)
  - `persistence.lastError` (optional)

## Tunables rollout status (API-facing)

As of February 26, 2026:

- Active effects: `170`
- Effects with API tunables: `170`
- Total exposed per-effect parameters: `1162`
- Missing named tunables: `0`
- Coverage gate: `python firmware/v2/tools/effect_tunables/generate_manifest.py --validate --enforce-all-families` passes

Wave A and Wave B are complete, and all remaining active families are now tunables-exposed.

Dashboard source of truth for per-effect keys:

- Runtime discovery: `GET /api/v1/effects/parameters?id=<effectId>`
- Static manifest: `firmware/v2/docs/effects/tunable_manifest.json`
