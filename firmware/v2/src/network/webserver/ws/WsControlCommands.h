/**
 * @file WsControlCommands.h
 * @brief WebSocket control lease command handlers
 */

#pragma once

#include "../../../config/features.h"

#if FEATURE_WEB_SERVER && FEATURE_CONTROL_LEASE

namespace lightwaveos {
namespace network {
namespace webserver {

struct WebServerContext;

namespace ws {

void registerWsControlCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_CONTROL_LEASE

