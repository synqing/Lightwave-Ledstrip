#pragma once

// Minimal subset of Emotiscope v1.1_320 global constants used by the DSP pipeline.

#define NUM_FREQS (64)
#define SAMPLE_HISTORY_LENGTH (4096)
#define CHUNK_SIZE (64)
#define SAMPLE_RATE (12800)

#define NOVELTY_LOG_HZ (50)
#define NOVELTY_HISTORY_LENGTH (1024)

#define NUM_TEMPI (96)
#define TEMPO_LOW (48)
#define TEMPO_HIGH (TEMPO_LOW + NUM_TEMPI)

#define BEAT_SHIFT_PERCENT (0.16f)

// ES reference frame rate used for its internal delta scaling (see gpu_core.h).
#define REFERENCE_FPS (100)

