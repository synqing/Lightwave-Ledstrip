/**
 * @file WebServerBroadcast.cpp
 * @brief WebServer broadcasting and subscription methods
 *
 * Split from WebServer.cpp for maintainability. These are still WebServer::
 * methods — just in a separate translation unit to keep WebServer.cpp focused
 * on lifecycle, setup, and the update() loop.
 *
 * Contains:
 *   - Status broadcasting (JSON status, zone state, effect change)
 *   - LED frame streaming (binary WS frames)
 *   - Audio/beat/FFT streaming (conditional on FEATURE_AUDIO_SYNC)
 *   - Benchmark streaming (conditional on FEATURE_AUDIO_BENCHMARK)
 *   - Subscription management for all stream types
 */

#include "WebServer.h"

#if FEATURE_WEB_SERVER

#define LW_LOG_TAG "WebServer"
#include "utils/Log.h"

#include "ApiResponse.h"
#include "webserver/LedStreamBroadcaster.h"
#include "webserver/LogStreamBroadcaster.h"
#include "../core/actors/NodeOrchestrator.h"
#include "../core/actors/RendererNode.h"
#include "../effects/zones/ZoneComposer.h"
#include "../effects/enhancement/EdgeMixer.h"
#include "../effects/zones/ZoneDefinition.h"
#include "../effects/zones/BlendMode.h"
#include "../config/network_config.h"

#if FEATURE_AUDIO_SYNC
#include "webserver/AudioStreamBroadcaster.h"
#include "webserver/ws/WsStreamCommands.h"
#include "../audio/AudioNode.h"
#include "../audio/contracts/ControlBus.h"
#include "../audio/tempo/TempoTracker.h"
#include <cmath>
#endif

#if FEATURE_AUDIO_BENCHMARK
#include "webserver/BenchmarkStreamBroadcaster.h"
#include "../audio/AudioBenchmarkMetrics.h"
#endif

namespace {

size_t getFreeInternalHeap() {
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}

} // namespace

namespace lightwaveos {
namespace network {

// ============================================================================
// Broadcasting
// ============================================================================

#if FEATURE_AUDIO_SYNC
/**
 * @brief Format chord state to musical key string (e.g., "C", "Am", "Dm")
 */
static const char* formatKeyName(uint8_t rootNote, audio::ChordType type) {
    static const char* NOTE_NAMES[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    if (rootNote >= 12) rootNote = rootNote % 12;
    const char* note = NOTE_NAMES[rootNote];

    switch (type) {
        case audio::ChordType::MAJOR:
            return note;
        case audio::ChordType::MINOR:
            static char minorKey[4];
            snprintf(minorKey, sizeof(minorKey), "%sm", note);
            return minorKey;
        case audio::ChordType::DIMINISHED:
            static char dimKey[5];
            snprintf(dimKey, sizeof(dimKey), "%sdim", note);
            return dimKey;
        case audio::ChordType::AUGMENTED:
            static char augKey[5];
            snprintf(augKey, sizeof(augKey), "%saug", note);
            return augKey;
        default:
            return note;
    }
}
#endif

void WebServer::broadcastStatus() {
    // SAFETY: Always defer broadcasts to avoid calling doBroadcastStatus() from within
    // AsyncTCP/AsyncWebServer callback context. This prevents re-entrancy issues and
    // corrupted pointer access when accessing m_renderer (Core 1) or m_ws from Core 0.
    //
    // QUEUE PROTECTION: Setting m_broadcastPending is idempotent - multiple calls
    // before the next update() tick will only result in one broadcast, preventing
    // queue buildup from rapid API calls.
    m_broadcastPending = true;
}

void WebServer::doBroadcastStatus() {
    if (m_lowHeapShed) {
        return;
    }
    if (!m_ws) {
        LW_LOGW("doBroadcastStatus: m_ws is null");
        return;
    }

    if (m_ws->count() == 0) return;

    const uint32_t now = millis();
    if (m_lastClientConnectMs != 0 && (now - m_lastClientConnectMs) < CONNECT_STABILISE_MS) {
        return;
    }

    m_ws->cleanupClients();
    if (m_ws->count() == 0) return;

    // QUEUE PROTECTION: Throttle broadcast frequency to prevent queue saturation
    static uint32_t lastBroadcastAttempt = 0;
    if (now - lastBroadcastAttempt < 50) {
        return;
    }
    lastBroadcastAttempt = now;

    JsonDocument doc;
    doc["type"] = "status";

    const CachedRendererState& cached = m_cachedRendererState;
    doc["effectId"] = cached.currentEffect;
    const char* curName = cached.findEffectName(cached.currentEffect);
    if (curName) {
        doc["effectName"] = curName;
    }
    doc["brightness"] = cached.brightness;
    doc["speed"] = cached.speed;
    doc["paletteId"] = cached.paletteIndex;
    doc["hue"] = cached.hue;
    doc["intensity"] = cached.intensity;
    doc["saturation"] = cached.saturation;
    doc["complexity"] = cached.complexity;
    doc["variation"] = cached.variation;
    doc["fps"] = cached.stats.currentFPS;
    doc["cpuPercent"] = cached.stats.cpuPercent;

    doc["freeHeap"] = ESP.getFreeHeap();
    doc["freeHeapInternal"] = static_cast<uint32_t>(getFreeInternalHeap());
    doc["freePsram"] = ESP.getFreePsram();
    doc["uptime"] = millis() / 1000;

    // Edge mixer state (from cache)
    doc["edgeMixerMode"] = cached.edgeMixerMode;
    doc["edgeMixerModeName"] = enhancement::EdgeMixer::modeName(
        static_cast<enhancement::EdgeMixerMode>(cached.edgeMixerMode));
    doc["edgeMixerSpread"] = cached.edgeMixerSpread;
    doc["edgeMixerStrength"] = cached.edgeMixerStrength;
    doc["edgeMixerSpatial"] = cached.edgeMixerSpatial;
    doc["edgeMixerSpatialName"] = enhancement::EdgeMixer::spatialName(
        static_cast<enhancement::EdgeMixerSpatial>(cached.edgeMixerSpatial));
    doc["edgeMixerTemporal"] = cached.edgeMixerTemporal;
    doc["edgeMixerTemporalName"] = enhancement::EdgeMixer::temporalName(
        static_cast<enhancement::EdgeMixerTemporal>(cached.edgeMixerTemporal));

#if FEATURE_AUDIO_SYNC
    auto* audio = m_orchestrator.getAudio();
    if (audio) {
        if (m_orchestrator.getRenderer() != nullptr) {
            doc["audioSyncMode"] = m_orchestrator.getRenderer()->getAudioSyncMode();
        }
#if FEATURE_AUDIO_BACKEND_ESV11
        if (m_audioFrameScratch) {
            audio->getControlBusBuffer().ReadLatest(*m_audioFrameScratch);
            const audio::ControlBusFrame& frame = *m_audioFrameScratch;

            doc["bpm"] = frame.es_bpm;

            float micLevelDb = (frame.rms > 0.0001f)
                ? (20.0f * log10f(frame.rms))
                : -80.0f;
            doc["mic"] = micLevelDb;

            const audio::ChordState& chord = frame.chordState;
            if (chord.confidence > 0.1f && chord.type != audio::ChordType::NONE) {
                doc["key"] = formatKeyName(chord.rootNote, chord.type);
            } else {
                doc["key"] = "";
            }
        } else {
            doc["bpm"] = 0.0f;
            doc["mic"] = -80.0f;
            doc["key"] = "";
        }
#elif FEATURE_AUDIO_BACKEND_PIPELINECORE
        if (m_audioFrameScratch) {
            audio->getControlBusBuffer().ReadLatest(*m_audioFrameScratch);
            const audio::ControlBusFrame& frame = *m_audioFrameScratch;
            doc["bpm"] = frame.tempoBpm;

            audio::AudioDspState dsp = audio->getDspState();
            float micLevelDb = (dsp.rmsPreGain > 0.0001f)
                ? (20.0f * log10f(dsp.rmsPreGain))
                : -80.0f;
            doc["mic"] = micLevelDb;

            const audio::ChordState& chord = frame.chordState;
            if (chord.confidence > 0.1f && chord.type != audio::ChordType::NONE) {
                doc["key"] = formatKeyName(chord.rootNote, chord.type);
            } else {
                doc["key"] = "";
            }
        } else {
            doc["bpm"] = 0.0f;
            doc["mic"] = -80.0f;
            doc["key"] = "";
        }
#else
        audio::TempoTrackerOutput tempo = audio->getTempo().getOutput();
        doc["bpm"] = tempo.bpm;

        audio::AudioDspState dsp = audio->getDspState();
        float micLevelDb = (dsp.rmsPreGain > 0.0001f)
            ? (20.0f * log10f(dsp.rmsPreGain))
            : -80.0f;
        doc["mic"] = micLevelDb;

        if (m_audioFrameScratch) {
            audio->getControlBusBuffer().ReadLatest(*m_audioFrameScratch);
            const audio::ChordState& chord = m_audioFrameScratch->chordState;
            if (chord.confidence > 0.1f && chord.type != audio::ChordType::NONE) {
                doc["key"] = formatKeyName(chord.rootNote, chord.type);
            } else {
                doc["key"] = "";
            }
        } else {
            doc["key"] = "";
        }
#endif
    }
#endif

    String output;
    serializeJson(doc, output);

    if (m_ws && m_ws->count() > 0) {
        if (!m_ws->availableForWriteAll()) return;
        m_ws->textAll(output);
    }
}

void WebServer::broadcastZoneState() {
    if (m_lowHeapShed) {
        return;
    }
    if (!m_ws) {
        LW_LOGW("broadcastZoneState: m_ws is null");
        return;
    }

    if (m_ws->count() == 0 || !m_zoneComposer) return;

    const uint32_t now = millis();
    if (m_lastClientConnectMs != 0 && (now - m_lastClientConnectMs) < CONNECT_STABILISE_MS) {
        return;
    }

    // QUEUE PROTECTION: Throttle zone broadcasts (4 Hz max)
    static uint32_t lastZoneBroadcastAttempt = 0;
    if (now - lastZoneBroadcastAttempt < 250) {
        return;
    }
    lastZoneBroadcastAttempt = now;

    JsonDocument doc;
    doc["type"] = "zones.list";
    doc["enabled"] = m_zoneComposer->isEnabled();
    doc["zoneCount"] = m_zoneComposer->getZoneCount();

    JsonArray segmentsArray = doc["segments"].to<JsonArray>();
    const zones::ZoneSegment* segments = m_zoneComposer->getZoneConfig();
    for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
        JsonObject seg = segmentsArray.add<JsonObject>();
        seg["zoneId"] = segments[i].zoneId;
        seg["s1LeftStart"] = segments[i].s1LeftStart;
        seg["s1LeftEnd"] = segments[i].s1LeftEnd;
        seg["s1RightStart"] = segments[i].s1RightStart;
        seg["s1RightEnd"] = segments[i].s1RightEnd;
        seg["totalLeds"] = segments[i].totalLeds;
    }

    JsonArray zones = doc["zones"].to<JsonArray>();
    for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
        JsonObject zone = zones.add<JsonObject>();
        zone["id"] = i;
        zone["enabled"] = m_zoneComposer->isZoneEnabled(i);
        EffectId effectId = m_zoneComposer->getZoneEffect(i);
        zone["effectId"] = effectId;
        const CachedRendererState& cached = m_cachedRendererState;
        const char* eName = cached.findEffectName(effectId);
        if (eName) {
            zone["effectName"] = eName;
        }
        zone["brightness"] = m_zoneComposer->getZoneBrightness(i);
        zone["speed"] = m_zoneComposer->getZoneSpeed(i);
        zone["paletteId"] = m_zoneComposer->getZonePalette(i);
        zone["blendMode"] = static_cast<uint8_t>(m_zoneComposer->getZoneBlendMode(i));
        zone["blendModeName"] = getBlendModeName(m_zoneComposer->getZoneBlendMode(i));

        zones::ZoneAudioConfig audioConfig = m_zoneComposer->getZoneAudioConfig(i);
        zone["tempoSync"] = audioConfig.tempoSync;
        zone["beatModulation"] = audioConfig.beatModulation;
        zone["tempoSpeedScale"] = audioConfig.tempoSpeedScale;
        zone["beatDecay"] = audioConfig.beatDecay;
        zone["audioBand"] = audioConfig.audioBand;
        zone["beatTriggerEnabled"] = audioConfig.beatTriggerEnabled;
        zone["beatTriggerInterval"] = audioConfig.beatTriggerInterval;
    }

    JsonArray presets = doc["presets"].to<JsonArray>();
    for (uint8_t i = 0; i < 5; i++) {
        JsonObject preset = presets.add<JsonObject>();
        preset["id"] = i;
        preset["name"] = zones::ZoneComposer::getPresetName(i);
    }

    String output;
    serializeJson(doc, output);

    if (m_ws && m_ws->count() > 0) {
        if (!m_ws->availableForWriteAll()) return;
        m_ws->textAll(output);
    }
}

void WebServer::broadcastSingleZoneState(uint8_t zoneId) {
    if (m_lowHeapShed) {
        return;
    }
    if (!m_ws) {
        LW_LOGW("broadcastSingleZoneState: m_ws is null");
        return;
    }

    if (m_ws->count() == 0 || !m_zoneComposer) return;

    const uint32_t now = millis();
    if (m_lastClientConnectMs != 0 && (now - m_lastClientConnectMs) < CONNECT_STABILISE_MS) {
        return;
    }

    // QUEUE PROTECTION: Throttle rapid per-zone updates (20 Hz max)
    static uint32_t lastSingleZoneBroadcastAttempt = 0;
    if (now - lastSingleZoneBroadcastAttempt < 50) {
        return;
    }
    lastSingleZoneBroadcastAttempt = now;

    if (zoneId >= m_zoneComposer->getZoneCount()) {
        LW_LOGW("broadcastSingleZoneState: invalid zoneId %d", zoneId);
        return;
    }

    JsonDocument doc;
    doc["type"] = "zones.stateChanged";
    doc["zoneId"] = zoneId;
    doc["timestamp"] = now;

    JsonObject current = doc["current"].to<JsonObject>();
    current["enabled"] = m_zoneComposer->isZoneEnabled(zoneId);

    EffectId effectId = m_zoneComposer->getZoneEffect(zoneId);
    current["effectId"] = effectId;

    const CachedRendererState& cached = m_cachedRendererState;
    const char* effName = cached.findEffectName(effectId);
    if (effName) {
        current["effectName"] = effName;
    } else {
        current["effectName"] = "Unknown";
    }

    current["brightness"] = m_zoneComposer->getZoneBrightness(zoneId);
    current["speed"] = m_zoneComposer->getZoneSpeed(zoneId);
    current["paletteId"] = m_zoneComposer->getZonePalette(zoneId);

    uint8_t blendModeValue = static_cast<uint8_t>(m_zoneComposer->getZoneBlendMode(zoneId));
    current["blendMode"] = blendModeValue;
    current["blendModeName"] = zones::getBlendModeName(m_zoneComposer->getZoneBlendMode(zoneId));

    String output;
    serializeJson(doc, output);

    if (m_ws && m_ws->count() > 0) {
        if (!m_ws->availableForWriteAll()) return;
        m_ws->textAll(output);
    }

    LW_LOGD("Broadcast zones.stateChanged for zone %d", zoneId);
}

void WebServer::notifyEffectChange(EffectId effectId, const char* name) {
    if (m_lowHeapShed) {
        return;
    }
    if (!m_ws) {
        LW_LOGW("notifyEffectChange: m_ws is null");
        return;
    }

    if (m_ws->count() == 0) return;

    // QUEUE PROTECTION: Throttle notifications (50ms minimum)
    static uint32_t lastEffectNotifyAttempt = 0;
    uint32_t now = millis();
    if (now - lastEffectNotifyAttempt < 50) {
        return;
    }
    lastEffectNotifyAttempt = now;

    JsonDocument doc;
    doc["type"] = "effectChanged";
    doc["effectId"] = effectId;
    doc["name"] = name;

    String output;
    serializeJson(doc, output);

    if (m_ws && m_ws->count() > 0) {
        if (!m_ws->availableForWriteAll()) return;
        m_ws->textAll(output);
    }
}

void WebServer::notifyParameterChange() {
    broadcastStatus();
}

// ============================================================================
// LED Frame Streaming
// ============================================================================

void WebServer::broadcastLEDFrame() {
    if (!m_ledBroadcaster || !m_renderer || !m_ledFrameScratch) return;

    if (m_lowHeapShed) {
        return;
    }

    if (!m_ledBroadcaster->hasSubscribers()) {
        return;
    }

    m_renderer->getBufferCopy(m_ledFrameScratch);
    m_ledBroadcaster->broadcast(m_ledFrameScratch);
}

bool WebServer::setLEDStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_ledBroadcaster) return false;
    if (subscribe) {
        const uint32_t freeInternal = static_cast<uint32_t>(getFreeInternalHeap());
        if (m_lowHeapShed || freeInternal < INTERNAL_HEAP_SHED_BELOW_BYTES) {
            LW_LOGW("LED stream subscribe rejected (low internal heap=%lu)", (unsigned long)freeInternal);
            return false;
        }
    }
    uint32_t clientId = client->id();
    bool success = m_ledBroadcaster->setSubscription(clientId, subscribe);

    if (subscribe && success) {
        LW_LOGD("Client %u subscribed to LED stream", clientId);
    } else if (!subscribe) {
        LW_LOGD("Client %u unsubscribed from LED stream", clientId);
    }

    return success;
}

bool WebServer::hasLEDStreamSubscribers() const {
    return m_ledBroadcaster && m_ledBroadcaster->hasSubscribers();
}

// ============================================================================
// Log Stream (Wireless Serial Monitoring)
// ============================================================================

bool WebServer::setLogStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_logBroadcaster) return false;
    uint32_t clientId = client->id();
    bool success = m_logBroadcaster->setSubscription(clientId, subscribe);
    return success;
}

bool WebServer::hasLogStreamSubscribers() const {
    return m_logBroadcaster && m_logBroadcaster->hasSubscribers();
}

#if FEATURE_AUDIO_SYNC
// ============================================================================
// Audio Frame Streaming
// ============================================================================

void WebServer::broadcastAudioFrame() {
    if (!m_audioBroadcaster || !m_renderer || !m_audioFrameScratch || !m_audioGridScratch) return;
    if (m_lowHeapShed) {
        return;
    }

    m_renderer->copyCachedAudioSnapshot(*m_audioFrameScratch, *m_audioGridScratch);
    m_audioBroadcaster->broadcast(*m_audioFrameScratch, *m_audioGridScratch);
}

void WebServer::broadcastBeatEvent() {
    if (m_lowHeapShed) {
        return;
    }
    if (!webserver::ws::hasBeatEventSubscribers()) return;

    if (!m_ws || m_ws->count() == 0) return;

    // Throttle beat events (20 Hz max)
    static uint32_t lastBeatBroadcastAttempt = 0;
    uint32_t now = millis();
    if (now - lastBeatBroadcastAttempt < 50) return;
    lastBeatBroadcastAttempt = now;

    if (!m_renderer) return;

    if (!m_audioGridScratch) return;
    m_renderer->copyLastMusicalGrid(*m_audioGridScratch);

    if (!m_audioGridScratch->beat_tick && !m_audioGridScratch->downbeat_tick) return;

    JsonDocument doc;
    doc["type"] = "beat.event";
    doc["tick"] = m_audioGridScratch->beat_tick;
    doc["downbeat"] = m_audioGridScratch->downbeat_tick;
    doc["beat_index"] = (uint32_t)(m_audioGridScratch->beat_index & 0xFFFFFFFF);
    doc["bar_index"] = (uint32_t)(m_audioGridScratch->bar_index & 0xFFFFFFFF);
    doc["beat_in_bar"] = m_audioGridScratch->beat_in_bar;
    doc["beat_phase"] = m_audioGridScratch->beat_phase01;
    doc["bpm"] = m_audioGridScratch->bpm_smoothed;
    doc["confidence"] = m_audioGridScratch->tempo_confidence;

    String json;
    serializeJson(doc, json);

    if (m_ws) {
        if (!m_ws->availableForWriteAll()) return;
        m_ws->textAll(json);
    }
}

void WebServer::broadcastFftFrame() {
    if (!m_ws || !m_renderer || !m_audioFrameScratch) return;

    if (!webserver::ws::hasFftStreamSubscribers()) {
        return;
    }

    m_renderer->copyCachedAudioFrame(*m_audioFrameScratch);
    webserver::ws::broadcastFftFrame(*m_audioFrameScratch, m_ws);
}

bool WebServer::setAudioStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_audioBroadcaster) return false;
    if (subscribe) {
        const uint32_t freeInternal = static_cast<uint32_t>(getFreeInternalHeap());
        if (m_lowHeapShed || freeInternal < INTERNAL_HEAP_SHED_BELOW_BYTES) {
            LW_LOGW("Audio stream subscribe rejected (low internal heap=%lu)", (unsigned long)freeInternal);
            return false;
        }
    }
    uint32_t clientId = client->id();
    bool success = m_audioBroadcaster->setSubscription(clientId, subscribe);

    if (subscribe && success) {
        LW_LOGD("Client %u subscribed to audio stream", clientId);
    } else if (!subscribe) {
        LW_LOGD("Client %u unsubscribed from audio stream", clientId);
    }

    return success;
}

bool WebServer::hasAudioStreamSubscribers() const {
    return m_audioBroadcaster && m_audioBroadcaster->hasSubscribers();
}
#endif

#if FEATURE_AUDIO_BENCHMARK
// ============================================================================
// Benchmark Metrics Streaming
// ============================================================================

void WebServer::broadcastBenchmarkStats() {
    if (!m_benchmarkBroadcaster) return;

    auto* audio = m_orchestrator.getAudio();
    if (!audio) return;

    const audio::AudioBenchmarkStats& stats = audio->getBenchmarkStats();
    m_benchmarkBroadcaster->broadcastCompact(stats);
}

bool WebServer::setBenchmarkStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_benchmarkBroadcaster) return false;
    uint32_t clientId = client->id();
    bool success = m_benchmarkBroadcaster->setSubscription(clientId, subscribe);

    if (subscribe && success) {
        LW_LOGD("Client %u subscribed to benchmark stream", clientId);
    } else if (!subscribe) {
        LW_LOGD("Client %u unsubscribed from benchmark stream", clientId);
    }

    return success;
}

bool WebServer::hasBenchmarkStreamSubscribers() const {
    return m_benchmarkBroadcaster && m_benchmarkBroadcaster->hasSubscribers();
}
#endif

// ============================================================================
// Rate Limiting
// ============================================================================

bool WebServer::checkRateLimit(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (!m_rateLimiter.checkHTTP(clientIP)) {
        uint32_t retryAfter = m_rateLimiter.getRetryAfterSeconds(clientIP);
        sendRateLimitError(request, retryAfter);
        return false;
    }
    return true;
}

bool WebServer::checkWsRateLimit(AsyncWebSocketClient* client) {
    IPAddress clientIP = client->remoteIP();
    return m_rateLimiter.checkWebSocket(clientIP);
}

bool WebServer::checkAPIKey(AsyncWebServerRequest* request) {
#if FEATURE_API_AUTH
    String currentKey = m_apiKeyManager.getKey();

    if (currentKey.length() == 0) {
        return true;
    }

    IPAddress clientIP = request->client()->remoteIP();

    if (m_authRateLimiter.isBlocked(clientIP)) {
        uint32_t retryAfter = m_authRateLimiter.getRetryAfterSeconds(clientIP);
        sendAuthRateLimitError(request, retryAfter);
        return false;
    }

    if (!request->hasHeader("X-API-Key")) {
        bool nowBlocked = m_authRateLimiter.recordFailure(clientIP);
        if (nowBlocked) {
            uint32_t retryAfter = m_authRateLimiter.getRetryAfterSeconds(clientIP);
            sendAuthRateLimitError(request, retryAfter);
        } else {
            sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                              ErrorCodes::UNAUTHORIZED, "Missing X-API-Key header");
        }
        return false;
    }

    if (!m_apiKeyManager.validateKey(request->header("X-API-Key"))) {
        bool nowBlocked = m_authRateLimiter.recordFailure(clientIP);
        if (nowBlocked) {
            uint32_t retryAfter = m_authRateLimiter.getRetryAfterSeconds(clientIP);
            sendAuthRateLimitError(request, retryAfter);
        } else {
            sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                              ErrorCodes::UNAUTHORIZED, "Invalid API key");
        }
        return false;
    }

    m_authRateLimiter.recordSuccess(clientIP);
#endif
    return true;
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
