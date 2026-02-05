/**
 * @file EsV11Buffers.h
 * @brief Pointer-backed vendor DSP buffers for Emotiscope v1.1_320.
 *
 * The upstream ES code defines several large globals in headers. On ESP32-S3 this
 * inflates internal DRAM (.bss) and can starve WiFi/AsyncTCP/mDNS.
 *
 * LightwaveOS allocates these buffers once during backend initialisation, using
 * PSRAM heap where available. There must be no allocations in any render path.
 */

#pragma once

#include "global_defines.h"
#include "types_min.h"

// Time-domain history (float -1..1), used by goertzel + waveform export.
extern float* sample_history;

// Shared window lookup table (size 4096), used by goertzel + tempo.
extern float* window_lookup;

// Tempo history (NOVELTY_HISTORY_LENGTH), used by tempo pipeline.
extern float* novelty_curve;
extern float* novelty_curve_normalized;
extern float* vu_curve;
extern float* vu_curve_normalized;

// Tempo resonator bank.
extern tempo* tempi;

// Goertzel frequency bin bank.
extern freq* frequencies_musical;

// Rolling spectrogram average (NUM_SPECTROGRAM_AVERAGE_SAMPLES x NUM_FREQS).
extern float (*spectrogram_average)[NUM_FREQS];

// Noise history used during magnitudes calculation (10 x NUM_FREQS).
extern float (*noise_history)[NUM_FREQS];

// Allocate all pointer-backed buffers. Safe to call more than once.
bool esv11_init_buffers();

// Free buffers (primarily for partial init failure paths).
void esv11_free_buffers();
