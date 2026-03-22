/**
 * @file WsStreamCommands.cpp
 * @brief WebSocket stream subscription command handlers implementation
 */

#include "WsStreamCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#if FEATURE_INPUT_MERGE_LAYER
#include "../../../core/actors/ActorSystem.h"
#endif
#include "../../ApiResponse.h"
#include "../LedStreamBroadcaster.h"
#include "../UdpStreamer.h"
#include "../../../codec/WsStreamCodec.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

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

#if FEATURE_VRMS_METRICS

static constexpr size_t MAX_VRMS_SUBSCRIBERS = 4;
static uint32_t s_vrmsSubscribers[MAX_VRMS_SUBSCRIBERS] = {0};

bool hasVrmsStreamSubscribers() {
    for (size_t i = 0; i < MAX_VRMS_SUBSCRIBERS; ++i) {
        if (s_vrmsSubscribers[i] != 0) return true;
    }
    return false;
}

void removeVrmsSubscriber(uint32_t clientId) {
    for (size_t i = 0; i < MAX_VRMS_SUBSCRIBERS; ++i) {
        if (s_vrmsSubscribers[i] == clientId) {
            s_vrmsSubscribers[i] = 0;
        }
    }
}

uint32_t* getVrmsSubscriberTable(size_t& outCount) {
    outCount = MAX_VRMS_SUBSCRIBERS;
    return s_vrmsSubscribers;
}

static void handleVrmsSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    bool subscribed = false;
    for (size_t i = 0; i < MAX_VRMS_SUBSCRIBERS; ++i) {
        if (s_vrmsSubscribers[i] == clientId) { subscribed = true; break; }
        if (!subscribed && s_vrmsSubscribers[i] == 0) { s_vrmsSubscribers[i] = clientId; subscribed = true; break; }
    }

    if (subscribed) {
        String response = buildWsResponse("vrms.subscribed", requestId, [clientId](JsonObject& data) {
            data["clientId"] = clientId;
            data["intervalMs"] = 100;
        });
        client->text(response);
    } else {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "VRMS subscriber table full", requestId));
    }
}

static void handleVrmsUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";
    removeVrmsSubscriber(clientId);
    String response = buildWsResponse("vrms.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
}
#endif

// ============================================================================
// Input Merge Layer — AI Agent Parameter Submission
// ============================================================================

#if FEATURE_INPUT_MERGE_LAYER

// Param name → index lookup (matches InputMergeLayer.h constants)
static int8_t mergeParamIndex(const char* name) {
    if (!name) return -1;
    if (strcmp(name, "brightness") == 0) return 0;
    if (strcmp(name, "speed") == 0) return 1;
    if (strcmp(name, "intensity") == 0) return 2;
    if (strcmp(name, "saturation") == 0) return 3;
    if (strcmp(name, "complexity") == 0) return 4;
    if (strcmp(name, "variation") == 0) return 5;
    if (strcmp(name, "hue") == 0) return 6;
    if (strcmp(name, "mood") == 0) return 7;
    if (strcmp(name, "fadeAmount") == 0) return 8;
    if (strcmp(name, "paletteIndex") == 0) return 9;
    return -1;
}

static void handleMergeSubmit(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t sourceId = doc["source"] | 2;  // Default: AI_AGENT (2)

    // Validate source (only AI_AGENT=2 and GESTURE=3 allowed via WS)
    if (sourceId < 2 || sourceId > 3) {
        client->text(buildWsError(ErrorCodes::INVALID_PARAMETER,
            "source must be 2 (ai_agent) or 3 (gesture)", requestId));
        return;
    }

    auto* renderer = ctx.actorSystem.getRenderer();
    if (!renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }

    // Parse params object: {"brightness": 128, "speed": 50, ...}
    JsonObject params = doc["params"];
    if (params.isNull()) {
        client->text(buildWsError(ErrorCodes::INVALID_PARAMETER,
            "Missing 'params' object", requestId));
        return;
    }

    uint8_t count = 0;
    for (JsonPair kv : params) {
        int8_t idx = mergeParamIndex(kv.key().c_str());
        if (idx < 0) continue;
        uint8_t val = kv.value().as<uint8_t>();

        lightwaveos::actors::Message msg;
        msg.type = lightwaveos::actors::MessageType::MERGE_SUBMIT;
        msg.param1 = sourceId;
        msg.param2 = static_cast<uint8_t>(idx);
        msg.param3 = val;
        msg.timestamp = millis();
        renderer->send(msg);
        count++;
    }

    String response = buildWsResponse("merge.accepted", requestId,
        [count, sourceId](JsonObject& data) {
            data["paramsApplied"] = count;
            data["source"] = sourceId;
        });
    client->text(response);
}
#endif  // FEATURE_INPUT_MERGE_LAYER

// ============================================================================
// Registration
// ============================================================================

void registerWsStreamCommands(const WebServerContext& ctx) {
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

#if FEATURE_VRMS_METRICS
    WsCommandRouter::registerCommand("vrms.subscribe", handleVrmsSubscribe);
    WsCommandRouter::registerCommand("vrms.unsubscribe", handleVrmsUnsubscribe);
#endif

#if FEATURE_INPUT_MERGE_LAYER
    WsCommandRouter::registerCommand("merge.submit", handleMergeSubmit);
#endif
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

