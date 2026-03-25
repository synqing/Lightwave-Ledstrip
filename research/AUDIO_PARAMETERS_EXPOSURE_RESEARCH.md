---
abstract: "Research findings on how embedded audio-reactive LED projects (WLED, Emotiscope, LightwaveOS) expose runtime-tunable parameters to users. Documents parameter naming conventions, ranges, storage mechanisms, and recommended practices for K1 onset/silence system."
---

# Audio-Reactive Parameter Exposure Research

**Date:** 2026-03-25
**Focus:** How embedded ESP32 LED projects expose audio parameters to users via web UI, REST API, and serial CLI.

---

## Executive Summary

This research surveyed three major embedded audio-reactive LED projects and K1's existing infrastructure to identify best practices for exposing onset and silence detection parameters. Key findings:

1. **WLED Sound Reactive** is the industry standard for parameter exposure, offering two core tunable parameters: **Squelch** (noise gate threshold) and **Gain** (input amplification).
2. **K1 already has the infrastructure** to expose parameters via REST API (`/api/v1/audio/parameters`), WebSocket commands, and serial CLI.
3. **Parameter naming matters**: User-friendly names (Squelch, Sensitivity, Threshold) outperform DSP-internal names (agcTargetRms, silenceThreshold).
4. **Immediate effect is critical**: Users expect parameter changes to affect the system **within the current frame or next frame**, not after a DSP reset.
5. **Persistence is mandatory**: All parameters must be stored in NVS (Non-Volatile Storage) and survive power cycles.

---

## Comparative Analysis

### 1. WLED Sound Reactive

**Project:** [athom-tech/Sound-Reactive-WLED](https://github.com/athom-tech/Sound-Reactive-WLED)
**Platforms:** ESP8266, ESP32
**Audio Backend:** FFT + AGC

#### Exposed Parameters

| Parameter | Type | Range | Default | Purpose |
|-----------|------|-------|---------|---------|
| **Squelch** | uint8 | 0–255 | ~20–50 (env. dependent) | Silence gate threshold. Prevents LEDs from flashing in quiet/noisy backgrounds |
| **Gain** | uint8 | 1–255 | 40–60 | Input amplification. Range: −20 dB (gain=1) to +16 dB (gain=255) |
| **AGC Mode** | enum | Off / Normal / Vivid / Lazy | Normal | Automatic gain control preset. Vivid = faster attack, Lazy = slower, Off = manual only |
| **Input Source** | enum | Analog GPIO / I2S Digital / Line-In | (varies) | Microphone type or audio line input |
| **Sync Settings** | object | (varies) | (disabled) | Send/receive audio between networked WLED devices |

#### User-Facing Workflow (WLED)

From [WLED Sound Settings documentation](https://mm.kno.wled.ge/soundreactive/Sound-Settings/):

1. User enters "Sound Settings" web UI (musical note icon)
2. Sees **Squelch** and **Gain** sliders
3. In quiet room, increases Squelch incrementally until LEDs stop flashing
4. Sets Gain to 40, plays typical music, adjusts Gain until LEDs respond appropriately
5. Optionally selects AGC preset (Normal/Vivid/Lazy) for automatic amplification

#### Storage & Persistence

- **Location:** Flash memory (WLED's config storage)
- **Exposure:** Web UI sliders, MQTT topics (if enabled)
- **Effect timing:** Immediate (applied to next FFT frame, ~50 ms typical)

#### Key Insight

WLED's success comes from **simplicity**: only two user controls (Squelch + Gain) that directly map to intuitive goals ("stop false triggers" + "respond to my music"). AGC handles complexity internally.

---

### 2. Emotiscope (Lixie Labs)

**Project:** [Emotiscope](https://www.tindie.com/products/lixielabs/emotiscope-a-music-visualizer-from-the-future/)
**Platform:** ESP32-S3
**Audio Backend:** 128 parallel Goertzel + spectral flux

#### Exposed Parameters

Emotiscope deliberately **avoids low-level audio parameter exposure**. Instead:

| Control Type | Options | Purpose |
|--------------|---------|---------|
| **Visualization Modes** | 8 presets (analog, spectrum, octave, metronome, spectronome, hype, bloom, neutral) | Choose visualization style; DSP tuning is preset-specific |
| **UI Sliders** | Brightness, Colour, Reaction Speed, (others) | Control visual properties, not audio processing |
| **Touch Buttons** | Mode cycling, power toggle | Physical interface to top-mounted controls |

#### Storage & Persistence

- **Location:** NVS (Non-Volatile Storage)
- **Exposure:** Web app (no downloads; visit `app.emotiscope.rocks` on same WiFi)
- **Persistence:** Automatic per-mode saves

#### Key Insight

Emotiscope's philosophy is **"no tuning required"**. Users pick a visualization style, then adjust visual parameters only. The device infers audio sensitivity from selected mode. This trades off granular control for simplicity.

---

### 3. LightwaveOS (K1)

**Project:** Firmware-v3
**Platform:** ESP32-S3
**Audio Backends:** ESV11 (Goertzel-based, production) | PipelineCore (FFT-based, broken)

#### Current Audio Parameter Infrastructure

K1 **already exposes a full audio parameter system** via REST API:

**REST Endpoints (active):**
- `GET /api/v1/audio/parameters` — read all DSP tuning parameters
- `POST /api/v1/audio/parameters` — batch update parameters
- `PATCH /api/v1/audio/parameters` — partial update (single field)
- `GET /api/v1/audio/state` — runtime state (RMS, flux, silence status)

**WebSocket Commands:**
- `debug.audio.get` — fetch audio parameters
- `debug.audio.set` — update audio parameters

**Serial CLI:**
- `tempo` — show BPM, confidence, phase, lock state (read-only debug)
- Custom multi-char command support exists (extracted into `SerialCLI.cpp`)

#### Existing Tunable Parameters (ESV11 Backend)

K1's ESV11 backend currently exposes only **contract parameters** (beat tracking constraints):

```json
{
  "contract": {
    "bpmMin": 60.0,
    "bpmMax": 200.0,
    "bpmTau": 4.0,
    "confidenceTau": 8.0,
    "phaseCorrectionGain": 0.1,
    "barCorrectionGain": 0.05,
    "beatsPerBar": 4,
    "beatUnit": 4
  }
}
```

**Notably absent from REST API:**
- Silence gate threshold (currently hardcoded)
- Onset detection sensitivity (no exposure)
- Microphone gain (hardware-fixed or not configurable)
- Per-band noise floors (PipelineCore only, not ESV11)

#### Storage & Persistence

K1 uses **NVS (Non-Volatile Storage)** for all persistent configuration:
- Located: `src/core/persistence/` directory
- Pattern: `ZoneConfigManager`, `ConfigManager` classes for state serialisation
- Restore on boot: automatic from NVS

#### Effect Timing

Parameters update **immediately** for beat tracking (next hop, ~256 samples @ 32 kHz ≈ 8 ms).
For onset/silence system: TBD (depends on implementation).

---

## Industry Best Practices

### 1. Parameter Naming (User vs. DSP)

**User-friendly names (expose these):**
- Squelch, Sensitivity, Threshold, Loudness Gate
- Gain, Amplification, Input Level
- Reaction Speed, Attack Time, Decay Time

**DSP-internal names (hide these from users):**
- agcTargetRms, noiseFloorMin, silenceThreshold
- bandAttack, bandRelease, gateStartFactor
- dcAlpha, fluxScale

**Why it matters:** Users think in terms of **goals** ("silence is too quiet") not **mechanisms** ("raise the agcTargetRms"). WLED's success is partly naming discipline.

### 2. Ranges & Defaults

**Recommended approach:**
- **Squelch / Silence Threshold:** 0–100 (normalized), Default 30–40
  - Meaning: 0 = no silence gate, 100 = require complete silence
  - Backend maps to internal threshold (e.g., RMS < 0.005)

- **Sensitivity / Gain:** 0–100 (normalized), Default 50
  - Meaning: 0 = minimum, 100 = maximum input amplification
  - Backend maps to DSP gain factor (e.g., 0.5–8.0x)

- **Reaction Speed / Attack Time:** 0–100 (normalized), Default 50
  - Meaning: 0 = no smoothing (jittery), 100 = maximum smoothing (sluggish)
  - Backend maps to exponential smoothing factor (α, e.g., 0.1–0.9)

**Why normalize:** Users don't care about absolute dB or time constants. A 0–100 scale with sensible defaults is intuitive across all environments.

### 3. Parameter Change Effect

**Best practice: Immediate effect, no reset required**
- Parameter change applies to **next processing frame** (8–50 ms depending on hop size)
- DSP state (RMS history, agc state, beat tracker) is **NOT reset**
- This allows live parameter tweaking (e.g., user raises squelch while music plays)

**Why:** Resetting state on every parameter change breaks the illusion of "real-time control." WLED, Emotiscope, and K1 all use immediate-effect architecture.

### 4. Storage Tier

**Tier 0 (Runtime only):**
- Changes stay in RAM only
- Lost on power cycle
- API: `POST /api/v1/audio/parameters` without `"save": true`

**Tier 1 (Persistent, default boot):**
- Changes written to NVS
- Survive power cycles and reboots
- API: `POST /api/v1/audio/parameters` with `"save": true`
- OR: Serial command `audio save` (proposed)

**K1's existing pattern:**
- `EdgeMixer.cpp` has `saveEdgeMixer()` → NVS write
- REST endpoint `/api/v1/edgeMixer` accepts `"save": true` flag
- **Recommended:** Follow same pattern for audio parameters

---

## Recommended Parameter Set for K1 Onset/Silence System

Based on research and K1's existing infrastructure, **propose three user-facing parameters**:

### Core Parameters (REST API, WebSocket, Serial)

| Parameter Name | Type | Range | Default | Unit | Storage | Purpose |
|---|---|---|---|---|---|---|
| **silenceThreshold** | uint8 | 0–100 | 30 | % of max RMS | NVS | Noise gate: only fire onset when RMS exceeds this |
| **onsetSensitivity** | uint8 | 0–100 | 60 | % detection strength | NVS | Onset detection confidence threshold. Higher = fewer false positives |
| **silenceHysteresis** | uint16 | 0–1000 | 200 | milliseconds | NVS | Time silence must persist before "all quiet" signal. Prevents jitter. |

#### Backend Mapping (ESV11)

| User Parameter | Backend Variable | Mapping Formula |
|---|---|---|
| silenceThreshold (0–100) | rmsThreshold | rmsThreshold = (silenceThreshold / 100.0) * maxRMS |
| onsetSensitivity (0–100) | onsetConfThreshold | onsetConfThreshold = 0.3 + (onsetSensitivity / 100.0) * 0.4 (range 0.3–0.7) |
| silenceHysteresis (0–1000 ms) | silenceHoldMs | silenceHoldMs = silenceHysteresis (direct map) |

### Secondary Parameters (Debug/Power-Users, REST API only)

If desired, expose these via `/api/v1/audio/parameters` for advanced users:

| Parameter Name | Type | Range | Default | Purpose |
|---|---|---|---|---|
| **onsetSmoothing** | uint8 | 0–100 | 40 | Exponential smoothing on onset detection. Higher = less jittery |
| **silenceAttackMs** | uint16 | 10–500 | 100 | Ramp time to close silence gate when RMS drops |
| **silenceReleaseMs** | uint16 | 10–500 | 300 | Ramp time to open silence gate when RMS rises |

---

## API Design Proposal

### REST Endpoint: `POST /api/v1/audio/onset-parameters`

**Purpose:** Set onset detection and silence gate parameters.

**Request Body:**
```json
{
  "silenceThreshold": 30,
  "onsetSensitivity": 60,
  "silenceHysteresis": 200,
  "save": true
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "updated": ["silenceThreshold", "onsetSensitivity", "silenceHysteresis"],
    "applied": {
      "silenceThreshold": 30,
      "onsetSensitivity": 60,
      "silenceHysteresis": 200,
      "rmsThreshold": 0.028,
      "onsetConfThreshold": 0.54
    }
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

**Error Responses:**
- `400 INVALID_JSON` — malformed request
- `400 OUT_OF_RANGE` — parameter outside valid range
- `503 SYSTEM_NOT_READY` — audio actor not initialized

### REST Endpoint: `GET /api/v1/audio/onset-parameters`

**Purpose:** Fetch current onset/silence parameters.

**Response:**
```json
{
  "success": true,
  "data": {
    "silenceThreshold": 30,
    "onsetSensitivity": 60,
    "silenceHysteresis": 200,
    "rmsThreshold": 0.028,
    "onsetConfThreshold": 0.54
  },
  "timestamp": 123456789,
  "version": "1.0.0"
}
```

### WebSocket Commands

**Get:**
```json
{"type": "onset.parameters.get"}
```

**Set (immediate, no save):**
```json
{"type": "onset.parameters.set", "silenceThreshold": 35, "onsetSensitivity": 65}
```

**Save to NVS:**
```json
{"type": "onset.parameters.save"}
```

### Serial CLI

**Get (proposed):**
```
> audio onset get
silenceThreshold: 30
onsetSensitivity: 60
silenceHysteresis: 200
rmsThreshold: 0.028
onsetConfThreshold: 0.54
```

**Set (proposed):**
```
> audio onset set 35 65 200
Updated: silenceThreshold=35, onsetSensitivity=65, silenceHysteresis=200
> audio onset save
Saved to NVS
```

---

## Implementation Checklist

- [ ] Define `OnsetParameters` struct in `src/audio/contracts/OnsetParameters.h`
- [ ] Add NVS serialization to `OnsetParameters` (see `K1_PERSISTENCE_IMPLEMENTATION_GUIDE.md`)
- [ ] Implement `GET /api/v1/audio/onset-parameters` handler in `WebServer.cpp`
- [ ] Implement `POST /api/v1/audio/onset-parameters` handler (with `"save": true` support)
- [ ] Add WebSocket commands: `onset.parameters.get`, `onset.parameters.set`, `onset.parameters.save`
- [ ] Add serial CLI commands: `audio onset get/set/save`
- [ ] Integrate parameter reads into onset detection logic (OnsetDetector class)
- [ ] Test parameter update latency (should be < 16 ms for 120 FPS target)
- [ ] Document in API reference (update `docs/api/api-v1.md`)
- [ ] Add unit tests for parameter bounds checking and serialization

---

## References

### WLED Sound Reactive
- [WLED Sound Reactive GitHub](https://github.com/athom-tech/Sound-Reactive-WLED)
- [Squelch and Gain Documentation](https://mm.kno.wled.ge/WLEDSR/Squelch-and-Gain/)
- [Sound Settings Reference](https://mm.kno.wled.ge/soundreactive/Sound-Settings/)
- [Audio Reactive Overview](https://kno.wled.ge/advanced/audio-reactive/)

### Emotiscope
- [Emotiscope Product Page](https://www.tindie.com/products/lixielabs/emotiscope-a-music-visualizer-from-the-future/)
- [Hackster.io Feature](https://www.hackster.io/news/lixie-labs-emotiscope-is-a-powerful-bridge-between-sight-and-sound-b97e93c4e92d)

### LightwaveOS K1
- `firmware-v3/docs/api/api-v1.md` — Full REST API reference (§ Audio Endpoints)
- `firmware-v3/src/serial/SerialCLI.cpp` — Serial command dispatch
- `firmware-v3/src/core/persistence/` — NVS serialization patterns
- `firmware-v3/docs/api/CLAUDE.md` — API implementation notes

### ESP32 Audio & Persistence
- [ESP32 Preferences Library (NVS Guide)](https://zbotic.in/esp32-preferences-library-non-volatile-storage-nvs-guide/)
- [Espressif Microphone Design Guidelines](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/Espressif_Microphone_Design_Guidelines.html)
- [ESP32 I2S Audio Guide](https://dronebotworkshop.com/esp32-i2s/)

---

## Document Changelog

| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:search | Initial research compilation. WLED squelch/gain, Emotiscope philosophy, K1 existing infrastructure, parameter naming best practices, API design proposal |

