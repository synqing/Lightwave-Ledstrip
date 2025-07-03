# Audio Sync Integration Test Plan

## Overview
This document outlines how to test the audio sync integration with LightwaveOS.

## Prerequisites
1. ESP32-S3 device flashed with the experimental branch firmware
2. WiFi credentials configured in `network_config.h`
3. A sample JSON audio data file (from VP_DECODER processing)
4. An MP3 file of the same song

## Test Steps

### 1. Device Setup
1. Power on the ESP32-S3 device
2. Monitor serial output to confirm WiFi connection
3. Note the device IP address

### 2. Access Web Portal
1. Open browser and navigate to `http://[device-ip]/`
2. You should see the audio sync control interface

### 3. Upload JSON Data
1. Click "Choose File" and select your JSON audio data file
2. Click "Upload JSON" 
3. Monitor progress bar for upload completion
4. Check serial output for successful load confirmation

### 4. Load MP3 in Browser
1. Use the MP3 file selector to choose the corresponding MP3
2. The browser will load the audio file

### 5. Start Synchronized Playback
1. Click "Start Sync"
2. The browser will:
   - Start MP3 playback
   - Send sync command to device with timestamp
   - Begin network latency monitoring

### 6. Observe LED Visualization
1. LEDs should react to the music:
   - Bass frequencies trigger pulses
   - Spectrum effect shows frequency distribution
   - Energy flow creates moving particles
2. Visualization should be synchronized with audio

### 7. Test Audio Effects
1. Switch between effects using the web interface:
   - Bass Reactive: Pulses with bass beats
   - Spectrum: Shows frequency spectrum
   - Energy Flow: Particles move with music energy

### 8. Monitor Performance
1. Check FPS counter in web interface
2. Monitor serial output for any sync issues
3. Verify smooth playback without stuttering

## Expected Results
- JSON upload completes without errors
- Audio playback starts synchronized with LED effects
- Effects respond accurately to music
- Network latency compensation keeps sync stable
- FPS remains at or near 176 FPS

## Troubleshooting
- If upload fails: Check file size and WiFi stability
- If sync drifts: Refresh page and restart
- If effects don't show: Verify FEATURE_AUDIO_SYNC is enabled
- If no audio: Check browser audio permissions

## Sample JSON Structure
The JSON file should contain:
```json
{
  "global_metrics": {
    "duration_ms": 180000,
    "bpm": 128
  },
  "layers": {
    "frequency": {
      "bass": [
        {"time_ms": 0, "intensity": 0.1},
        {"time_ms": 100, "intensity": 0.8}
      ]
    },
    "rhythm": {
      "beat_grid_ms": [0, 468.75, 937.5, ...]
    }
  }
}
```