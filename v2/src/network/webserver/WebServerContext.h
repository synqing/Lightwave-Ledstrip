/**
 * @file WebServerContext.h
 * @brief Shared context for WebServer modules
 *
 * Provides non-owning references to business systems and state,
 * enabling modules to access dependencies without circular dependencies.
 * All references are guaranteed valid during WebServer lifetime.
 */

#pragma once

#include "../../config/features.h"
#include <stdint.h>
#include <functional>
#include <ArduinoJson.h>

// Forward declarations
namespace lightwaveos {
    namespace actors {
        class ActorSystem;
        class RendererActor;
    }
    namespace zones {
        class ZoneComposer;
    }
    namespace network {
        namespace webserver {
            class RateLimiter;
            class LedStreamBroadcaster;
#if FEATURE_AUDIO_SYNC
            class AudioStreamBroadcaster;
#endif
#if FEATURE_AUDIO_BENCHMARK
            class BenchmarkStreamBroadcaster;
#endif
        }
    }
}
class AsyncWebSocket;
class AsyncWebSocketClient;

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief Shared context for WebServer modules
 *
 * Contains non-owning references to all business systems and state
 * needed by route handlers and WebSocket command handlers.
 * This breaks circular dependencies and enables SOLID design.
 */
struct WebServerContext {
    // Business systems (non-owning references)
    actors::ActorSystem& actorSystem;
    actors::RendererActor* renderer;
    zones::ZoneComposer* zoneComposer;

    // Cross-cutting concerns
    RateLimiter& rateLimiter;

    // Streaming broadcasters
    LedStreamBroadcaster* ledBroadcaster;
#if FEATURE_AUDIO_SYNC
    AudioStreamBroadcaster* audioBroadcaster;
#endif
#if FEATURE_AUDIO_BENCHMARK
    BenchmarkStreamBroadcaster* benchmarkBroadcaster;
#endif

    // State flags
    uint32_t startTime;
    bool apMode;

    // WebServer callbacks (for commands that need to broadcast or manage subscriptions)
    std::function<void()> broadcastStatus;
    std::function<void()> broadcastZoneState;
    AsyncWebSocket* ws;  // For broadcasting events to all clients
    std::function<bool(AsyncWebSocketClient*, bool)> setLEDStreamSubscription;
#if FEATURE_AUDIO_SYNC
    std::function<bool(AsyncWebSocketClient*, bool)> setAudioStreamSubscription;
#endif
#if FEATURE_EFFECT_VALIDATION
    std::function<bool(AsyncWebSocketClient*, bool)> setValidationStreamSubscription;
#endif
#if FEATURE_AUDIO_BENCHMARK
    std::function<bool(AsyncWebSocketClient*, bool)> setBenchmarkStreamSubscription;
#endif
    std::function<bool(const String&, JsonVariant)> executeBatchAction;

    /**
     * @brief Construct context with all required references
     */
    WebServerContext(
        actors::ActorSystem& actors,
        actors::RendererActor* rendererPtr,
        zones::ZoneComposer* zoneComposerPtr,
        RateLimiter& limiter,
        LedStreamBroadcaster* ledBroadcast,
#if FEATURE_AUDIO_SYNC
        AudioStreamBroadcaster* audioBroadcast,
#endif
#if FEATURE_AUDIO_BENCHMARK
        BenchmarkStreamBroadcaster* benchmarkBroadcast,
#endif
        uint32_t startTimeMs,
        bool isAPMode,
        std::function<void()> broadcastStatusFn = nullptr,
        std::function<void()> broadcastZoneStateFn = nullptr,
        AsyncWebSocket* wsPtr = nullptr,
        std::function<bool(AsyncWebSocketClient*, bool)> setLEDStreamFn = nullptr,
#if FEATURE_AUDIO_SYNC
        std::function<bool(AsyncWebSocketClient*, bool)> setAudioStreamFn = nullptr,
#endif
#if FEATURE_EFFECT_VALIDATION
        std::function<bool(AsyncWebSocketClient*, bool)> setValidationStreamFn = nullptr,
#endif
#if FEATURE_AUDIO_BENCHMARK
        std::function<bool(AsyncWebSocketClient*, bool)> setBenchmarkStreamFn = nullptr,
#endif
        std::function<bool(const String&, JsonVariant)> executeBatchFn = nullptr
    )
        : actorSystem(actors)
        , renderer(rendererPtr)
        , zoneComposer(zoneComposerPtr)
        , rateLimiter(limiter)
        , ledBroadcaster(ledBroadcast)
#if FEATURE_AUDIO_SYNC
        , audioBroadcaster(audioBroadcast)
#endif
#if FEATURE_AUDIO_BENCHMARK
        , benchmarkBroadcaster(benchmarkBroadcast)
#endif
        , startTime(startTimeMs)
        , apMode(isAPMode)
        , broadcastStatus(broadcastStatusFn)
        , broadcastZoneState(broadcastZoneStateFn)
        , ws(wsPtr)
        , setLEDStreamSubscription(setLEDStreamFn)
#if FEATURE_AUDIO_SYNC
        , setAudioStreamSubscription(setAudioStreamFn)
#endif
#if FEATURE_EFFECT_VALIDATION
        , setValidationStreamSubscription(setValidationStreamFn)
#endif
#if FEATURE_AUDIO_BENCHMARK
        , setBenchmarkStreamSubscription(setBenchmarkStreamFn)
#endif
        , executeBatchAction(executeBatchFn)
    {}
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

