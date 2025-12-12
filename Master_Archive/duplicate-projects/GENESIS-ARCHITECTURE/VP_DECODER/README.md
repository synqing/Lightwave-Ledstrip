# VP Decoder Module

The VP Decoder (Visual Pipeline Decoder) is a core component of SpectraSynq that transforms pre-analyzed musical data from Genesis Engine into real-time visual representations.

## Overview

The VP Decoder acts as a "musical data interpreter" that:
- Loads Genesis Maps (JSON files containing pre-analyzed musical data)
- Supports streaming for large files (15-20MB) using sliding windows
- Synthesizes 96 frequency bins from bass/mid/high intensities
- Provides interpolated AudioFrame structures at 60fps
- Maintains precise playback timing and beat synchronization

## Files Included

### Core Files
- `vp_decoder.h` - Main decoder interface and data structures
- `vp_decoder.cpp` - Implementation with streaming support

### Dependencies
- `audio_frame.h` - AudioFrame data structure
- `audio_frame_constants.h` - FFT bin count and other constants
- `frequency_bin_optimizer.h/cpp` - Optimized frequency bin synthesis

## Genesis Map Format

Genesis Maps are JSON files produced by the Genesis Engine containing:

```json
{
  "metadata": {
    "filename": "song.mp3",
    "analysis_engine": "Genesis_Engine_v3.0_ML_Demucs",
    "timestamp_utc": "2025-06-15T07:18:23.349Z",
    "version": "v3.0"
  },
  "global_metrics": {
    "duration_ms": 201408,
    "bpm": 140
  },
  "layers": {
    "rhythm": {
      "beat_grid_ms": [0, 429, 857, 1286, ...]
    },
    "frequency": {
      "bass": [
        {"time_ms": 0, "intensity": 0.5},
        {"time_ms": 500, "intensity": 0.8},
        ...
      ],
      "mids": [...],
      "highs": [...]
    }
  }
}
```

## Usage Example

```cpp
#include "vp_decoder.h"

VPDecoder vp_decoder;

// Load from small JSON string
if (vp_decoder.loadFromJson(json_string)) {
    vp_decoder.startPlayback();
}

// Or load from large file with streaming
if (vp_decoder.loadFromFile("/spiffs/genesis_map.json")) {
    vp_decoder.startPlayback();
}

// In render loop
AudioFrame frame = vp_decoder.getCurrentFrame();
// Use frame.bass_energy, frame.frequency_bins, etc.
```

## Key Features

1. **Streaming Support**: Handles large files by loading 30-second windows
2. **Memory Efficient**: Uses sliding buffers to limit RAM usage
3. **Smooth Interpolation**: Linear interpolation between data points
4. **Beat Synchronization**: Precise timing with beat grid support
5. **Synthetic Frequency Generation**: Creates realistic spectrum from band intensities

## Data Flow

1. Genesis Engine analyzes music → produces Genesis Map (JSON)
2. VP Decoder loads Genesis Map → streams/interpolates data
3. VP Decoder generates AudioFrames → 60fps real-time output
4. Visual Pipeline consumes AudioFrames → LED animations

## Performance

- Frame generation: <1ms per frame
- Memory usage: ~400 bytes per AudioFrame + sliding window buffers
- File loading: Progressive streaming prevents blocking
- Supports files up to 20MB (approximately 30 minutes of music data)