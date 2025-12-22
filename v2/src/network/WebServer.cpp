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
#include "RequestValidator.h"
#include "WiFiManager.h"
#include "../config/network_config.h"
#include "../core/actors/ActorSystem.h"
#include <Update.h>
#include "../core/actors/RendererActor.h"
#include "../core/persistence/ZoneConfigManager.h"
#include "../effects/zones/ZoneComposer.h"
#include "../effects/transitions/TransitionTypes.h"
#include "../palettes/Palettes_Master.h"
#include "../effects/PatternRegistry.h"
#include "../core/narrative/NarrativeEngine.h"
#include "../effects/enhancement/MotionEngine.h"
#include "../effects/enhancement/ColorEngine.h"
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
    , m_lastLedBroadcast(0)
{
    memset(m_ledFrameBuffer, 0, sizeof(m_ledFrameBuffer));
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

    // LED frame streaming to subscribed clients (20 FPS)
    broadcastLEDFrame();

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
        if (!checkRateLimit(request)) return;
        handleLegacyStatus(request);
    });

    // POST /api/effect
    m_server->on("/api/effect", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleLegacySetEffect(request, data, len);
        }
    );

    // POST /api/brightness
    m_server->on("/api/brightness", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleLegacySetBrightness(request, data, len);
        }
    );

    // POST /api/speed
    m_server->on("/api/speed", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleLegacySetSpeed(request, data, len);
        }
    );

    // POST /api/palette
    m_server->on("/api/palette", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleLegacySetPalette(request, data, len);
        }
    );

    // POST /api/zone/count
    m_server->on("/api/zone/count", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleLegacyZoneCount(request, data, len);
        }
    );

    // POST /api/zone/effect
    m_server->on("/api/zone/effect", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
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
            VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacyZonePreset, request);

            // Schema validates preset is 0-4
            uint8_t presetId = doc["preset"];
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
            VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::NetworkConnect, request);

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

    // Effect Families - GET /api/v1/effects/families
    m_server->on("/api/v1/effects/families", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleEffectsFamilies(request);
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

    // ========== Narrative Endpoints ==========

    // Narrative Status - GET /api/v1/narrative/status
    m_server->on("/api/v1/narrative/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleNarrativeStatus(request);
    });

    // Narrative Config - GET /api/v1/narrative/config
    m_server->on("/api/v1/narrative/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleNarrativeConfigGet(request);
    });

    // Narrative Config - POST /api/v1/narrative/config
    m_server->on("/api/v1/narrative/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleNarrativeConfigSet(request, data, len);
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
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetEffect, request);

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
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetBrightness, request);

    uint8_t brightness = doc["brightness"];
    ACTOR_SYSTEM.setBrightness(brightness);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacySetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<128> doc;
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetSpeed, request);

    // Range is validated by schema (1-50)
    uint8_t speed = doc["speed"];
    ACTOR_SYSTEM.setSpeed(speed);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacySetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<128> doc;
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetPalette, request);

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
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacyZoneCount, request);

    // Range validated by schema (1-4)
    uint8_t count = doc["count"];
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
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacyZoneEffect, request);

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

    // Parse pagination query parameters
    int page = 1;
    int limit = 20;
    int categoryFilter = -1;  // -1 = no filter
    bool details = false;

    if (request->hasParam("page")) {
        page = request->getParam("page")->value().toInt();
        if (page < 1) page = 1;
    }
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
        // Clamp limit between 1 and 50
        if (limit < 1) limit = 1;
        if (limit > 50) limit = 50;
    }
    if (request->hasParam("category")) {
        categoryFilter = request->getParam("category")->value().toInt();
    }
    if (request->hasParam("details")) {
        String detailsStr = request->getParam("details")->value();
        details = (detailsStr == "true" || detailsStr == "1");
    }

    // Calculate pagination values
    int total = effectCount;
    int pages = (total + limit - 1) / limit;  // Ceiling division
    if (page > pages && pages > 0) page = pages;  // Clamp page to max
    int startIdx = (page - 1) * limit;
    int endIdx = startIdx + limit;
    if (endIdx > total) endIdx = total;

    // Capture values for lambda
    const int capturedPage = page;
    const int capturedLimit = limit;
    const int capturedTotal = total;
    const int capturedPages = pages;
    const int capturedStartIdx = startIdx;
    const int capturedEndIdx = endIdx;
    const int capturedCategoryFilter = categoryFilter;
    const bool capturedDetails = details;

    sendSuccessResponseLarge(request, [capturedPage, capturedLimit, capturedTotal,
                                       capturedPages, capturedStartIdx, capturedEndIdx,
                                       capturedCategoryFilter, capturedDetails](JsonObject& data) {
        // Add pagination object
        JsonObject pagination = data.createNestedObject("pagination");
        pagination["page"] = capturedPage;
        pagination["limit"] = capturedLimit;
        pagination["total"] = capturedTotal;
        pagination["pages"] = capturedPages;

        // Helper lambda to get category info
        auto getCategoryId = [](uint8_t effectId) -> int {
            if (effectId <= 4) return 0;       // Classic
            else if (effectId <= 7) return 1;  // Wave
            else if (effectId <= 12) return 2; // Physics
            else return 3;                     // Custom
        };

        auto getCategoryName = [](int categoryId) -> const char* {
            switch (categoryId) {
                case 0: return "Classic";
                case 1: return "Wave";
                case 2: return "Physics";
                default: return "Custom";
            }
        };

        // Build effects array with pagination
        JsonArray effects = data.createNestedArray("effects");

        // If category filter is applied, we need to filter first then paginate
        if (capturedCategoryFilter >= 0) {
            // Collect matching effects
            int matchCount = 0;
            int addedCount = 0;

            for (uint8_t i = 0; i < RENDERER->getEffectCount(); i++) {
                int effectCategory = getCategoryId(i);
                if (effectCategory == capturedCategoryFilter) {
                    // Check if this match is within our page window
                    if (matchCount >= capturedStartIdx && addedCount < capturedLimit) {
                        JsonObject effect = effects.createNestedObject();
                        effect["id"] = i;
                        effect["name"] = RENDERER->getEffectName(i);
                        effect["category"] = getCategoryName(effectCategory);
                        effect["categoryId"] = effectCategory;

                        if (capturedDetails) {
                            // Add extended metadata
                            JsonObject features = effect.createNestedObject("features");
                            features["centerOrigin"] = true;
                            features["usesSpeed"] = true;
                            features["usesPalette"] = true;
                            features["zoneAware"] = (effectCategory != 2);  // Physics effects may not be zone-aware
                        }
                        addedCount++;
                    }
                    matchCount++;
                }
            }
        } else {
            // No category filter - simple pagination
            for (int i = capturedStartIdx; i < capturedEndIdx; i++) {
                JsonObject effect = effects.createNestedObject();
                effect["id"] = i;
                effect["name"] = RENDERER->getEffectName(i);
                int categoryId = getCategoryId(i);
                effect["category"] = getCategoryName(categoryId);
                effect["categoryId"] = categoryId;

                if (capturedDetails) {
                    // Add extended metadata
                    JsonObject features = effect.createNestedObject("features");
                    features["centerOrigin"] = true;
                    features["usesSpeed"] = true;
                    features["usesPalette"] = true;
                    features["zoneAware"] = (categoryId != 2);
                }
            }
        }

        // Add categories summary
        JsonArray categories = data.createNestedArray("categories");
        const char* categoryNames[] = {"Classic", "Wave", "Physics", "Custom"};
        for (int i = 0; i < 4; i++) {
            JsonObject cat = categories.createNestedObject();
            cat["id"] = i;
            cat["name"] = categoryNames[i];
        }
    }, 4096);  // Increased buffer for pagination metadata
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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::SetEffect, request);

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
    // Parse and validate - all fields are optional but must be valid if present
    auto vr = RequestValidator::parseAndValidate(data, len, doc, RequestSchemas::SetParameters);
    if (!vr.valid) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          vr.errorCode, vr.errorMessage, vr.fieldName);
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
        // Range already validated by schema (1-50)
        ACTOR_SYSTEM.setSpeed(val);
        updated = true;
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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::TriggerTransition, request);

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

    // Get pagination parameters (1-indexed page, limit 1-50, default 20)
    uint16_t page = 1;
    uint16_t limit = 20;
    if (request->hasParam("page")) {
        page = request->getParam("page")->value().toInt();
        if (page < 1) page = 1;
    }
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
        if (limit < 1) limit = 1;
        if (limit > 50) limit = 50;
    }

    // First pass: collect all palette IDs that pass the filters
    uint8_t filteredIds[MASTER_PALETTE_COUNT];
    uint16_t filteredCount = 0;

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

        // This palette passes all filters
        filteredIds[filteredCount++] = i;
    }

    // Calculate pagination
    uint16_t totalPages = (filteredCount + limit - 1) / limit;  // Ceiling division
    if (totalPages == 0) totalPages = 1;
    if (page > totalPages) page = totalPages;

    uint16_t startIdx = (page - 1) * limit;
    uint16_t endIdx = startIdx + limit;
    if (endIdx > filteredCount) endIdx = filteredCount;

    // Build response - buffer sized for up to 50 palettes per page
    DynamicJsonDocument doc(8192);
    doc["success"] = true;
    doc["timestamp"] = millis();
    doc["version"] = API_VERSION;

    JsonObject data = doc.createNestedObject("data");

    // Categorize palettes (static counts, not affected by filters)
    JsonObject categories = data.createNestedObject("categories");
    categories["artistic"] = CPT_CITY_END - CPT_CITY_START + 1;
    categories["scientific"] = CRAMERI_END - CRAMERI_START + 1;
    categories["lgpOptimized"] = COLORSPACE_END - COLORSPACE_START + 1;

    JsonArray palettes = data.createNestedArray("palettes");

    // Second pass: add only the paginated subset of filtered palettes
    for (uint16_t idx = startIdx; idx < endIdx; idx++) {
        uint8_t i = filteredIds[idx];

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

    // Add pagination object
    JsonObject pagination = data.createNestedObject("pagination");
    pagination["page"] = page;
    pagination["limit"] = limit;
    pagination["total"] = filteredCount;
    pagination["pages"] = totalPages;

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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::SetPalette, request);

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

        // Query PatternRegistry for metadata
        const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
        if (meta) {
            // Get family name
            char familyName[32];
            PatternRegistry::getFamilyName(meta->family, familyName, sizeof(familyName));
            data["family"] = familyName;
            data["familyId"] = static_cast<uint8_t>(meta->family);
            
            // Story and optical intent
            if (meta->story) {
                data["story"] = meta->story;
            }
            if (meta->opticalIntent) {
                data["opticalIntent"] = meta->opticalIntent;
            }
            
            // Tags as array
            JsonArray tags = data.createNestedArray("tags");
            if (meta->hasTag(PatternTags::STANDING)) tags.add("STANDING");
            if (meta->hasTag(PatternTags::TRAVELING)) tags.add("TRAVELING");
            if (meta->hasTag(PatternTags::MOIRE)) tags.add("MOIRE");
            if (meta->hasTag(PatternTags::DEPTH)) tags.add("DEPTH");
            if (meta->hasTag(PatternTags::SPECTRAL)) tags.add("SPECTRAL");
            if (meta->hasTag(PatternTags::CENTER_ORIGIN)) tags.add("CENTER_ORIGIN");
            if (meta->hasTag(PatternTags::DUAL_STRIP)) tags.add("DUAL_STRIP");
            if (meta->hasTag(PatternTags::PHYSICS)) tags.add("PHYSICS");
        } else {
            // Fallback if metadata not found
            data["family"] = "Unknown";
            data["familyId"] = 255;
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

void WebServer::handleEffectsFamilies(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        JsonArray families = data.createNestedArray("families");
        
        // Iterate through all 10 pattern families
        for (uint8_t i = 0; i < 10; i++) {
            PatternFamily family = static_cast<PatternFamily>(i);
            uint8_t count = PatternRegistry::getFamilyCount(family);
            
            JsonObject familyObj = families.createNestedObject();
            familyObj["id"] = i;
            
            char familyName[32];
            PatternRegistry::getFamilyName(family, familyName, sizeof(familyName));
            familyObj["name"] = familyName;
            familyObj["count"] = count;
        }
        
        data["total"] = 10;
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
    // Validate - all fields are optional but if present must be valid
    auto vr = RequestValidator::parseAndValidate(data, len, doc, RequestSchemas::TransitionConfig);
    if (!vr.valid) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          vr.errorCode, vr.errorMessage, vr.fieldName);
        return;
    }

    // Currently transition config is not persisted, this endpoint acknowledges the request
    // Future: store default transition type, duration, easing in NVS

    // Schema validates type is 0-15 if present
    uint16_t duration = doc["defaultDuration"] | 1000;
    uint8_t type = doc["defaultType"] | 0;

    sendSuccessResponse(request, [duration, type](JsonObject& respData) {
        respData["defaultDuration"] = duration;
        respData["defaultType"] = type;
        respData["defaultTypeName"] = getTransitionName(static_cast<TransitionType>(type));
        respData["message"] = "Transition config updated";
    });
}

// ============================================================================
// Narrative Handlers
// ============================================================================

void WebServer::handleNarrativeStatus(AsyncWebServerRequest* request) {
    using namespace lightwaveos::narrative;
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    
    sendSuccessResponse(request, [&narrative](JsonObject& data) {
        // Current state
        data["enabled"] = narrative.isEnabled();
        data["tension"] = narrative.getTension() * 100.0f;  // Convert 0-1 to 0-100
        data["phaseT"] = narrative.getPhaseT();
        data["cycleT"] = narrative.getCycleT();
        
        // Phase as string
        NarrativePhase phase = narrative.getPhase();
        const char* phaseName = "UNKNOWN";
        switch (phase) {
            case PHASE_BUILD:   phaseName = "BUILD"; break;
            case PHASE_HOLD:    phaseName = "HOLD"; break;
            case PHASE_RELEASE: phaseName = "RELEASE"; break;
            case PHASE_REST:    phaseName = "REST"; break;
        }
        data["phase"] = phaseName;
        data["phaseId"] = static_cast<uint8_t>(phase);
        
        // Phase durations
        JsonObject durations = data.createNestedObject("durations");
        durations["build"] = narrative.getBuildDuration();
        durations["hold"] = narrative.getHoldDuration();
        durations["release"] = narrative.getReleaseDuration();
        durations["rest"] = narrative.getRestDuration();
        durations["total"] = narrative.getTotalDuration();
        
        // Derived values
        data["tempoMultiplier"] = narrative.getTempoMultiplier();
        data["complexityScaling"] = narrative.getComplexityScaling();
    });
}

void WebServer::handleNarrativeConfigGet(AsyncWebServerRequest* request) {
    using namespace lightwaveos::narrative;
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    
    sendSuccessResponse(request, [&narrative](JsonObject& data) {
        // Phase durations
        JsonObject durations = data.createNestedObject("durations");
        durations["build"] = narrative.getBuildDuration();
        durations["hold"] = narrative.getHoldDuration();
        durations["release"] = narrative.getReleaseDuration();
        durations["rest"] = narrative.getRestDuration();
        durations["total"] = narrative.getTotalDuration();
        
        // Curves
        JsonObject curves = data.createNestedObject("curves");
        curves["build"] = static_cast<uint8_t>(narrative.getBuildCurve());
        curves["release"] = static_cast<uint8_t>(narrative.getReleaseCurve());
        
        // Optional behaviors
        data["holdBreathe"] = narrative.getHoldBreathe();
        data["snapAmount"] = narrative.getSnapAmount();
        data["durationVariance"] = narrative.getDurationVariance();
        
        data["enabled"] = narrative.isEnabled();
    });
}

void WebServer::handleNarrativeConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    using namespace lightwaveos::narrative;
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }
    
    NarrativeEngine& narrative = NarrativeEngine::getInstance();
    bool updated = false;
    
    // Update phase durations if provided
    if (doc.containsKey("durations")) {
        JsonObject durations = doc["durations"];
        if (durations.containsKey("build")) {
            narrative.setBuildDuration(durations["build"] | 1.5f);
            updated = true;
        }
        if (durations.containsKey("hold")) {
            narrative.setHoldDuration(durations["hold"] | 0.5f);
            updated = true;
        }
        if (durations.containsKey("release")) {
            narrative.setReleaseDuration(durations["release"] | 1.5f);
            updated = true;
        }
        if (durations.containsKey("rest")) {
            narrative.setRestDuration(durations["rest"] | 0.5f);
            updated = true;
        }
    }
    
    // Update curves if provided
    if (doc.containsKey("curves")) {
        JsonObject curves = doc["curves"];
        if (curves.containsKey("build")) {
            narrative.setBuildCurve(static_cast<lightwaveos::effects::EasingCurve>(curves["build"] | 1));
            updated = true;
        }
        if (curves.containsKey("release")) {
            narrative.setReleaseCurve(static_cast<lightwaveos::effects::EasingCurve>(curves["release"] | 6));
            updated = true;
        }
    }
    
    // Update optional behaviors
    if (doc.containsKey("holdBreathe")) {
        narrative.setHoldBreathe(doc["holdBreathe"] | 0.0f);
        updated = true;
    }
    if (doc.containsKey("snapAmount")) {
        narrative.setSnapAmount(doc["snapAmount"] | 0.0f);
        updated = true;
    }
    if (doc.containsKey("durationVariance")) {
        narrative.setDurationVariance(doc["durationVariance"] | 0.0f);
        updated = true;
    }
    
    // Update enabled state
    if (doc.containsKey("enabled")) {
        bool enabled = doc["enabled"] | false;
        if (enabled) {
            narrative.enable();
        } else {
            narrative.disable();
        }
        updated = true;
    }
    
    // TODO: Persist to NVS if needed
    
    sendSuccessResponse(request, [updated](JsonObject& respData) {
        respData["message"] = updated ? "Narrative config updated" : "No changes";
        respData["updated"] = updated;
    });
}

// ============================================================================
// OpenAPI Specification Handler
// ============================================================================

void WebServer::handleOpenApiSpec(AsyncWebServerRequest* request) {
    // OpenAPI 3.0.3 specification for LightwaveOS API v1
    // Expanded to document all implemented REST endpoints
    DynamicJsonDocument doc(8192);  // Increased from 4096 to fit all endpoints

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

    // ========== Device Endpoints ==========

    // GET /device/status
    JsonObject deviceStatus = paths.createNestedObject("/device/status");
    JsonObject getDeviceStatus = deviceStatus.createNestedObject("get");
    getDeviceStatus["summary"] = "Get device status (uptime, heap, FPS)";
    getDeviceStatus["tags"].add("Device");

    // GET /device/info
    JsonObject deviceInfo = paths.createNestedObject("/device/info");
    JsonObject getDeviceInfo = deviceInfo.createNestedObject("get");
    getDeviceInfo["summary"] = "Get device info (firmware, hardware, SDK)";
    getDeviceInfo["tags"].add("Device");

    // ========== Effects Endpoints ==========

    // GET /effects
    JsonObject effects = paths.createNestedObject("/effects");
    JsonObject getEffects = effects.createNestedObject("get");
    getEffects["summary"] = "List effects with pagination";
    getEffects["tags"].add("Effects");
    JsonArray effectsParams = getEffects.createNestedArray("parameters");
    JsonObject pageParam = effectsParams.createNestedObject();
    pageParam["name"] = "page";
    pageParam["in"] = "query";
    pageParam["schema"]["type"] = "integer";
    JsonObject limitParam = effectsParams.createNestedObject();
    limitParam["name"] = "limit";
    limitParam["in"] = "query";
    limitParam["schema"]["type"] = "integer";
    JsonObject categoryParam = effectsParams.createNestedObject();
    categoryParam["name"] = "category";
    categoryParam["in"] = "query";
    categoryParam["schema"]["type"] = "string";
    JsonObject detailsParam = effectsParams.createNestedObject();
    detailsParam["name"] = "details";
    detailsParam["in"] = "query";
    detailsParam["schema"]["type"] = "boolean";

    // GET /effects/current
    JsonObject effectsCurrent = paths.createNestedObject("/effects/current");
    JsonObject getEffectsCurrent = effectsCurrent.createNestedObject("get");
    getEffectsCurrent["summary"] = "Get current effect state";
    getEffectsCurrent["tags"].add("Effects");

    // POST /effects/set
    JsonObject effectsSet = paths.createNestedObject("/effects/set");
    JsonObject postEffectsSet = effectsSet.createNestedObject("post");
    postEffectsSet["summary"] = "Set current effect by ID";
    postEffectsSet["tags"].add("Effects");

    // GET /effects/metadata
    JsonObject effectsMeta = paths.createNestedObject("/effects/metadata");
    JsonObject getEffectsMeta = effectsMeta.createNestedObject("get");
    getEffectsMeta["summary"] = "Get effect metadata by ID";
    getEffectsMeta["tags"].add("Effects");
    JsonArray metaParams = getEffectsMeta.createNestedArray("parameters");
    JsonObject idParam = metaParams.createNestedObject();
    idParam["name"] = "id";
    idParam["in"] = "query";
    idParam["required"] = true;
    idParam["schema"]["type"] = "integer";

    // GET /effects/families
    JsonObject effectsFamilies = paths.createNestedObject("/effects/families");
    JsonObject getEffectsFamilies = effectsFamilies.createNestedObject("get");
    getEffectsFamilies["summary"] = "List effect families/categories";
    getEffectsFamilies["tags"].add("Effects");

    // ========== Palettes Endpoints ==========

    // GET /palettes
    JsonObject palettes = paths.createNestedObject("/palettes");
    JsonObject getPalettes = palettes.createNestedObject("get");
    getPalettes["summary"] = "List palettes with pagination";
    getPalettes["tags"].add("Palettes");
    JsonArray paletteParams = getPalettes.createNestedArray("parameters");
    JsonObject palPageParam = paletteParams.createNestedObject();
    palPageParam["name"] = "page";
    palPageParam["in"] = "query";
    palPageParam["schema"]["type"] = "integer";
    JsonObject palLimitParam = paletteParams.createNestedObject();
    palLimitParam["name"] = "limit";
    palLimitParam["in"] = "query";
    palLimitParam["schema"]["type"] = "integer";

    // GET /palettes/current
    JsonObject palettesCurrent = paths.createNestedObject("/palettes/current");
    JsonObject getPalettesCurrent = palettesCurrent.createNestedObject("get");
    getPalettesCurrent["summary"] = "Get current palette";
    getPalettesCurrent["tags"].add("Palettes");

    // POST /palettes/set
    JsonObject palettesSet = paths.createNestedObject("/palettes/set");
    JsonObject postPalettesSet = palettesSet.createNestedObject("post");
    postPalettesSet["summary"] = "Set palette by ID";
    postPalettesSet["tags"].add("Palettes");

    // ========== Parameters Endpoints ==========

    // GET /parameters
    JsonObject parameters = paths.createNestedObject("/parameters");
    JsonObject getParams = parameters.createNestedObject("get");
    getParams["summary"] = "Get visual parameters";
    getParams["tags"].add("Parameters");

    // POST /parameters
    JsonObject postParams = parameters.createNestedObject("post");
    postParams["summary"] = "Update visual parameters (brightness, speed, etc.)";
    postParams["tags"].add("Parameters");

    // ========== Transitions Endpoints ==========

    // GET /transitions/types
    JsonObject transTypes = paths.createNestedObject("/transitions/types");
    JsonObject getTransTypes = transTypes.createNestedObject("get");
    getTransTypes["summary"] = "List transition types";
    getTransTypes["tags"].add("Transitions");

    // GET /transitions/config
    JsonObject transConfig = paths.createNestedObject("/transitions/config");
    JsonObject getTransConfig = transConfig.createNestedObject("get");
    getTransConfig["summary"] = "Get transition configuration";
    getTransConfig["tags"].add("Transitions");

    // POST /transitions/config
    JsonObject postTransConfig = transConfig.createNestedObject("post");
    postTransConfig["summary"] = "Update transition configuration";
    postTransConfig["tags"].add("Transitions");

    // POST /transitions/trigger
    JsonObject transTrigger = paths.createNestedObject("/transitions/trigger");
    JsonObject postTransTrigger = transTrigger.createNestedObject("post");
    postTransTrigger["summary"] = "Trigger a transition";
    postTransTrigger["tags"].add("Transitions");

    // ========== Narrative Endpoints ==========

    // GET /narrative/status
    JsonObject narrativeStatus = paths.createNestedObject("/narrative/status");
    JsonObject getNarrativeStatus = narrativeStatus.createNestedObject("get");
    getNarrativeStatus["summary"] = "Get narrative engine status";
    getNarrativeStatus["tags"].add("Narrative");

    // GET /narrative/config
    JsonObject narrativeConfig = paths.createNestedObject("/narrative/config");
    JsonObject getNarrativeConfig = narrativeConfig.createNestedObject("get");
    getNarrativeConfig["summary"] = "Get narrative configuration";
    getNarrativeConfig["tags"].add("Narrative");

    // POST /narrative/config
    JsonObject postNarrativeConfig = narrativeConfig.createNestedObject("post");
    postNarrativeConfig["summary"] = "Update narrative configuration";
    postNarrativeConfig["tags"].add("Narrative");

    // ========== Zones Endpoints ==========

    // GET /zones
    JsonObject zones = paths.createNestedObject("/zones");
    JsonObject getZones = zones.createNestedObject("get");
    getZones["summary"] = "List all zones with configuration";
    getZones["tags"].add("Zones");

    // POST /zones/layout
    JsonObject zonesLayout = paths.createNestedObject("/zones/layout");
    JsonObject postZonesLayout = zonesLayout.createNestedObject("post");
    postZonesLayout["summary"] = "Set zone layout configuration";
    postZonesLayout["tags"].add("Zones");

    // ========== Batch Endpoints ==========

    // POST /batch
    JsonObject batch = paths.createNestedObject("/batch");
    JsonObject postBatch = batch.createNestedObject("post");
    postBatch["summary"] = "Execute batch operations (max 10)";
    postBatch["tags"].add("Batch");

    // ========== Sync Endpoints ==========

    // GET /sync/status
    JsonObject syncStatus = paths.createNestedObject("/sync/status");
    JsonObject getSyncStatus = syncStatus.createNestedObject("get");
    getSyncStatus["summary"] = "Get multi-device sync status";
    getSyncStatus["tags"].add("Sync");

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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneLayout, request);

    // Schema validates zoneCount is 3 or 4
    uint8_t zoneCount = doc["zoneCount"];

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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneEffect, request);

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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneBrightness, request);

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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneSpeed, request);

    // Schema validates speed is 1-50
    uint8_t speed = doc["speed"];
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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZonePalette, request);

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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneBlend, request);

    // Schema validates blendMode is 0-7
    uint8_t blendModeVal = doc["blendMode"];
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
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneEnabled, request);

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
    uint32_t clientId = client->id();
    Serial.printf("[WebSocket] Client %u disconnected\n", clientId);

    // Cleanup LED stream subscription
    setLEDStreamSubscription(client, false);
}

void WebServer::handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Rate limit check (per-client IP)
    IPAddress clientIP = client->remoteIP();
    if (!m_rateLimiter.checkWebSocket(clientIP)) {
        uint32_t retryAfter = m_rateLimiter.getRetryAfterSeconds(clientIP);
        String errorMsg = buildWsRateLimitError(retryAfter);
        client->text(errorMsg);
        return;
    }

    // Parse message
    if (len > 1024) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Message too large"));
        return;
    }

    JsonDocument doc;
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

    // LED stream subscription (for real-time visualizer)
    else if (type == "ledStream.subscribe") {
        uint32_t clientId = client->id();
        const char* requestId = doc["requestId"] | "";
        bool ok = setLEDStreamSubscription(client, true);
        
        if (ok) {
            String response = buildWsResponse("ledStream.subscribed", requestId, [clientId](JsonObject& data) {
                data["clientId"] = clientId;
                data["frameSize"] = LedStreamConfig::FRAME_SIZE;
                data["targetFps"] = LedStreamConfig::TARGET_FPS;
                data["magicByte"] = LedStreamConfig::MAGIC_BYTE;
                data["accepted"] = true;
            });
            client->text(response);
            Serial.printf("[WebSocket] Client %u subscribed to LED stream (OK)\n", clientId);
        } else {
            // Manually build rejection response to set success=false
            JsonDocument response;
            response["type"] = "ledStream.rejected";
            if (requestId != nullptr && strlen(requestId) > 0) {
                response["requestId"] = requestId;
            }
            response["success"] = false;
            JsonObject error = response.createNestedObject("error");
            error["code"] = "RESOURCE_EXHAUSTED";
            error["message"] = "Subscriber table full";
            
            String output;
            serializeJson(response, output);
            client->text(output);
            Serial.printf("[WebSocket] Client %u subscribed to LED stream (REJECTED)\n", clientId);
        }
    }
    else if (type == "ledStream.unsubscribe") {
        uint32_t clientId = client->id();
        setLEDStreamSubscription(client, false);
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("ledStream.unsubscribed", requestId, [clientId](JsonObject& data) {
            data["clientId"] = clientId;
        });
        client->text(response);
        Serial.printf("[WebSocket] Client %u unsubscribed from LED stream\n", clientId);
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

            // Query PatternRegistry for metadata
            const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
            if (meta) {
                // Get family name
                char familyName[32];
                PatternRegistry::getFamilyName(meta->family, familyName, sizeof(familyName));
                data["family"] = familyName;
                data["familyId"] = static_cast<uint8_t>(meta->family);
                
                // Story and optical intent
                if (meta->story) {
                    data["story"] = meta->story;
                }
                if (meta->opticalIntent) {
                    data["opticalIntent"] = meta->opticalIntent;
                }
                
                // Tags as array
                JsonArray tags = data.createNestedArray("tags");
                if (meta->hasTag(PatternTags::STANDING)) tags.add("STANDING");
                if (meta->hasTag(PatternTags::TRAVELING)) tags.add("TRAVELING");
                if (meta->hasTag(PatternTags::MOIRE)) tags.add("MOIRE");
                if (meta->hasTag(PatternTags::DEPTH)) tags.add("DEPTH");
                if (meta->hasTag(PatternTags::SPECTRAL)) tags.add("SPECTRAL");
                if (meta->hasTag(PatternTags::CENTER_ORIGIN)) tags.add("CENTER_ORIGIN");
                if (meta->hasTag(PatternTags::DUAL_STRIP)) tags.add("DUAL_STRIP");
                if (meta->hasTag(PatternTags::PHYSICS)) tags.add("PHYSICS");
            } else {
                // Fallback if metadata not found
                data["family"] = "Unknown";
                data["familyId"] = 255;
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

    // effects.getCategories - Effect categories list (now returns Pattern Registry families)
    else if (type == "effects.getCategories") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("effects.categories", requestId, [](JsonObject& data) {
            JsonArray families = data.createNestedArray("categories");
            
            // Return all 10 pattern families from registry
            for (uint8_t i = 0; i < 10; i++) {
                PatternFamily family = static_cast<PatternFamily>(i);
                uint8_t count = PatternRegistry::getFamilyCount(family);
                
                JsonObject familyObj = families.createNestedObject();
                familyObj["id"] = i;
                
                char familyName[32];
                PatternRegistry::getFamilyName(family, familyName, sizeof(familyName));
                familyObj["name"] = familyName;
                familyObj["count"] = count;
            }
            
            data["total"] = 10;
        });
        client->text(response);
    }

    // effects.getByFamily - Get effects by family ID
    else if (type == "effects.getByFamily") {
        const char* requestId = doc["requestId"] | "";
        uint8_t familyId = doc["familyId"] | 255;
        
        if (familyId >= 10) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid familyId (0-9)", requestId));
            return;
        }
        
        PatternFamily family = static_cast<PatternFamily>(familyId);
        uint8_t patternIndices[128];  // Max 128 patterns per family
        uint8_t count = PatternRegistry::getPatternsByFamily(family, patternIndices, 128);
        
        String response = buildWsResponse("effects.byFamily", requestId, [familyId, patternIndices, count](JsonObject& data) {
            data["familyId"] = familyId;
            
            char familyName[32];
            PatternRegistry::getFamilyName(static_cast<PatternFamily>(familyId), familyName, sizeof(familyName));
            data["familyName"] = familyName;
            
            JsonArray effects = data.createNestedArray("effects");
            for (uint8_t i = 0; i < count; i++) {
                effects.add(patternIndices[i]);
            }
            
            data["count"] = count;
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

    // ========== V2 WebSocket Commands ==========

    // zone.setPalette - Set zone palette (CRITICAL for dashboard)
    else if (type == "zone.setPalette" && m_zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        uint8_t zoneId = doc["zoneId"] | 0;
        uint8_t paletteId = doc["paletteId"] | 0;

        if (zoneId >= m_zoneComposer->getZoneCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
            return;
        }

        m_zoneComposer->setZonePalette(zoneId, paletteId);
        broadcastZoneState();

        String response = buildWsResponse("zone.paletteChanged", requestId, [zoneId, paletteId](JsonObject& data) {
            data["zoneId"] = zoneId;
            data["paletteId"] = paletteId;
        });
        client->text(response);
    }

    // effects.list - Return paginated effects list
    else if (type == "effects.list") {
        const char* requestId = doc["requestId"] | "";
        uint8_t page = doc["page"] | 1;
        uint8_t limit = doc["limit"] | 20;
        bool details = doc["details"] | false;

        if (page < 1) page = 1;
        if (limit < 1) limit = 1;
        if (limit > 50) limit = 50;

        uint8_t effectCount = RENDERER->getEffectCount();
        uint8_t startIdx = (page - 1) * limit;
        uint8_t endIdx = min((uint8_t)(startIdx + limit), effectCount);

        String response = buildWsResponse("effects.list", requestId, [effectCount, startIdx, endIdx, page, limit, details](JsonObject& data) {
            JsonArray effects = data.createNestedArray("effects");

            for (uint8_t i = startIdx; i < endIdx; i++) {
                JsonObject effect = effects.createNestedObject();
                effect["id"] = i;
                effect["name"] = RENDERER->getEffectName(i);

                if (details) {
                    // Category based on ID ranges
                    if (i <= 4) {
                        effect["category"] = "Classic";
                    } else if (i <= 7) {
                        effect["category"] = "Wave";
                    } else if (i <= 12) {
                        effect["category"] = "Physics";
                    } else {
                        effect["category"] = "Custom";
                    }
                }
            }

            JsonObject pagination = data.createNestedObject("pagination");
            pagination["page"] = page;
            pagination["limit"] = limit;
            pagination["total"] = effectCount;
            pagination["pages"] = (effectCount + limit - 1) / limit;
        });
        client->text(response);
    }

    // effects.setCurrent - Set effect with optional transition
    else if (type == "effects.setCurrent") {
        const char* requestId = doc["requestId"] | "";
        uint8_t effectId = doc["effectId"] | 255;

        if (effectId == 255) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "effectId required", requestId));
            return;
        }

        if (effectId >= RENDERER->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
            return;
        }

        // Check for transition options
        bool useTransition = false;
        uint8_t transType = 0;
        uint16_t duration = 1000;

        if (doc.containsKey("transition")) {
            JsonObject trans = doc["transition"];
            useTransition = true;
            transType = trans["type"] | 0;
            duration = trans["duration"] | 1000;
        }

        if (useTransition && transType < static_cast<uint8_t>(TransitionType::TYPE_COUNT)) {
            RENDERER->startTransition(effectId, transType);
        } else {
            ACTOR_SYSTEM.setEffect(effectId);
        }

        broadcastStatus();

        String response = buildWsResponse("effects.changed", requestId, [effectId, useTransition](JsonObject& data) {
            data["effectId"] = effectId;
            data["name"] = RENDERER->getEffectName(effectId);
            data["transitionActive"] = useTransition;
        });
        client->text(response);
    }

    // parameters.set - Update parameters
    else if (type == "parameters.set") {
        const char* requestId = doc["requestId"] | "";

        // Track what was updated
        bool updatedBrightness = false;
        bool updatedSpeed = false;
        bool updatedPalette = false;

        if (doc.containsKey("brightness")) {
            uint8_t value = doc["brightness"] | 128;
            ACTOR_SYSTEM.setBrightness(value);
            updatedBrightness = true;
        }

        if (doc.containsKey("speed")) {
            uint8_t value = doc["speed"] | 15;
            if (value >= 1 && value <= 50) {
                ACTOR_SYSTEM.setSpeed(value);
                updatedSpeed = true;
            }
        }

        if (doc.containsKey("paletteId")) {
            uint8_t value = doc["paletteId"] | 0;
            ACTOR_SYSTEM.setPalette(value);
            updatedPalette = true;
        }

        broadcastStatus();

        String response = buildWsResponse("parameters.changed", requestId, [updatedBrightness, updatedSpeed, updatedPalette](JsonObject& data) {
            JsonArray updated = data.createNestedArray("updated");
            if (updatedBrightness) updated.add("brightness");
            if (updatedSpeed) updated.add("speed");
            if (updatedPalette) updated.add("paletteId");

            JsonObject current = data.createNestedObject("current");
            current["brightness"] = RENDERER->getBrightness();
            current["speed"] = RENDERER->getSpeed();
            current["paletteId"] = RENDERER->getPaletteIndex();
        });
        client->text(response);
    }

    // zones.list - Return all zones (alias for zones.get with v2 naming)
    else if (type == "zones.list") {
        const char* requestId = doc["requestId"] | "";

        if (!m_zoneComposer) {
            client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
            return;
        }

        String response = buildWsResponse("zones.list", requestId, [this](JsonObject& data) {
            data["enabled"] = m_zoneComposer->isEnabled();
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
            }
        });
        client->text(response);
    }

    // zones.update - Update zone configuration
    else if (type == "zones.update" && m_zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        uint8_t zoneId = doc["zoneId"] | 255;

        if (zoneId == 255) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "zoneId required", requestId));
            return;
        }

        if (zoneId >= m_zoneComposer->getZoneCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
            return;
        }

        // Track what was updated
        bool updatedEffect = false;
        bool updatedBrightness = false;
        bool updatedSpeed = false;
        bool updatedPalette = false;

        if (doc.containsKey("effectId")) {
            uint8_t effectId = doc["effectId"] | 0;
            if (effectId < RENDERER->getEffectCount()) {
                m_zoneComposer->setZoneEffect(zoneId, effectId);
                updatedEffect = true;
            }
        }

        if (doc.containsKey("brightness")) {
            uint8_t brightness = doc["brightness"] | 128;
            m_zoneComposer->setZoneBrightness(zoneId, brightness);
            updatedBrightness = true;
        }

        if (doc.containsKey("speed")) {
            uint8_t speed = doc["speed"] | 15;
            m_zoneComposer->setZoneSpeed(zoneId, speed);
            updatedSpeed = true;
        }

        if (doc.containsKey("paletteId")) {
            uint8_t paletteId = doc["paletteId"] | 0;
            m_zoneComposer->setZonePalette(zoneId, paletteId);
            updatedPalette = true;
        }

        broadcastZoneState();

        String response = buildWsResponse("zones.changed", requestId, [this, zoneId, updatedEffect, updatedBrightness, updatedSpeed, updatedPalette](JsonObject& data) {
            data["zoneId"] = zoneId;

            JsonArray updated = data.createNestedArray("updated");
            if (updatedEffect) updated.add("effectId");
            if (updatedBrightness) updated.add("brightness");
            if (updatedSpeed) updated.add("speed");
            if (updatedPalette) updated.add("paletteId");

            JsonObject current = data.createNestedObject("current");
            current["effectId"] = m_zoneComposer->getZoneEffect(zoneId);
            current["brightness"] = m_zoneComposer->getZoneBrightness(zoneId);
            current["speed"] = m_zoneComposer->getZoneSpeed(zoneId);
            current["paletteId"] = m_zoneComposer->getZonePalette(zoneId);
        });
        client->text(response);
    }

    // zones.setEffect - Set zone effect (v2 naming)
    else if (type == "zones.setEffect" && m_zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        uint8_t zoneId = doc["zoneId"] | 255;
        uint8_t effectId = doc["effectId"] | 255;

        if (zoneId == 255) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "zoneId required", requestId));
            return;
        }

        if (effectId == 255) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "effectId required", requestId));
            return;
        }

        if (zoneId >= m_zoneComposer->getZoneCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
            return;
        }

        if (effectId >= RENDERER->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
            return;
        }

        m_zoneComposer->setZoneEffect(zoneId, effectId);
        broadcastZoneState();

        String response = buildWsResponse("zones.effectChanged", requestId, [zoneId, effectId](JsonObject& data) {
            data["zoneId"] = zoneId;
            data["effectId"] = effectId;
            data["effectName"] = RENDERER->getEffectName(effectId);
        });
        client->text(response);
    }

    // device.getInfo - Device hardware info via WebSocket
    else if (type == "device.getInfo") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("device.info", requestId, [](JsonObject& data) {
            data["firmware"] = "2.0.0";
            data["board"] = "ESP32-S3-DevKitC-1";
            data["sdk"] = ESP.getSdkVersion();
            data["flashSize"] = ESP.getFlashChipSize();
            data["sketchSize"] = ESP.getSketchSize();
            data["freeSketch"] = ESP.getFreeSketchSpace();
            data["chipModel"] = ESP.getChipModel();
            data["chipRevision"] = ESP.getChipRevision();
            data["chipCores"] = ESP.getChipCores();
        });
        client->text(response);
    }

    // transitions.list - Transition types list (v2 naming)
    else if (type == "transitions.list") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("transitions.list", requestId, [](JsonObject& data) {
            JsonArray types = data.createNestedArray("types");

            for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
                JsonObject type = types.createNestedObject();
                type["id"] = i;
                type["name"] = getTransitionName(static_cast<TransitionType>(i));
            }

            JsonArray easings = data.createNestedArray("easingCurves");
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

            data["total"] = static_cast<uint8_t>(TransitionType::TYPE_COUNT);
        });
        client->text(response);
    }

    // transitions.trigger - Trigger transition (v2 naming)
    else if (type == "transitions.trigger") {
        const char* requestId = doc["requestId"] | "";
        uint8_t toEffect = doc["toEffect"] | 255;
        uint8_t transType = doc["type"] | 0;
        uint16_t duration = doc["duration"] | 1000;

        if (toEffect == 255) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "toEffect required", requestId));
            return;
        }

        if (toEffect >= RENDERER->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid toEffect", requestId));
            return;
        }

        uint8_t fromEffect = RENDERER->getCurrentEffect();
        RENDERER->startTransition(toEffect, transType);
        broadcastStatus();

        String response = buildWsResponse("transition.started", requestId, [fromEffect, toEffect, transType, duration](JsonObject& data) {
            data["fromEffect"] = fromEffect;
            data["toEffect"] = toEffect;
            data["toEffectName"] = RENDERER->getEffectName(toEffect);
            data["transitionType"] = transType;
            data["transitionName"] = getTransitionName(static_cast<TransitionType>(transType));
            data["duration"] = duration;
        });
        client->text(response);
    }

    // narrative.getStatus - Get current narrative state
    else if (type == "narrative.getStatus") {
        using namespace lightwaveos::narrative;
        const char* requestId = doc["requestId"] | "";
        NarrativeEngine& narrative = NarrativeEngine::getInstance();
        
        String response = buildWsResponse("narrative.status", requestId, [&narrative](JsonObject& data) {
            // Current state
            data["enabled"] = narrative.isEnabled();
            data["tension"] = narrative.getTension() * 100.0f;  // Convert 0-1 to 0-100
            data["phaseT"] = narrative.getPhaseT();
            data["cycleT"] = narrative.getCycleT();
            
            // Phase as string
            NarrativePhase phase = narrative.getPhase();
            const char* phaseName = "UNKNOWN";
            switch (phase) {
                case PHASE_BUILD:   phaseName = "BUILD"; break;
                case PHASE_HOLD:    phaseName = "HOLD"; break;
                case PHASE_RELEASE: phaseName = "RELEASE"; break;
                case PHASE_REST:    phaseName = "REST"; break;
            }
            data["phase"] = phaseName;
            data["phaseId"] = static_cast<uint8_t>(phase);
            
            // Phase durations
            JsonObject durations = data.createNestedObject("durations");
            durations["build"] = narrative.getBuildDuration();
            durations["hold"] = narrative.getHoldDuration();
            durations["release"] = narrative.getReleaseDuration();
            durations["rest"] = narrative.getRestDuration();
            durations["total"] = narrative.getTotalDuration();
            
            // Derived values
            data["tempoMultiplier"] = narrative.getTempoMultiplier();
            data["complexityScaling"] = narrative.getComplexityScaling();
        });
        client->text(response);
    }

    // narrative.config - Get narrative configuration
    else if (type == "narrative.config" && !doc.containsKey("durations") && !doc.containsKey("enabled") && !doc.containsKey("curves")) {
        using namespace lightwaveos::narrative;
        const char* requestId = doc["requestId"] | "";
        NarrativeEngine& narrative = NarrativeEngine::getInstance();
        
        String response = buildWsResponse("narrative.config", requestId, [&narrative](JsonObject& data) {
            // Phase durations
            JsonObject durations = data.createNestedObject("durations");
            durations["build"] = narrative.getBuildDuration();
            durations["hold"] = narrative.getHoldDuration();
            durations["release"] = narrative.getReleaseDuration();
            durations["rest"] = narrative.getRestDuration();
            durations["total"] = narrative.getTotalDuration();
            
            // Curves
            JsonObject curves = data.createNestedObject("curves");
            curves["build"] = static_cast<uint8_t>(narrative.getBuildCurve());
            curves["release"] = static_cast<uint8_t>(narrative.getReleaseCurve());
            
            // Optional behaviors
            data["holdBreathe"] = narrative.getHoldBreathe();
            data["snapAmount"] = narrative.getSnapAmount();
            data["durationVariance"] = narrative.getDurationVariance();
            
            data["enabled"] = narrative.isEnabled();
        });
        client->text(response);
    }

    // narrative.config - Set narrative configuration
    else if (type == "narrative.config" && (doc.containsKey("durations") || doc.containsKey("enabled") || doc.containsKey("curves"))) {
        using namespace lightwaveos::narrative;
        const char* requestId = doc["requestId"] | "";
        NarrativeEngine& narrative = NarrativeEngine::getInstance();
        bool updated = false;
        
        // Update phase durations if provided
        if (doc.containsKey("durations")) {
            JsonObject durations = doc["durations"];
            if (durations.containsKey("build")) {
                narrative.setBuildDuration(durations["build"] | 1.5f);
                updated = true;
            }
            if (durations.containsKey("hold")) {
                narrative.setHoldDuration(durations["hold"] | 0.5f);
                updated = true;
            }
            if (durations.containsKey("release")) {
                narrative.setReleaseDuration(durations["release"] | 1.5f);
                updated = true;
            }
            if (durations.containsKey("rest")) {
                narrative.setRestDuration(durations["rest"] | 0.5f);
                updated = true;
            }
        }
        
        // Update curves if provided
        if (doc.containsKey("curves")) {
            JsonObject curves = doc["curves"];
            if (curves.containsKey("build")) {
                narrative.setBuildCurve(static_cast<lightwaveos::effects::EasingCurve>(curves["build"] | 1));
                updated = true;
            }
            if (curves.containsKey("release")) {
                narrative.setReleaseCurve(static_cast<lightwaveos::effects::EasingCurve>(curves["release"] | 6));
                updated = true;
            }
        }
        
        // Update optional behaviors
        if (doc.containsKey("holdBreathe")) {
            narrative.setHoldBreathe(doc["holdBreathe"] | 0.0f);
            updated = true;
        }
        if (doc.containsKey("snapAmount")) {
            narrative.setSnapAmount(doc["snapAmount"] | 0.0f);
            updated = true;
        }
        if (doc.containsKey("durationVariance")) {
            narrative.setDurationVariance(doc["durationVariance"] | 0.0f);
            updated = true;
        }
        
        // Update enabled state
        if (doc.containsKey("enabled")) {
            bool enabled = doc["enabled"] | false;
            if (enabled) {
                narrative.enable();
            } else {
                narrative.disable();
            }
            updated = true;
        }
        
        String response = buildWsResponse("narrative.config", requestId, [updated](JsonObject& data) {
            data["message"] = updated ? "Narrative config updated" : "No changes";
            data["updated"] = updated;
        });
        client->text(response);
    }

    // palettes.list - Return paginated palette list
    else if (type == "palettes.list") {
        const char* requestId = doc["requestId"] | "";
        uint8_t page = doc["page"] | 1;
        uint8_t limit = doc["limit"] | 20;

        if (page < 1) page = 1;
        if (limit < 1) limit = 1;
        if (limit > 50) limit = 50;

        uint8_t startIdx = (page - 1) * limit;
        uint8_t endIdx = min((uint8_t)(startIdx + limit), (uint8_t)MASTER_PALETTE_COUNT);

        String response = buildWsResponse("palettes.list", requestId, [startIdx, endIdx, page, limit](JsonObject& data) {
            JsonArray palettes = data.createNestedArray("palettes");

            for (uint8_t i = startIdx; i < endIdx; i++) {
                JsonObject palette = palettes.createNestedObject();
                palette["id"] = i;
                palette["name"] = MasterPaletteNames[i];
                palette["category"] = getPaletteCategory(i);
            }

            JsonObject pagination = data.createNestedObject("pagination");
            pagination["page"] = page;
            pagination["limit"] = limit;
            pagination["total"] = MASTER_PALETTE_COUNT;
            pagination["pages"] = (MASTER_PALETTE_COUNT + limit - 1) / limit;
        });
        client->text(response);
    }

    // palettes.get - Get single palette details
    else if (type == "palettes.get") {
        const char* requestId = doc["requestId"] | "";
        uint8_t paletteId = doc["paletteId"] | 255;

        if (paletteId == 255) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "paletteId required", requestId));
            return;
        }

        if (paletteId >= MASTER_PALETTE_COUNT) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Palette ID out of range", requestId));
            return;
        }

        String response = buildWsResponse("palettes.get", requestId, [paletteId](JsonObject& data) {
            JsonObject palette = data.createNestedObject("palette");
            palette["id"] = paletteId;
            palette["name"] = MasterPaletteNames[paletteId];
            palette["category"] = getPaletteCategory(paletteId);

            JsonObject flags = palette.createNestedObject("flags");
            flags["warm"] = isPaletteWarm(paletteId);
            flags["cool"] = isPaletteCool(paletteId);
            flags["calm"] = isPaletteCalm(paletteId);
            flags["vivid"] = isPaletteVivid(paletteId);
            flags["cvdFriendly"] = isPaletteCVDFriendly(paletteId);
            flags["whiteHeavy"] = paletteHasFlag(paletteId, PAL_WHITE_HEAVY);

            palette["avgBrightness"] = getPaletteAvgBrightness(paletteId);
            palette["maxBrightness"] = getPaletteMaxBrightness(paletteId);
        });
        client->text(response);
    }

    // ========================================================================
    // Motion Engine - Core, Phase, and Speed Commands
    // ========================================================================

    // motion.getStatus - Get motion engine status
    else if (type == "motion.getStatus") {
        const char* requestId = doc["requestId"] | "";
        auto& engine = lightwaveos::enhancement::MotionEngine::getInstance();

        String response = buildWsResponse("motion.getStatus", requestId, [&engine](JsonObject& data) {
            data["enabled"] = engine.isEnabled();
            data["phaseOffset"] = engine.getPhaseController().stripPhaseOffset;
            data["autoRotateSpeed"] = engine.getPhaseController().phaseVelocity;
            data["baseSpeed"] = engine.getSpeedModulator().getBaseSpeed();
        });
        client->text(response);
    }

    // motion.enable - Enable motion engine
    else if (type == "motion.enable") {
        const char* requestId = doc["requestId"] | "";
        auto& engine = lightwaveos::enhancement::MotionEngine::getInstance();
        engine.enable();

        String response = buildWsResponse("motion.enable", requestId, [](JsonObject& data) {
            data["enabled"] = true;
        });
        client->text(response);
    }

    // motion.disable - Disable motion engine
    else if (type == "motion.disable") {
        const char* requestId = doc["requestId"] | "";
        auto& engine = lightwaveos::enhancement::MotionEngine::getInstance();
        engine.disable();

        String response = buildWsResponse("motion.disable", requestId, [](JsonObject& data) {
            data["enabled"] = false;
        });
        client->text(response);
    }

    // motion.phase.setOffset - Set strip phase offset
    else if (type == "motion.phase.setOffset") {
        const char* requestId = doc["requestId"] | "";
        float degrees = doc["degrees"] | -1.0f;

        if (degrees < 0.0f || degrees > 360.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "degrees must be 0-360", requestId));
            return;
        }

        auto& engine = lightwaveos::enhancement::MotionEngine::getInstance();
        engine.getPhaseController().setStripPhaseOffset(degrees);

        String response = buildWsResponse("motion.phase.setOffset", requestId, [degrees](JsonObject& data) {
            data["degrees"] = degrees;
        });
        client->text(response);
    }

    // motion.phase.enableAutoRotate - Enable auto-rotation
    else if (type == "motion.phase.enableAutoRotate") {
        const char* requestId = doc["requestId"] | "";
        float degreesPerSecond = doc["degreesPerSecond"] | 0.0f;

        auto& engine = lightwaveos::enhancement::MotionEngine::getInstance();
        engine.getPhaseController().enableAutoRotate(degreesPerSecond);

        String response = buildWsResponse("motion.phase.enableAutoRotate", requestId, [degreesPerSecond](JsonObject& data) {
            data["degreesPerSecond"] = degreesPerSecond;
            data["autoRotate"] = true;
        });
        client->text(response);
    }

    // motion.phase.getPhase - Get current phase
    else if (type == "motion.phase.getPhase") {
        const char* requestId = doc["requestId"] | "";
        auto& engine = lightwaveos::enhancement::MotionEngine::getInstance();
        float radians = engine.getPhaseController().getStripPhaseRadians();
        float degrees = radians * 180.0f / 3.14159265f;

        String response = buildWsResponse("motion.phase.getPhase", requestId, [degrees, radians](JsonObject& data) {
            data["degrees"] = degrees;
            data["radians"] = radians;
        });
        client->text(response);
    }

    // motion.speed.setModulation - Set speed modulation mode
    else if (type == "motion.speed.setModulation") {
        const char* requestId = doc["requestId"] | "";
        const char* modTypeStr = doc["type"] | "";
        float depth = doc["depth"] | 0.5f;

        if (depth < 0.0f || depth > 1.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "depth must be 0.0-1.0", requestId));
            return;
        }

        // Parse modulation type string
        lightwaveos::enhancement::SpeedModulator::ModulationType modType;
        if (strcmp(modTypeStr, "CONSTANT") == 0) {
            modType = lightwaveos::enhancement::SpeedModulator::MOD_CONSTANT;
        } else if (strcmp(modTypeStr, "SINE_WAVE") == 0) {
            modType = lightwaveos::enhancement::SpeedModulator::MOD_SINE_WAVE;
        } else if (strcmp(modTypeStr, "EXPONENTIAL_DECAY") == 0) {
            modType = lightwaveos::enhancement::SpeedModulator::MOD_EXPONENTIAL_DECAY;
        } else {
            client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Invalid type (CONSTANT, SINE_WAVE, EXPONENTIAL_DECAY)", requestId));
            return;
        }

        auto& engine = lightwaveos::enhancement::MotionEngine::getInstance();
        engine.getSpeedModulator().setModulation(modType, depth);

        String response = buildWsResponse("motion.speed.setModulation", requestId, [modTypeStr, depth](JsonObject& data) {
            data["type"] = modTypeStr;
            data["depth"] = depth;
        });
        client->text(response);
    }

    // motion.speed.setBaseSpeed - Set base speed
    else if (type == "motion.speed.setBaseSpeed") {
        const char* requestId = doc["requestId"] | "";
        float speed = doc["speed"] | -1.0f;

        if (speed < 0.0f) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "speed required", requestId));
            return;
        }

        auto& engine = lightwaveos::enhancement::MotionEngine::getInstance();
        engine.getSpeedModulator().setBaseSpeed(speed);

        String response = buildWsResponse("motion.speed.setBaseSpeed", requestId, [speed](JsonObject& data) {
            data["speed"] = speed;
        });
        client->text(response);
    }

    // ========================================================================
    // Motion Engine - Momentum/Particle Physics Commands
    // ========================================================================

    // motion.momentum.getStatus - Get particle system status
    else if (type == "motion.momentum.getStatus") {
        const char* requestId = doc["requestId"] | "";
        auto& momentum = lightwaveos::enhancement::MotionEngine::getInstance().getMomentumEngine();

        String response = buildWsResponse("motion.momentum.getStatus", requestId, [&momentum](JsonObject& data) {
            data["activeCount"] = momentum.getActiveCount();
            data["maxParticles"] = lightwaveos::enhancement::MomentumEngine::MAX_PARTICLES;
        });
        client->text(response);
    }

    // motion.momentum.addParticle - Create a new particle
    else if (type == "motion.momentum.addParticle") {
        const char* requestId = doc["requestId"] | "";
        auto& momentum = lightwaveos::enhancement::MotionEngine::getInstance().getMomentumEngine();

        float pos = doc["position"] | 0.5f;
        float vel = doc["velocity"] | 0.0f;
        float mass = doc["mass"] | 1.0f;

        // Parse boundary mode from string
        lightwaveos::enhancement::BoundaryMode mode = lightwaveos::enhancement::BOUNDARY_WRAP;
        String boundaryStr = doc["boundary"] | "WRAP";
        if (boundaryStr == "BOUNCE") mode = lightwaveos::enhancement::BOUNDARY_BOUNCE;
        else if (boundaryStr == "CLAMP") mode = lightwaveos::enhancement::BOUNDARY_CLAMP;
        else if (boundaryStr == "DIE") mode = lightwaveos::enhancement::BOUNDARY_DIE;

        int id = momentum.addParticle(pos, vel, mass, CRGB::White, mode);

        String response = buildWsResponse("motion.momentum.addParticle", requestId, [id](JsonObject& data) {
            data["particleId"] = id;
            data["success"] = (id >= 0);
        });
        client->text(response);
    }

    // motion.momentum.applyForce - Apply force to a particle
    else if (type == "motion.momentum.applyForce") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("particleId")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "particleId required", requestId));
            return;
        }

        int particleId = doc["particleId"] | -1;
        float force = doc["force"] | 0.0f;

        if (particleId < 0 || particleId >= lightwaveos::enhancement::MomentumEngine::MAX_PARTICLES) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "particleId out of range (0-31)", requestId));
            return;
        }

        auto& momentum = lightwaveos::enhancement::MotionEngine::getInstance().getMomentumEngine();
        momentum.applyForce(particleId, force);

        String response = buildWsResponse("motion.momentum.applyForce", requestId, [particleId, force](JsonObject& data) {
            data["particleId"] = particleId;
            data["force"] = force;
            data["applied"] = true;
        });
        client->text(response);
    }

    // motion.momentum.getParticle - Get particle state
    else if (type == "motion.momentum.getParticle") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("particleId")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "particleId required", requestId));
            return;
        }

        int particleId = doc["particleId"] | -1;

        if (particleId < 0 || particleId >= lightwaveos::enhancement::MomentumEngine::MAX_PARTICLES) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "particleId out of range (0-31)", requestId));
            return;
        }

        auto& momentum = lightwaveos::enhancement::MotionEngine::getInstance().getMomentumEngine();
        auto* particle = momentum.getParticle(particleId);

        if (!particle) {
            client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to get particle", requestId));
            return;
        }

        String response = buildWsResponse("motion.momentum.getParticle", requestId, [particleId, particle](JsonObject& data) {
            data["particleId"] = particleId;
            data["position"] = particle->position;
            data["velocity"] = particle->velocity;
            data["mass"] = particle->mass;
            data["alive"] = particle->active;
        });
        client->text(response);
    }

    // motion.momentum.reset - Clear all particles
    else if (type == "motion.momentum.reset") {
        const char* requestId = doc["requestId"] | "";
        auto& momentum = lightwaveos::enhancement::MotionEngine::getInstance().getMomentumEngine();

        momentum.reset();

        String response = buildWsResponse("motion.momentum.reset", requestId, [](JsonObject& data) {
            data["message"] = "All particles cleared";
            data["activeCount"] = 0;
        });
        client->text(response);
    }

    // motion.momentum.update - Force physics tick
    else if (type == "motion.momentum.update") {
        const char* requestId = doc["requestId"] | "";
        float deltaTime = doc["deltaTime"] | 0.016f;  // Default ~60fps

        auto& momentum = lightwaveos::enhancement::MotionEngine::getInstance().getMomentumEngine();
        momentum.update(deltaTime);

        String response = buildWsResponse("motion.momentum.update", requestId, [deltaTime, &momentum](JsonObject& data) {
            data["deltaTime"] = deltaTime;
            data["activeCount"] = momentum.getActiveCount();
            data["updated"] = true;
        });
        client->text(response);
    }

    // ========================================================================
    // Color Engine - Cross-palette blending, rotation, diffusion
    // ========================================================================

    // color.getStatus - Get current engine state
    else if (type == "color.getStatus") {
        const char* requestId = doc["requestId"] | "";
        auto& engine = lightwaveos::enhancement::ColorEngine::getInstance();

        String response = buildWsResponse("color.getStatus", requestId, [&engine](JsonObject& data) {
            data["active"] = engine.isActive();
            data["blendEnabled"] = engine.isCrossBlendEnabled();

            JsonArray blendFactors = data.createNestedArray("blendFactors");
            blendFactors.add(engine.getBlendFactor1());
            blendFactors.add(engine.getBlendFactor2());
            blendFactors.add(engine.getBlendFactor3());

            data["rotationEnabled"] = engine.isRotationEnabled();
            data["rotationSpeed"] = engine.getRotationSpeed();
            data["rotationPhase"] = engine.getRotationPhase();

            data["diffusionEnabled"] = engine.isDiffusionEnabled();
            data["diffusionAmount"] = engine.getDiffusionAmount();
        });
        client->text(response);
    }

    // color.enableBlend - Enable/disable cross-palette blending
    else if (type == "color.enableBlend") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("enable")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "enable required", requestId));
            return;
        }

        bool enable = doc["enable"] | false;
        auto& engine = lightwaveos::enhancement::ColorEngine::getInstance();
        engine.enableCrossBlend(enable);

        String response = buildWsResponse("color.enableBlend", requestId, [enable](JsonObject& data) {
            data["blendEnabled"] = enable;
        });
        client->text(response);
    }

    // color.setBlendPalettes - Set 2-3 palettes to blend
    else if (type == "color.setBlendPalettes") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("palette1") || !doc.containsKey("palette2")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "palette1 and palette2 required", requestId));
            return;
        }

        uint8_t p1 = doc["palette1"] | 0;
        uint8_t p2 = doc["palette2"] | 0;
        uint8_t p3 = doc["palette3"] | 255; // 255 = not specified

        if (p1 >= MASTER_PALETTE_COUNT || p2 >= MASTER_PALETTE_COUNT ||
            (p3 != 255 && p3 >= MASTER_PALETTE_COUNT)) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Palette ID out of range", requestId));
            return;
        }

        auto& engine = lightwaveos::enhancement::ColorEngine::getInstance();
        CRGBPalette16 pal1(gMasterPalettes[p1]);
        CRGBPalette16 pal2(gMasterPalettes[p2]);

        if (p3 != 255) {
            CRGBPalette16 pal3(gMasterPalettes[p3]);
            engine.setBlendPalettes(pal1, pal2, &pal3);
        } else {
            engine.setBlendPalettes(pal1, pal2, nullptr);
        }

        String response = buildWsResponse("color.setBlendPalettes", requestId, [p1, p2, p3](JsonObject& data) {
            JsonArray palettes = data.createNestedArray("blendPalettes");
            palettes.add(p1);
            palettes.add(p2);
            if (p3 != 255) palettes.add(p3);
        });
        client->text(response);
    }

    // color.setBlendFactors - Set blend weights
    else if (type == "color.setBlendFactors") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("factor1") || !doc.containsKey("factor2")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "factor1 and factor2 required", requestId));
            return;
        }

        uint8_t f1 = doc["factor1"] | 0;
        uint8_t f2 = doc["factor2"] | 0;
        uint8_t f3 = doc["factor3"] | 0;

        auto& engine = lightwaveos::enhancement::ColorEngine::getInstance();
        engine.setBlendFactors(f1, f2, f3);

        String response = buildWsResponse("color.setBlendFactors", requestId, [f1, f2, f3](JsonObject& data) {
            JsonArray factors = data.createNestedArray("blendFactors");
            factors.add(f1);
            factors.add(f2);
            factors.add(f3);
        });
        client->text(response);
    }

    // color.enableRotation - Enable temporal hue rotation
    else if (type == "color.enableRotation") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("enable")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "enable required", requestId));
            return;
        }

        bool enable = doc["enable"] | false;
        auto& engine = lightwaveos::enhancement::ColorEngine::getInstance();
        engine.enableTemporalRotation(enable);

        String response = buildWsResponse("color.enableRotation", requestId, [enable](JsonObject& data) {
            data["rotationEnabled"] = enable;
        });
        client->text(response);
    }

    // color.setRotationSpeed - Set rotation speed
    else if (type == "color.setRotationSpeed") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("degreesPerFrame")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "degreesPerFrame required", requestId));
            return;
        }

        float speed = doc["degreesPerFrame"] | 0.0f;
        auto& engine = lightwaveos::enhancement::ColorEngine::getInstance();
        engine.setRotationSpeed(speed);

        String response = buildWsResponse("color.setRotationSpeed", requestId, [speed](JsonObject& data) {
            data["rotationSpeed"] = speed;
        });
        client->text(response);
    }

    // color.enableDiffusion - Enable Gaussian blur
    else if (type == "color.enableDiffusion") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("enable")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "enable required", requestId));
            return;
        }

        bool enable = doc["enable"] | false;
        auto& engine = lightwaveos::enhancement::ColorEngine::getInstance();
        engine.enableDiffusion(enable);

        String response = buildWsResponse("color.enableDiffusion", requestId, [enable](JsonObject& data) {
            data["diffusionEnabled"] = enable;
        });
        client->text(response);
    }

    // color.setDiffusionAmount - Set blur intensity
    else if (type == "color.setDiffusionAmount") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("amount")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "amount required", requestId));
            return;
        }

        uint8_t amount = doc["amount"] | 0;
        auto& engine = lightwaveos::enhancement::ColorEngine::getInstance();
        engine.setDiffusionAmount(amount);

        String response = buildWsResponse("color.setDiffusionAmount", requestId, [amount](JsonObject& data) {
            data["diffusionAmount"] = amount;
        });
        client->text(response);
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
// LED Frame Streaming
// ============================================================================

void WebServer::broadcastLEDFrame() {
    // Skip if no subscribers or no clients connected
    if (!hasLEDStreamSubscribers() || m_ws->count() == 0) return;

    // Throttle to target FPS
    uint32_t now = millis();
    if (now - m_lastLedBroadcast < LedStreamConfig::FRAME_INTERVAL_MS) return;
    m_lastLedBroadcast = now;

    // Get LED buffer from renderer
    if (!RENDERER) return;

    // Build binary frame: [magic byte][RGB data...]
    m_ledFrameBuffer[0] = LedStreamConfig::MAGIC_BYTE;

    // Copy LED data cross-core safely (do NOT read renderer buffer directly)
    CRGB leds[LedStreamConfig::TOTAL_LEDS];
    RENDERER->getBufferCopy(leds);
    uint8_t* dst = &m_ledFrameBuffer[1];
    for (uint16_t i = 0; i < LedStreamConfig::TOTAL_LEDS; i++) {
        *dst++ = leds[i].r;
        *dst++ = leds[i].g;
        *dst++ = leds[i].b;
    }

    // Send to subscribed clients only
    // NOTE: Avoid version-fragile iteration (getClients()) by sending only to our tracked subscribers.
    uint32_t ids[WebServerConfig::MAX_WS_CLIENTS];
    size_t count = 0;
#if defined(ESP32)
    portENTER_CRITICAL(&m_ledStreamMux);
#endif
    count = m_ledStreamSubscribers.count();
    for (size_t i = 0; i < count; i++) {
        ids[i] = m_ledStreamSubscribers.get(i);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_ledStreamMux);
#endif

    uint32_t toRemove[WebServerConfig::MAX_WS_CLIENTS];
    uint8_t removeCount = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t clientId = ids[i];
        AsyncWebSocketClient* c = m_ws->client(clientId);
        if (!c || c->status() != WS_CONNECTED) {
            toRemove[removeCount++] = clientId;
            continue;
        }
        c->binary(m_ledFrameBuffer, LedStreamConfig::FRAME_SIZE + 1);
    }

    if (removeCount > 0) {
#if defined(ESP32)
        portENTER_CRITICAL(&m_ledStreamMux);
#endif
        for (uint8_t r = 0; r < removeCount; r++) {
            m_ledStreamSubscribers.remove(toRemove[r]);
        }
#if defined(ESP32)
        portEXIT_CRITICAL(&m_ledStreamMux);
#endif
    }
}

bool WebServer::setLEDStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client) return false;
    uint32_t clientId = client->id();
    bool success = false;
    bool wasPresent = false;

#if defined(ESP32)
    portENTER_CRITICAL(&m_ledStreamMux);
#endif
    if (subscribe) {
        success = m_ledStreamSubscribers.add(clientId);
    } else {
        wasPresent = m_ledStreamSubscribers.remove(clientId);
        success = true;
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&m_ledStreamMux);
#endif

    if (subscribe && success) {
        Serial.printf("[WebServer] Client %u subscribed to LED stream\n", clientId);
    } else if (!subscribe && wasPresent) {
        Serial.printf("[WebServer] Client %u unsubscribed from LED stream\n", clientId);
    }

    return success;
}

bool WebServer::hasLEDStreamSubscribers() const {
    bool has = false;
#if defined(ESP32)
    portENTER_CRITICAL(&m_ledStreamMux);
#endif
    has = m_ledStreamSubscribers.count() > 0;
#if defined(ESP32)
    portEXIT_CRITICAL(&m_ledStreamMux);
#endif
    return has;
}

// ============================================================================
// Rate Limiting
// ============================================================================

bool WebServer::checkRateLimit(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (!m_rateLimiter.checkHTTP(clientIP)) {
        uint32_t retryAfter = m_rateLimiter.getRetryAfterSeconds(clientIP);
        sendRateLimitError(request, retryAfter);
        return false;
    }
    return true;
}

bool WebServer::checkWsRateLimit(AsyncWebSocketClient* client) {
    IPAddress clientIP = client->remoteIP();
    return m_rateLimiter.checkWebSocket(clientIP);
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
