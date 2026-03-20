#pragma once

/**
 * @file SerialJsonGateway.h
 * @brief JSON command gateway for PRISM Studio (Web Serial) control.
 *
 * Each line beginning with '{' received over Serial is routed here for
 * parsing and dispatch. Responses are printed as JSON lines back to Serial.
 */

#include <Arduino.h>

// Forward declarations — avoid pulling heavy headers into every includer.
namespace lightwaveos {
namespace actors {
class ActorSystem;
class RendererActor;
}
namespace zones {
class ZoneComposer;
}
}
namespace prism {
class DynamicShowStore;
}

namespace lightwaveos {
namespace serial {

/**
 * @brief Dependencies required by the Serial JSON command handler.
 *
 * All pointers/references must remain valid for the lifetime of the gateway.
 * Constructed once in main.cpp and passed to processCommand() on each call.
 */
struct SerialJsonGatewayDeps {
    lightwaveos::actors::ActorSystem&   actors;
    lightwaveos::actors::RendererActor* renderer;
    lightwaveos::zones::ZoneComposer&   zoneComposer;
    ::prism::DynamicShowStore&          showStore;
};

/**
 * @brief Process a JSON command received over Serial.
 *
 * This is the gateway for PRISM Studio (Web Serial) control.
 * Each line beginning with '{' is parsed as a JSON command.
 * Responses are printed as JSON lines back to Serial.
 *
 * @param json  The raw JSON string (one complete line).
 * @param deps  References to the subsystems the handler needs.
 */
void processSerialJsonCommand(const String& json, const SerialJsonGatewayDeps& deps);

} // namespace serial
} // namespace lightwaveos
