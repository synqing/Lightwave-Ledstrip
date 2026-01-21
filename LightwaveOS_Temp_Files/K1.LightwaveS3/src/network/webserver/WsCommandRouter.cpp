/**
 * @file WsCommandRouter.cpp
 * @brief WebSocket command router implementation
 */

#include "WsCommandRouter.h"
#include "WebServerContext.h"
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

// Static storage for command handlers
WsCommandEntry WsCommandRouter::s_handlers[MAX_HANDLERS];
size_t WsCommandRouter::s_handlerCount = 0;

void WsCommandRouter::registerCommand(const char* type, WsCommandHandler handler) {
    if (!type || type[0] == '\0' || !handler) {
        Serial.println("[WsCommandRouter] ERROR: Invalid handler registration");
        return;
    }
    if (s_handlerCount >= MAX_HANDLERS) {
        Serial.printf("[WsCommandRouter] ERROR: Handler table full (%zu/%zu), cannot register '%s'\n", 
                      s_handlerCount, MAX_HANDLERS, type);
        return;
    }
    
    WsCommandEntry& entry = s_handlers[s_handlerCount++];
    entry.type = type;
    entry.typeLen = static_cast<uint16_t>(strlen(type));
    entry.firstChar = type[0];
    entry.handler = handler;
}

bool WsCommandRouter::route(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* type = doc["type"].as<const char*>();
    if (!type || type[0] == '\0') {
        return false;
    }

    const uint16_t typeLen = static_cast<uint16_t>(strlen(type));
    const char firstChar = type[0];
    
    // DEFENSIVE CHECK: Validate handler count before array access
    size_t safe_handler_count = s_handlerCount;
    if (safe_handler_count > MAX_HANDLERS) {
        safe_handler_count = MAX_HANDLERS;  // Clamp to safe maximum
    }
    
    // Fast lookup: filter by first char and length, then exact match
    for (size_t i = 0; i < safe_handler_count; i++) {
        // DEFENSIVE CHECK: Validate array index before access
        if (i >= MAX_HANDLERS) {
            break;  // Safety: should never happen, but protects against corruption
        }
        const WsCommandEntry& entry = s_handlers[i];
        
        // Quick pre-filter: first char and length must match
        if (entry.firstChar != firstChar || entry.typeLen != typeLen) {
            continue;
        }
        
        // Exact string match
        if (matchesCommand(type, entry)) {
            if (!entry.handler) {
                return false;
            }
            entry.handler(client, doc, ctx);
            return true;
        }
    }
    
    return false;
}

bool WsCommandRouter::matchesCommand(const char* type, const WsCommandEntry& entry) {
    // Already checked length and first char, do exact match
    return strcmp(type, entry.type) == 0;
}

size_t WsCommandRouter::getHandlerCount() {
    return s_handlerCount;
}

size_t WsCommandRouter::getMaxHandlers() {
    return MAX_HANDLERS;
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos
