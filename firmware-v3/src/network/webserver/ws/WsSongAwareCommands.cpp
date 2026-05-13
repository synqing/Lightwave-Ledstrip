/**
 * @file WsSongAwareCommands.cpp
 * @brief WebSocket commands for runtime-only song-aware director controls.
 */

#include "WsSongAwareCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/songaware/SongAwareDirector.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

namespace {

songaware::SongAwareRuntimeState g_restorePoint;
bool g_restorePointValid = false;

void captureRestorePoint() {
    g_restorePoint = songaware::SongAwareDirector::instance().exportRuntimeState();
    g_restorePointValid = true;
}

void encodeConfig(JsonObject& data, const songaware::SongAwareConfig& config) {
    data["enabled"] = config.enabled;
    data["mode"] = songaware::songAwareModeName(config.mode);
    data["profile"] = songaware::songAwareProfileName(config.profile);
    data["switchingEnabled"] = config.switchingEnabled;
    data["familyMorphing"] = config.familyMorphing;
    data["constrainedSwitching"] = config.constrainedSwitching;
    data["sensitivity"] = config.sensitivity;
    data["intensityScalar"] = config.intensityScalar;
    data["motionScalar"] = config.motionScalar;
    data["confidenceFloor"] = config.confidenceFloor;
}

void encodePolicy(JsonObject data, const songaware::SongAwarePolicySnapshot& policy) {
    data["state"] = songaware::songAwareStateName(policy.state);
    data["effectId"] = policy.effectId;
    data["family"] = policy.family;
    data["visualLanguage"] = policy.visualLanguage;
    data["reason"] = songaware::songAwareSwitchReasonName(policy.reason);
    data["minConfidence"] = policy.minConfidence;
    data["enabled"] = policy.enabled;
}

void encodeAllowlist(JsonObject data, const songaware::SongAwareAllowlistSnapshot& allowlist) {
    data["count"] = allowlist.count;
    JsonArray policies = data["policies"].to<JsonArray>();
    for (uint8_t i = 0; i < allowlist.count; ++i) {
        JsonObject item = policies.add<JsonObject>();
        encodePolicy(item, allowlist.policies[i]);
    }
}

void encodeHealth(JsonObject data, const songaware::SongAwareStatus& status) {
    data["healthDegraded"] = status.healthDegraded;
    data["showSkips"] = status.showSkips;
    data["failures"] = status.failures;
    data["rmtErrors"] = status.rmtErrors;
    data["underruns"] = status.underruns;
    data["healthCleanForMs"] = status.healthCleanForMs;
    data["healthCleanWindowRemainingMs"] = status.healthCleanWindowRemainingMs;
}

void encodeStatus(JsonObject& data, const songaware::SongAwareStatus& status) {
    data["enabled"] = status.enabled;
    data["mode"] = songaware::songAwareModeName(status.effectiveMode);
    data["effectiveMode"] = songaware::songAwareModeName(status.effectiveMode);
    data["profile"] = songaware::songAwareProfileName(status.profile);
    data["owner"] = songaware::songAwareOwnerName(status.owner);
    data["suppressedReason"] = songaware::songAwareSuppressedReasonName(status.suppressedReason);
    data["previousSuppressedReason"] = songaware::songAwareSuppressedReasonName(status.previousSuppressedReason);
    data["classificationReason"] = songaware::songAwareClassificationReasonName(status.classificationReason);
    data["rawSongState"] = songaware::songAwareStateName(status.rawSongState);
    data["previousSongState"] = songaware::songAwareStateName(status.previousSongState);
    data["currentSongState"] = songaware::songAwareStateName(status.currentSongState);
    data["candidateSongState"] = songaware::songAwareStateName(status.candidateSongState);
    data["intent"] = songaware::songAwareIntentName(status.intent);
    data["actionPlan"] = songaware::songAwareActionPlanName(status.actionPlan);
    data["boundaryGate"] = songaware::songAwareBoundaryGateName(status.boundaryGate);
    data["boundaryReady"] = status.boundaryReady;
    data["waitingForBoundary"] = status.waitingForBoundary;
    data["boundaryConfidence"] = status.boundaryConfidence;
    data["confidence"] = status.confidence;
    data["selectionScore"] = status.selectionScore;
    data["lastAction"] = songaware::songAwareLastActionName(status.lastAction);
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

void encodeDebug(JsonObject& data, const songaware::SongAwareDebugSnapshot& debug) {
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

bool applyConfigJson(JsonObjectConst root, songaware::SongAwareConfig& config, const char** error) {
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
        const auto legacyProfile = songaware::parseSongAwareProfile(value, &profileOk);
        config.mode = songaware::parseSongAwareMode(value, &ok);
        if (!ok) {
            *error = "mode must be off, assist, or director";
            return false;
        }
        if (profileOk && value &&
            (strcmp(value, "subtle") == 0 || strcmp(value, "balanced") == 0 ||
             strcmp(value, "high") == 0 || strcmp(value, "high_energy") == 0)) {
            config.profile = legacyProfile;
            config.mode = songaware::SongAwareMode::Assist;
        }
    }
    if (root.containsKey("profile")) {
        bool ok = false;
        config.profile = songaware::parseSongAwareProfile(root["profile"].as<const char*>(), &ok);
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

void handleSongAwareConfigGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto config = songaware::SongAwareDirector::instance().getConfig();
    client->text(buildWsResponse("songAware.config", requestId, [&config](JsonObject& data) {
        encodeConfig(data, config);
    }));
}

void handleSongAwareConfigSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    songaware::SongAwareConfig config = songaware::SongAwareDirector::instance().getConfig();
    const char* error = nullptr;
    if (!applyConfigJson(doc.as<JsonObjectConst>(), config, &error)) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                                  error ? error : "Invalid songAware config",
                                  requestId));
        return;
    }

    captureRestorePoint();
    songaware::SongAwareDirector::instance().setConfig(config);
    client->text(buildWsResponse("songAware.config", requestId, [&config](JsonObject& data) {
        encodeConfig(data, config);
    }));
}

void handleSongAwareStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto status = songaware::SongAwareDirector::instance().getStatus();
    client->text(buildWsResponse("songAware.status", requestId, [&status](JsonObject& data) {
        encodeStatus(data, status);
    }));
}

void handleSongAwareReset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    captureRestorePoint();
    songaware::SongAwareDirector::instance().reset();
    const auto status = songaware::SongAwareDirector::instance().getStatus();
    client->text(buildWsResponse("songAware.reset", requestId, [&status](JsonObject& data) {
        data["reset"] = true;
        JsonObject statusObj = data["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    }));
}

void handleSongAwareRestore(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    if (!g_restorePointValid) {
        client->text(buildWsError(ErrorCodes::INVALID_ACTION,
                                  "No songAware restore point captured by WebSocket",
                                  requestId));
        return;
    }
    songaware::SongAwareDirector::instance().restoreRuntimeState(g_restorePoint);
    const auto status = songaware::SongAwareDirector::instance().getStatus();
    client->text(buildWsResponse("songAware.restore", requestId, [&status](JsonObject& data) {
        data["restored"] = true;
        JsonObject statusObj = data["status"].to<JsonObject>();
        encodeStatus(statusObj, status);
    }));
}

void handleSongAwareDebug(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto debug = songaware::SongAwareDirector::instance().getDebugSnapshot();
    client->text(buildWsResponse("songAware.debug", requestId, [&debug](JsonObject& data) {
        encodeDebug(data, debug);
    }));
}

void handleSongAwarePolicy(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto debug = songaware::SongAwareDirector::instance().getDebugSnapshot();
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

void handleSongAwareAllowlist(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto allowlist = songaware::SongAwareDirector::instance().getAllowlistSnapshot();
    client->text(buildWsResponse("songAware.allowlist", requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSongAwareAllowlistSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    if (!doc["state"].is<const char*>() || !doc["enabled"].is<bool>()) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                                  "state and enabled are required",
                                  requestId));
        return;
    }
    bool ok = false;
    const auto state = songaware::parseSongAwareState(doc["state"].as<const char*>(), &ok);
    if (!ok) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                                  "Invalid songAware state",
                                  requestId));
        return;
    }
    captureRestorePoint();
    songaware::SongAwareDirector::instance().setPolicyAllowed(state, doc["enabled"].as<bool>());
    const auto allowlist = songaware::SongAwareDirector::instance().getAllowlistSnapshot();
    client->text(buildWsResponse("songAware.allowlist", requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSongAwareAllowlistReset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    captureRestorePoint();
    songaware::SongAwareDirector::instance().resetPolicyAllowlist();
    const auto allowlist = songaware::SongAwareDirector::instance().getAllowlistSnapshot();
    client->text(buildWsResponse("songAware.allowlist", requestId, [&allowlist](JsonObject& data) {
        encodeAllowlist(data, allowlist);
    }));
}

void handleSongAwareHealth(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    const auto status = songaware::SongAwareDirector::instance().getStatus();
    client->text(buildWsResponse("songAware.health", requestId, [&status](JsonObject& data) {
        encodeHealth(data, status);
    }));
}

void handleSongAwareCountersReset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext&) {
    const char* requestId = doc["requestId"] | "";
    captureRestorePoint();
    songaware::SongAwareDirector::instance().resetCounters();
    const auto status = songaware::SongAwareDirector::instance().getStatus();
    client->text(buildWsResponse("songAware.counters.reset", requestId, [&status](JsonObject& data) {
        data["reset"] = true;
        data["parameterUpdates"] = status.parameterUpdates;
        data["automaticEffectSwitches"] = status.automaticEffectSwitches;
        JsonObject health = data["health"].to<JsonObject>();
        encodeHealth(health, status);
    }));
}

} // namespace

void registerWsSongAwareCommands(const WebServerContext& ctx) {
    (void)ctx;
    WsCommandRouter::registerCommand("songAware.config.get", handleSongAwareConfigGet);
    WsCommandRouter::registerCommand("songAware.config.set", handleSongAwareConfigSet);
    WsCommandRouter::registerCommand("songAware.status", handleSongAwareStatus);
    WsCommandRouter::registerCommand("songAware.reset", handleSongAwareReset);
    WsCommandRouter::registerCommand("songAware.restore", handleSongAwareRestore);
    WsCommandRouter::registerCommand("songAware.debug", handleSongAwareDebug);
    WsCommandRouter::registerCommand("songAware.policy", handleSongAwarePolicy);
    WsCommandRouter::registerCommand("songAware.allowlist", handleSongAwareAllowlist);
    WsCommandRouter::registerCommand("songAware.allowlist.set", handleSongAwareAllowlistSet);
    WsCommandRouter::registerCommand("songAware.allowlist.reset", handleSongAwareAllowlistReset);
    WsCommandRouter::registerCommand("songAware.health", handleSongAwareHealth);
    WsCommandRouter::registerCommand("songAware.counters.reset", handleSongAwareCountersReset);
    WsCommandRouter::registerCommand("songAware.countersReset", handleSongAwareCountersReset);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
