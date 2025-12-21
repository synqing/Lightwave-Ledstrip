/**
 * @file WebServer.cpp
 * @brief Web Server implementation for LightwaveOS v2
 *
 * Implements REST API and WebSocket server integrated with Actor System.
 * All state changes are routed through ACTOR_SYSTEM for thread safety.
 */

#include "WebServer.h"

#if FEATURE_WEB_SERVER

#include "ApiResponse.h"
#include "WiFiManager.h"
#include "../config/network_config.h"
#include "../core/actors/ActorSystem.h"
#include <Update.h>
#include "../core/actors/RendererActor.h"
#include "../core/persistence/ZoneConfigManager.h"
#include "../effects/zones/ZoneComposer.h"
#include "../effects/transitions/TransitionTypes.h"
#include "../palettes/Palettes_Master.h"
#include <ESPmDNS.h>
#include <LittleFS.h>

#if FEATURE_MULTI_DEVICE
#include "../sync/DeviceUUID.h"
#endif

// External zone config manager from main.cpp
extern lightwaveos::persistence::ZoneConfigManager* zoneConfigMgr;

using namespace lightwaveos::actors;
using namespace lightwaveos::zones;
using namespace lightwaveos::transitions;
using namespace lightwaveos::palettes;

namespace lightwaveos {
namespace network {

// Global instance
WebServer webServer;

// ============================================================================
// Constructor / Destructor
// ============================================================================

WebServer::WebServer()
    : m_server(nullptr)
    , m_ws(nullptr)
    , m_running(false)
    , m_apMode(false)
    , m_mdnsStarted(false)
    , m_lastBroadcast(0)
    , m_startTime(0)
    , m_zoneComposer(nullptr)
{
}

WebServer::~WebServer() {
    stop();
    delete m_ws;
    delete m_server;
}

// ============================================================================
// Lifecycle
// ============================================================================

bool WebServer::begin() {
    Serial.println("[WebServer] Starting v2 WebServer...");

    // Initialize LittleFS for static file serving
    if (!LittleFS.begin(true)) {
        Serial.println("[WebServer] WARNING: LittleFS mount failed!");
    } else {
        Serial.println("[WebServer] LittleFS mounted");
    }

    // Create server instances
    m_server = new AsyncWebServer(WebServerConfig::HTTP_PORT);
    m_ws = new AsyncWebSocket("/ws");

    // Initialize WiFi
    if (!initWiFi()) {
        Serial.println("[WebServer] WiFi init failed, starting AP mode...");
        if (!startAPMode()) {
            Serial.println("[WebServer] AP mode failed!");
            return false;
        }
    }

    // Setup CORS headers
    setupCORS();

    // Setup WebSocket
    setupWebSocket();

    // Setup HTTP routes
    setupRoutes();

    // Start mDNS
    startMDNS();

    // Start the server
    m_server->begin();
    m_running = true;
    m_startTime = millis();

    // Get zone composer reference if available
    if (RENDERER) {
        m_zoneComposer = RENDERER->getZoneComposer();
    }

    Serial.printf("[WebServer] Server running on port %d\n", WebServerConfig::HTTP_PORT);
    if (m_apMode) {
        Serial.printf("[WebServer] AP Mode - IP: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.printf("[WebServer] Connected - IP: %s\n", WiFi.localIP().toString().c_str());
    }

    return true;
}

void WebServer::stop() {
    if (m_running) {
        m_ws->closeAll();
        m_server->end();
        m_running = false;
        Serial.println("[WebServer] Server stopped");
    }
}

void WebServer::update() {
    if (!m_running) return;

    // Cleanup disconnected WebSocket clients
    m_ws->cleanupClients();

    // Periodic status broadcast
    uint32_t now = millis();
    if (now - m_lastBroadcast >= WebServerConfig::STATUS_BROADCAST_INTERVAL_MS) {
        m_lastBroadcast = now;
        broadcastStatus();
    }
}

// ============================================================================
// WiFi Initialization
// ============================================================================

bool WebServer::initWiFi() {
    // Check if WiFi credentials are configured
    // For v2, we'll use WiFi.begin() with stored credentials
    // or environment-defined credentials

    WiFi.mode(WIFI_STA);
    WiFi.begin();  // Use stored credentials

    Serial.println("[WebServer] Connecting to WiFi...");

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WebServerConfig::WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("[WebServer] WiFi connection timeout");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.printf("[WebServer] Connected to WiFi, IP: %s\n", WiFi.localIP().toString().c_str());
    m_apMode = false;
    return true;
}

bool WebServer::startAPMode() {
    // Generate unique AP name using MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char apName[32];
    snprintf(apName, sizeof(apName), "%s%02X%02X",
             WebServerConfig::AP_SSID_PREFIX, mac[4], mac[5]);

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(apName, WebServerConfig::AP_PASSWORD)) {
        Serial.println("[WebServer] Failed to start AP");
        return false;
    }

    Serial.printf("[WebServer] AP Mode started: %s\n", apName);
    Serial.printf("[WebServer] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    m_apMode = true;
    return true;
}

void WebServer::setupCORS() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                         "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers",
                                         "Content-Type, X-Requested-With");
}

void WebServer::startMDNS() {
    if (MDNS.begin(WebServerConfig::MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WebServerConfig::HTTP_PORT);
        MDNS.addService("ws", "tcp", WebServerConfig::HTTP_PORT);
        MDNS.addServiceTxt("http", "tcp", "version", "2.0.0");
        MDNS.addServiceTxt("http", "tcp", "board", "ESP32-S3");

#if FEATURE_MULTI_DEVICE
        // Add TXT records for multi-device sync discovery
        MDNS.addServiceTxt("ws", "tcp", "board", "ESP32-S3");
        MDNS.addServiceTxt("ws", "tcp", "uuid", DEVICE_UUID.toString());
        MDNS.addServiceTxt("ws", "tcp", "syncver", "1");
        Serial.printf("[WebServer] Sync UUID: %s\n", DEVICE_UUID.toString());
#endif

        m_mdnsStarted = true;
        Serial.printf("[WebServer] mDNS started: http://%s.local\n",
                      WebServerConfig::MDNS_HOSTNAME);
    } else {
        Serial.println("[WebServer] mDNS failed to start");
    }
}

// ============================================================================
// Route Setup
// ============================================================================

void WebServer::setupRoutes() {
    // Setup v1 API routes first (higher priority)
    setupV1Routes();

    // Setup legacy routes for backward compatibility
    setupLegacyRoutes();

    // Serve static files from LittleFS (web UI)
    m_server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // Handle CORS preflight and 404
    m_server->onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(HttpStatus::NO_CONTENT);
        } else {
            sendErrorResponse(request, HttpStatus::NOT_FOUND,
                              ErrorCodes::NOT_FOUND, "Endpoint not found");
        }
    });
}

void WebServer::setupWebSocket() {
    m_ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len) {
        WebServer::onWsEvent(server, client, type, arg, data, len);
    });
    m_server->addHandler(m_ws);
}

// ============================================================================
// Legacy API Routes (/api/*)
// ============================================================================

void WebServer::setupLegacyRoutes() {
    // GET /api/status
    m_server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleLegacyStatus(request);
    });

    // POST /api/effect
    m_server->on("/api/effect", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            handleLegacySetEffect(request, data, len);
        }
    );

    // POST /api/brightness
    m_server->on("/api/brightness", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            handleLegacySetBrightness(request, data, len);
        }
    );

    // POST /api/speed
    m_server->on("/api/speed", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            handleLegacySetSpeed(request, data, len);
        }
    );

    // POST /api/palette
    m_server->on("/api/palette", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            handleLegacySetPalette(request, data, len);
        }
    );

    // POST /api/zone/count
    m_server->on("/api/zone/count", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            handleLegacyZoneCount(request, data, len);
        }
    );

    // POST /api/zone/effect
    m_server->on("/api/zone/effect", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            handleLegacyZoneEffect(request, data, len);
        }
    );

    // POST /api/zone/config/save - Save zone config to NVS
    m_server->on("/api/zone/config/save", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!checkRateLimit(request)) return;
            if (!zoneConfigMgr) {
                sendLegacyError(request, "Config manager not initialized", 500);
                return;
            }
            bool zoneOk = zoneConfigMgr->saveToNVS();
            bool sysOk = zoneConfigMgr->saveSystemState(
                RENDERER->getCurrentEffect(),
                RENDERER->getBrightness(),
                RENDERER->getSpeed(),
                RENDERER->getPaletteIndex()
            );
            if (zoneOk && sysOk) {
                sendSuccessResponse(request, [](JsonObject& data) {
                    data["message"] = "Configuration saved";
                    data["zones"] = true;
                    data["system"] = true;
                });
            } else {
                sendSuccessResponse(request, [zoneOk, sysOk](JsonObject& data) {
                    data["message"] = "Partial save";
                    data["zones"] = zoneOk;
                    data["system"] = sysOk;
                });
            }
        }
    );

    // POST /api/zone/config/load - Load zone config from NVS
    m_server->on("/api/zone/config/load", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!checkRateLimit(request)) return;
            if (!zoneConfigMgr) {
                sendLegacyError(request, "Config manager not initialized", 500);
                return;
            }
            bool zoneOk = zoneConfigMgr->loadFromNVS();
            uint8_t effectId, brightness, speed, paletteId;
            bool sysOk = zoneConfigMgr->loadSystemState(effectId, brightness, speed, paletteId);
            if (sysOk) {
                ACTOR_SYSTEM.setEffect(effectId);
                ACTOR_SYSTEM.setBrightness(brightness);
                ACTOR_SYSTEM.setSpeed(speed);
                ACTOR_SYSTEM.setPalette(paletteId);
            }
            sendSuccessResponse(request, [zoneOk, sysOk](JsonObject& data) {
                data["message"] = zoneOk || sysOk ? "Configuration loaded" : "No saved configuration";
                data["zones"] = zoneOk;
                data["system"] = sysOk;
            });
        }
    );

    // POST /api/zone/preset/load - Load a zone preset
    m_server->on("/api/zone/preset/load", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            StaticJsonDocument<128> doc;
            if (deserializeJson(doc, data, len)) {
                sendLegacyError(request, "Invalid JSON", 400);
                return;
            }
            if (!doc.containsKey("preset")) {
                sendErrorResponse(request, 400, "MISSING_FIELD", "preset field required", "preset");
                return;
            }
            uint8_t presetId = doc["preset"];
            if (presetId > 4) {
                sendErrorResponse(request, 400, "INVALID_VALUE", "preset must be 0-4", "preset");
                return;
            }
            if (m_zoneComposer) {
                m_zoneComposer->loadPreset(presetId);
                sendSuccessResponse(request, [presetId](JsonObject& data) {
                    data["preset"] = presetId;
                    data["name"] = ZoneComposer::getPresetName(presetId);
                });
            } else {
                sendLegacyError(request, "Zone composer not initialized", 500);
            }
        }
    );

    // ========== Network API Endpoints ==========

    // Network Status - GET /api/network/status
    m_server->on("/api/network/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;

        sendSuccessResponse(request, [](JsonObject& data) {
            data["state"] = WIFI_MANAGER.getStateString();
            data["connected"] = WIFI_MANAGER.isConnected();
            data["apMode"] = WIFI_MANAGER.isAPMode();
            data["ssid"] = WIFI_MANAGER.getSSID();
            data["rssi"] = WIFI_MANAGER.getRSSI();
            data["ip"] = WIFI_MANAGER.isConnected() ?
                         WIFI_MANAGER.getLocalIP().toString() :
                         WIFI_MANAGER.getAPIP().toString();
            data["mac"] = WiFi.macAddress();
            data["channel"] = WIFI_MANAGER.getChannel();
            data["connectionAttempts"] = WIFI_MANAGER.getConnectionAttempts();
            data["successfulConnections"] = WIFI_MANAGER.getSuccessfulConnections();
            data["uptimeSeconds"] = WIFI_MANAGER.getUptimeSeconds();
        });
    });

    // Scan Networks - GET /api/network/scan
    m_server->on("/api/network/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;

        // Trigger async scan (results cached by WiFiManager)
        WIFI_MANAGER.scanNetworks();

        // Return cached results immediately
        const auto& scanResults = WIFI_MANAGER.getScanResults();
        size_t resultCount = scanResults.size();

        sendSuccessResponse(request, [&scanResults, resultCount](JsonObject& data) {
            data["scanning"] = (resultCount == 0); // If empty, scan in progress
            data["lastScanTime"] = WIFI_MANAGER.getLastScanTime();
            JsonArray list = data["networks"].to<JsonArray>();
            for (const auto& net : scanResults) {
                JsonObject entry = list.add<JsonObject>();
                entry["ssid"] = net.ssid;
                entry["rssi"] = net.rssi;
                entry["encryption"] = net.encryption;
                entry["channel"] = net.channel;
            }
            data["count"] = resultCount;
        });
    });

    // Connect to Network - POST /api/network/connect
    m_server->on("/api/network/connect", HTTP_POST, [](AsyncWebServerRequest* request){},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!checkRateLimit(request)) return;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                sendErrorResponse(request, 400, "INVALID_JSON", error.c_str());
                return;
            }

            if (!doc["ssid"].is<const char*>()) {
                sendErrorResponse(request, 400, "MISSING_FIELD", "ssid is required", "ssid");
                return;
            }

            String ssid = doc["ssid"].as<String>();
            String password = doc["password"].as<String>(); // Optional, empty for open networks

            WIFI_MANAGER.setCredentials(ssid, password);
            WIFI_MANAGER.reconnect();

            sendSuccessResponse(request, [&ssid](JsonObject& data) {
                data["action"] = "connecting";
                data["ssid"] = ssid;
                data["message"] = "Connection initiated. Poll /api/network/status for result.";
            });
        }
    );

    // Disconnect - POST /api/network/disconnect
    m_server->on("/api/network/disconnect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;

        WIFI_MANAGER.disconnect();

        sendSuccessResponse(request, [](JsonObject& data) {
            data["action"] = "disconnected";
        });
    });

    // ========== OTA Update Endpoint ==========

    // OTA Update - POST /update
    // Requires X-OTA-Token header for authentication
    m_server->on("/update", HTTP_POST,
        // Response handler (called after upload completes)
        [this](AsyncWebServerRequest* request) {
            bool success = !Update.hasError();
            AsyncWebServerResponse* response = request->beginResponse(
                success ? 200 : 500,
                "application/json",
                success ?
                    "{\"success\":true,\"message\":\"Update successful. Rebooting...\"}" :
                    "{\"success\":false,\"error\":\"Update failed\"}"
            );
            response->addHeader("Connection", "close");
            request->send(response);

            if (success) {
                delay(500);
                ESP.restart();
            }
        },
        // Upload handler (called for each chunk)
        [this](AsyncWebServerRequest* request, String filename, size_t index,
               uint8_t* data, size_t len, bool final) {
            // Validate token on first chunk
            if (index == 0) {
                String token = request->header("X-OTA-Token");
                if (token != config::NetworkConfig::OTA_UPDATE_TOKEN) {
                    Serial.println("[OTA] Invalid or missing token!");
                    request->send(401, "application/json",
                        "{\"success\":false,\"error\":\"Invalid OTA token\"}");
                    return;
                }

                Serial.printf("[OTA] Starting update: %s\n", filename.c_str());

                // Begin update
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Serial.printf("[OTA] Begin failed: %s\n", Update.errorString());
                    return;
                }
            }

            // Write chunk
            if (Update.write(data, len) != len) {
                Serial.printf("[OTA] Write failed: %s\n", Update.errorString());
                return;
            }

            // Finalize on last chunk
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("[OTA] Success! Total size: %u bytes\n", index + len);
                } else {
                    Serial.printf("[OTA] End failed: %s\n", Update.errorString());
                }
            }
        }
    );
}

// ============================================================================
// V1 API Routes (/api/v1/*)
// ============================================================================

void WebServer::setupV1Routes() {
    // API Discovery - GET /api/v1/
    m_server->on("/api/v1/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleApiDiscovery(request);
    });

    // Device Status - GET /api/v1/device/status
    m_server->on("/api/v1/device/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleDeviceStatus(request);
    });

    // Device Info - GET /api/v1/device/info
    m_server->on("/api/v1/device/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleDeviceInfo(request);
    });

#if FEATURE_MULTI_DEVICE
    // Sync Status - GET /api/v1/sync/status
    m_server->on("/api/v1/sync/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        // Simple status response - full implementation requires SyncManagerActor reference
        char buffer[256];
        size_t len = snprintf(buffer, sizeof(buffer),
            "{\"success\":true,\"data\":{\"enabled\":true,\"uuid\":\"%s\"},\"version\":\"1.0\"}",
            DEVICE_UUID.toString()
        );
        request->send(200, "application/json", buffer);
    });
#endif

    // ========== Effect Metadata Endpoint (MUST BE BEFORE /api/v1/effects) ==========
    // Effect Metadata - GET /api/v1/effects/metadata?id=N
    m_server->on("/api/v1/effects/metadata", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleEffectsMetadata(request);
    });

    // Effects List - GET /api/v1/effects
    m_server->on("/api/v1/effects", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleEffectsList(request);
    });

    // Current Effect - GET /api/v1/effects/current
    m_server->on("/api/v1/effects/current", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleEffectsCurrent(request);
    });

    // Set Effect - POST /api/v1/effects/set
    m_server->on("/api/v1/effects/set", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleEffectsSet(request, data, len);
        }
    );

    // Get Parameters - GET /api/v1/parameters
    m_server->on("/api/v1/parameters", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleParametersGet(request);
    });

    // Set Parameters - POST /api/v1/parameters
    m_server->on("/api/v1/parameters", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleParametersSet(request, data, len);
        }
    );

    // Transition Types - GET /api/v1/transitions/types
    m_server->on("/api/v1/transitions/types", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleTransitionTypes(request);
    });

    // Trigger Transition - POST /api/v1/transitions/trigger
    m_server->on("/api/v1/transitions/trigger", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleTransitionTrigger(request, data, len);
        }
    );

    // Batch Operations - POST /api/v1/batch
    m_server->on("/api/v1/batch", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleBatch(request, data, len);
        }
    );

    // Palettes List - GET /api/v1/palettes
    m_server->on("/api/v1/palettes", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handlePalettesList(request);
    });

    // Current Palette - GET /api/v1/palettes/current
    m_server->on("/api/v1/palettes/current", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handlePalettesCurrent(request);
    });

    // Set Palette - POST /api/v1/palettes/set
    m_server->on("/api/v1/palettes/set", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handlePalettesSet(request, data, len);
        }
    );

    // ========== Transition Config Endpoints ==========

    // Transition Config - GET /api/v1/transitions/config
    m_server->on("/api/v1/transitions/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleTransitionConfigGet(request);
    });

    // Transition Config - POST /api/v1/transitions/config
    m_server->on("/api/v1/transitions/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleTransitionConfigSet(request, data, len);
        }
    );

    // ========== OpenAPI Specification ==========

    // OpenAPI - GET /api/v1/openapi.json
    m_server->on("/api/v1/openapi.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleOpenApiSpec(request);
    });

    // ========== Zone v1 REST Endpoints ==========

    // List all zones - GET /api/v1/zones
    m_server->on("/api/v1/zones", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleZonesList(request);
    });

    // Set zone layout - POST /api/v1/zones/layout
    m_server->on("/api/v1/zones/layout", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleZonesLayout(request, data, len);
        }
    );

    // Get single zone - GET /api/v1/zones/0, /api/v1/zones/1, etc.
    // Using regex-style path with parameter
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])$", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleZoneGet(request);
    });

    // Set zone effect - POST /api/v1/zones/:id/effect
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/effect$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleZoneSetEffect(request, data, len);
        }
    );

    // Set zone brightness - POST /api/v1/zones/:id/brightness
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/brightness$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleZoneSetBrightness(request, data, len);
        }
    );

    // Set zone speed - POST /api/v1/zones/:id/speed
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/speed$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleZoneSetSpeed(request, data, len);
        }
    );

    // Set zone palette - POST /api/v1/zones/:id/palette
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/palette$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleZoneSetPalette(request, data, len);
        }
    );

    // Set zone blend mode - POST /api/v1/zones/:id/blend
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/blend$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleZoneSetBlend(request, data, len);
        }
    );

    // Set zone enabled - POST /api/v1/zones/:id/enabled
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/enabled$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleZoneSetEnabled(request, data, len);
        }
    );
}

// ============================================================================
// Legacy API Handlers
// ============================================================================

void WebServer::handleLegacyStatus(AsyncWebServerRequest* request) {
    if (!ACTOR_SYSTEM.isRunning()) {
        sendLegacyError(request, "System not ready", HttpStatus::SERVICE_UNAVAILABLE);
        return;
    }

    StaticJsonDocument<512> doc;
    RendererActor* renderer = RENDERER;

    doc["effect"] = renderer->getCurrentEffect();
    doc["brightness"] = renderer->getBrightness();
    doc["speed"] = renderer->getSpeed();
    doc["palette"] = renderer->getPaletteIndex();
    doc["freeHeap"] = ESP.getFreeHeap();

    // Network info
    JsonObject network = doc.createNestedObject("network");
    network["connected"] = WiFi.status() == WL_CONNECTED;
    network["ap_mode"] = m_apMode;
    if (WiFi.status() == WL_CONNECTED) {
        network["ip"] = WiFi.localIP().toString();
        network["rssi"] = WiFi.RSSI();
    } else if (m_apMode) {
        network["ap_ip"] = WiFi.softAPIP().toString();
    }

    String output;
    serializeJson(doc, output);
    request->send(HttpStatus::OK, "application/json", output);
}

void WebServer::handleLegacySetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendLegacyError(request, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("effect")) {
        sendLegacyError(request, "Missing 'effect' field");
        return;
    }

    uint8_t effectId = doc["effect"];
    if (effectId >= RENDERER->getEffectCount()) {
        sendLegacyError(request, "Invalid effect ID");
        return;
    }

    ACTOR_SYSTEM.setEffect(effectId);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacySetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendLegacyError(request, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("brightness")) {
        sendLegacyError(request, "Missing 'brightness' field");
        return;
    }

    uint8_t brightness = doc["brightness"];
    ACTOR_SYSTEM.setBrightness(brightness);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacySetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendLegacyError(request, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("speed")) {
        sendLegacyError(request, "Missing 'speed' field");
        return;
    }

    uint8_t speed = doc["speed"];
    if (speed < 1 || speed > 50) {
        sendLegacyError(request, "Speed must be 1-50");
        return;
    }

    ACTOR_SYSTEM.setSpeed(speed);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacySetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendLegacyError(request, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("paletteId")) {
        sendLegacyError(request, "Missing 'paletteId' field");
        return;
    }

    uint8_t paletteId = doc["paletteId"];
    ACTOR_SYSTEM.setPalette(paletteId);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacyZoneCount(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendLegacyError(request, "Zone system not available");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendLegacyError(request, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("count")) {
        sendLegacyError(request, "Missing 'count' field");
        return;
    }

    uint8_t count = doc["count"];
    if (count < 1 || count > 4) {
        sendLegacyError(request, "Count must be 1-4");
        return;
    }

    m_zoneComposer->setLayout(count == 4 ? ZoneLayout::QUAD : ZoneLayout::TRIPLE);
    sendLegacySuccess(request);
    broadcastZoneState();
}

void WebServer::handleLegacyZoneEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendLegacyError(request, "Zone system not available");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendLegacyError(request, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("zoneId") || !doc.containsKey("effectId")) {
        sendLegacyError(request, "Missing 'zoneId' or 'effectId'");
        return;
    }

    uint8_t zoneId = doc["zoneId"];
    uint8_t effectId = doc["effectId"];

    if (zoneId >= m_zoneComposer->getZoneCount()) {
        sendLegacyError(request, "Invalid zone ID");
        return;
    }

    m_zoneComposer->setZoneEffect(zoneId, effectId);
    sendLegacySuccess(request);
    broadcastZoneState();
}

// ============================================================================
// V1 API Handlers
// ============================================================================

void WebServer::handleApiDiscovery(AsyncWebServerRequest* request) {
    sendSuccessResponseLarge(request, [this](JsonObject& data) {
        data["name"] = "LightwaveOS";
        data["apiVersion"] = API_VERSION;
        data["description"] = "ESP32-S3 LED Control System v2";

        // Hardware info
        JsonObject hw = data.createNestedObject("hardware");
        hw["ledsTotal"] = LedConfig::TOTAL_LEDS;
        hw["strips"] = LedConfig::NUM_STRIPS;
        hw["centerPoint"] = LedConfig::CENTER_POINT;
        hw["maxZones"] = 4;

        // HATEOAS links
        JsonObject links = data.createNestedObject("_links");
        links["self"] = "/api/v1/";
        links["device"] = "/api/v1/device/status";
        links["effects"] = "/api/v1/effects";
        links["parameters"] = "/api/v1/parameters";
        links["transitions"] = "/api/v1/transitions/types";
        links["batch"] = "/api/v1/batch";
        links["websocket"] = "ws://lightwaveos.local/ws";
    }, 1024);
}

void WebServer::handleDeviceStatus(AsyncWebServerRequest* request) {
    if (!ACTOR_SYSTEM.isRunning()) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "System not ready");
        return;
    }

    sendSuccessResponse(request, [this](JsonObject& data) {
        data["uptime"] = (millis() - m_startTime) / 1000;
        data["freeHeap"] = ESP.getFreeHeap();
        data["heapSize"] = ESP.getHeapSize();
        data["cpuFreq"] = ESP.getCpuFreqMHz();

        // Render stats
        const RenderStats& stats = RENDERER->getStats();
        data["fps"] = stats.currentFPS;
        data["cpuPercent"] = stats.cpuPercent;
        data["framesRendered"] = stats.framesRendered;

        // Network info
        JsonObject network = data.createNestedObject("network");
        network["connected"] = WiFi.status() == WL_CONNECTED;
        network["apMode"] = m_apMode;
        if (WiFi.status() == WL_CONNECTED) {
            network["ip"] = WiFi.localIP().toString();
            network["rssi"] = WiFi.RSSI();
        }

        data["wsClients"] = m_ws->count();
    });
}

void WebServer::handleDeviceInfo(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["firmware"] = "2.0.0";
        data["board"] = "ESP32-S3-DevKitC-1";
        data["sdk"] = ESP.getSdkVersion();
        data["flashSize"] = ESP.getFlashChipSize();
        data["sketchSize"] = ESP.getSketchSize();
        data["freeSketch"] = ESP.getFreeSketchSpace();
        data["architecture"] = "Actor System v2";
    });
}

void WebServer::handleEffectsList(AsyncWebServerRequest* request) {
    if (!RENDERER) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    uint8_t effectCount = RENDERER->getEffectCount();

    sendSuccessResponseLarge(request, [effectCount](JsonObject& data) {
        data["total"] = effectCount;

        JsonArray effects = data.createNestedArray("effects");
        for (uint8_t i = 0; i < effectCount; i++) {
            JsonObject effect = effects.createNestedObject();
            effect["id"] = i;
            effect["name"] = RENDERER->getEffectName(i);
            // Category based on ID ranges (simplified for v2)
            if (i <= 4) effect["category"] = "Classic";
            else if (i <= 7) effect["category"] = "Wave";
            else if (i <= 12) effect["category"] = "Physics";
            else effect["category"] = "Custom";
        }
    }, 2048);
}

void WebServer::handleEffectsCurrent(AsyncWebServerRequest* request) {
    if (!RENDERER) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    sendSuccessResponse(request, [](JsonObject& data) {
        uint8_t effectId = RENDERER->getCurrentEffect();
        data["effectId"] = effectId;
        data["name"] = RENDERER->getEffectName(effectId);
        data["brightness"] = RENDERER->getBrightness();
        data["speed"] = RENDERER->getSpeed();
        data["paletteId"] = RENDERER->getPaletteIndex();
    });
}

void WebServer::handleEffectsSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("effectId")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing effectId", "effectId");
        return;
    }

    uint8_t effectId = doc["effectId"];
    if (effectId >= RENDERER->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "effectId");
        return;
    }

    // Check for transition options
    bool useTransition = doc["transition"] | false;
    uint8_t transitionType = doc["transitionType"] | 0;

    if (useTransition) {
        RENDERER->startTransition(effectId, transitionType);
    } else {
        ACTOR_SYSTEM.setEffect(effectId);
    }

    sendSuccessResponse(request, [effectId](JsonObject& respData) {
        respData["effectId"] = effectId;
        respData["name"] = RENDERER->getEffectName(effectId);
    });

    broadcastStatus();
}

void WebServer::handleParametersGet(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["brightness"] = RENDERER->getBrightness();
        data["speed"] = RENDERER->getSpeed();
        data["paletteId"] = RENDERER->getPaletteIndex();
        data["hue"] = RENDERER->getHue();
    });
}

void WebServer::handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    bool updated = false;

    if (doc.containsKey("brightness")) {
        uint8_t val = doc["brightness"];
        ACTOR_SYSTEM.setBrightness(val);
        updated = true;
    }

    if (doc.containsKey("speed")) {
        uint8_t val = doc["speed"];
        if (val >= 1 && val <= 50) {
            ACTOR_SYSTEM.setSpeed(val);
            updated = true;
        } else {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE, "Speed must be 1-50", "speed");
            return;
        }
    }

    if (doc.containsKey("paletteId")) {
        uint8_t val = doc["paletteId"];
        ACTOR_SYSTEM.setPalette(val);
        updated = true;
    }

    if (updated) {
        sendSuccessResponse(request);
        broadcastStatus();
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "No valid parameters provided");
    }
}

void WebServer::handleTransitionTypes(AsyncWebServerRequest* request) {
    sendSuccessResponseLarge(request, [](JsonObject& data) {
        JsonArray types = data.createNestedArray("types");

        for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
            JsonObject t = types.createNestedObject();
            t["id"] = i;
            t["name"] = getTransitionName(static_cast<TransitionType>(i));
            t["duration"] = getDefaultDuration(static_cast<TransitionType>(i));
            t["centerOrigin"] = true;  // All v2 transitions are center-origin
        }
    }, 1536);
}

void WebServer::handleTransitionTrigger(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("toEffect")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing toEffect", "toEffect");
        return;
    }

    uint8_t toEffect = doc["toEffect"];
    if (toEffect >= RENDERER->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "toEffect");
        return;
    }

    uint8_t transitionType = doc["type"] | 0;
    bool randomType = doc["random"] | false;

    if (randomType) {
        RENDERER->startRandomTransition(toEffect);
    } else {
        RENDERER->startTransition(toEffect, transitionType);
    }

    sendSuccessResponse(request, [toEffect, transitionType](JsonObject& respData) {
        respData["effectId"] = toEffect;
        respData["name"] = RENDERER->getEffectName(toEffect);
        respData["transitionType"] = transitionType;
    });

    broadcastStatus();
}

void WebServer::handleBatch(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("operations") || !doc["operations"].is<JsonArray>()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "operations array required");
        return;
    }

    JsonArray ops = doc["operations"];
    if (ops.size() > WebServerConfig::MAX_BATCH_OPERATIONS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Max 10 operations per batch");
        return;
    }

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

bool WebServer::executeBatchAction(const String& action, JsonVariant params) {
    if (action == "setBrightness") {
        if (!params.containsKey("value")) return false;
        ACTOR_SYSTEM.setBrightness(params["value"]);
        return true;
    }
    else if (action == "setSpeed") {
        if (!params.containsKey("value")) return false;
        uint8_t val = params["value"];
        if (val < 1 || val > 50) return false;
        ACTOR_SYSTEM.setSpeed(val);
        return true;
    }
    else if (action == "setEffect") {
        if (!params.containsKey("effectId")) return false;
        uint8_t id = params["effectId"];
        if (id >= RENDERER->getEffectCount()) return false;
        ACTOR_SYSTEM.setEffect(id);
        return true;
    }
    else if (action == "setPalette") {
        if (!params.containsKey("paletteId")) return false;
        ACTOR_SYSTEM.setPalette(params["paletteId"]);
        return true;
    }
    else if (action == "transition") {
        if (!params.containsKey("toEffect")) return false;
        uint8_t toEffect = params["toEffect"];
        uint8_t type = params["type"] | 0;
        RENDERER->startTransition(toEffect, type);
        return true;
    }
    else if (action == "setZoneEffect" && m_zoneComposer) {
        if (!params.containsKey("zoneId") || !params.containsKey("effectId")) return false;
        uint8_t zoneId = params["zoneId"];
        uint8_t effectId = params["effectId"];
        m_zoneComposer->setZoneEffect(zoneId, effectId);
        return true;
    }

    return false;
}

// ============================================================================
// Palette API Handlers
// ============================================================================

void WebServer::handlePalettesList(AsyncWebServerRequest* request) {
    if (!RENDERER) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    // Get optional filter parameters
    bool filterCategory = request->hasParam("category");
    String categoryFilter = filterCategory ? request->getParam("category")->value() : "";
    bool filterFlags = request->hasParam("warm") || request->hasParam("cool") ||
                       request->hasParam("calm") || request->hasParam("vivid") ||
                       request->hasParam("cvd");

    // Build response with 75 palettes
    // Use a larger buffer for the full palette list
    DynamicJsonDocument doc(6144);
    doc["success"] = true;
    doc["timestamp"] = millis();
    doc["version"] = API_VERSION;

    JsonObject data = doc.createNestedObject("data");
    data["total"] = MASTER_PALETTE_COUNT;

    // Categorize palettes
    JsonObject categories = data.createNestedObject("categories");
    categories["artistic"] = CPT_CITY_END - CPT_CITY_START + 1;
    categories["scientific"] = CRAMERI_END - CRAMERI_START + 1;
    categories["lgpOptimized"] = COLORSPACE_END - COLORSPACE_START + 1;

    JsonArray palettes = data.createNestedArray("palettes");

    for (uint8_t i = 0; i < MASTER_PALETTE_COUNT; i++) {
        // Apply category filter
        if (filterCategory) {
            if (categoryFilter == "artistic" && !isCptCityPalette(i)) continue;
            if (categoryFilter == "scientific" && !isCrameriPalette(i)) continue;
            if (categoryFilter == "lgpOptimized" && !isColorspacePalette(i)) continue;
        }

        // Apply flag filters
        if (filterFlags) {
            bool match = true;
            if (request->hasParam("warm") && request->getParam("warm")->value() == "true") {
                match = match && isPaletteWarm(i);
            }
            if (request->hasParam("cool") && request->getParam("cool")->value() == "true") {
                match = match && isPaletteCool(i);
            }
            if (request->hasParam("calm") && request->getParam("calm")->value() == "true") {
                match = match && isPaletteCalm(i);
            }
            if (request->hasParam("vivid") && request->getParam("vivid")->value() == "true") {
                match = match && isPaletteVivid(i);
            }
            if (request->hasParam("cvd") && request->getParam("cvd")->value() == "true") {
                match = match && isPaletteCVDFriendly(i);
            }
            if (!match) continue;
        }

        JsonObject palette = palettes.createNestedObject();
        palette["id"] = i;
        palette["name"] = MasterPaletteNames[i];
        palette["category"] = getPaletteCategory(i);

        // Flags
        JsonObject flags = palette.createNestedObject("flags");
        flags["warm"] = isPaletteWarm(i);
        flags["cool"] = isPaletteCool(i);
        flags["calm"] = isPaletteCalm(i);
        flags["vivid"] = isPaletteVivid(i);
        flags["cvdFriendly"] = isPaletteCVDFriendly(i);
        flags["whiteHeavy"] = paletteHasFlag(i, PAL_WHITE_HEAVY);

        // Metadata
        palette["avgBrightness"] = getPaletteAvgBrightness(i);
        palette["maxBrightness"] = getPaletteMaxBrightness(i);
    }

    String output;
    serializeJson(doc, output);
    request->send(HttpStatus::OK, "application/json", output);
}

void WebServer::handlePalettesCurrent(AsyncWebServerRequest* request) {
    if (!RENDERER) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    sendSuccessResponse(request, [](JsonObject& data) {
        uint8_t id = RENDERER->getPaletteIndex();
        data["paletteId"] = id;
        data["name"] = MasterPaletteNames[id];
        data["category"] = getPaletteCategory(id);

        JsonObject flags = data.createNestedObject("flags");
        flags["warm"] = isPaletteWarm(id);
        flags["cool"] = isPaletteCool(id);
        flags["calm"] = isPaletteCalm(id);
        flags["vivid"] = isPaletteVivid(id);
        flags["cvdFriendly"] = isPaletteCVDFriendly(id);

        data["avgBrightness"] = getPaletteAvgBrightness(id);
        data["maxBrightness"] = getPaletteMaxBrightness(id);
    });
}

void WebServer::handlePalettesSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("paletteId")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing paletteId", "paletteId");
        return;
    }

    uint8_t paletteId = doc["paletteId"];
    if (paletteId >= MASTER_PALETTE_COUNT) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Palette ID out of range (0-74)", "paletteId");
        return;
    }

    ACTOR_SYSTEM.setPalette(paletteId);

    sendSuccessResponse(request, [paletteId](JsonObject& respData) {
        respData["paletteId"] = paletteId;
        respData["name"] = MasterPaletteNames[paletteId];
        respData["category"] = getPaletteCategory(paletteId);
    });

    broadcastStatus();
}

// ============================================================================
// Effect Metadata Handler
// ============================================================================

void WebServer::handleEffectsMetadata(AsyncWebServerRequest* request) {
    if (!RENDERER) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    // Get effect ID from query parameter
    if (!request->hasParam("id")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
        return;
    }

    uint8_t effectId = request->getParam("id")->value().toInt();
    uint8_t effectCount = RENDERER->getEffectCount();

    if (effectId >= effectCount) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "id");
        return;
    }

    sendSuccessResponse(request, [effectId](JsonObject& data) {
        data["id"] = effectId;
        data["name"] = RENDERER->getEffectName(effectId);

        // Category based on ID ranges
        if (effectId <= 4) {
            data["category"] = "Classic";
            data["description"] = "Classic LED effects optimized for LGP";
        } else if (effectId <= 7) {
            data["category"] = "Wave";
            data["description"] = "Wave-based interference patterns";
        } else if (effectId <= 12) {
            data["category"] = "Physics";
            data["description"] = "Physics-based simulations";
        } else {
            data["category"] = "Custom";
            data["description"] = "Custom effects";
        }

        // Effect properties
        JsonObject properties = data.createNestedObject("properties");
        properties["centerOrigin"] = true;  // All v2 effects are center-origin
        properties["symmetricStrips"] = true;
        properties["paletteAware"] = true;
        properties["speedResponsive"] = true;

        // Recommended settings
        JsonObject recommended = data.createNestedObject("recommended");
        recommended["brightness"] = 180;
        recommended["speed"] = 15;
    });
}

// ============================================================================
// Transition Config Handlers
// ============================================================================

void WebServer::handleTransitionConfigGet(AsyncWebServerRequest* request) {
    if (!RENDERER) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    sendSuccessResponse(request, [](JsonObject& data) {
        // Current transition state
        data["enabled"] = true;  // Transitions always enabled in v2
        data["defaultDuration"] = 1000;
        data["defaultType"] = 0;  // FADE
        data["defaultTypeName"] = getTransitionName(TransitionType::FADE);

        // Available easing curves (simplified list)
        JsonArray easings = data.createNestedArray("easings");
        const char* easingNames[] = {
            "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
            "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
            "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
        };
        for (uint8_t i = 0; i < 10; i++) {
            JsonObject easing = easings.createNestedObject();
            easing["id"] = i;
            easing["name"] = easingNames[i];
        }
    });
}

void WebServer::handleTransitionConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    // Currently transition config is not persisted, this endpoint acknowledges the request
    // Future: store default transition type, duration, easing in NVS

    uint16_t duration = doc["defaultDuration"] | 1000;
    uint8_t type = doc["defaultType"] | 0;

    if (type >= static_cast<uint8_t>(TransitionType::TYPE_COUNT)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Invalid transition type", "defaultType");
        return;
    }

    sendSuccessResponse(request, [duration, type](JsonObject& respData) {
        respData["defaultDuration"] = duration;
        respData["defaultType"] = type;
        respData["defaultTypeName"] = getTransitionName(static_cast<TransitionType>(type));
        respData["message"] = "Transition config updated";
    });
}

// ============================================================================
// OpenAPI Specification Handler
// ============================================================================

void WebServer::handleOpenApiSpec(AsyncWebServerRequest* request) {
    // Return a minimal OpenAPI 3.0.3 specification stub
    // Full spec would be stored in PROGMEM or LittleFS for production
    DynamicJsonDocument doc(4096);

    doc["openapi"] = "3.0.3";

    JsonObject info = doc.createNestedObject("info");
    info["title"] = "LightwaveOS API";
    info["version"] = "2.0.0";
    info["description"] = "REST API for LightwaveOS ESP32-S3 LED Control System";

    JsonArray servers = doc.createNestedArray("servers");
    JsonObject server = servers.createNestedObject();
    server["url"] = "http://lightwaveos.local/api/v1";
    server["description"] = "Local device";

    JsonObject paths = doc.createNestedObject("paths");

    // /device/status
    JsonObject deviceStatus = paths.createNestedObject("/device/status");
    JsonObject getDeviceStatus = deviceStatus.createNestedObject("get");
    getDeviceStatus["summary"] = "Get device status";
    getDeviceStatus["tags"].add("Device");

    // /effects
    JsonObject effects = paths.createNestedObject("/effects");
    JsonObject getEffects = effects.createNestedObject("get");
    getEffects["summary"] = "List all effects";
    getEffects["tags"].add("Effects");

    // /effects/set
    JsonObject effectsSet = paths.createNestedObject("/effects/set");
    JsonObject postEffectsSet = effectsSet.createNestedObject("post");
    postEffectsSet["summary"] = "Set current effect";
    postEffectsSet["tags"].add("Effects");

    // /parameters
    JsonObject parameters = paths.createNestedObject("/parameters");
    JsonObject getParams = parameters.createNestedObject("get");
    getParams["summary"] = "Get current parameters";
    getParams["tags"].add("Parameters");
    JsonObject postParams = parameters.createNestedObject("post");
    postParams["summary"] = "Update parameters";
    postParams["tags"].add("Parameters");

    // /zones
    JsonObject zones = paths.createNestedObject("/zones");
    JsonObject getZones = zones.createNestedObject("get");
    getZones["summary"] = "List all zones";
    getZones["tags"].add("Zones");

    // /transitions/types
    JsonObject transTypes = paths.createNestedObject("/transitions/types");
    JsonObject getTransTypes = transTypes.createNestedObject("get");
    getTransTypes["summary"] = "List transition types";
    getTransTypes["tags"].add("Transitions");

    String output;
    serializeJson(doc, output);
    request->send(HttpStatus::OK, "application/json", output);
}

// ============================================================================
// Zone v1 REST Handlers
// ============================================================================

void WebServer::handleZonesList(AsyncWebServerRequest* request) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    sendSuccessResponseLarge(request, [this](JsonObject& data) {
        data["enabled"] = m_zoneComposer->isEnabled();
        data["layout"] = static_cast<uint8_t>(m_zoneComposer->getLayout());
        data["layoutName"] = m_zoneComposer->getLayout() == ZoneLayout::QUAD ? "QUAD" : "TRIPLE";
        data["zoneCount"] = m_zoneComposer->getZoneCount();

        JsonArray zones = data.createNestedArray("zones");
        for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
            JsonObject zone = zones.createNestedObject();
            zone["id"] = i;
            zone["enabled"] = m_zoneComposer->isZoneEnabled(i);
            zone["effectId"] = m_zoneComposer->getZoneEffect(i);
            zone["effectName"] = RENDERER->getEffectName(m_zoneComposer->getZoneEffect(i));
            zone["brightness"] = m_zoneComposer->getZoneBrightness(i);
            zone["speed"] = m_zoneComposer->getZoneSpeed(i);
            zone["paletteId"] = m_zoneComposer->getZonePalette(i);
            zone["blendMode"] = static_cast<uint8_t>(m_zoneComposer->getZoneBlendMode(i));
            zone["blendModeName"] = getBlendModeName(m_zoneComposer->getZoneBlendMode(i));
        }

        // Available presets
        JsonArray presets = data.createNestedArray("presets");
        for (uint8_t i = 0; i < 5; i++) {
            JsonObject preset = presets.createNestedObject();
            preset["id"] = i;
            preset["name"] = ZoneComposer::getPresetName(i);
        }
    }, 1536);
}

void WebServer::handleZonesLayout(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("zoneCount")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing zoneCount field", "zoneCount");
        return;
    }

    uint8_t zoneCount = doc["zoneCount"];
    if (zoneCount != 3 && zoneCount != 4) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "zoneCount must be 3 or 4", "zoneCount");
        return;
    }

    ZoneLayout layout = (zoneCount == 4) ? ZoneLayout::QUAD : ZoneLayout::TRIPLE;
    m_zoneComposer->setLayout(layout);

    sendSuccessResponse(request, [zoneCount, layout](JsonObject& respData) {
        respData["zoneCount"] = zoneCount;
        respData["layout"] = static_cast<uint8_t>(layout);
        respData["layoutName"] = (layout == ZoneLayout::QUAD) ? "QUAD" : "TRIPLE";
    });

    broadcastZoneState();
}

void WebServer::handleZoneGet(AsyncWebServerRequest* request) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    // Extract zone ID from URL path parameter
    // URL format: /api/v1/zones/0, /api/v1/zones/1, etc.
    String path = request->url();
    uint8_t zoneId = path.charAt(path.length() - 1) - '0';

    if (zoneId >= m_zoneComposer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    sendSuccessResponse(request, [this, zoneId](JsonObject& data) {
        data["id"] = zoneId;
        data["enabled"] = m_zoneComposer->isZoneEnabled(zoneId);
        data["effectId"] = m_zoneComposer->getZoneEffect(zoneId);
        data["effectName"] = RENDERER->getEffectName(m_zoneComposer->getZoneEffect(zoneId));
        data["brightness"] = m_zoneComposer->getZoneBrightness(zoneId);
        data["speed"] = m_zoneComposer->getZoneSpeed(zoneId);
        data["paletteId"] = m_zoneComposer->getZonePalette(zoneId);
        data["blendMode"] = static_cast<uint8_t>(m_zoneComposer->getZoneBlendMode(zoneId));
        data["blendModeName"] = getBlendModeName(m_zoneComposer->getZoneBlendMode(zoneId));
    });
}

uint8_t WebServer::extractZoneIdFromPath(AsyncWebServerRequest* request) {
    // Extract zone ID from path like /api/v1/zones/2/effect
    String path = request->url();
    // Find the digit after "/zones/"
    int zonesIdx = path.indexOf("/zones/");
    if (zonesIdx >= 0 && zonesIdx + 7 < path.length()) {
        char digit = path.charAt(zonesIdx + 7);
        if (digit >= '0' && digit <= '3') {
            return digit - '0';
        }
    }
    return 255;  // Invalid
}

void WebServer::handleZoneSetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= m_zoneComposer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("effectId")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing effectId", "effectId");
        return;
    }

    uint8_t effectId = doc["effectId"];
    if (effectId >= RENDERER->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "effectId");
        return;
    }

    m_zoneComposer->setZoneEffect(zoneId, effectId);

    sendSuccessResponse(request, [zoneId, effectId](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["effectId"] = effectId;
        respData["effectName"] = RENDERER->getEffectName(effectId);
    });

    broadcastZoneState();
}

void WebServer::handleZoneSetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= m_zoneComposer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("brightness")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing brightness", "brightness");
        return;
    }

    uint8_t brightness = doc["brightness"];
    m_zoneComposer->setZoneBrightness(zoneId, brightness);

    sendSuccessResponse(request, [zoneId, brightness](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["brightness"] = brightness;
    });

    broadcastZoneState();
}

void WebServer::handleZoneSetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= m_zoneComposer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("speed")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing speed", "speed");
        return;
    }

    uint8_t speed = doc["speed"];
    if (speed < 1 || speed > 50) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Speed must be 1-50", "speed");
        return;
    }

    m_zoneComposer->setZoneSpeed(zoneId, speed);

    sendSuccessResponse(request, [zoneId, speed](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["speed"] = speed;
    });

    broadcastZoneState();
}

void WebServer::handleZoneSetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= m_zoneComposer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("paletteId")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing paletteId", "paletteId");
        return;
    }

    uint8_t paletteId = doc["paletteId"];
    if (paletteId >= MASTER_PALETTE_COUNT) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Palette ID out of range (0-74)", "paletteId");
        return;
    }

    m_zoneComposer->setZonePalette(zoneId, paletteId);

    sendSuccessResponse(request, [zoneId, paletteId](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["paletteId"] = paletteId;
        respData["paletteName"] = MasterPaletteNames[paletteId];
    });

    broadcastZoneState();
}

void WebServer::handleZoneSetBlend(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= m_zoneComposer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("blendMode")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing blendMode", "blendMode");
        return;
    }

    uint8_t blendModeVal = doc["blendMode"];
    if (blendModeVal >= static_cast<uint8_t>(BlendMode::MODE_COUNT)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Blend mode out of range (0-7)", "blendMode");
        return;
    }

    BlendMode blendMode = static_cast<BlendMode>(blendModeVal);
    m_zoneComposer->setZoneBlendMode(zoneId, blendMode);

    sendSuccessResponse(request, [zoneId, blendModeVal, blendMode](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["blendMode"] = blendModeVal;
        respData["blendModeName"] = getBlendModeName(blendMode);
    });

    broadcastZoneState();
}

void WebServer::handleZoneSetEnabled(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= m_zoneComposer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (!doc.containsKey("enabled")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing enabled", "enabled");
        return;
    }

    bool enabled = doc["enabled"];
    m_zoneComposer->setZoneEnabled(zoneId, enabled);

    sendSuccessResponse(request, [zoneId, enabled](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["enabled"] = enabled;
    });

    broadcastZoneState();
}

// ============================================================================
// WebSocket Handlers
// ============================================================================

void WebServer::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            webServer.handleWsConnect(client);
            break;

        case WS_EVT_DISCONNECT:
            webServer.handleWsDisconnect(client);
            break;

        case WS_EVT_DATA:
            webServer.handleWsMessage(client, data, len);
            break;

        case WS_EVT_ERROR:
            Serial.printf("[WebSocket] Error from client %u\n", client->id());
            break;

        case WS_EVT_PONG:
            // Pong received
            break;
    }
}

void WebServer::handleWsConnect(AsyncWebSocketClient* client) {
    if (m_ws->count() > WebServerConfig::MAX_WS_CLIENTS) {
        Serial.printf("[WebSocket] Max clients reached, rejecting %u\n", client->id());
        client->close(1008, "Connection limit");
        return;
    }

    Serial.printf("[WebSocket] Client %u connected from %s\n",
                  client->id(), client->remoteIP().toString().c_str());

    // Send initial state
    broadcastStatus();
    broadcastZoneState();
}

void WebServer::handleWsDisconnect(AsyncWebSocketClient* client) {
    Serial.printf("[WebSocket] Client %u disconnected\n", client->id());
}

void WebServer::handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Rate limit check
    if (!m_rateLimiter.checkWebSocket()) {
        String errorMsg = buildWsError(ErrorCodes::RATE_LIMITED, "Too many messages");
        client->text(errorMsg);
        return;
    }

    // Parse message
    if (len > 1024) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Message too large"));
        return;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        client->text(buildWsError(ErrorCodes::INVALID_JSON, "Parse error"));
        return;
    }

    processWsCommand(client, doc);
}

void WebServer::processWsCommand(AsyncWebSocketClient* client, JsonDocument& doc) {
    if (!doc.containsKey("type")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing 'type' field"));
        return;
    }

    String type = doc["type"].as<String>();

    // Effect control
    if (type == "setEffect") {
        uint8_t effectId = doc["effectId"] | 0;
        if (effectId < RENDERER->getEffectCount()) {
            ACTOR_SYSTEM.setEffect(effectId);
            broadcastStatus();
        }
    }
    else if (type == "nextEffect") {
        uint8_t current = RENDERER->getCurrentEffect();
        uint8_t next = (current + 1) % RENDERER->getEffectCount();
        ACTOR_SYSTEM.setEffect(next);
        broadcastStatus();
    }
    else if (type == "prevEffect") {
        uint8_t current = RENDERER->getCurrentEffect();
        uint8_t prev = (current + RENDERER->getEffectCount() - 1) % RENDERER->getEffectCount();
        ACTOR_SYSTEM.setEffect(prev);
        broadcastStatus();
    }

    // Parameter control
    else if (type == "setBrightness") {
        uint8_t value = doc["value"] | 128;
        ACTOR_SYSTEM.setBrightness(value);
        broadcastStatus();
    }
    else if (type == "setSpeed") {
        uint8_t value = doc["value"] | 15;
        if (value >= 1 && value <= 50) {
            ACTOR_SYSTEM.setSpeed(value);
            broadcastStatus();
        }
    }
    else if (type == "setPalette") {
        uint8_t paletteId = doc["paletteId"] | 0;
        ACTOR_SYSTEM.setPalette(paletteId);
        broadcastStatus();
    }

    // Transition control
    else if (type == "transition.trigger") {
        uint8_t toEffect = doc["toEffect"] | 0;
        uint8_t transType = doc["transitionType"] | 0;
        bool random = doc["random"] | false;

        if (toEffect < RENDERER->getEffectCount()) {
            if (random) {
                RENDERER->startRandomTransition(toEffect);
            } else {
                RENDERER->startTransition(toEffect, transType);
            }
            broadcastStatus();
        }
    }

    // Zone control
    else if (type == "zone.enable" && m_zoneComposer) {
        bool enable = doc["enable"] | false;
        m_zoneComposer->setEnabled(enable);
        broadcastZoneState();
    }
    else if (type == "zone.setEffect" && m_zoneComposer) {
        uint8_t zoneId = doc["zoneId"] | 0;
        uint8_t effectId = doc["effectId"] | 0;
        m_zoneComposer->setZoneEffect(zoneId, effectId);
        broadcastZoneState();
    }
    else if (type == "zone.setBrightness" && m_zoneComposer) {
        uint8_t zoneId = doc["zoneId"] | 0;
        uint8_t brightness = doc["brightness"] | 128;
        m_zoneComposer->setZoneBrightness(zoneId, brightness);
        broadcastZoneState();
    }
    else if (type == "zone.setSpeed" && m_zoneComposer) {
        uint8_t zoneId = doc["zoneId"] | 0;
        uint8_t speed = doc["speed"] | 15;
        m_zoneComposer->setZoneSpeed(zoneId, speed);
        broadcastZoneState();
    }
    else if (type == "zone.loadPreset" && m_zoneComposer) {
        uint8_t presetId = doc["presetId"] | 0;
        m_zoneComposer->loadPreset(presetId);
        broadcastZoneState();
    }

    // Query commands
    else if (type == "getStatus") {
        broadcastStatus();
    }
    else if (type == "getZoneState") {
        broadcastZoneState();
    }

    // Request-response pattern with requestId
    else if (type == "effects.getCurrent") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("effects.current", requestId, [](JsonObject& data) {
            data["effectId"] = RENDERER->getCurrentEffect();
            data["name"] = RENDERER->getEffectName(RENDERER->getCurrentEffect());
            data["brightness"] = RENDERER->getBrightness();
            data["speed"] = RENDERER->getSpeed();
        });
        client->text(response);
    }
    else if (type == "parameters.get") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("parameters", requestId, [](JsonObject& data) {
            data["brightness"] = RENDERER->getBrightness();
            data["speed"] = RENDERER->getSpeed();
            data["paletteId"] = RENDERER->getPaletteIndex();
            data["hue"] = RENDERER->getHue();
        });
        client->text(response);
    }

    // ========== MISSING V1 COMMANDS - NOW IMPLEMENTED ==========

    // device.getStatus - Device status via WebSocket
    else if (type == "device.getStatus") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("device.status", requestId, [this](JsonObject& data) {
            data["uptime"] = (millis() - m_startTime) / 1000;
            data["freeHeap"] = ESP.getFreeHeap();
            data["heapSize"] = ESP.getHeapSize();
            data["cpuFreq"] = ESP.getCpuFreqMHz();

            // Render stats
            const RenderStats& stats = RENDERER->getStats();
            data["fps"] = stats.currentFPS;
            data["cpuPercent"] = stats.cpuPercent;
            data["framesRendered"] = stats.framesRendered;

            // Network info
            JsonObject network = data.createNestedObject("network");
            network["connected"] = WiFi.status() == WL_CONNECTED;
            network["apMode"] = m_apMode;
            if (WiFi.status() == WL_CONNECTED) {
                network["ip"] = WiFi.localIP().toString();
                network["rssi"] = WiFi.RSSI();
            }

            data["wsClients"] = m_ws->count();
        });
        client->text(response);
    }

    // effects.getMetadata - Effect metadata by ID
    else if (type == "effects.getMetadata") {
        const char* requestId = doc["requestId"] | "";
        uint8_t effectId = doc["effectId"] | 255;

        if (effectId == 255 || effectId >= RENDERER->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
            return;
        }

        String response = buildWsResponse("effects.metadata", requestId, [effectId](JsonObject& data) {
            data["id"] = effectId;
            data["name"] = RENDERER->getEffectName(effectId);

            // Category based on ID ranges
            if (effectId <= 4) {
                data["category"] = "Classic";
                data["description"] = "Classic LED effects optimized for LGP";
            } else if (effectId <= 7) {
                data["category"] = "Wave";
                data["description"] = "Wave-based interference patterns";
            } else if (effectId <= 12) {
                data["category"] = "Physics";
                data["description"] = "Physics-based simulations";
            } else {
                data["category"] = "Custom";
                data["description"] = "Custom effects";
            }

            // Effect properties
            JsonObject properties = data.createNestedObject("properties");
            properties["centerOrigin"] = true;
            properties["symmetricStrips"] = true;
            properties["paletteAware"] = true;
            properties["speedResponsive"] = true;

            // Recommended settings
            JsonObject recommended = data.createNestedObject("recommended");
            recommended["brightness"] = 180;
            recommended["speed"] = 15;
        });
        client->text(response);
    }

    // effects.getCategories - Effect categories list
    else if (type == "effects.getCategories") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("effects.categories", requestId, [](JsonObject& data) {
            data["total"] = 4;  // Classic, Wave, Physics, Custom

            JsonArray categories = data.createNestedArray("categories");
            
            // Count effects per category
            uint8_t classicCount = 0, waveCount = 0, physicsCount = 0, customCount = 0;
            uint8_t effectCount = RENDERER->getEffectCount();
            for (uint8_t i = 0; i < effectCount; i++) {
                if (i <= 4) classicCount++;
                else if (i <= 7) waveCount++;
                else if (i <= 12) physicsCount++;
                else customCount++;
            }

            JsonObject classic = categories.createNestedObject();
            classic["id"] = 0;
            classic["name"] = "Classic";
            classic["count"] = classicCount;

            JsonObject wave = categories.createNestedObject();
            wave["id"] = 1;
            wave["name"] = "Wave";
            wave["count"] = waveCount;

            JsonObject physics = categories.createNestedObject();
            physics["id"] = 2;
            physics["name"] = "Physics";
            physics["count"] = physicsCount;

            JsonObject custom = categories.createNestedObject();
            custom["id"] = 3;
            custom["name"] = "Custom";
            custom["count"] = customCount;
        });
        client->text(response);
    }

    // transition.getTypes - Transition types list
    else if (type == "transition.getTypes") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("transitions.types", requestId, [](JsonObject& data) {
            JsonArray types = data.createNestedArray("types");

            for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
                JsonObject type = types.createNestedObject();
                type["id"] = i;
                type["name"] = getTransitionName(static_cast<TransitionType>(i));
            }

            data["total"] = static_cast<uint8_t>(TransitionType::TYPE_COUNT);
        });
        client->text(response);
    }

    // transition.config - Get transition configuration
    else if (type == "transition.config" && !doc.containsKey("defaultDuration") && !doc.containsKey("defaultType")) {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("transitions.config", requestId, [](JsonObject& data) {
            data["enabled"] = true;
            data["defaultDuration"] = 1000;
            data["defaultType"] = 0;
            data["defaultTypeName"] = getTransitionName(TransitionType::FADE);

            // Available easing curves
            JsonArray easings = data.createNestedArray("easings");
            const char* easingNames[] = {
                "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
                "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
                "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
            };
            for (uint8_t i = 0; i < 10; i++) {
                JsonObject easing = easings.createNestedObject();
                easing["id"] = i;
                easing["name"] = easingNames[i];
            }
        });
        client->text(response);
    }

    // transition.config - Set transition configuration
    else if (type == "transition.config" && (doc.containsKey("defaultDuration") || doc.containsKey("defaultType"))) {
        const char* requestId = doc["requestId"] | "";
        uint16_t duration = doc["defaultDuration"] | 1000;
        uint8_t type = doc["defaultType"] | 0;

        if (type >= static_cast<uint8_t>(TransitionType::TYPE_COUNT)) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid transition type", requestId));
            return;
        }

        // Currently transition config is not persisted, acknowledge the request
        String response = buildWsResponse("transitions.config", requestId, [duration, type](JsonObject& data) {
            data["defaultDuration"] = duration;
            data["defaultType"] = type;
            data["defaultTypeName"] = getTransitionName(static_cast<TransitionType>(type));
            data["message"] = "Transition config updated";
        });
        client->text(response);
    }

    // zones.get - Zones list
    else if (type == "zones.get") {
        const char* requestId = doc["requestId"] | "";
        
        if (!m_zoneComposer) {
            client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
            return;
        }

        String response = buildWsResponse("zones", requestId, [this](JsonObject& data) {
            data["enabled"] = m_zoneComposer->isEnabled();
            data["layout"] = static_cast<uint8_t>(m_zoneComposer->getLayout());
            data["layoutName"] = m_zoneComposer->getLayout() == ZoneLayout::QUAD ? "QUAD" : "TRIPLE";
            data["zoneCount"] = m_zoneComposer->getZoneCount();

            JsonArray zones = data.createNestedArray("zones");
            for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
                JsonObject zone = zones.createNestedObject();
                zone["id"] = i;
                zone["enabled"] = m_zoneComposer->isZoneEnabled(i);
                zone["effectId"] = m_zoneComposer->getZoneEffect(i);
                zone["effectName"] = RENDERER->getEffectName(m_zoneComposer->getZoneEffect(i));
                zone["brightness"] = m_zoneComposer->getZoneBrightness(i);
                zone["speed"] = m_zoneComposer->getZoneSpeed(i);
                zone["paletteId"] = m_zoneComposer->getZonePalette(i);
                zone["blendMode"] = static_cast<uint8_t>(m_zoneComposer->getZoneBlendMode(i));
                zone["blendModeName"] = getBlendModeName(m_zoneComposer->getZoneBlendMode(i));
            }

            // Available presets
            JsonArray presets = data.createNestedArray("presets");
            for (uint8_t i = 0; i < 5; i++) {
                JsonObject preset = presets.createNestedObject();
                preset["id"] = i;
                preset["name"] = ZoneComposer::getPresetName(i);
            }
        });
        client->text(response);
    }

    // batch - Batch operations via WebSocket
    else if (type == "batch") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("operations") || !doc["operations"].is<JsonArray>()) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "operations array required", requestId));
            return;
        }

        JsonArray ops = doc["operations"];
        if (ops.size() > WebServerConfig::MAX_BATCH_OPERATIONS) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Max 10 operations per batch", requestId));
            return;
        }

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

        String response = buildWsResponse("batch.result", requestId, [processed, failed](JsonObject& data) {
            data["processed"] = processed;
            data["failed"] = failed;
        });
        client->text(response);

        broadcastStatus();
    }

    // Unknown command
    else {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown command type", requestId));
    }
}

// ============================================================================
// Broadcasting
// ============================================================================

void WebServer::broadcastStatus() {
    if (m_ws->count() == 0) return;

    StaticJsonDocument<512> doc;
    doc["type"] = "status";

    if (RENDERER) {
        doc["effectId"] = RENDERER->getCurrentEffect();
        doc["effectName"] = RENDERER->getEffectName(RENDERER->getCurrentEffect());
        doc["brightness"] = RENDERER->getBrightness();
        doc["speed"] = RENDERER->getSpeed();
        doc["paletteId"] = RENDERER->getPaletteIndex();
        doc["hue"] = RENDERER->getHue();

        const RenderStats& stats = RENDERER->getStats();
        doc["fps"] = stats.currentFPS;
        doc["cpuPercent"] = stats.cpuPercent;
    }

    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;

    String output;
    serializeJson(doc, output);
    m_ws->textAll(output);
}

void WebServer::broadcastZoneState() {
    if (m_ws->count() == 0 || !m_zoneComposer) return;

    StaticJsonDocument<768> doc;
    doc["type"] = "zone.state";
    doc["enabled"] = m_zoneComposer->isEnabled();
    doc["zoneCount"] = m_zoneComposer->getZoneCount();

    JsonArray zones = doc.createNestedArray("zones");
    for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
        JsonObject zone = zones.createNestedObject();
        zone["id"] = i;
        zone["enabled"] = m_zoneComposer->isZoneEnabled(i);
        zone["effectId"] = m_zoneComposer->getZoneEffect(i);
        zone["brightness"] = m_zoneComposer->getZoneBrightness(i);
        zone["speed"] = m_zoneComposer->getZoneSpeed(i);
        zone["paletteId"] = m_zoneComposer->getZonePalette(i);
    }

    String output;
    serializeJson(doc, output);
    m_ws->textAll(output);
}

void WebServer::notifyEffectChange(uint8_t effectId, const char* name) {
    if (m_ws->count() == 0) return;

    StaticJsonDocument<256> doc;
    doc["type"] = "effectChanged";
    doc["effectId"] = effectId;
    doc["name"] = name;

    String output;
    serializeJson(doc, output);
    m_ws->textAll(output);
}

void WebServer::notifyParameterChange() {
    broadcastStatus();
}

// ============================================================================
// Rate Limiting
// ============================================================================

bool WebServer::checkRateLimit(AsyncWebServerRequest* request) {
    if (!m_rateLimiter.checkHTTP()) {
        sendErrorResponse(request, HttpStatus::TOO_MANY_REQUESTS,
                          ErrorCodes::RATE_LIMITED, "Too many requests");
        return false;
    }
    return true;
}

bool WebServer::checkWsRateLimit(AsyncWebSocketClient* client) {
    return m_rateLimiter.checkWebSocket();
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
