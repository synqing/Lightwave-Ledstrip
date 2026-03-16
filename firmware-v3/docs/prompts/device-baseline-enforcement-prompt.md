# Device Baseline Enforcement — CC Agent Prompt

## Context

K1V2 and V1 are showing fundamentally different behaviour on the same effect (K1 Waveform 0x1302):

| Metric | V1 | K1V2 |
|--------|-----|------|
| mean_brightness | 0.950 | 0.233 |
| flicker | 0.107 | 0.753 |
| near_black_leaks | 0 | 186 |
| composite | 0.654 | 0.211 |

The two build environments (`esp32dev_audio_esv11_32khz` for V1, `esp32dev_audio_esv11_k1v2_32khz` for K1V2) share identical software parameters — same default brightness, silence gate, smoothing coefficients, frame rate, and audio sample rate. The only build differences are GPIO pin assignments and K1V2-specific hardware flags.

The divergence is almost certainly caused by **stale NVS state** (persisted settings from prior testing sessions) and/or **firmware version mismatch** (devices not running the same build).

Your task is to establish a verified, clean baseline on both devices.

## Step 1: Clean Build (MANDATORY)

Build both firmware images from the SAME source tree, at the SAME commit:

```bash
cd firmware-v3
git stash  # If any uncommitted changes
git log --oneline -1  # Record the commit hash — BOTH devices must run this exact commit

pio run -e esp32dev_audio_esv11_32khz 2>&1 | tail -5
pio run -e esp32dev_audio_esv11_k1v2_32khz 2>&1 | tail -5
```

**Pass:** Both builds complete with zero errors. Record the commit hash.
**Fail:** Any build error. Fix before proceeding.

## Step 2: Flash Both Devices

Flash each device with its correct firmware:

```bash
# V1 device
pio run -e esp32dev_audio_esv11_32khz -t upload

# K1V2 device
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload
```

**Verify flash success:** Check serial output on each device for boot messages. Both should show the same firmware version string and commit hash.

## Step 3: NVS Erase and Reset (MANDATORY)

After flashing, erase NVS on BOTH devices to eliminate stale persisted state:

```bash
# V1 device
pio run -e esp32dev_audio_esv11_32khz -t erase

# K1V2 device
pio run -e esp32dev_audio_esv11_k1v2_32khz -t erase
```

**Then reflash** (erase wipes everything including firmware):

```bash
pio run -e esp32dev_audio_esv11_32khz -t upload
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload
```

**Alternative if full NVS erase is disruptive:** Use serial commands to reset individual namespaces:

```json
{"cmd": "setEdgeMixer", "mode": 0, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0}
{"cmd": "saveEdgeMixer"}
{"cmd": "setBrightness", "value": 96}
{"cmd": "setEffect", "id": "0x1302"}
```

The full erase + reflash is preferred — it eliminates ALL stale state, not just EdgeMixer.

## Step 4: Post-Flash Verification

After both devices are running fresh firmware with clean NVS, verify identical configuration:

### 4a: Firmware Version Match

Query both devices via serial or WebSocket:

```json
{"cmd": "getStatus"}
```

Both must report the same firmware version and build timestamp. If they differ, one device didn't flash correctly — reflash.

### 4b: EdgeMixer State Match

```json
{"cmd": "getEdgeMixer"}
```

Both must report: `mode=0 (MIRROR), spread=30, strength=255 (factory default), spatial=0, temporal=0`.

If any field differs, the NVS erase didn't take — erase and reflash again.

### 4c: Brightness Match

Both devices must report `brightness=96` (DEFAULT_BRIGHTNESS from RendererActor.h). If either differs, NVS is still carrying stale state.

### 4d: Effect Match

Set both to K1 Waveform:

```json
{"cmd": "setEffect", "id": "0x1302"}
```

## Step 5: Baseline Capture

With both devices in verified identical state, capture baselines:

**Configuration for baseline:**
- Effect: 0x1302 (K1 Waveform)
- EdgeMixer: mode=0 (MIRROR)
- Brightness: 96 (default)
- Audio: Same music source, same volume, same start point

**Capture:** 900 frames on each device.

### Baseline Acceptance Criteria

The two devices should now produce SIMILAR metrics (not identical — hardware differences in GPIO, LED binning, microphone sensitivity will cause natural variance):

| Metric | Acceptable Cross-Device Delta | Hard Fail |
|--------|-------------------------------|-----------|
| mean_brightness | Within 0.20 of each other | Delta > 0.40 |
| flicker | Within 0.15 of each other | Delta > 0.30 |
| VMCR | Within 0.15 of each other | Delta > 0.30 |
| energy_corr | Within 0.15 of each other | Delta > 0.30 |
| composite | Within 0.15 of each other | Delta > 0.30 |
| near_black_leaks | Both zero | Either > 0 |

**If baseline PASSES:** The divergence was stale NVS/firmware mismatch. Proceed with EdgeMixer validation from the test plan.

**If baseline FAILS:** The divergence is hardware-level. Report the specific failing metrics and proceed to Step 6.

## Step 6: Hardware Divergence Investigation (only if Step 5 fails)

If the devices still diverge after clean flash + NVS erase, investigate hardware causes:

### 6a: Audio Path Check

K1V2 swaps I2S pins (BCLK=13, DOUT=14, LRCL=11) vs V1 (BCLK=14, DOUT=13, LRCL=12). Check audio is actually reaching the DSP:

```json
{"cmd": "getAudioStatus"}
```

Compare RMS, flux, and beat detection on both devices with the same music source. If K1V2 shows significantly lower RMS or no beat detection, the I2S pin configuration may be wrong.

### 6b: LED Output Check

K1V2 uses GPIO 6/7 for strips vs V1's GPIO 4/5. If K1V2 LEDs appear dim:

- Check RMT channel assignment for GPIO 6/7
- Check if ESP32-S3 GPIO 6/7 have different drive strength defaults
- Test with a simple solid-colour pattern (no audio dependency) to isolate LED output from audio

### 6c: Silence Gate Behaviour

If K1V2 is cycling between frozen and active states, the silence gate may be triggering incorrectly due to audio issues:

- Monitor `isSilent` state via serial debug output
- Check if the 8000ms silence hysteresis is cycling (audio intermittently detected then lost)
- If silence gate is oscillating, the I2S audio input is unreliable on K1V2

## What You Must NOT Do

1. **Do NOT modify AudioActor.cpp.** Coefficients are correct.
2. **Do NOT modify EdgeMixer.h.** The RGB matrix refactor is complete and validated.
3. **Do NOT modify any effect source files.**
4. **Do NOT change build flags or platformio.ini** unless you identify a specific GPIO configuration error AND get explicit approval.
5. **Do NOT skip the NVS erase.** Stale state is the most likely cause — eliminate it first.

## Deliverables

1. Commit hash both devices are running
2. Post-flash `getStatus` output from both devices (firmware version match)
3. Post-flash `getEdgeMixer` output from both devices (config match)
4. Baseline capture results (900 frames per device)
5. Cross-device delta table with pass/fail assessment
6. If hardware investigation needed: audio status comparison, silence gate log, LED solid-colour test results
