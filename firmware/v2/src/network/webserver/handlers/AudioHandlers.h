/**
 * @file AudioHandlers.h
 * @brief Audio-related HTTP handlers
 *
 * Extracted from WebServer for better separation of concerns.
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Forward declarations
namespace lightwaveos {
namespace actors {
class ActorSystem;
class RendererActor;
}
}

// Include AudioActor header for namespace alias (cannot forward-declare using alias)
#include "../../../audio/AudioActor.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Audio-related HTTP handlers
 */
class AudioHandlers {
public:
    // Audio Parameters
    static void handleParametersGet(AsyncWebServerRequest* request,
                                    lightwaveos::actors::ActorSystem& actorSystem,
                                    lightwaveos::actors::RendererActor* renderer);
    
    static void handleParametersSet(AsyncWebServerRequest* request,
                                    uint8_t* data, size_t len,
                                    lightwaveos::actors::ActorSystem& actorSystem,
                                    lightwaveos::actors::RendererActor* renderer);

    // Audio Control
    static void handleControl(AsyncWebServerRequest* request,
                              uint8_t* data, size_t len,
                              lightwaveos::actors::ActorSystem& actorSystem);

    // Audio State
    static void handleStateGet(AsyncWebServerRequest* request,
                               lightwaveos::actors::ActorSystem& actorSystem);

    // Audio Tempo
    static void handleTempoGet(AsyncWebServerRequest* request,
                                lightwaveos::actors::ActorSystem& actorSystem);

    // Audio Presets
    static void handlePresetsList(AsyncWebServerRequest* request);
    
    static void handlePresetGet(AsyncWebServerRequest* request, uint8_t presetId);
    
    static void handlePresetSave(AsyncWebServerRequest* request,
                                  uint8_t* data, size_t len,
                                  lightwaveos::actors::ActorSystem& actorSystem,
                                  lightwaveos::actors::RendererActor* renderer);
    
    static void handlePresetApply(AsyncWebServerRequest* request, uint8_t presetId,
                                   lightwaveos::actors::ActorSystem& actorSystem,
                                   lightwaveos::actors::RendererActor* renderer);
    
    static void handlePresetDelete(AsyncWebServerRequest* request, uint8_t presetId);

    // Audio-Effect Mappings
    static void handleMappingsListSources(AsyncWebServerRequest* request);
    
    static void handleMappingsListTargets(AsyncWebServerRequest* request);
    
    static void handleMappingsListCurves(AsyncWebServerRequest* request);
    
    static void handleMappingsList(AsyncWebServerRequest* request,
                                    lightwaveos::actors::RendererActor* renderer);
    
    static void handleMappingsGet(AsyncWebServerRequest* request, uint8_t effectId,
                                   lightwaveos::actors::RendererActor* renderer);
    
    static void handleMappingsSet(AsyncWebServerRequest* request, uint8_t effectId,
                                  uint8_t* data, size_t len,
                                  lightwaveos::actors::RendererActor* renderer);
    
    static void handleMappingsDelete(AsyncWebServerRequest* request, uint8_t effectId);
    
    static void handleMappingsEnable(AsyncWebServerRequest* request, uint8_t effectId, bool enable);
    
    static void handleMappingsStats(AsyncWebServerRequest* request);

    // Zone AGC
    static void handleZoneAGCGet(AsyncWebServerRequest* request,
                                  lightwaveos::actors::ActorSystem& actorSystem);
    
    static void handleZoneAGCSet(AsyncWebServerRequest* request,
                                  uint8_t* data, size_t len,
                                  lightwaveos::actors::ActorSystem& actorSystem);

    // Spike Detection
    static void handleSpikeDetectionGet(AsyncWebServerRequest* request,
                                         lightwaveos::actors::ActorSystem& actorSystem);
    
    static void handleSpikeDetectionReset(AsyncWebServerRequest* request,
                                           lightwaveos::actors::ActorSystem& actorSystem);

    // Noise Calibration
    static void handleCalibrateStatus(AsyncWebServerRequest* request,
                                       lightwaveos::actors::ActorSystem& actorSystem);
    
    static void handleCalibrateStart(AsyncWebServerRequest* request,
                                      uint8_t* data, size_t len,
                                      lightwaveos::actors::ActorSystem& actorSystem);
    
    static void handleCalibrateCancel(AsyncWebServerRequest* request,
                                       lightwaveos::actors::ActorSystem& actorSystem);
    
    static void handleCalibrateApply(AsyncWebServerRequest* request,
                                      lightwaveos::actors::ActorSystem& actorSystem);

#if FEATURE_AUDIO_BENCHMARK
    // Benchmark
    static void handleBenchmarkGet(AsyncWebServerRequest* request,
                                    lightwaveos::actors::ActorSystem& actorSystem,
                                    std::function<bool()> hasSubscribers);
    
    static void handleBenchmarkStart(AsyncWebServerRequest* request,
                                      lightwaveos::actors::ActorSystem& actorSystem,
                                      std::function<void(bool)> setStreamingActive);
    
    static void handleBenchmarkStop(AsyncWebServerRequest* request,
                                     lightwaveos::actors::ActorSystem& actorSystem,
                                     std::function<void(bool)> setStreamingActive);
    
    static void handleBenchmarkHistory(AsyncWebServerRequest* request,
                                        lightwaveos::actors::ActorSystem& actorSystem);
#endif
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

