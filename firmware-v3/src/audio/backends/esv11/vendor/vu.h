/**
 * Vendored from Emotiscope v1.1_320 (DSP-only; unchanged logic).
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

#include "EsV11Shim.h"
#include "global_defines.h"
#include "microphone.h"
#include "utilities_min.h"

#define NUM_VU_LOG_SAMPLES 20
#define NUM_VU_SMOOTH_SAMPLES 12

float vu_log[NUM_VU_LOG_SAMPLES] = { 0 };
uint16_t vu_log_index = 0;

float vu_smooth[NUM_VU_SMOOTH_SAMPLES] = { 0 };
uint16_t vu_smooth_index = 0;

volatile float vu_level_raw = 0.0;
volatile float vu_level = 0.0;
volatile float vu_max = 0.0;
volatile float vu_floor = 0.0;

uint32_t last_vu_log = 0;

inline void init_vu(){
    for(uint8_t i = 0; i < NUM_VU_LOG_SAMPLES; i++){
        vu_log[i] = 0.0;
    }
    for(uint8_t i = 0; i < NUM_VU_SMOOTH_SAMPLES; i++){
        vu_smooth[i] = 0.0;
    }
}

inline void run_vu(){
    profile_function([&]() {
        // CALCULATE AMPLITUDE ------------------------------------------------
        static float max_amplitude_cap = 0.0000001f;
        float* samples = &sample_history[(SAMPLE_HISTORY_LENGTH-1) - CHUNK_SIZE];

        float max_amplitude_now = 0.000001f;
        for(uint16_t i = 0; i < CHUNK_SIZE; i++){
            float sample = samples[i];
            float sample_abs = fabsf(sample);

            max_amplitude_now = fmaxf(max_amplitude_now, sample_abs*sample_abs);
        }
        max_amplitude_now = clip_float(max_amplitude_now);

        // LOG AMPLITUDE FOR NOISE REMOVAL ------------------------------------
        if(t_now_ms < 2000){
            // Fill log with current amplitude at boot for fast convergence
            for (uint8_t i = 0; i < NUM_VU_LOG_SAMPLES; i++) {
                vu_log[i] = max_amplitude_now;
            }
        }
        else if(t_now_ms - last_vu_log >= 250){
            last_vu_log = t_now_ms;
            vu_log[vu_log_index] = max_amplitude_now;
            vu_log_index = (vu_log_index + 1) % NUM_VU_LOG_SAMPLES;

            float vu_sum = 0.0f;
            for(uint16_t i = 0; i < NUM_VU_LOG_SAMPLES; i++){
                vu_sum += vu_log[i];
            }
            vu_floor = vu_sum / NUM_VU_LOG_SAMPLES;

            vu_floor *= 0.90f;
        }

        // SCALE OUTPUT -------------------------------------------------------
        max_amplitude_now = fmaxf(max_amplitude_now - vu_floor, 0.0f);

        if(max_amplitude_now > max_amplitude_cap){
            float distance = max_amplitude_now - max_amplitude_cap;
            max_amplitude_cap += (distance * 0.1f);
        }
        else if(max_amplitude_cap > max_amplitude_now){
            float distance = max_amplitude_cap - max_amplitude_now;
            max_amplitude_cap -= (distance * 0.1f);
        }
        max_amplitude_cap = clip_float(max_amplitude_cap);

        if(max_amplitude_cap < 0.000025f){
            max_amplitude_cap = 0.000025f;
        }

        float auto_scale = 1.0f / fmaxf(max_amplitude_cap, 0.00001f);

        vu_level_raw = clip_float(max_amplitude_now*auto_scale);

        // SMOOTHING ---------------------------------------------------------
        vu_smooth[vu_smooth_index] = vu_level_raw;
        vu_smooth_index = (vu_smooth_index + 1) % NUM_VU_SMOOTH_SAMPLES;

        float vu_sum = 0.0f;
        for(uint8_t i = 0; i < NUM_VU_SMOOTH_SAMPLES; i++){
            vu_sum += vu_smooth[i];
        }
        vu_level = vu_sum / NUM_VU_SMOOTH_SAMPLES;

        // MAX VALUE ---------------------------------------------------------
        vu_max = fmaxf(vu_max, vu_level);
    }, __func__);
}

