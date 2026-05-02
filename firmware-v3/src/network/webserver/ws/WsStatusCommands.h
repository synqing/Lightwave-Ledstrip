/**
 * @file WsStatusCommands.h
 * @brief WebSocket status subscription command handlers
 *
 * Gates the periodic 5 s status broadcast (and parameter-change re-broadcasts)
 * behind an explicit subscription, mirroring the per-stream pattern used for
 * LED, audio, and beat events. Without an active status.subscribe, K1 will
 * not push periodic status JSON to the client — saving heap pressure on the
 * AsyncWebSocket per-client message queue.
 *
 * Mirrors WsStreamCommands beat.subscribe (no UDP, no codec) and the
 * WebServer-owned subscription table pattern used by LED/log streams.
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register status subscription WebSocket commands
 * @param ctx WebServer context (carries the WebServer pointer that owns the
 *            status subscriber table)
 *
 * Registers:
 *   - status.subscribe   (client -> K1, ack: status.subscribed)
 *   - status.unsubscribe (client -> K1, ack: status.unsubscribed)
 */
void registerWsStatusCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
