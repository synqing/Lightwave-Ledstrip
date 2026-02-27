/**
 * @file ParameterSweeper.h
 * @brief Manages concurrent parameter interpolations for smooth show transitions
 *
 * LightwaveOS v2 - Show System
 *
 * Supports up to 8 simultaneous sweeps across different parameters/zones.
 * RAM: ~80 bytes (8 ActiveSweep * 10 bytes each)
 */

#pragma once

#include <Arduino.h>
#include "ShowTypes.h"

namespace lightwaveos {
namespace shows {

/**
 * @brief Callback function type for applying parameter values
 * @param param Parameter ID
 * @param zone Zone ID (ZONE_GLOBAL = all zones)
 * @param value Parameter value (0-255)
 */
using ParamApplyCallback = void (*)(ParamId param, uint8_t zone, uint8_t value);

/**
 * @brief Callback function type for getting current parameter values
 * @param param Parameter ID
 * @param zone Zone ID
 * @return Current parameter value (0-255)
 */
using ParamGetCallback = uint8_t (*)(ParamId param, uint8_t zone);

/**
 * @brief Manages concurrent parameter interpolations
 */
class ParameterSweeper {
public:
    static constexpr uint8_t MAX_SWEEPS = 8;

    /**
     * @brief Construct ParameterSweeper with callbacks
     * @param applyCallback Function to call when applying parameter values
     * @param getCallback Function to call when getting current parameter values
     */
    ParameterSweeper(ParamApplyCallback applyCallback, ParamGetCallback getCallback);

    // Start a new sweep
    // Returns true if sweep started, false if no slots available
    bool startSweep(ParamId param, uint8_t zone, uint8_t startVal, uint8_t targetVal, uint16_t durationMs);

    // Convenience: start sweep from current value
    bool startSweepFromCurrent(ParamId param, uint8_t zone, uint8_t targetVal, uint16_t durationMs);

    // Update all active sweeps and apply values
    // Should be called once per frame
    void update(uint32_t currentTimeMs);

    // Cancel all active sweeps
    void cancelAll();

    // Cancel sweeps for specific parameter
    void cancelParam(ParamId param);

    // Cancel sweeps for specific zone
    void cancelZone(uint8_t zone);

    // Get number of active sweeps
    uint8_t activeSweepCount() const;

    // Check if any sweeps are active
    bool hasActiveSweeps() const { return activeSweepCount() > 0; }

private:
    ActiveSweep m_sweeps[MAX_SWEEPS];
    ParamApplyCallback m_applyCallback;
    ParamGetCallback m_getCallback;

    // Apply interpolated value to the appropriate parameter
    void applyValue(ParamId param, uint8_t zone, uint8_t value);

    // Get current value of a parameter (for startSweepFromCurrent)
    uint8_t getCurrentParamValue(ParamId param, uint8_t zone);

    // Find an available sweep slot (-1 if none)
    int8_t findFreeSlot() const;

    // Find existing sweep for param/zone (-1 if none)
    int8_t findSweep(ParamId param, uint8_t zone) const;
};

} // namespace shows
} // namespace lightwaveos

