#ifndef PARAMETER_SWEEPER_H
#define PARAMETER_SWEEPER_H

#include <Arduino.h>
#include "ShowTypes.h"

// ============================================================================
// PARAMETER SWEEPER
// ============================================================================
// Manages concurrent parameter interpolations for smooth show transitions.
// Supports up to 8 simultaneous sweeps across different parameters/zones.
//
// RAM: ~80 bytes (8 ActiveSweep * 10 bytes each)
// ============================================================================

class ParameterSweeper {
public:
    static constexpr uint8_t MAX_SWEEPS = 8;

    ParameterSweeper();

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

    // Apply interpolated value to the appropriate parameter
    void applyValue(ParamId param, uint8_t zone, uint8_t value);

    // Get current value of a parameter (for startSweepFromCurrent)
    uint8_t getCurrentParamValue(ParamId param, uint8_t zone);

    // Find an available sweep slot (-1 if none)
    int8_t findFreeSlot() const;

    // Find existing sweep for param/zone (-1 if none)
    int8_t findSweep(ParamId param, uint8_t zone) const;
};

#endif // PARAMETER_SWEEPER_H
