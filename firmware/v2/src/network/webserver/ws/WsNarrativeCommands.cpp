/**
 * @file WsNarrativeCommands.cpp
 * @brief WebSocket narrative command handlers implementation
 */

#include "WsNarrativeCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/narrative/NarrativeEngine.h"
#include "../../../codec/WsNarrativeCodec.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::narrative;

static void handleNarrativeGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::NarrativeSimpleDecodeResult decodeResult = codec::WsNarrativeCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    
    String response = buildWsResponse("narrative.status", requestId, [&narrative](JsonObject& data) {
        NarrativePhase phase = narrative.getPhase();
        const char* phaseName = "UNKNOWN";
        switch (phase) {
            case PHASE_BUILD:   phaseName = "BUILD"; break;
            case PHASE_HOLD:    phaseName = "HOLD"; break;
            case PHASE_RELEASE: phaseName = "RELEASE"; break;
            case PHASE_REST:    phaseName = "REST"; break;
        }
        
        codec::WsNarrativeCodec::encodeStatus(
            narrative.isEnabled(),
            narrative.getTension() * 100.0f,
            narrative.getPhaseT(),
            narrative.getCycleT(),
            phaseName,
            static_cast<uint8_t>(phase),
            narrative.getBuildDuration(),
            narrative.getHoldDuration(),
            narrative.getReleaseDuration(),
            narrative.getRestDuration(),
            narrative.getTotalDuration(),
            narrative.getTempoMultiplier(),
            narrative.getComplexityScaling(),
            data
        );
    });
    client->text(response);
}

static void handleNarrativeConfig(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::NarrativeConfigDecodeResult decodeResult = codec::WsNarrativeCodec::decodeConfig(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    
    if (!decodeResult.request.isSet) {
        // Get config
        String response = buildWsResponse("narrative.config", requestId, [&narrative](JsonObject& data) {
            codec::WsNarrativeCodec::encodeConfigGet(
                narrative.getBuildDuration(),
                narrative.getHoldDuration(),
                narrative.getReleaseDuration(),
                narrative.getRestDuration(),
                narrative.getTotalDuration(),
                static_cast<uint8_t>(narrative.getBuildCurve()),
                static_cast<uint8_t>(narrative.getReleaseCurve()),
                narrative.getHoldBreathe(),
                narrative.getSnapAmount(),
                narrative.getDurationVariance(),
                narrative.isEnabled(),
                data
            );
        });
        client->text(response);
    } else {
        // Set config
        bool updated = false;
        const codec::NarrativeConfigRequest& req = decodeResult.request;
        
        if (req.hasDurations) {
            narrative.setBuildDuration(req.buildDuration);
            narrative.setHoldDuration(req.holdDuration);
            narrative.setReleaseDuration(req.releaseDuration);
            narrative.setRestDuration(req.restDuration);
            updated = true;
        }
        
        if (req.hasCurves) {
            narrative.setBuildCurve(static_cast<lightwaveos::effects::EasingCurve>(req.buildCurveId));
            narrative.setReleaseCurve(static_cast<lightwaveos::effects::EasingCurve>(req.releaseCurveId));
            updated = true;
        }
        
        if (req.hasHoldBreathe) {
            narrative.setHoldBreathe(req.holdBreathe);
            updated = true;
        }
        
        if (req.hasSnapAmount) {
            narrative.setSnapAmount(req.snapAmount);
            updated = true;
        }
        
        if (req.hasDurationVariance) {
            narrative.setDurationVariance(req.durationVariance);
            updated = true;
        }
        
        if (req.hasEnabled) {
            if (req.enabled) {
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
            codec::WsNarrativeCodec::encodeConfigSetResult(updated, data);
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

