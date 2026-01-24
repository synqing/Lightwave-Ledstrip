# Audio Control API Documentation

## Overview

LightwaveOS v2 provides comprehensive audio control APIs for managing audio capture, receiving beat events, and persisting audio tuning configurations. This document covers:

1. **Audio Capture Control** - Pause/resume audio processing for power saving
2. **Beat Event Streaming** - Real-time beat and tempo notifications
3. **Audio Tuning Presets** - Save/load audio DSP configurations to NVS

All features require the `esp32dev_audio` build environment with `FEATURE_AUDIO_SYNC=1`.

---

## Table of Contents

1. [Requirements](#requirements)
2. [Audio Capture Control](#audio-capture-control)
3. [Beat Event Streaming](#beat-event-streaming)
4. [Audio Tuning Presets](#audio-tuning-presets)
5. [Tuning Parameters Reference](#tuning-parameters-reference)
6. [JavaScript Examples](#javascript-examples)
7. [Error Codes](#error-codes)
8. [Implementation Files](#implementation-files)

---

## Requirements

- **Firmware**: Built with `pio run -e esp32dev_audio`
- **Feature Flag**: `FEATURE_AUDIO_SYNC=1` (automatically set by esp32dev_audio)
- **Connection**: REST via `http://lightwaveos.local/api/v1/` or WebSocket via `ws://lightwaveos.local/ws`

---

## Audio Capture Control

Control the audio capture pipeline to pause/resume processing. Useful for power saving when audio-reactive features aren't needed.

### REST API

#### Get Audio State

```http
GET /api/v1/audio/state
```

**Response:**
```json
{
  "success": true,
  "data": {
    "state": "RUNNING",
    "capturing": true,
    "hopCount": 12345,
    "sampleIndex": 987654,
    "stats": {
      "tickCount": 50000,
      "captureSuccess": 49998,
      "captureFail": 2
    }
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**State Values:**
| State | Description |
|-------|-------------|
| `UNINITIALIZED` | Audio system not yet started |
| `INITIALIZING` | I2S/DMA setup in progress |
| `RUNNING` | Actively capturing and processing audio |
| `PAUSED` | Capture suspended, DSP idle |
| `ERROR` | Hardware or configuration error |

#### Control Audio Capture

```http
POST /api/v1/audio/control
Content-Type: application/json

{
  "action": "pause"
}
```

**Actions:**
| Action | Description |
|--------|-------------|
| `pause` | Suspend audio capture, DSP enters idle mode |
| `resume` | Resume audio capture and processing |

**Response:**
```json
{
  "success": true,
  "data": {
    "state": "PAUSED",
    "action": "pause"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

### WebSocket Commands

#### Get Audio State

```json
{
  "type": "audio.state.get",
  "requestId": "optional-correlation-id"
}
```

**Response:**
```json
{
  "type": "audio.state",
  "requestId": "optional-correlation-id",
  "success": true,
  "data": {
    "state": "RUNNING",
    "capturing": true,
    "hopCount": 12345,
    "sampleIndex": 987654,
    "stats": {
      "tickCount": 50000,
      "captureSuccess": 49998,
      "captureFail": 2
    }
  }
}
```

#### Pause/Resume Capture

```json
{
  "type": "audio.control",
  "action": "pause",
  "requestId": "optional-correlation-id"
}
```

**Response:**
```json
{
  "type": "audio.control",
  "requestId": "optional-correlation-id",
  "success": true,
  "data": {
    "state": "PAUSED",
    "action": "pause"
  }
}
```

---

## Beat Event Streaming

Receive real-time beat and downbeat notifications over WebSocket. Events are pushed automatically when beats are detected - no subscription required.

### Event Format

When a beat is detected, all connected WebSocket clients receive:

```json
{
  "type": "beat.event",
  "tick": true,
  "downbeat": false,
  "beat_index": 245,
  "bar_index": 61,
  "beat_in_bar": 1,
  "beat_phase": 0.0,
  "bpm": 124.5,
  "confidence": 0.87
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `tick` | boolean | True on any beat |
| `downbeat` | boolean | True on first beat of bar (beat_in_bar == 0) |
| `beat_index` | uint32 | Monotonic beat counter since boot |
| `bar_index` | uint32 | Monotonic bar counter since boot |
| `beat_in_bar` | uint8 | Position within bar (0 to beatsPerBar-1) |
| `beat_phase` | float | Phase within current beat (0.0-1.0) |
| `bpm` | float | Smoothed tempo estimate |
| `confidence` | float | Beat detection confidence (0.0-1.0) |

### Event Timing

- Events are broadcast at render rate (~60 FPS) but only sent when `tick` or `downbeat` is true
- Typical beat events occur every 400-600ms at 100-150 BPM
- `beat_phase` resets to 0.0 on each tick

### Usage Example

```javascript
ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);

  if (msg.type === 'beat.event') {
    if (msg.downbeat) {
      // First beat of bar - trigger major visual change
      triggerDownbeatEffect();
    } else if (msg.tick) {
      // Regular beat - pulse animation
      triggerBeatPulse();
    }

    // Use beat_phase for smooth animations
    updateBeatPhaseAnimation(msg.beat_phase);

    // Display tempo
    displayBPM(msg.bpm);
  }
};
```

---

## Audio Tuning Presets

Save and load audio DSP tuning configurations to non-volatile storage. Supports up to 10 named presets that persist across reboots.

### REST API

#### List All Presets

```http
GET /api/v1/audio/presets
```

**Response:**
```json
{
  "success": true,
  "data": {
    "count": 2,
    "presets": [
      { "id": 0, "name": "Bass Heavy" },
      { "id": 3, "name": "Balanced" }
    ]
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

#### Get Preset Details

```http
GET /api/v1/audio/presets/get?id=0
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 0,
    "name": "Bass Heavy",
    "pipeline": {
      "dcAlpha": 0.001,
      "agcTargetRms": 0.25,
      "agcMinGain": 1.0,
      "agcMaxGain": 100.0,
      "agcAttack": 0.08,
      "agcRelease": 0.02,
      "agcClipReduce": 0.90,
      "agcIdleReturnRate": 0.01,
      "noiseFloorMin": 0.0004,
      "noiseFloorRise": 0.0005,
      "noiseFloorFall": 0.01,
      "gateStartFactor": 1.0,
      "gateRangeFactor": 1.5,
      "gateRangeMin": 0.0005,
      "rmsDbFloor": -65.0,
      "rmsDbCeil": -12.0,
      "bandDbFloor": -65.0,
      "bandDbCeil": -12.0,
      "chromaDbFloor": -65.0,
      "chromaDbCeil": -12.0,
      "fluxScale": 1.0,
      "controlBusAlphaFast": 0.35,
      "controlBusAlphaSlow": 0.12
    },
    "contract": {
      "audioStalenessMs": 100.0,
      "bpmMin": 30.0,
      "bpmMax": 300.0,
      "bpmTau": 0.50,
      "confidenceTau": 1.00,
      "phaseCorrectionGain": 0.35,
      "barCorrectionGain": 0.20,
      "beatsPerBar": 4,
      "beatUnit": 4
    }
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

#### Save Current Tuning as Preset

Saves the current audio pipeline and contract tuning to a new preset slot.

```http
POST /api/v1/audio/presets
Content-Type: application/json

{
  "name": "My Custom Preset"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 4,
    "name": "My Custom Preset",
    "message": "Preset saved"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**Error (no free slots):**
```json
{
  "success": false,
  "error": {
    "code": "STORAGE_FULL",
    "message": "No free preset slots"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

#### Apply Preset

Loads a preset and applies its tuning to the audio pipeline.

```http
POST /api/v1/audio/presets/apply?id=0
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 0,
    "name": "Bass Heavy",
    "message": "Preset applied"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

#### Delete Preset

```http
DELETE /api/v1/audio/presets/delete?id=0
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 0,
    "message": "Preset deleted"
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

### WebSocket Commands

#### List Presets

```json
{
  "type": "audio.preset.list",
  "requestId": "optional"
}
```

**Response:**
```json
{
  "type": "audio.preset.list",
  "requestId": "optional",
  "success": true,
  "data": {
    "count": 2,
    "presets": [
      { "id": 0, "name": "Bass Heavy" },
      { "id": 3, "name": "Balanced" }
    ]
  }
}
```

#### Save Preset

```json
{
  "type": "audio.preset.save",
  "name": "My Preset",
  "requestId": "optional"
}
```

**Response:**
```json
{
  "type": "audio.preset.saved",
  "requestId": "optional",
  "success": true,
  "data": {
    "id": 4,
    "name": "My Preset",
    "message": "Preset saved"
  }
}
```

#### Load/Apply Preset

```json
{
  "type": "audio.preset.load",
  "presetId": 0,
  "requestId": "optional"
}
```

**Response:**
```json
{
  "type": "audio.preset.loaded",
  "requestId": "optional",
  "success": true,
  "data": {
    "id": 0,
    "name": "Bass Heavy",
    "message": "Preset applied"
  }
}
```

#### Delete Preset

```json
{
  "type": "audio.preset.delete",
  "presetId": 0,
  "requestId": "optional"
}
```

**Response:**
```json
{
  "type": "audio.preset.deleted",
  "requestId": "optional",
  "success": true,
  "data": {
    "id": 0,
    "message": "Preset deleted"
  }
}
```

---

## Tuning Parameters Reference

### Pipeline Tuning (DSP Parameters)

Controls low-level digital signal processing.

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `dcAlpha` | float | 0.001 | 0.000001-0.1 | DC offset removal filter coefficient |
| `agcTargetRms` | float | 0.25 | 0.01-1.0 | Target RMS level for AGC |
| `agcMinGain` | float | 1.0 | 0.1-50.0 | Minimum AGC gain multiplier |
| `agcMaxGain` | float | 100.0 | 1.0-500.0 | Maximum AGC gain multiplier |
| `agcAttack` | float | 0.08 | 0.0-1.0 | AGC attack rate (fast response) |
| `agcRelease` | float | 0.02 | 0.0-1.0 | AGC release rate (slow recovery) |
| `agcClipReduce` | float | 0.90 | 0.1-1.0 | Gain reduction on clipping |
| `agcIdleReturnRate` | float | 0.01 | 0.0-1.0 | Gain return rate during silence |
| `noiseFloorMin` | float | 0.0004 | 0.0-0.1 | Minimum noise floor threshold |
| `noiseFloorRise` | float | 0.0005 | 0.0-1.0 | Noise floor rise rate |
| `noiseFloorFall` | float | 0.01 | 0.0-1.0 | Noise floor fall rate |
| `gateStartFactor` | float | 1.0 | 0.0-10.0 | Noise gate activation threshold |
| `gateRangeFactor` | float | 1.5 | 0.0-10.0 | Noise gate range multiplier |
| `gateRangeMin` | float | 0.0005 | 0.0-0.1 | Minimum gate range |
| `rmsDbFloor` | float | -65.0 | -120-0 | RMS dB floor for normalization |
| `rmsDbCeil` | float | -12.0 | -120-0 | RMS dB ceiling for normalization |
| `bandDbFloor` | float | -65.0 | -120-0 | Band energy dB floor |
| `bandDbCeil` | float | -12.0 | -120-0 | Band energy dB ceiling |
| `chromaDbFloor` | float | -65.0 | -120-0 | Chroma dB floor |
| `chromaDbCeil` | float | -12.0 | -120-0 | Chroma dB ceiling |
| `fluxScale` | float | 1.0 | 0.0-10.0 | Spectral flux scaling factor |
| `controlBusAlphaFast` | float | 0.35 | 0.0-1.0 | Fast control bus smoothing |
| `controlBusAlphaSlow` | float | 0.12 | 0.0-1.0 | Slow control bus smoothing |

### Contract Tuning (Tempo/Beat Parameters)

Controls musical timing and beat detection.

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `audioStalenessMs` | float | 100.0 | 0-1000 | Audio staleness timeout (ms) |
| `bpmMin` | float | 30.0 | 20-120 | Minimum detectable BPM |
| `bpmMax` | float | 300.0 | 60-400 | Maximum detectable BPM |
| `bpmTau` | float | 0.50 | 0.1-10 | BPM smoothing time constant |
| `confidenceTau` | float | 1.00 | 0.1-10 | Confidence smoothing time constant |
| `phaseCorrectionGain` | float | 0.35 | 0.0-1.0 | Beat phase correction strength |
| `barCorrectionGain` | float | 0.20 | 0.0-1.0 | Bar alignment correction strength |
| `beatsPerBar` | uint8 | 4 | 1-16 | Time signature numerator |
| `beatUnit` | uint8 | 4 | 1-16 | Time signature denominator |

---

## JavaScript Examples

### Complete Audio Control Manager

```javascript
class AudioControlManager {
  constructor(wsUrl = 'ws://lightwaveos.local/ws') {
    this.ws = null;
    this.wsUrl = wsUrl;
    this.callbacks = {
      beat: [],
      state: [],
      preset: []
    };
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.ws = new WebSocket(this.wsUrl);
      this.ws.onopen = () => resolve();
      this.ws.onerror = reject;
      this.ws.onmessage = (event) => this.handleMessage(event);
    });
  }

  handleMessage(event) {
    if (typeof event.data === 'string') {
      const msg = JSON.parse(event.data);

      switch (msg.type) {
        case 'beat.event':
          this.callbacks.beat.forEach(cb => cb(msg));
          break;
        case 'audio.state':
        case 'audio.control':
          this.callbacks.state.forEach(cb => cb(msg.data));
          break;
        case 'audio.preset.list':
        case 'audio.preset.saved':
        case 'audio.preset.loaded':
        case 'audio.preset.deleted':
          this.callbacks.preset.forEach(cb => cb(msg));
          break;
      }
    }
  }

  onBeat(callback) {
    this.callbacks.beat.push(callback);
  }

  onStateChange(callback) {
    this.callbacks.state.push(callback);
  }

  onPresetChange(callback) {
    this.callbacks.preset.push(callback);
  }

  // Audio Control
  async getState() {
    this.ws.send(JSON.stringify({ type: 'audio.state.get' }));
  }

  async pause() {
    this.ws.send(JSON.stringify({
      type: 'audio.control',
      action: 'pause'
    }));
  }

  async resume() {
    this.ws.send(JSON.stringify({
      type: 'audio.control',
      action: 'resume'
    }));
  }

  // Preset Management
  async listPresets() {
    this.ws.send(JSON.stringify({ type: 'audio.preset.list' }));
  }

  async savePreset(name) {
    this.ws.send(JSON.stringify({
      type: 'audio.preset.save',
      name: name
    }));
  }

  async loadPreset(id) {
    this.ws.send(JSON.stringify({
      type: 'audio.preset.load',
      presetId: id
    }));
  }

  async deletePreset(id) {
    this.ws.send(JSON.stringify({
      type: 'audio.preset.delete',
      presetId: id
    }));
  }
}

// Usage
const audio = new AudioControlManager();

await audio.connect();

// Listen for beats
audio.onBeat((beat) => {
  if (beat.downbeat) {
    console.log(`DOWNBEAT! Bar ${beat.bar_index}, BPM: ${beat.bpm.toFixed(1)}`);
  } else if (beat.tick) {
    console.log(`Beat ${beat.beat_in_bar + 1}/${beat.beatsPerBar || 4}`);
  }
});

// Control audio
await audio.pause();
await audio.resume();

// Manage presets
await audio.listPresets();
await audio.savePreset('My Settings');
await audio.loadPreset(0);
```

### React Hook for Beat Events

```javascript
import { useState, useEffect, useRef } from 'react';

function useBeatEvents(wsUrl = 'ws://lightwaveos.local/ws') {
  const [beat, setBeat] = useState(null);
  const [bpm, setBpm] = useState(0);
  const [connected, setConnected] = useState(false);
  const wsRef = useRef(null);

  useEffect(() => {
    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => setConnected(true);
    ws.onclose = () => setConnected(false);

    ws.onmessage = (event) => {
      if (typeof event.data === 'string') {
        const msg = JSON.parse(event.data);
        if (msg.type === 'beat.event') {
          setBeat(msg);
          setBpm(msg.bpm);
        }
      }
    };

    return () => ws.close();
  }, [wsUrl]);

  return { beat, bpm, connected };
}

// Usage
function BeatVisualizer() {
  const { beat, bpm, connected } = useBeatEvents();
  const [flash, setFlash] = useState(false);

  useEffect(() => {
    if (beat?.tick) {
      setFlash(true);
      setTimeout(() => setFlash(false), 100);
    }
  }, [beat?.beat_index]);

  return (
    <div className={flash ? 'beat-flash' : ''}>
      <p>BPM: {bpm.toFixed(1)}</p>
      <p>Beat: {beat?.beat_in_bar + 1}/4</p>
      <p>Bar: {beat?.bar_index}</p>
    </div>
  );
}
```

---

## Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `AUDIO_UNAVAILABLE` | 503 | Audio system not initialized or in error state |
| `INVALID_ACTION` | 400 | Unknown action (must be `pause` or `resume`) |
| `STORAGE_FULL` | 507 | No free preset slots (max 10 presets) |
| `NOT_FOUND` | 404 | Preset ID does not exist |
| `OUT_OF_RANGE` | 400 | Preset ID must be 0-9 |
| `MISSING_FIELD` | 400 | Required parameter missing (e.g., `id`) |
| `FEATURE_DISABLED` | 503 | Audio sync feature not enabled in build |

---

## Implementation Files

### New Files Created

| File | Purpose |
|------|---------|
| `v2/src/core/persistence/AudioTuningManager.h` | Preset manager class definition |
| `v2/src/core/persistence/AudioTuningManager.cpp` | NVS persistence implementation |

### Modified Files

| File | Changes |
|------|---------|
| `v2/src/network/WebServer.h` | Handler declarations for audio control/presets |
| `v2/src/network/WebServer.cpp` | REST routes, WebSocket commands, handlers |
| `v2/src/network/ApiResponse.h` | New error codes: `STORAGE_FULL`, `INVALID_PARAMETER` |
| `v2/src/core/actors/RendererActor.h` | Added `getLastMusicalGrid()` accessor |

### Storage Details

- **NVS Namespace**: `audio_tune`
- **Key Format**: `preset_0` through `preset_9`
- **Preset Size**: ~120 bytes (with checksum)
- **Max Presets**: 10
- **Checksum**: CRC32 validation on load

### Memory Impact

- **RAM**: ~200 bytes for AudioTuningManager singleton
- **Flash**: ~3KB code for preset management
- **NVS**: ~1.2KB when all 10 presets saved

---

# Phase 4: Audio-Effect Parameter Mapping

## Overview

The Audio-Effect Mapping system provides runtime-configurable bindings from audio sources to visual parameters on a per-effect basis. This allows any effect to become audio-reactive without hardcoded audio handling.

### Key Features

- **19 Audio Sources**: Energy metrics, frequency bands, aggregates, and timing
- **7 Visual Targets**: Brightness, speed, intensity, saturation, complexity, variation, hue
- **6 Response Curves**: LINEAR, SQUARED, SQRT, LOG, EXP, INVERTED
- **IIR Smoothing**: Configurable alpha coefficient per mapping
- **Per-Effect Configuration**: Up to 8 mappings per effect, 80 effects supported
- **Performance Instrumented**: <200μs per applyMappings() call

---

## Audio Sources

| ID | Name | Category | Range | Description |
|----|------|----------|-------|-------------|
| 0 | `RMS` | energy | 0-1 | Smoothed RMS level |
| 1 | `FAST_RMS` | energy | 0-1 | Fast-attack RMS |
| 2 | `FLUX` | energy | 0-1 | Spectral flux (onset) |
| 3 | `FAST_FLUX` | energy | 0-1 | Fast-attack flux |
| 4-11 | `BAND_0`-`BAND_7` | frequency | 0-1 | Individual frequency bands (60Hz-7.8kHz) |
| 12 | `BASS` | aggregate | 0-1 | (band0 + band1) / 2 |
| 13 | `MID` | aggregate | 0-1 | (band2 + band3 + band4) / 3 |
| 14 | `TREBLE` | aggregate | 0-1 | (band5 + band6 + band7) / 3 |
| 15 | `HEAVY_BASS` | aggregate | 0-1 | Squared bass response |
| 16 | `BEAT_PHASE` | timing | 0-1 | Beat phase [0,1) |
| 17 | `BPM` | timing | 30-300 | Tempo in BPM |
| 18 | `TEMPO_CONFIDENCE` | timing | 0-1 | Beat detection confidence |

## Visual Targets

| ID | Name | Range | Default | Effect |
|----|------|-------|---------|--------|
| 0 | `BRIGHTNESS` | 0-160 | 96 | Master LED intensity |
| 1 | `SPEED` | 1-50 | 10 | Animation rate |
| 2 | `INTENSITY` | 0-255 | 128 | Effect amplitude |
| 3 | `SATURATION` | 0-255 | 255 | Color saturation |
| 4 | `COMPLEXITY` | 0-255 | 128 | Pattern detail |
| 5 | `VARIATION` | 0-255 | 0 | Mode selection |
| 6 | `HUE` | 0-255 | auto | Color rotation |

## Mapping Curves

| Name | Formula | Description |
|------|---------|-------------|
| `LINEAR` | y = x | Direct proportional |
| `SQUARED` | y = x² | Gentle start, aggressive end |
| `SQRT` | y = √x | Aggressive start, gentle end |
| `LOG` | y = log(x+1)/log(2) | Logarithmic compression |
| `EXP` | y = (eˣ-1)/(e-1) | Exponential expansion |
| `INVERTED` | y = 1 - x | Inverse |

---

## REST API Endpoints

### List Audio Sources

```
GET /api/v1/audio/mappings/sources
```

**Response:**
```json
{
  "success": true,
  "data": {
    "sources": [
      {
        "name": "BASS",
        "id": 12,
        "category": "aggregate",
        "description": "(band0 + band1) / 2",
        "rangeMin": 0.0,
        "rangeMax": 1.0
      }
    ]
  }
}
```

### List Visual Targets

```
GET /api/v1/audio/mappings/targets
```

### List Curve Types

```
GET /api/v1/audio/mappings/curves
```

### List Effects with Active Mappings

```
GET /api/v1/audio/mappings
```

**Response:**
```json
{
  "success": true,
  "data": {
    "activeEffects": 2,
    "totalMappings": 5,
    "effects": [
      {
        "id": 5,
        "name": "LGP Beat Pulse",
        "mappingCount": 2,
        "enabled": true
      }
    ]
  }
}
```

### Get Mapping for Effect

```
GET /api/v1/audio/mappings/effect?id=5
```

**Response:**
```json
{
  "success": true,
  "data": {
    "effectId": 5,
    "effectName": "LGP Beat Pulse",
    "globalEnabled": true,
    "mappingCount": 2,
    "mappings": [
      {
        "source": "BASS",
        "target": "INTENSITY",
        "curve": "SQUARED",
        "inputMin": 0.0,
        "inputMax": 1.0,
        "outputMin": 64,
        "outputMax": 255,
        "smoothingAlpha": 0.3,
        "gain": 1.2,
        "enabled": true,
        "additive": false
      },
      {
        "source": "BEAT_PHASE",
        "target": "BRIGHTNESS",
        "curve": "LINEAR",
        "inputMin": 0.0,
        "inputMax": 1.0,
        "outputMin": 40,
        "outputMax": 160,
        "smoothingAlpha": 0.5,
        "gain": 1.0,
        "enabled": true,
        "additive": false
      }
    ]
  }
}
```

### Set Mapping for Effect

```
POST /api/v1/audio/mappings/effect?id=5
Content-Type: application/json

{
  "globalEnabled": true,
  "mappings": [
    {
      "source": "BASS",
      "target": "INTENSITY",
      "curve": "SQUARED",
      "inputMin": 0.0,
      "inputMax": 1.0,
      "outputMin": 64,
      "outputMax": 255,
      "smoothingAlpha": 0.3,
      "gain": 1.0,
      "enabled": true,
      "additive": false
    }
  ]
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "effectId": 5,
    "mappingCount": 1,
    "enabled": true,
    "message": "Mapping updated"
  }
}
```

### Delete Mapping for Effect

```
DELETE /api/v1/audio/mappings/effect?id=5
```

### Enable/Disable Mapping

```
POST /api/v1/audio/mappings/enable?id=5
POST /api/v1/audio/mappings/disable?id=5
```

### Get Mapping Statistics

```
GET /api/v1/audio/mappings/stats
```

**Response:**
```json
{
  "success": true,
  "data": {
    "applyCount": 123456,
    "lastApplyMicros": 127,
    "maxApplyMicros": 312,
    "activeEffectsWithMappings": 3,
    "totalMappingsConfigured": 7
  }
}
```

---

## WebSocket Commands

### List Effects with Mappings

```json
{"type": "audio.mapping.list", "requestId": "req_001"}
```

### Get Mapping for Effect

```json
{"type": "audio.mapping.get", "effectId": 5, "requestId": "req_002"}
```

### Set Mapping for Effect

```json
{
  "type": "audio.mapping.set",
  "effectId": 5,
  "config": {
    "globalEnabled": true,
    "mappings": [...]
  },
  "requestId": "req_003"
}
```

### Enable/Disable Mapping

```json
{"type": "audio.mapping.enable", "effectId": 5, "enabled": true, "requestId": "req_004"}
```

### Clear Mapping

```json
{"type": "audio.mapping.clear", "effectId": 5, "requestId": "req_005"}
```

---

## Mapping Parameters

### smoothingAlpha

Controls the IIR filter coefficient for temporal smoothing:

| Value | Effect |
|-------|--------|
| 0.05 | Very smooth (high latency) |
| 0.2 | Moderate smoothing |
| 0.5 | Balanced |
| 0.8 | Fast response |
| 0.95 | Nearly instant |

### gain

Pre-curve multiplier applied to the input value. Values >1.0 amplify, <1.0 attenuate.

### additive

- `false` (default): Replace existing parameter value
- `true`: Add to existing parameter value (allows layering multiple sources)

### inputMin/inputMax

Clips the source value before normalization. Useful for focusing on a specific range of the audio source.

### outputMin/outputMax

Target parameter range. Maps the curved [0,1] value to this output range.

---

## Usage Examples

### Bass-Reactive Brightness

Map bass energy to LED brightness with squared response:

```json
{
  "source": "BASS",
  "target": "BRIGHTNESS",
  "curve": "SQUARED",
  "inputMin": 0.1,
  "inputMax": 0.9,
  "outputMin": 40,
  "outputMax": 160,
  "smoothingAlpha": 0.3,
  "gain": 1.5,
  "enabled": true
}
```

### Beat-Synced Animation Speed

Modulate speed based on beat phase:

```json
{
  "source": "BEAT_PHASE",
  "target": "SPEED",
  "curve": "LINEAR",
  "inputMin": 0.0,
  "inputMax": 1.0,
  "outputMin": 5,
  "outputMax": 35,
  "smoothingAlpha": 0.8,
  "enabled": true
}
```

### Treble-Driven Saturation

Reduce saturation with high treble for a "washed out" effect:

```json
{
  "source": "TREBLE",
  "target": "SATURATION",
  "curve": "INVERTED",
  "inputMin": 0.0,
  "inputMax": 1.0,
  "outputMin": 128,
  "outputMax": 255,
  "smoothingAlpha": 0.4,
  "enabled": true
}
```

---

## Implementation Details

### Source Files

| File | Purpose |
|------|---------|
| `v2/src/audio/contracts/AudioEffectMapping.h` | Enums, structs, registry class |
| `v2/src/audio/contracts/AudioEffectMapping.cpp` | Implementation |
| `v2/src/core/actors/RendererActor.cpp` | Integration (line 758-796) |
| `v2/src/network/WebServer.cpp` | REST/WebSocket handlers |

### Render Pipeline Injection

Mappings are applied in `RendererActor::renderFrame()` after audio context population and before `effect->render()`:

```cpp
#if FEATURE_AUDIO_SYNC
if (AudioMappingRegistry::instance().hasActiveMappings(m_currentEffect)) {
    AudioMappingRegistry::instance().applyMappings(
        m_currentEffect,
        m_lastControlBus,
        m_lastMusicalGrid,
        ctx.audio.available,
        ctx.brightness, ctx.speed, ctx.intensity,
        ctx.saturation, ctx.complexity, ctx.variation, ctx.gHue
    );
}
#endif
```

### Memory Footprint

- **RAM**: ~1.5KB for AudioMappingRegistry (80 effects × ~20 bytes each)
- **Flash**: ~4KB code for mapping logic and API handlers
- **Performance**: <200μs per applyMappings() call at 120 FPS

---

## Related Documentation

- [Audio Stream API](AUDIO_STREAM_API.md) - Binary audio metrics streaming
- [API V1 Reference](../docs/api/API_V1.md) - Full REST API documentation
- [WebSocket Protocol](../docs/api/API_V1.md#websocket-commands) - WebSocket command reference
