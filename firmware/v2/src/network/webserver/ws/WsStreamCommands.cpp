/**
 * @file WsStreamCommands.cpp
 * @brief WebSocket stream subscription command handlers implementation
 */

#include "WsStreamCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../LedStreamBroadcaster.h"
#include "../LogStreamBroadcaster.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#if FEATURE_AUDIO_BENCHMARK
#include "../../../core/actors/NodeOrchestrator.h"
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
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.setLEDStreamSubscription) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "LED streaming not available", requestId));
        return;
    }
    
    bool ok = ctx.setLEDStreamSubscription(client, true);
    
    if (ok) {
        String response = buildWsResponse("ledStream.subscribed", requestId, [clientId](JsonObject& data) {
            data["clientId"] = clientId;
            data["frameSize"] = webserver::LedStreamConfig::FRAME_SIZE;
            data["frameVersion"] = webserver::LedStreamConfig::FRAME_VERSION;
            data["numStrips"] = webserver::LedStreamConfig::NUM_STRIPS;
            data["ledsPerStrip"] = webserver::LedStreamConfig::LEDS_PER_STRIP;
            data["targetFps"] = webserver::LedStreamConfig::TARGET_FPS;
            data["magicByte"] = webserver::LedStreamConfig::MAGIC_BYTE;
            data["accepted"] = true;
        });
        client->text(response);
    } else {
        JsonDocument response;
        response["type"] = "ledStream.rejected";
        if (requestId != nullptr && strlen(requestId) > 0) {
            response["requestId"] = requestId;
        }
        response["success"] = false;
        JsonObject error = response["error"].to<JsonObject>();
        error["code"] = "RESOURCE_EXHAUSTED";
        error["message"] = "Subscriber table full";
        
        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleLedStreamUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    if (ctx.setLEDStreamSubscription) {
        ctx.setLEDStreamSubscription(client, false);
    }

    String response = buildWsResponse("ledStream.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
}

// ============================================================================
// Log Stream Commands (Wireless Serial Monitoring)
// ============================================================================

static void handleLogStreamSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    if (!ctx.setLogStreamSubscription) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Log streaming not available", requestId));
        return;
    }

    bool ok = ctx.setLogStreamSubscription(client, true);

    if (ok) {
        size_t backfillCount = ctx.logBroadcaster ? ctx.logBroadcaster->getBackfillCount() : 0;
        String response = buildWsResponse("logStream.subscribed", requestId, [clientId, backfillCount](JsonObject& data) {
            data["clientId"] = clientId;
            data["backfillCount"] = backfillCount;
            data["maxMessageLength"] = webserver::LogStreamConfig::MAX_MESSAGE_LENGTH;
            data["accepted"] = true;
        });
        client->text(response);
        LW_LOGI("Log stream subscriber added: client %lu", clientId);
    } else {
        JsonDocument response;
        response["type"] = "logStream.rejected";
        if (requestId != nullptr && strlen(requestId) > 0) {
            response["requestId"] = requestId;
        }
        response["success"] = false;
        JsonObject error = response["error"].to<JsonObject>();
        error["code"] = "RESOURCE_EXHAUSTED";
        error["message"] = "Log subscriber table full";

        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleLogStreamUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    if (ctx.setLogStreamSubscription) {
        ctx.setLogStreamSubscription(client, false);
    }

    String response = buildWsResponse("logStream.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
    LW_LOGI("Log stream subscriber removed: client %lu", clientId);
}

#if FEATURE_EFFECT_VALIDATION
// Validation subscriber tracking (max 4 clients)
// Note: This is a global static array, matching the original implementation
static AsyncWebSocketClient* s_validationSubscribers[4] = {nullptr, nullptr, nullptr, nullptr};
static constexpr size_t MAX_VALIDATION_SUBSCRIBERS = 4;

static void handleValidationSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";
    
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
            data["clientId"] = clientId;
            data["sampleSize"] = lightwaveos::validation::ValidationStreamConfig::SAMPLE_SIZE;
            data["maxSamplesPerFrame"] = lightwaveos::validation::ValidationStreamConfig::MAX_SAMPLES_PER_FRAME;
            data["targetFps"] = lightwaveos::validation::ValidationStreamConfig::DEFAULT_DRAIN_RATE_HZ;
            data["accepted"] = true;
        });
        client->text(response);
    } else {
        JsonDocument response;
        response["type"] = "validation.rejected";
        if (requestId != nullptr && strlen(requestId) > 0) {
            response["requestId"] = requestId;
        }
        response["success"] = false;
        JsonObject error = response["error"].to<JsonObject>();
        error["code"] = "RESOURCE_EXHAUSTED";
        error["message"] = "Validation subscriber table full";
        
        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleValidationUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";
    
    for (size_t i = 0; i < MAX_VALIDATION_SUBSCRIBERS; ++i) {
        if (s_validationSubscribers[i] == client) {
            s_validationSubscribers[i] = nullptr;
            break;
        }
    }
    
    String response = buildWsResponse("validation.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
}
#endif

#if FEATURE_AUDIO_BENCHMARK
static void handleBenchmarkSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.setBenchmarkStreamSubscription) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Benchmark streaming not available", requestId));
        return;
    }
    
    bool ok = ctx.setBenchmarkStreamSubscription(client, true);
    
    if (ok) {
        String response = buildWsResponse("benchmark.subscribed", requestId, [clientId](JsonObject& data) {
            data["clientId"] = clientId;
            data["frameSize"] = webserver::BenchmarkStreamConfig::COMPACT_FRAME_SIZE;
            data["targetFps"] = webserver::BenchmarkStreamConfig::TARGET_FPS;
            data["magicByte"] = (webserver::BenchmarkStreamConfig::MAGIC >> 24) & 0xFF;
            data["accepted"] = true;
        });
        client->text(response);
    } else {
        JsonDocument response;
        response["type"] = "benchmark.rejected";
        if (requestId != nullptr && strlen(requestId) > 0) {
            response["requestId"] = requestId;
        }
        response["success"] = false;
        JsonObject error = response["error"].to<JsonObject>();
        error["code"] = "RESOURCE_EXHAUSTED";
        error["message"] = "Subscriber table full";
        
        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleBenchmarkUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";
    
    if (ctx.setBenchmarkStreamSubscription) {
        ctx.setBenchmarkStreamSubscription(client, false);
    }
    
    String response = buildWsResponse("benchmark.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
}

static void handleBenchmarkStart(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
        return;
    }
    
    audio->resetBenchmarkStats();
    if (ctx.benchmarkBroadcaster) {
        ctx.benchmarkBroadcaster->setStreamingActive(true);
    }
    
    String response = buildWsResponse("benchmark.started", requestId, [](JsonObject& data) {
        data["active"] = true;
    });
    client->text(response);
}

static void handleBenchmarkStop(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
        return;
    }
    
    if (ctx.benchmarkBroadcaster) {
        ctx.benchmarkBroadcaster->setStreamingActive(false);
    }
    
    const audio::AudioBenchmarkStats& stats = audio->getBenchmarkStats();
    
    String response = buildWsResponse("benchmark.stopped", requestId, [&stats](JsonObject& data) {
        data["active"] = false;
        JsonObject results = data["results"].to<JsonObject>();
        results["avgTotalUs"] = stats.avgTotalUs;
        results["avgGoertzelUs"] = stats.avgGoertzelUs;
        results["cpuLoadPercent"] = stats.cpuLoadPercent;
        results["hopCount"] = stats.hopCount;
        results["peakTotalUs"] = stats.peakTotalUs;
    });
    client->text(response);
}

static void handleBenchmarkGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
        return;
    }
    
    const audio::AudioBenchmarkStats& stats = audio->getBenchmarkStats();
    bool streaming = ctx.benchmarkBroadcaster ? ctx.benchmarkBroadcaster->hasSubscribers() : false;
    
    String response = buildWsResponse("benchmark.stats", requestId, [&stats, streaming](JsonObject& data) {
        data["streaming"] = streaming;
        
        JsonObject timing = data["timing"].to<JsonObject>();
        timing["avgTotalUs"] = stats.avgTotalUs;
        timing["avgGoertzelUs"] = stats.avgGoertzelUs;
        timing["avgDcAgcUs"] = stats.avgDcAgcUs;
        timing["avgChromaUs"] = stats.avgChromaUs;
        timing["peakTotalUs"] = stats.peakTotalUs;
        
        JsonObject load = data["load"].to<JsonObject>();
        load["cpuPercent"] = stats.cpuLoadPercent;
        load["hopCount"] = stats.hopCount;
    });
    client->text(response);
}
#endif

void registerWsStreamCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("ledStream.subscribe", handleLedStreamSubscribe);
    WsCommandRouter::registerCommand("ledStream.unsubscribe", handleLedStreamUnsubscribe);

    // Log stream (wireless serial monitoring)
    WsCommandRouter::registerCommand("logStream.subscribe", handleLogStreamSubscribe);
    WsCommandRouter::registerCommand("logStream.unsubscribe", handleLogStreamUnsubscribe);

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
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
