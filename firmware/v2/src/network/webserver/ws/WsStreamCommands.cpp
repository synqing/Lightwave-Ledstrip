/**
 * @file WsStreamCommands.cpp
 * @brief WebSocket stream subscription command handlers implementation
 */

#include "WsStreamCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../LedStreamBroadcaster.h"
#include "../UdpStreamer.h"
#include "../../../codec/WsStreamCodec.h"
#include "../../../core/actors/RendererActor.h"
#if FEATURE_CONTROL_LEASE
#include "../../../core/system/ControlLeaseManager.h"
#endif
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>
#if defined(ESP32) && !defined(NATIVE_BUILD)
#include <esp_system.h>
#endif

#if FEATURE_AUDIO_BENCHMARK
#include "../BenchmarkStreamBroadcaster.h"
#include "../../../audio/AudioBenchmarkMetrics.h"
#endif

#if FEATURE_EFFECT_VALIDATION
#include "../../../validation/ValidationFrameEncoder.h"
#include "../../../validation/EffectValidationMetrics.h"
#endif

#define LW_LOG_TAG "WsStreamCommands"
#include "../../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

namespace {
constexpr uint8_t RENDER_FRAME_MAGIC[4] = {'K', '1', 'F', '1'};
constexpr uint8_t RENDER_FRAME_CONTRACT_VERSION = 1;
constexpr uint8_t RENDER_PIXEL_FORMAT_RGB888 = 1;
constexpr uint16_t RENDER_FRAME_HEADER_BYTES = 16;
constexpr uint16_t RENDER_LED_COUNT = actors::RendererActor::EXTERNAL_STREAM_LED_COUNT;
constexpr uint16_t RENDER_FRAME_PAYLOAD_BYTES = actors::RendererActor::EXTERNAL_STREAM_FRAME_BYTES;
constexpr uint16_t RENDER_FRAME_TOTAL_BYTES = RENDER_FRAME_HEADER_BYTES + RENDER_FRAME_PAYLOAD_BYTES;
constexpr uint16_t RENDER_MAX_PAYLOAD_BYTES = RENDER_FRAME_PAYLOAD_BYTES;
constexpr uint8_t RENDER_MAILBOX_DEPTH = actors::RendererActor::EXTERNAL_STREAM_MAILBOX_DEPTH;
constexpr uint32_t RENDER_DEFAULT_TARGET_FPS = 120;
constexpr uint32_t RENDER_DEFAULT_STALE_TIMEOUT_MS = 750;

actors::RendererActor* s_renderer = nullptr;
AsyncWebSocket* s_ws = nullptr;

#if defined(ESP32) && !defined(NATIVE_BUILD)
portMUX_TYPE s_renderStateMux = portMUX_INITIALIZER_UNLOCKED;
inline void lockRenderState() { taskENTER_CRITICAL(&s_renderStateMux); }
inline void unlockRenderState() { taskEXIT_CRITICAL(&s_renderStateMux); }
#else
volatile uint8_t s_renderStateGuard = 0;
inline void lockRenderState() {
    while (__sync_lock_test_and_set(&s_renderStateGuard, 1) != 0) {
    }
}
inline void unlockRenderState() { __sync_lock_release(&s_renderStateGuard); }
#endif

struct RenderStreamSessionState {
    bool active = false;
    uint32_t ownerWsClientId = 0;
    char sessionId[32] = {0};
    uint32_t targetFps = RENDER_DEFAULT_TARGET_FPS;
    uint32_t staleTimeoutMs = RENDER_DEFAULT_STALE_TIMEOUT_MS;
    uint32_t startedAtMs = 0;
    uint32_t lastFrameSeq = 0;
    uint32_t lastFrameRxMs = 0;
    uint32_t framesRx = 0;
    uint32_t framesRendered = 0;
    uint32_t framesDroppedMailbox = 0;
    uint32_t framesInvalid = 0;
    uint32_t framesBlockedLease = 0;
    uint32_t staleTimeouts = 0;
};

RenderStreamSessionState s_renderState;

uint16_t readU16Le(const uint8_t* src) {
    return static_cast<uint16_t>(src[0]) | (static_cast<uint16_t>(src[1]) << 8);
}

uint32_t readU32Le(const uint8_t* src) {
    return static_cast<uint32_t>(src[0]) |
           (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) |
           (static_cast<uint32_t>(src[3]) << 24);
}

uint32_t clampU32(JsonVariantConst v, uint32_t fallback, uint32_t minValue, uint32_t maxValue) {
    if (v.isNull()) return fallback;
    int64_t raw = v.as<int64_t>();
    if (raw < static_cast<int64_t>(minValue)) return minValue;
    if (raw > static_cast<int64_t>(maxValue)) return maxValue;
    return static_cast<uint32_t>(raw);
}

void makeSessionId(char* out, size_t outSize) {
    if (!out || outSize == 0) return;
#if defined(ESP32) && !defined(NATIVE_BUILD)
    const uint32_t a = esp_random();
    const uint32_t b = esp_random();
#else
    const uint32_t a = millis() ^ 0xA53CF19Bu;
    const uint32_t b = micros() ^ 0x51D7A7F1u;
#endif
    snprintf(out, outSize, "rs_%08lx%04lx",
             static_cast<unsigned long>(a),
             static_cast<unsigned long>(b & 0xFFFFu));
}

void refreshRenderStateFromRenderer() {
    if (!s_renderer) return;
    const actors::ExternalRenderStats rendererStats = s_renderer->getExternalRenderStats();

    lockRenderState();
    s_renderState.framesRendered = rendererStats.framesRendered;
    s_renderState.framesDroppedMailbox = rendererStats.framesDroppedMailbox;
    if (rendererStats.framesInvalid > s_renderState.framesInvalid) {
        s_renderState.framesInvalid = rendererStats.framesInvalid;
    }
    s_renderState.staleTimeouts = rendererStats.staleTimeouts;
    s_renderState.lastFrameSeq = rendererStats.lastFrameSeq;
    s_renderState.lastFrameRxMs = rendererStats.lastFrameRxMs;
    const bool sessionActive = s_renderState.active;
    unlockRenderState();

    if (sessionActive && !rendererStats.active) {
        // Session falls back to internal render mode when stream stales out.
        lockRenderState();
        s_renderState.active = false;
        unlockRenderState();

        if (s_ws && s_ws->count() > 0) {
            JsonDocument event;
            event["type"] = "render.stream.stateChanged";
            event["event"] = "render.stream.stale_timeout";
            event["success"] = true;
            JsonObject data = event["data"].to<JsonObject>();
            data["active"] = false;
            data["reason"] = "stale_timeout";
            String output;
            serializeJson(event, output);
            s_ws->textAll(output);
        }
    }
}

void encodeRenderStreamData(JsonObject& data) {
    refreshRenderStateFromRenderer();

    RenderStreamSessionState snapshot;
    lockRenderState();
    snapshot = s_renderState;
    unlockRenderState();

    data["active"] = snapshot.active;
    data["sessionId"] = snapshot.sessionId;
    data["ownerWsClientId"] = snapshot.ownerWsClientId;
    data["targetFps"] = snapshot.targetFps;
    data["staleTimeoutMs"] = snapshot.staleTimeoutMs;
    data["frameContractVersion"] = RENDER_FRAME_CONTRACT_VERSION;
    data["pixelFormat"] = "rgb888";
    data["ledCount"] = RENDER_LED_COUNT;
    data["headerBytes"] = RENDER_FRAME_HEADER_BYTES;
    data["payloadBytes"] = RENDER_FRAME_PAYLOAD_BYTES;
    data["maxPayloadBytes"] = RENDER_MAX_PAYLOAD_BYTES;
    data["mailboxDepth"] = RENDER_MAILBOX_DEPTH;
    data["lastFrameSeq"] = snapshot.lastFrameSeq;
    data["lastFrameRxMs"] = snapshot.lastFrameRxMs;
    data["framesRx"] = snapshot.framesRx;
    data["framesRendered"] = snapshot.framesRendered;
    data["framesDroppedMailbox"] = snapshot.framesDroppedMailbox;
    data["framesInvalid"] = snapshot.framesInvalid;
    data["framesBlockedLease"] = snapshot.framesBlockedLease;
    data["staleTimeouts"] = snapshot.staleTimeouts;
}

void broadcastRenderStateChanged(const char* eventName, const char* reason = nullptr) {
    if (!s_ws || s_ws->count() == 0) return;

    JsonDocument event;
    event["type"] = "render.stream.stateChanged";
    event["event"] = eventName ? eventName : "render.stream.changed";
    event["success"] = true;
    JsonObject data = event["data"].to<JsonObject>();
    encodeRenderStreamData(data);
    if (reason && reason[0] != '\0') {
        data["reason"] = reason;
    }

    String output;
    serializeJson(event, output);
    s_ws->textAll(output);
}

void sendRenderStreamError(AsyncWebSocketClient* client,
                           const char* requestId,
                           const char* code,
                           const char* message,
                           const char* ownerClientName = nullptr,
                           uint32_t remainingMs = 0,
                           const char* scope = nullptr) {
    JsonDocument response;
    response["type"] = "error";
    if (requestId && requestId[0] != '\0') {
        response["requestId"] = requestId;
    }
    response["success"] = false;
    JsonObject err = response["error"].to<JsonObject>();
    err["code"] = code;
    err["message"] = message;
    if (ownerClientName) {
        err["ownerClientName"] = ownerClientName;
        err["remainingMs"] = remainingMs;
        err["scope"] = scope ? scope : "global";
    }
    String output;
    serializeJson(response, output);
    client->text(output);
}

void handleRenderStreamStart(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";

    if (!s_renderer) {
        sendRenderStreamError(client, requestId, ErrorCodes::SYSTEM_NOT_READY, "Renderer is not available");
        return;
    }

#if FEATURE_CONTROL_LEASE
    if (!core::system::ControlLeaseManager::hasActiveLease()) {
        sendRenderStreamError(client, requestId, ErrorCodes::LEASE_REQUIRED,
                              "Acquire control lease before starting external render stream");
        return;
    }

    const auto leaseCheck = core::system::ControlLeaseManager::checkMutationPermission(
        core::system::ControlLeaseManager::MutationSource::Ws,
        client->id()
    );
    if (!leaseCheck.allowed) {
        core::system::ControlLeaseManager::noteBlockedWsCommand("render.stream.start");
        sendRenderStreamError(client, requestId, ErrorCodes::CONTROL_LOCKED,
                              "Render stream start blocked by active control lease",
                              leaseCheck.state.ownerClientName.c_str(),
                              leaseCheck.remainingMs,
                              leaseCheck.state.scope.c_str());
        return;
    }
#endif

    const char* desiredPixelFormat = doc["desiredPixelFormat"] | "rgb888";
    const uint16_t desiredLedCount = static_cast<uint16_t>(doc["desiredLedCount"] | RENDER_LED_COUNT);
    if (strcmp(desiredPixelFormat, "rgb888") != 0 || desiredLedCount != RENDER_LED_COUNT) {
        sendRenderStreamError(client,
                              requestId,
                              ErrorCodes::STREAM_CONTRACT_MISMATCH,
                              "Requested render stream contract is not supported");
        return;
    }

    const uint32_t targetFps = clampU32(doc["targetFps"], RENDER_DEFAULT_TARGET_FPS, 1, 240);
    const uint32_t staleTimeoutMs = clampU32(doc["staleTimeoutMs"], RENDER_DEFAULT_STALE_TIMEOUT_MS, 250, 5000);

    lockRenderState();
    if (!s_renderState.active || s_renderState.ownerWsClientId != client->id() || s_renderState.sessionId[0] == '\0') {
        makeSessionId(s_renderState.sessionId, sizeof(s_renderState.sessionId));
    }
    s_renderState.active = true;
    s_renderState.ownerWsClientId = client->id();
    s_renderState.targetFps = targetFps;
    s_renderState.staleTimeoutMs = staleTimeoutMs;
    s_renderState.startedAtMs = millis();
    s_renderState.lastFrameSeq = 0;
    s_renderState.lastFrameRxMs = 0;
    s_renderState.framesRx = 0;
    s_renderState.framesRendered = 0;
    s_renderState.framesDroppedMailbox = 0;
    s_renderState.framesInvalid = 0;
    s_renderState.framesBlockedLease = 0;
    s_renderState.staleTimeouts = 0;
    unlockRenderState();

    s_renderer->startExternalRender(staleTimeoutMs);
    refreshRenderStateFromRenderer();

    String response = buildWsResponse("render.stream.started", requestId, [](JsonObject& data) {
        encodeRenderStreamData(data);
        data["recommendedFps"] = RENDER_DEFAULT_TARGET_FPS;
    });
    client->text(response);

    broadcastRenderStateChanged("render.stream.started", "owner_started");
}

void handleRenderStreamStop(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";
    const char* reason = doc["reason"] | "user_stop";

    RenderStreamSessionState snapshot;
    lockRenderState();
    snapshot = s_renderState;
    unlockRenderState();

    if (!snapshot.active) {
        sendRenderStreamError(client, requestId, ErrorCodes::STREAM_NOT_ACTIVE, "Render stream is not active");
        return;
    }

    if (snapshot.ownerWsClientId != client->id()) {
#if FEATURE_CONTROL_LEASE
        const auto leaseState = core::system::ControlLeaseManager::getState();
        const uint32_t remaining = core::system::ControlLeaseManager::getRemainingMs();
        sendRenderStreamError(client, requestId, ErrorCodes::CONTROL_LOCKED,
                              "Render stream is owned by another client",
                              leaseState.ownerClientName.c_str(),
                              remaining,
                              leaseState.scope.c_str());
#else
        sendRenderStreamError(client, requestId, ErrorCodes::CONTROL_LOCKED,
                              "Render stream is owned by another client");
#endif
        return;
    }

    s_renderer->stopExternalRender();
    lockRenderState();
    s_renderState.active = false;
    unlockRenderState();
    refreshRenderStateFromRenderer();

    String response = buildWsResponse("render.stream.stopped", requestId, [reason](JsonObject& data) {
        data["stopped"] = true;
        data["reason"] = reason;
        encodeRenderStreamData(data);
    });
    client->text(response);

    broadcastRenderStateChanged("render.stream.stopped", reason);
}

void handleRenderStreamStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("render.stream.status", requestId, [](JsonObject& data) {
        encodeRenderStreamData(data);
    });
    client->text(response);
}
} // namespace

static void handleLedStreamSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);
    
    uint32_t clientId = client->id();
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    // Check for UDP transport negotiation
    uint16_t udpPort = doc["udpPort"] | 0;
    if (udpPort > 0 && ctx.udpStreamer) {
        bool ok = ctx.udpStreamer->addLedSubscriber(client->remoteIP(), udpPort);
        if (ok) {
            String response = buildWsResponse("ledStream.subscribed", requestId, [clientId, udpPort](JsonObject& data) {
                data["clientId"] = clientId;
                data["transport"] = "udp";
                data["udpPort"] = udpPort;
                data["frameSize"] = webserver::LedStreamConfig::FRAME_SIZE;
                data["frameVersion"] = webserver::LedStreamConfig::FRAME_VERSION;
                data["numStrips"] = webserver::LedStreamConfig::NUM_STRIPS;
                data["ledsPerStrip"] = webserver::LedStreamConfig::LEDS_PER_STRIP;
                data["targetFps"] = webserver::LedStreamConfig::TARGET_FPS;
                data["magic"] = webserver::LedStreamConfig::MAGIC_BYTE;
            });
            client->text(response);
            LW_LOGI("LED stream: client %u subscribed via UDP port %u", clientId, udpPort);
            return;
        }
        // Fall through to WS if UDP add failed
    }

    if (!ctx.setLEDStreamSubscription) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "LED streaming not available", requestId));
        return;
    }

    bool ok = ctx.setLEDStreamSubscription(client, true);

    if (ok) {
        String response = buildWsResponse("ledStream.subscribed", requestId, [clientId](JsonObject& data) {
            codec::WsStreamCodec::encodeLedStreamSubscribed(
                clientId,
                webserver::LedStreamConfig::FRAME_SIZE,
                webserver::LedStreamConfig::FRAME_VERSION,
                webserver::LedStreamConfig::NUM_STRIPS,
                webserver::LedStreamConfig::LEDS_PER_STRIP,
                webserver::LedStreamConfig::TARGET_FPS,
                webserver::LedStreamConfig::MAGIC_BYTE,
                data
            );
            data["transport"] = "ws";
        });
        client->text(response);
    } else {
        JsonDocument response;
        codec::WsStreamCodec::encodeStreamRejected("ledStream.rejected", requestId, "RESOURCE_EXHAUSTED", "Subscriber table full", response);

        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleLedStreamUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);

    uint32_t clientId = client->id();
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";

    // Clean up UDP subscriber for this client's IP
    if (ctx.udpStreamer) {
        ctx.udpStreamer->removeSubscriber(client->remoteIP());
    }

    if (ctx.setLEDStreamSubscription) {
        ctx.setLEDStreamSubscription(client, false);
    }

    String response = buildWsResponse("ledStream.unsubscribed", requestId, [clientId](JsonObject& data) {
        codec::WsStreamCodec::encodeLedStreamUnsubscribed(clientId, data);
    });
    client->text(response);
}

#if FEATURE_EFFECT_VALIDATION
// Validation subscriber tracking (max 4 clients)
// Note: This is a global static array, matching the original implementation
static AsyncWebSocketClient* s_validationSubscribers[4] = {nullptr, nullptr, nullptr, nullptr};
static constexpr size_t MAX_VALIDATION_SUBSCRIBERS = 4;

static void handleValidationSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);
    
    uint32_t clientId = client->id();
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    bool subscribed = false;
    for (size_t i = 0; i < MAX_VALIDATION_SUBSCRIBERS; ++i) {
        if (s_validationSubscribers[i] == nullptr ||
            s_validationSubscribers[i]->status() != WS_CONNECTED) {
            s_validationSubscribers[i] = client;
            subscribed = true;
            break;
        }
        if (s_validationSubscribers[i] == client) {
            subscribed = true;
            break;
        }
    }
    
    if (subscribed) {
        String response = buildWsResponse("validation.subscribed", requestId, [clientId](JsonObject& data) {
            codec::WsStreamCodec::encodeValidationSubscribed(
                clientId,
                lightwaveos::validation::ValidationStreamConfig::SAMPLE_SIZE,
                lightwaveos::validation::ValidationStreamConfig::MAX_SAMPLES_PER_FRAME,
                lightwaveos::validation::ValidationStreamConfig::DEFAULT_DRAIN_RATE_HZ,
                data
            );
        });
        client->text(response);
    } else {
        JsonDocument response;
        codec::WsStreamCodec::encodeStreamRejected("validation.rejected", requestId, "RESOURCE_EXHAUSTED", "Validation subscriber table full", response);
        
        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleValidationUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);
    
    uint32_t clientId = client->id();
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    for (size_t i = 0; i < MAX_VALIDATION_SUBSCRIBERS; ++i) {
        if (s_validationSubscribers[i] == client) {
            s_validationSubscribers[i] = nullptr;
            break;
        }
    }
    
    String response = buildWsResponse("validation.unsubscribed", requestId, [clientId](JsonObject& data) {
        codec::WsStreamCodec::encodeValidationUnsubscribed(clientId, data);
    });
    client->text(response);
}
#endif

#if FEATURE_AUDIO_BENCHMARK
static void handleBenchmarkSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);
    
    uint32_t clientId = client->id();
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    if (!ctx.setBenchmarkStreamSubscription) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Benchmark streaming not available", requestId));
        return;
    }
    
    bool ok = ctx.setBenchmarkStreamSubscription(client, true);
    
    if (ok) {
        String response = buildWsResponse("benchmark.subscribed", requestId, [clientId](JsonObject& data) {
            codec::WsStreamCodec::encodeBenchmarkSubscribed(
                clientId,
                webserver::BenchmarkStreamConfig::COMPACT_FRAME_SIZE,
                webserver::BenchmarkStreamConfig::TARGET_FPS,
                static_cast<uint8_t>((webserver::BenchmarkStreamConfig::MAGIC >> 24) & 0xFF),
                data
            );
        });
        client->text(response);
    } else {
        JsonDocument response;
        codec::WsStreamCodec::encodeStreamRejected("benchmark.rejected", requestId, "RESOURCE_EXHAUSTED", "Subscriber table full", response);
        
        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleBenchmarkUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);
    
    uint32_t clientId = client->id();
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    if (ctx.setBenchmarkStreamSubscription) {
        ctx.setBenchmarkStreamSubscription(client, false);
    }
    
    String response = buildWsResponse("benchmark.unsubscribed", requestId, [clientId](JsonObject& data) {
        codec::WsStreamCodec::encodeBenchmarkUnsubscribed(clientId, data);
    });
    client->text(response);
}

static void handleBenchmarkStart(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    auto* audio = ctx.actorSystem.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
        return;
    }
    
    audio->resetBenchmarkStats();
    if (ctx.benchmarkBroadcaster) {
        ctx.benchmarkBroadcaster->setStreamingActive(true);
    }
    
    String response = buildWsResponse("benchmark.started", requestId, [](JsonObject& data) {
        codec::WsStreamCodec::encodeBenchmarkStarted(data);
    });
    client->text(response);
}

static void handleBenchmarkStop(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    auto* audio = ctx.actorSystem.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
        return;
    }
    
    if (ctx.benchmarkBroadcaster) {
        ctx.benchmarkBroadcaster->setStreamingActive(false);
    }
    
    const audio::AudioBenchmarkStats& stats = audio->getBenchmarkStats();
    
    String response = buildWsResponse("benchmark.stopped", requestId, [&stats](JsonObject& data) {
        codec::WsStreamCodec::encodeBenchmarkStopped(
            stats.avgTotalUs,
            stats.avgGoertzelUs,
            stats.cpuLoadPercent,
            stats.hopCount,
            stats.peakTotalUs,
            data
        );
    });
    client->text(response);
}

static void handleBenchmarkGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::StreamSimpleDecodeResult decodeResult = codec::WsStreamCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    auto* audio = ctx.actorSystem.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
        return;
    }
    
    const audio::AudioBenchmarkStats& stats = audio->getBenchmarkStats();
    bool streaming = ctx.benchmarkBroadcaster ? ctx.benchmarkBroadcaster->hasSubscribers() : false;
    
    String response = buildWsResponse("benchmark.stats", requestId, [&stats, streaming](JsonObject& data) {
        codec::WsStreamCodec::encodeBenchmarkStats(
            streaming,
            stats.avgTotalUs,
            stats.avgGoertzelUs,
            stats.avgDcAgcUs,
            stats.avgChromaUs,
            stats.peakTotalUs,
            stats.cpuLoadPercent,
            stats.hopCount,
            data
        );
    });
    client->text(response);
}
#endif

// ============================================================================
// Beat Event Subscribers
// ============================================================================

static constexpr size_t MAX_BEAT_SUBSCRIBERS = 4;
static uint32_t s_beatSubscribers[MAX_BEAT_SUBSCRIBERS] = {0};

bool hasBeatEventSubscribers() {
    for (size_t i = 0; i < MAX_BEAT_SUBSCRIBERS; ++i) {
        if (s_beatSubscribers[i] != 0) return true;
    }
    return false;
}

void removeBeatSubscriber(uint32_t clientId) {
    for (size_t i = 0; i < MAX_BEAT_SUBSCRIBERS; ++i) {
        if (s_beatSubscribers[i] == clientId) {
            s_beatSubscribers[i] = 0;
        }
    }
}

static void handleBeatSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    bool subscribed = false;
    for (size_t i = 0; i < MAX_BEAT_SUBSCRIBERS; ++i) {
        if (s_beatSubscribers[i] == clientId) {
            subscribed = true;
            break;
        }
        if (!subscribed && s_beatSubscribers[i] == 0) {
            s_beatSubscribers[i] = clientId;
            subscribed = true;
            break;
        }
    }

    if (subscribed) {
        String response = buildWsResponse("beat.subscribed", requestId, [clientId](JsonObject& data) {
            data["clientId"] = clientId;
        });
        client->text(response);
    } else {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Beat subscriber table full", requestId));
    }
}

static void handleBeatUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    removeBeatSubscriber(clientId);

    String response = buildWsResponse("beat.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
}

// ============================================================================
// External render stream ingest/service
// ============================================================================

bool handleRenderStreamBinaryFrame(AsyncWebSocketClient* client, const uint8_t* payload, size_t len) {
    if (!client || !payload || !s_renderer) {
        return false;
    }

    refreshRenderStateFromRenderer();

    RenderStreamSessionState snapshot;
    lockRenderState();
    snapshot = s_renderState;
    unlockRenderState();

    if (!snapshot.active) {
        lockRenderState();
        ++s_renderState.framesInvalid;
        unlockRenderState();
        return false;
    }

    if (snapshot.ownerWsClientId != client->id()) {
        lockRenderState();
        ++s_renderState.framesBlockedLease;
        unlockRenderState();
        return false;
    }

#if FEATURE_CONTROL_LEASE
    const auto leaseCheck = core::system::ControlLeaseManager::checkMutationPermission(
        core::system::ControlLeaseManager::MutationSource::Ws,
        client->id()
    );
    if (!leaseCheck.allowed) {
        core::system::ControlLeaseManager::noteBlockedWsCommand("render.frame.binary");
        lockRenderState();
        ++s_renderState.framesBlockedLease;
        unlockRenderState();
        return false;
    }
#endif

    if (len != RENDER_FRAME_TOTAL_BYTES) {
        lockRenderState();
        ++s_renderState.framesInvalid;
        unlockRenderState();
        return false;
    }

    if (memcmp(payload, RENDER_FRAME_MAGIC, sizeof(RENDER_FRAME_MAGIC)) != 0) {
        lockRenderState();
        ++s_renderState.framesInvalid;
        unlockRenderState();
        return false;
    }

    const uint8_t contractVersion = payload[4];
    const uint8_t pixelFormat = payload[5];
    const uint32_t seq = readU32Le(&payload[8]);
    const uint16_t ledCount = readU16Le(&payload[12]);
    const uint16_t reserved = readU16Le(&payload[14]);

    if (contractVersion != RENDER_FRAME_CONTRACT_VERSION ||
        pixelFormat != RENDER_PIXEL_FORMAT_RGB888 ||
        ledCount != RENDER_LED_COUNT ||
        reserved != 0) {
        lockRenderState();
        ++s_renderState.framesInvalid;
        unlockRenderState();
        return false;
    }

    const uint32_t rxMs = millis();
    if (!s_renderer->ingestExternalFrame(seq, payload + RENDER_FRAME_HEADER_BYTES, RENDER_FRAME_PAYLOAD_BYTES, rxMs)) {
        lockRenderState();
        ++s_renderState.framesInvalid;
        unlockRenderState();
        return false;
    }

    lockRenderState();
    ++s_renderState.framesRx;
    s_renderState.lastFrameSeq = seq;
    s_renderState.lastFrameRxMs = rxMs;
    unlockRenderState();
    return true;
}

void handleRenderStreamClientDisconnect(uint32_t clientId) {
    refreshRenderStateFromRenderer();
    lockRenderState();
    const bool shouldStop = s_renderState.active && s_renderState.ownerWsClientId == clientId;
    unlockRenderState();
    if (!shouldStop) {
        return;
    }

    if (s_renderer) {
        s_renderer->stopExternalRender();
    }
    lockRenderState();
    s_renderState.active = false;
    unlockRenderState();
    broadcastRenderStateChanged("render.stream.stopped", "owner_disconnected");
}

void serviceRenderStreamState() {
    refreshRenderStateFromRenderer();

    if (!s_renderer) {
        return;
    }

    lockRenderState();
    const bool active = s_renderState.active;
    const uint32_t ownerClientId = s_renderState.ownerWsClientId;
    unlockRenderState();

    if (!active) {
        return;
    }

#if FEATURE_CONTROL_LEASE
    if (!core::system::ControlLeaseManager::isWsOwner(ownerClientId)) {
        s_renderer->stopExternalRender();
        lockRenderState();
        s_renderState.active = false;
        unlockRenderState();
        broadcastRenderStateChanged("render.stream.stopped", "lease_lost");
        return;
    }
#endif

    const actors::ExternalRenderStats rendererStats = s_renderer->getExternalRenderStats();
    if (!rendererStats.active) {
        lockRenderState();
        s_renderState.active = false;
        unlockRenderState();
        broadcastRenderStateChanged("render.stream.stopped", "renderer_inactive");
    }
}

RenderStreamStatusSnapshot getRenderStreamStatusSnapshot() {
    refreshRenderStateFromRenderer();
    RenderStreamSessionState snapshot;
    lockRenderState();
    snapshot = s_renderState;
    unlockRenderState();

    RenderStreamStatusSnapshot out;
    out.active = snapshot.active;
    out.sessionId = snapshot.sessionId;
    out.ownerWsClientId = snapshot.ownerWsClientId;
    out.targetFps = snapshot.targetFps;
    out.staleTimeoutMs = snapshot.staleTimeoutMs;
    out.frameContractVersion = RENDER_FRAME_CONTRACT_VERSION;
    out.pixelFormat = RENDER_PIXEL_FORMAT_RGB888;
    out.ledCount = RENDER_LED_COUNT;
    out.headerBytes = RENDER_FRAME_HEADER_BYTES;
    out.payloadBytes = RENDER_FRAME_PAYLOAD_BYTES;
    out.maxPayloadBytes = RENDER_MAX_PAYLOAD_BYTES;
    out.mailboxDepth = RENDER_MAILBOX_DEPTH;
    out.lastFrameSeq = snapshot.lastFrameSeq;
    out.lastFrameRxMs = snapshot.lastFrameRxMs;
    out.framesRx = snapshot.framesRx;
    out.framesRendered = snapshot.framesRendered;
    out.framesDroppedMailbox = snapshot.framesDroppedMailbox;
    out.framesInvalid = snapshot.framesInvalid;
    out.framesBlockedLease = snapshot.framesBlockedLease;
    out.staleTimeouts = snapshot.staleTimeouts;
    return out;
}

// ============================================================================
// Registration
// ============================================================================

void registerWsStreamCommands(const WebServerContext& ctx) {
    s_renderer = ctx.renderer;
    s_ws = ctx.ws;

    WsCommandRouter::registerCommand("render.stream.start", handleRenderStreamStart);
    WsCommandRouter::registerCommand("render.stream.stop", handleRenderStreamStop);
    WsCommandRouter::registerCommand("render.stream.status", handleRenderStreamStatus);

    WsCommandRouter::registerCommand("ledStream.subscribe", handleLedStreamSubscribe);
    WsCommandRouter::registerCommand("ledStream.unsubscribe", handleLedStreamUnsubscribe);

#if FEATURE_EFFECT_VALIDATION
    WsCommandRouter::registerCommand("validation.subscribe", handleValidationSubscribe);
    WsCommandRouter::registerCommand("validation.unsubscribe", handleValidationUnsubscribe);
#endif

#if FEATURE_AUDIO_BENCHMARK
    WsCommandRouter::registerCommand("benchmark.subscribe", handleBenchmarkSubscribe);
    WsCommandRouter::registerCommand("benchmark.unsubscribe", handleBenchmarkUnsubscribe);
    WsCommandRouter::registerCommand("benchmark.start", handleBenchmarkStart);
    WsCommandRouter::registerCommand("benchmark.stop", handleBenchmarkStop);
    WsCommandRouter::registerCommand("benchmark.get", handleBenchmarkGet);
#endif

    // Beat event subscription (gated: no textAll() spam to non-subscribing clients)
    WsCommandRouter::registerCommand("beat.subscribe", handleBeatSubscribe);
    WsCommandRouter::registerCommand("beat.unsubscribe", handleBeatUnsubscribe);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
