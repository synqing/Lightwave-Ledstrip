#ifndef AUDIO_FRAME_CONSTANTS_H
#define AUDIO_FRAME_CONSTANTS_H

// FFT bin count for frequency analysis
// 96 bins provides good frequency resolution for LED visualization
#ifndef FFT_BIN_COUNT
#define FFT_BIN_COUNT 96
#endif

// Frequency band definitions (bin ranges)
#define BASS_BIN_START 0
#define BASS_BIN_END 31
#define MID_BIN_START 32
#define MID_BIN_END 63
#define HIGH_BIN_START 64
#define HIGH_BIN_END (FFT_BIN_COUNT - 1)

// Energy scaling factors for visual impact
#define BASS_ENERGY_SCALE 1000.0f
#define MID_ENERGY_SCALE 800.0f
#define HIGH_ENERGY_SCALE 600.0f

// Transient detection threshold
#define TRANSIENT_THRESHOLD 200.0f

// Silence detection threshold
#define SILENCE_THRESHOLD 10.0f

#endif // AUDIO_FRAME_CONSTANTS_H