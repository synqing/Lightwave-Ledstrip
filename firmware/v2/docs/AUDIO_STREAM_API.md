# Audio Stream API Documentation

## Overview

LightwaveOS provides real-time audio metrics streaming over WebSocket for building audio-reactive visualizations in web applications. The ESP32 captures audio via I2S, performs spectral analysis, and broadcasts compressed binary frames at 30 FPS.

## Requirements

- Firmware built with `esp32dev_audio_esv11` environment
- `FEATURE_AUDIO_SYNC=1` compile flag (set automatically by esp32dev_audio_esv11)
- WebSocket connection to `ws://lightwaveos.local/ws`

---

## WebSocket Commands

### Subscribe to Audio Stream

```json
{
  "type": "audioStream.subscribe",
  "requestId": "optional-correlation-id"
}
```

**Response:**
```json
{
  "type": "audioStream.subscribed",
  "requestId": "optional-correlation-id",
  "success": true,
  "data": {
    "clientId": 3,
    "frameSize": 448,
    "version": 1,
    "numBands": 8,
    "numChroma": 12,
    "waveformSize": 128,
    "targetFps": 30
  }
}
```

### Unsubscribe from Audio Stream

```json
{
  "type": "audioStream.unsubscribe",
  "requestId": "optional-correlation-id"
}
```

**Response:**
```json
{
  "type": "audioStream.unsubscribed",
  "requestId": "optional-correlation-id",
  "success": true,
  "data": {
    "clientId": 3
  }
}
```

---

## Binary Frame Format

After subscribing, the client receives binary WebSocket messages at approximately 30 FPS. Each frame is exactly **448 bytes**.

### Frame Structure

| Offset | Size (bytes) | Type | Field | Description |
|--------|--------------|------|-------|-------------|
| 0 | 4 | uint32_t | magic | `0x00445541` ("AUD\0" little-endian) |
| 4 | 4 | uint32_t | hop_seq | Audio hop sequence number |
| 8 | 4 | uint32_t | timestamp | Milliseconds since boot |
| 12 | 4 | float | rms | Root-mean-square level (0.0-1.0) |
| 16 | 4 | float | flux | Spectral flux/novelty (0.0-1.0) |
| 20 | 4 | float | fast_rms | Fast-smoothed RMS |
| 24 | 4 | float | fast_flux | Fast-smoothed flux |
| 28 | 32 | float[8] | bands | Frequency band energies (0.0-1.0) |
| 60 | 32 | float[8] | heavy_bands | Slow-smoothed band energies |
| 92 | 48 | float[12] | chroma | Pitch class energies C-B (0.0-1.0) |
| 140 | 48 | float[12] | heavy_chroma | Slow-smoothed chroma |
| 188 | 4 | uint32_t | reserved | Alignment padding |
| 192 | 256 | int16_t[128] | waveform | Time-domain audio samples |

**Total: 448 bytes**

### Frequency Bands

The 8 frequency bands cover the audible spectrum:

| Index | Approximate Range | Musical Context |
|-------|-------------------|-----------------|
| 0 | 60-120 Hz | Sub-bass, kick drums |
| 1 | 120-250 Hz | Bass, bass guitar |
| 2 | 250-500 Hz | Low-mids, warmth |
| 3 | 500-1000 Hz | Mids, vocals |
| 4 | 1000-2000 Hz | Upper-mids, presence |
| 5 | 2000-4000 Hz | High-mids, clarity |
| 6 | 4000-7800 Hz | Highs, cymbals |
| 7 | 7800+ Hz | Air, brilliance |

### Chroma (Pitch Classes)

The 12 chroma values represent pitch class energy:

| Index | Note |
|-------|------|
| 0 | C |
| 1 | C# |
| 2 | D |
| 3 | D# |
| 4 | E |
| 5 | F |
| 6 | F# |
| 7 | G |
| 8 | G# |
| 9 | A |
| 10 | A# |
| 11 | B |

---

## JavaScript Client Example

### Basic Subscription

```javascript
const ws = new WebSocket('ws://lightwaveos.local/ws');

ws.onopen = () => {
  ws.send(JSON.stringify({ type: 'audioStream.subscribe' }));
};

ws.onmessage = (event) => {
  if (event.data instanceof Blob) {
    event.data.arrayBuffer().then(buffer => {
      const view = new DataView(buffer);
      const magic = view.getUint32(0, true);

      if (magic === 0x00445541) {  // "AUD\0"
        const audio = parseAudioFrame(view);
        updateVisualization(audio);
      }
    });
  } else {
    const msg = JSON.parse(event.data);
    console.log('JSON message:', msg);
  }
};
```

### Full Frame Parser

```javascript
function parseAudioFrame(view) {
  return {
    magic: view.getUint32(0, true),
    hopSeq: view.getUint32(4, true),
    timestamp: view.getUint32(8, true),
    rms: view.getFloat32(12, true),
    flux: view.getFloat32(16, true),
    fastRms: view.getFloat32(20, true),
    fastFlux: view.getFloat32(24, true),
    bands: Array.from({ length: 8 }, (_, i) =>
      view.getFloat32(28 + i * 4, true)
    ),
    heavyBands: Array.from({ length: 8 }, (_, i) =>
      view.getFloat32(60 + i * 4, true)
    ),
    chroma: Array.from({ length: 12 }, (_, i) =>
      view.getFloat32(92 + i * 4, true)
    ),
    heavyChroma: Array.from({ length: 12 }, (_, i) =>
      view.getFloat32(140 + i * 4, true)
    ),
    waveform: Array.from({ length: 128 }, (_, i) =>
      view.getInt16(192 + i * 2, true)
    )
  };
}
```

### React Hook Example

```javascript
import { useState, useEffect, useRef } from 'react';

function useAudioStream(wsUrl) {
  const [audio, setAudio] = useState(null);
  const [connected, setConnected] = useState(false);
  const wsRef = useRef(null);

  useEffect(() => {
    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      setConnected(true);
      ws.send(JSON.stringify({ type: 'audioStream.subscribe' }));
    };

    ws.onclose = () => setConnected(false);

    ws.onmessage = (event) => {
      if (event.data instanceof Blob) {
        event.data.arrayBuffer().then(buffer => {
          const view = new DataView(buffer);
          if (view.getUint32(0, true) === 0x00445541) {
            setAudio(parseAudioFrame(view));
          }
        });
      }
    };

    return () => {
      ws.send(JSON.stringify({ type: 'audioStream.unsubscribe' }));
      ws.close();
    };
  }, [wsUrl]);

  return { audio, connected };
}

// Usage
function Visualizer() {
  const { audio, connected } = useAudioStream('ws://lightwaveos.local/ws');

  if (!connected) return <div>Connecting...</div>;
  if (!audio) return <div>Waiting for audio...</div>;

  return (
    <div>
      <p>RMS: {audio.rms.toFixed(3)}</p>
      <p>Flux: {audio.flux.toFixed(3)}</p>
      {/* Render visualization */}
    </div>
  );
}
```

---

## Python Client Example

```python
import asyncio
import websockets
import json
import struct

async def audio_stream_client():
    uri = 'ws://lightwaveos.local/ws'

    async with websockets.connect(uri) as ws:
        # Subscribe
        await ws.send(json.dumps({'type': 'audioStream.subscribe'}))

        while True:
            msg = await ws.recv()

            if isinstance(msg, bytes) and len(msg) >= 448:
                frame = parse_audio_frame(msg)
                print(f"RMS: {frame['rms']:.3f}, Flux: {frame['flux']:.3f}")
            elif isinstance(msg, str):
                data = json.loads(msg)
                print(f"JSON: {data.get('type')}")

def parse_audio_frame(data):
    return {
        'magic': struct.unpack('<I', data[0:4])[0],
        'hop_seq': struct.unpack('<I', data[4:8])[0],
        'timestamp': struct.unpack('<I', data[8:12])[0],
        'rms': struct.unpack('<f', data[12:16])[0],
        'flux': struct.unpack('<f', data[16:20])[0],
        'fast_rms': struct.unpack('<f', data[20:24])[0],
        'fast_flux': struct.unpack('<f', data[24:28])[0],
        'bands': list(struct.unpack('<8f', data[28:60])),
        'heavy_bands': list(struct.unpack('<8f', data[60:92])),
        'chroma': list(struct.unpack('<12f', data[92:140])),
        'heavy_chroma': list(struct.unpack('<12f', data[140:188])),
        'waveform': list(struct.unpack('<128h', data[192:448])),
    }

asyncio.run(audio_stream_client())
```

---

## Smoothing Strategies

The firmware provides two smoothing levels for bands and chroma:

| Field | Smoothing | Use Case |
|-------|-----------|----------|
| `bands` | Fast (~50ms) | Reactive animations, beat detection |
| `heavy_bands` | Slow (~500ms) | Ambient effects, gradual transitions |
| `chroma` | Fast (~50ms) | Note detection, harmonic analysis |
| `heavy_chroma` | Slow (~500ms) | Key detection, mood analysis |

### Recommended Usage

```javascript
// For beat-reactive effects
const beatIntensity = audio.bands[0] + audio.bands[1];  // Bass energy

// For smooth ambient effects
const ambientWarmth = audio.heavyBands.slice(1, 4).reduce((a, b) => a + b) / 3;

// For harmonic visualization
const dominantNote = audio.chroma.indexOf(Math.max(...audio.chroma));
```

---

## Performance Considerations

### Bandwidth

- Frame size: 448 bytes
- Target FPS: 30
- Bandwidth per client: ~13.4 KB/s

### Client Limits

- Maximum concurrent audio stream subscribers: 4
- Subscription table uses fixed memory allocation

### Frame Dropping

If the WebSocket buffer fills up (slow client), frames may be dropped. The `hop_seq` field can be used to detect gaps:

```javascript
let lastHop = 0;
function checkFrameDrops(frame) {
  if (lastHop > 0 && frame.hopSeq !== lastHop + 2) {
    console.warn(`Dropped ${(frame.hopSeq - lastHop) / 2 - 1} frames`);
  }
  lastHop = frame.hopSeq;
}
```

---

## Firmware Implementation

### Source Files

| File | Purpose |
|------|---------|
| `src/network/webserver/AudioStreamConfig.h` | Configuration constants |
| `src/network/webserver/AudioFrameEncoder.h` | Binary frame encoding |
| `src/network/webserver/AudioStreamBroadcaster.h` | Subscription management |
| `src/network/webserver/AudioStreamBroadcaster.cpp` | Broadcasting implementation |

### Configuration Constants

```cpp
namespace AudioStreamConfig {
    constexpr uint8_t STREAM_VERSION = 1;
    constexpr uint32_t MAGIC = 0x00445541;  // "AUD\0"
    constexpr size_t FRAME_SIZE = 448;
    constexpr uint8_t TARGET_FPS = 30;
    constexpr uint32_t FRAME_INTERVAL_MS = 33;  // ~30 FPS
    constexpr uint8_t MAX_CLIENTS = 4;
}
```

---

## Comparison with LED Stream

| Feature | LED Stream | Audio Stream |
|---------|------------|--------------|
| Command | `ledStream.subscribe` | `audioStream.subscribe` |
| Frame Size | 966 bytes | 448 bytes |
| Target FPS | 20 | 30 |
| Magic Byte | `0xFE` | `0x00445541` |
| Data | RGB values for 320 LEDs | Audio metrics |
| Bandwidth | ~19 KB/s | ~13.4 KB/s |

Both streams can be subscribed simultaneously.

---

## Troubleshooting

### No Audio Frames Received

1. **Check firmware build**: Must use `esp32dev_audio_esv11` environment
2. **Verify subscription response**: Should receive `audioStream.subscribed`
3. **Check WebSocket connection**: Ensure binary message handling is implemented

### All Values Are Zero

- Audio input may not be connected
- I2S microphone may need configuration
- Check audio hardware connections

### Frame Rate Lower Than Expected

- Network congestion
- Too many WebSocket clients
- Client not processing frames fast enough

### Subscription Rejected

```json
{"type": "audioStream.error", "error": "subscription_failed"}
```

- Maximum clients (4) already subscribed
- Try unsubscribing other clients first

---

## Related Documentation

- [Audio Control API](AUDIO_CONTROL_API.md) - Pause/resume capture, beat events, tuning presets
- [API V1 Reference](../docs/api/API_V1.md) - Full REST API documentation
