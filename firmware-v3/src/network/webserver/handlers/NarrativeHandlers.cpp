// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file NarrativeHandlers.cpp
 * @brief Narrative handlers implementation
 */

#include "NarrativeHandlers.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../core/narrative/NarrativeEngine.h"
#include "../../../effects/enhancement/MotionEngine.h"

using namespace lightwaveos::network;
using namespace lightwaveos::narrative;
using namespace lightwaveos::effects;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void NarrativeHandlers::handleStatus(AsyncWebServerRequest* request) {
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    
    sendSuccessResponse(request, [&narrative](JsonObject& data) {
        // Current state
        data["enabled"] = narrative.isEnabled();
        data["tension"] = narrative.getTension() * 100.0f;  // Convert 0-1 to 0-100
        data["phaseT"] = narrative.getPhaseT();
        data["cycleT"] = narrative.getCycleT();
        
        // Phase as string
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
        
        // Phase durations
        JsonObject durations = data["durations"].to<JsonObject>();
        durations["build"] = narrative.getBuildDuration();
        durations["hold"] = narrative.getHoldDuration();
        durations["release"] = narrative.getReleaseDuration();
        durations["rest"] = narrative.getRestDuration();
        durations["total"] = narrative.getTotalDuration();
        
        // Derived values
        data["tempoMultiplier"] = narrative.getTempoMultiplier();
        data["complexityScaling"] = narrative.getComplexityScaling();
    });
}

void NarrativeHandlers::handleConfigGet(AsyncWebServerRequest* request) {
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    
    sendSuccessResponse(request, [&narrative](JsonObject& data) {
        // Phase durations
        JsonObject durations = data["durations"].to<JsonObject>();
        durations["build"] = narrative.getBuildDuration();
        durations["hold"] = narrative.getHoldDuration();
        durations["release"] = narrative.getReleaseDuration();
        durations["rest"] = narrative.getRestDuration();
        durations["total"] = narrative.getTotalDuration();
        
        // Curves
        JsonObject curves = data["curves"].to<JsonObject>();
        curves["build"] = static_cast<uint8_t>(narrative.getBuildCurve());
        curves["release"] = static_cast<uint8_t>(narrative.getReleaseCurve());
        
        // Optional behaviors
        data["holdBreathe"] = narrative.getHoldBreathe();
        data["snapAmount"] = narrative.getSnapAmount();
        data["durationVariance"] = narrative.getDurationVariance();
        
        data["enabled"] = narrative.isEnabled();
    });
}

void NarrativeHandlers::handleConfigSet(AsyncWebServerRequest* request,
                                  uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }
    
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    bool updated = false;
    
    // Update phase durations if provided
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
    
    // Update curves if provided
    if (doc.containsKey("curves")) {
        JsonObject curves = doc["curves"];
        if (curves.containsKey("build")) {
            narrative.setBuildCurve(static_cast<EasingCurve>(curves["build"] | 1));
            updated = true;
        }
        if (curves.containsKey("release")) {
            narrative.setReleaseCurve(static_cast<EasingCurve>(curves["release"] | 6));
            updated = true;
        }
    }
    
    // Update optional behaviors
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
    
    // Update enabled state
    if (doc.containsKey("enabled")) {
        bool enabled = doc["enabled"] | false;
        if (enabled) {
            narrative.enable();
        } else {
            narrative.disable();
        }
        updated = true;
    }
    
    // TODO: Persist to NVS if we need this to survive reboot
    
    sendSuccessResponse(request, [updated](JsonObject& respData) {
        respData["message"] = updated ? "Narrative config updated" : "No changes";
        respData["updated"] = updated;
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
