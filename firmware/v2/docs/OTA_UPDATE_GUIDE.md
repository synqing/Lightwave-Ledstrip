# LightwaveOS OTA Update Guide

## Overview

LightwaveOS supports Over-The-Air (OTA) firmware updates via HTTP POST to the `/update` endpoint. This allows firmware deployment without physical USB connection to the ESP32-S3 device.

## Quick Reference

### OTA Endpoint

| Property | Value |
|----------|-------|
| URL | `http://lightwaveos.local/update` or `http://<device-ip>/update` |
| Method | `POST` |
| Auth Header | `X-OTA-Token: LW-OTA-2024-SecureUpdate` |
| Content Type | `multipart/form-data` or `application/octet-stream` |

### Recommended Upload Command

```bash
curl -X POST \
  -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
  -F "update=@.pio/build/esp32dev_audio/firmware.bin;type=application/octet-stream" \
  "http://lightwaveos.local/update"
```

### Build Commands

```bash
# Build firmware with audio features (required for audio streaming)
pio run -e esp32dev_audio

# Firmware binary location
.pio/build/esp32dev_audio/firmware.bin
```

---

## Detailed OTA Process

### 1. Build the Firmware

```bash
# Clean build recommended before OTA
pio run -e esp32dev_audio -t clean
pio run -e esp32dev_audio

# Verify build output
ls -la .pio/build/esp32dev_audio/firmware.bin
md5 .pio/build/esp32dev_audio/firmware.bin
```

### 2. Verify Device Connectivity

```bash
# Check device is reachable
ping lightwaveos.local

# Or use IP directly (more reliable)
curl -s http://192.168.x.x/api/v1/device/status | python3 -m json.tool
```

### 3. Upload Firmware

**Method 1: Multipart Form (Recommended)**
```bash
curl -X POST \
  -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
  -F "update=@.pio/build/esp32dev_audio/firmware.bin;type=application/octet-stream" \
  "http://192.168.x.x/update"
```

**Method 2: Raw Binary**
```bash
curl -X POST \
  -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @.pio/build/esp32dev_audio/firmware.bin \
  "http://192.168.x.x/update"
```

### 4. Expected Behavior

1. Upload progress shows 0-100%
2. At 100%, connection resets with: `curl: (56) Recv failure: Connection reset by peer`
3. Device reboots (takes 5-15 seconds)
4. Device reconnects to WiFi

**The connection reset is NORMAL** - it indicates the device is rebooting with new firmware.

### 5. Verify Update

```bash
# Wait for reboot, then check
sleep 15
curl -s http://192.168.x.x/api/v1/device/info | python3 -m json.tool
```

Look for:
- `sketchSize` matching your firmware.bin size
- `uptime` showing low value (recent reboot)

---

## Build Environments

| Environment | WiFi | Audio | Audio Broadcast | Use Case |
|-------------|------|-------|-----------------|----------|
| `esp32dev` | No | No | No | Standalone LED effects |
| `esp32dev_audio` | Yes | No | No | Web control, no audio |
| `esp32dev_audio` | Yes | Yes | Yes | Full audio-reactive features |

**Important**: Audio stream broadcasting requires `esp32dev_audio`. The audio pipeline (I2S capture, Goertzel analysis, chroma extraction) is only compiled with `FEATURE_AUDIO_SYNC=1`.

---

## Troubleshooting

### OTA Upload Fails Immediately

**Symptom**: 401 Unauthorized or immediate rejection

**Cause**: Missing or incorrect OTA token

**Fix**: Ensure header is exactly:
```
X-OTA-Token: LW-OTA-2024-SecureUpdate
```

### Upload Completes But Nothing Changes

**Symptom**: 100% upload, device reboots, but behavior unchanged

**Possible Causes**:

1. **Same firmware uploaded** - Check MD5 hash differs from previous
   ```bash
   md5 .pio/build/esp32dev_audio/firmware.bin
   ```

2. **Build not updated** - Force clean rebuild
   ```bash
   pio run -e esp32dev_audio -t clean
   pio run -e esp32dev_audio
   ```

3. **Code bug masking changes** - See "WebSocket Silent Failure Bug" below

### Device Not Responding After OTA

**Symptom**: Device unreachable after upload

**Causes & Fixes**:

1. **Still rebooting** - Wait 15-30 seconds
2. **WiFi reconnection** - Check router for device IP
3. **Firmware crash** - Requires USB serial connection to diagnose
4. **mDNS not resolved** - Use IP address directly instead of `lightwaveos.local`

### Slow Upload Speed

**Symptom**: Upload takes 30-60+ seconds

**Cause**: WiFi congestion or weak signal

**Mitigation**:
- Use IP address instead of mDNS hostname
- Move device closer to router during update
- Reduce network traffic during upload

---

## Known Issues & Fixes

### WebSocket Silent Failure Bug (Fixed Dec 2024)

**Symptom**: WebSocket commands like `ledStream.subscribe` or `audioStream.subscribe` silently fail. Status broadcasts still work.

**Root Cause**: The global `webServerInstance` pointer was declared but never assigned:

```cpp
// WebServer.cpp
WebServer* webServerInstance = nullptr;  // Never set!

// In event handler - always fails silently
if (webServerInstance) webServerInstance->handleWsMessage(...);
```

**Fix Applied**: Added initialization in `WebServer::begin()`:

```cpp
bool WebServer::begin() {
    webServerInstance = this;  // Critical fix
    // ...
}
```

**Why Status Broadcasts Still Worked**: The `update()` loop calls `broadcastStatus()` directly via `m_ws->textAll()`, bypassing the `webServerInstance` pointer.

**Lesson**: Silent null-pointer guards can mask bugs for extended periods. Always log when guards trigger.

---

## OTA Security

### Current Implementation

- Token-based authentication via `X-OTA-Token` header
- Token configured in `src/config/network_config.h`
- No encryption (HTTP, not HTTPS)

### Recommendations for Production

1. Use unique token per device
2. Implement HTTPS if network is untrusted
3. Add firmware signature verification
4. Rate limit OTA attempts
5. Log OTA attempts with client IP

---

## Firmware Verification Checklist

After OTA update, verify these endpoints respond correctly:

```bash
# Device info (check sketchSize, uptime)
curl -s http://192.168.x.x/api/v1/device/info

# Device status (check fps, freeHeap)
curl -s http://192.168.x.x/api/v1/device/status

# WebSocket test (Python)
python3 -c "
import asyncio, websockets, json
async def test():
    async with websockets.connect('ws://192.168.x.x/ws') as ws:
        await ws.send(json.dumps({'type': 'ledStream.subscribe'}))
        msg = await asyncio.wait_for(ws.recv(), timeout=2)
        print('Response:', msg)
asyncio.run(test())
"
```

---

## Audio Stream Verification

After deploying `esp32dev_audio` build:

```python
import asyncio
import websockets
import json
import struct

async def test_audio():
    async with websockets.connect('ws://192.168.x.x/ws') as ws:
        await ws.send(json.dumps({'type': 'audioStream.subscribe'}))

        for _ in range(10):
            msg = await asyncio.wait_for(ws.recv(), timeout=1)
            if isinstance(msg, bytes) and len(msg) >= 16:
                magic = struct.unpack('<I', msg[0:4])[0]
                if magic == 0x00445541:  # "AUD\0"
                    rms = struct.unpack('<f', msg[12:16])[0]
                    print(f'Audio frame: rms={rms:.4f}')
            elif isinstance(msg, str):
                data = json.loads(msg)
                if 'audioStream' in data.get('type', ''):
                    print(f'Subscription: {data}')

asyncio.run(test_audio())
```

Expected output:
```
Subscription: {'type': 'audioStream.subscribed', 'success': True, 'data': {'frameSize': 448, ...}}
Audio frame: rms=0.8234
Audio frame: rms=0.7891
...
```

---

## Quick Troubleshooting Flowchart

```
OTA Upload Started
        │
        ▼
   100% Complete? ──No──► Check network, retry
        │
       Yes
        │
        ▼
Connection Reset? ──No──► Check token, endpoint
        │
       Yes (Normal!)
        │
        ▼
   Wait 15 seconds
        │
        ▼
Device Responds? ──No──► Check WiFi, use IP not mDNS
        │
       Yes
        │
        ▼
 sketchSize Changed? ──No──► Verify build updated, check MD5
        │
       Yes
        │
        ▼
Features Working? ──No──► Check for code bugs (see Known Issues)
        │
       Yes
        │
        ▼
    OTA Success!
```

---

## References

- [ESP32 OTA Updates Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [ESPAsyncWebServer Library](https://github.com/me-no-dev/ESPAsyncWebServer)
- LightwaveOS API Documentation: `docs/api/API_V1.md`
