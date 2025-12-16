# Beat Detection Diagnostic Guide

## Quick Start

1. **Connect your device** to USB port `/dev/cu.usbmodem1401`

2. **Run the diagnostic**:
   ```bash
   cd AP_SOT
   python3 diagnose_beat_detection.py
   ```

3. **Follow the on-screen prompts** for each test phase:
   - **SILENCE** (30s): No music, complete silence
   - **EDM** (60s): Electronic music with clear 4/4 beat
   - **ACOUSTIC** (60s): Soft guitar or piano music  
   - **ROCK** (60s): Dynamic rock/pop with varied intensity
   - **VOLUME SWEEP** (90s): EDM track, gradually increase volume

## What It Does

The script will:
- Monitor serial output during each phase
- Track energy levels and beat detection
- Analyze VoG confidence ratios
- Calculate optimal threshold values
- Generate specific code recommendations

## Expected Results

After running, you'll see:
1. Recommended energy threshold (likely 5000-10000 instead of 50)
2. Suggested transient detection improvements
3. Explanation of why VoG shows 0.00
4. Specific code changes to fix the issues

## Interpreting Results

- **Energy Threshold**: Should be 3x silence baseline but below 25th percentile of music
- **VoG Ratio**: AGC > raw (ratio < 1.0) is normal except during transients
- **Beat Detection**: Should show 120-160 BPM for EDM, not constant detection

## Troubleshooting

If serial connection fails:
```bash
# Check your port:
ls /dev/cu.usb*

# Run with custom port:
python3 diagnose_beat_detection.py /dev/cu.usbmodem2101
```

The diagnostic creates a timestamped report file with all raw data for later analysis.