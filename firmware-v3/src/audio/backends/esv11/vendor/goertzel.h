/**
 * Vendored from Emotiscope v1.1_320 (DSP-only; trimmed of UI/web helpers).
 *
 * Provides:
 * - init_window_lookup()
 * - init_goertzel_constants()
 * - calculate_magnitudes() -> spectrogram + spectrogram_smooth
 * - get_chromagram() -> chromagram[12]
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

#include "EsV11Shim.h"
#include "EsV11Buffers.h"
#include "global_defines.h"
#include "types_min.h"
#include "microphone.h"
#include "utilities_min.h"

#define TWOPI   6.28318530f
#define FOURPI 12.56637061f
#define SIXPI  18.84955593f

#define BOTTOM_NOTE 12  // Quarter-step index in ES table
#define NOTE_STEP 2     // Half-step spacing

#define NOISE_CALIBRATION_WAIT_FRAMES   256
#define NOISE_CALIBRATION_ACTIVE_FRAMES 512

// NOTE: ES note table is data-only; preserved for frequency mapping.
static const float notes[] = {
    55.0f, 56.635235f, 58.27047f, 60.00294f, 61.73541f, 63.5709f, 65.40639f, 67.351025f, 69.29566f, 71.355925f, 73.41619f, 75.59897f,
    77.78175f, 80.09432f, 82.40689f, 84.856975f, 87.30706f, 89.902835f, 92.49861f, 95.248735f, 97.99886f, 100.91253f, 103.8262f, 106.9131f,
    110.0f, 113.27045f, 116.5409f, 120.00585f, 123.4708f, 127.1418f, 130.8128f, 134.70205f, 138.5913f, 142.71185f, 146.8324f, 151.19795f,
    155.5635f, 160.18865f, 164.8138f, 169.71395f, 174.6141f, 179.80565f, 184.9972f, 190.49745f, 195.9977f, 201.825f, 207.6523f, 213.82615f,
    220.0f, 226.54095f, 233.0819f, 240.0118f, 246.9417f, 254.28365f, 261.6256f, 269.4041f, 277.1826f, 285.4237f, 293.6648f, 302.3959f,
    311.127f, 320.3773f, 329.6276f, 339.4279f, 349.2282f, 359.6113f, 369.9944f, 380.9949f, 391.9954f, 403.65005f, 415.3047f, 427.65235f,
    440.0f, 453.0819f, 466.1638f, 480.02355f, 493.8833f, 508.5672f, 523.2511f, 538.8082f, 554.3653f, 570.8474f, 587.3295f, 604.79175f,
    622.254f, 640.75455f, 659.2551f, 678.8558f, 698.4565f, 719.22265f, 739.9888f, 761.98985f, 783.9909f, 807.30015f, 830.6094f, 855.3047f,
    880.0f, 906.16375f, 932.3275f, 960.04705f, 987.7666f, 1017.1343f, 1046.502f, 1077.6165f, 1108.731f, 1141.695f, 1174.659f, 1209.5835f,
    1244.508f, 1281.509f, 1318.51f, 1357.7115f, 1396.913f, 1438.4455f, 1479.978f, 1523.98f, 1567.982f, 1614.6005f, 1661.219f, 1710.6095f,
    1760.0f, 1812.3275f, 1864.655f, 1920.094f, 1975.533f, 2034.269f, 2093.005f, 2155.233f, 2217.461f, 2283.3895f, 2349.318f, 2419.167f,
    2489.016f, 2563.018f, 2637.02f, 2715.4225f, 2793.825f, 2876.8905f, 2959.956f, 3047.96f, 3135.964f, 3229.2005f, 3322.437f, 3421.2185f,
    3520.0f, 3624.655f, 3729.31f, 3840.1875f, 3951.065f, 4068.537f, 4186.009f, 4310.4655f, 4434.922f, 4566.779f, 4698.636f, 4838.334f,
    4978.032f, 5126.0365f, 5274.041f, 5430.8465f, 5587.652f, 5753.7815f, 5919.911f, 6095.919f, 6271.927f, 6458.401f, 6644.875f, 6842.4375f,
    7040.0f, 7249.31f, 7458.62f, 7680.375f, 7902.13f, 8137.074f, 8372.018f, 8620.931f, 8869.844f, 9133.558f, 9397.272f, 9676.668f,
    9956.064f, 10252.072f, 10548.08f, 10861.69f, 11175.3f, 11507.56f, 11839.82f, 12191.835f, 12543.85f, 12916.8f, 13289.75f, 13684.875f,
    14080.0f, 14498.62f, 14917.24f, 15360.75f, 15804.26f, 16274.145f
};

uint32_t noise_calibration_wait_frames_remaining = 0;
uint32_t noise_calibration_active_frames_remaining = 0;

uint16_t max_goertzel_block_size = 0;

volatile bool magnitudes_locked = false;

float spectrogram[NUM_FREQS];
float chromagram[12];

const uint8_t NUM_SPECTROGRAM_AVERAGE_SAMPLES = 12;
float spectrogram_smooth[NUM_FREQS] = { 0.0f };
uint8_t spectrogram_average_index = 0;

inline void init_goertzel(uint16_t frequency_slot, float frequency, float bandwidth) {
    frequencies_musical[frequency_slot].target_freq = frequency;
    frequencies_musical[frequency_slot].block_size = static_cast<uint16_t>(SAMPLE_RATE / (bandwidth));

    while (frequencies_musical[frequency_slot].block_size % 4 != 0) {
        frequencies_musical[frequency_slot].block_size -= 1;
    }

    if (frequencies_musical[frequency_slot].block_size > SAMPLE_HISTORY_LENGTH - 1) {
        frequencies_musical[frequency_slot].block_size = SAMPLE_HISTORY_LENGTH - 1;
    }

    max_goertzel_block_size = static_cast<uint16_t>(fmaxf(max_goertzel_block_size, frequencies_musical[frequency_slot].block_size));

    frequencies_musical[frequency_slot].window_step = 4096.0f / frequencies_musical[frequency_slot].block_size;

    float k = (int)(0.5f + ((frequencies_musical[frequency_slot].block_size * frequencies_musical[frequency_slot].target_freq) / SAMPLE_RATE));
    float w = (2.0f * static_cast<float>(M_PI) * k) / frequencies_musical[frequency_slot].block_size;
    frequencies_musical[frequency_slot].coeff = 2.0f * cosf(w);
}

inline void init_goertzel_constants() {
    for(uint16_t i = 0; i < NUM_FREQS; i++){
        uint16_t note = BOTTOM_NOTE + (i * NOTE_STEP);
        frequencies_musical[i].target_freq = notes[note];

        float neighbor_left;
        float neighbor_right;

        if (note == 0) {
            neighbor_left = notes[note];
            neighbor_right = notes[note + 1];
        }
        else if (note == NUM_FREQS - 1) {
            neighbor_left = notes[note - 1];
            neighbor_right = notes[note];
        }
        else {
            neighbor_left = notes[note - 1];
            neighbor_right = notes[note + 1];
        }

        float neighbor_distance_hz = fmaxf(
            fabsf(frequencies_musical[i].target_freq - neighbor_left),
            fabsf(frequencies_musical[i].target_freq - neighbor_right));

        init_goertzel(i, frequencies_musical[i].target_freq, neighbor_distance_hz * 4.0f);
    }
}

inline void init_window_lookup() {
    for (uint16_t i = 0; i < 2048; i++) {
        float sigma = 0.8f;
        float n_minus_halfN = i - 2048 / 2;
        float gaussian_weighing_factor = expf(-0.5f * powf((n_minus_halfN / (sigma * 2048 / 2)), 2));
        float weighing_factor = gaussian_weighing_factor;

        window_lookup[i] = weighing_factor;
        window_lookup[4095 - i] = weighing_factor;
    }
}

inline float calculate_magnitude_of_bin(uint16_t bin_number) {
    float magnitude_squared;
    float normalized_magnitude;
    float scale;

    profile_function([&]() {
        float q1 = 0.0f;
        float q2 = 0.0f;
        float window_pos = 0.0f;

        const uint16_t block_size = frequencies_musical[bin_number].block_size;
        float coeff = frequencies_musical[bin_number].coeff;
        float window_step = frequencies_musical[bin_number].window_step;

        float* sample_ptr = &sample_history[(SAMPLE_HISTORY_LENGTH - 1) - block_size];

        for (uint16_t i = 0; i < block_size; i++) {
            float windowed_sample = sample_ptr[i] * window_lookup[(uint32_t)window_pos];
            float q0 = coeff * q1 - q2 + windowed_sample;
            q2 = q1;
            q1 = q0;

            window_pos += window_step;
        }

        magnitude_squared = (q1 * q1) + (q2 * q2) - q1 * q2 * coeff;
        normalized_magnitude = magnitude_squared / (block_size / 2.0f);

        float progress = float(bin_number) / NUM_FREQS;
        progress *= progress;
        progress *= progress;
        scale = (progress * 0.9975f) + 0.0025f;
    }, __func__ );

    return sqrtf(normalized_magnitude * scale);
}

inline void calculate_magnitudes() {
    profile_function([&]() {
        magnitudes_locked = true;

        const uint16_t NUM_AVERAGE_SAMPLES = 2;

        static float magnitudes_raw[NUM_FREQS];
        static float magnitudes_noise_filtered[NUM_FREQS];
        static float magnitudes_avg[NUM_AVERAGE_SAMPLES][NUM_FREQS];
        static float magnitudes_smooth[NUM_FREQS];
        static float max_val_smooth = 0.0f;

        static float noise_floor[NUM_FREQS];
        static uint8_t noise_history_index = 0;
        static uint32_t last_noise_spectrum_log = 0;

        if(t_now_ms - last_noise_spectrum_log >= 1000){
            last_noise_spectrum_log = t_now_ms;
            noise_history_index++;
            if(noise_history_index >= 10){
                noise_history_index = 0;
            }
            memcpy(noise_history[noise_history_index], magnitudes_raw, sizeof(float) * NUM_FREQS);
        }

        static uint32_t iter = 0;
        iter++;

        static bool interlacing_frame_field = 0;
        interlacing_frame_field = !interlacing_frame_field;

        float max_val = 0.0f;
        for (uint16_t i = 0; i < NUM_FREQS; i++) {
            bool interlace_field_now = ((i % 2) == 0);
            if (interlace_field_now == interlacing_frame_field) {
                magnitudes_raw[i] = calculate_magnitude_of_bin(i);

                float avg_val = 0.0f;
                for(uint8_t j = 0; j < 10; j++){
                    avg_val += noise_history[j][i];
                }
                avg_val /= 10.0f;
                avg_val *= 0.90f;

                // Noise floor EMA runs per chunk; retune against baseline chunk rate (12.8k/64 = 200 Hz).
                static constexpr float kBaselineChunkRateHz = 12800.0f / 64.0f;
                static constexpr float kCurrentChunkRateHz =
                    static_cast<float>(SAMPLE_RATE) / static_cast<float>(CHUNK_SIZE);
                static const float kNoiseAlpha = 1.0f - powf(0.99f, kBaselineChunkRateHz / kCurrentChunkRateHz);
                noise_floor[i] = noise_floor[i] * (1.0f - kNoiseAlpha) + avg_val * kNoiseAlpha;
                magnitudes_noise_filtered[i] = fmaxf(magnitudes_raw[i] - noise_floor[i], 0.0f);
            }

            frequencies_musical[i].magnitude_full_scale = magnitudes_noise_filtered[i];
            magnitudes_avg[iter % NUM_AVERAGE_SAMPLES][i] = magnitudes_noise_filtered[i];

            float magnitudes_avg_result = 0.0f;
            for (uint8_t a = 0; a < NUM_AVERAGE_SAMPLES; a++) {
                magnitudes_avg_result += magnitudes_avg[a][i];
            }
            magnitudes_avg_result /= NUM_AVERAGE_SAMPLES;

            magnitudes_smooth[i] = magnitudes_avg_result;

            if (magnitudes_smooth[i] > max_val) {
                max_val = magnitudes_smooth[i];
            }
        }

        // Smooth max_val with different speed limits for increases vs decreases.
        // Autoranger EMA runs per chunk; retune against baseline chunk rate.
        static constexpr float kBaselineChunkRateHz = 12800.0f / 64.0f;
        static constexpr float kCurrentChunkRateHz =
            static_cast<float>(SAMPLE_RATE) / static_cast<float>(CHUNK_SIZE);
        static const float kAutorangerAlpha = 1.0f - powf(0.995f, kBaselineChunkRateHz / kCurrentChunkRateHz);
        if (max_val > max_val_smooth) {
            float delta = max_val - max_val_smooth;
            max_val_smooth += delta * kAutorangerAlpha;
        }
        if (max_val < max_val_smooth) {
            float delta = max_val_smooth - max_val;
            max_val_smooth -= delta * kAutorangerAlpha;
        }

        if (max_val_smooth < 0.0025f) {
            max_val_smooth = 0.0025f;
        }

        float autoranger_scale = 1.0f / (max_val_smooth);

        for (uint16_t i = 0; i < NUM_FREQS; i++) {
            frequencies_musical[i].magnitude = clip_float(magnitudes_smooth[i] * autoranger_scale);
            spectrogram[i] = frequencies_musical[i].magnitude;
        }

        spectrogram_average_index++;
        if(spectrogram_average_index >= NUM_SPECTROGRAM_AVERAGE_SAMPLES){
            spectrogram_average_index = 0;
        }

        for(uint16_t i = 0; i < NUM_FREQS; i++){
            spectrogram_average[spectrogram_average_index][i] = spectrogram[i];

            spectrogram_smooth[i] = 0;
            for(uint16_t a = 0; a < NUM_SPECTROGRAM_AVERAGE_SAMPLES; a++){
                spectrogram_smooth[i] += spectrogram_average[a][i];
            }
            spectrogram_smooth[i] /= float(NUM_SPECTROGRAM_AVERAGE_SAMPLES);
        }

        magnitudes_locked = false;
    }, __func__ );
}

inline void get_chromagram(){
    profile_function([&]() {
        memset(chromagram, 0, sizeof(float) * 12);

        float max_val = 0.2f;
        for(uint16_t i = 0; i < 60; i++){
            chromagram[ i % 12 ] += (spectrogram_smooth[i] / 5.0f);
            max_val = fmaxf(max_val, chromagram[ i % 12 ]);
        }

        float auto_scale = 1.0f / max_val;
        for(uint16_t i = 0; i < 12; i++){
            chromagram[i] *= auto_scale;
        }
    }, __func__ );
}
