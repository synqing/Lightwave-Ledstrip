/**
 * @file WsSynqMatrixCommands.cpp
 * @brief WebSocket commands for runtime-only synq-matrix director controls.
 *
 * Canonical command names live under the `synqMatrix.*` namespace. The legacy
 * `songAware.*` names are retained for one release as deprecated aliases — each
 * canonical handler has a paired alias wrapper that delegates to the same impl
 * with the legacy envelope type. Aliases will be removed when the SynqMatrix
 * algorithmic contract ships.
 */

#include "WsSynqMatrixCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/synqmatrix/SynqMatrix.h"
#include "../../../core/synqmatrix/SynqMatrixBootPreference.h"
#include "../../../core/synqmatrix/SynqMatrixRestorePoint.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

namespace {

String buildSynqMatrixWsError(const char* envelopeType,
                              const char* errorCode,
                              const char* message,
                              const char* requestId) {
    JsonDocument response;
    response["type"] = envelopeType;
    if (requestId != nullptr && strlen(requestId) > 0) {
        response["requestId"] = requestId;
    }
    response["success"] = false;
    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = errorCode;
    error["message"] = message;

    String output;
    serializeJson(response, output);
    return output;
}

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
        bool ok = false;
        const char* value = root["mode"].as<const char*>();
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
        bool ok = false;
        config.profile = synqmatrix::parseSynqMatrixProfile(root["profile"].as<const char*>(), &ok);
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

// ── Shared impls — each takes envelope-type as a parameter so canonical and
//    legacy-alias wrappers emit the same payload under different envelope names.

void handleSynqMatrixConfigGetImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    const auto config = synqmatrix::SynqMatrix::instance().getConfig();
    client->text(buildWsResponse(envelopeType, requestId, [&config](JsonObject& data) {
        encodeConfig(data, config);
    }));
}

void handleSynqMatrixConfigSetImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    synqmatrix::SynqMatrixConfig config = synqmatrix::SynqMatrix::instance().getConfig();
    const char* error = nullptr;
    if (!applyConfigJson(doc.as<JsonObjectConst>(), config, &error)) {
        client->text(buildSynqMatrixWsError(envelopeType,
                                            ErrorCodes::INVALID_VALUE,
                                            error ? error : "Invalid SynqMatrix config",
                                            requestId));
        return;
    }

    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket);
    synqmatrix::SynqMatrix::instance().setConfig(config);
    client->text(buildWsResponse(envelopeType, requestId, [&config](JsonObject& data) {
        encodeConfig(data, config);
    }));
}

void handleSynqMatrixStatusImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse(envelopeType, requestId, [&status](JsonObject& data) {
        encodeStatus(data, status);
    }));
}

void handleSynqMatrixResetImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket);
    synqmatrix::SynqMatrix::instance().reset();
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse(envelopeType, requestId, [&status](JsonObject& data) {
        data["reset"] = true;
        JsonObject statusObj = data["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    }));
}

void handleSynqMatrixRestoreImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    if (!synqmatrix::hasSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket)) {
        client->text(buildSynqMatrixWsError(envelopeType,
                                            ErrorCodes::INVALID_ACTION,
                                            "No SynqMatrix restore point captured by WebSocket",
                                            requestId));
        return;
    }
    synqmatrix::restoreSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket);
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse(envelopeType, requestId, [&status](JsonObject& data) {
        data["restored"] = true;
        JsonObject statusObj = data["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    }));
}

void handleSynqMatrixDebugImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    const auto debug = synqmatrix::SynqMatrix::instance().getDebugSnapshot();
    client->text(buildWsResponse(envelopeType, requestId, [&debug](JsonObject& data) {
        encodeDebug(data, debug);
    }));
}

void handleSynqMatrixPolicyImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    const auto debug = synqmatrix::SynqMatrix::instance().getDebugSnapshot();
    client->text(buildWsResponse(envelopeType, requestId, [&debug](JsonObject& data) {
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
    }));
}

void handleSynqMatrixAllowlistImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    client->text(buildWsResponse(envelopeType, requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSynqMatrixAllowlistSetImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    if (!doc["state"].is<const char*>() || !doc["enabled"].is<bool>()) {
        client->text(buildSynqMatrixWsError(envelopeType,
                                            ErrorCodes::INVALID_VALUE,
                                            "state and enabled are required",
                                            requestId));
        return;
    }
    bool ok = false;
    const auto state = synqmatrix::parseSynqMatrixState(doc["state"].as<const char*>(), &ok);
    if (!ok) {
        client->text(buildSynqMatrixWsError(envelopeType,
                                            ErrorCodes::INVALID_VALUE,
                                            "Invalid SynqMatrix state",
                                            requestId));
        return;
    }
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket);
    synqmatrix::SynqMatrix::instance().setPolicyAllowed(state, doc["enabled"].as<bool>());
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    client->text(buildWsResponse(envelopeType, requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSynqMatrixAllowlistResetImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket);
    synqmatrix::SynqMatrix::instance().resetPolicyAllowlist();
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    client->text(buildWsResponse(envelopeType, requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSynqMatrixHealthImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse(envelopeType, requestId, [&status](JsonObject& data) {
        encodeHealth(data, status);
    }));
}

void handleSynqMatrixCountersResetImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket);
    synqmatrix::SynqMatrix::instance().resetCounters();
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse(envelopeType, requestId, [&status](JsonObject& data) {
        data["reset"] = true;
        data["parameterUpdates"] = status.parameterUpdates;
        data["automaticEffectSwitches"] = status.automaticEffectSwitches;
        JsonObject health = data["health"].to<JsonObject>();
        encodeHealth(health, status);
    }));
}

// ── Director boot preference + on-demand engage/release ──────────────────

void handleSynqMatrixBootGetImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    const auto mode = synqmatrix::getSynqMatrixBootMode();
    client->text(buildWsResponse(envelopeType, requestId, [mode](JsonObject& data) {
        data["mode"] = synqmatrix::synqMatrixBootModeName(mode);
    }));
}

void handleSynqMatrixBootSetImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    if (!doc["mode"].is<const char*>()) {
        client->text(buildSynqMatrixWsError(envelopeType,
                                            ErrorCodes::MISSING_FIELD,
                                            "Missing 'mode' field (expected 'on' or 'off')",
                                            requestId));
        return;
    }
    bool ok = false;
    const auto mode = synqmatrix::parseSynqMatrixBootMode(doc["mode"].as<const char*>(), &ok);
    if (!ok) {
        client->text(buildSynqMatrixWsError(envelopeType,
                                            ErrorCodes::INVALID_VALUE,
                                            "Invalid mode (expected 'on' or 'off')",
                                            requestId));
        return;
    }
    if (!synqmatrix::setSynqMatrixBootMode(mode)) {
        client->text(buildSynqMatrixWsError(envelopeType,
                                            ErrorCodes::INTERNAL_ERROR,
                                            "NVS write failed",
                                            requestId));
        return;
    }
    client->text(buildWsResponse(envelopeType, requestId, [mode](JsonObject& data) {
        data["mode"] = synqmatrix::synqMatrixBootModeName(mode);
    }));
}

void handleSynqMatrixEngageImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket);
    synqmatrix::engageSynqMatrixDirector();
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse(envelopeType, requestId, [&status](JsonObject& data) {
        data["engaged"] = true;
        JsonObject statusObj = data["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    }));
}

void handleSynqMatrixReleaseImpl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&, const char* envelopeType) {
    const char* requestId = doc["requestId"] | "";
    synqmatrix::captureSynqMatrixRestorePoint(synqmatrix::SynqMatrixRestoreScope::WebSocket);
    synqmatrix::releaseSynqMatrixDirector();
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse(envelopeType, requestId, [&status](JsonObject& data) {
        data["engaged"] = false;
        JsonObject statusObj = data["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    }));
}

// ── Canonical wrappers (synqMatrix.* envelope) ────────────────────────────

void handleSynqMatrixConfigGetCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixConfigGetImpl(client, doc, ctx, "synqMatrix.config");
}
void handleSynqMatrixConfigSetCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixConfigSetImpl(client, doc, ctx, "synqMatrix.config");
}
void handleSynqMatrixStatusCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixStatusImpl(client, doc, ctx, "synqMatrix.status");
}
void handleSynqMatrixResetCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixResetImpl(client, doc, ctx, "synqMatrix.reset");
}
void handleSynqMatrixRestoreCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixRestoreImpl(client, doc, ctx, "synqMatrix.restore");
}
void handleSynqMatrixDebugCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixDebugImpl(client, doc, ctx, "synqMatrix.debug");
}
void handleSynqMatrixPolicyCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixPolicyImpl(client, doc, ctx, "synqMatrix.policy");
}
void handleSynqMatrixAllowlistCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixAllowlistImpl(client, doc, ctx, "synqMatrix.allowlist");
}
void handleSynqMatrixAllowlistSetCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixAllowlistSetImpl(client, doc, ctx, "synqMatrix.allowlist");
}
void handleSynqMatrixAllowlistResetCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixAllowlistResetImpl(client, doc, ctx, "synqMatrix.allowlist");
}
void handleSynqMatrixHealthCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixHealthImpl(client, doc, ctx, "synqMatrix.health");
}
void handleSynqMatrixCountersResetCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixCountersResetImpl(client, doc, ctx, "synqMatrix.counters.reset");
}
void handleSynqMatrixBootGetCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixBootGetImpl(client, doc, ctx, "synqMatrix.boot");
}
void handleSynqMatrixBootSetCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixBootSetImpl(client, doc, ctx, "synqMatrix.boot");
}
void handleSynqMatrixEngageCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixEngageImpl(client, doc, ctx, "synqMatrix.engage");
}
void handleSynqMatrixReleaseCanonical(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixReleaseImpl(client, doc, ctx, "synqMatrix.release");
}

// ── Legacy alias wrappers (songAware.* envelope, deprecated) ──────────────

void handleSynqMatrixConfigGetLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixConfigGetImpl(client, doc, ctx, "songAware.config");
}
void handleSynqMatrixConfigSetLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixConfigSetImpl(client, doc, ctx, "songAware.config");
}
void handleSynqMatrixStatusLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixStatusImpl(client, doc, ctx, "songAware.status");
}
void handleSynqMatrixResetLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixResetImpl(client, doc, ctx, "songAware.reset");
}
void handleSynqMatrixRestoreLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixRestoreImpl(client, doc, ctx, "songAware.restore");
}
void handleSynqMatrixDebugLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixDebugImpl(client, doc, ctx, "songAware.debug");
}
void handleSynqMatrixPolicyLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixPolicyImpl(client, doc, ctx, "songAware.policy");
}
void handleSynqMatrixAllowlistLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixAllowlistImpl(client, doc, ctx, "songAware.allowlist");
}
void handleSynqMatrixAllowlistSetLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixAllowlistSetImpl(client, doc, ctx, "songAware.allowlist");
}
void handleSynqMatrixAllowlistResetLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixAllowlistResetImpl(client, doc, ctx, "songAware.allowlist");
}
void handleSynqMatrixHealthLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixHealthImpl(client, doc, ctx, "songAware.health");
}
void handleSynqMatrixCountersResetLegacyAlias(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    handleSynqMatrixCountersResetImpl(client, doc, ctx, "songAware.counters.reset");
}

} // namespace

void registerWsSynqMatrixCommands(const WebServerContext& ctx) {
    (void)ctx;
    // Canonical synqMatrix.* commands
    WsCommandRouter::registerCommand("synqMatrix.config.get",      handleSynqMatrixConfigGetCanonical);
    WsCommandRouter::registerCommand("synqMatrix.config.set",      handleSynqMatrixConfigSetCanonical);
    WsCommandRouter::registerCommand("synqMatrix.status",          handleSynqMatrixStatusCanonical);
    WsCommandRouter::registerCommand("synqMatrix.reset",           handleSynqMatrixResetCanonical);
    WsCommandRouter::registerCommand("synqMatrix.restore",         handleSynqMatrixRestoreCanonical);
    WsCommandRouter::registerCommand("synqMatrix.debug",           handleSynqMatrixDebugCanonical);
    WsCommandRouter::registerCommand("synqMatrix.policy",          handleSynqMatrixPolicyCanonical);
    WsCommandRouter::registerCommand("synqMatrix.allowlist",       handleSynqMatrixAllowlistCanonical);
    WsCommandRouter::registerCommand("synqMatrix.allowlist.set",   handleSynqMatrixAllowlistSetCanonical);
    WsCommandRouter::registerCommand("synqMatrix.allowlist.reset", handleSynqMatrixAllowlistResetCanonical);
    WsCommandRouter::registerCommand("synqMatrix.health",          handleSynqMatrixHealthCanonical);
    WsCommandRouter::registerCommand("synqMatrix.counters.reset",  handleSynqMatrixCountersResetCanonical);
    // Director boot preference (NVS-persisted) + on-demand engage/release.
    WsCommandRouter::registerCommand("synqMatrix.boot.get",        handleSynqMatrixBootGetCanonical);
    WsCommandRouter::registerCommand("synqMatrix.boot.set",        handleSynqMatrixBootSetCanonical);
    WsCommandRouter::registerCommand("synqMatrix.engage",          handleSynqMatrixEngageCanonical);
    WsCommandRouter::registerCommand("synqMatrix.release",         handleSynqMatrixReleaseCanonical);

    // Legacy songAware.* aliases — deprecated, removed at SynqMatrix algorithmic-contract release.
    WsCommandRouter::registerCommand("songAware.config.get",      handleSynqMatrixConfigGetLegacyAlias);
    WsCommandRouter::registerCommand("songAware.config.set",      handleSynqMatrixConfigSetLegacyAlias);
    WsCommandRouter::registerCommand("songAware.status",          handleSynqMatrixStatusLegacyAlias);
    WsCommandRouter::registerCommand("songAware.reset",           handleSynqMatrixResetLegacyAlias);
    WsCommandRouter::registerCommand("songAware.restore",         handleSynqMatrixRestoreLegacyAlias);
    WsCommandRouter::registerCommand("songAware.debug",           handleSynqMatrixDebugLegacyAlias);
    WsCommandRouter::registerCommand("songAware.policy",          handleSynqMatrixPolicyLegacyAlias);
    WsCommandRouter::registerCommand("songAware.allowlist",       handleSynqMatrixAllowlistLegacyAlias);
    WsCommandRouter::registerCommand("songAware.allowlist.set",   handleSynqMatrixAllowlistSetLegacyAlias);
    WsCommandRouter::registerCommand("songAware.allowlist.reset", handleSynqMatrixAllowlistResetLegacyAlias);
    WsCommandRouter::registerCommand("songAware.health",          handleSynqMatrixHealthLegacyAlias);
    WsCommandRouter::registerCommand("songAware.counters.reset",  handleSynqMatrixCountersResetLegacyAlias);
    WsCommandRouter::registerCommand("songAware.countersReset",   handleSynqMatrixCountersResetLegacyAlias);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
