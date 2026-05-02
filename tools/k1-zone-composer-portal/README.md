---
abstract: "K1 Zone Composer diagnostic portal — single-file HTML/CSS/JS surface that talks to K1 over WebSocket + REST + WebSerial with auto-reconnect, heartbeat, requestId-tracked command queue, and idempotent retry on transport failover. Surfaces the full Zone Composer command matrix (sections 1-3, 6, 7 live; sections 4, 5, 8-11 placeholder for future phases). Designed for diagnostic depth, not user-facing product UI."
---

# K1 Zone Composer Portal

A diagnostic portal for interacting with the K1 Zone Composer. Single-file HTML, no build step, no external dependencies.

## Run

```bash
# Option A: open file directly
open tools/k1-zone-composer-portal/index.html

# Option B: serve via local http server (avoids file:// CORS edge cases on some browsers)
cd tools/k1-zone-composer-portal && python3 -m http.server 8080
# then open http://localhost:8080
```

Connect to a K1 by typing the device host (default `192.168.4.1` for the K1's AP) and clicking **Connect**. The portal tries WS and REST in parallel and promotes whichever opens first to primary.

For USB CDC (no WiFi required), select **USB Serial** in the Raw Command transport dropdown — the browser will prompt for serial-port permission.

## Architecture

### Transports

| Transport | Purpose | Endpoint | Protocol |
|-----------|---------|----------|----------|
| **WebSocket** (primary) | real-time bidi, broadcast subscription | `ws://host/ws` | JSON-over-WS, `requestId`-tracked |
| **REST** (fallback) | state mutations, capability discovery, polling | `http://host/api/v1/...` | JSON-over-HTTPS, idempotent |
| **WebSerial** (last resort) | direct USB CDC SerialJSON, no WiFi | USB CDC, 115200 baud | newline-delimited JSON |

Failover priority: **WS → REST → WebSerial**. Any healthy transport becomes primary; if the primary errors, the manager auto-promotes the next healthy one. Commands carry a `requestId` so the firmware can deduplicate retries that land on multiple transports.

### State model

- **Store** — server-of-truth: `zones[]`, composer enabled, active preset, device stats, effects/palettes/presets catalogues, last broadcast. Sync'd from K1 via REST `GET /zones` or WS `zones.get`, plus broadcast subscription.
- **editor** — local UI state for the layout editor: `b1`, `b2` boundary positions during drag. When the user drags, only `editor` mutates (visual feedback). When they hit **Apply Layout**, the portal derives segments from `editor` and sends `zones.setLayout`. **Revert Pending** snaps `editor` back to derived-from-Store.

### Liveness + reconnect

- WS heartbeat: ping every 5 s with 3 s timeout → mark transport ERROR if no pong → trigger reconnect.
- REST liveness: poll `GET /firmware/version` every 30 s — error count > 0 means transport unhealthy.
- WebSerial: receive loop runs continuously; errors propagate to state.
- Reconnect: exponential backoff 1 s → 32 s, capped. Reset to 1 s on successful reconnect.

### Command queue

Every WS-bound command gets a `requestId` (UUID). The queue tracks pending commands by ID; responses match on `requestId` and resolve the matching promise. Timeout: 5 s per command. On transport failure, the manager retries on the next healthy transport with the same `requestId` (firmware idempotent-handles).

REST commands are unary (request → response) and resolve immediately; no queue tracking needed.

### Wire format

Post-B2 1-indexed. All wire `zoneId` ∈ {1, 2, 3}. Wire `zoneId = 0` is RESERVED — K1 rejects with `INVALID_VALUE`. Internal portal `editor.b1`/`editor.b2` are distance-from-centre integers (0..80); they're translated to per-zone segment quadruples (`s1LeftStart/End`, `s1RightStart/End`) at the boundary when sending `zones.setLayout`.

### Capability gating (future)

The Forward-Compatible UI accordions (D-3 params, D-4 audio, snapshots, performance, Z-order, render diagnostics) are placeholder-only. Future capability discovery will:
- Fetch `GET /api/v1/openapi.json` on connect
- Parse path inventory + 501 status
- Auto-flip accordion gate-badges from "stub" / "gated" to "live"
- Enable the placeholder controls

## Live coverage (v0.1)

From `docs/protocol/zones-command-matrix.md`:

| Section | Status | Wired |
|---------|--------|-------|
| §1 Composer enable/disable | ✅ live | `zone.enable` / `POST /zones/enabled` |
| §2 Layout get/set | ✅ live | `zones.setLayout` / `POST /zones/layout`, `GET /zones` |
| §3 Per-zone setters (effect, brightness, speed, palette, blend, enabled) | ✅ live | `zone.setX` / `POST /zones/{id}/X` |
| §4 Per-zone parameters (D-3) | 📋 placeholder | gated on D-1 hardware validation |
| §5 Audio routing (D-4) | 📋 placeholder | currently 🚫 501 stub |
| §6 Factory presets | ✅ live | `zone.loadPreset` / `POST /zone-presets/apply?id=N`, `GET /zone-presets` |
| §7 Custom presets | ⚠ partial | apply ✅, save/delete pending |
| §8 Snapshots | 📋 placeholder | currently 🚫 501 stubs |
| §9 Live performance (solo, A/B, beat trigger) | 📋 placeholder | currently 🚫 501 / 📋 planned |
| §10 Render diagnostics | 📋 placeholder | currently 🚫 501 stubs |
| §11 Z-order / layering | 📋 placeholder | currently 🚫 501 stub |

## v0.2 follow-ups

- **Capability-driven feature gating** — fetch and parse `/openapi.json`, auto-enable accordions when their endpoints leave 501.
- **Effect/palette catalogue auto-populate** — REST hydration calls `/effects` and `/palettes`; need to thread the response into the per-zone `<select>` options dynamically (currently the dropdowns retain their hardcoded mockup options).
- **Lease lifecycle** — implement `control.acquire` / `control.heartbeat` / `control.release` for safer concurrent access (k1-composer pattern). Currently the portal sends commands without explicit lease.
- **Custom preset save/delete** — wire the existing buttons to `POST /zone-presets` / `DELETE /zone-presets/delete?id=N`.
- **Connection-state persistence** — remember last-used host in localStorage.
- **WebSerial reconnect** — currently WebSerial doesn't auto-reconnect on USB drop; user has to re-pick the port.

## Files

- `index.html` — portal (HTML + CSS + JS, single file, ~3,300 lines / ~130 KB)
- `README.md` — this document

## Wire-protocol gotchas

- **B5 SerialJSON parity gaps:** `zones.setLayout` and per-zone `zone.enableZone` are NOT exposed on SerialJSON. The portal's `WebSerialTransport.supports(commandType)` returns false for those, so the manager auto-falls-back to WS or REST. Surfaced in connection-diagnostics card as "parity gaps: 2 known".
- **CORS:** firmware sends `Access-Control-Allow-Origin: *` on `/api/v1/...` so the portal works from `file://` and `http://localhost:8080`.
- **WebSerial requires HTTPS or localhost** in production (per browser policy). `file://` works for local development.

## Browser compatibility

- WebSocket + fetch: all modern browsers
- WebSerial: Chrome / Edge only (Firefox / Safari don't support it as of 2026-05); the WebSerial transport gracefully reports `WebSerial unsupported` and the portal still works on WS+REST

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-02 | Claude (claude-opus-4-7) | Created. Mockup v0 — interactive layout editor with stub data only. |
| 2026-05-03 | Claude (claude-opus-4-7) | v0.1 — full connection layer landed. WS+REST+WebSerial with auto-reconnect, heartbeat, command queue, idempotent retry, live state subscription. Tab5-aligned strip colours. Forward-compatible accordions for sections 4, 5, 8-11 remain placeholder. |
