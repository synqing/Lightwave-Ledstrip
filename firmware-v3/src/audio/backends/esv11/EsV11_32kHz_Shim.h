/**
 * @file EsV11_32kHz_Shim.h
 * @brief Vendor constant overrides for 32kHz ESV11 pipeline.
 *
 * Included via -include build flag BEFORE any vendor headers.
 * Overrides global_defines.h and microphone.h constants while
 * preserving all vendor DSP logic unmodified.
 *
 * 32kHz / 256-hop = 125 Hz frame rate (up from 12.8kHz / 256-hop = 50 Hz).
 * All time-domain windows keep the same DURATION, requiring larger buffers.
 */

#pragma once

// ============================================================================
// Core DSP Constants (override global_defines.h)
// ============================================================================

#define SAMPLE_RATE             (32000)     // 32 kHz (was 12800)
#define CHUNK_SIZE              (128)       // 4ms @ 32kHz, 2 chunks per 256-hop (was 64 = 5ms @ 12.8kHz)
#define SAMPLE_HISTORY_LENGTH   (10240)     // 320ms @ 32kHz (was 4096 = 320ms @ 12.8kHz)
#define NOVELTY_LOG_HZ          (125)       // Match new frame rate (was 50)
#define NOVELTY_HISTORY_LENGTH  (2560)      // 20.48s @ 125 Hz (was 1024 = 20.48s @ 50Hz)

// ============================================================================
// DC Blocker Coefficients (override microphone.h)
// ============================================================================

// R = 1 - (2 * pi * fc / SR),  G = (1 + R) / 2
// fc = 5 Hz, SR = 32000 Hz
#define DC_BLOCKER_R  0.999019f   // was 0.997545f @ 12.8kHz
#define DC_BLOCKER_G  0.999509f   // was 0.998772f @ 12.8kHz
