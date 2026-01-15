/**
 * @file ShowTranslator.cpp
 * @brief Implementation of ShowTranslator
 */

#include "ShowTranslator.h"
#include <cstring>
#include <cstdlib>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace shows {

// ============================================================================
// Effect Name ↔ ID Conversion
// ============================================================================

uint8_t ShowTranslator::getEffectIdByName(const char* effectName) {
    if (!effectName || effectName[0] == '\0') {
        return 0xFF;
    }

    // Search through PatternRegistry
    uint8_t count = PatternRegistry::getPatternCount();
    for (uint8_t i = 0; i < count; i++) {
        const PatternMetadata* meta = PatternRegistry::getPatternMetadata(i);
        if (!meta) continue;

        // Read name from PROGMEM
        char name[32];
        strncpy_P(name, meta->name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';

        if (strcmp(name, effectName) == 0) {
            return i;
        }
    }

    return 0xFF;  // Not found
}

bool ShowTranslator::getEffectNameById(uint8_t effectId, char* outName, size_t maxLen) {
    if (!outName || maxLen == 0) {
        return false;
    }

    const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
    if (!meta) {
        return false;
    }

    // Copy name from PROGMEM
    strncpy_P(outName, meta->name, maxLen - 1);
    outName[maxLen - 1] = '\0';
    return true;
}

void ShowTranslator::getZoneColor(uint8_t zoneId, char* outColor, size_t maxLen) {
    if (!outColor || maxLen == 0) {
        return;
    }

    // Zone color mapping (matches dashboard)
    const char* colors[] = {
        "primary",           // 0 = Global
        "accent-cyan",       // 1 = Zone 1
        "accent-green",      // 2 = Zone 2
        "text-secondary",    // 3 = Zone 3
        "primary"            // 4 = Zone 4
    };

    if (zoneId < 5) {
        strncpy(outColor, colors[zoneId], maxLen - 1);
    } else {
        strncpy(outColor, "primary", maxLen - 1);
    }
    outColor[maxLen - 1] = '\0';
}

void ShowTranslator::generateSceneId(uint8_t index, char* outId, size_t maxLen) {
    if (!outId || maxLen == 0) {
        return;
    }
    snprintf(outId, maxLen, "scene-%u", index);
}

// ============================================================================
// Comparison Functions
// ============================================================================

int ShowTranslator::compareScenesByTime(const void* a, const void* b) {
    const TimelineScene* sceneA = static_cast<const TimelineScene*>(a);
    const TimelineScene* sceneB = static_cast<const TimelineScene*>(b);
    
    if (sceneA->startTimePercent < sceneB->startTimePercent) return -1;
    if (sceneA->startTimePercent > sceneB->startTimePercent) return 1;
    return 0;
}

int ShowTranslator::compareCuesByTime(const void* a, const void* b) {
    const ShowCue* cueA = static_cast<const ShowCue*>(a);
    const ShowCue* cueB = static_cast<const ShowCue*>(b);
    
    if (cueA->timeMs < cueB->timeMs) return -1;
    if (cueA->timeMs > cueB->timeMs) return 1;
    return 0;
}

// ============================================================================
// Scene → Cue Conversion
// ============================================================================

bool ShowTranslator::scenesToCues(
    const TimelineScene* scenes,
    uint8_t sceneCount,
    uint32_t totalDurationMs,
    ShowCue* outCues,
    uint8_t& outCueCount,
    uint8_t maxCues
) {
    if (!scenes || !outCues || sceneCount == 0 || maxCues == 0) {
        outCueCount = 0;
        return false;
    }

    outCueCount = 0;

    // Sort scenes by start time (create temporary copy for sorting)
    TimelineScene sorted[MAX_SCENES];
    if (sceneCount > MAX_SCENES) {
        sceneCount = MAX_SCENES;  // Clamp to max
    }
    memcpy(sorted, scenes, sizeof(TimelineScene) * sceneCount);
    qsort(sorted, sceneCount, sizeof(TimelineScene), compareScenesByTime);

    // Convert each scene to cues
    for (uint8_t i = 0; i < sceneCount && outCueCount < maxCues - 1; i++) {
        const TimelineScene& scene = sorted[i];

        // Calculate absolute times
        uint32_t startMs = percentToMs(scene.startTimePercent, totalDurationMs);
        uint32_t endMs = startMs + percentToMs(scene.durationPercent, totalDurationMs);

        // Convert zone
        uint8_t firmwareZone = uiZoneToFirmware(scene.zoneId);

        // Get effect ID from name
        uint8_t effectId = scene.effectId;
        if (effectId == 0xFF) {
            effectId = getEffectIdByName(scene.effectName);
        }
        if (effectId == 0xFF) {
            continue;  // Skip invalid effects
        }

        // Create effect cue at scene start
        ShowCue& cue = outCues[outCueCount++];
        cue.timeMs = startMs;
        cue.type = CUE_EFFECT;
        cue.targetZone = firmwareZone;
        cue.data[0] = effectId;
        cue.data[1] = 0;  // Default transition (no transition)
        cue.data[2] = 0;
        cue.data[3] = 0;

        // Optional: Add marker cue at scene end (if space available)
        // This helps with cue → scene conversion later
        if (outCueCount < maxCues && endMs < totalDurationMs) {
            ShowCue& marker = outCues[outCueCount++];
            marker.timeMs = endMs;
            marker.type = CUE_MARKER;
            marker.targetZone = firmwareZone;
            marker.data[0] = 0;
            marker.data[1] = 0;
            marker.data[2] = 0;
            marker.data[3] = 0;
        }
    }

    // Sort cues by time (in case markers were added)
    qsort(outCues, outCueCount, sizeof(ShowCue), compareCuesByTime);

    // Fill gaps: If there are gaps between scenes, add default effect cues
    // This ensures the firmware always has an effect active
    if (outCueCount > 0 && outCueCount < maxCues) {
        // Check for gap at start
        if (outCues[0].timeMs > 0) {
            // Add default effect at time 0
            if (outCueCount < maxCues) {
                // Shift existing cues
                memmove(&outCues[1], &outCues[0], sizeof(ShowCue) * outCueCount);
                ShowCue& defaultCue = outCues[0];
                defaultCue.timeMs = 0;
                defaultCue.type = CUE_EFFECT;
                defaultCue.targetZone = ZONE_GLOBAL;
                defaultCue.data[0] = 0;  // Effect 0 (Fire)
                defaultCue.data[1] = 0;
                defaultCue.data[2] = 0;
                defaultCue.data[3] = 0;
                outCueCount++;
            }
        }

        // Check for gaps between cues (simplified - only check major gaps)
        // Full implementation would scan all gaps and fill them
    }

    return outCueCount > 0;
}

// ============================================================================
// Cue → Scene Conversion
// ============================================================================

bool ShowTranslator::cuesToScenes(
    const ShowCue* cues,
    uint8_t cueCount,
    uint32_t totalDurationMs,
    TimelineScene* outScenes,
    uint8_t& outSceneCount,
    uint8_t maxScenes
) {
    if (!cues || !outScenes || cueCount == 0 || maxScenes == 0) {
        outSceneCount = 0;
        return false;
    }

    outSceneCount = 0;

    // Group consecutive CUE_EFFECT cues with same effect and zone into scenes
    uint8_t i = 0;
    while (i < cueCount && outSceneCount < maxScenes) {
        // Find next CUE_EFFECT
        while (i < cueCount && cues[i].type != CUE_EFFECT) {
            i++;
        }
        if (i >= cueCount) break;

        const ShowCue& startCue = cues[i];
        uint8_t effectId = startCue.effectId();
        uint8_t zone = startCue.targetZone;
        uint32_t sceneStartMs = startCue.timeMs;

        // Find scene end: next CUE_EFFECT with different effect/zone, or CUE_MARKER, or show end
        uint32_t sceneEndMs = totalDurationMs;
        uint8_t j = i + 1;
        while (j < cueCount) {
            if (cues[j].type == CUE_EFFECT) {
                if (cues[j].effectId() != effectId || cues[j].targetZone != zone) {
                    sceneEndMs = cues[j].timeMs;
                    break;
                }
            } else if (cues[j].type == CUE_MARKER && cues[j].targetZone == zone) {
                sceneEndMs = cues[j].timeMs;
                break;
            }
            j++;
        }

        // Create scene
        TimelineScene& scene = outScenes[outSceneCount++];
        generateSceneId(outSceneCount - 1, scene.id, sizeof(scene.id));
        scene.zoneId = firmwareZoneToUi(zone);
        scene.effectId = effectId;
        
        // Get effect name
        if (!getEffectNameById(effectId, scene.effectName, sizeof(scene.effectName))) {
            snprintf(scene.effectName, sizeof(scene.effectName), "Effect %u", effectId);
        }

        scene.startTimePercent = msToPercent(sceneStartMs, totalDurationMs);
        uint32_t durationMs = sceneEndMs - sceneStartMs;
        scene.durationPercent = msToPercent(durationMs, totalDurationMs);

        // Get zone color
        getZoneColor(scene.zoneId, scene.accentColor, sizeof(scene.accentColor));

        i = j;
    }

    return outSceneCount > 0;
}

} // namespace shows
} // namespace lightwaveos

