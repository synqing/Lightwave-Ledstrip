# LightwaveOS Hub OTA System

## Directory Structure

```
data/ota/
├── manifest.json          # Release manifest
└── k1/
    └── s3/
        ├── v1.0.0.bin     # K1 Node firmware binaries
        ├── v1.1.0-beta.bin
        └── ...
```

## Uploading to Hub LittleFS

```bash
# Upload filesystem to Hub
cd Tab5.encoder
pio run -e tab5 -t uploadfs
```

## Building Node Firmware Binary

```bash
# Build K1 Node firmware
cd K1.LightwaveS3
pio run -e esp32dev_audio

# Copy binary to Hub OTA directory
cp .pio/build/esp32dev_audio/firmware.bin \
   ../Tab5.encoder/data/ota/k1/s3/v1.0.0.bin
```

## Computing SHA256

```bash
# macOS/Linux
shasum -a 256 data/ota/k1/s3/v1.0.0.bin

# Update manifest.json with the computed hash
```

## Testing OTA

### 1. Start Rolling Update
```bash
# Update node 1 to stable track
curl -X POST "http://192.168.4.1/ota/rollout?track=stable&node=1"

# Update multiple nodes
curl -X POST "http://192.168.4.1/ota/rollout?track=stable&node=1&node=2"
```

### 2. Check Update Status
```bash
curl http://192.168.4.1/ota/state
# Returns: {"state":"in_progress","currentNode":1,"completed":0,"total":1}
```

### 3. Abort (if needed)
```bash
curl -X POST http://192.168.4.1/ota/abort
```

## Node Implementation (Required)

Nodes must implement:

### 1. WebSocket Handler for `ota_update`
```cpp
void NodeWsCtrl::handleOtaUpdate(JsonDocument& doc) {
    const char* version = doc["version"];
    const char* url = doc["url"];
    const char* sha256 = doc["sha256"];
    
    // Start OTA download from Hub
    String fullUrl = "http://192.168.4.1" + String(url);
    // ... perform OTA update ...
}
```

### 2. Send `ota_status` Updates
```cpp
void NodeOta::reportStatus(const char* state, uint8_t pct, const char* error) {
    JsonDocument doc;
    doc["t"] = "ota_status";
    doc["nodeId"] = nodeId_;
    doc["state"] = state;  // "downloading", "installing", "complete", "error"
    doc["pct"] = pct;
    doc["error"] = error;
    
    // Send via WebSocket to Hub
}
```

### States:
- `downloading` - Fetching binary from Hub
- `installing` - Writing to flash, setting boot partition
- `complete` - Update successful, rebooting
- `error` - Update failed (send error message)

## Manifest Format

```json
{
  "platforms": {
    "k1": {
      "releases": {
        "stable": {
          "version": "1.0.0",
          "url": "/ota/k1/s3/v1.0.0.bin",
          "sha256": "...",
          "size": 1048576,
          "notes": "Release notes"
        }
      }
    }
  }
}
```

## HTTP Endpoints

- `GET /ota/manifest.json` - Get release manifest
- `GET /ota/k1/s3/<version>.bin` - Download firmware binary
- `POST /ota/rollout?track=<track>&node=<id>...` - Start rolling update
- `POST /ota/abort` - Abort ongoing rollout
- `GET /ota/state` - Get dispatcher status
- `GET /nodes` - Check node OTA state (ota_state, ota_pct, ota_version, ota_error)

## WebSocket Protocol

### Hub → Node: `ota_update`
```json
{
  "t": "ota_update",
  "version": "1.0.0",
  "url": "/ota/k1/s3/v1.0.0.bin",
  "sha256": "..."
}
```

### Node → Hub: `ota_status`
```json
{
  "t": "ota_status",
  "nodeId": 1,
  "state": "downloading",
  "pct": 45,
  "error": ""
}
```

## Safety Features

- **Rolling updates** - Only one node updates at a time
- **Timeout protection** - 180s timeout per node (configurable via `LW_OTA_NODE_TIMEOUT_S`)
- **SHA256 verification** - Nodes should verify binary integrity
- **Path traversal protection** - Hub validates binary paths
- **State tracking** - Hub tracks OTA progress per node
- **Abort capability** - Can abort mid-rollout
