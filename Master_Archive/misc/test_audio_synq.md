# Audio Synq Test Plan

## Summary of Changes

1. **Renamed AudioSync to AudioSynq** throughout the codebase
2. **Implemented 10 LGP audio-reactive effects**:
   - LGP Frequency Collision
   - LGP Beat Interference
   - LGP Spectral Morphing
   - LGP Audio Quantum Collapse
   - LGP Rhythm Waves
   - LGP Envelope Interference
   - LGP Kick Shockwave
   - LGP FFT Color Map
   - LGP Harmonic Resonance
   - LGP Stereo Phase Pattern

3. **Added I2S Microphone Support** (SPH0645):
   - Pin configuration: BCLK=GPIO16, DOUT=GPIO10, LRCL=GPIO4
   - Real-time audio capture and analysis
   - Beat detection
   - Frequency band analysis (bass/mid/high)
   - Synthetic FFT generation

4. **Added Encoder Controls** (Encoder 7):
   - Source selection (VP_DECODER / I2S Mic)
   - Beat threshold adjustment
   - Sync offset control
   - Visual feedback via encoder LED

## Testing Steps

### 1. Build Test
```bash
pio run -e esp32dev
```

### 2. VP_DECODER Mode Test
- Upload an audio JSON file via web interface
- Start playback via WebSocket command
- Verify audio-reactive effects respond to data

### 3. I2S Microphone Test
- Send WebSocket command: `{"cmd": "audio", "subCommand": "startMic"}`
- Play music near the device
- Verify real-time response in effects

### 4. Encoder Control Test
- Press button 7 to cycle through audio parameters
- Turn encoder 7 to adjust selected parameter
- Verify LED color changes indicate mode

## WebSocket Commands

```javascript
// Start microphone
ws.send(JSON.stringify({
    cmd: "audio",
    subCommand: "startMic"
}));

// Stop microphone
ws.send(JSON.stringify({
    cmd: "audio",
    subCommand: "stopMic"
}));

// Switch to VP_DECODER
ws.send(JSON.stringify({
    cmd: "audio",
    subCommand: "setSource",
    useMicrophone: false
}));

// Get status
ws.send(JSON.stringify({
    cmd: "audio",
    subCommand: "getStatus"
}));
```