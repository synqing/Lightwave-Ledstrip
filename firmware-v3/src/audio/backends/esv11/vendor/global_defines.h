#pragma once

// Minimal subset of Emotiscope v1.1_320 global constants used by the DSP pipeline.
// Guards allow override via build flags or -include shim (e.g. EsV11_32kHz_Shim.h).

#define NUM_FREQS (64)

#ifndef SAMPLE_HISTORY_LENGTH
#define SAMPLE_HISTORY_LENGTH (4096)
#endif

#ifndef CHUNK_SIZE
#define CHUNK_SIZE (64)
#endif

#ifndef SAMPLE_RATE
#define SAMPLE_RATE (12800)
#endif

#ifndef NOVELTY_LOG_HZ
#define NOVELTY_LOG_HZ (50)
#endif

#ifndef NOVELTY_HISTORY_LENGTH
#define NOVELTY_HISTORY_LENGTH (1024)
#endif

#define NUM_TEMPI (96)
#define TEMPO_LOW (48)
#define TEMPO_HIGH (TEMPO_LOW + NUM_TEMPI)

#define BEAT_SHIFT_PERCENT (0.16f)

// ES reference frame rate used for its internal delta scaling (see gpu_core.h).
#define REFERENCE_FPS (100)

