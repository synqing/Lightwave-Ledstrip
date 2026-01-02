# Lightwave Simple

A minimal web dashboard for controlling LightwaveOS devices. Provides a standalone web interface that can run on any computer and communicate with the ESP32 over WiFi.

## Files

| File | Purpose |
|------|---------|
| `index.html` | Main dashboard UI |
| `app.js` | JavaScript controller logic |
| `styles.css` | Dashboard styling |
| `server.py` | Development server with mDNS discovery |
| `start-server.sh` | Quick-start script |
| `audio-viz-mockup.html` | Audio visualization prototype |

## Quick Start

### Option 1: Python Server (Recommended)

```bash
cd lightwave-simple
python3 server.py
```

Access at:
- Local: `http://localhost:8888`
- Mobile: `http://<your-ip>:8888`

### Option 2: Shell Script

```bash
./lightwave-simple/start-server.sh
```

### Option 3: Any HTTP Server

```bash
cd lightwave-simple
python3 -m http.server 8888
# or
npx serve -p 8888
```

## Features

- **Device Discovery:** Automatic mDNS resolution via `/api/discover` endpoint
- **Effect Control:** Select and configure visual effects
- **Parameter Adjustment:** Brightness, speed, intensity controls
- **Zone Management:** Multi-zone configuration
- **Real-time Updates:** WebSocket connection for live state sync

## Device Discovery

The `server.py` provides an `/api/discover` endpoint that resolves `lightwaveos.local` via mDNS:

```bash
curl http://localhost:8888/api/discover
```

Response:
```json
{
  "success": true,
  "ip": "192.168.1.42",
  "hostname": "lightwaveos.local"
}
```

## Development

### Connecting to Device

The dashboard connects to the LightwaveOS device at `http://lightwaveos.local` or the IP returned by `/api/discover`.

Ensure:
1. ESP32 is powered on and connected to WiFi
2. Both computer and ESP32 are on the same network
3. mDNS is working (try `ping lightwaveos.local`)

### API Endpoints

The dashboard communicates with the device using:

- **REST API:** `/api/v1/*` endpoints for device control
- **WebSocket:** `ws://lightwaveos.local/ws` for real-time updates

See [../docs/api/API_V1.md](../docs/api/API_V1.md) for complete API documentation.

### Modifying the UI

1. Edit `index.html`, `app.js`, or `styles.css`
2. Refresh browser to see changes
3. No build step required

## Troubleshooting

### Device Not Found

```bash
# Check if device is reachable
ping lightwaveos.local

# If mDNS fails, use IP directly
# Find device IP from router or serial monitor
```

### CORS Errors

The `server.py` adds CORS headers automatically. If using a different server, ensure CORS is configured or use a browser extension.

### WebSocket Connection Failed

1. Verify device is running (check serial monitor)
2. Confirm correct IP/hostname
3. Check browser console for detailed errors

## Related Documentation

- [../CLAUDE.md](../CLAUDE.md) - Main project documentation
- [../docs/api/API_V1.md](../docs/api/API_V1.md) - Complete API reference
- [../firmware/v2/data/](../firmware/v2/data/) - Device-hosted web interface
