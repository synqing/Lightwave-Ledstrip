/**
 * @file ShowTranslator.h
 * @brief Translation layer between UI scene-based shows and firmware cue-based shows
 *
 * LightwaveOS v2 - Show System Bridge
 *
 * Provides bidirectional conversion between:
 * - Dashboard: TimelineScene[] (continuous blocks with percentages)
 * - Firmware: ShowCue[] (discrete time-stamped actions)
 *
 * This enables the drag-and-drop timeline editor to create shows that the
 * firmware can execute, and allows the firmware to export its shows in
 * a format the UI can display and edit.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "ShowTypes.h"
#include "../../effects/PatternRegistry.h"
#include <stdint.h>
#include <Arduino.h>

namespace lightwaveos {
namespace shows {

// ============================================================================
// Timeline Scene (UI Model)
// ============================================================================
// Matches dashboard TimelineScene interface

struct TimelineScene {
    char id[32];                    // Unique identifier (e.g., "scene-1")
    uint8_t zoneId;                 // 0=Global, 1-4=Zones (UI format)
    char effectName[32];            // Effect display name
    float startTimePercent;         // 0-100 position
    float durationPercent;          // 0-100 width
    char accentColor[16];           // Color class (e.g., "accent-cyan")
    
    // Optional: Effect ID (for faster lookup, set during conversion)
    uint8_t effectId;                // Effect ID (0xFF = invalid)
    
    TimelineScene() 
        : zoneId(0)
        , startTimePercent(0.0f)
        , durationPercent(0.0f)
        , effectId(0xFF)
    {
        id[0] = '\0';
        effectName[0] = '\0';
        accentColor[0] = '\0';
    }
};

// ============================================================================
// Show Translator
// ============================================================================

/**
 * @brief Translates between UI scene model and firmware cue model
 */
class ShowTranslator {
public:
    // Maximum scenes/cues for safety
    static constexpr uint8_t MAX_SCENES = 50;
    static constexpr uint8_t MAX_CUES = 100;

    // ========================================================================
    // UI → Firmware Conversion
    // ========================================================================

    /**
     * @brief Convert TimelineScene array to ShowCue array
     *
     * Converts continuous scene blocks into discrete time-stamped cues.
     * Each scene generates at least one CUE_EFFECT at its start time.
     *
     * @param scenes Array of scenes to convert
     * @param sceneCount Number of scenes
     * @param totalDurationMs Total show duration in milliseconds
     * @param outCues Output array for cues (must be pre-allocated)
     * @param outCueCount Output: number of cues generated
     * @param maxCues Maximum cues that can be written to outCues
     * @return true if conversion successful, false on error
     */
    static bool scenesToCues(
        const TimelineScene* scenes,
        uint8_t sceneCount,
        uint32_t totalDurationMs,
        ShowCue* outCues,
        uint8_t& outCueCount,
        uint8_t maxCues
    );

    // ========================================================================
    // Firmware → UI Conversion
    // ========================================================================

    /**
     * @brief Convert ShowCue array to TimelineScene array
     *
     * Groups consecutive CUE_EFFECT cues with the same effect and zone
     * into continuous scene blocks.
     *
     * @param cues Array of cues to convert
     * @param cueCount Number of cues
     * @param totalDurationMs Total show duration in milliseconds
     * @param outScenes Output array for scenes (must be pre-allocated)
     * @param outSceneCount Output: number of scenes generated
     * @param maxScenes Maximum scenes that can be written to outScenes
     * @return true if conversion successful, false on error
     */
    static bool cuesToScenes(
        const ShowCue* cues,
        uint8_t cueCount,
        uint32_t totalDurationMs,
        TimelineScene* outScenes,
        uint8_t& outSceneCount,
        uint8_t maxScenes
    );

    // ========================================================================
    // Time Conversion
    // ========================================================================

    /**
     * @brief Convert percentage to milliseconds
     * @param percent Percentage (0-100)
     * @param totalDurationMs Total duration in milliseconds
     * @return Time in milliseconds
     */
    static uint32_t percentToMs(float percent, uint32_t totalDurationMs) {
        return (uint32_t)((percent / 100.0f) * totalDurationMs);
    }

    /**
     * @brief Convert milliseconds to percentage
     * @param timeMs Time in milliseconds
     * @param totalDurationMs Total duration in milliseconds
     * @return Percentage (0-100)
     */
    static float msToPercent(uint32_t timeMs, uint32_t totalDurationMs) {
        if (totalDurationMs == 0) return 0.0f;
        return (float)timeMs * 100.0f / totalDurationMs;
    }

    // ========================================================================
    // Zone Conversion
    // ========================================================================

    /**
     * @brief Convert UI zone ID to firmware zone ID
     * @param uiZoneId UI zone (0=Global, 1-4=Zones)
     * @return Firmware zone (0xFF=Global, 0-3=Zones)
     */
    static uint8_t uiZoneToFirmware(uint8_t uiZoneId) {
        if (uiZoneId == 0) return ZONE_GLOBAL;  // 0xFF
        if (uiZoneId >= 1 && uiZoneId <= 4) return uiZoneId - 1;  // 1-4 → 0-3
        return ZONE_GLOBAL;  // Default to global
    }

    /**
     * @brief Convert firmware zone ID to UI zone ID
     * @param firmwareZoneId Firmware zone (0xFF=Global, 0-3=Zones)
     * @return UI zone (0=Global, 1-4=Zones)
     */
    static uint8_t firmwareZoneToUi(uint8_t firmwareZoneId) {
        if (firmwareZoneId == ZONE_GLOBAL) return 0;  // 0xFF → 0
        if (firmwareZoneId < 4) return firmwareZoneId + 1;  // 0-3 → 1-4
        return 0;  // Default to global
    }

    // ========================================================================
    // Effect Name ↔ ID Conversion
    // ========================================================================

    /**
     * @brief Get effect ID from effect name
     * @param effectName Effect name (case-sensitive)
     * @return Effect ID, or 0xFF if not found
     */
    static uint8_t getEffectIdByName(const char* effectName);

    /**
     * @brief Get effect name from effect ID
     * @param effectId Effect ID
     * @param outName Output buffer (must be at least 32 bytes)
     * @param maxLen Maximum length of output buffer
     * @return true if found, false otherwise
     */
    static bool getEffectNameById(uint8_t effectId, char* outName, size_t maxLen);

    /**
     * @brief Get zone color class for UI
     * @param zoneId UI zone ID (0-4)
     * @param outColor Output buffer (must be at least 16 bytes)
     * @param maxLen Maximum length of output buffer
     */
    static void getZoneColor(uint8_t zoneId, char* outColor, size_t maxLen);

    /**
     * @brief Generate unique scene ID
     * @param index Scene index
     * @param outId Output buffer (must be at least 32 bytes)
     * @param maxLen Maximum length of output buffer
     */
    static void generateSceneId(uint8_t index, char* outId, size_t maxLen);

private:
    // Comparison functions for sorting
    static int compareScenesByTime(const void* a, const void* b);
    static int compareCuesByTime(const void* a, const void* b);
};

} // namespace shows
} // namespace lightwaveos

