/**
 * @file WsGateway.cpp
 * @brief WebSocket gateway implementation
 */

#include "WsGateway.h"
#include "WsCommandRouter.h"
#include "../WebServer.h"
#include "../ApiResponse.h"
#include "../../codec/WsCommonCodec.h"
#include "../../utils/Log.h"
#include "../../config/network_config.h"
#include "ws/WsOtaCommands.h"
#include <cstring>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#undef LW_LOG_TAG
#define LW_LOG_TAG "WsGateway"

namespace lightwaveos {
namespace network {
namespace webserver {

// ============================================================================
// Helper: Escape JSON string for embedding in telemetry payloadSummary
// ============================================================================
static void escapeJsonString(const char* input, char* output, size_t outputSize) {
    if (!input || !output || outputSize == 0) {
        if (output && outputSize > 0) {
            output[0] = '\0';
        }
        return;
    }
    
    size_t outIdx = 0;
    for (size_t i = 0; input[i] != '\0' && outIdx < outputSize - 1; i++) {
        char c = input[i];
        
        // Escape special characters per JSON spec
        if (c == '"') {
            if (outIdx < outputSize - 2) {
                output[outIdx++] = '\\';
                output[outIdx++] = '"';
            } else {
                break;
            }
        } else if (c == '\\') {
            if (outIdx < outputSize - 2) {
                output[outIdx++] = '\\';
                output[outIdx++] = '\\';
            } else {
                break;
            }
        } else if (c == '\n') {
            if (outIdx < outputSize - 2) {
                output[outIdx++] = '\\';
                output[outIdx++] = 'n';
            } else {
                break;
            }
        } else if (c == '\r') {
            if (outIdx < outputSize - 2) {
                output[outIdx++] = '\\';
                output[outIdx++] = 'r';
            } else {
                break;
            }
        } else if (c == '\t') {
            if (outIdx < outputSize - 2) {
                output[outIdx++] = '\\';
                output[outIdx++] = 't';
            } else {
                break;
            }
        } else if (c >= 32 && c < 127) {
            // Printable ASCII
            output[outIdx++] = c;
        } else {
            // Non-printable or high-byte: skip (can't easily encode in JSON string)
            continue;
        }
    }
    
    output[outIdx] = '\0';
}

// Static instance for event handler
WsGateway* WsGateway::s_instance = nullptr;

// Monotonic event sequence counter (wraps at UINT32_MAX)
uint32_t WsGateway::s_eventSeq = 0;

WsGateway::WsGateway(
    AsyncWebSocket* ws,
    const WebServerContext& ctx,
    std::function<bool(AsyncWebSocketClient*)> checkRateLimit,
    std::function<bool(AsyncWebSocketClient*, JsonDocument&)> checkAuth,
    std::function<void(AsyncWebSocketClient*)> onConnect,
    std::function<void(AsyncWebSocketClient*)> onDisconnect,
    std::function<void(AsyncWebSocketClient*, JsonDocument&)> fallbackHandler
)
    : m_ws(ws)
    , m_ctx(ctx)  // Copy: ctx in WebServer::setupWebSocket() is stack-allocated
    , m_checkRateLimit(checkRateLimit)
    , m_checkAuth(checkAuth)
    , m_onConnect(onConnect)
    , m_onDisconnect(onDisconnect)
    , m_fallbackHandler(fallbackHandler)
{
    s_instance = this;
}

void WsGateway::onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (!s_instance) return;

    switch (type) {
        case WS_EVT_CONNECT:
            s_instance->handleConnect(client);
            break;

        case WS_EVT_DISCONNECT:
            s_instance->handleDisconnect(client);
            break;

        case WS_EVT_DATA:
            s_instance->handleMessage(client, data, len);
            break;

        case WS_EVT_ERROR:
            LW_LOGW("WS: Error from client %u", client->id());
            break;

        case WS_EVT_PONG:
            // Pong received
            break;
    }
}

void WsGateway::handleConnect(AsyncWebSocketClient* client) {
    // Log that upgrade request was received and processed
    LW_LOGI("WS: Upgrade request received from %s (client ID: %u)", 
            client->remoteIP().toString().c_str(), client->id());
    
    // Ensure stale client entries are purged before applying connection limits.
    m_ws->cleanupClients();

    const uint32_t nowMs = millis();
    const IPAddress ip = client->remoteIP();
    const uint32_t ipKey =
        (static_cast<uint32_t>(ip[0]) << 24) |
        (static_cast<uint32_t>(ip[1]) << 16) |
        (static_cast<uint32_t>(ip[2]) << 8)  |
        (static_cast<uint32_t>(ip[3]) << 0);

    // #region agent log (DISABLED)
    // {
    //     // Hws1: Confirm connect thrash + measure reject causes (max clients vs cooldown).
    //     char buf[320];
    //     const int n = snprintf(
    //         buf, sizeof(buf),
    //         "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws1\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.enter\",\"data\":{\"clientId\":%lu,\"wsCount\":%u,\"wsMax\":%u,\"ip\":\"%s\"},\"timestamp\":%lu}",
    //         static_cast<unsigned long>(client->id()),
    //         static_cast<unsigned>(m_ws->count()),
    //         static_cast<unsigned>(lightwaveos::network::WebServerConfig::MAX_WS_CLIENTS),
    //         ip.toString().c_str(),
    //         static_cast<unsigned long>(nowMs)
    //     );
    //     if (n > 0) Serial.println(buf);
    // }
    // #endregion

    // Per-IP connection cooldown + single-session guard (reduces reconnect storms / overlap)
    if (ipKey != 0) {
        uint8_t slot = 0xFF;
        for (uint8_t i = 0; i < CONNECT_GUARD_SLOTS; i++) {
            if (m_connectGuard[i].ipKey == ipKey) {
                slot = i;
                break;
            }
            if (slot == 0xFF && m_connectGuard[i].ipKey == 0) {
                slot = i;  // first empty slot
            }
        }
        if (slot != 0xFF) {
            const uint32_t last = m_connectGuard[slot].lastMs;
            const bool tooSoon = (last != 0) && (nowMs - last < CONNECT_COOLDOWN_MS);
            m_connectGuard[slot].ipKey = ipKey;
            m_connectGuard[slot].lastMs = nowMs;
            if (tooSoon) {
                // #region agent log (DISABLED)
                // {
                //     char buf[320];
                //     const int n = snprintf(
                //         buf, sizeof(buf),
                //         "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws1\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.reject.cooldown\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"cooldownMs\":%lu},\"timestamp\":%lu}",
                //         static_cast<unsigned long>(client->id()),
                //         ip.toString().c_str(),
                //         static_cast<unsigned long>(CONNECT_COOLDOWN_MS),
                //         static_cast<unsigned long>(nowMs)
                //     );
                //     if (n > 0) Serial.println(buf);
                // }
                // #endregion
                client->close(1013, "Reconnect too fast");
                return;
            }

            // Reject overlapping WS sessions from the same IP.
            // This protects the device from clients that repeatedly call connect() without
            // closing the previous connection or without servicing the socket.
            if (m_connectGuard[slot].active >= 1) {
                // #region agent log (DISABLED)
                // {
                //     char buf[320];
                //     const int n = snprintf(
                //         buf, sizeof(buf),
                //         "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws2\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.reject.overlap\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"active\":%u},\"timestamp\":%lu}",
                //         static_cast<unsigned long>(client->id()),
                //         ip.toString().c_str(),
                //         static_cast<unsigned>(m_connectGuard[slot].active),
                //         static_cast<unsigned long>(nowMs)
                //     );
                //     if (n > 0) Serial.println(buf);
                // }
                // #endregion
                client->close(1008, "Only one session per device");
                return;
            }
        }
    }

    // Hard cap on connected WS clients (>=, not >)
    if (m_ws->count() >= lightwaveos::network::WebServerConfig::MAX_WS_CLIENTS) {
        LW_LOGW("WS: Max clients reached, rejecting %u", client->id());
        client->close(1008, "Connection limit");
        return;
    }

    LW_LOGI("WS: Client %u connected from %s", client->id(), client->remoteIP().toString().c_str());

    // Mark active for this IP (best-effort) and set initial activity timestamp
    if (ipKey != 0) {
        for (uint8_t i = 0; i < CONNECT_GUARD_SLOTS; i++) {
            if (m_connectGuard[i].ipKey == ipKey) {
                if (m_connectGuard[i].active < 255) m_connectGuard[i].active++;
                m_connectGuard[i].lastActivityMs = nowMs;  // Set initial activity time
                break;
            }
        }
    }

    // Store client ID → IP mapping for disconnect cleanup
    // (remoteIP() may return 0.0.0.0 after disconnect)
    if (ipKey != 0) {
        uint32_t clientId = client->id();
        // Find empty slot or existing entry for this client
        uint8_t mapSlot = 0xFF;
        for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
            if (m_clientIpMap[i].clientId == clientId) {
                // Update existing entry
                m_clientIpMap[i].ipKey = ipKey;
                mapSlot = i;
                break;
            }
            if (mapSlot == 0xFF && m_clientIpMap[i].clientId == 0) {
                mapSlot = i;  // first empty slot
            }
        }
        if (mapSlot != 0xFF && m_clientIpMap[mapSlot].clientId == 0) {
            // Store new mapping
            m_clientIpMap[mapSlot].clientId = clientId;
            m_clientIpMap[mapSlot].ipKey = ipKey;
        } else if (mapSlot == 0xFF) {
            // CRITICAL FIX: Table is full - evict slot 0 (LRU approximation) to ensure
            // we always have a mapping for disconnect cleanup. Without this, the active
            // counter can become permanently stuck.
            LW_LOGW("WS: IP mapping table full, evicting slot 0 for client %u", clientId);
            m_clientIpMap[0].clientId = clientId;
            m_clientIpMap[0].ipKey = ipKey;
        }
    }

    // Track connection epoch (increments on reconnect)
    uint32_t clientId = client->id();
    uint32_t connEpoch = getOrIncrementEpoch(clientId);  // Epoch is set/incremented here
    uint32_t eventSeq = s_eventSeq++;
    uint32_t tsMonoms = millis();

    // Structured telemetry: ws.connect event
    {
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ws.connect\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"ip\":\"%s\",\"result\":\"ok\",\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId),
            ip.toString().c_str()
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    }

    // Call connection callback (for status broadcasts, etc.)
    if (m_onConnect) {
        m_onConnect(client);
    }
}

uint32_t WsGateway::getOrIncrementEpoch(uint32_t clientId) {
    // Find existing epoch entry or create new one
    uint8_t epochSlot = 0xFF;
    for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
        if (m_clientEpochs[i].clientId == clientId) {
            // Client exists - increment epoch (contract: every connect = new epoch)
            m_clientEpochs[i].connEpoch++;
            m_clientEpochs[i].connectTs = millis();
            return m_clientEpochs[i].connEpoch;
        }
        if (epochSlot == 0xFF && m_clientEpochs[i].clientId == 0) {
            epochSlot = i;  // first empty slot
        }
    }
    
    // New client - create entry with epoch 0
    if (epochSlot != 0xFF) {
        m_clientEpochs[epochSlot].clientId = clientId;
        m_clientEpochs[epochSlot].connEpoch = 0;
        m_clientEpochs[epochSlot].connectTs = millis();
        return 0;
    }
    
    // Table full - return 0 (shouldn't happen with normal client counts)
    return 0;
}

void WsGateway::handleDisconnect(AsyncWebSocketClient* client) {
    uint32_t clientId = client->id();
    LW_LOGI("WS: Client %u disconnected", clientId);

    const uint32_t nowMs = millis();
    const IPAddress ip = client->remoteIP();
    uint32_t ipKey =
        (static_cast<uint32_t>(ip[0]) << 24) |
        (static_cast<uint32_t>(ip[1]) << 16) |
        (static_cast<uint32_t>(ip[2]) << 8)  |
        (static_cast<uint32_t>(ip[3]) << 0);

    // Lookup connection epoch before cleanup
    uint32_t connEpoch = 0;
    for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
        if (m_clientEpochs[i].clientId == clientId) {
            connEpoch = m_clientEpochs[i].connEpoch;
            // Clear epoch entry (disconnect cleanup)
            m_clientEpochs[i].clientId = 0;
            m_clientEpochs[i].connEpoch = 0;
            m_clientEpochs[i].connectTs = 0;
            break;
        }
    }
    
    uint32_t eventSeq = s_eventSeq++;
    
    // Structured telemetry: ws.disconnect event
    {
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ws.disconnect\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"ip\":\"%s\"}",
            static_cast<unsigned long>(nowMs),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId),
            ip.toString().c_str()
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    }

    // CRITICAL FIX: Always search mapping table FIRST before relying on remoteIP().
    // remoteIP() often returns 0.0.0.0 after disconnect, and the mapping table is our
    // reliable fallback. Clear the entry regardless to prevent stale mappings.
    uint32_t mappedIpKey = 0;
    for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
        if (m_clientIpMap[i].clientId == clientId) {
            mappedIpKey = m_clientIpMap[i].ipKey;
            // Clear the mapping entry
            m_clientIpMap[i].clientId = 0;
            m_clientIpMap[i].ipKey = 0;
            break;
        }
    }

    // Prefer mapped IP over remoteIP() (which may be 0.0.0.0 after disconnect)
    if (ipKey == 0 && mappedIpKey != 0) {
        ipKey = mappedIpKey;
    } else if (ipKey == 0 && mappedIpKey == 0) {
        LW_LOGW("WS: Client %u disconnected but no IP found (remoteIP=0, no mapping)", clientId);
    }

    // Mark inactive for this IP (best-effort)
    if (ipKey != 0) {
        for (uint8_t i = 0; i < CONNECT_GUARD_SLOTS; i++) {
            if (m_connectGuard[i].ipKey == ipKey) {
                if (m_connectGuard[i].active > 0) m_connectGuard[i].active--;
                break;
            }
        }
    }

    // #region agent log (DISABLED)
    // {
    //     // Hws3: Confirm whether disconnects correlate with zero messages received.
    //     char buf[320];
    //     const int n = snprintf(
    //         buf, sizeof(buf),
    //         "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws3\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleDisconnect\",\"message\":\"ws.disconnect\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"ipKey\":%lu},\"timestamp\":%lu}",
    //         static_cast<unsigned long>(clientId),
    //         ip.toString().c_str(),
    //         static_cast<unsigned long>(ipKey),
    //         static_cast<unsigned long>(nowMs)
    //     );
    //     if (n > 0) Serial.println(buf);
    // }
    // #endregion

    // Abort any OTA session owned by this client (epoch-scoping: disconnect resets OTA state)
#if FEATURE_OTA_UPDATE
    webserver::ws::handleOtaClientDisconnect(clientId);
#endif

    // Call disconnection callback (for cleanup)
    if (m_onDisconnect) {
        m_onDisconnect(client);
    }
}

bool WsGateway::validateOrigin(AsyncWebServerRequest* request) {
    // Policy A: Allow empty/missing Origin (non-browser clients) + allowlist local origins
    // OWASP guidance: validate Origin during handshake to prevent CSWSH
    
    String origin = request->header("Origin");
    
    // Allow: missing/empty Origin (non-browser clients commonly omit it)
    if (origin.length() == 0) {
        return true;
    }
    
    // Parse Origin URL to extract host
    // Origin format: "scheme://host[:port]"
    int schemeEnd = origin.indexOf("://");
    if (schemeEnd < 0) {
        // Malformed Origin - reject (but log it)
        uint32_t tsMonoms = millis();
        char originEscaped[128] = {0};
        escapeJsonString(origin.c_str(), originEscaped, sizeof(originEscaped));
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ws.connect\",\"ts_mono_ms\":%lu,\"eventSeq\":%lu,\"ip\":\"%s\","
            "\"result\":\"rejected\",\"reason\":\"origin_not_allowed\","
            "\"origin\":\"%s\",\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(s_eventSeq++),
            request->client()->remoteIP().toString().c_str(),
            originEscaped
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
        return false;
    }
    
    int hostStart = schemeEnd + 3;
    int hostEnd = origin.indexOf(":", hostStart);
    if (hostEnd < 0) {
        hostEnd = origin.indexOf("/", hostStart);
    }
    if (hostEnd < 0) {
        hostEnd = origin.length();
    }
    
    String host = origin.substring(hostStart, hostEnd);
    
    // Allow: lightwaveos.local (mDNS hostname)
    if (host == "lightwaveos.local" || host == "lightwaveos") {
        return true;
    }
    
    // Allow: device IP addresses (http://192.168.x.x or https://192.168.x.x)
    // Check if host is an IP address (simplified: check for dots and numeric pattern)
    IPAddress deviceIP = WiFi.localIP();
    if (deviceIP != IPAddress(0, 0, 0, 0)) {  // Valid IP (not 0.0.0.0)
        String ipStr = deviceIP.toString();
        if (host == ipStr) {
            return true;
        }
    }
    
    // Also check AP IP if in AP mode
    IPAddress apIP = WiFi.softAPIP();
    if (apIP != IPAddress(0, 0, 0, 0)) {  // Valid IP (not 0.0.0.0)
        String apIpStr = apIP.toString();
        if (host == apIpStr) {
            return true;
        }
    }
    
    // Reject: Origin not in allowlist
    uint32_t tsMonoms = millis();
    char originEscaped[128] = {0};
    escapeJsonString(origin.c_str(), originEscaped, sizeof(originEscaped));
    char buf[512];
    const int n = snprintf(buf, sizeof(buf),
        "{\"event\":\"ws.connect\",\"ts_mono_ms\":%lu,\"eventSeq\":%lu,\"ip\":\"%s\","
        "\"result\":\"rejected\",\"reason\":\"origin_not_allowed\","
        "\"origin\":\"%s\",\"schemaVersion\":\"1.0.0\"}",
        static_cast<unsigned long>(tsMonoms),
        static_cast<unsigned long>(s_eventSeq++),
        request->client()->remoteIP().toString().c_str(),
        originEscaped
    );
    if (n > 0 && n < static_cast<int>(sizeof(buf))) {
        Serial.println(buf);
    }
    
    return false;
}

void WsGateway::handleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Rate limit check
    if (!m_checkRateLimit(client)) {
        // Structured telemetry: msg.recv with result="rejected", reason="rate_limit"
        uint32_t clientId = client->id();
        uint32_t connEpoch = 0;
        for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
            if (m_clientEpochs[i].clientId == clientId) {
                connEpoch = m_clientEpochs[i].connEpoch;
                break;
            }
        }
        uint32_t eventSeq = s_eventSeq++;
        uint32_t tsMonoms = millis();
        
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"\",\"result\":\"rejected\",\"reason\":\"rate_limit\",\"payloadSummary\":\"\",\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
        return;  // Drop message silently
    }

    // Update activity timestamp for idle timeout tracking
    {
        const uint32_t nowMs = millis();
        const IPAddress ip = client->remoteIP();
        const uint32_t ipKey =
            (static_cast<uint32_t>(ip[0]) << 24) |
            (static_cast<uint32_t>(ip[1]) << 16) |
            (static_cast<uint32_t>(ip[2]) << 8)  |
            (static_cast<uint32_t>(ip[3]) << 0);
        if (ipKey != 0) {
            for (uint8_t i = 0; i < CONNECT_GUARD_SLOTS; i++) {
                if (m_connectGuard[i].ipKey == ipKey) {
                    m_connectGuard[i].lastActivityMs = nowMs;
                    break;
                }
            }
        }
    }

    // Parse message
    if (len > MAX_WS_MESSAGE_SIZE) {
        // Log rejected frame (oversize)
        uint32_t clientId = client->id();
        uint32_t connEpoch = 0;
        for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
            if (m_clientEpochs[i].clientId == clientId) {
                connEpoch = m_clientEpochs[i].connEpoch;
                break;
            }
        }
        uint32_t eventSeq = s_eventSeq++;
        uint32_t tsMonoms = millis();
        
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"\",\"result\":\"rejected\",\"reason\":\"size_limit\",\"payloadSummary\":\"\",\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
        
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Message too large"));
        return;
    }

    // #region agent log (DISABLED)
    // {
    //     // Hws3: Prove whether the encoder ever sends WS data before ack timeouts.
    //     char buf[320];
    //     const int n = snprintf(
    //         buf, sizeof(buf),
    //         "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws3\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleMessage\",\"message\":\"ws.message.recv\",\"data\":{\"clientId\":%lu,\"len\":%u,\"ip\":\"%s\"},\"timestamp\":%lu}",
    //         static_cast<unsigned long>(client->id()),
    //         static_cast<unsigned>(len),
    //         client->remoteIP().toString().c_str(),
    //         static_cast<unsigned long>(millis())
    //     );
    //     if (n > 0) Serial.println(buf);
    // }
    // #endregion

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    // Structured telemetry: msg.recv event (log ALL inbound frames, before auth check)
    uint32_t clientId = client->id();
    uint32_t connEpoch = 0;
    
    // Lookup connection epoch for this client
    for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
        if (m_clientEpochs[i].clientId == clientId) {
            connEpoch = m_clientEpochs[i].connEpoch;
            break;
        }
    }
    
    uint32_t eventSeq = s_eventSeq++;
    uint32_t tsMonoms = millis();
    
    if (error) {
        // Log rejected frame (parse error)
        // Create bounded payload summary from raw data (~100 chars max, escaped for JSON)
        char payloadSummaryRaw[128] = {0};
        size_t copyLen = len < (sizeof(payloadSummaryRaw) - 1) ? len : (sizeof(payloadSummaryRaw) - 1);
        memcpy(payloadSummaryRaw, data, copyLen);
        payloadSummaryRaw[copyLen] = '\0';
        
        // Escape for JSON embedding
        char payloadSummaryEscaped[256] = {0};  // Larger buffer for escaped content
        escapeJsonString(payloadSummaryRaw, payloadSummaryEscaped, sizeof(payloadSummaryEscaped));
        
        // #region agent log (DISABLED)
        // {
        //     // Hwse2: Prove whether disconnects follow parse errors (invalid JSON / partial frames).
        //     char buf[360];
        //     const int n = snprintf(
        //         buf, sizeof(buf),
        //         "{\"sessionId\":\"debug-session\",\"runId\":\"ws-drop-pre\",\"hypothesisId\":\"Hwse2\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleMessage\",\"message\":\"ws.json.parse_error\",\"data\":{\"clientId\":%lu,\"len\":%u},\"timestamp\":%lu}",
        //         static_cast<unsigned long>(client->id()),
        //         static_cast<unsigned>(len),
        //         static_cast<unsigned long>(millis())
        //     );
        //     if (n > 0) Serial.println(buf);
        // }
        // #endregion
        
        // Structured telemetry: msg.recv with result="rejected" (DISABLED - verbose)
        // {
        //     char buf[512];
        //     const int n = snprintf(buf, sizeof(buf),
        //         "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"\",\"result\":\"rejected\",\"reason\":\"parse_error\",\"payloadSummary\":\"%.200s\",\"schemaVersion\":\"1.0.0\"}",
        //         static_cast<unsigned long>(tsMonoms),
        //         static_cast<unsigned long>(connEpoch),
        //         static_cast<unsigned long>(eventSeq),
        //         static_cast<unsigned long>(clientId),
        //         payloadSummaryEscaped
        //     );
        //     if (n > 0 && n < static_cast<int>(sizeof(buf))) {
        //         Serial.println(buf);
        //     }
        // }
        
        client->text(buildWsError(ErrorCodes::INVALID_JSON, "Parse error"));
        return;
    }
    
    using namespace lightwaveos::codec;
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TypeDecodeResult typeResult = WsCommonCodec::decodeType(root);
    const char* msgType = typeResult.type;
    
    // Create bounded payload summary (~100 chars max)
    char payloadSummaryRaw[128] = {0};
    serializeJson(doc, payloadSummaryRaw, sizeof(payloadSummaryRaw) - 1);
    
    // Escape for JSON embedding (payloadSummary contains JSON that needs escaping)
    char payloadSummaryEscaped[256] = {0};  // Larger buffer for escaped content
    escapeJsonString(payloadSummaryRaw, payloadSummaryEscaped, sizeof(payloadSummaryEscaped));
    
    // Auth check (before logging msg.recv to determine result)
    bool authPassed = m_checkAuth(client, doc);
    
    // Log structured msg.recv event (DISABLED - verbose, logs every WS message)
    // {
    //     char buf[512];
    //     const int n = snprintf(buf, sizeof(buf),
    //         "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"%s\",\"result\":\"%s\",\"reason\":\"%s\",\"payloadSummary\":\"%.200s\",\"schemaVersion\":\"1.0.0\"}",
    //         static_cast<unsigned long>(tsMonoms),
    //         static_cast<unsigned long>(connEpoch),
    //         static_cast<unsigned long>(eventSeq),
    //         static_cast<unsigned long>(clientId),
    //         msgType,
    //         authPassed ? "ok" : "rejected",
    //         authPassed ? "" : "auth_failed",
    //         payloadSummaryEscaped
    //     );
    //     if (n > 0 && n < static_cast<int>(sizeof(buf))) {
    //         Serial.println(buf);
    //     }
    // }
    
    if (!authPassed) {
        // Auth failure - error response already sent by auth checker
        return;
    }

    // Route command via WsCommandRouter
    const char* typeStr = msgType;
    bool handled = WsCommandRouter::route(client, doc, m_ctx);

    // #region agent log (DISABLED)
    // {
    //     // Hwse1: Determine whether encoder sends unknown command types that trigger errors/closures.
    //     char buf[380];
    //     const int n = snprintf(
    //         buf, sizeof(buf),
    //         "{\"sessionId\":\"debug-session\",\"runId\":\"ws-drop-pre\",\"hypothesisId\":\"Hwse1\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleMessage\",\"message\":\"ws.route.result\",\"data\":{\"clientId\":%lu,\"type\":\"%s\",\"handled\":%s},\"timestamp\":%lu}",
    //         static_cast<unsigned long>(client->id()),
    //         typeStr,
    //         handled ? "true" : "false",
    //         static_cast<unsigned long>(millis())
    //     );
    //     if (n > 0) Serial.println(buf);
    // }
    // #endregion
    
    // If not handled by router, send error (all commands should be registered)
    if (!handled) {
        RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown command type", requestIdResult.requestId));
    }
}

void WsGateway::cleanupStaleConnections() {
    const uint32_t nowMs = millis();
    for (uint8_t i = 0; i < CONNECT_GUARD_SLOTS; i++) {
        if (m_connectGuard[i].active > 0 && m_connectGuard[i].ipKey != 0) {
            const uint32_t idleMs = nowMs - m_connectGuard[i].lastActivityMs;
            if (idleMs > IDLE_TIMEOUT_MS) {
                LW_LOGW("WS: Clearing stale guard entry slot %u (idle %lu ms)", i, idleMs);
                m_connectGuard[i].active = 0;
                // Structured telemetry: ws.stale_cleanup event
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "{\"event\":\"ws.stale_cleanup\",\"ts_mono_ms\":%lu,\"ipSlot\":%u,\"idleMs\":%lu}",
                    static_cast<unsigned long>(nowMs), static_cast<unsigned>(i), static_cast<unsigned long>(idleMs));
                Serial.println(buf);
            }
        }
    }
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

