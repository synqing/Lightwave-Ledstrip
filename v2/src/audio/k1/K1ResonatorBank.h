/**
 * @file K1ResonatorBank.h
 * @brief K1-Lightwave Stage 2: Goertzel Resonator Bank
 *
 * Analyzes continuous novelty signal for periodic content using 121 Goertzel
 * resonators (60-180 BPM at 1 BPM resolution).
 *
 * Ported from Tab5.DSP for ESP32-S3 Lightwave v2 integration.
 */

#pragma once

#include <stdint.h>
#include "K1Config.h"
#include "K1Types.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

class K1ResonatorBank {
public:
    void begin(uint32_t now_ms);

    /**
     * @brief Update from novelty input
     * @param novelty_z Z-score novelty value [-6, +6]
     * @param t_ms Current timestamp
     * @param out Output resonator frame (filled when update triggered)
     * @return true when ResonatorFrame is produced (at ST2_UPDATE_HZ)
     */
    bool update(float novelty_z, uint32_t t_ms, K1ResonatorFrame& out);

    // Access to resonator state (for debugging/visualization)
    const K1GoertzelBin& getBin(int idx) const { return bins_[idx]; }

    // Access to novelty history
    const K1RingBuffer<float, ST2_HISTORY_FRAMES>& getNoveltyHistory() const {
        return novelty_history_;
    }

    // Stats
    uint32_t updates() const { return updates_; }

private:
    void computeBin(int bin_idx);
    void extractTopK(K1ResonatorFrame& out);
    float refineBpm(int peak_bin) const;

private:
    // Goertzel bins
    K1GoertzelBin bins_[ST2_BPM_BINS];

    // Window lookup (Gaussian)
    float window_[ST2_HISTORY_FRAMES];

    // Novelty history buffer
    K1RingBuffer<float, ST2_HISTORY_FRAMES> novelty_history_;

    // Timing
    uint32_t last_update_ms_ = 0;
    uint32_t updates_ = 0;
    int      frame_counter_ = 0;
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos
