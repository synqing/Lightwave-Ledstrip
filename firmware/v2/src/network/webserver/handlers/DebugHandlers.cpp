/**
 * @file DebugHandlers.cpp
 * @brief Debug handlers implementation
 *
 * Implements both unified debug config endpoints and legacy audio debug endpoints.
 */

#include "DebugHandlers.h"
#include "../../ApiResponse.h"
#include "../../../config/DebugConfig.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../effects/zones/ZoneComposer.h"
#include "../UdpStreamer.h"
#include <ArduinoJson.h>
#include <esp_system.h>
#include <cmath>  // for log10f

#if FEATURE_AUDIO_SYNC
#include "../../../audio/AudioDebugConfig.h"
#include "../../../audio/AudioActor.h"
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// ============================================================================
// Unified Debug Config Handlers (always available)
// ============================================================================

void DebugHandlers::handleDebugConfigGet(AsyncWebServerRequest* request) {
    auto& cfg = config::getDebugConfig();

    sendSuccessResponse(request, [&cfg](JsonObject& data) {
        // Global level
        data["globalLevel"] = cfg.globalLevel;

        // Domain-specific levels (raw values, -1 = use global)
        JsonObject domains = data["domains"].to<JsonObject>();
        domains["audio"] = cfg.audioLevel;
        domains["render"] = cfg.renderLevel;
        domains["network"] = cfg.networkLevel;
        domains["actor"] = cfg.actorLevel;
        domains["system"] = cfg.systemLevel;

        // Periodic intervals
        JsonObject intervals = data["intervals"].to<JsonObject>();
        intervals["status"] = cfg.statusIntervalSec;
        intervals["spectrum"] = cfg.spectrumIntervalSec;

        // Computed effective levels for each domain
        JsonObject effectiveLevels = data["effectiveLevels"].to<JsonObject>();
        effectiveLevels["audio"] = cfg.effectiveLevel(config::DebugDomain::AUDIO);
        effectiveLevels["render"] = cfg.effectiveLevel(config::DebugDomain::RENDER);
        effectiveLevels["network"] = cfg.effectiveLevel(config::DebugDomain::NETWORK);
        effectiveLevels["actor"] = cfg.effectiveLevel(config::DebugDomain::ACTOR);
        effectiveLevels["system"] = cfg.effectiveLevel(config::DebugDomain::SYSTEM);

        // Level names for UI reference
        JsonArray levels = data["levels"].to<JsonArray>();
        for (uint8_t i = 0; i <= 5; i++) {
            levels.add(config::DebugConfig::levelName(i));
        }
    });
}

void DebugHandlers::handleDebugConfigSet(AsyncWebServerRequest* request,
                                          uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON payload");
        return;
    }

    auto& cfg = config::getDebugConfig();
    bool updated = false;

    // Update global level
    if (doc.containsKey("globalLevel")) {
        int level = doc["globalLevel"].as<int>();
        if (level < 0 || level > 5) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "globalLevel must be 0-5", "globalLevel");
            return;
        }
        cfg.globalLevel = static_cast<uint8_t>(level);
        updated = true;
    }

    // Update domain-specific levels
    auto updateDomainLevel = [&doc, &cfg, &updated](const char* key, int8_t& target) {
        if (doc.containsKey(key)) {
            int level = doc[key].as<int>();
            if (level < -1 || level > 5) {
                return false;  // Invalid
            }
            target = static_cast<int8_t>(level);
            updated = true;
        }
        return true;  // Valid or not present
    };

    if (!updateDomainLevel("audio", cfg.audioLevel)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "audio level must be -1 to 5", "audio");
        return;
    }
    if (!updateDomainLevel("render", cfg.renderLevel)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "render level must be -1 to 5", "render");
        return;
    }
    if (!updateDomainLevel("network", cfg.networkLevel)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "network level must be -1 to 5", "network");
        return;
    }
    if (!updateDomainLevel("actor", cfg.actorLevel)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "actor level must be -1 to 5", "actor");
        return;
    }
    if (!updateDomainLevel("system", cfg.systemLevel)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "system level must be -1 to 5", "system");
        return;
    }

    // Update periodic intervals
    if (doc.containsKey("statusInterval")) {
        int interval = doc["statusInterval"].as<int>();
        if (interval < 0 || interval > 3600) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "statusInterval must be 0-3600 seconds", "statusInterval");
            return;
        }
        cfg.statusIntervalSec = static_cast<uint16_t>(interval);
        updated = true;
    }

    if (doc.containsKey("spectrumInterval")) {
        int interval = doc["spectrumInterval"].as<int>();
        if (interval < 0 || interval > 3600) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "spectrumInterval must be 0-3600 seconds", "spectrumInterval");
            return;
        }
        cfg.spectrumIntervalSec = static_cast<uint16_t>(interval);
        updated = true;
    }

    if (!updated) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "At least one configuration field required");
        return;
    }

    // Print to serial for visibility
    config::printDebugConfig();

    // Return updated config
    sendSuccessResponse(request, [&cfg](JsonObject& data) {
        data["globalLevel"] = cfg.globalLevel;

        JsonObject domains = data["domains"].to<JsonObject>();
        domains["audio"] = cfg.audioLevel;
        domains["render"] = cfg.renderLevel;
        domains["network"] = cfg.networkLevel;
        domains["actor"] = cfg.actorLevel;
        domains["system"] = cfg.systemLevel;

        JsonObject intervals = data["intervals"].to<JsonObject>();
        intervals["status"] = cfg.statusIntervalSec;
        intervals["spectrum"] = cfg.spectrumIntervalSec;

        JsonObject effectiveLevels = data["effectiveLevels"].to<JsonObject>();
        effectiveLevels["audio"] = cfg.effectiveLevel(config::DebugDomain::AUDIO);
        effectiveLevels["render"] = cfg.effectiveLevel(config::DebugDomain::RENDER);
        effectiveLevels["network"] = cfg.effectiveLevel(config::DebugDomain::NETWORK);
        effectiveLevels["actor"] = cfg.effectiveLevel(config::DebugDomain::ACTOR);
        effectiveLevels["system"] = cfg.effectiveLevel(config::DebugDomain::SYSTEM);
    });
}

void DebugHandlers::handleDebugStatus(AsyncWebServerRequest* request,
                                       actors::ActorSystem& actorSystem) {
#if FEATURE_AUDIO_SYNC
    // Get audio actor reference once, use in both lambda and serial output
    auto* audioActor = actorSystem.getAudio();
    audio::AudioActorStats actorStats;
    audio::ControlBusFrame lastFrame;
    uint32_t lastSeq = 0;
    bool hasAudio = false;
#if !FEATURE_AUDIO_BACKEND_ESV11
    audio::AudioDspState dspState;
#endif

    if (audioActor) {
#if FEATURE_AUDIO_BACKEND_ESV11
        lastSeq = audioActor->getControlBusBuffer().ReadLatest(lastFrame);
#else
        dspState = audioActor->getDspState();
#endif
        actorStats = audioActor->getStats();
        hasAudio = true;
    }
#endif

    // Collect system status data
    sendSuccessResponse(request, [
#if FEATURE_AUDIO_SYNC
        &actorStats, &lastFrame, lastSeq, hasAudio
#if !FEATURE_AUDIO_BACKEND_ESV11
        , &dspState
#endif
#endif
    ](JsonObject& data) {
        // Memory status (always available)
        JsonObject memory = data["memory"].to<JsonObject>();
        memory["heapFree"] = esp_get_free_heap_size();
        memory["heapMin"] = esp_get_minimum_free_heap_size();
        memory["heapMaxBlock"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

#if FEATURE_AUDIO_SYNC
        // Audio status (when audio is enabled)
        JsonObject audio = data["audio"].to<JsonObject>();

        if (hasAudio) {
#if FEATURE_AUDIO_BACKEND_ESV11
            audio["seq"] = lastSeq;
            audio["rms"] = lastFrame.rms;
            audio["flux"] = lastFrame.flux;
            audio["bpm"] = lastFrame.es_bpm;
            audio["tempoConfidence"] = lastFrame.es_tempo_confidence;
            audio["phase01AtAudioT"] = lastFrame.es_phase01_at_audio_t;
            audio["beatTick"] = lastFrame.es_beat_tick;
            audio["downbeatTick"] = lastFrame.es_downbeat_tick;
            audio["beatInBar"] = lastFrame.es_beat_in_bar;
            audio["beatStrength"] = lastFrame.es_beat_strength;

            float micLevelDb = (lastFrame.rms > 0.0001f)
                ? 20.0f * log10f(lastFrame.rms)
                : -60.0f;  // Floor at -60 dB
            audio["micLevelDb"] = micLevelDb;

            // From actor stats
            audio["captures"] = actorStats.captureSuccessCount;
            audio["capturesFailed"] = actorStats.captureFailCount;
            audio["tickCount"] = actorStats.tickCount;
#else
            // From DSP state
            audio["rms"] = dspState.rmsMapped;
            audio["rmsPreGain"] = dspState.rmsPreGain;
            audio["agcGain"] = dspState.agcGain;
            audio["dcEstimate"] = dspState.dcEstimate;
            audio["noiseFloor"] = dspState.noiseFloor;
            audio["clips"] = dspState.clipCount;

            // From actor stats
            audio["captures"] = actorStats.captureSuccessCount;
            audio["capturesFailed"] = actorStats.captureFailCount;
            audio["tickCount"] = actorStats.tickCount;

            // Compute mic level in dB from RMS (approximate)
            float micLevelDb = (dspState.rmsMapped > 0.0001f)
                ? 20.0f * log10f(dspState.rmsMapped)
                : -60.0f;  // Floor at -60 dB
            audio["micLevelDb"] = micLevelDb;
#endif
        } else {
            audio["error"] = "AudioActor not initialized";
        }
#endif

        // System info
        JsonObject system = data["system"].to<JsonObject>();
        system["uptimeMs"] = millis();
        system["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    });

    // Also print to serial as one-shot output
    Serial.println(F("\n=== Debug Status (one-shot) ==="));
    Serial.printf("Heap: %lu free, %lu min, %lu max block\n",
                  (unsigned long)esp_get_free_heap_size(),
                  (unsigned long)esp_get_minimum_free_heap_size(),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.printf("Uptime: %lu ms\n", (unsigned long)millis());

#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
#if FEATURE_AUDIO_BACKEND_ESV11
        Serial.printf("Audio(ES): RMS=%.3f FLUX=%.3f BPM=%.1f conf=%.2f beat=%u/%u\n",
                      lastFrame.rms, lastFrame.flux,
                      lastFrame.es_bpm, lastFrame.es_tempo_confidence,
                      (unsigned)lastFrame.es_beat_in_bar,
                      (unsigned)lastFrame.es_beat_tick);
#else
        Serial.printf("Audio: RMS=%.3f, AGC=%.2f, DC=%.1f, clips=%u\n",
                      dspState.rmsMapped, dspState.agcGain,
                      dspState.dcEstimate, dspState.clipCount);
        Serial.printf("Captures: %lu success, %lu failed\n",
                      (unsigned long)actorStats.captureSuccessCount,
                      (unsigned long)actorStats.captureFailCount);
#endif
    }
#endif
    Serial.println(F("===============================\n"));
}

void DebugHandlers::handleUdpStatsGet(AsyncWebServerRequest* request,
                                       webserver::UdpStreamer* udpStreamer) {
    if (!udpStreamer) {
        sendSuccessResponse(request, [](JsonObject& data) {
            data["status"] = "unavailable";
            data["message"] = "UDP streamer not initialised";
        });
        return;
    }

    webserver::UdpStreamer::UdpStats stats;
    udpStreamer->getStats(stats);
    uint32_t nowMs = millis();

    sendSuccessResponse(request, [stats, nowMs](JsonObject& data) {
        data["started"] = stats.started;
        data["subscribers"] = stats.subscriberCount;
        data["suppressed"] = stats.suppressedCount;
        data["consecutiveFailures"] = stats.consecutiveFailures;
        data["lastFailureMs"] = stats.lastFailureMs;
        data["lastFailureAgoMs"] = stats.lastFailureMs > 0 ? (nowMs - stats.lastFailureMs) : 0;
        data["cooldownRemainingMs"] = stats.cooldownUntilMs > nowMs ? (stats.cooldownUntilMs - nowMs) : 0;
        data["socketResets"] = stats.socketResets;
        data["lastSocketResetMs"] = stats.lastSocketResetMs;

        JsonObject led = data["led"].to<JsonObject>();
        led["attempts"] = stats.ledAttempts;
        led["success"] = stats.ledSuccess;
        led["failures"] = stats.ledFailures;

        JsonObject audio = data["audio"].to<JsonObject>();
        audio["attempts"] = stats.audioAttempts;
        audio["success"] = stats.audioSuccess;
        audio["failures"] = stats.audioFailures;
    });
}

// ============================================================================
// Legacy Audio Debug Handlers (backward compatibility)
// ============================================================================

#if FEATURE_AUDIO_SYNC

void DebugHandlers::handleAudioDebugGet(AsyncWebServerRequest* request) {
    auto& cfg = audio::getAudioDebugConfig();

    sendSuccessResponse(request, [&cfg](JsonObject& data) {
        data["verbosity"] = cfg.verbosity;
        data["baseInterval"] = cfg.baseInterval;

        // Include derived intervals for client reference
        JsonObject intervals = data["intervals"].to<JsonObject>();
        intervals["8band"] = cfg.interval8Band();
        intervals["64bin"] = cfg.interval64Bin();
        intervals["dma"] = cfg.intervalDMA();

        // Include level descriptions for UI
        JsonArray levels = data["levels"].to<JsonArray>();
        levels.add("Off - No debug output");
        levels.add("Minimal - Errors only");
        levels.add("Status - 10s health reports");
        levels.add("Low - + DMA diagnostics (~5s)");
        levels.add("Medium - + 64-bin Goertzel (~2s)");
        levels.add("High - + 8-band Goertzel (~1s)");
    });
}

void DebugHandlers::handleAudioDebugSet(AsyncWebServerRequest* request,
                                         uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON payload");
        return;
    }

    auto& cfg = audio::getAudioDebugConfig();
    bool updated = false;

    // Validate and update verbosity
    if (doc.containsKey("verbosity")) {
        int verbosity = doc["verbosity"].as<int>();
        if (verbosity < 0 || verbosity > 5) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "verbosity must be 0-5", "verbosity");
            return;
        }
        cfg.verbosity = static_cast<uint8_t>(verbosity);
        updated = true;
    }

    // Validate and update baseInterval
    if (doc.containsKey("baseInterval")) {
        int interval = doc["baseInterval"].as<int>();
        if (interval < 1 || interval > 1000) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "baseInterval must be 1-1000", "baseInterval");
            return;
        }
        cfg.baseInterval = static_cast<uint16_t>(interval);
        updated = true;
    }

    if (!updated) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "At least one of 'verbosity' or 'baseInterval' required");
        return;
    }

    // Return updated config
    sendSuccessResponse(request, [&cfg](JsonObject& data) {
        data["verbosity"] = cfg.verbosity;
        data["baseInterval"] = cfg.baseInterval;

        JsonObject intervals = data["intervals"].to<JsonObject>();
        intervals["8band"] = cfg.interval8Band();
        intervals["64bin"] = cfg.interval64Bin();
        intervals["dma"] = cfg.intervalDMA();
    });
}

#endif // FEATURE_AUDIO_SYNC

void DebugHandlers::handleZoneMemoryStats(AsyncWebServerRequest* request,
                                           lightwaveos::zones::ZoneComposer* zoneComposer) {
    if (!zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Zone composer not available");
        return;
    }

    sendSuccessResponse(request, [zoneComposer](JsonObject& data) {
        uint8_t zoneCount = zoneComposer->getZoneCount();

        // Estimate memory based on zone count and typical configuration
        // Each zone typically has: 40 LEDs × 3 bytes (CRGB) = 120 bytes buffer
        // Plus zone metadata (~64 bytes per zone)
        size_t perZoneBufferSize = 40 * 3;  // Typical zone LED count
        size_t perZoneMetadata = 64;
        size_t totalMemory = zoneCount * (perZoneBufferSize + perZoneMetadata);

        JsonArray zones = data["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < zoneCount; i++) {
            JsonObject zoneObj = zones.add<JsonObject>();
            zoneObj["id"] = i;
            zoneObj["estimatedBufferSize"] = perZoneBufferSize;
            zoneObj["estimatedMetadata"] = perZoneMetadata;
        }

        data["zoneCount"] = zoneCount;
        data["totalMemoryEstimate"] = totalMemory;
        data["composerOverhead"] = sizeof(*zoneComposer);
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
