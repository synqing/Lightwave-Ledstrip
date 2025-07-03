# LightwaveOS API Endpoints & Protocols

## Overview
This document formally documents all REST/HTTP and WebSocket API endpoints for LightwaveOS, including OTA, settings, and effect control. It is intended for developers and integrators.

---

## HTTP/REST Endpoints

| Endpoint      | Method | Description                  | Auth | Payload/Response         |
|---------------|--------|-----------------------------|------|-------------------------|
| /             | GET    | Web UI (index.html)         | No   | HTML                    |
| /update       | POST   | OTA firmware upload         | Yes* | Binary, text/plain      |
| /ws           | WS     | WebSocket for control/data  | No   | JSON                    |

*Recommend enabling authentication for /update in production.

---

## WebSocket Protocol

### Client → Server Commands

| Command         | Example Payload                        | Description                |
|-----------------|----------------------------------------|----------------------------|
| get_state       | `{}`                                   | Request full system state  |
| set_parameter   | `{ "parameter": "brightness", "value": 200 }` | Set parameter value        |
| set_effect      | `{ "effect": 5 }`                     | Change effect              |
| set_palette     | `{ "palette": 7 }`                    | Change palette             |
| toggle_power    | `{}`                                   | Toggle power               |
| save_preset     | `{ "name": "MyPreset" }`             | Save preset                |

### Server → Client Updates

| Type         | Frequency | Example Fields                |
|--------------|----------|-------------------------------|
| state        | On change| currentEffect, brightness, ...|
| led_data     | 20Hz     | leds (array)                  |
| performance  | 1Hz      | fps, heap, timing             |
| error        | On error | message                       |

---

## Security & Versioning
- **OTA endpoint** should require authentication in production.
- Recommend using HTTPS for all endpoints.
- Version API endpoints (e.g., `/api/v1/...`) for future compatibility.

---

## Example Usage

**OTA Update (curl):**
```sh
curl -F "firmware=@firmware.bin" http://device.local/update
```

**WebSocket Connection (JS):**
```js
const ws = new WebSocket('ws://device.local/ws');
ws.onmessage = (msg) => { /* handle JSON */ };
```

---

For further details, see the main architecture document or contact the maintainers. 