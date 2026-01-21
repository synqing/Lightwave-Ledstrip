/**
 * @file WindowBank.h
 * @brief Per-N window LUT bank (no slicing from single 1536 LUT)
 *
 * CRITICAL: Each unique N gets its own Hann window LUT.
 * Do NOT slice N from a 1536 LUT - that produces incorrect windows.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "K1Spec.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Window bank providing per-N Hann window LUTs
 *
 * Builds separate Hann Q15 LUT for each unique N value.
 * Provides O(1) lookup for window coefficients.
 */
class WindowBank {
public:
    /**
     * @brief Constructor
     */
    WindowBank();

    /**
     * @brief Destructor
     */
    ~WindowBank();

    /**
     * @brief Initialize window bank with unique N values
     *
     * Builds Hann Q15 LUT for each unique N.
     *
     * @param unique_N Array of unique N values (must be sorted, ascending)
     * @param count Number of unique N values
     * @return true if successful, false on failure
     */
    bool init(const uint16_t* unique_N, size_t count);

    /**
     * @brief Get Hann window LUT for specific N
     *
     * @param N Window length (must be one of the unique N values)
     * @return Pointer to Q15 window LUT (length N), or nullptr if N not found
     */
    const int16_t* getHannQ15(uint16_t N) const;

    /**
     * @brief Get normalization factor for specific N
     *
     * Normalization factor accounts for window energy and N scaling.
     *
     * @param N Window length
     * @return Normalization factor, or 0.0f if N not found
     */
    float getNormFactor(uint16_t N) const;

    /**
     * @brief Check if window bank is initialized
     */
    bool isInitialized() const { return m_initialized; }

private:
    struct WindowEntry {
        uint16_t N;                      ///< Window length
        int16_t* lut;                    ///< Q15 Hann window LUT (length N)
        float normFactor;                ///< Normalization factor (Ew * N)
    };

    WindowEntry* m_entries;              ///< Array of window entries
    size_t m_count;                      ///< Number of unique N values
    bool m_initialized;                  ///< Initialization flag

    /**
     * @brief Build Hann Q15 LUT for specific N
     *
     * Formula: w[n] = 0.5 * (1 - cos(2πn/(N-1)))
     * Clamped to [0..32767] (Q15)
     *
     * @param N Window length
     * @param out_lut Output buffer (must have space for N samples)
     */
    void buildHannQ15(uint16_t N, int16_t* out_lut) const;

    /**
     * @brief Compute normalization factor for window
     *
     * Computes Ew = Σ(w[n]^2) and returns normFactor = 1.0f / (Ew * N)
     *
     * @param lut Q15 window LUT
     * @param N Window length
     * @return Normalization factor
     */
    float computeNormFactor(const int16_t* lut, uint16_t N) const;
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

