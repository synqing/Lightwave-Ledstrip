# FFT WebSocket Streaming - Usage Guide

**Feature:** Real-time 64-bin FFT data streaming via WebSocket
**API Version:** 1.0
**Update Rate:** ~31 Hz (32ms intervals)
**Max Clients:** 4 concurrent subscriptions

---

## Quick Start

### 1. Connect to WebSocket

```javascript
const ws = new WebSocket('ws://lightwaveos.local/ws');

ws.onopen = () => {
  console.log('Connected to LightwaveOS');
};

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);

  if (msg.type === 'audio.fft.subscribed') {
    console.log('FFT subscription successful', msg);
  } else if (msg.type === 'audio.fft.frame') {
    handleFftFrame(msg);
  } else if (msg.type === 'audio.fft.rejected') {
    console.error('FFT subscription rejected:', msg.error.message);
  }
};
```

### 2. Subscribe to FFT Stream

```javascript
ws.send(JSON.stringify({
  type: 'audio.fft.subscribe',
  requestId: 'subscribe-1'
}));
```

**Expected Response (within 100ms):**
```json
{
  "type": "audio.fft.subscribed",
  "requestId": "subscribe-1",
  "clientId": 2048,
  "fftBins": 64,
  "updateRateHz": 31,
  "accepted": true
}
```

### 3. Handle FFT Frames

Frames arrive approximately every 32 milliseconds:

```javascript
function handleFftFrame(frame) {
  console.log('FFT Frame:', frame.hopSeq, frame.bins.length, 'bins');

  // Access frequency data
  const bass = frame.bins.slice(0, 16).reduce((a, b) => a + b) / 16;     // 55-82 Hz
  const treble = frame.bins.slice(48, 64).reduce((a, b) => a + b) / 16;  // 988-2093 Hz

  console.log(`Bass: ${(bass * 100).toFixed(1)}%, Treble: ${(treble * 100).toFixed(1)}%`);
}
```

### 4. Unsubscribe from FFT Stream

```javascript
ws.send(JSON.stringify({
  type: 'audio.fft.unsubscribe',
  requestId: 'unsubscribe-1'
}));
```

**Expected Response:**
```json
{
  "type": "audio.fft.unsubscribed",
  "requestId": "unsubscribe-1",
  "clientId": 2048
}
```

---

## FFT Bin Frequency Mapping

The 64 FFT bins are semitone-spaced across the audible spectrum.

| Bin Range | Frequency Range | Musical Range | Description |
|-----------|-----------------|---------------|-------------|
| 0-7       | 55-82 Hz        | A1-E2         | Sub-bass / kick fundamentals |
| 8-15      | 87-123 Hz       | F2-B2         | Bass / bass guitar |
| 16-23     | 131-185 Hz      | C3-F#3        | Low-mids / male vocals |
| 24-31     | 196-277 Hz      | G3-C#4        | Mids / snare fundamentals |
| 32-39     | 294-415 Hz      | D4-G#4        | Upper mids / female vocals |
| 40-47     | 440-622 Hz      | A4-D#5        | Presence / cymbals |
| 48-55     | 659-932 Hz      | E5-A#5        | Treble / hi-hat |
| 56-63     | 988-2093 Hz     | B5-C7         | Air / shimmer |

### Example: Extract Music Frequency Bands

```javascript
function extractBands(frame) {
  const bins = frame.bins;

  return {
    subBass: average(bins.slice(0, 4)),      // 55-65 Hz
    bass: average(bins.slice(4, 16)),        // 65-123 Hz
    lowMid: average(bins.slice(16, 24)),     // 131-185 Hz
    mid: average(bins.slice(24, 32)),        // 196-277 Hz
    highMid: average(bins.slice(32, 40)),    // 294-415 Hz
    presence: average(bins.slice(40, 48)),   // 440-622 Hz
    treble: average(bins.slice(48, 56)),     // 659-932 Hz
    air: average(bins.slice(56, 64))         // 988-2093 Hz
  };
}

function average(arr) {
  return arr.reduce((a, b) => a + b, 0) / arr.length;
}
```

---

## Data Format Reference

### Subscribe Request
```json
{
  "type": "audio.fft.subscribe",
  "requestId": "any-unique-string"
}
```

### Subscription Confirmation
```json
{
  "type": "audio.fft.subscribed",
  "requestId": "any-unique-string",
  "clientId": <client-socket-id>,
  "fftBins": 64,
  "updateRateHz": 31,
  "accepted": true
}
```

### Subscription Rejection
```json
{
  "type": "audio.fft.rejected",
  "requestId": "any-unique-string",
  "success": false,
  "error": {
    "code": "RESOURCE_EXHAUSTED",
    "message": "FFT subscriber table full (max 4 concurrent clients)"
  }
}
```

### FFT Frame (Streaming)
```json
{
  "type": "audio.fft.frame",
  "hopSeq": <sequence-number>,
  "bins": [
    <float 0.0-1.0>,  // Bin 0: 55 Hz
    <float 0.0-1.0>,  // Bin 1: 58.3 Hz
    // ... 62 more bins ...
    <float 0.0-1.0>   // Bin 63: 2093 Hz
  ]
}
```

### Unsubscribe Request
```json
{
  "type": "audio.fft.unsubscribe",
  "requestId": "any-unique-string"
}
```

### Unsubscribe Confirmation
```json
{
  "type": "audio.fft.unsubscribed",
  "requestId": "any-unique-string",
  "clientId": <client-socket-id>
}
```

---

## Common Patterns

### Pattern 1: Simple Frequency Monitoring

```javascript
const frequencyBands = {};
let frameCount = 0;

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);

  if (msg.type === 'audio.fft.frame') {
    // Update frequency data
    frequencyBands.low = avg(msg.bins.slice(0, 16));
    frequencyBands.mid = avg(msg.bins.slice(16, 48));
    frequencyBands.high = avg(msg.bins.slice(48, 64));

    // Log every 10th frame (3 seconds at 31 Hz)
    if (++frameCount % 10 === 0) {
      console.log(`Low: ${freq.low.toFixed(3)}, Mid: ${freq.mid.toFixed(3)}, High: ${freq.high.toFixed(3)}`);
    }
  }
};

function avg(arr) {
  return arr.reduce((a, b) => a + b, 0) / arr.length;
}
```

### Pattern 2: Onset Detection (Kick Detection)

```javascript
const history = {
  current: 0,
  previous: 0
};

const KICK_THRESHOLD = 0.7;
const KICK_BANDS = [0, 1, 2, 3];  // Sub-bass: 55-65 Hz

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);

  if (msg.type === 'audio.fft.frame') {
    // Get sub-bass energy
    const subBassEnergy = avg(KICK_BANDS.map(i => msg.bins[i]));

    // Detect kick: sudden increase in bass energy
    if (subBassEnergy > KICK_THRESHOLD && subBassEnergy > history.previous * 1.5) {
      console.log('KICK DETECTED!');
      triggerVisualEffect();
    }

    history.previous = history.current;
    history.current = subBassEnergy;
  }
};
```

### Pattern 3: Spectrum Analyzer Display

```javascript
class SpectrumAnalyzer {
  constructor(canvasId) {
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas.getContext('2d');
    this.currentBins = new Array(64).fill(0);
  }

  updateFrame(fftFrame) {
    // Smooth update using exponential moving average
    const alpha = 0.7;  // 70% new data, 30% history
    for (let i = 0; i < 64; i++) {
      this.currentBins[i] = alpha * fftFrame.bins[i] + (1 - alpha) * this.currentBins[i];
    }
    this.draw();
  }

  draw() {
    const w = this.canvas.width / 64;
    const h = this.canvas.height;

    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    this.ctx.fillStyle = 'rgb(0, 200, 100)';

    for (let i = 0; i < 64; i++) {
      const barHeight = this.currentBins[i] * h;
      this.ctx.fillRect(i * w, h - barHeight, w - 1, barHeight);
    }
  }
}

// Usage
const analyzer = new SpectrumAnalyzer('spectrum-canvas');

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  if (msg.type === 'audio.fft.frame') {
    analyzer.updateFrame(msg);
  }
};
```

### Pattern 4: Multi-Client (4 Dashboard Tabs)

```javascript
// Create separate WebSocket for each dashboard tab
const wsClient1 = new WebSocket('ws://lightwaveos.local/ws');
const wsClient2 = new WebSocket('ws://lightwaveos.local/ws');
const wsClient3 = new WebSocket('ws://lightwaveos.local/ws');

let subscribedClients = 0;

[wsClient1, wsClient2, wsClient3].forEach((ws) => {
  ws.onopen = () => {
    ws.send(JSON.stringify({type: 'audio.fft.subscribe', requestId: 'sub-' + Date.now()}));
  };

  ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    if (msg.type === 'audio.fft.subscribed') {
      subscribedClients++;
      console.log(`${subscribedClients}/4 clients subscribed`);
    } else if (msg.type === 'audio.fft.rejected') {
      console.warn('Subscription rejected - max 4 clients');
    }
  };
});
```

---

## Performance Considerations

### Frame Rate
- **Nominal:** 31 Hz (32ms intervals)
- **Jitter:** ±5% expected
- **Latency:** ~32ms from audio capture to WebSocket delivery

### Bandwidth
- **Per Frame:** ~400-500 bytes (JSON)
- **Streaming Rate:** ~12-16 KB/sec per client
- **4 Clients:** ~50-64 KB/sec total

### CPU Impact
- **Per Client:** <0.1% CPU (on ESP32-S3 @ 240 MHz)
- **Total Overhead:** <0.5% for FFT broadcast cycle

### Memory
- **Static:** 32 bytes (4 subscriber pointers)
- **Per Frame:** ~500 bytes JSON (temporary)
- **No persistent buffers:** Memory freed after each frame

---

## Error Handling

### Subscription Rejected

```javascript
ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);

  if (msg.type === 'audio.fft.rejected') {
    // Max 4 concurrent clients reached
    if (msg.error.code === 'RESOURCE_EXHAUSTED') {
      // Show user friendly error
      alert('FFT streaming unavailable (max 4 active). Close other dashboard tabs.');
    }

    // Retry in 5 seconds
    setTimeout(() => {
      ws.send(JSON.stringify({
        type: 'audio.fft.subscribe',
        requestId: 'retry-' + Date.now()
      }));
    }, 5000);
  }
};
```

### Connection Lost

```javascript
ws.onclose = () => {
  console.log('WebSocket disconnected. Audio sync stopped.');

  // Try to reconnect
  setTimeout(() => {
    location.reload();  // or reconnect via new WebSocket
  }, 3000);
};

ws.onerror = (error) => {
  console.error('WebSocket error:', error);
  // Handle gracefully
};
```

---

## Troubleshooting

### Frames not being received

1. **Check subscription confirmation:**
   ```javascript
   ws.onmessage = (msg) => {
     if (msg.type === 'audio.fft.subscribed') {
       console.log('Subscription OK');
     } else if (msg.type === 'audio.fft.rejected') {
       console.error('Subscription failed:', msg.error);
     }
   };
   ```

2. **Verify max 4 clients:**
   - Open only one dashboard tab
   - Check Device Developers Tools for any other WebSocket connections

3. **Check device is running audio:**
   - Visit http://lightwaveos.local/api/v1/device/status
   - Verify `audio_sync_enabled: true`

### Frame rate irregular

1. **Network latency:** Check WiFi signal strength
   ```
   GET http://lightwaveos.local/api/v1/network/status
   Look for RSSI value > -70 dBm
   ```

2. **Device CPU overloaded:** Check CPU usage
   ```
   GET http://lightwaveos.local/api/v1/device/status
   Look for cpu_percent < 80
   ```

### Jitter in frames

- Expected ±5% (±2ms at 31 Hz)
- Use exponential moving average (alpha ~0.7) to smooth display

---

## Browser Compatibility

| Browser | Version | FFT Support |
|---------|---------|-------------|
| Chrome  | 60+     | Full |
| Firefox | 55+     | Full |
| Safari  | 12+     | Full |
| Edge    | 79+     | Full |
| Mobile  | Any     | Full |

All modern browsers support WebSocket and JSON parsing needed for this API.

---

## Complete Example: Live Spectrum Dashboard

```html
<!DOCTYPE html>
<html>
<head>
  <title>FFT Spectrum Analyzer</title>
  <style>
    body { font-family: sans-serif; margin: 20px; }
    canvas { border: 1px solid #ccc; display: block; margin: 20px 0; }
    #status { padding: 10px; background: #f0f0f0; border-radius: 4px; }
  </style>
</head>
<body>
  <h1>FFT Spectrum Analyzer</h1>
  <div id="status">Status: <span id="statusText">Connecting...</span></div>
  <canvas id="spectrum" width="1024" height="300"></canvas>
  <canvas id="waveform" width="1024" height="200"></canvas>

  <script>
    class SpectrumDashboard {
      constructor() {
        this.ws = new WebSocket('ws://lightwaveos.local/ws');
        this.specCanvas = document.getElementById('spectrum');
        this.specCtx = this.specCanvas.getContext('2d');
        this.currentBins = new Array(64).fill(0);
        this.connected = false;
        this.frameCount = 0;

        this.setupWebSocket();
      }

      setupWebSocket() {
        this.ws.onopen = () => {
          this.setStatus('Connected - subscribing to FFT...');
          this.ws.send(JSON.stringify({
            type: 'audio.fft.subscribe',
            requestId: 'dashboard-' + Date.now()
          }));
        };

        this.ws.onmessage = (event) => {
          const msg = JSON.parse(event.data);

          if (msg.type === 'audio.fft.subscribed') {
            this.connected = true;
            this.setStatus('Receiving FFT stream (31 Hz)');
          } else if (msg.type === 'audio.fft.rejected') {
            this.setStatus('ERROR: ' + msg.error.message);
          } else if (msg.type === 'audio.fft.frame') {
            this.updateFrame(msg);
          }
        };

        this.ws.onclose = () => {
          this.connected = false;
          this.setStatus('Disconnected');
        };
      }

      updateFrame(frame) {
        // Exponential moving average for smoothing
        const alpha = 0.8;
        for (let i = 0; i < 64; i++) {
          this.currentBins[i] = alpha * frame.bins[i] + (1 - alpha) * this.currentBins[i];
        }

        this.draw();

        // Log stats every 31 frames (1 second at 31 Hz)
        if (++this.frameCount % 31 === 0) {
          const avg = this.currentBins.reduce((a, b) => a + b) / 64;
          this.setStatus(`Receiving FFT stream (31 Hz) - Avg Energy: ${(avg * 100).toFixed(1)}%`);
        }
      }

      draw() {
        const w = this.specCanvas.width / 64;
        const h = this.specCanvas.height;

        this.specCtx.clearRect(0, 0, this.specCanvas.width, this.specCanvas.height);

        // Draw spectrum bars
        const gradient = this.specCtx.createLinearGradient(0, 0, this.specCanvas.width, 0);
        gradient.addColorStop(0, 'rgb(0, 100, 200)');  // Blue (bass)
        gradient.addColorStop(0.5, 'rgb(100, 200, 0)');  // Green (mid)
        gradient.addColorStop(1, 'rgb(200, 100, 0)');  // Red (treble)

        this.specCtx.fillStyle = gradient;
        for (let i = 0; i < 64; i++) {
          const barHeight = this.currentBins[i] * h;
          this.specCtx.fillRect(i * w, h - barHeight, w - 1, barHeight);
        }

        // Draw grid
        this.specCtx.strokeStyle = 'rgba(200, 200, 200, 0.3)';
        this.specCtx.lineWidth = 1;
        for (let i = 0; i <= 8; i++) {
          const x = (this.specCanvas.width / 8) * i;
          this.specCtx.beginPath();
          this.specCtx.moveTo(x, 0);
          this.specCtx.lineTo(x, h);
          this.specCtx.stroke();
        }
      }

      setStatus(text) {
        document.getElementById('statusText').textContent = text;
      }
    }

    // Start dashboard
    const dashboard = new SpectrumDashboard();
  </script>
</body>
</html>
```

---

## References

- **WebSocket API:** https://developer.mozilla.org/en-US/docs/Web/API/WebSocket
- **FFT Bin Mapping:** Semitone-spaced 64-bin Goertzel analysis from AudioNode
- **LightwaveOS Documentation:** http://lightwaveos.local/
- **Feature:** FFT WebSocket Streaming (Feature C)
