/**
 * @file SynqMatrixHandlers.cpp
 * @brief REST handlers for runtime-only synq-matrix director controls.
 */

#include "SynqMatrixHandlers.h"

#include "../../ApiResponse.h"
#include "../../../core/synqmatrix/SynqMatrix.h"
#include "../../../core/synqmatrix/SynqMatrixBootPreference.h"
#include "../../../core/synqmatrix/SynqMatrixRestorePoint.h"

#include <ArduinoJson.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

namespace {

void encodeConfig(JsonObject& data, const synqmatrix::SynqMatrixConfig& config) {
    data["enabled"] = config.enabled;
    data["mode"] = synqmatrix::synqMatrixModeName(config.mode);
    data["profile"] = synqmatrix::synqMatrixProfileName(config.profile);
    data["switchingEnabled"] = config.switchingEnabled;
    data["familyMorphing"] = config.familyMorphing;
    data["constrainedSwitching"] = config.constrainedSwitching;
    data["sensitivity"] = config.sensitivity;
    data["intensityScalar"] = config.intensityScalar;
    data["motionScalar"] = config.motionScalar;
    data["confidenceFloor"] = config.confidenceFloor;
}

void encodePolicy(JsonObject data, const synqmatrix::SynqMatrixPolicySnapshot& policy) {
    data["state"] = synqmatrix::synqMatrixStateName(policy.state);
    data["effectId"] = policy.effectId;
    data["family"] = policy.family;
    data["visualLanguage"] = policy.visualLanguage;
    data["reason"] = synqmatrix::synqMatrixSwitchReasonName(policy.reason);
    data["minConfidence"] = policy.minConfidence;
    data["enabled"] = policy.enabled;
}

void encodeAllowlist(JsonObject data, const synqmatrix::SynqMatrixAllowlistSnapshot& allowlist) {
    data["count"] = allowlist.count;
    JsonArray policies = data["policies"].to<JsonArray>();
    for (uint8_t i = 0; i < allowlist.count; ++i) {
        JsonObject item = policies.add<JsonObject>();
        encodePolicy(item, allowlist.policies[i]);
    }
}

void encodeHealth(JsonObject data, const synqmatrix::SynqMatrixStatus& status) {
    data["healthDegraded"] = status.healthDegraded;
    data["showSkips"] = status.showSkips;
    data["failures"] = status.failures;
    data["rmtErrors"] = status.rmtErrors;
    data["underruns"] = status.underruns;
    data["healthCleanForMs"] = status.healthCleanForMs;
    data["healthCleanWindowRemainingMs"] = status.healthCleanWindowRemainingMs;
}

void encodeStatus(JsonObject& data, const synqmatrix::SynqMatrixStatus& status) {
    data["enabled"] = status.enabled;
    data["mode"] = synqmatrix::synqMatrixModeName(status.effectiveMode);
    data["effectiveMode"] = synqmatrix::synqMatrixModeName(status.effectiveMode);
    data["profile"] = synqmatrix::synqMatrixProfileName(status.profile);
    data["owner"] = synqmatrix::synqMatrixOwnerName(status.owner);
    data["suppressedReason"] = synqmatrix::synqMatrixSuppressedReasonName(status.suppressedReason);
    data["previousSuppressedReason"] = synqmatrix::synqMatrixSuppressedReasonName(status.previousSuppressedReason);
    data["classificationReason"] = synqmatrix::synqMatrixClassificationReasonName(status.classificationReason);
    data["rawState"] = synqmatrix::synqMatrixStateName(status.rawState);
    data["previousState"] = synqmatrix::synqMatrixStateName(status.previousState);
    data["currentState"] = synqmatrix::synqMatrixStateName(status.currentState);
    data["candidateState"] = synqmatrix::synqMatrixStateName(status.candidateState);
    data["intent"] = synqmatrix::synqMatrixIntentName(status.intent);
    data["actionPlan"] = synqmatrix::synqMatrixActionPlanName(status.actionPlan);
    data["boundaryGate"] = synqmatrix::synqMatrixBoundaryGateName(status.boundaryGate);
    data["boundaryReady"] = status.boundaryReady;
    data["waitingForBoundary"] = status.waitingForBoundary;
    data["boundaryConfidence"] = status.boundaryConfidence;
    data["confidence"] = status.confidence;
    data["selectionScore"] = status.selectionScore;
    data["lastAction"] = synqmatrix::synqMatrixLastActionName(status.lastAction);
    data["activeEffectId"] = status.activeEffectId;
    data["previousEffectId"] = status.previousEffectId;
    data["selectedEffectId"] = status.selectedEffectId;
    data["selectedFamily"] = status.selectedFamily;
    data["selectedVisualLanguage"] = status.selectedVisualLanguage;
    data["lastSwitchReason"] = status.lastSwitchReason;
    data["parameterUpdates"] = status.parameterUpdates;
    data["automaticEffectSwitches"] = status.automaticEffectSwitches;
    data["coasting"] = status.coasting;
    data["audioConfidenceBelowFloorMs"] = status.audioConfidenceBelowFloorMs;
    data["missedPredictionCount"] = status.missedPredictionCount;
    data["tempoWinnerChanges"] = status.tempoWinnerChanges;
    data["lastDecisionAtMs"] = status.lastDecisionAtMs;
    data["lastSwitchAtMs"] = status.lastSwitchAtMs;
    data["stateAgeMs"] = status.stateAgeMs;
    data["candidateAgeMs"] = status.candidateAgeMs;
    data["candidateHoldRemainingMs"] = status.candidateHoldRemainingMs;
    data["dwellRemainingMs"] = status.dwellRemainingMs;
    data["cooldownRemainingMs"] = status.cooldownRemainingMs;
    data["bootGraceRemainingMs"] = status.bootGraceRemainingMs;
    data["enableGraceRemainingMs"] = status.enableGraceRemainingMs;
    data["switchWindowRemainingMs"] = status.switchWindowRemainingMs;
    data["switchesInWindow"] = status.switchesInWindow;
    data["maxSwitchesPerWindow"] = status.maxSwitchesPerWindow;
    data["antiThrashRemainingMs"] = status.antiThrashRemainingMs;
    data["lastSwitchFromEffectId"] = status.lastSwitchFromEffectId;
    data["lastSwitchToEffectId"] = status.lastSwitchToEffectId;
    data["transitionActive"] = status.transitionActive;
    data["transitionPreviousEffectId"] = status.transitionPreviousEffectId;
    data["transitionTargetEffectId"] = status.transitionTargetEffectId;
    data["transitionStartedAtMs"] = status.transitionStartedAtMs;
    data["transitionDurationMs"] = status.transitionDurationMs;
    data["transitionRemainingMs"] = status.transitionRemainingMs;
    data["transitionProgress"] = status.transitionProgress;
    data["rms"] = status.rms;
    data["flux"] = status.flux;
    data["bpm"] = status.bpm;
    data["audioConfidence"] = status.audioConfidence;
    JsonObject health = data["health"].to<JsonObject>();
    encodeHealth(health, status);
}

void encodeDebug(JsonObject& data, const synqmatrix::SynqMatrixDebugSnapshot& debug) {
    JsonObject config = data["config"].to<JsonObject>();
    encodeConfig(config, debug.config);
    JsonObject status = data["status"].to<JsonObject>();
    encodeStatus(status, debug.status);
    JsonObject policy = data["policy"].to<JsonObject>();
    policy["bootGraceMs"] = debug.bootGraceMs;
    policy["postEnableGraceMs"] = debug.postEnableGraceMs;
    policy["stableStateHoldMs"] = debug.stableStateHoldMs;
    policy["dropStateHoldMs"] = debug.dropStateHoldMs;
    policy["minimumDwellMs"] = debug.minimumDwellMs;
    policy["switchCooldownMs"] = debug.switchCooldownMs;
    policy["switchWindowMs"] = debug.switchWindowMs;
    policy["maxSwitchesPerWindow"] = debug.maxSwitchesPerWindow;
    policy["antiThrashWindowMs"] = debug.antiThrashWindowMs;
    policy["healthCleanWindowMs"] = debug.healthCleanWindowMs;
    JsonObject allowlist = data["allowlist"].to<JsonObject>();
    encodeAllowlist(allowlist, debug.allowlist);
}

bool applyConfigJson(JsonObjectConst root, synqmatrix::SynqMatrixConfig& config, const char** error) {
    if (root.containsKey("enabled")) {
        if (!root["enabled"].is<bool>()) {
            *error = "enabled must be bool";
            return false;
        }
        config.enabled = root["enabled"].as<bool>();
    }

    if (root.containsKey("mode")) {
        const char* value = root["mode"].as<const char*>();
        bool ok = false;
        bool profileOk = false;
        const auto legacyProfile = synqmatrix::parseSynqMatrixProfile(value, &profileOk);
        config.mode = synqmatrix::parseSynqMatrixMode(value, &ok);
        if (!ok) {
            *error = "mode must be off, assist, or director";
            return false;
        }
        if (profileOk && value &&
            (strcmp(value, "subtle") == 0 || strcmp(value, "balanced") == 0 ||
             strcmp(value, "high") == 0 || strcmp(value, "high_energy") == 0)) {
            config.profile = legacyProfile;
            config.mode = synqmatrix::SynqMatrixMode::Assist;
        }
    }

    if (root.containsKey("profile")) {
        const char* value = root["profile"].as<const char*>();
        bool ok = false;
        config.profile = synqmatrix::parseSynqMatrixProfile(value, &ok);
        if (!ok) {
            *error = "profile must be subtle, balanced, or high";
            return false;
        }
    }

    if (root.containsKey("familyMorphing")) {
        if (!root["familyMorphing"].is<bool>()) {
            *error = "familyMorphing must be bool";
            return false;
        }
        config.familyMorphing = root["familyMorphing"].as<bool>();
    }

    if (root.containsKey("constrainedSwitching")) {
        if (!root["constrainedSwitching"].is<bool>()) {
            *error = "constrainedSwitching must be bool";
            return false;
        }
        config.constrainedSwitching = root["constrainedSwitching"].as<bool>();
        config.switchingEnabled = config.constrainedSwitching;
    }

    if (root.containsKey("switchingEnabled")) {
        if (!root["switchingEnabled"].is<bool>()) {
            *error = "switchingEnabled must be bool";
            return false;
        }
        config.switchingEnabled = root["switchingEnabled"].as<bool>();
        config.constrainedSwitching = config.switchingEnabled;
    }

    const char* floatFields[] = {"sensitivity", "intensityScalar", "motionScalar", "confidenceFloor"};
    for (const char* field : floatFields) {
        if (root.containsKey(field)) {
            if (!root[field].is<float>()) {
                *error = "scalar fields must be numeric";
                return false;
            }
            float value = root[field].as<float>();
            if (value < 0.0f || value > 1.0f) {
                *error = "scalar fields must be in range 0.0-1.0";
                return false;
            }
            if (strcmp(field, "sensitivity") == 0) config.sensitivity = value;
            else if (strcmp(field, "intensityScalar") == 0) config.intensityScalar = value;
            else if (strcmp(field, "motionScalar") == 0) config.motionScalar = value;
            else if (strcmp(field, "confidenceFloor") == 0) config.confidenceFloor = value;
        }
    }

    return true;
}

} // namespace

void SynqMatrixHandlers::handleGetConfig(AsyncWebServerRequest* request) {
    const auto config = synqmatrix::SynqMatrix::instance().getConfig();
    sendSuccessResponse(request, [&config](JsonObject& data) {
        encodeConfig(data, config);
    });
}

void SynqMatrixHandlers::handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }

    const char* action = doc["action"] | "";
    if (strcmp(action, "reset") == 0 || strcmp(action, "wipe") == 0) {
        synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest);
        synqmatrix::SynqMatrix::instance().reset();
        const auto status = synqmatrix::SynqMatrix::instance().getStatus();
        sendSuccessResponse(request, [&status](JsonObject& response) {
            response["reset"] = true;
            JsonObject statusObj = response["status"].to<JsonObject>();
            encodeStatus(statusObj, status);
        });
        return;
    }
    if (strcmp(action, "restore") == 0) {
        if (!synqmatrix::hasSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest)) {
            sendErrorResponse(request, HttpStatus::CONFLICT, ErrorCodes::INVALID_ACTION,
                              "No SynqMatrix restore point captured by REST config");
            return;
        }
        synqmatrix::restoreSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest);
        const auto status = synqmatrix::SynqMatrix::instance().getStatus();
        sendSuccessResponse(request, [&status](JsonObject& response) {
            response["restored"] = true;
            JsonObject statusObj = response["status"].to<JsonObject>();
            encodeStatus(statusObj, status);
        });
        return;
    }
    if (strcmp(action, "countersReset") == 0 || strcmp(action, "counters_reset") == 0) {
        synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest);
        synqmatrix::SynqMatrix::instance().resetCounters();
        const auto status = synqmatrix::SynqMatrix::instance().getStatus();
        sendSuccessResponse(request, [&status](JsonObject& response) {
            response["reset"] = true;
            response["parameterUpdates"] = status.parameterUpdates;
            response["automaticEffectSwitches"] = status.automaticEffectSwitches;
            JsonObject health = response["health"].to<JsonObject>();
            encodeHealth(health, status);
        });
        return;
    }
    if (strlen(action) > 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_ACTION,
                          "action must be reset, wipe, restore, countersReset, or counters_reset");
        return;
    }

    synqmatrix::SynqMatrixConfig config = synqmatrix::SynqMatrix::instance().getConfig();
    const char* error = nullptr;
    if (!applyConfigJson(doc.as<JsonObjectConst>(), config, &error)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_VALUE,
                          error ? error : "Invalid SynqMatrix config");
        return;
    }

    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest);
    synqmatrix::SynqMatrix::instance().setConfig(config);
    sendSuccessResponse(request, [&config](JsonObject& response) {
        encodeConfig(response, config);
    });
}

void SynqMatrixHandlers::handleGetStatus(AsyncWebServerRequest* request) {
    String view = request->hasParam("view") ? request->getParam("view")->value() : "status";
    view.toLowerCase();

    if (view == "debug") {
        const auto debug = synqmatrix::SynqMatrix::instance().getDebugSnapshot();
        sendSuccessResponse(request, [&debug](JsonObject& data) {
            encodeDebug(data, debug);
        });
        return;
    }
    if (view == "policy") {
        const auto debug = synqmatrix::SynqMatrix::instance().getDebugSnapshot();
        sendSuccessResponse(request, [&debug](JsonObject& data) {
            data["bootGraceMs"] = debug.bootGraceMs;
            data["postEnableGraceMs"] = debug.postEnableGraceMs;
            data["stableStateHoldMs"] = debug.stableStateHoldMs;
            data["dropStateHoldMs"] = debug.dropStateHoldMs;
            data["minimumDwellMs"] = debug.minimumDwellMs;
            data["switchCooldownMs"] = debug.switchCooldownMs;
            data["switchWindowMs"] = debug.switchWindowMs;
            data["maxSwitchesPerWindow"] = debug.maxSwitchesPerWindow;
            data["antiThrashWindowMs"] = debug.antiThrashWindowMs;
            data["healthCleanWindowMs"] = debug.healthCleanWindowMs;
            data["allowlistCount"] = debug.allowlist.count;
        });
        return;
    }
    if (view == "allowlist") {
        const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
        sendSuccessResponse(request, [&allowlist](JsonObject& data) {
            encodeAllowlist(data, allowlist);
        });
        return;
    }
    if (view == "health") {
        const auto status = synqmatrix::SynqMatrix::instance().getStatus();
        sendSuccessResponse(request, [&status](JsonObject& data) {
            encodeHealth(data, status);
        });
        return;
    }
    if (view != "status") {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_VALUE,
                          "view must be status, health, debug, policy, or allowlist");
        return;
    }

    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    sendSuccessResponse(request, [&status](JsonObject& data) {
        encodeStatus(data, status);
    });
}

void SynqMatrixHandlers::handleGetAllowlist(AsyncWebServerRequest* request) {
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    sendSuccessResponse(request, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    });
}

void SynqMatrixHandlers::handleSetAllowlist(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }
    if (!doc["state"].is<const char*>() || !doc["enabled"].is<bool>()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_VALUE,
                          "state and enabled are required");
        return;
    }
    bool ok = false;
    const auto state = synqmatrix::parseSynqMatrixState(doc["state"].as<const char*>(), &ok);
    if (!ok) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_VALUE,
                          "Invalid SynqMatrix state");
        return;
    }

    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest);
    synqmatrix::SynqMatrix::instance().setPolicyAllowed(state, doc["enabled"].as<bool>());
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    sendSuccessResponse(request, [&allowlist](JsonObject& response) {
        encodeAllowlist(response, allowlist);
    });
}

void SynqMatrixHandlers::handleResetAllowlist(AsyncWebServerRequest* request) {
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest);
    synqmatrix::SynqMatrix::instance().resetPolicyAllowlist();
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    sendSuccessResponse(request, [&allowlist](JsonObject& response) {
        encodeAllowlist(response, allowlist);
    });
}

void SynqMatrixHandlers::handleGetBoot(AsyncWebServerRequest* request) {
    const auto mode = synqmatrix::getSynqMatrixBootMode();
    sendSuccessResponse(request, [&mode](JsonObject& response) {
        response["mode"] = synqmatrix::synqMatrixBootModeName(mode);
    });
}

void SynqMatrixHandlers::handleSetBoot(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }
    const char* modeStr = doc["mode"] | "";
    if (modeStr[0] == '\0') {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::MISSING_FIELD,
                          "Missing 'mode' field (expected 'on' or 'off')");
        return;
    }
    bool ok = false;
    const auto mode = synqmatrix::parseSynqMatrixBootMode(modeStr, &ok);
    if (!ok) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_VALUE,
                          "Invalid mode (expected 'on' or 'off')");
        return;
    }
    if (!synqmatrix::setSynqMatrixBootMode(mode)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_SERVER_ERROR, ErrorCodes::INTERNAL_ERROR,
                          "NVS write failed");
        return;
    }
    sendSuccessResponse(request, [&mode](JsonObject& response) {
        response["mode"] = synqmatrix::synqMatrixBootModeName(mode);
    });
}

void SynqMatrixHandlers::handleEngage(AsyncWebServerRequest* request) {
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest);
    synqmatrix::engageSynqMatrixDirector();
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    sendSuccessResponse(request, [&status](JsonObject& response) {
        response["engaged"] = true;
        JsonObject statusObj = response["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    });
}

void SynqMatrixHandlers::handleRelease(AsyncWebServerRequest* request) {
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::Rest);
    synqmatrix::releaseSynqMatrixDirector();
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    sendSuccessResponse(request, [&status](JsonObject& response) {
        response["engaged"] = false;
        JsonObject statusObj = response["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
