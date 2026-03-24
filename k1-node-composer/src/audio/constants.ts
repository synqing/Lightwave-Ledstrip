/**
 * @file constants.ts
 * @brief ESV11 DSP pipeline constants — exact port from firmware.
 *
 * Sources:
 *   firmware-v3/src/audio/backends/esv11/vendor/global_defines.h
 *   firmware-v3/src/audio/backends/esv11/EsV11_32kHz_Shim.h
 *   firmware-v3/src/audio/backends/esv11/vendor/goertzel.h
 *   firmware-v3/src/audio/backends/esv11/vendor/vu.h
 *   firmware-v3/src/audio/backends/esv11/vendor/tempo.h
 *
 * All values match the 32kHz shim build (esp32dev_audio_esv11_k1v2_32khz).
 */

// ============================================================================
// Core DSP Constants (from EsV11_32kHz_Shim.h overriding global_defines.h)
// ============================================================================

/** Sample rate in Hz. 32kHz shim overrides the original 12800. */
export const SAMPLE_RATE = 32000;

/** Samples per DMA chunk. 128 = 4ms @ 32kHz. */
export const CHUNK_SIZE = 128;

/**
 * Time-domain sample ring buffer length.
 * 320ms @ 32kHz = 10240 samples (was 4096 = 320ms @ 12.8kHz).
 */
export const SAMPLE_HISTORY_LENGTH = 10240;

/** Number of Goertzel frequency bins. */
export const NUM_FREQS = 64;

// ============================================================================
// Goertzel Note Table Parameters (from goertzel.h)
// ============================================================================

/** Quarter-step index into notes[] for the lowest bin. */
export const BOTTOM_NOTE = 12;

/** Half-step spacing between bins in the notes[] table. */
export const NOTE_STEP = 2;

// ============================================================================
// Averaging Constants (from goertzel.h)
// ============================================================================

/** Number of frames averaged for magnitude smoothing (inside calculate_magnitudes). */
export const NUM_AVERAGE_SAMPLES = 2;

/** Number of frames averaged for spectrogram_smooth output. */
export const NUM_SPECTROGRAM_AVERAGE_SAMPLES = 12;

// ============================================================================
// Noise Floor Constants (from goertzel.h)
// ============================================================================

export const NOISE_CALIBRATION_WAIT_FRAMES = 256;
export const NOISE_CALIBRATION_ACTIVE_FRAMES = 512;

/** Number of 1-second noise snapshots kept for noise floor estimation. */
export const NOISE_HISTORY_SAMPLES = 10;

// ============================================================================
// Tempo Constants (from global_defines.h)
// ============================================================================

/** Number of tempo Goertzel bins. */
export const NUM_TEMPI = 96;

/** Lowest tempo in BPM. */
export const TEMPO_LOW = 48;

/** Highest tempo in BPM (TEMPO_LOW + NUM_TEMPI). */
export const TEMPO_HIGH = TEMPO_LOW + NUM_TEMPI;

/** Beat phase shift as fraction of pi. */
export const BEAT_SHIFT_PERCENT = 0.16;

/** ES reference frame rate used for internal delta scaling. */
export const REFERENCE_FPS = 100;

// ============================================================================
// Novelty Constants (from EsV11_32kHz_Shim.h / global_defines.h)
// ============================================================================

/** Novelty curve log rate in Hz. Restored to original 50 Hz. */
export const NOVELTY_LOG_HZ = 50;

/** Length of the novelty history ring buffer. */
export const NOVELTY_HISTORY_LENGTH = 1024;

// ============================================================================
// VU Constants (from vu.h)
// ============================================================================

/** Number of VU amplitude snapshots for noise floor estimation (250ms interval). */
export const NUM_VU_LOG_SAMPLES = 20;

/** Number of VU frames for smoothing average. */
export const NUM_VU_SMOOTH_SAMPLES = 12;

// ============================================================================
// DC Blocker Coefficients (from EsV11_32kHz_Shim.h)
// R = 1 - (2 * pi * fc / SR),  G = (1 + R) / 2
// fc = 5 Hz, SR = 32000 Hz
// ============================================================================

export const DC_BLOCKER_R = 0.999019;
export const DC_BLOCKER_G = 0.999509;

// ============================================================================
// Derived Constants
// ============================================================================

/** Hop size in samples (CHUNK_SIZE * 2 = 256). Two chunks per hop. */
export const HOP_SIZE = CHUNK_SIZE * 2;

/** Hop rate in Hz (SAMPLE_RATE / HOP_SIZE = 125 Hz). */
export const HOP_RATE_HZ = SAMPLE_RATE / HOP_SIZE;

// ============================================================================
// Mathematical Constants (from goertzel.h)
// ============================================================================

export const TWOPI = 6.28318530;
export const FOURPI = 12.56637061;
export const SIXPI = 18.84955593;

// ============================================================================
// Window Lookup Table Size (from EsV11Buffers.cpp — always 4096 points)
// ============================================================================

export const WINDOW_LOOKUP_SIZE = 4096;

// ============================================================================
// Rate-correction constants used by EMA filters throughout the pipeline.
// The original ES code ran at 12.8kHz/64 = 200 Hz chunk rate.
// At 32kHz/128 = 250 Hz chunk rate, EMA alphas are retuned to preserve
// the same time-domain behaviour.
// ============================================================================

export const BASELINE_CHUNK_RATE_HZ = 12800.0 / 64.0; // 200 Hz
export const CURRENT_CHUNK_RATE_HZ = SAMPLE_RATE / CHUNK_SIZE; // 250 Hz

/** Noise floor EMA alpha, retuned for current chunk rate. */
export const NOISE_ALPHA = 1.0 - Math.pow(0.99, BASELINE_CHUNK_RATE_HZ / CURRENT_CHUNK_RATE_HZ);

/** Autoranger EMA alpha, retuned for current chunk rate. */
export const AUTORANGER_ALPHA = 1.0 - Math.pow(0.995, BASELINE_CHUNK_RATE_HZ / CURRENT_CHUNK_RATE_HZ);

/** VU cap follower EMA alpha, retuned for current chunk rate. */
export const CAP_ALPHA = 1.0 - Math.pow(0.9, BASELINE_CHUNK_RATE_HZ / CURRENT_CHUNK_RATE_HZ);

/** Tempo EMA alpha, retuned for current chunk rate. */
export const TEMPO_ALPHA = 1.0 - Math.pow(0.975, BASELINE_CHUNK_RATE_HZ / CURRENT_CHUNK_RATE_HZ);

// ============================================================================
// ES Note Frequency Table (from goertzel.h)
// Quarter-step chromatic scale. EXACT values from firmware — do NOT recalculate.
// 192 entries covering ~55 Hz (A1) to ~16274 Hz.
// Index mapping: bin i uses notes[BOTTOM_NOTE + i * NOTE_STEP].
// ============================================================================

export const NOTES: readonly number[] = [
    // Octave ~55 Hz (A1 region) — indices 0–11
    55.0, 56.635235, 58.27047, 60.00294, 61.73541, 63.5709,
    65.40639, 67.351025, 69.29566, 71.355925, 73.41619, 75.59897,

    // Octave ~78 Hz — indices 12–23
    77.78175, 80.09432, 82.40689, 84.856975, 87.30706, 89.902835,
    92.49861, 95.248735, 97.99886, 100.91253, 103.8262, 106.9131,

    // Octave ~110 Hz (A2 region) — indices 24–35
    110.0, 113.27045, 116.5409, 120.00585, 123.4708, 127.1418,
    130.8128, 134.70205, 138.5913, 142.71185, 146.8324, 151.19795,

    // Octave ~156 Hz — indices 36–47
    155.5635, 160.18865, 164.8138, 169.71395, 174.6141, 179.80565,
    184.9972, 190.49745, 195.9977, 201.825, 207.6523, 213.82615,

    // Octave ~220 Hz (A3 region) — indices 48–59
    220.0, 226.54095, 233.0819, 240.0118, 246.9417, 254.28365,
    261.6256, 269.4041, 277.1826, 285.4237, 293.6648, 302.3959,

    // Octave ~311 Hz — indices 60–71
    311.127, 320.3773, 329.6276, 339.4279, 349.2282, 359.6113,
    369.9944, 380.9949, 391.9954, 403.65005, 415.3047, 427.65235,

    // Octave ~440 Hz (A4 region) — indices 72–83
    440.0, 453.0819, 466.1638, 480.02355, 493.8833, 508.5672,
    523.2511, 538.8082, 554.3653, 570.8474, 587.3295, 604.79175,

    // Octave ~622 Hz — indices 84–95
    622.254, 640.75455, 659.2551, 678.8558, 698.4565, 719.22265,
    739.9888, 761.98985, 783.9909, 807.30015, 830.6094, 855.3047,

    // Octave ~880 Hz (A5 region) — indices 96–107
    880.0, 906.16375, 932.3275, 960.04705, 987.7666, 1017.1343,
    1046.502, 1077.6165, 1108.731, 1141.695, 1174.659, 1209.5835,

    // Octave ~1244 Hz — indices 108–119
    1244.508, 1281.509, 1318.51, 1357.7115, 1396.913, 1438.4455,
    1479.978, 1523.98, 1567.982, 1614.6005, 1661.219, 1710.6095,

    // Octave ~1760 Hz (A6 region) — indices 120–131
    1760.0, 1812.3275, 1864.655, 1920.094, 1975.533, 2034.269,
    2093.005, 2155.233, 2217.461, 2283.3895, 2349.318, 2419.167,

    // Octave ~2489 Hz — indices 132–143
    2489.016, 2563.018, 2637.02, 2715.4225, 2793.825, 2876.8905,
    2959.956, 3047.96, 3135.964, 3229.2005, 3322.437, 3421.2185,

    // Octave ~3520 Hz (A7 region) — indices 144–155
    3520.0, 3624.655, 3729.31, 3840.1875, 3951.065, 4068.537,
    4186.009, 4310.4655, 4434.922, 4566.779, 4698.636, 4838.334,

    // Octave ~4978 Hz — indices 156–167
    4978.032, 5126.0365, 5274.041, 5430.8465, 5587.652, 5753.7815,
    5919.911, 6095.919, 6271.927, 6458.401, 6644.875, 6842.4375,

    // Octave ~7040 Hz (A8 region) — indices 168–179
    7040.0, 7249.31, 7458.62, 7680.375, 7902.13, 8137.074,
    8372.018, 8620.931, 8869.844, 9133.558, 9397.272, 9676.668,

    // Octave ~9956 Hz — indices 180–191 (partial; last entry is index 185)
    9956.064, 10252.072, 10548.08, 10861.69, 11175.3, 11507.56,
    11839.82, 12191.835, 12543.85, 12916.8, 13289.75, 13684.875,

    // Octave ~14080 Hz — indices 192–197
    14080.0, 14498.62, 14917.24, 15360.75, 15804.26, 16274.145,
] as const;
