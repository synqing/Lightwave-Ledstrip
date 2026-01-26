/**
 * @file WsCommandRouter.h
 * @brief WebSocket command router with table-driven dispatch
 *
 * Replaces long if-else chain with efficient lookup table for better performance.
 * Uses string length + first char optimization for fast matching.
 */

#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <stdint.h>

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;  // Forward declare to avoid circular dependency

/**
 * @brief WebSocket command handler function type
 */
using WsCommandHandler = void (*)(AsyncWebSocketClient*, JsonDocument&, const WebServerContext&);

/**
 * @brief Command entry in lookup table
 */
struct WsCommandEntry {
    const char* type;           // Command type string
    uint16_t typeLen;           // Precomputed length for fast comparison
    char firstChar;             // First character for quick filtering
    WsCommandHandler handler;   // Handler function
};

/**
 * @brief WebSocket command router
 *
 * Uses static lookup table for O(1) average case dispatch.
 * Optimized string matching using length + first char pre-filtering.
 */
class WsCommandRouter {
public:
    /**
     * @brief Register a command handler
     * @param type Command type string (must be stable)
     * @param handler Handler function
     */
    static void registerCommand(const char* type, WsCommandHandler handler);
    
    /**
     * @brief Route a command to appropriate handler
     * @param client WebSocket client
     * @param doc Parsed JSON document
     * @param ctx WebServer context (for accessing state)
     * @return true if command was handled, false if unknown
     */
    static bool route(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx);
    
    /**
     * @brief Get handler count (for testing/debugging)
     */
    static size_t getHandlerCount();
    
    /**
     * @brief Get maximum handler capacity
     */
    static size_t getMaxHandlers();

private:
    static constexpr size_t MAX_HANDLERS = 192;  // Capacity for all current commands (126 registered) plus ~50% headroom
    static WsCommandEntry s_handlers[MAX_HANDLERS];
    static size_t s_handlerCount;
    
    /**
     * @brief Fast string comparison optimized for command routing
     */
    static bool matchesCommand(const char* type, const WsCommandEntry& entry);
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos
