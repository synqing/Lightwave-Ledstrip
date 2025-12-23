/**
 * @file WsCommandRouter.cpp
 * @brief WebSocket command router implementation
 */

#include "WsCommandRouter.h"
#include "../WebServer.h"
#include "../ApiResponse.h"
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

// Static storage for command handlers
WsCommandEntry WsCommandRouter::s_handlers[MAX_HANDLERS];
size_t WsCommandRouter::s_handlerCount = 0;

void WsCommandRouter::registerCommand(const char* type, WsCommandHandler handler) {
    if (s_handlerCount >= MAX_HANDLERS) {
        Serial.printf("[WsCommandRouter] ERROR: Handler table full, cannot register '%s'\n", type);
        return;
    }
    
    WsCommandEntry& entry = s_handlers[s_handlerCount++];
    entry.type = type;
    entry.typeLen = strlen(type);
    entry.firstChar = type[0];
    entry.handler = handler;
}

bool WsCommandRouter::route(AsyncWebSocketClient* client, JsonDocument& doc, WebServer* server) {
    if (!doc.containsKey("type")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing 'type' field"));
        return false;
    }
    
    String typeStr = doc["type"].as<String>();
    const char* type = typeStr.c_str();
    uint8_t typeLen = typeStr.length();
    char firstChar = type[0];
    
    // Fast lookup: filter by first char and length, then exact match
    for (size_t i = 0; i < s_handlerCount; i++) {
        const WsCommandEntry& entry = s_handlers[i];
        
        // Quick pre-filter: first char and length must match
        if (entry.firstChar != firstChar || entry.typeLen != typeLen) {
            continue;
        }
        
        // Exact string match
        if (matchesCommand(type, entry)) {
            entry.handler(client, doc, server);
            return true;
        }
    }
    
    // Unknown command
    const char* requestId = doc["requestId"] | "";
    client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown command type", requestId));
    return false;
}

bool WsCommandRouter::matchesCommand(const char* type, const WsCommandEntry& entry) {
    // Already checked length and first char, do exact match
    return strcmp(type, entry.type) == 0;
}

size_t WsCommandRouter::getHandlerCount() {
    return s_handlerCount;
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

