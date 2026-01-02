#include "WsMessageRouter.h"
#include "../parameters/ParameterHandler.h"
#include <Arduino.h>

ParameterHandler* WsMessageRouter::s_paramHandler = nullptr;

void WsMessageRouter::init(ParameterHandler* paramHandler) {
    s_paramHandler = paramHandler;
}

bool WsMessageRouter::route(StaticJsonDocument<512>& doc) {
    // Must have "type" field (LightwaveOS protocol)
    if (!doc.containsKey("type")) {
        return false; // Ignore messages without type
    }

    const char* type = doc["type"];
    if (!type) {
        return false;
    }

    // Route by message type
    if (strcmp(type, "status") == 0) {
        handleStatus(doc);
        return true;
    } else if (strcmp(type, "device.status") == 0) {
        handleDeviceStatus(doc);
        return true; // Handled (even if we ignore it)
    } else if (strcmp(type, "parameters.changed") == 0) {
        handleParametersChanged(doc);
        return true; // Handled (even if we ignore it)
    } else if (strcmp(type, "effects.changed") == 0) {
        handleEffectsChanged(doc);
        return true; // Handled (even if we ignore it)
    }

    // Unknown message type - ignore silently (rate-limited logging could go here)
    return false;
}

void WsMessageRouter::handleStatus(StaticJsonDocument<512>& doc) {
    if (s_paramHandler) {
        s_paramHandler->applyStatus(doc);
    }
}

void WsMessageRouter::handleDeviceStatus(StaticJsonDocument<512>& doc) {
    // Optional: Can log device status or ignore
    // For now, we ignore it since we only care about parameter state
    (void)doc;
}

void WsMessageRouter::handleParametersChanged(StaticJsonDocument<512>& doc) {
    // Optional: Could trigger a getStatus request to refresh state
    // For now, we rely on periodic status broadcasts from server
    (void)doc;
}

void WsMessageRouter::handleEffectsChanged(StaticJsonDocument<512>& doc) {
    // Optional: Could trigger a getStatus request to refresh state
    // For now, we rely on periodic status broadcasts from server
    (void)doc;
}

