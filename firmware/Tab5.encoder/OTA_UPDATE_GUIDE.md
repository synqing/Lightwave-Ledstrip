# Tab5.encoder OTA Update Guide

## Overview

Tab5.encoder supports Over-The-Air (OTA) firmware updates via HTTP POST. This allows firmware deployment without physical USB connection to the ESP32-P4 device.

## Quick Reference

### OTA Endpoints

| Endpoint | URL | Response Format |
|----------|-----|-----------------|
| V1 API | `POST /api/v1/firmware/update` | JSON (v1 API standard) |
| Legacy | `POST /update` | Plain text |
| Version | `GET /api/v1/firmware/version` | JSON (v1 API standard) |

**Base URL:** `http://<device-ip>` (Tab5.encoder IP address from WiFi connection)

**Auth Header:** `X-OTA-Token: LW-OTA-2024-SecureUpdate`

**Content Type:** `multipart/form-data` (recommended) or `application/octet-stream`

### Check Current Version

```bash
curl -s http://<device-ip>/api/v1/firmware/version | python3 -m json.tool
```

Response:
```json
{
  "success": true,
  "data": {
    "version": "1.0.0",
    "board": "M5Stack-Tab5-ESP32-P4",
    "sdk": "v5.1.4",
    "sketchSize": 1234567,
    "freeSketch": 2345678,
    "flashSize": 8388608,
    "buildDate": "Jan  3 2026",
    "buildTime": "12:00:00",
    "maxOtaSize": 2345678,
    "otaAvailable": true
  },
  "timestamp": 1234567890
}
```

### Recommended Upload Commands

**V1 API Endpoint (recommended):**
```bash
curl -X POST \
  -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
  -F "update=@.pio/build/tab5/firmware.bin;type=application/octet-stream" \
  "http://<device-ip>/api/v1/firmware/update"
```

**Legacy Endpoint (curl-friendly):**
```bash
curl -X POST \
  -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
  -F "update=@.pio/build/tab5/firmware.bin;type=application/octet-stream" \
  "http://<device-ip>/update"
```

### Build Commands

```bash
# Build firmware from repository root
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -d firmware/Tab5.encoder

# Firmware binary location
firmware/Tab5.encoder/.pio/build/tab5/firmware.bin
```

---

## Detailed OTA Process

### 1. Build the Firmware

```bash
# Clean build recommended before OTA
cd firmware/Tab5.encoder
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -t clean
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5

# Verify build output
ls -la .pio/build/tab5/firmware.bin
md5 .pio/build/tab5/firmware.bin
```

### 2. Verify Device Connectivity

```bash
# Find device IP (check serial monitor or router admin)
# Or use mDNS if available: tab5encoder.local

# Check device is reachable
ping <device-ip>

# Check version endpoint
curl -s http://<device-ip>/api/v1/firmware/version | python3 -m json.tool
```

### 3. Upload Firmware

**Method 1: Multipart Form (Recommended)**
```bash
curl -X POST \
  -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
  -F "update=@.pio/build/tab5/firmware.bin;type=application/octet-stream" \
  "http://<device-ip>/update"
```

**Method 2: Raw Binary**
```bash
curl -X POST \
  -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @.pio/build/tab5/firmware.bin \
  "http://<device-ip>/update"
```

### 4. Expected Behavior

1. Upload progress shows 0-100% (logged to serial, every 10%)
2. At 100%, connection resets with: `curl: (56) Recv failure: Connection reset by peer`
3. Device reboots (takes 5-15 seconds)
4. Device reconnects to WiFi

**The connection reset is NORMAL** - it indicates the device is rebooting with new firmware.

### 5. Verify Update

```bash
# Wait for reboot, then check
sleep 15
curl -s http://<device-ip>/api/v1/firmware/version | python3 -m json.tool
```

Look for:
- `sketchSize` matching your firmware.bin size
- `uptime` showing low value (recent reboot)
- `version` updated if you changed it

---

## Security Considerations

### OTA Token

The default OTA token is `LW-OTA-2024-SecureUpdate`. **Change this in production deployments!**

To change the token:

1. Edit `firmware/Tab5.encoder/src/config/network_config.h`:
```cpp
#ifndef OTA_UPDATE_TOKEN
#define OTA_UPDATE_TOKEN "your-unique-secret-token-here"
#endif
```

2. Or add to `wifi_credentials.ini`:
```ini
build_flags =
    -DOTA_UPDATE_TOKEN=\"your-unique-secret-token-here\"
```

3. Rebuild and flash firmware

### Network Isolation

- OTA server only accepts connections when WiFi is connected
- Consider restricting OTA to local network only (firewall rules)
- Future enhancement: HTTPS support for encrypted updates

---

## Troubleshooting

### OTA Upload Fails Immediately

**Symptoms:** Request returns 401 Unauthorized

**Solutions:**
- Verify `X-OTA-Token` header matches configured token
- Check token is set correctly in `network_config.h`
- Ensure token is exactly as configured (case-sensitive)

### OTA Upload Fails with "Firmware too large"

**Symptoms:** Error message indicates insufficient space

**Solutions:**
- Check `freeSketch` value from version endpoint
- Verify partition table supports OTA (should have OTA partition)
- Reduce firmware size (disable features, optimize code)

### Device Doesn't Reboot After Update

**Symptoms:** Upload completes but device doesn't restart

**Solutions:**
- Check serial monitor for error messages
- Verify `Update.end(true)` succeeded (check serial logs)
- Manually reboot device if needed
- Check if update was actually written (compare sketchSize)

### Connection Reset During Upload

**Symptoms:** Upload fails mid-way with connection reset

**Solutions:**
- Check WiFi signal strength
- Ensure stable network connection
- Try uploading from wired connection
- Check device has sufficient power (USB power may be insufficient)

### HTTP Server Not Responding

**Symptoms:** Cannot reach OTA endpoints

**Solutions:**
- Verify WiFi is connected (check serial monitor)
- Verify HTTP server started (look for "[OTA] HTTP server started" in serial)
- Check device IP address is correct
- Ensure no firewall blocking port 80
- Try ping to verify device is reachable

---

## Implementation Details

### Architecture

- HTTP server runs on port 80 (alongside WebSocket on port 80, different path)
- Uses ESP32 `Update` library for flash writing
- Token authentication via `X-OTA-Token` header
- Progress tracking (logged every 10%)
- Automatic reboot after successful update

### Endpoints

- `GET /api/v1/firmware/version` - Returns firmware version and build info
- `POST /api/v1/firmware/update` - OTA update with JSON response
- `POST /update` - OTA update with plain text response (curl-friendly)

### File Locations

- OTA handler: `firmware/Tab5.encoder/src/network/OtaHandler.*`
- Configuration: `firmware/Tab5.encoder/src/config/network_config.h`
- Server initialization: `firmware/Tab5.encoder/src/main.cpp`

---

## Future Enhancements

- HTTPS support for encrypted OTA updates
- Dual-OTA partition scheme for safe rollback
- Progress display on Tab5 screen during update
- Automatic update check on startup
- Update notification system

