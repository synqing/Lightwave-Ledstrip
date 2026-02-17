/**
 * @file WsShowCommands.h
 * @brief WebSocket show and Prim8 command handlers for PRISM Studio
 *
 * Registers the show.* and prim8.* command groups with the
 * WsCommandRouter. Follows the same pattern as WsTrinityCommands.
 *
 * Commands:
 *   show.upload   - Upload ShowBundle JSON
 *   show.play     - Start show playback
 *   show.pause    - Pause playback
 *   show.resume   - Resume playback
 *   show.stop     - Stop playback
 *   show.seek     - Seek to time position
 *   show.status   - Get current playback state
 *   show.list     - List all available shows
 *   show.delete   - Delete an uploaded show
 *   show.cue.inject - Inject a single cue in real-time
 *   prim8.set     - Apply Prim8 semantic vector
 */

#pragma once

namespace lightwaveos {
namespace network {
namespace webserver {

// Forward declarations
struct WebServerContext;

namespace ws {

/**
 * @brief Register show.* and prim8.* WebSocket commands
 * @param ctx WebServer context
 */
void registerWsShowCommands(const WebServerContext& ctx);

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
