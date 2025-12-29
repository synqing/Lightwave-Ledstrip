/**
 * @file K1ResonatorBank.cpp
 * @brief K1-Lightwave Stage 2: Goertzel Resonator Bank Implementation
 *
 * Ported from Tab5.DSP for ESP32-S3 Lightwave v2 integration.
 */

#include "K1ResonatorBank.h"
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace audio {
namespace k1 {

void K1ResonatorBank::begin(uint32_t now_ms) {
    last_update_ms_ = now_ms;
    updates_ = 0;
    frame_counter_ = 0;

    // Clear novelty history
    novelty_history_.clear();

    // Precompute Gaussian window (sigma=0.6 for good low-tempo detection)
    float sigma = 0.6f;
    for (int i = 0; i < ST2_HISTORY_FRAMES; ++i) {
        float n_minus_half = (float)i - (float)ST2_HISTORY_FRAMES / 2.0f;
        float gaussian = expf(-0.5f * powf(n_minus_half / (sigma * ST2_HISTORY_FRAMES / 2.0f), 2.0f));
        window_[i] = gaussian;
    }

    // Initialize Goertzel bins
    for (int bi = 0; bi < ST2_BPM_BINS; ++bi) {
        float bpm = (float)(ST2_BPM_MIN + bi * ST2_BPM_STEP);
        float hz = bpm / 60.0f;

        bins_[bi].target_hz = hz;
        bins_[bi].block_size = ST2_HISTORY_FRAMES;
        bins_[bi].magnitude_raw = 0.0f;
        bins_[bi].magnitude_smooth = 0.0f;
        bins_[bi].phase = 0.0f;
        bins_[bi].coeff = 0.0f;
        bins_[bi].cosine = 0.0f;
        bins_[bi].sine = 0.0f;
        bins_[bi].window_step = 1.0f;
    }
}

void K1ResonatorBank::computeBin(int bin_idx) {
    K1GoertzelBin& bin = bins_[bin_idx];

    int N = novelty_history_.size();

    // Require minimum history for meaningful frequency resolution
    static constexpr int MIN_HISTORY = 125;  // ~2 seconds at 62.5 Hz
    if (N < MIN_HISTORY) {
        bin.magnitude_raw = 0.0f;
        bin.phase = 0.0f;
        return;
    }

    int use_n = (N < bin.block_size) ? N : bin.block_size;

    // Compute Goertzel coefficients based on actual block size
    float hz = bin.target_hz;
    float k = (float)use_n * hz / K1_NOVELTY_FS;
    float w = 2.0f * (float)M_PI * k / (float)use_n;
    float cosine = cosf(w);
    float sine = sinf(w);
    float coeff = 2.0f * cosine;

    // Goertzel algorithm
    float q1 = 0.0f, q2 = 0.0f;
    float win_pos = 0.0f;
    float win_step = (float)ST2_HISTORY_FRAMES / (float)use_n;

    for (int i = 0; i < use_n; ++i) {
        float sample = novelty_history_.get(use_n - 1 - i);

        // Apply window with linear interpolation
        int win_idx0 = (int)win_pos;
        int win_idx1 = win_idx0 + 1;
        if (win_idx0 >= ST2_HISTORY_FRAMES - 1) {
            win_idx0 = ST2_HISTORY_FRAMES - 1;
            win_idx1 = ST2_HISTORY_FRAMES - 1;
        }
        float frac = win_pos - (float)win_idx0;
        float win_val = window_[win_idx0] * (1.0f - frac) + window_[win_idx1] * frac;
        float windowed = sample * win_val;

        float q0 = coeff * q1 - q2 + windowed;
        q2 = q1;
        q1 = q0;

        win_pos += win_step;
    }

    // Compute magnitude and phase
    float real = q1 - q2 * cosine;
    float imag = q2 * sine;

    float mag_sq = real * real + imag * imag;
    float mag = sqrtf(mag_sq);

    bin.magnitude_raw = mag / ((float)use_n / 2.0f);
    bin.phase = atan2f(imag, real);

    // Smooth magnitude with EMA
    bin.magnitude_smooth = ST2_MAG_SMOOTH * bin.magnitude_smooth
                         + (1.0f - ST2_MAG_SMOOTH) * bin.magnitude_raw;
}

float K1ResonatorBank::refineBpm(int peak_bin) const {
    if (peak_bin <= 0 || peak_bin >= ST2_BPM_BINS - 1) {
        return (float)(ST2_BPM_MIN + peak_bin * ST2_BPM_STEP);
    }

    float sL = bins_[peak_bin - 1].magnitude_smooth;
    float s0 = bins_[peak_bin].magnitude_smooth;
    float sR = bins_[peak_bin + 1].magnitude_smooth;

    float denom = sL - 2.0f * s0 + sR;
    if (fabsf(denom) < 1e-6f) {
        return (float)(ST2_BPM_MIN + peak_bin * ST2_BPM_STEP);
    }

    float offset = 0.5f * (sL - sR) / denom;
    if (offset < -0.5f) offset = -0.5f;
    if (offset > 0.5f) offset = 0.5f;

    return (float)ST2_BPM_MIN + ((float)peak_bin + offset) * (float)ST2_BPM_STEP;
}

void K1ResonatorBank::extractTopK(K1ResonatorFrame& out) {
    out.k = ST2_TOPK;

    // Initialize with valid default BPMs
    for (int i = 0; i < ST2_TOPK; ++i) {
        out.candidates[i].bpm = (float)(ST2_BPM_MIN + i * 10);
        out.candidates[i].magnitude = 0.0f;
        out.candidates[i].phase = 0.0f;
        out.candidates[i].raw_mag = 0.0f;
    }

    // Find max for normalization
    float max_mag = 0.0f;
    for (int bi = 0; bi < ST2_BPM_BINS; ++bi) {
        if (bins_[bi].magnitude_smooth > max_mag) {
            max_mag = bins_[bi].magnitude_smooth;
        }
        out.spectrum[bi] = bins_[bi].magnitude_smooth;
    }

    if (max_mag < 1e-9f) {
        return;  // Silence - keep defaults
    }

    // Extract top-K by insertion sort
    for (int bi = 0; bi < ST2_BPM_BINS; ++bi) {
        float mag_norm = bins_[bi].magnitude_smooth / max_mag;

        for (int k = 0; k < ST2_TOPK; ++k) {
            if (mag_norm > out.candidates[k].magnitude ||
                (k == 0 && out.candidates[k].raw_mag == 0.0f)) {
                // Shift down
                for (int j = ST2_TOPK - 1; j > k; --j) {
                    out.candidates[j] = out.candidates[j - 1];
                }
                // Insert
                out.candidates[k].bpm = refineBpm(bi);
                out.candidates[k].magnitude = mag_norm;
                out.candidates[k].phase = bins_[bi].phase;
                out.candidates[k].raw_mag = bins_[bi].magnitude_raw;
                break;
            }
        }
    }
}

bool K1ResonatorBank::update(float novelty_z, uint32_t t_ms, K1ResonatorFrame& out) {
    // Push novelty to history
    novelty_history_.push(novelty_z);

    // Count frames for update cadence
    frame_counter_++;

    int update_every = (int)(K1_NOVELTY_FS / (float)ST2_UPDATE_HZ);
    if (update_every < 1) update_every = 1;

    if ((frame_counter_ % update_every) != 0) {
        return false;  // Not time to update yet
    }

    last_update_ms_ = t_ms;

    // Compute all bins
    for (int bi = 0; bi < ST2_BPM_BINS; ++bi) {
        computeBin(bi);
    }

    // Extract top-K candidates
    extractTopK(out);
    out.t_ms = t_ms;

    updates_++;
    return true;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos
