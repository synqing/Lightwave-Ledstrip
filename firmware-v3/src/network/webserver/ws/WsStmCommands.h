/**
 * @file WsStmCommands.h
 * @brief WebSocket STM stream subscription command handlers
 */

#pragma once

#include "../../../config/features.h"

#if FEATURE_AUDIO_SYNC

namespace lightwaveos {
namespace network {
namespace webserver {

struct WebServerContext;

namespace ws {

/**
 * @brief Register STM stream subscription WebSocket commands
 * @param ctx WebServer context
 */
void registerWsStmCommands(const webserver::WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
