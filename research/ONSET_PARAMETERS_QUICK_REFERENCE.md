---
abstract: "Quick reference for runtime-tunable onset/silence parameters recommended for K1. Three core user-facing parameters (silenceThreshold, onsetSensitivity, silenceHysteresis) with normalized ranges, defaults, and REST/WebSocket/serial exposure patterns."
---

# K1 Onset Parameters — Quick Reference

**Date:** 2026-03-25
**Status:** Recommended design (awaiting implementation)

---

## Three Core Parameters

### 1. Silence Threshold (silenceThreshold)

| Property | Value |
|---|---|
| **Type** | uint8 |
| **Range** | 0–100 (%) |
| **Default** | 30 |
| **User Meaning** | "How quiet is silent?" — 0 = always active, 100 = require complete silence |
| **Backend Maps To** | `rmsThreshold = (value / 100.0) * maxRMS` |
| **Typical Mapping** | value=30 → rmsThreshold ≈ 0.025 RMS |
| **Storage** | NVS (persistent across power cycles) |
| **Effect Latency** | Immediate (next hop, ~8 ms) |

**User workflow:**
1. In quiet room, raise silenceThreshold until LEDs/effects stop reacting to background noise
2. Do NOT require perfect silence; aim for "normal room tone is ignored"

---

### 2. Onset Sensitivity (onsetSensitivity)

| Property | Value |
|---|---|
| **Type** | uint8 |
| **Range** | 0–100 (%) |
| **Default** | 60 |
| **User Meaning** | "How strong must a beat hit to register?" — 0 = ignore weak beats, 100 = ultra-sensitive |
| **Backend Maps To** | `onsetConfThreshold = 0.3 + (value / 100.0) * 0.4` (range 0.3–0.7) |
| **Typical Mapping** | value=60 → onsetConfThreshold ≈ 0.54 |
| **Storage** | NVS (persistent across power cycles) |
| **Effect Latency** | Immediate (next hop, ~8 ms) |

**User workflow:**
1. During music, adjust onsetSensitivity so that drum hits register but hi-hats do not
2. Lower = fewer, more confident detections; Higher = more, including false positives
3. Sweet spot is usually 50–70 for typical music

---

### 3. Silence Hysteresis (silenceHysteresis)

| Property | Value |
|---|---|
| **Type** | uint16 |
| **Range** | 0–1000 (milliseconds) |
| **Default** | 200 |
| **User Meaning** | "How long must it be quiet before declaring 'silence'?" |
| **Backend Maps To** | Direct: `silenceHoldMs = silenceHysteresis` |
| **Storage** | NVS (persistent across power cycles) |
| **Effect Latency** | Immediate (applies to next silence gate evaluation) |

**User workflow:**
1. Prevents rapid flickering when music momentarily dips
2. Default 200 ms is good for most music (catches breath breaks without over-delaying)
3. Increase to 500 ms for very choppy audio or speech
4. Decrease to 100 ms for minimal latency (may flicker on hi-hats)

---

## REST API

### Get Current Parameters
```bash
curl http://192.168.4.1/api/v1/audio/onset-parameters
```

**Response:**
```json
{
  "success": true,
  "data": {
    "silenceThreshold": 30,
    "onsetSensitivity": 60,
    "silenceHysteresis": 200,
    "rmsThreshold": 0.025,
    "onsetConfThreshold": 0.54
  },
  "timestamp": 1711353600,
  "version": "1.0.0"
}
```

### Set Parameters (immediate, no save)
```bash
curl -X POST http://192.168.4.1/api/v1/audio/onset-parameters \
  -H "Content-Type: application/json" \
  -d '{"silenceThreshold": 35, "onsetSensitivity": 65}'
```

### Set & Save to NVS (survives reboot)
```bash
curl -X POST http://192.168.4.1/api/v1/audio/onset-parameters \
  -H "Content-Type: application/json" \
  -d '{"silenceThreshold": 35, "onsetSensitivity": 65, "save": true}'
```

---

## WebSocket Commands

### Get Parameters
```json
{"type": "onset.parameters.get"}
```

### Set Parameters (immediate)
```json
{"type": "onset.parameters.set", "silenceThreshold": 35}
```

### Save to NVS
```json
{"type": "onset.parameters.save"}
```

---

## Serial CLI (Proposed)

### Get Current Values
```
> audio onset get
silenceThreshold: 30
onsetSensitivity: 60
silenceHysteresis: 200
rmsThreshold: 0.025
onsetConfThreshold: 0.54
```

### Set All Three
```
> audio onset set 35 65 200
Updated: silenceThreshold=35, onsetSensitivity=65, silenceHysteresis=200
```

### Save to NVS
```
> audio onset save
Saved to NVS
```

---

## Implementation Files

**Expected locations (when implemented):**

- `src/audio/contracts/OnsetParameters.h` — struct definition & NVS serialization
- `src/network/handlers/AudioOnsetHandler.cpp` — REST endpoint handlers
- `src/serial/commands/AudioOnsetCommand.cpp` — serial CLI commands
- `src/audio/OnsetDetector.cpp` — integration with detection logic
- `docs/api/api-v1.md` — REST API documentation (add § Audio Onset Endpoints)

---

## Calibration Guide for Users

### Step 1: Silence Threshold

1. Turn off music, silence the environment
2. Serial: `audio onset get` to see current RMS baseline
3. Set silenceThreshold to 50
4. Play normal music at typical volume
5. If effects trigger during quiet passages, raise silenceThreshold to 40, 45, 50
6. Stop when background noise no longer triggers effects, but music does

**Target:** silenceThreshold = 25–40 for most rooms

### Step 2: Onset Sensitivity

1. Play a song with clear drum hits
2. Set onsetSensitivity to 50 (neutral)
3. Watch effect response to kicks and snare hits
4. If hi-hats trigger unwanted reactions, lower onsetSensitivity (45, 40)
5. If kicks feel delayed or weak, raise onsetSensitivity (65, 70)

**Target:** onsetSensitivity = 55–70 for typical music

### Step 3: Silence Hysteresis

1. Play a song with instrumental breaks (pauses between phrases)
2. Watch how long the effect "remembers" sound during pauses
3. Default 200 ms should handle most breath breaks
4. If effects flicker too much during sustained quiet, raise to 300–500 ms
5. If you want instant response to silence, lower to 100 ms (may flicker on hi-hats)

**Target:** silenceHysteresis = 150–250 for most music

---

## Comparison to WLED

| Aspect | WLED | K1 (Proposed) |
|---|---|---|
| **Silence Gate** | "Squelch" (0–255) | "silenceThreshold" (0–100 normalized) |
| **Gain** | "Gain" (1–255, −20 to +16 dB) | Not exposed (hardware-fixed) |
| **AGC** | "AGC Mode" (Off/Normal/Vivid/Lazy) | Not exposed (may add as future work) |
| **Advanced Params** | Per-band gains, noise floors | Available via REST API, not recommended for UI |
| **Storage** | EEPROM / Flash | NVS (same as K1) |
| **Effect Latency** | ~50 ms (FFT frame) | ~8 ms (hop-based, faster) |

---

## Notes for Implementation

1. **Normalization (0–100) is intentional** — hides DSP complexity from users, works across different microphone gains and environments
2. **Defaults should match environment assumptions** — may need field tuning for K1 v2 dual-strip hardware
3. **No reset on parameter change** — DSP state (silence gate memory, beat tracker) persists
4. **Persistence is automatic** — `"save": true` writes to NVS; next boot loads saved values
5. **Broadcast to WebSocket clients** — if parameters are changed via one client, all others should see the update

---

## Document Changelog

| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:search | Initial quick reference. Three core parameters, ranges, defaults, REST/WebSocket/serial APIs, calibration guide |

