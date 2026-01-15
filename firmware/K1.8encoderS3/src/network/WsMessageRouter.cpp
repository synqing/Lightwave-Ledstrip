#include "WsMessageRouter.h"
#include "../parameters/ParameterHandler.h"
#include <Arduino.h>

ParameterHandler* WsMessageRouter::s_paramHandler = nullptr;

void WsMessageRouter::init(ParameterHandler* paramHandler) {
    s_paramHandler = paramHandler;
}

bool WsMessageRouter::route(StaticJsonDocument<512>& doc) {
    // 1. Dual Key Support (FR-1)
    const char* type = nullptr;
    if (doc.containsKey("type")) {
        type = doc["type"];
    } else if (doc.containsKey("t")) {
        type = doc["t"];
    }

    if (!type) {
        // FR-4: Log error on missing type
        Serial.println("[WS] Error: Missing 'type' or 't' key");
        return false;
    }

    // 2. Command Aliasing (FR-2)
    // Legacy Hub uses "effects.setCurrent" -> Map to "effects.changed"
    if (strcmp(type, "effects.setCurrent") == 0) {
        type = "effects.changed";
    }
    // Legacy Hub uses "parameters.set" -> Map to "parameters.changed"
    else if (strcmp(type, "parameters.set") == 0) {
        type = "parameters.changed";
    }
    // Legacy Hub uses "state.snapshot" -> Map to "status"
    else if (strcmp(type, "state.snapshot") == 0) {
        type = "status";
    }

    // 3. Routing
    if (strcmp(type, "status") == 0) {
        handleStatus(doc);
        return true;
    } else if (strcmp(type, "device.status") == 0) {
        handleDeviceStatus(doc);
        return true; 
    } else if (strcmp(type, "parameters.changed") == 0) {
        handleParametersChanged(doc);
        return true; 
    } else if (strcmp(type, "effects.changed") == 0) {
        handleEffectsChanged(doc);
        return true; 
    }

    // 4. Rate-Limited Logging (NFR-1)
    static unsigned long lastLogTime = 0;
    if (millis() - lastLogTime > 2000) {
        Serial.printf("[WS] Ignored unknown type: %s\n", type);
        lastLogTime = millis();
    }
    
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

