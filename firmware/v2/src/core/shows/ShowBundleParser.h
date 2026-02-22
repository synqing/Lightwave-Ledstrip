/**
 * @file ShowBundleParser.h
 * @brief Parses ShowBundle JSON into runtime DynamicShowData
 *
 * Accepts JSON conforming to SHOWBUNDLE_SCHEMA v0.1 and produces
 * a fully populated DynamicShowData struct allocated in PSRAM.
 *
 * Design constraints:
 * - Single-pass streaming parse via ArduinoJson
 * - No heap allocation beyond the final PSRAM block
 * - Validates all fields and ranges before committing
 * - Cues must be pre-sorted by timeMs (validated, not re-sorted)
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>
#include <algorithm>

#include "DynamicShowStore.h"
#include "ShowTypes.h"

namespace prism {

/**
 * @brief Result of a ShowBundle parse operation
 */
struct ParseResult {
    bool success;
    const char* errorMessage;    // Static string, no allocation
    uint16_t cueCount;
    uint8_t chapterCount;
    size_t ramUsageBytes;
    char showId[MAX_SHOW_ID_LEN];

    static ParseResult ok(const char* id, uint16_t cues, uint8_t chapters, size_t ram) {
        ParseResult r;
        r.success = true;
        r.errorMessage = nullptr;
        r.cueCount = cues;
        r.chapterCount = chapters;
        r.ramUsageBytes = ram;
        strncpy(r.showId, id, MAX_SHOW_ID_LEN - 1);
        r.showId[MAX_SHOW_ID_LEN - 1] = '\0';
        return r;
    }

    static ParseResult error(const char* msg) {
        ParseResult r;
        r.success = false;
        r.errorMessage = msg;
        r.cueCount = 0;
        r.chapterCount = 0;
        r.ramUsageBytes = 0;
        r.showId[0] = '\0';
        return r;
    }
};


/**
 * @brief Parse ShowBundle JSON and allocate runtime show data
 */
class ShowBundleParser {
public:
    /**
     * @brief Parse a ShowBundle JSON payload
     * @param json Raw JSON bytes
     * @param jsonLen Length of JSON data
     * @param store DynamicShowStore to allocate from
     * @param outSlot Output: slot index where show was stored
     * @return ParseResult with success/error info
     */
    static ParseResult parse(
        const uint8_t* json,
        size_t jsonLen,
        DynamicShowStore& store,
        uint8_t& outSlot
    ) {
        outSlot = 0xFF;

        // Validate size
        if (jsonLen == 0) {
            return ParseResult::error("Empty payload");
        }
        if (jsonLen > MAX_SHOW_JSON_SIZE) {
            return ParseResult::error("Payload exceeds 32KB limit");
        }

        // Parse JSON
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json, jsonLen);

        if (err) {
            return ParseResult::error("Invalid JSON");
        }

        JsonObjectConst root = doc.as<JsonObjectConst>();

        // ============================================================
        // Validate top-level required fields
        // ============================================================

        if (!root.containsKey("version")) {
            return ParseResult::error("Missing 'version' field");
        }
        uint8_t version = root["version"].as<uint8_t>();
        if (version != 1) {
            return ParseResult::error("Unsupported schema version (expected 1)");
        }

        if (!root.containsKey("id") || !root["id"].is<const char*>()) {
            return ParseResult::error("Missing or invalid 'id' field");
        }
        const char* id = root["id"].as<const char*>();
        if (strlen(id) == 0 || strlen(id) > MAX_SHOW_ID_LEN - 1) {
            return ParseResult::error("Show ID must be 1-32 characters");
        }

        if (!root.containsKey("name") || !root["name"].is<const char*>()) {
            return ParseResult::error("Missing or invalid 'name' field");
        }
        const char* name = root["name"].as<const char*>();
        if (strlen(name) == 0 || strlen(name) > MAX_SHOW_NAME_LEN - 1) {
            return ParseResult::error("Show name must be 1-64 characters");
        }

        if (!root.containsKey("durationMs")) {
            return ParseResult::error("Missing 'durationMs' field");
        }
        uint32_t durationMs = root["durationMs"].as<uint32_t>();
        if (durationMs == 0) {
            return ParseResult::error("durationMs must be > 0");
        }

        bool looping = root["looping"] | false;
        float bpm = root["bpm"] | 0.0f;

        // ============================================================
        // Validate chapters array
        // ============================================================

        if (!root.containsKey("chapters") || !root["chapters"].is<JsonArrayConst>()) {
            return ParseResult::error("Missing or invalid 'chapters' array");
        }
        JsonArrayConst chaptersArr = root["chapters"].as<JsonArrayConst>();
        uint8_t chapterCount = static_cast<uint8_t>(chaptersArr.size());
        if (chapterCount == 0) {
            return ParseResult::error("chapters array must not be empty");
        }
        if (chapterCount > MAX_CHAPTERS_PER_SHOW) {
            return ParseResult::error("Too many chapters (max 32)");
        }

        // ============================================================
        // Validate cues array
        // ============================================================

        if (!root.containsKey("cues") || !root["cues"].is<JsonArrayConst>()) {
            return ParseResult::error("Missing or invalid 'cues' array");
        }
        JsonArrayConst cuesArr = root["cues"].as<JsonArrayConst>();
        uint16_t cueCount = static_cast<uint16_t>(cuesArr.size());
        if (cueCount > MAX_CUES_PER_SHOW) {
            return ParseResult::error("Too many cues (max 512)");
        }

        // ============================================================
        // Find or allocate slot
        // ============================================================

        // Check if a show with this ID already exists (overwrite)
        int8_t existingSlot = store.findById(id);
        int8_t slot;
        if (existingSlot >= 0) {
            slot = existingSlot;
        } else {
            slot = store.findFreeSlot();
            if (slot < 0) {
                return ParseResult::error("No free show slots (max 4)");
            }
        }

        // ============================================================
        // Allocate PSRAM block
        // ============================================================

        DynamicShowData* data = store.allocateShowData(cueCount, chapterCount);
        if (!data) {
            return ParseResult::error("PSRAM allocation failed");
        }

        // ============================================================
        // Populate metadata
        // ============================================================

        strncpy(data->id, id, MAX_SHOW_ID_LEN - 1);
        data->id[MAX_SHOW_ID_LEN - 1] = '\0';
        strncpy(data->name, name, MAX_SHOW_NAME_LEN - 1);
        data->name[MAX_SHOW_NAME_LEN - 1] = '\0';
        data->totalDurationMs = durationMs;
        data->looping = looping;
        data->bpm = bpm;

        // ============================================================
        // Parse chapters
        // ============================================================

        uint8_t chIdx = 0;
        for (JsonObjectConst ch : chaptersArr) {
            if (chIdx >= chapterCount) break;

            const char* chName = ch["name"] | "Untitled";
            strncpy(data->chapterNames[chIdx], chName, MAX_CHAPTER_NAME_LEN - 1);
            data->chapterNames[chIdx][MAX_CHAPTER_NAME_LEN - 1] = '\0';

            data->chapters[chIdx].startTimeMs = ch["startTimeMs"] | 0u;
            data->chapters[chIdx].durationMs = ch["durationMs"] | 0u;
            data->chapters[chIdx].tensionLevel = ch["tensionLevel"] | 128u;

            // Parse narrative phase string
            const char* phaseStr = ch["narrativePhase"] | "build";
            data->chapters[chIdx].narrativePhase = parseNarrativePhase(phaseStr);

            // Cue index mapping will be computed after cue parsing
            data->chapters[chIdx].cueStartIndex = 0;
            data->chapters[chIdx].cueCount = 0;

            chIdx++;
        }

        // ============================================================
        // Parse cues
        // ============================================================

        uint16_t cIdx = 0;
        uint32_t lastTimeMs = 0;
        bool sorted = true;

        for (JsonObjectConst cue : cuesArr) {
            if (cIdx >= cueCount) break;

            uint32_t timeMs = cue["timeMs"] | 0u;

            // Validate sort order
            if (cIdx > 0 && timeMs < lastTimeMs) {
                sorted = false;
            }
            lastTimeMs = timeMs;

            data->cues[cIdx].timeMs = timeMs;
            data->cues[cIdx].targetZone = cue["zone"] | ZONE_GLOBAL;

            // Parse cue type
            const char* typeStr = cue["type"] | "marker";
            JsonObjectConst cueData = cue["data"].as<JsonObjectConst>();

            if (!parseCue(typeStr, cueData, data->cues[cIdx])) {
                // Free the allocated block on error
#ifndef NATIVE_BUILD
                heap_caps_free(data);
#else
                free(data);
#endif
                return ParseResult::error("Invalid cue type or data");
            }

            cIdx++;
        }

        // Sort cues by time if not pre-sorted
        if (!sorted) {
            std::sort(data->cues, data->cues + cueCount,
                [](const ShowCue& a, const ShowCue& b) {
                    return a.timeMs < b.timeMs;
                });
        }

        // ============================================================
        // Compute chapter -> cue index mapping
        // ============================================================

        for (uint8_t ci = 0; ci < chapterCount; ci++) {
            uint32_t chStart = data->chapters[ci].startTimeMs;
            uint32_t chEnd = chStart + data->chapters[ci].durationMs;

            uint8_t firstCue = 0;
            uint8_t cueCountInChapter = 0;
            bool foundFirst = false;

            for (uint16_t qi = 0; qi < cueCount; qi++) {
                if (data->cues[qi].timeMs >= chStart && data->cues[qi].timeMs < chEnd) {
                    if (!foundFirst) {
                        firstCue = static_cast<uint8_t>(qi > 255 ? 255 : qi);
                        foundFirst = true;
                    }
                    cueCountInChapter++;
                    if (cueCountInChapter == 255) break;  // uint8_t limit
                }
            }

            data->chapters[ci].cueStartIndex = firstCue;
            data->chapters[ci].cueCount = cueCountInChapter;
        }

        // ============================================================
        // Register in store
        // ============================================================

        if (!store.registerShow(static_cast<uint8_t>(slot), data)) {
#ifndef NATIVE_BUILD
            heap_caps_free(data);
#else
            free(data);
#endif
            return ParseResult::error("Failed to register show in store");
        }

        outSlot = static_cast<uint8_t>(slot);
        return ParseResult::ok(id, cueCount, chapterCount, data->totalRamBytes);
    }

private:
    /**
     * @brief Parse narrative phase string to enum value
     */
    static uint8_t parseNarrativePhase(const char* phase) {
        if (strcmp(phase, "build") == 0) return SHOW_PHASE_BUILD;
        if (strcmp(phase, "hold") == 0) return SHOW_PHASE_HOLD;
        if (strcmp(phase, "release") == 0) return SHOW_PHASE_RELEASE;
        if (strcmp(phase, "rest") == 0) return SHOW_PHASE_REST;
        return SHOW_PHASE_BUILD;  // default
    }

    /**
     * @brief Parse paramId string to ParamId enum
     */
    static ParamId parseParamId(const char* paramStr) {
        if (strcmp(paramStr, "brightness") == 0) return PARAM_BRIGHTNESS;
        if (strcmp(paramStr, "speed") == 0) return PARAM_SPEED;
        if (strcmp(paramStr, "intensity") == 0) return PARAM_INTENSITY;
        if (strcmp(paramStr, "saturation") == 0) return PARAM_SATURATION;
        if (strcmp(paramStr, "complexity") == 0) return PARAM_COMPLEXITY;
        if (strcmp(paramStr, "variation") == 0) return PARAM_VARIATION;
        return PARAM_BRIGHTNESS;  // default
    }

    /**
     * @brief Parse a single cue from JSON into ShowCue struct
     * @return true if cue was parsed successfully
     */
    static bool parseCue(const char* typeStr, JsonObjectConst data, ShowCue& out) {
        memset(out.data, 0, sizeof(out.data));

        if (strcmp(typeStr, "effect") == 0) {
            out.type = CUE_EFFECT;
            // 2-byte little-endian EffectId in data[0..1], transitionType in data[2]
            uint16_t eid = data["effectId"] | 0u;
            out.data[0] = static_cast<uint8_t>(eid & 0xFF);
            out.data[1] = static_cast<uint8_t>((eid >> 8) & 0xFF);
            out.data[2] = data["transitionType"] | 0u;
            return true;
        }

        if (strcmp(typeStr, "parameter_sweep") == 0) {
            out.type = CUE_PARAMETER_SWEEP;
            const char* paramStr = data["paramId"] | "brightness";
            out.data[0] = static_cast<uint8_t>(parseParamId(paramStr));
            out.data[1] = data["targetValue"] | 128u;
            uint16_t durMs = data["durationMs"] | 1000u;
            out.data[2] = static_cast<uint8_t>(durMs & 0xFF);
            out.data[3] = static_cast<uint8_t>((durMs >> 8) & 0xFF);
            return true;
        }

        if (strcmp(typeStr, "zone_config") == 0) {
            out.type = CUE_ZONE_CONFIG;
            // 2-byte little-endian EffectId in data[0..1], paletteId in data[2]
            uint16_t eid = data["effectId"] | 0u;
            out.data[0] = static_cast<uint8_t>(eid & 0xFF);
            out.data[1] = static_cast<uint8_t>((eid >> 8) & 0xFF);
            out.data[2] = data["paletteId"] | 0u;
            return true;
        }

        if (strcmp(typeStr, "palette") == 0) {
            out.type = CUE_PALETTE;
            out.data[0] = data["paletteId"] | 0u;
            return true;
        }

        if (strcmp(typeStr, "narrative") == 0) {
            out.type = CUE_NARRATIVE;
            const char* phaseStr = data["phase"] | "build";
            out.data[0] = parseNarrativePhase(phaseStr);
            uint16_t tensionAsTempoMs = data["tensionLevel"] | 128u;
            // Map tension to tempo: higher tension = faster tempo
            uint16_t tempoMs = static_cast<uint16_t>(
                8000 - (tensionAsTempoMs / 255.0f) * 6000
            );
            out.data[1] = static_cast<uint8_t>(tempoMs & 0xFF);
            out.data[2] = static_cast<uint8_t>((tempoMs >> 8) & 0xFF);
            return true;
        }

        if (strcmp(typeStr, "transition") == 0) {
            out.type = CUE_TRANSITION;
            out.data[0] = data["transitionType"] | 0u;
            uint16_t durMs = data["durationMs"] | 800u;
            out.data[1] = static_cast<uint8_t>(durMs & 0xFF);
            out.data[2] = static_cast<uint8_t>((durMs >> 8) & 0xFF);
            return true;
        }

        if (strcmp(typeStr, "marker") == 0) {
            out.type = CUE_MARKER;
            return true;
        }

        return false;  // Unknown cue type
    }
};

} // namespace prism
