/**
 * @file WsNarrativeCommands.cpp
 * @brief WebSocket narrative command handlers implementation
 */

#include "WsNarrativeCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/narrative/NarrativeEngine.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::narrative;

static void handleNarrativeGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    
    String response = buildWsResponse("narrative.status", requestId, [&narrative](JsonObject& data) {
        data["enabled"] = narrative.isEnabled();
        data["tension"] = narrative.getTension() * 100.0f;
        data["phaseT"] = narrative.getPhaseT();
        data["cycleT"] = narrative.getCycleT();
        
        NarrativePhase phase = narrative.getPhase();
        const char* phaseName = "UNKNOWN";
        switch (phase) {
            case PHASE_BUILD:   phaseName = "BUILD"; break;
            case PHASE_HOLD:    phaseName = "HOLD"; break;
            case PHASE_RELEASE: phaseName = "RELEASE"; break;
            case PHASE_REST:    phaseName = "REST"; break;
        }
        data["phase"] = phaseName;
        data["phaseId"] = static_cast<uint8_t>(phase);
        
        JsonObject durations = data["durations"].to<JsonObject>();
        durations["build"] = narrative.getBuildDuration();
        durations["hold"] = narrative.getHoldDuration();
        durations["release"] = narrative.getReleaseDuration();
        durations["rest"] = narrative.getRestDuration();
        durations["total"] = narrative.getTotalDuration();
        
        data["tempoMultiplier"] = narrative.getTempoMultiplier();
        data["complexityScaling"] = narrative.getComplexityScaling();
    });
    client->text(response);
}

static void handleNarrativeConfig(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    
    // Check if this is a get or set operation
    bool isSet = doc.containsKey("durations") || doc.containsKey("enabled") || doc.containsKey("curves");
    
    if (!isSet) {
        // Get config
        String response = buildWsResponse("narrative.config", requestId, [&narrative](JsonObject& data) {
            JsonObject durations = data["durations"].to<JsonObject>();
            durations["build"] = narrative.getBuildDuration();
            durations["hold"] = narrative.getHoldDuration();
            durations["release"] = narrative.getReleaseDuration();
            durations["rest"] = narrative.getRestDuration();
            durations["total"] = narrative.getTotalDuration();
            
            JsonObject curves = data["curves"].to<JsonObject>();
            curves["build"] = static_cast<uint8_t>(narrative.getBuildCurve());
            curves["release"] = static_cast<uint8_t>(narrative.getReleaseCurve());
            
            data["holdBreathe"] = narrative.getHoldBreathe();
            data["snapAmount"] = narrative.getSnapAmount();
            data["durationVariance"] = narrative.getDurationVariance();
            data["enabled"] = narrative.isEnabled();
        });
        client->text(response);
    } else {
        // Set config
        bool updated = false;
        
        if (doc.containsKey("durations")) {
            JsonObject durations = doc["durations"];
            if (durations.containsKey("build")) {
                narrative.setBuildDuration(durations["build"] | 1.5f);
                updated = true;
            }
            if (durations.containsKey("hold")) {
                narrative.setHoldDuration(durations["hold"] | 0.5f);
                updated = true;
            }
            if (durations.containsKey("release")) {
                narrative.setReleaseDuration(durations["release"] | 1.5f);
                updated = true;
            }
            if (durations.containsKey("rest")) {
                narrative.setRestDuration(durations["rest"] | 0.5f);
                updated = true;
            }
        }
        
        if (doc.containsKey("curves")) {
            JsonObject curves = doc["curves"];
            if (curves.containsKey("build")) {
                narrative.setBuildCurve(static_cast<lightwaveos::effects::EasingCurve>(curves["build"] | 1));
                updated = true;
            }
            if (curves.containsKey("release")) {
                narrative.setReleaseCurve(static_cast<lightwaveos::effects::EasingCurve>(curves["release"] | 6));
                updated = true;
            }
        }
        
        if (doc.containsKey("holdBreathe")) {
            narrative.setHoldBreathe(doc["holdBreathe"] | 0.0f);
            updated = true;
        }
        if (doc.containsKey("snapAmount")) {
            narrative.setSnapAmount(doc["snapAmount"] | 0.0f);
            updated = true;
        }
        if (doc.containsKey("durationVariance")) {
            narrative.setDurationVariance(doc["durationVariance"] | 0.0f);
            updated = true;
        }
        
        if (doc.containsKey("enabled")) {
            bool enabled = doc["enabled"] | false;
            if (enabled) {
                narrative.enable();
            } else {
                narrative.disable();
            }
            updated = true;
        }
        
        // Persist to NVS if configuration was updated
        if (updated) {
            narrative.saveToNVS();
        }
        
        String response = buildWsResponse("narrative.config", requestId, [updated](JsonObject& data) {
            data["message"] = updated ? "Narrative config updated" : "No changes";
            data["updated"] = updated;
        });
        client->text(response);
    }
}

void registerWsNarrativeCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("narrative.getStatus", handleNarrativeGetStatus);
    WsCommandRouter::registerCommand("narrative.config", handleNarrativeConfig);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

