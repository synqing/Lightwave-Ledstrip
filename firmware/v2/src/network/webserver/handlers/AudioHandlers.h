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
namespace nodes {
class NodeOrchestrator;
class RendererNode;
}
namespace audio {
class AudioNode;
}
namespace network {
namespace webserver {
#if FEATURE_AUDIO_BENCHMARK
class BenchmarkStreamBroadcaster;
#endif
} // namespace webserver
} // namespace network
}

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
                                    lightwaveos::nodes::NodeOrchestrator& orchestrator,
                                    lightwaveos::nodes::RendererNode* renderer);
    
    static void handleParametersSet(AsyncWebServerRequest* request,
                                    uint8_t* data, size_t len,
                                    lightwaveos::nodes::NodeOrchestrator& orchestrator,
                                    lightwaveos::nodes::RendererNode* renderer);

    // Audio Control
    static void handleControl(AsyncWebServerRequest* request,
                              uint8_t* data, size_t len,
                              lightwaveos::nodes::NodeOrchestrator& orchestrator);
    
    // AGC Toggle
    static void handleAGCToggle(AsyncWebServerRequest* request,
                                uint8_t* data, size_t len,
                                lightwaveos::nodes::NodeOrchestrator& orchestrator);

    // Audio State
    static void handleStateGet(AsyncWebServerRequest* request,
                               lightwaveos::nodes::NodeOrchestrator& orchestrator);

    // Audio Tempo
    static void handleTempoGet(AsyncWebServerRequest* request,
                                lightwaveos::nodes::NodeOrchestrator& orchestrator);

    // Audio Presets
    static void handlePresetsList(AsyncWebServerRequest* request);
    
    static void handlePresetGet(AsyncWebServerRequest* request, uint8_t presetId);
    
    static void handlePresetSave(AsyncWebServerRequest* request,
                                  uint8_t* data, size_t len,
                                  lightwaveos::nodes::NodeOrchestrator& orchestrator,
                                  lightwaveos::nodes::RendererNode* renderer);
    
    static void handlePresetApply(AsyncWebServerRequest* request, uint8_t presetId,
                                   lightwaveos::nodes::NodeOrchestrator& orchestrator,
                                   lightwaveos::nodes::RendererNode* renderer);
    
    static void handlePresetDelete(AsyncWebServerRequest* request, uint8_t presetId);

    // Audio-Effect Mappings
    static void handleMappingsListSources(AsyncWebServerRequest* request);
    
    static void handleMappingsListTargets(AsyncWebServerRequest* request);
    
    static void handleMappingsListCurves(AsyncWebServerRequest* request);
    
    static void handleMappingsList(AsyncWebServerRequest* request,
                                    lightwaveos::nodes::RendererNode* renderer);
    
    static void handleMappingsGet(AsyncWebServerRequest* request, uint8_t effectId,
                                   lightwaveos::nodes::RendererNode* renderer);
    
    static void handleMappingsSet(AsyncWebServerRequest* request, uint8_t effectId,
                                  uint8_t* data, size_t len,
                                  lightwaveos::nodes::RendererNode* renderer);
    
    static void handleMappingsDelete(AsyncWebServerRequest* request, uint8_t effectId);
    
    static void handleMappingsEnable(AsyncWebServerRequest* request, uint8_t effectId, bool enable);
    
    static void handleMappingsStats(AsyncWebServerRequest* request);

    // Zone AGC
    static void handleZoneAGCGet(AsyncWebServerRequest* request,
                                  lightwaveos::nodes::NodeOrchestrator& orchestrator);
    
    static void handleZoneAGCSet(AsyncWebServerRequest* request,
                                  uint8_t* data, size_t len,
                                  lightwaveos::nodes::NodeOrchestrator& orchestrator);

    // Spike Detection
    static void handleSpikeDetectionGet(AsyncWebServerRequest* request,
                                         lightwaveos::nodes::NodeOrchestrator& orchestrator);
    
    static void handleSpikeDetectionReset(AsyncWebServerRequest* request,
                                           lightwaveos::nodes::NodeOrchestrator& orchestrator);

    // Noise Calibration
    static void handleCalibrateStatus(AsyncWebServerRequest* request,
                                       lightwaveos::nodes::NodeOrchestrator& orchestrator);
    
    static void handleCalibrateStart(AsyncWebServerRequest* request,
                                      uint8_t* data, size_t len,
                                      lightwaveos::nodes::NodeOrchestrator& orchestrator);
    
    static void handleCalibrateCancel(AsyncWebServerRequest* request,
                                       lightwaveos::nodes::NodeOrchestrator& orchestrator);
    
    static void handleCalibrateApply(AsyncWebServerRequest* request,
                                      lightwaveos::nodes::NodeOrchestrator& orchestrator);

#if FEATURE_AUDIO_BENCHMARK
    // Benchmark
    static void handleBenchmarkGet(AsyncWebServerRequest* request,
                                    lightwaveos::nodes::NodeOrchestrator& orchestrator,
                                    lightwaveos::network::webserver::BenchmarkStreamBroadcaster* broadcaster);
    
    static void handleBenchmarkStart(AsyncWebServerRequest* request,
                                      lightwaveos::nodes::NodeOrchestrator& orchestrator,
                                      lightwaveos::network::webserver::BenchmarkStreamBroadcaster* broadcaster);
    
    static void handleBenchmarkStop(AsyncWebServerRequest* request,
                                     lightwaveos::nodes::NodeOrchestrator& orchestrator,
                                     lightwaveos::network::webserver::BenchmarkStreamBroadcaster* broadcaster);
    
    static void handleBenchmarkHistory(AsyncWebServerRequest* request,
                                        lightwaveos::nodes::NodeOrchestrator& orchestrator);
#endif
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
