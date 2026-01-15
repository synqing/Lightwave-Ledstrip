/**
 * @file BatchHandlers.cpp
 * @brief Batch handlers implementation
 */

#include "BatchHandlers.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../core/actors/NodeOrchestrator.h"

using namespace lightwaveos::nodes;
using namespace lightwaveos::network;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void BatchHandlers::handleExecute(AsyncWebServerRequest* request,
                                    uint8_t* data, size_t len,
                                    NodeOrchestrator& orchestrator,
                                    std::function<bool(const String&, JsonVariant)> executeBatchAction,
                                    std::function<void()> broadcastStatus) {
    StaticJsonDocument<512> doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::BatchOperations, request);

    // Schema validates operations is an array with 1-10 items
    JsonArray ops = doc["operations"];

    uint8_t processed = 0;
    uint8_t failed = 0;

    for (JsonVariant op : ops) {
        String action = op["action"] | "";
        if (executeBatchAction(action, op)) {
            processed++;
        } else {
            failed++;
        }
    }

    sendSuccessResponse(request, [processed, failed](JsonObject& data) {
        data["processed"] = processed;
        data["failed"] = failed;
    });

    broadcastStatus();
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

