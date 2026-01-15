/**
 * @file GoertzelBank.h
 * @brief Goertzel bank with group-by-N processing
 *
 * CRITICAL: Processes bins in groups by N to share history/window operations.
 * CopyLast + window once per N per tick, then run kernel for all bins with that N.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "K1Types.h"
#include "AudioRingBuffer.h"
#include "WindowBank.h"
#include "BinGroups.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Goertzel bank with group processing
 *
 * Efficiently processes multiple bins by grouping those with the same N.
 */
class GoertzelBank {
public:
    /**
     * @brief Constructor
     */
    GoertzelBank();

    /**
     * @brief Destructor
     */
    ~GoertzelBank();

    /**
     * @brief Initialize bank from bin specifications
     *
     * @param specs Array of GoertzelBinSpec
     * @param num_bins Number of bins
     * @param windowBank Window bank for LUT lookup
     * @return true if successful
     */
    bool init(const GoertzelBinSpec* specs, size_t num_bins, const WindowBank* windowBank);

    /**
     * @brief Process all bins using group-by-N optimization
     *
     * CRITICAL: CopyLast + window once per N, then run kernel for all bins with that N.
     *
     * @param ring Ring buffer for history access
     * @param out_mags Output magnitude array (must have space for num_bins)
     */
    void processAll(const AudioRingBuffer& ring, float* out_mags);

private:
    const GoertzelBinSpec* m_specs;      ///< Bin specifications
    size_t m_num_bins;                   ///< Number of bins
    const WindowBank* m_windowBank;      ///< Window bank for LUT lookup
    BinGroups m_groups;                   ///< Bin groups by N
    int16_t* m_scratch;                   ///< Scratch buffer for windowed samples (size N_MAX)
    bool m_initialized;                   ///< Initialization flag

    /**
     * @brief Process a single group
     *
     * CopyLast + window once, then run kernel for all bins in group.
     *
     * @param group Bin group to process
     * @param ring Ring buffer for history access
     * @param out_mags Output magnitude array
     */
    void processGroup(const BinGroup& group, const AudioRingBuffer& ring, float* out_mags);
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

