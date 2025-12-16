# Genesis Engine v3.0 - Audio Analysis for LED Visualization

The Genesis Engine analyzes MP3/audio files and generates Genesis Maps - JSON files containing pre-processed musical data for real-time LED visualization.

## Installation

```bash
# Install required Python packages
pip install librosa numpy scipy matplotlib

# For advanced beat detection (optional but recommended)
pip install madmom

# For source separation (optional)
pip install demucs torch torchaudio
```

## Basic Usage

### Simple Analysis
```bash
python genesis_engine_v3.py input.mp3 -o output.json
```

### Advanced Analysis with Visualization
```bash
python genesis_engine_advanced.py input.mp3 --visualize
```

### With Genre-Specific Preset
```bash
python genesis_engine_advanced.py input.mp3 --config config_presets.json
```

## Fine-Tuning Parameters

### Frequency Bands
Adjust the frequency ranges for different music styles:
- **EDM**: Lower bass cutoff (20-150Hz) for sub-bass emphasis
- **Rock**: Extended mid-range (300-3000Hz) for guitars
- **Classical**: Balanced ranges with extended highs

### Intensity Boosts
Control the relative strength of each band:
- `bass_boost`: 1.0-2.0 (higher for bass-heavy music)
- `mid_boost`: 0.8-1.5 (higher for vocal/instrument focus)
- `high_boost`: 0.5-1.2 (lower for smooth, higher for crisp)

### Smoothing
- `smoothing_window`: 5-21 (odd numbers)
  - Lower = more reactive, sharper changes
  - Higher = smoother, more flowing

### Analysis Resolution
- `hop_length`: 256-2048
  - Lower = higher time resolution, larger files
  - Higher = lower resolution, smaller files

## Output Format

The Genesis Map contains:

```json
{
  "metadata": {
    "filename": "song.mp3",
    "analysis_engine": "Genesis_Engine_v3.0",
    "timestamp_utc": "2024-01-01T00:00:00Z",
    "version": "v3.0"
  },
  "global_metrics": {
    "duration_ms": 180000,
    "bpm": 128,
    "dynamic_range": 0.8
  },
  "layers": {
    "rhythm": {
      "beat_grid_ms": [0, 468, 937, ...],
      "beat_strengths": [1.0, 0.7, 0.8, ...]
    },
    "frequency": {
      "bass": [{"time_ms": 0, "intensity": 0.5}, ...],
      "mids": [...],
      "highs": [...]
    }
  }
}
```

## Advanced Features

### Harmonic-Percussive Separation
Separates tonal and rhythmic elements for better analysis:
```bash
python genesis_engine_advanced.py input.mp3 --no-harmonic  # Disable
```

### Onset Detection
Detects note attacks and transients for reactive effects.

### Dynamic Range Analysis
Measures loudness variations for adaptive brightness.

### Spectral Features
- **Centroid**: Brightness/timbre
- **Rolloff**: High frequency content
- **ZCR**: Noisiness/texture

## Tips for Best Results

1. **High-Quality Audio**: Use 320kbps MP3 or better
2. **Genre Presets**: Start with a preset and fine-tune
3. **Visual Check**: Use `--visualize` to verify analysis
4. **File Size**: Expect 100-500KB per minute of audio

## Troubleshooting

### Large File Sizes
- Increase `hop_length` (try 1024 or 2048)
- Reduce sampling interval in the code

### Missing Beats
- Lower `onset_threshold` (try 0.2-0.3)
- Ensure percussive elements are clear in mix

### Over-Reactive Effects
- Increase `smoothing_window`
- Reduce boost values

### Under-Reactive Effects  
- Decrease `smoothing_window`
- Increase boost values
- Check frequency ranges match your content

## Integration with LightwaveOS

1. Generate Genesis Map:
   ```bash
   python genesis_engine_advanced.py song.mp3 -o song_genesis.json
   ```

2. Upload to device via web interface

3. Load MP3 in browser and sync

The VP_DECODER in LightwaveOS will stream and interpolate the data for smooth 176 FPS playback.