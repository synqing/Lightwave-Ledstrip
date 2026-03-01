/**
 * Vendored core of Emotiscope v1.1_320 tempo pipeline (trimmed to required symbols).
 */

#pragma once

#include <cmath>
#include <cstring>

#include "EsV11Shim.h"
#include "EsV11Buffers.h"
#include "global_defines.h"
#include "types_min.h"
#include "utilities_min.h"
#include "goertzel.h"
#include "vu.h"

bool silence_detected = true;
float silence_level = 1.0f;

float tempo_confidence = 0.0f;

float MAX_TEMPO_RANGE = 1.0f;

float tempi_bpm_values_hz[NUM_TEMPI];

float tempi_smooth[NUM_TEMPI];
float tempi_power_sum = 0.0f;

inline void init_tempo_goertzel_constants() {
    profile_function([&]() {
        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            float progress = float(i) / NUM_TEMPI;
            float tempi_range = TEMPO_HIGH - TEMPO_LOW;
            float tempo_bpm = tempi_range * progress + TEMPO_LOW;
            tempi_bpm_values_hz[i] = tempo_bpm / 60.0f;
        }

        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            tempi[i].target_tempo_hz = tempi_bpm_values_hz[i];

            float neighbor_left;
            float neighbor_right;
            if (i == 0) {
                neighbor_left = tempi_bpm_values_hz[i];
                neighbor_right = tempi_bpm_values_hz[i + 1];
            } else if (i == NUM_TEMPI - 1) {
                neighbor_left = tempi_bpm_values_hz[i - 1];
                neighbor_right = tempi_bpm_values_hz[i];
            } else {
                neighbor_left = tempi_bpm_values_hz[i - 1];
                neighbor_right = tempi_bpm_values_hz[i + 1];
            }

            float neighbor_left_distance_hz = fabsf(neighbor_left - tempi[i].target_tempo_hz);
            float neighbor_right_distance_hz = fabsf(neighbor_right - tempi[i].target_tempo_hz);
            float max_distance_hz = fmaxf(neighbor_left_distance_hz, neighbor_right_distance_hz);

            tempi[i].block_size = static_cast<uint32_t>(NOVELTY_LOG_HZ / (max_distance_hz * 0.5f));
            if (tempi[i].block_size > NOVELTY_HISTORY_LENGTH) {
                tempi[i].block_size = NOVELTY_HISTORY_LENGTH;
            }

            float k = (int)(0.5f + ((tempi[i].block_size * tempi[i].target_tempo_hz) / NOVELTY_LOG_HZ));
            float w = (2.0f * static_cast<float>(M_PI) * k) / tempi[i].block_size;
            tempi[i].cosine = cosf(w);
            tempi[i].sine = sinf(w);
            tempi[i].coeff = 2.0f * tempi[i].cosine;

            tempi[i].window_step = 4096.0f / tempi[i].block_size;
            tempi[i].phase_radians_per_reference_frame =
                ((2.0f * static_cast<float>(M_PI) * tempi[i].target_tempo_hz) / float(REFERENCE_FPS));

            tempi[i].phase_inverted = false;
        }
    }, __func__);
}

inline void normalize_novelty_curve() {
    profile_function([&]() {
        // Auto-scale novelty_curve into novelty_curve_normalized
        float max_val = 0.00001f;
        for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
            if (novelty_curve[i] > max_val) max_val = novelty_curve[i];
        }
        float auto_scale = 1.0f / max_val;
        dsps_mulc_f32(novelty_curve, novelty_curve_normalized, NOVELTY_HISTORY_LENGTH, auto_scale, 1, 1);

        max_val = 0.00001f;
        for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
            if (vu_curve[i] > max_val) max_val = vu_curve[i];
        }
        auto_scale = 1.0f / max_val;
        dsps_mulc_f32(vu_curve, vu_curve_normalized, NOVELTY_HISTORY_LENGTH, auto_scale, 1, 1);
    }, __func__);
}

inline float calculate_magnitude_of_tempo(uint16_t tempo_bin) {
    float normalized_magnitude = 0.0f;
    profile_function([&]() {
        uint16_t block_size = static_cast<uint16_t>(tempi[tempo_bin].block_size);
        float q1 = 0.0f;
        float q2 = 0.0f;
        float window_pos = 0.0f;

        for (uint16_t i = 0; i < block_size; i++) {
            float sample_novelty = novelty_curve_normalized[((NOVELTY_HISTORY_LENGTH - 1) - block_size) + i];
            float q0 = tempi[tempo_bin].coeff * q1 - q2 + (sample_novelty * window_lookup[(uint32_t)window_pos]);
            q2 = q1;
            q1 = q0;
            window_pos += (tempi[tempo_bin].window_step);
        }

        float real = (q1 - q2 * tempi[tempo_bin].cosine);
        float imag = (q2 * tempi[tempo_bin].sine);

        tempi[tempo_bin].phase = atan2f(imag, real) + (static_cast<float>(M_PI) * BEAT_SHIFT_PERCENT);
        if (tempi[tempo_bin].phase > static_cast<float>(M_PI)) {
            tempi[tempo_bin].phase -= (2 * static_cast<float>(M_PI));
            tempi[tempo_bin].phase_inverted = !tempi[tempo_bin].phase_inverted;
        } else if (tempi[tempo_bin].phase < -static_cast<float>(M_PI)) {
            tempi[tempo_bin].phase += (2 * static_cast<float>(M_PI));
            tempi[tempo_bin].phase_inverted = !tempi[tempo_bin].phase_inverted;
        }

        float magnitude_squared = (q1 * q1) + (q2 * q2) - q1 * q2 * tempi[tempo_bin].coeff;
        float magnitude = sqrtf(magnitude_squared);
        normalized_magnitude = magnitude / (block_size / 2.0f);
    }, __func__);
    return normalized_magnitude;
}

inline void calculate_tempi_magnitudes(int16_t single_bin = -1) {
    profile_function([&]() {
        float max_val = 0.0f;
        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            if (single_bin != -1) {
                if (i == single_bin) {
                    tempi[i].magnitude_full_scale = calculate_magnitude_of_tempo(single_bin);
                }
            } else {
                tempi[i].magnitude_full_scale = calculate_magnitude_of_tempo(i);
            }
            if (tempi[i].magnitude_full_scale > max_val) {
                max_val = tempi[i].magnitude_full_scale;
            }
        }

        if (max_val < 0.02f) max_val = 0.02f;
        float autoranger_scale = 1.0f / max_val;

        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            float scaled_magnitude = (tempi[i].magnitude_full_scale * autoranger_scale);
            if (scaled_magnitude < 0.0f) scaled_magnitude = 0.0f;
            if (scaled_magnitude > 1.0f) scaled_magnitude = 1.0f;
            tempi[i].magnitude = scaled_magnitude * scaled_magnitude * scaled_magnitude;
        }
    }, __func__);
}

inline void update_tempo() {
    profile_function([&]() {
        static uint32_t iter = 0;
        iter++;

        normalize_novelty_curve();

        static uint16_t calc_bin = 0;
        uint16_t max_bin = static_cast<uint16_t>((NUM_TEMPI - 1) * MAX_TEMPO_RANGE);

        if (iter % 2 == 0) {
            calculate_tempi_magnitudes(static_cast<int16_t>(calc_bin + 0));
        } else {
            calculate_tempi_magnitudes(static_cast<int16_t>(calc_bin + 1));
        }

        calc_bin += 2;
        if (calc_bin >= max_bin) {
            calc_bin = 0;
        }
    }, __func__);
}

inline void log_novelty(float input) {
    profile_function([&]() {
        shift_array_left(novelty_curve, NOVELTY_HISTORY_LENGTH, 1);
        novelty_curve[NOVELTY_HISTORY_LENGTH - 1] = input;
    }, __func__);
}

inline void log_vu(float input) {
    profile_function([&]() {
        static float last_input = input;
        float positive_difference = fmaxf(input - last_input, 0.0f);
        shift_array_left(vu_curve, NOVELTY_HISTORY_LENGTH, 1);
        vu_curve[NOVELTY_HISTORY_LENGTH - 1] = positive_difference;
        last_input = input;
    }, __func__);
}

inline void reduce_tempo_history(float reduction_amount) {
    profile_function([&]() {
        float reduction_amount_inv = 1.0f - reduction_amount;
        for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
            novelty_curve[i] = fmaxf(novelty_curve[i] * reduction_amount_inv, 0.00001f);
            vu_curve[i] = fmaxf(vu_curve[i] * reduction_amount_inv, 0.00001f);
        }
    }, __func__);
}

inline void check_silence(float current_novelty) {
    profile_function([&]() {
        // Silence window: 2.56s regardless of frame rate
        static constexpr uint16_t kSilenceFrames = static_cast<uint16_t>(2.56f * NOVELTY_LOG_HZ);
        float min_val = 1.0f;
        float max_val = 0.0f;
        for (uint16_t i = 0; i < kSilenceFrames; i++) {
            float recent_novelty = novelty_curve_normalized[(NOVELTY_HISTORY_LENGTH - 1 - kSilenceFrames) + i];
            recent_novelty = fminf(0.5f, recent_novelty) * 2.0f;

            float scaled_value = sqrtf(recent_novelty);
            max_val = fmaxf(max_val, scaled_value);
            min_val = fminf(min_val, scaled_value);
        }
        float novelty_contrast = fabsf(max_val - min_val);
        float silence_level_raw = 1.0f - novelty_contrast;

        silence_level = fmaxf(0.0f, silence_level_raw - 0.5f) * 2.0f;
        if (silence_level_raw > 0.5f) {
            silence_detected = true;
            reduce_tempo_history(silence_level * 0.10f);
        } else {
            silence_level = 0.0f;
            silence_detected = false;
        }
    }, __func__);
}

inline void update_novelty() {
    profile_function([&]() {
        static uint32_t next_update = t_now_us;

        const uint32_t update_interval_us = 1000000 / NOVELTY_LOG_HZ;
        if (t_now_us >= next_update) {
            next_update += update_interval_us;

            float current_novelty = 0.0f;
            for (uint16_t i = 0; i < NUM_FREQS; i++) {
                float new_mag = spectrogram_smooth[i];
                frequencies_musical[i].novelty = fmaxf(0.0f, new_mag - frequencies_musical[i].magnitude_last);
                current_novelty += frequencies_musical[i].novelty;
                frequencies_musical[i].magnitude_last = new_mag;
            }
            current_novelty /= float(NUM_FREQS);
            check_silence(current_novelty);
            log_novelty(log1pf(current_novelty));
            log_vu(vu_max);
            vu_max = 0.000001f;
        }
    }, __func__);
}

inline void sync_beat_phase(uint16_t tempo_bin, float delta) {
    profile_function([&]() {
        float push = (tempi[tempo_bin].phase_radians_per_reference_frame * delta);
        tempi[tempo_bin].phase += push;

        if (tempi[tempo_bin].phase > static_cast<float>(M_PI)) {
            tempi[tempo_bin].phase -= (2 * static_cast<float>(M_PI));
            tempi[tempo_bin].phase_inverted = !tempi[tempo_bin].phase_inverted;
        } else if (tempi[tempo_bin].phase < -static_cast<float>(M_PI)) {
            tempi[tempo_bin].phase += (2 * static_cast<float>(M_PI));
            tempi[tempo_bin].phase_inverted = !tempi[tempo_bin].phase_inverted;
        }

        tempi[tempo_bin].beat = sinf(tempi[tempo_bin].phase);
    }, __func__);
}

inline void update_tempi_phase(float delta) {
    profile_function([&]() {
        tempi_power_sum = 0.00000001f;
        for (uint16_t tempo_bin = 0; tempo_bin < NUM_TEMPI; tempo_bin++) {
            float tempi_magnitude = tempi[tempo_bin].magnitude;
            // Tempo EMA runs per chunk; retune against baseline chunk rate (12.8k/64 = 200 Hz).
            static constexpr float kBaselineChunkRateHz = 12800.0f / 64.0f;
            static constexpr float kCurrentChunkRateHz =
                static_cast<float>(SAMPLE_RATE) / static_cast<float>(CHUNK_SIZE);
            static const float kTempoAlpha = 1.0f - powf(0.975f, kBaselineChunkRateHz / kCurrentChunkRateHz);
            tempi_smooth[tempo_bin] = tempi_smooth[tempo_bin] * (1.0f - kTempoAlpha) + (tempi_magnitude) * kTempoAlpha;
            tempi_power_sum += tempi_smooth[tempo_bin];
            sync_beat_phase(tempo_bin, delta);
        }

        float max_contribution = 0.000001f;
        for (uint16_t tempo_bin = 0; tempo_bin < NUM_TEMPI; tempo_bin++) {
            max_contribution = fmaxf(tempi_smooth[tempo_bin] / tempi_power_sum, max_contribution);
        }
        tempo_confidence = max_contribution;
    }, __func__);
}

inline uint16_t esv11_pick_top_tempo_bin_raw() {
    uint16_t top_bin = 0;
    float top_mag = 0.0f;
    for (uint16_t i = 0; i < NUM_TEMPI; ++i) {
        const float m = tempi_smooth[i];
        if (m > top_mag) {
            top_mag = m;
            top_bin = i;
        }
    }
    return top_bin;
}

inline int16_t esv11_bpm_to_bin(float bpm) {
    const int32_t idx = static_cast<int32_t>(lroundf(bpm - static_cast<float>(TEMPO_LOW)));
    if (idx < 0 || idx >= static_cast<int32_t>(NUM_TEMPI)) {
        return -1;
    }
    return static_cast<int16_t>(idx);
}

inline float esv11_pick_local_bin_magnitude(int16_t center_bin, uint8_t radius = 1) {
    if (center_bin < 0 || center_bin >= static_cast<int16_t>(NUM_TEMPI)) {
        return 0.0f;
    }
    float best = 0.0f;
    const int16_t r = static_cast<int16_t>(radius);
    for (int16_t o = -r; o <= r; ++o) {
        const int16_t idx = static_cast<int16_t>(center_bin + o);
        if (idx >= 0 && idx < static_cast<int16_t>(NUM_TEMPI)) {
            best = fmaxf(best, tempi_smooth[idx]);
        }
    }
    return best;
}

inline uint16_t esv11_find_peak_bin_in_bpm_window(float bpm_min, float bpm_max, uint8_t radius, float* out_mag = nullptr) {
    int16_t start_bin = esv11_bpm_to_bin(bpm_min);
    int16_t end_bin = esv11_bpm_to_bin(bpm_max);

    if (start_bin < 0) start_bin = 0;
    if (end_bin < 0) end_bin = static_cast<int16_t>(NUM_TEMPI - 1);
    if (end_bin >= static_cast<int16_t>(NUM_TEMPI)) end_bin = static_cast<int16_t>(NUM_TEMPI - 1);
    if (end_bin < start_bin) {
        const int16_t swap_tmp = start_bin;
        start_bin = end_bin;
        end_bin = swap_tmp;
    }

    uint16_t best_bin = static_cast<uint16_t>(start_bin);
    float best_mag = 0.0f;
    for (int16_t bin = start_bin; bin <= end_bin; ++bin) {
        const float mag = esv11_pick_local_bin_magnitude(bin, radius);
        if (mag > best_mag) {
            best_mag = mag;
            best_bin = static_cast<uint16_t>(bin);
        }
    }

    if (out_mag) {
        *out_mag = best_mag;
    }
    return best_bin;
}

inline uint16_t esv11_pick_top_tempo_bin_octave_aware() {
    const uint16_t raw_bin = esv11_pick_top_tempo_bin_raw();
    const float raw_mag = fmaxf(tempi_smooth[raw_bin], 1e-6f);
    const float raw_bpm = static_cast<float>(TEMPO_LOW + raw_bin);

    uint16_t selected_bin = raw_bin;
    float selected_score = raw_mag;

    const int16_t double_bin = esv11_bpm_to_bin(raw_bpm * 2.0f);
    if (double_bin >= 0) {
        // Favour musical tactus over half-time aliases when the raw winner
        // lands in sub-80 BPM territory and confidence indicates active music.
        if (raw_bpm < 80.0f && tempo_confidence > 0.12f) {
            selected_bin = static_cast<uint16_t>(double_bin);
            selected_score = esv11_pick_local_bin_magnitude(double_bin, 1);
        }

        const float double_mag = esv11_pick_local_bin_magnitude(double_bin, 1);
        const float ratio = double_mag / raw_mag;
        const float ratio_threshold = (raw_bpm <= 72.0f) ? 0.56f : 0.72f;
        if (ratio >= ratio_threshold && double_mag > selected_score) {
            selected_bin = static_cast<uint16_t>(double_bin);
            selected_score = double_mag;
        }
    }

    // Edge rebound: when the raw winner is pinned near bank ceiling, recover
    // tactus candidates around 78-84 BPM if they carry most of the raw energy.
    if (raw_bpm >= 138.0f && tempo_confidence < 0.35f) {
        float rebound_mag = 0.0f;
        const uint16_t rebound_bin = esv11_find_peak_bin_in_bpm_window(76.0f, 84.0f, 1, &rebound_mag);
        if (rebound_mag >= raw_mag * 0.70f) {
            selected_bin = rebound_bin;
            selected_score = rebound_mag;
        }
    }

    // 210-BPM alias rescue: tracks can surface a strong ~133 BPM surrogate while
    // retaining secondary tactus energy near 105 BPM; prefer that metrical anchor.
    if (raw_bpm >= 128.0f && raw_bpm <= 136.0f && tempo_confidence < 0.32f) {
        float rescue_mag = 0.0f;
        const uint16_t rescue_bin = esv11_find_peak_bin_in_bpm_window(102.0f, 108.0f, 1, &rescue_mag);
        if (rescue_mag >= 0.09f && rescue_mag >= raw_mag * 0.10f && rescue_mag > selected_score * 0.80f) {
            selected_bin = rescue_bin;
            selected_score = rescue_mag;
        }
    }

    if (raw_bpm >= 132.0f) {
        const int16_t half_bin = esv11_bpm_to_bin(raw_bpm * 0.5f);
        if (half_bin >= 0) {
            const float half_mag = esv11_pick_local_bin_magnitude(half_bin, 1);
            if (half_mag >= selected_score * 0.92f) {
                selected_bin = static_cast<uint16_t>(half_bin);
                selected_score = half_mag;
            }
        }
    }

    return selected_bin;
}
