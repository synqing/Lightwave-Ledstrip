/**
 * @file WsSynqMatrixCommands.cpp
 * @brief WebSocket commands for runtime-only synq-matrix director controls.
 */

#include "WsSynqMatrixCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/synqmatrix/SynqMatrix.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

namespace {

synqmatrix::SynqMatrixRuntimeState g_restorePoint;
bool g_restorePointValid = false;

void captureRestorePoint() {
    g_restorePoint = synqmatrix::SynqMatrix::instance().exportRuntimeState();
    g_restorePointValid = true;
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
    data["rawSongState"] = synqmatrix::synqMatrixStateName(status.rawState);
    data["previousSongState"] = synqmatrix::synqMatrixStateName(status.previousState);
    data["currentSongState"] = synqmatrix::synqMatrixStateName(status.currentState);
    data["candidateSongState"] = synqmatrix::synqMatrixStateName(status.candidateState);
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

void handleSynqMatrixConfigGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto config = synqmatrix::SynqMatrix::instance().getConfig();
    client->text(buildWsResponse("songAware.config", requestId, [&config](JsonObject& data) {
        encodeConfig(data, config);
    }));
}

void handleSynqMatrixConfigSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    synqmatrix::SynqMatrixConfig config = synqmatrix::SynqMatrix::instance().getConfig();
    const char* error = nullptr;
    if (!applyConfigJson(doc.as<JsonObjectConst>(), config, &error)) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                                  error ? error : "Invalid songAware config",
                                  requestId));
        return;
    }

    captureRestorePoint();
    synqmatrix::SynqMatrix::instance().setConfig(config);
    client->text(buildWsResponse("songAware.config", requestId, [&config](JsonObject& data) {
        encodeConfig(data, config);
    }));
}

void handleSynqMatrixStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse("songAware.status", requestId, [&status](JsonObject& data) {
        encodeStatus(data, status);
    }));
}

void handleSynqMatrixReset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    captureRestorePoint();
    synqmatrix::SynqMatrix::instance().reset();
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse("songAware.reset", requestId, [&status](JsonObject& data) {
        data["reset"] = true;
        JsonObject statusObj = data["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    }));
}

void handleSynqMatrixRestore(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    if (!g_restorePointValid) {
        client->text(buildWsError(ErrorCodes::INVALID_ACTION,
                                  "No songAware restore point captured by WebSocket",
                                  requestId));
        return;
    }
    synqmatrix::SynqMatrix::instance().restoreRuntimeState(g_restorePoint);
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse("songAware.restore", requestId, [&status](JsonObject& data) {
        data["restored"] = true;
        JsonObject statusObj = data["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    }));
}

void handleSynqMatrixDebug(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto debug = synqmatrix::SynqMatrix::instance().getDebugSnapshot();
    client->text(buildWsResponse("songAware.debug", requestId, [&debug](JsonObject& data) {
        encodeDebug(data, debug);
    }));
}

void handleSynqMatrixPolicy(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto debug = synqmatrix::SynqMatrix::instance().getDebugSnapshot();
    client->text(buildWsResponse("songAware.policy", requestId, [&debug](JsonObject& data) {
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

void handleSynqMatrixAllowlist(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    client->text(buildWsResponse("songAware.allowlist", requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSynqMatrixAllowlistSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    if (!doc["state"].is<const char*>() || !doc["enabled"].is<bool>()) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                                  "state and enabled are required",
                                  requestId));
        return;
    }
    bool ok = false;
    const auto state = synqmatrix::parseSynqMatrixState(doc["state"].as<const char*>(), &ok);
    if (!ok) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                                  "Invalid songAware state",
                                  requestId));
        return;
    }
    captureRestorePoint();
    synqmatrix::SynqMatrix::instance().setPolicyAllowed(state, doc["enabled"].as<bool>());
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    client->text(buildWsResponse("songAware.allowlist", requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSynqMatrixAllowlistReset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    captureRestorePoint();
    synqmatrix::SynqMatrix::instance().resetPolicyAllowlist();
    const auto allowlist = synqmatrix::SynqMatrix::instance().getAllowlistSnapshot();
    client->text(buildWsResponse("songAware.allowlist", requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSynqMatrixHealth(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse("songAware.health", requestId, [&status](JsonObject& data) {
        encodeHealth(data, status);
    }));
}

void handleSynqMatrixCountersReset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    captureRestorePoint();
    synqmatrix::SynqMatrix::instance().resetCounters();
    const auto status = synqmatrix::SynqMatrix::instance().getStatus();
    client->text(buildWsResponse("songAware.counters.reset", requestId, [&status](JsonObject& data) {
        data["reset"] = true;
        data["parameterUpdates"] = status.parameterUpdates;
        data["automaticEffectSwitches"] = status.automaticEffectSwitches;
        JsonObject health = data["health"].to<JsonObject>();
        encodeHealth(health, status);
    }));
}

} // namespace

void registerWsSynqMatrixCommands(const WebServerContext& ctx) {
    (void)ctx;
    WsCommandRouter::registerCommand("songAware.config.get", handleSynqMatrixConfigGet);
    WsCommandRouter::registerCommand("songAware.config.set", handleSynqMatrixConfigSet);
    WsCommandRouter::registerCommand("songAware.status", handleSynqMatrixStatus);
    WsCommandRouter::registerCommand("songAware.reset", handleSynqMatrixReset);
    WsCommandRouter::registerCommand("songAware.restore", handleSynqMatrixRestore);
    WsCommandRouter::registerCommand("songAware.debug", handleSynqMatrixDebug);
    WsCommandRouter::registerCommand("songAware.policy", handleSynqMatrixPolicy);
    WsCommandRouter::registerCommand("songAware.allowlist", handleSynqMatrixAllowlist);
    WsCommandRouter::registerCommand("songAware.allowlist.set", handleSynqMatrixAllowlistSet);
    WsCommandRouter::registerCommand("songAware.allowlist.reset", handleSynqMatrixAllowlistReset);
    WsCommandRouter::registerCommand("songAware.health", handleSynqMatrixHealth);
    WsCommandRouter::registerCommand("songAware.counters.reset", handleSynqMatrixCountersReset);
    WsCommandRouter::registerCommand("songAware.countersReset", handleSynqMatrixCountersReset);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
