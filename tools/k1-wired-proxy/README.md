# K1 Wired Proxy

Local HTTP + WebSocket proxy for **K1 Lightwave** over USB serial. Enables **wired-first** discovery for the K1 Composer dashboard: when the device is plugged into the computer, the dashboard connects to `localhost:8765` without WiFi.

## Why

- K1 Lightwave is **plugged into the computer** for power and for computer-based software; users do this by default.
- **Wired connection is the primary** means of communication for K1 Composer (see `docs/dashboard/k1-composer-auto-connection-spec.md`).
- The device exposes **USB CDC serial** (115200 baud) and speaks a **JSON line protocol** (see `firmware/v2/src/main.cpp` — `processSerialJsonCommand`). Browsers cannot open serial ports directly (except via Web Serial API with user gesture). This proxy bridges serial ↔ HTTP/WS so the dashboard can talk to the device over localhost.

## Usage

1. **Install dependencies** (from this directory):

   ```bash
   npm install
   ```

2. **Plug in K1 Lightwave** via USB.

3. **Start the proxy**:

   ```bash
   npm start
   # or: node k1-wired-proxy.mjs
   ```

   The proxy will auto-detect the K1 serial port, then listen on `http://127.0.0.1:8765`. Open the K1 Composer dashboard; it will discover and connect to `localhost:8765` first (wired-first discovery).

4. **Optional:** Use a specific serial port or port number:

   ```bash
   node k1-wired-proxy.mjs --serial-path /dev/cu.usbmodem1101 --port 8765
   ```

5. **Probe only** (list ports and test for K1, do not start server):

   ```bash
   npm run probe
   # or: node k1-wired-proxy.mjs --probe-only
   ```

## Protocol

- **Probe:** Open serial at 115200 baud, send `{"type":"device.getStatus"}\n`, wait for a JSON line with `type === "device.getStatus"` or `type === "error"`. If received within ~2.5s, treat as K1.
- **HTTP:**  
  - `GET /api/v1/device/status` → serial `device.getStatus`, return `data` as JSON.  
  - `GET /api/v1/control/status` → serial `control.status`. If the device responds with `type === "error"` and `error` contains `"unknown command"`, the firmware does not implement `control.status` over serial (it is WebSocket-only); the proxy returns a stub `{ "active": false, "remainingMs": 0, "_serialUnsupported": true }` so the dashboard still works.  
  CORS headers allow the dashboard (any origin) to call these.
- **WebSocket:** `ws://127.0.0.1:8765/ws` — bidirectional: WS text messages are sent as JSON lines to serial; serial JSON lines are forwarded to WS. Request/response matching uses `requestId` when present.

## Firmware contract (K1-Lightwave behaviour)

K1-Lightwave is designed to respond over the wired protocol:

- **USB CDC:** Serial is the same as USB CDC when `ARDUINO_USB_CDC_ON_BOOT=1` (`platformio.ini`). The proxy talks to this port.
- **JSON lines:** Lines starting with `{` are handled by `processSerialJsonCommand()` in `main.cpp`; responses are JSON lines with `type`, `requestId`, and `data` or `error`.
- **`device.getStatus`** is implemented over serial and is used for probe and for GET `/api/v1/device/status`.
- **`control.status`** (and `control.acquire` / `control.release` / `control.heartbeat`) are implemented only over WebSocket (`WsControlCommands.cpp`), not in `processSerialJsonCommand()`. Over serial they yield `"unknown command type"`; the proxy returns a stub for GET `/api/v1/control/status` in that case.

See also: `docs/dashboard/k1-composer-auto-connection-spec.md` §3.4 Firmware contract.

## References

- **Firmware serial gateway:** `firmware/v2/src/main.cpp` (line buffer, JSON gateway, `serialJsonResponse` / `serialJsonError`).
- **Init order:** `firmware/v2/docs/INIT_SEQUENCE.md`.
- **Wired-first spec:** `docs/dashboard/k1-composer-auto-connection-spec.md` (§3 Wired-First Bring-Up).
- **PRISM serial bridge:** `docs/operations/SESSION_DEBRIEF_2026-02-12_SERIAL_BRIDGE.md` (normalisation, type map).
- **Trinity bridge pattern:** `tools/trinity-bridge/trinity-bridge.mjs` (host-side proxy).
