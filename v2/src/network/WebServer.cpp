/**
 * @file WebServer.cpp
 * @brief Web Server implementation for LightwaveOS v2
 *
 * Implements REST API and WebSocket server integrated with Actor System.
 * All state changes are routed through m_actorSystem for thread safety.
 */

#include "WebServer.h"

#if FEATURE_WEB_SERVER

#include "ApiResponse.h"
#include "RequestValidator.h"
#include "WiFiManager.h"
#include "webserver/RateLimiter.h"
#include "webserver/LedStreamBroadcaster.h"
#include "webserver/handlers/DeviceHandlers.h"
#include "webserver/handlers/EffectHandlers.h"
#include "webserver/handlers/ZoneHandlers.h"
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
#include "../plugins/api/IEffect.h"
#if FEATURE_AUDIO_SYNC
#include "../audio/AudioTuning.h"
#include "../config/audio_config.h"
#endif
#include <cstring>
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

// Global instance pointer (initialized in setup)
WebServer* webServerInstance = nullptr;

// ============================================================================
// Constructor / Destructor
// ============================================================================

WebServer::WebServer(ActorSystem& actors, RendererActor* renderer)
    : m_server(nullptr)
    , m_ws(nullptr)
    , m_running(false)
    , m_apMode(false)
    , m_mdnsStarted(false)
    , m_lastBroadcast(0)
    , m_startTime(0)
    , m_zoneComposer(nullptr)
    , m_ledBroadcaster(nullptr)
    , m_actorSystem(actors)
    , m_renderer(renderer)
{
}

WebServer::~WebServer() {
    stop();
    delete m_ledBroadcaster;
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
    
    // Create LED stream broadcaster
    m_ledBroadcaster = new webserver::LedStreamBroadcaster(m_ws, WebServerConfig::MAX_WS_CLIENTS);

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
    if (m_renderer) {
        m_zoneComposer = m_renderer->getZoneComposer();
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

    m_server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
            return;
        }

        const char* html =
            "<!doctype html><html><head><meta charset=\"utf-8\"/>"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
            "<title>LightwaveOS</title></head><body>"
            "<h1>LightwaveOS Web UI not installed</h1>"
            "<p>The device web server is running, but no UI files were found on LittleFS.</p>"
            "<p>API is available at <a href=\"/api/v1/\">/api/v1/</a>.</p>"
            "</body></html>";
        request->send(200, "text/html", html);
    });

    m_server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/favicon.ico")) {
            request->send(LittleFS, "/favicon.ico", "image/x-icon");
            return;
        }
        request->send(HttpStatus::NO_CONTENT);
    });

    // Serve the dashboard assets explicitly.
    // We intentionally avoid serveStatic() here because its gzip probing can spam VFS errors
    // on missing *.gz files (ESP-IDF logs these at error level).
    m_server->on("/assets/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/assets/app.js")) {
            request->send(LittleFS, "/assets/app.js", "application/javascript");
            return;
        }
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /assets/app.js");
    });

    m_server->on("/assets/styles.css", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/assets/styles.css")) {
            request->send(LittleFS, "/assets/styles.css", "text/css");
            return;
        }
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /assets/styles.css");
    });

    m_server->on("/vite.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/vite.svg")) {
            request->send(LittleFS, "/vite.svg", "image/svg+xml");
            return;
        }
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /vite.svg");
    });

    // Handle CORS preflight and 404
    m_server->onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(HttpStatus::NO_CONTENT);
            return;
        }

        String url = request->url();
        if (!url.startsWith("/api") && !url.startsWith("/ws")) {
            if (LittleFS.exists("/index.html")) {
                int slash = url.lastIndexOf('/');
                int dot = url.lastIndexOf('.');
                bool hasExtension = (dot > slash);
                if (!hasExtension) {
                    request->send(LittleFS, "/index.html", "text/html");
                    return;
                }
            }
            request->send(HttpStatus::NOT_FOUND, "text/plain", "Not found");
            return;
        }

        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Endpoint not found");
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
                m_renderer->getCurrentEffect(),
                m_renderer->getBrightness(),
                m_renderer->getSpeed(),
                m_renderer->getPaletteIndex()
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
                m_actorSystem.setEffect(effectId);
                m_actorSystem.setBrightness(brightness);
                m_actorSystem.setSpeed(speed);
                m_actorSystem.setPalette(paletteId);
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
            StaticJsonDocument<512> doc;
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

            StaticJsonDocument<512> doc;
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
        webserver::handlers::DeviceHandlers::handleStatus(request, m_actorSystem, m_renderer, m_startTime, m_apMode, m_ws->count());
    });

    // Device Info - GET /api/v1/device/info
    m_server->on("/api/v1/device/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        webserver::handlers::DeviceHandlers::handleInfo(request, m_actorSystem, m_renderer);
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
        webserver::handlers::EffectHandlers::handleMetadata(request, m_renderer);
    });

    // Effect Parameters - GET /api/v1/effects/parameters?id=N
    m_server->on("/api/v1/effects/parameters", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        webserver::handlers::EffectHandlers::handleParametersGet(request, m_renderer);
    });

    // Effect Parameters - POST /api/v1/effects/parameters
    m_server->on("/api/v1/effects/parameters", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::EffectHandlers::handleParametersSet(request, data, len, m_renderer);
        }
    );

    // Effect Parameters - PATCH /api/v1/effects/parameters (compat)
    m_server->on("/api/v1/effects/parameters", HTTP_PATCH,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::EffectHandlers::handleParametersSet(request, data, len, m_renderer);
        }
    );

    // Effect Families - GET /api/v1/effects/families
    m_server->on("/api/v1/effects/families", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        webserver::handlers::EffectHandlers::handleFamilies(request);
    });

    // Effects List - GET /api/v1/effects
    m_server->on("/api/v1/effects", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        webserver::handlers::EffectHandlers::handleList(request, m_renderer);
    });

    // Current Effect - GET /api/v1/effects/current
    m_server->on("/api/v1/effects/current", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        webserver::handlers::EffectHandlers::handleCurrent(request, m_renderer);
    });

    // Set Effect - POST /api/v1/effects/set
    m_server->on("/api/v1/effects/set", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::EffectHandlers::handleSet(request, data, len, m_actorSystem, m_renderer, [this](){ broadcastStatus(); });
        }
    );

    // Set Effect - PUT /api/v1/effects/current (V2 API compatibility)
    m_server->on("/api/v1/effects/current", HTTP_PUT,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::EffectHandlers::handleSet(request, data, len, m_actorSystem, m_renderer, [this](){ broadcastStatus(); });
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

    // Set Parameters - PATCH /api/v1/parameters (V2 API compatibility)
    m_server->on("/api/v1/parameters", HTTP_PATCH,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleParametersSet(request, data, len);
        }
    );

    // Get Audio Parameters - GET /api/v1/audio/parameters
    m_server->on("/api/v1/audio/parameters", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleAudioParametersGet(request);
    });

    // Set Audio Parameters - POST /api/v1/audio/parameters
    m_server->on("/api/v1/audio/parameters", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleAudioParametersSet(request, data, len);
        }
    );

    // Set Audio Parameters - PATCH /api/v1/audio/parameters (V2 API compatibility)
    m_server->on("/api/v1/audio/parameters", HTTP_PATCH,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            handleAudioParametersSet(request, data, len);
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
        webserver::handlers::ZoneHandlers::handleList(request, m_actorSystem, m_renderer, m_zoneComposer);
    });

    // Set zone layout - POST /api/v1/zones/layout
    m_server->on("/api/v1/zones/layout", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::ZoneHandlers::handleLayout(request, data, len, m_zoneComposer, [this](){ broadcastZoneState(); });
        }
    );

    // Get single zone - GET /api/v1/zones/0, /api/v1/zones/1, etc.
    // Using regex-style path with parameter
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])$", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        webserver::handlers::ZoneHandlers::handleGet(request, m_actorSystem, m_renderer, m_zoneComposer);
    });

    // Set zone effect - POST /api/v1/zones/:id/effect
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/effect$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::ZoneHandlers::handleSetEffect(request, data, len, m_actorSystem, m_renderer, m_zoneComposer, [this](){ broadcastZoneState(); });
        }
    );

    // Set zone brightness - POST /api/v1/zones/:id/brightness
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/brightness$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::ZoneHandlers::handleSetBrightness(request, data, len, m_zoneComposer, [this](){ broadcastZoneState(); });
        }
    );

    // Set zone speed - POST /api/v1/zones/:id/speed
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/speed$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::ZoneHandlers::handleSetSpeed(request, data, len, m_zoneComposer, [this](){ broadcastZoneState(); });
        }
    );

    // Set zone palette - POST /api/v1/zones/:id/palette
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/palette$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::ZoneHandlers::handleSetPalette(request, data, len, m_zoneComposer, [this](){ broadcastZoneState(); });
        }
    );

    // Set zone blend mode - POST /api/v1/zones/:id/blend
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/blend$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::ZoneHandlers::handleSetBlend(request, data, len, m_zoneComposer, [this](){ broadcastZoneState(); });
        }
    );

    // Set zone enabled - POST /api/v1/zones/:id/enabled
    m_server->on("^\\/api\\/v1\\/zones\\/([0-3])\\/enabled$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            webserver::handlers::ZoneHandlers::handleSetEnabled(request, data, len, m_zoneComposer, [this](){ broadcastZoneState(); });
        }
    );
}

// ============================================================================
// Legacy API Handlers
// ============================================================================

void WebServer::handleLegacyStatus(AsyncWebServerRequest* request) {
    if (!m_actorSystem.isRunning()) {
        sendLegacyError(request, "System not ready", HttpStatus::SERVICE_UNAVAILABLE);
        return;
    }

    StaticJsonDocument<512> doc;
    RendererActor* renderer = m_renderer;

    doc["effect"] = renderer->getCurrentEffect();
    doc["brightness"] = renderer->getBrightness();
    doc["speed"] = renderer->getSpeed();
    doc["palette"] = renderer->getPaletteIndex();
    doc["freeHeap"] = ESP.getFreeHeap();

    // Network info
    JsonObject network = doc["network"].to<JsonObject>();
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
    StaticJsonDocument<512> doc;
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetEffect, request);

    uint8_t effectId = doc["effect"];
    if (effectId >= m_renderer->getEffectCount()) {
        sendLegacyError(request, "Invalid effect ID");
        return;
    }

    m_actorSystem.setEffect(effectId);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacySetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<512> doc;
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetBrightness, request);

    uint8_t brightness = doc["brightness"];
    m_actorSystem.setBrightness(brightness);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacySetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<512> doc;
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetSpeed, request);

    // Range is validated by schema (1-50)
    uint8_t speed = doc["speed"];
    m_actorSystem.setSpeed(speed);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacySetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<512> doc;
    VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetPalette, request);

    uint8_t paletteId = doc["paletteId"];
    m_actorSystem.setPalette(paletteId);
    sendLegacySuccess(request);
    broadcastStatus();
}

void WebServer::handleLegacyZoneCount(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!m_zoneComposer) {
        sendLegacyError(request, "Zone system not available");
        return;
    }

    StaticJsonDocument<512> doc;
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

    StaticJsonDocument<512> doc;
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
        JsonObject hw = data["hardware"].to<JsonObject>();
        hw["ledsTotal"] = LedConfig::TOTAL_LEDS;
        hw["strips"] = LedConfig::NUM_STRIPS;
        hw["centerPoint"] = LedConfig::CENTER_POINT;
        hw["maxZones"] = 4;

        // HATEOAS links
        JsonObject links = data["_links"].to<JsonObject>();
        links["self"] = "/api/v1/";
        links["device"] = "/api/v1/device/status";
        links["effects"] = "/api/v1/effects";
        links["parameters"] = "/api/v1/parameters";
        links["audioParameters"] = "/api/v1/audio/parameters";
        links["transitions"] = "/api/v1/transitions/types";
        links["batch"] = "/api/v1/batch";
        links["websocket"] = "ws://lightwaveos.local/ws";
    }, 1024);
}

void WebServer::handleParametersGet(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [this](JsonObject& data) {
        data["brightness"] = m_renderer->getBrightness();
        data["speed"] = m_renderer->getSpeed();
        data["paletteId"] = m_renderer->getPaletteIndex();
        data["hue"] = m_renderer->getHue();
        data["intensity"] = m_renderer->getIntensity();
        data["saturation"] = m_renderer->getSaturation();
        data["complexity"] = m_renderer->getComplexity();
        data["variation"] = m_renderer->getVariation();
    });
}

void WebServer::handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<512> doc;
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
        m_actorSystem.setBrightness(val);
        updated = true;
    }

    if (doc.containsKey("speed")) {
        uint8_t val = doc["speed"];
        // Range already validated by schema (1-50)
        m_actorSystem.setSpeed(val);
        updated = true;
    }

    if (doc.containsKey("paletteId")) {
        uint8_t val = doc["paletteId"];
        m_actorSystem.setPalette(val);
        updated = true;
    }

    if (doc.containsKey("intensity")) {
        uint8_t val = doc["intensity"];
        m_actorSystem.setIntensity(val);
        updated = true;
    }

    if (doc.containsKey("saturation")) {
        uint8_t val = doc["saturation"];
        m_actorSystem.setSaturation(val);
        updated = true;
    }

    if (doc.containsKey("complexity")) {
        uint8_t val = doc["complexity"];
        m_actorSystem.setComplexity(val);
        updated = true;
    }

    if (doc.containsKey("variation")) {
        uint8_t val = doc["variation"];
        m_actorSystem.setVariation(val);
        updated = true;
    }

    if (doc.containsKey("hue")) {
        uint8_t val = doc["hue"];
        m_actorSystem.setHue(val);
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

#if FEATURE_AUDIO_SYNC
void WebServer::handleAudioParametersGet(AsyncWebServerRequest* request) {
    auto* audio = m_actorSystem.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Audio system not available");
        return;
    }

    audio::AudioPipelineTuning pipeline = audio->getPipelineTuning();
    audio::AudioDspState state = audio->getDspState();
    audio::AudioContractTuning contract = m_renderer ? m_renderer->getAudioContractTuning()
                                                     : audio::clampAudioContractTuning(audio::AudioContractTuning{});

    sendSuccessResponse(request, [&](JsonObject& data) {
        JsonObject pipelineObj = data["pipeline"].to<JsonObject>();
        pipelineObj["dcAlpha"] = pipeline.dcAlpha;
        pipelineObj["agcTargetRms"] = pipeline.agcTargetRms;
        pipelineObj["agcMinGain"] = pipeline.agcMinGain;
        pipelineObj["agcMaxGain"] = pipeline.agcMaxGain;
        pipelineObj["agcAttack"] = pipeline.agcAttack;
        pipelineObj["agcRelease"] = pipeline.agcRelease;
        pipelineObj["agcClipReduce"] = pipeline.agcClipReduce;
        pipelineObj["agcIdleReturnRate"] = pipeline.agcIdleReturnRate;
        pipelineObj["noiseFloorMin"] = pipeline.noiseFloorMin;
        pipelineObj["noiseFloorRise"] = pipeline.noiseFloorRise;
        pipelineObj["noiseFloorFall"] = pipeline.noiseFloorFall;
        pipelineObj["gateStartFactor"] = pipeline.gateStartFactor;
        pipelineObj["gateRangeFactor"] = pipeline.gateRangeFactor;
        pipelineObj["gateRangeMin"] = pipeline.gateRangeMin;
        pipelineObj["rmsDbFloor"] = pipeline.rmsDbFloor;
        pipelineObj["rmsDbCeil"] = pipeline.rmsDbCeil;
        pipelineObj["bandDbFloor"] = pipeline.bandDbFloor;
        pipelineObj["bandDbCeil"] = pipeline.bandDbCeil;
        pipelineObj["chromaDbFloor"] = pipeline.chromaDbFloor;
        pipelineObj["chromaDbCeil"] = pipeline.chromaDbCeil;
        pipelineObj["fluxScale"] = pipeline.fluxScale;

        JsonObject controlBus = data["controlBus"].to<JsonObject>();
        controlBus["alphaFast"] = pipeline.controlBusAlphaFast;
        controlBus["alphaSlow"] = pipeline.controlBusAlphaSlow;

        JsonObject contractObj = data["contract"].to<JsonObject>();
        contractObj["audioStalenessMs"] = contract.audioStalenessMs;
        contractObj["bpmMin"] = contract.bpmMin;
        contractObj["bpmMax"] = contract.bpmMax;
        contractObj["bpmTau"] = contract.bpmTau;
        contractObj["confidenceTau"] = contract.confidenceTau;
        contractObj["phaseCorrectionGain"] = contract.phaseCorrectionGain;
        contractObj["barCorrectionGain"] = contract.barCorrectionGain;
        contractObj["beatsPerBar"] = contract.beatsPerBar;
        contractObj["beatUnit"] = contract.beatUnit;

        JsonObject stateObj = data["state"].to<JsonObject>();
        stateObj["rmsRaw"] = state.rmsRaw;
        stateObj["rmsMapped"] = state.rmsMapped;
        stateObj["rmsPreGain"] = state.rmsPreGain;
        stateObj["fluxMapped"] = state.fluxMapped;
        stateObj["agcGain"] = state.agcGain;
        stateObj["dcEstimate"] = state.dcEstimate;
        stateObj["noiseFloor"] = state.noiseFloor;
        stateObj["minSample"] = state.minSample;
        stateObj["maxSample"] = state.maxSample;
        stateObj["peakCentered"] = state.peakCentered;
        stateObj["meanSample"] = state.meanSample;
        stateObj["clipCount"] = state.clipCount;

        JsonObject caps = data["capabilities"].to<JsonObject>();
        caps["sampleRate"] = audio::SAMPLE_RATE;
        caps["hopSize"] = audio::HOP_SIZE;
        caps["fftSize"] = audio::FFT_SIZE;
        caps["goertzelWindow"] = audio::GOERTZEL_WINDOW;
        caps["bandCount"] = audio::NUM_BANDS;
        caps["chromaCount"] = audio::CONTROLBUS_NUM_CHROMA;
        caps["waveformPoints"] = audio::CONTROLBUS_WAVEFORM_N;
    });
}

void WebServer::handleAudioParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    auto* audio = m_actorSystem.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Audio system not available");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON payload");
        return;
    }

    bool updatedPipeline = false;
    bool updatedContract = false;
    bool resetState = false;

    audio::AudioPipelineTuning pipeline = audio->getPipelineTuning();
    audio::AudioContractTuning contract = m_renderer ? m_renderer->getAudioContractTuning()
                                                     : audio::clampAudioContractTuning(audio::AudioContractTuning{});

    auto applyFloat = [](JsonVariant source, const char* key, float& target, bool& updated) {
        if (!source.is<JsonObject>()) return;
        if (source.containsKey(key)) {
            target = source[key].as<float>();
            updated = true;
        }
    };

    auto applyUint8 = [](JsonVariant source, const char* key, uint8_t& target, bool& updated) {
        if (!source.is<JsonObject>()) return;
        if (source.containsKey(key)) {
            target = source[key].as<uint8_t>();
            updated = true;
        }
    };

    JsonVariant pipelineSrc = doc.as<JsonVariant>();
    if (doc.containsKey("pipeline")) {
        pipelineSrc = doc["pipeline"];
    }
    applyFloat(pipelineSrc, "dcAlpha", pipeline.dcAlpha, updatedPipeline);
    applyFloat(pipelineSrc, "agcTargetRms", pipeline.agcTargetRms, updatedPipeline);
    applyFloat(pipelineSrc, "agcMinGain", pipeline.agcMinGain, updatedPipeline);
    applyFloat(pipelineSrc, "agcMaxGain", pipeline.agcMaxGain, updatedPipeline);
    applyFloat(pipelineSrc, "agcAttack", pipeline.agcAttack, updatedPipeline);
    applyFloat(pipelineSrc, "agcRelease", pipeline.agcRelease, updatedPipeline);
    applyFloat(pipelineSrc, "agcClipReduce", pipeline.agcClipReduce, updatedPipeline);
    applyFloat(pipelineSrc, "agcIdleReturnRate", pipeline.agcIdleReturnRate, updatedPipeline);
    applyFloat(pipelineSrc, "noiseFloorMin", pipeline.noiseFloorMin, updatedPipeline);
    applyFloat(pipelineSrc, "noiseFloorRise", pipeline.noiseFloorRise, updatedPipeline);
    applyFloat(pipelineSrc, "noiseFloorFall", pipeline.noiseFloorFall, updatedPipeline);
    applyFloat(pipelineSrc, "gateStartFactor", pipeline.gateStartFactor, updatedPipeline);
    applyFloat(pipelineSrc, "gateRangeFactor", pipeline.gateRangeFactor, updatedPipeline);
    applyFloat(pipelineSrc, "gateRangeMin", pipeline.gateRangeMin, updatedPipeline);
    applyFloat(pipelineSrc, "rmsDbFloor", pipeline.rmsDbFloor, updatedPipeline);
    applyFloat(pipelineSrc, "rmsDbCeil", pipeline.rmsDbCeil, updatedPipeline);
    applyFloat(pipelineSrc, "bandDbFloor", pipeline.bandDbFloor, updatedPipeline);
    applyFloat(pipelineSrc, "bandDbCeil", pipeline.bandDbCeil, updatedPipeline);
    applyFloat(pipelineSrc, "chromaDbFloor", pipeline.chromaDbFloor, updatedPipeline);
    applyFloat(pipelineSrc, "chromaDbCeil", pipeline.chromaDbCeil, updatedPipeline);
    applyFloat(pipelineSrc, "fluxScale", pipeline.fluxScale, updatedPipeline);

    JsonVariant controlBusSrc = doc.as<JsonVariant>();
    if (doc.containsKey("controlBus")) {
        controlBusSrc = doc["controlBus"];
    }
    applyFloat(controlBusSrc, "alphaFast", pipeline.controlBusAlphaFast, updatedPipeline);
    applyFloat(controlBusSrc, "alphaSlow", pipeline.controlBusAlphaSlow, updatedPipeline);

    JsonVariant contractSrc = doc.as<JsonVariant>();
    if (doc.containsKey("contract")) {
        contractSrc = doc["contract"];
    }
    applyFloat(contractSrc, "audioStalenessMs", contract.audioStalenessMs, updatedContract);
    applyFloat(contractSrc, "bpmMin", contract.bpmMin, updatedContract);
    applyFloat(contractSrc, "bpmMax", contract.bpmMax, updatedContract);
    applyFloat(contractSrc, "bpmTau", contract.bpmTau, updatedContract);
    applyFloat(contractSrc, "confidenceTau", contract.confidenceTau, updatedContract);
    applyFloat(contractSrc, "phaseCorrectionGain", contract.phaseCorrectionGain, updatedContract);
    applyFloat(contractSrc, "barCorrectionGain", contract.barCorrectionGain, updatedContract);
    applyUint8(contractSrc, "beatsPerBar", contract.beatsPerBar, updatedContract);
    applyUint8(contractSrc, "beatUnit", contract.beatUnit, updatedContract);

    if (doc.containsKey("resetState")) {
        resetState = doc["resetState"] | false;
    }

    if (updatedPipeline) {
        audio->setPipelineTuning(pipeline);
    }
    if (updatedContract && m_renderer) {
        m_renderer->setAudioContractTuning(contract);
    }
    if (resetState) {
        audio->resetDspState();
    }

    sendSuccessResponse(request, [updatedPipeline, updatedContract, resetState](JsonObject& resp) {
        JsonArray updated = resp["updated"].to<JsonArray>();
        if (updatedPipeline) updated.add("pipeline");
        if (updatedContract) updated.add("contract");
        if (resetState) updated.add("state");
    });
}
#else
void WebServer::handleAudioParametersGet(AsyncWebServerRequest* request) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void WebServer::handleAudioParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}
#endif

void WebServer::handleTransitionTypes(AsyncWebServerRequest* request) {
    sendSuccessResponseLarge(request, [](JsonObject& data) {
        JsonArray types = data["types"].to<JsonArray>();

        for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
            JsonObject t = types.add<JsonObject>();
            t["id"] = i;
            t["name"] = getTransitionName(static_cast<TransitionType>(i));
            t["duration"] = getDefaultDuration(static_cast<TransitionType>(i));
            t["centerOrigin"] = true;  // All v2 transitions are center-origin
        }
    }, 1536);
}

void WebServer::handleTransitionTrigger(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<512> doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::TriggerTransition, request);

    uint8_t toEffect = doc["toEffect"];
    if (toEffect >= m_renderer->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "toEffect");
        return;
    }

    uint8_t transitionType = doc["type"] | 0;
    bool randomType = doc["random"] | false;

    if (randomType) {
        m_renderer->startRandomTransition(toEffect);
    } else {
        m_renderer->startTransition(toEffect, transitionType);
    }

    sendSuccessResponse(request, [this, toEffect, transitionType](JsonObject& respData) {
        respData["effectId"] = toEffect;
        respData["name"] = m_renderer->getEffectName(toEffect);
        respData["transitionType"] = transitionType;
    });

    broadcastStatus();
}

void WebServer::handleBatch(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
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

bool WebServer::executeBatchAction(const String& action, JsonVariant params) {
    if (action == "setBrightness") {
        if (!params.containsKey("value")) return false;
        m_actorSystem.setBrightness(params["value"]);
        return true;
    }
    else if (action == "setSpeed") {
        if (!params.containsKey("value")) return false;
        uint8_t val = params["value"];
        if (val < 1 || val > 50) return false;
        m_actorSystem.setSpeed(val);
        return true;
    }
    else if (action == "setEffect") {
        if (!params.containsKey("effectId")) return false;
        uint8_t id = params["effectId"];
        if (id >= m_renderer->getEffectCount()) return false;
        m_actorSystem.setEffect(id);
        return true;
    }
    else if (action == "setPalette") {
        if (!params.containsKey("paletteId")) return false;
        m_actorSystem.setPalette(params["paletteId"]);
        return true;
    }
    else if (action == "transition") {
        if (!params.containsKey("toEffect")) return false;
        uint8_t toEffect = params["toEffect"];
        uint8_t type = params["type"] | 0;
        m_renderer->startTransition(toEffect, type);
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
    if (!m_renderer) {
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
    StaticJsonDocument<512> doc;
    doc["success"] = true;
    doc["timestamp"] = millis();
    doc["version"] = API_VERSION;

    JsonObject data = doc["data"].to<JsonObject>();

    // Categorize palettes (static counts, not affected by filters)
    JsonObject categories = data["categories"].to<JsonObject>();
    categories["artistic"] = CPT_CITY_END - CPT_CITY_START + 1;
    categories["scientific"] = CRAMERI_END - CRAMERI_START + 1;
    categories["lgpOptimized"] = COLORSPACE_END - COLORSPACE_START + 1;

    JsonArray palettes = data["palettes"].to<JsonArray>();

    // Second pass: add only the paginated subset of filtered palettes
    for (uint16_t idx = startIdx; idx < endIdx; idx++) {
        uint8_t i = filteredIds[idx];

        JsonObject palette = palettes.add<JsonObject>();
        palette["id"] = i;
        palette["name"] = MasterPaletteNames[i];
        palette["category"] = getPaletteCategory(i);

        // Flags
        JsonObject flags = palette["flags"].to<JsonObject>();
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
    JsonObject pagination = data["pagination"].to<JsonObject>();
    pagination["page"] = page;
    pagination["limit"] = limit;
    pagination["total"] = filteredCount;
    pagination["pages"] = totalPages;

    String output;
    serializeJson(doc, output);
    request->send(HttpStatus::OK, "application/json", output);
}

void WebServer::handlePalettesCurrent(AsyncWebServerRequest* request) {
    if (!m_renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    sendSuccessResponse(request, [this](JsonObject& data) {
        uint8_t id = m_renderer->getPaletteIndex();
        data["paletteId"] = id;
        data["name"] = MasterPaletteNames[id];
        data["category"] = getPaletteCategory(id);

        JsonObject flags = data["flags"].to<JsonObject>();
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
    StaticJsonDocument<512> doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::SetPalette, request);

    uint8_t paletteId = doc["paletteId"];
    if (paletteId >= MASTER_PALETTE_COUNT) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Palette ID out of range (0-74)", "paletteId");
        return;
    }

    m_actorSystem.setPalette(paletteId);

    sendSuccessResponse(request, [paletteId](JsonObject& respData) {
        respData["paletteId"] = paletteId;
        respData["name"] = MasterPaletteNames[paletteId];
        respData["category"] = getPaletteCategory(paletteId);
    });

    broadcastStatus();
}



// ============================================================================
// Transition Config Handlers
// ============================================================================

void WebServer::handleTransitionConfigGet(AsyncWebServerRequest* request) {
    if (!m_renderer) {
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
        JsonArray easings = data["easings"].to<JsonArray>();
        const char* easingNames[] = {
            "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
            "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
            "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
        };
        for (uint8_t i = 0; i < 10; i++) {
            JsonObject easing = easings.add<JsonObject>();
            easing["id"] = i;
            easing["name"] = easingNames[i];
        }
    });
}

void WebServer::handleTransitionConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<512> doc;
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
        JsonObject durations = data["durations"].to<JsonObject>();
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
        JsonObject durations = data["durations"].to<JsonObject>();
        durations["build"] = narrative.getBuildDuration();
        durations["hold"] = narrative.getHoldDuration();
        durations["release"] = narrative.getReleaseDuration();
        durations["rest"] = narrative.getRestDuration();
        durations["total"] = narrative.getTotalDuration();
        
        // Curves
        JsonObject curves = data["curves"].to<JsonObject>();
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
    StaticJsonDocument<512> doc;

    doc["openapi"] = "3.0.3";

    JsonObject info = doc["info"].to<JsonObject>();
    info["title"] = "LightwaveOS API";
    info["version"] = "2.0.0";
    info["description"] = "REST API for LightwaveOS ESP32-S3 LED Control System";

    JsonArray servers = doc["servers"].to<JsonArray>();
    JsonObject server = servers.add<JsonObject>();
    server["url"] = "http://lightwaveos.local/api/v1";
    server["description"] = "Local device";

    JsonObject paths = doc["paths"].to<JsonObject>();

    // ========== Device Endpoints ==========

    // GET /device/status
    JsonObject deviceStatus = paths["/device/status"].to<JsonObject>();
    JsonObject getDeviceStatus = deviceStatus["get"].to<JsonObject>();
    getDeviceStatus["summary"] = "Get device status (uptime, heap, FPS)";
    getDeviceStatus["tags"].add("Device");

    // GET /device/info
    JsonObject deviceInfo = paths["/device/info"].to<JsonObject>();
    JsonObject getDeviceInfo = deviceInfo["get"].to<JsonObject>();
    getDeviceInfo["summary"] = "Get device info (firmware, hardware, SDK)";
    getDeviceInfo["tags"].add("Device");

    // ========== Effects Endpoints ==========

    // GET /effects
    JsonObject effects = paths["/effects"].to<JsonObject>();
    JsonObject getEffects = effects["get"].to<JsonObject>();
    getEffects["summary"] = "List effects with pagination";
    getEffects["tags"].add("Effects");
    JsonArray effectsParams = getEffects["parameters"].to<JsonArray>();
    JsonObject pageParam = effectsParams.add<JsonObject>();
    pageParam["name"] = "page";
    pageParam["in"] = "query";
    pageParam["schema"]["type"] = "integer";
    JsonObject limitParam = effectsParams.add<JsonObject>();
    limitParam["name"] = "limit";
    limitParam["in"] = "query";
    limitParam["schema"]["type"] = "integer";
    JsonObject categoryParam = effectsParams.add<JsonObject>();
    categoryParam["name"] = "category";
    categoryParam["in"] = "query";
    categoryParam["schema"]["type"] = "string";
    JsonObject detailsParam = effectsParams.add<JsonObject>();
    detailsParam["name"] = "details";
    detailsParam["in"] = "query";
    detailsParam["schema"]["type"] = "boolean";

    // GET /effects/current
    JsonObject effectsCurrent = paths["/effects/current"].to<JsonObject>();
    JsonObject getEffectsCurrent = effectsCurrent["get"].to<JsonObject>();
    getEffectsCurrent["summary"] = "Get current effect state";
    getEffectsCurrent["tags"].add("Effects");

    // POST /effects/set
    JsonObject effectsSet = paths["/effects/set"].to<JsonObject>();
    JsonObject postEffectsSet = effectsSet["post"].to<JsonObject>();
    postEffectsSet["summary"] = "Set current effect by ID";
    postEffectsSet["tags"].add("Effects");

    // GET /effects/metadata
    JsonObject effectsMeta = paths["/effects/metadata"].to<JsonObject>();
    JsonObject getEffectsMeta = effectsMeta["get"].to<JsonObject>();
    getEffectsMeta["summary"] = "Get effect metadata by ID";
    getEffectsMeta["tags"].add("Effects");
    JsonArray metaParams = getEffectsMeta["parameters"].to<JsonArray>();
    JsonObject idParam = metaParams.add<JsonObject>();
    idParam["name"] = "id";
    idParam["in"] = "query";
    idParam["required"] = true;
    idParam["schema"]["type"] = "integer";

    // GET/POST /effects/parameters
    JsonObject effectsParametersPath = paths["/effects/parameters"].to<JsonObject>();
    JsonObject getEffectsParams = effectsParametersPath["get"].to<JsonObject>();
    getEffectsParams["summary"] = "Get effect parameter schema and values";
    getEffectsParams["tags"].add("Effects");
    JsonArray effectParamQuery = getEffectsParams["parameters"].to<JsonArray>();
    JsonObject effectIdParam = effectParamQuery.add<JsonObject>();
    effectIdParam["name"] = "id";
    effectIdParam["in"] = "query";
    effectIdParam["required"] = false;
    effectIdParam["schema"]["type"] = "integer";

    JsonObject postEffectsParams = effectsParametersPath["post"].to<JsonObject>();
    postEffectsParams["summary"] = "Update effect parameters";
    postEffectsParams["tags"].add("Effects");

    // GET /effects/families
    JsonObject effectsFamilies = paths["/effects/families"].to<JsonObject>();
    JsonObject getEffectsFamilies = effectsFamilies["get"].to<JsonObject>();
    getEffectsFamilies["summary"] = "List effect families/categories";
    getEffectsFamilies["tags"].add("Effects");

    // ========== Palettes Endpoints ==========

    // GET /palettes
    JsonObject palettes = paths["/palettes"].to<JsonObject>();
    JsonObject getPalettes = palettes["get"].to<JsonObject>();
    getPalettes["summary"] = "List palettes with pagination";
    getPalettes["tags"].add("Palettes");
    JsonArray paletteParams = getPalettes["parameters"].to<JsonArray>();
    JsonObject palPageParam = paletteParams.add<JsonObject>();
    palPageParam["name"] = "page";
    palPageParam["in"] = "query";
    palPageParam["schema"]["type"] = "integer";
    JsonObject palLimitParam = paletteParams.add<JsonObject>();
    palLimitParam["name"] = "limit";
    palLimitParam["in"] = "query";
    palLimitParam["schema"]["type"] = "integer";

    // GET /palettes/current
    JsonObject palettesCurrent = paths["/palettes/current"].to<JsonObject>();
    JsonObject getPalettesCurrent = palettesCurrent["get"].to<JsonObject>();
    getPalettesCurrent["summary"] = "Get current palette";
    getPalettesCurrent["tags"].add("Palettes");

    // POST /palettes/set
    JsonObject palettesSet = paths["/palettes/set"].to<JsonObject>();
    JsonObject postPalettesSet = palettesSet["post"].to<JsonObject>();
    postPalettesSet["summary"] = "Set palette by ID";
    postPalettesSet["tags"].add("Palettes");

    // ========== Parameters Endpoints ==========

    // GET /parameters
    JsonObject parameters = paths["/parameters"].to<JsonObject>();
    JsonObject getParams = parameters["get"].to<JsonObject>();
    getParams["summary"] = "Get visual parameters";
    getParams["tags"].add("Parameters");

    // POST /parameters
    JsonObject postParams = parameters["post"].to<JsonObject>();
    postParams["summary"] = "Update visual parameters (brightness, speed, etc.)";
    postParams["tags"].add("Parameters");

    // ========== Audio Parameters Endpoints ==========
    JsonObject audioParams = paths["/audio/parameters"].to<JsonObject>();
    JsonObject getAudioParams = audioParams["get"].to<JsonObject>();
    getAudioParams["summary"] = "Get audio pipeline and contract parameters";
    getAudioParams["tags"].add("Audio");

    JsonObject postAudioParams = audioParams["post"].to<JsonObject>();
    postAudioParams["summary"] = "Update audio pipeline and contract parameters";
    postAudioParams["tags"].add("Audio");

    // ========== Transitions Endpoints ==========

    // GET /transitions/types
    JsonObject transTypes = paths["/transitions/types"].to<JsonObject>();
    JsonObject getTransTypes = transTypes["get"].to<JsonObject>();
    getTransTypes["summary"] = "List transition types";
    getTransTypes["tags"].add("Transitions");

    // GET /transitions/config
    JsonObject transConfig = paths["/transitions/config"].to<JsonObject>();
    JsonObject getTransConfig = transConfig["get"].to<JsonObject>();
    getTransConfig["summary"] = "Get transition configuration";
    getTransConfig["tags"].add("Transitions");

    // POST /transitions/config
    JsonObject postTransConfig = transConfig["post"].to<JsonObject>();
    postTransConfig["summary"] = "Update transition configuration";
    postTransConfig["tags"].add("Transitions");

    // POST /transitions/trigger
    JsonObject transTrigger = paths["/transitions/trigger"].to<JsonObject>();
    JsonObject postTransTrigger = transTrigger["post"].to<JsonObject>();
    postTransTrigger["summary"] = "Trigger a transition";
    postTransTrigger["tags"].add("Transitions");

    // ========== Narrative Endpoints ==========

    // GET /narrative/status
    JsonObject narrativeStatus = paths["/narrative/status"].to<JsonObject>();
    JsonObject getNarrativeStatus = narrativeStatus["get"].to<JsonObject>();
    getNarrativeStatus["summary"] = "Get narrative engine status";
    getNarrativeStatus["tags"].add("Narrative");

    // GET /narrative/config
    JsonObject narrativeConfig = paths["/narrative/config"].to<JsonObject>();
    JsonObject getNarrativeConfig = narrativeConfig["get"].to<JsonObject>();
    getNarrativeConfig["summary"] = "Get narrative configuration";
    getNarrativeConfig["tags"].add("Narrative");

    // POST /narrative/config
    JsonObject postNarrativeConfig = narrativeConfig["post"].to<JsonObject>();
    postNarrativeConfig["summary"] = "Update narrative configuration";
    postNarrativeConfig["tags"].add("Narrative");

    // ========== Zones Endpoints ==========

    // GET /zones
    JsonObject zones = paths["/zones"].to<JsonObject>();
    JsonObject getZones = zones["get"].to<JsonObject>();
    getZones["summary"] = "List all zones with configuration";
    getZones["tags"].add("Zones");

    // POST /zones/layout
    JsonObject zonesLayout = paths["/zones/layout"].to<JsonObject>();
    JsonObject postZonesLayout = zonesLayout["post"].to<JsonObject>();
    postZonesLayout["summary"] = "Set zone layout configuration";
    postZonesLayout["tags"].add("Zones");

    // ========== Batch Endpoints ==========

    // POST /batch
    JsonObject batch = paths["/batch"].to<JsonObject>();
    JsonObject postBatch = batch["post"].to<JsonObject>();
    postBatch["summary"] = "Execute batch operations (max 10)";
    postBatch["tags"].add("Batch");

    // ========== Sync Endpoints ==========

    // GET /sync/status
    JsonObject syncStatus = paths["/sync/status"].to<JsonObject>();
    JsonObject getSyncStatus = syncStatus["get"].to<JsonObject>();
    getSyncStatus["summary"] = "Get multi-device sync status";
    getSyncStatus["tags"].add("Sync");

    String output;
    serializeJson(doc, output);
    request->send(HttpStatus::OK, "application/json", output);
}

// ============================================================================
// WebSocket Handlers
// ============================================================================

void WebServer::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            if (webServerInstance) webServerInstance->handleWsConnect(client);
            break;

        case WS_EVT_DISCONNECT:
            if (webServerInstance) webServerInstance->handleWsDisconnect(client);
            break;

        case WS_EVT_DATA:
            if (webServerInstance) webServerInstance->handleWsMessage(client, data, len);
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
    // Ensure stale client entries are purged before applying connection limits.
    m_ws->cleanupClients();
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

    StaticJsonDocument<512> doc;
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
        if (effectId < m_renderer->getEffectCount()) {
            m_actorSystem.setEffect(effectId);
            broadcastStatus();
        }
    }
    else if (type == "nextEffect") {
        uint8_t current = m_renderer->getCurrentEffect();
        uint8_t next = (current + 1) % m_renderer->getEffectCount();
        m_actorSystem.setEffect(next);
        broadcastStatus();
    }
    else if (type == "prevEffect") {
        uint8_t current = m_renderer->getCurrentEffect();
        uint8_t prev = (current + m_renderer->getEffectCount() - 1) % m_renderer->getEffectCount();
        m_actorSystem.setEffect(prev);
        broadcastStatus();
    }

    // Parameter control
    else if (type == "setBrightness") {
        uint8_t value = doc["value"] | 128;
        m_actorSystem.setBrightness(value);
        broadcastStatus();
    }
    else if (type == "setSpeed") {
        uint8_t value = doc["value"] | 15;
        if (value >= 1 && value <= 50) {
            m_actorSystem.setSpeed(value);
            broadcastStatus();
        }
    }
    else if (type == "setPalette") {
        uint8_t paletteId = doc["paletteId"] | 0;
        m_actorSystem.setPalette(paletteId);
        broadcastStatus();
    }

    // Transition control
    else if (type == "transition.trigger") {
        uint8_t toEffect = doc["toEffect"] | 0;
        uint8_t transType = doc["transitionType"] | 0;
        bool random = doc["random"] | false;

        if (toEffect < m_renderer->getEffectCount()) {
            if (random) {
                m_renderer->startRandomTransition(toEffect);
            } else {
                m_renderer->startTransition(toEffect, transType);
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
                data["frameSize"] = webserver::LedStreamConfig::FRAME_SIZE;
                data["frameVersion"] = webserver::LedStreamConfig::FRAME_VERSION;
                data["numStrips"] = webserver::LedStreamConfig::NUM_STRIPS;
                data["ledsPerStrip"] = webserver::LedStreamConfig::LEDS_PER_STRIP;
                data["targetFps"] = webserver::LedStreamConfig::TARGET_FPS;
                data["magicByte"] = webserver::LedStreamConfig::MAGIC_BYTE;
                data["accepted"] = true;
            });
            client->text(response);
            Serial.printf("[WebSocket] Client %u subscribed to LED stream (v%d, %d bytes)\n", 
                         clientId, webserver::LedStreamConfig::FRAME_VERSION, webserver::LedStreamConfig::FRAME_SIZE);
        } else {
            // Manually build rejection response to set success=false
            JsonDocument response;
            response["type"] = "ledStream.rejected";
            if (requestId != nullptr && strlen(requestId) > 0) {
                response["requestId"] = requestId;
            }
            response["success"] = false;
            JsonObject error = response["error"].to<JsonObject>();
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
        String response = buildWsResponse("effects.current", requestId, [this](JsonObject& data) {
            data["effectId"] = m_renderer->getCurrentEffect();
            data["name"] = m_renderer->getEffectName(m_renderer->getCurrentEffect());
            data["brightness"] = m_renderer->getBrightness();
            data["speed"] = m_renderer->getSpeed();
            data["paletteId"] = m_renderer->getPaletteIndex();
            data["hue"] = m_renderer->getHue();
            data["intensity"] = m_renderer->getIntensity();
            data["saturation"] = m_renderer->getSaturation();
            data["complexity"] = m_renderer->getComplexity();
            data["variation"] = m_renderer->getVariation();
        });
        client->text(response);
    }
    else if (type == "parameters.get") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("parameters", requestId, [this](JsonObject& data) {
            data["brightness"] = m_renderer->getBrightness();
            data["speed"] = m_renderer->getSpeed();
            data["paletteId"] = m_renderer->getPaletteIndex();
            data["hue"] = m_renderer->getHue();
            data["intensity"] = m_renderer->getIntensity();
            data["saturation"] = m_renderer->getSaturation();
            data["complexity"] = m_renderer->getComplexity();
            data["variation"] = m_renderer->getVariation();
        });
        client->text(response);
    }
#if FEATURE_AUDIO_SYNC
    else if (type == "audio.parameters.get") {
        const char* requestId = doc["requestId"] | "";
        auto* audio = m_actorSystem.getAudio();
        if (!audio) {
            client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
            return;
        }

        audio::AudioPipelineTuning pipeline = audio->getPipelineTuning();
        audio::AudioDspState state = audio->getDspState();
        audio::AudioContractTuning contract = m_renderer ? m_renderer->getAudioContractTuning()
                                                         : audio::clampAudioContractTuning(audio::AudioContractTuning{});

        String response = buildWsResponse("audio.parameters", requestId, [&](JsonObject& data) {
            JsonObject pipelineObj = data["pipeline"].to<JsonObject>();
            pipelineObj["dcAlpha"] = pipeline.dcAlpha;
            pipelineObj["agcTargetRms"] = pipeline.agcTargetRms;
            pipelineObj["agcMinGain"] = pipeline.agcMinGain;
            pipelineObj["agcMaxGain"] = pipeline.agcMaxGain;
            pipelineObj["agcAttack"] = pipeline.agcAttack;
            pipelineObj["agcRelease"] = pipeline.agcRelease;
            pipelineObj["agcClipReduce"] = pipeline.agcClipReduce;
            pipelineObj["agcIdleReturnRate"] = pipeline.agcIdleReturnRate;
            pipelineObj["noiseFloorMin"] = pipeline.noiseFloorMin;
            pipelineObj["noiseFloorRise"] = pipeline.noiseFloorRise;
            pipelineObj["noiseFloorFall"] = pipeline.noiseFloorFall;
            pipelineObj["gateStartFactor"] = pipeline.gateStartFactor;
            pipelineObj["gateRangeFactor"] = pipeline.gateRangeFactor;
            pipelineObj["gateRangeMin"] = pipeline.gateRangeMin;
            pipelineObj["rmsDbFloor"] = pipeline.rmsDbFloor;
            pipelineObj["rmsDbCeil"] = pipeline.rmsDbCeil;
            pipelineObj["bandDbFloor"] = pipeline.bandDbFloor;
            pipelineObj["bandDbCeil"] = pipeline.bandDbCeil;
            pipelineObj["chromaDbFloor"] = pipeline.chromaDbFloor;
            pipelineObj["chromaDbCeil"] = pipeline.chromaDbCeil;
            pipelineObj["fluxScale"] = pipeline.fluxScale;

            JsonObject controlBus = data["controlBus"].to<JsonObject>();
            controlBus["alphaFast"] = pipeline.controlBusAlphaFast;
            controlBus["alphaSlow"] = pipeline.controlBusAlphaSlow;

            JsonObject contractObj = data["contract"].to<JsonObject>();
            contractObj["audioStalenessMs"] = contract.audioStalenessMs;
            contractObj["bpmMin"] = contract.bpmMin;
            contractObj["bpmMax"] = contract.bpmMax;
            contractObj["bpmTau"] = contract.bpmTau;
            contractObj["confidenceTau"] = contract.confidenceTau;
            contractObj["phaseCorrectionGain"] = contract.phaseCorrectionGain;
            contractObj["barCorrectionGain"] = contract.barCorrectionGain;
            contractObj["beatsPerBar"] = contract.beatsPerBar;
            contractObj["beatUnit"] = contract.beatUnit;

            JsonObject stateObj = data["state"].to<JsonObject>();
            stateObj["rmsRaw"] = state.rmsRaw;
            stateObj["rmsMapped"] = state.rmsMapped;
            stateObj["rmsPreGain"] = state.rmsPreGain;
            stateObj["fluxMapped"] = state.fluxMapped;
            stateObj["agcGain"] = state.agcGain;
            stateObj["dcEstimate"] = state.dcEstimate;
            stateObj["noiseFloor"] = state.noiseFloor;
            stateObj["minSample"] = state.minSample;
            stateObj["maxSample"] = state.maxSample;
            stateObj["peakCentered"] = state.peakCentered;
            stateObj["meanSample"] = state.meanSample;
            stateObj["clipCount"] = state.clipCount;

            JsonObject caps = data["capabilities"].to<JsonObject>();
            caps["sampleRate"] = audio::SAMPLE_RATE;
            caps["hopSize"] = audio::HOP_SIZE;
            caps["fftSize"] = audio::FFT_SIZE;
            caps["goertzelWindow"] = audio::GOERTZEL_WINDOW;
            caps["bandCount"] = audio::NUM_BANDS;
            caps["chromaCount"] = audio::CONTROLBUS_NUM_CHROMA;
            caps["waveformPoints"] = audio::CONTROLBUS_WAVEFORM_N;
        });
        client->text(response);
    }
#endif

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
            const RenderStats& stats = m_renderer->getStats();
            data["fps"] = stats.currentFPS;
            data["cpuPercent"] = stats.cpuPercent;
            data["framesRendered"] = stats.framesRendered;

            // Network info
            JsonObject network = data["network"].to<JsonObject>();
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

        if (effectId == 255 || effectId >= m_renderer->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
            return;
        }

        String response = buildWsResponse("effects.metadata", requestId, [this, effectId](JsonObject& data) {
            data["id"] = effectId;
            data["name"] = m_renderer->getEffectName(effectId);

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
                JsonArray tags = data["tags"].to<JsonArray>();
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
            JsonObject properties = data["properties"].to<JsonObject>();
            properties["centerOrigin"] = true;
            properties["symmetricStrips"] = true;
            properties["paletteAware"] = true;
            properties["speedResponsive"] = true;

            // Recommended settings
            JsonObject recommended = data["recommended"].to<JsonObject>();
            recommended["brightness"] = 180;
            recommended["speed"] = 15;
        });
        client->text(response);
    }
    else if (type == "effects.parameters.get") {
        const char* requestId = doc["requestId"] | "";
        uint8_t effectId = doc["effectId"] | m_renderer->getCurrentEffect();

        if (effectId >= m_renderer->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
            return;
        }

        plugins::IEffect* effect = m_renderer->getEffectInstance(effectId);
        String response = buildWsResponse("effects.parameters", requestId, [this, effectId, effect](JsonObject& data) {
            data["effectId"] = effectId;
            data["name"] = m_renderer->getEffectName(effectId);
            data["hasParameters"] = (effect != nullptr && effect->getParameterCount() > 0);

            JsonArray params = data["parameters"].to<JsonArray>();
            if (!effect) {
                return;
            }

            uint8_t count = effect->getParameterCount();
            for (uint8_t i = 0; i < count; ++i) {
                const plugins::EffectParameter* param = effect->getParameter(i);
                if (!param) continue;
                JsonObject p = params.add<JsonObject>();
                p["name"] = param->name;
                p["displayName"] = param->displayName;
                p["min"] = param->minValue;
                p["max"] = param->maxValue;
                p["default"] = param->defaultValue;
                p["value"] = effect->getParameter(param->name);
            }
        });
        client->text(response);
    }

    // effects.getCategories - Effect categories list (now returns Pattern Registry families)
    else if (type == "effects.getCategories") {
        const char* requestId = doc["requestId"] | "";
        String response = buildWsResponse("effects.categories", requestId, [](JsonObject& data) {
            JsonArray families = data["categories"].to<JsonArray>();
            
            // Return all 10 pattern families from registry
            for (uint8_t i = 0; i < 10; i++) {
                PatternFamily family = static_cast<PatternFamily>(i);
                uint8_t count = PatternRegistry::getFamilyCount(family);
                
                JsonObject familyObj = families.add<JsonObject>();
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
            
            JsonArray effects = data["effects"].to<JsonArray>();
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
            JsonArray types = data["types"].to<JsonArray>();

            for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
                JsonObject type = types.add<JsonObject>();
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
            JsonArray easings = data["easings"].to<JsonArray>();
            const char* easingNames[] = {
                "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
                "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
                "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
            };
            for (uint8_t i = 0; i < 10; i++) {
                JsonObject easing = easings.add<JsonObject>();
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

            JsonArray zones = data["zones"].to<JsonArray>();
            for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
                JsonObject zone = zones.add<JsonObject>();
                zone["id"] = i;
                zone["enabled"] = m_zoneComposer->isZoneEnabled(i);
                zone["effectId"] = m_zoneComposer->getZoneEffect(i);
                zone["effectName"] = m_renderer->getEffectName(m_zoneComposer->getZoneEffect(i));
                zone["brightness"] = m_zoneComposer->getZoneBrightness(i);
                zone["speed"] = m_zoneComposer->getZoneSpeed(i);
                zone["paletteId"] = m_zoneComposer->getZonePalette(i);
                zone["blendMode"] = static_cast<uint8_t>(m_zoneComposer->getZoneBlendMode(i));
                zone["blendModeName"] = getBlendModeName(m_zoneComposer->getZoneBlendMode(i));
            }

            // Available presets
            JsonArray presets = data["presets"].to<JsonArray>();
            for (uint8_t i = 0; i < 5; i++) {
                JsonObject preset = presets.add<JsonObject>();
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

        uint8_t effectCount = m_renderer->getEffectCount();
        uint8_t startIdx = (page - 1) * limit;
        uint8_t endIdx = min((uint8_t)(startIdx + limit), effectCount);

        String response = buildWsResponse("effects.list", requestId, [this, effectCount, startIdx, endIdx, page, limit, details](JsonObject& data) {
            JsonArray effects = data["effects"].to<JsonArray>();

            for (uint8_t i = startIdx; i < endIdx; i++) {
                JsonObject effect = effects.add<JsonObject>();
                effect["id"] = i;
                effect["name"] = m_renderer->getEffectName(i);

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

            JsonObject pagination = data["pagination"].to<JsonObject>();
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

        if (effectId >= m_renderer->getEffectCount()) {
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
            m_renderer->startTransition(effectId, transType);
        } else {
            m_actorSystem.setEffect(effectId);
        }

        broadcastStatus();

        String response = buildWsResponse("effects.changed", requestId, [this, effectId, useTransition](JsonObject& data) {
            data["effectId"] = effectId;
            data["name"] = m_renderer->getEffectName(effectId);
            data["transitionActive"] = useTransition;
        });
        client->text(response);
    }

    else if (type == "effects.parameters.set") {
        const char* requestId = doc["requestId"] | "";
        uint8_t effectId = doc["effectId"] | m_renderer->getCurrentEffect();

        if (effectId >= m_renderer->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
            return;
        }

        plugins::IEffect* effect = m_renderer->getEffectInstance(effectId);
        if (!effect) {
            client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Effect has no parameters", requestId));
            return;
        }

        if (!doc.containsKey("parameters") || !doc["parameters"].is<JsonObject>()) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing parameters object", requestId));
            return;
        }

        JsonObject params = doc["parameters"].as<JsonObject>();
        String response = buildWsResponse("effects.parameters.changed", requestId,
                                          [this, effectId, effect, params](JsonObject& data) {
            data["effectId"] = effectId;
            data["name"] = m_renderer->getEffectName(effectId);

            JsonArray queuedArr = data["queued"].to<JsonArray>();
            JsonArray failedArr = data["failed"].to<JsonArray>();

            for (JsonPair kv : params) {
                const char* key = kv.key().c_str();
                float value = kv.value().as<float>();
                bool known = false;
                uint8_t count = effect->getParameterCount();
                for (uint8_t i = 0; i < count; ++i) {
                    const plugins::EffectParameter* param = effect->getParameter(i);
                    if (param && strcmp(param->name, key) == 0) {
                        known = true;
                        break;
                    }
                }
                if (!known) {
                    failedArr.add(key);
                    continue;
                }
                if (m_renderer->enqueueEffectParameterUpdate(effectId, key, value)) {
                    queuedArr.add(key);
                } else {
                    failedArr.add(key);
                }
            }
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
        bool updatedIntensity = false;
        bool updatedSaturation = false;
        bool updatedComplexity = false;
        bool updatedVariation = false;
        bool updatedHue = false;

        if (doc.containsKey("brightness")) {
            uint8_t value = doc["brightness"] | 128;
            m_actorSystem.setBrightness(value);
            updatedBrightness = true;
        }

        if (doc.containsKey("speed")) {
            uint8_t value = doc["speed"] | 15;
            if (value >= 1 && value <= 50) {
                m_actorSystem.setSpeed(value);
                updatedSpeed = true;
            }
        }

        if (doc.containsKey("paletteId")) {
            uint8_t value = doc["paletteId"] | 0;
            m_actorSystem.setPalette(value);
            updatedPalette = true;
        }

        if (doc.containsKey("intensity")) {
            uint8_t value = doc["intensity"] | 128;
            m_actorSystem.setIntensity(value);
            updatedIntensity = true;
        }

        if (doc.containsKey("saturation")) {
            uint8_t value = doc["saturation"] | 255;
            m_actorSystem.setSaturation(value);
            updatedSaturation = true;
        }

        if (doc.containsKey("complexity")) {
            uint8_t value = doc["complexity"] | 128;
            m_actorSystem.setComplexity(value);
            updatedComplexity = true;
        }

        if (doc.containsKey("variation")) {
            uint8_t value = doc["variation"] | 0;
            m_actorSystem.setVariation(value);
            updatedVariation = true;
        }

        if (doc.containsKey("hue")) {
            uint8_t value = doc["hue"] | 0;
            m_actorSystem.setHue(value);
            updatedHue = true;
        }

        broadcastStatus();

        String response = buildWsResponse("parameters.changed", requestId, [this, updatedBrightness, updatedSpeed, updatedPalette, updatedIntensity, updatedSaturation, updatedComplexity, updatedVariation, updatedHue](JsonObject& data) {
            JsonArray updated = data["updated"].to<JsonArray>();
            if (updatedBrightness) updated.add("brightness");
            if (updatedSpeed) updated.add("speed");
            if (updatedPalette) updated.add("paletteId");
            if (updatedIntensity) updated.add("intensity");
            if (updatedSaturation) updated.add("saturation");
            if (updatedComplexity) updated.add("complexity");
            if (updatedVariation) updated.add("variation");
            if (updatedHue) updated.add("hue");

            JsonObject current = data["current"].to<JsonObject>();
            current["brightness"] = m_renderer->getBrightness();
            current["speed"] = m_renderer->getSpeed();
            current["paletteId"] = m_renderer->getPaletteIndex();
            current["hue"] = m_renderer->getHue();
            current["intensity"] = m_renderer->getIntensity();
            current["saturation"] = m_renderer->getSaturation();
            current["complexity"] = m_renderer->getComplexity();
            current["variation"] = m_renderer->getVariation();
        });
        client->text(response);
    }
#if FEATURE_AUDIO_SYNC
    else if (type == "audio.parameters.set") {
        const char* requestId = doc["requestId"] | "";
        auto* audio = m_actorSystem.getAudio();
        if (!audio) {
            client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
            return;
        }

        bool updatedPipeline = false;
        bool updatedContract = false;
        bool resetState = false;

        audio::AudioPipelineTuning pipeline = audio->getPipelineTuning();
        audio::AudioContractTuning contract = m_renderer ? m_renderer->getAudioContractTuning()
                                                         : audio::clampAudioContractTuning(audio::AudioContractTuning{});

        auto applyFloat = [](JsonVariant source, const char* key, float& target, bool& updated) {
            if (!source.is<JsonObject>()) return;
            if (source.containsKey(key)) {
                target = source[key].as<float>();
                updated = true;
            }
        };

        auto applyUint8 = [](JsonVariant source, const char* key, uint8_t& target, bool& updated) {
            if (!source.is<JsonObject>()) return;
            if (source.containsKey(key)) {
                target = source[key].as<uint8_t>();
                updated = true;
            }
        };

        JsonVariant pipelineSrc = doc.as<JsonVariant>();
        if (doc.containsKey("pipeline")) {
            pipelineSrc = doc["pipeline"];
        }
        applyFloat(pipelineSrc, "dcAlpha", pipeline.dcAlpha, updatedPipeline);
        applyFloat(pipelineSrc, "agcTargetRms", pipeline.agcTargetRms, updatedPipeline);
        applyFloat(pipelineSrc, "agcMinGain", pipeline.agcMinGain, updatedPipeline);
        applyFloat(pipelineSrc, "agcMaxGain", pipeline.agcMaxGain, updatedPipeline);
        applyFloat(pipelineSrc, "agcAttack", pipeline.agcAttack, updatedPipeline);
        applyFloat(pipelineSrc, "agcRelease", pipeline.agcRelease, updatedPipeline);
        applyFloat(pipelineSrc, "agcClipReduce", pipeline.agcClipReduce, updatedPipeline);
        applyFloat(pipelineSrc, "agcIdleReturnRate", pipeline.agcIdleReturnRate, updatedPipeline);
        applyFloat(pipelineSrc, "noiseFloorMin", pipeline.noiseFloorMin, updatedPipeline);
        applyFloat(pipelineSrc, "noiseFloorRise", pipeline.noiseFloorRise, updatedPipeline);
        applyFloat(pipelineSrc, "noiseFloorFall", pipeline.noiseFloorFall, updatedPipeline);
        applyFloat(pipelineSrc, "gateStartFactor", pipeline.gateStartFactor, updatedPipeline);
        applyFloat(pipelineSrc, "gateRangeFactor", pipeline.gateRangeFactor, updatedPipeline);
        applyFloat(pipelineSrc, "gateRangeMin", pipeline.gateRangeMin, updatedPipeline);
        applyFloat(pipelineSrc, "rmsDbFloor", pipeline.rmsDbFloor, updatedPipeline);
        applyFloat(pipelineSrc, "rmsDbCeil", pipeline.rmsDbCeil, updatedPipeline);
        applyFloat(pipelineSrc, "bandDbFloor", pipeline.bandDbFloor, updatedPipeline);
        applyFloat(pipelineSrc, "bandDbCeil", pipeline.bandDbCeil, updatedPipeline);
        applyFloat(pipelineSrc, "chromaDbFloor", pipeline.chromaDbFloor, updatedPipeline);
        applyFloat(pipelineSrc, "chromaDbCeil", pipeline.chromaDbCeil, updatedPipeline);
        applyFloat(pipelineSrc, "fluxScale", pipeline.fluxScale, updatedPipeline);

        JsonVariant controlBusSrc = doc.as<JsonVariant>();
        if (doc.containsKey("controlBus")) {
            controlBusSrc = doc["controlBus"];
        }
        applyFloat(controlBusSrc, "alphaFast", pipeline.controlBusAlphaFast, updatedPipeline);
        applyFloat(controlBusSrc, "alphaSlow", pipeline.controlBusAlphaSlow, updatedPipeline);

        JsonVariant contractSrc = doc.as<JsonVariant>();
        if (doc.containsKey("contract")) {
            contractSrc = doc["contract"];
        }
        applyFloat(contractSrc, "audioStalenessMs", contract.audioStalenessMs, updatedContract);
        applyFloat(contractSrc, "bpmMin", contract.bpmMin, updatedContract);
        applyFloat(contractSrc, "bpmMax", contract.bpmMax, updatedContract);
        applyFloat(contractSrc, "bpmTau", contract.bpmTau, updatedContract);
        applyFloat(contractSrc, "confidenceTau", contract.confidenceTau, updatedContract);
        applyFloat(contractSrc, "phaseCorrectionGain", contract.phaseCorrectionGain, updatedContract);
        applyFloat(contractSrc, "barCorrectionGain", contract.barCorrectionGain, updatedContract);
        applyUint8(contractSrc, "beatsPerBar", contract.beatsPerBar, updatedContract);
        applyUint8(contractSrc, "beatUnit", contract.beatUnit, updatedContract);

        if (doc.containsKey("resetState")) {
            resetState = doc["resetState"] | false;
        }

        if (updatedPipeline) {
            audio->setPipelineTuning(pipeline);
        }
        if (updatedContract && m_renderer) {
            m_renderer->setAudioContractTuning(contract);
        }
        if (resetState) {
            audio->resetDspState();
        }

        String response = buildWsResponse("audio.parameters.changed", requestId,
                                          [updatedPipeline, updatedContract, resetState](JsonObject& data) {
            JsonArray updated = data["updated"].to<JsonArray>();
            if (updatedPipeline) updated.add("pipeline");
            if (updatedContract) updated.add("contract");
            if (resetState) updated.add("state");
        });
        client->text(response);
    }
#endif

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

            JsonArray zones = data["zones"].to<JsonArray>();
            for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
                JsonObject zone = zones.add<JsonObject>();
                zone["id"] = i;
                zone["enabled"] = m_zoneComposer->isZoneEnabled(i);
                zone["effectId"] = m_zoneComposer->getZoneEffect(i);
                zone["effectName"] = m_renderer->getEffectName(m_zoneComposer->getZoneEffect(i));
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
            if (effectId < m_renderer->getEffectCount()) {
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

            JsonArray updated = data["updated"].to<JsonArray>();
            if (updatedEffect) updated.add("effectId");
            if (updatedBrightness) updated.add("brightness");
            if (updatedSpeed) updated.add("speed");
            if (updatedPalette) updated.add("paletteId");

            JsonObject current = data["current"].to<JsonObject>();
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

        if (effectId >= m_renderer->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
            return;
        }

        m_zoneComposer->setZoneEffect(zoneId, effectId);
        broadcastZoneState();

        String response = buildWsResponse("zones.effectChanged", requestId, [this, zoneId, effectId](JsonObject& data) {
            data["zoneId"] = zoneId;
            data["effectId"] = effectId;
            data["effectName"] = m_renderer->getEffectName(effectId);
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
            JsonArray types = data["types"].to<JsonArray>();

            for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
                JsonObject type = types.add<JsonObject>();
                type["id"] = i;
                type["name"] = getTransitionName(static_cast<TransitionType>(i));
            }

            JsonArray easings = data["easingCurves"].to<JsonArray>();
            const char* easingNames[] = {
                "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
                "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
                "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
            };
            for (uint8_t i = 0; i < 10; i++) {
                JsonObject easing = easings.add<JsonObject>();
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

        if (toEffect >= m_renderer->getEffectCount()) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid toEffect", requestId));
            return;
        }

        uint8_t fromEffect = m_renderer->getCurrentEffect();
        m_renderer->startTransition(toEffect, transType);
        broadcastStatus();

        String response = buildWsResponse("transition.started", requestId, [this, fromEffect, toEffect, transType, duration](JsonObject& data) {
            data["fromEffect"] = fromEffect;
            data["toEffect"] = toEffect;
            data["toEffectName"] = m_renderer->getEffectName(toEffect);
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
            JsonObject durations = data["durations"].to<JsonObject>();
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
            JsonObject durations = data["durations"].to<JsonObject>();
            durations["build"] = narrative.getBuildDuration();
            durations["hold"] = narrative.getHoldDuration();
            durations["release"] = narrative.getReleaseDuration();
            durations["rest"] = narrative.getRestDuration();
            durations["total"] = narrative.getTotalDuration();
            
            // Curves
            JsonObject curves = data["curves"].to<JsonObject>();
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
            JsonArray palettes = data["palettes"].to<JsonArray>();

            for (uint8_t i = startIdx; i < endIdx; i++) {
                JsonObject palette = palettes.add<JsonObject>();
                palette["id"] = i;
                palette["name"] = MasterPaletteNames[i];
                palette["category"] = getPaletteCategory(i);
            }

            JsonObject pagination = data["pagination"].to<JsonObject>();
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
            JsonObject palette = data["palette"].to<JsonObject>();
            palette["id"] = paletteId;
            palette["name"] = MasterPaletteNames[paletteId];
            palette["category"] = getPaletteCategory(paletteId);

            JsonObject flags = palette["flags"].to<JsonObject>();
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

            JsonArray blendFactors = data["blendFactors"].to<JsonArray>();
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
            JsonArray palettes = data["blendPalettes"].to<JsonArray>();
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
            JsonArray factors = data["blendFactors"].to<JsonArray>();
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

    // ========================================================================
    // Color Correction Engine - White/Brown guardrails, gamma, auto-exposure
    // ========================================================================

    // colorCorrection.getConfig - Get full configuration
    else if (type == "colorCorrection.getConfig") {
        const char* requestId = doc["requestId"] | "";
        auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
        auto& cfg = engine.getConfig();

        String response = buildWsResponse("colorCorrection.getConfig", requestId, [&cfg](JsonObject& data) {
            data["mode"] = (uint8_t)cfg.mode;
            data["modeNames"] = "OFF,HSV,RGB,BOTH";
            data["hsvMinSaturation"] = cfg.hsvMinSaturation;
            data["rgbWhiteThreshold"] = cfg.rgbWhiteThreshold;
            data["rgbTargetMin"] = cfg.rgbTargetMin;
            data["autoExposureEnabled"] = cfg.autoExposureEnabled;
            data["autoExposureTarget"] = cfg.autoExposureTarget;
            data["gammaEnabled"] = cfg.gammaEnabled;
            data["gammaValue"] = cfg.gammaValue;
            data["brownGuardrailEnabled"] = cfg.brownGuardrailEnabled;
            data["maxGreenPercentOfRed"] = cfg.maxGreenPercentOfRed;
            data["maxBluePercentOfRed"] = cfg.maxBluePercentOfRed;
        });
        client->text(response);
    }

    // colorCorrection.setMode - Set correction mode
    else if (type == "colorCorrection.setMode") {
        const char* requestId = doc["requestId"] | "";

        if (!doc.containsKey("mode")) {
            client->text(buildWsError(ErrorCodes::MISSING_FIELD, "mode required (0-3)", requestId));
            return;
        }

        uint8_t mode = doc["mode"] | 2;
        if (mode > 3) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "mode must be 0-3 (OFF,HSV,RGB,BOTH)", requestId));
            return;
        }

        auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
        engine.setMode((lightwaveos::enhancement::CorrectionMode)mode);

        String response = buildWsResponse("colorCorrection.setMode", requestId, [mode](JsonObject& data) {
            data["mode"] = mode;
            const char* names[] = {"OFF", "HSV", "RGB", "BOTH"};
            data["modeName"] = names[mode];
        });
        client->text(response);
    }

    // colorCorrection.setConfig - Update configuration fields
    else if (type == "colorCorrection.setConfig") {
        const char* requestId = doc["requestId"] | "";
        auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
        auto& cfg = engine.getConfig();

        // Update any fields provided
        if (doc.containsKey("mode")) {
            uint8_t mode = doc["mode"];
            if (mode <= 3) cfg.mode = (lightwaveos::enhancement::CorrectionMode)mode;
        }
        if (doc.containsKey("hsvMinSaturation")) {
            cfg.hsvMinSaturation = doc["hsvMinSaturation"];
        }
        if (doc.containsKey("rgbWhiteThreshold")) {
            cfg.rgbWhiteThreshold = doc["rgbWhiteThreshold"];
        }
        if (doc.containsKey("rgbTargetMin")) {
            cfg.rgbTargetMin = doc["rgbTargetMin"];
        }
        if (doc.containsKey("autoExposureEnabled")) {
            cfg.autoExposureEnabled = doc["autoExposureEnabled"];
        }
        if (doc.containsKey("autoExposureTarget")) {
            cfg.autoExposureTarget = doc["autoExposureTarget"];
        }
        if (doc.containsKey("gammaEnabled")) {
            cfg.gammaEnabled = doc["gammaEnabled"];
        }
        if (doc.containsKey("gammaValue")) {
            float val = doc["gammaValue"];
            if (val >= 1.0f && val <= 3.0f) cfg.gammaValue = val;
        }
        if (doc.containsKey("brownGuardrailEnabled")) {
            cfg.brownGuardrailEnabled = doc["brownGuardrailEnabled"];
        }
        if (doc.containsKey("maxGreenPercentOfRed")) {
            cfg.maxGreenPercentOfRed = doc["maxGreenPercentOfRed"];
        }
        if (doc.containsKey("maxBluePercentOfRed")) {
            cfg.maxBluePercentOfRed = doc["maxBluePercentOfRed"];
        }

        String response = buildWsResponse("colorCorrection.setConfig", requestId, [&cfg](JsonObject& data) {
            data["mode"] = (uint8_t)cfg.mode;
            data["updated"] = true;
        });
        client->text(response);
    }

    // colorCorrection.save - Save configuration to NVS
    else if (type == "colorCorrection.save") {
        const char* requestId = doc["requestId"] | "";
        lightwaveos::enhancement::ColorCorrectionEngine::getInstance().saveToNVS();

        String response = buildWsResponse("colorCorrection.save", requestId, [](JsonObject& data) {
            data["saved"] = true;
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

    if (m_renderer) {
        doc["effectId"] = m_renderer->getCurrentEffect();
        doc["effectName"] = m_renderer->getEffectName(m_renderer->getCurrentEffect());
        doc["brightness"] = m_renderer->getBrightness();
        doc["speed"] = m_renderer->getSpeed();
        doc["paletteId"] = m_renderer->getPaletteIndex();
        doc["hue"] = m_renderer->getHue();
        doc["intensity"] = m_renderer->getIntensity();
        doc["saturation"] = m_renderer->getSaturation();
        doc["complexity"] = m_renderer->getComplexity();
        doc["variation"] = m_renderer->getVariation();

        const RenderStats& stats = m_renderer->getStats();
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

    StaticJsonDocument<512> doc;
    doc["type"] = "zone.state";
    doc["enabled"] = m_zoneComposer->isEnabled();
    doc["zoneCount"] = m_zoneComposer->getZoneCount();

    JsonArray zones = doc["zones"].to<JsonArray>();
    for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
        JsonObject zone = zones.add<JsonObject>();
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

    StaticJsonDocument<512> doc;
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
    if (!m_ledBroadcaster || !m_renderer) return;
    
    // Get LED buffer from renderer (cross-core safe copy)
    CRGB leds[webserver::LedStreamConfig::TOTAL_LEDS];
    m_renderer->getBufferCopy(leds);
    
    // Broadcast to subscribed clients (throttling handled internally)
    m_ledBroadcaster->broadcast(leds);
}

bool WebServer::setLEDStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_ledBroadcaster) return false;
    uint32_t clientId = client->id();
    bool success = m_ledBroadcaster->setSubscription(clientId, subscribe);
    
    if (subscribe && success) {
        Serial.printf("[WebServer] Client %u subscribed to LED stream\n", clientId);
    } else if (!subscribe) {
        Serial.printf("[WebServer] Client %u unsubscribed from LED stream\n", clientId);
    }
    
    return success;
}

bool WebServer::hasLEDStreamSubscribers() const {
    return m_ledBroadcaster && m_ledBroadcaster->hasSubscribers();
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
