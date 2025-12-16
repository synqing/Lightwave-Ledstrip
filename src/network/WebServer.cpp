#include "WebServer.h"

#if FEATURE_WEB_SERVER

#include "WiFiManager.h"
#include "../effects/EffectMetadata.h"
#include "../config/network_config.h"
#include "../config/hardware_config.h"
#include "OpenApiSpec.h"
#include <FastLED.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <AsyncJson.h>

#if FEATURE_AUDIO_SYNC
#include "../audio/audio_web_handlers.h"
#endif

#if FEATURE_ENHANCEMENT_ENGINES
#include "../effects/engines/ColorEngine.h"
#include "../effects/engines/MotionEngine.h"
#endif

// External references from main.cpp
extern uint8_t currentEffect;
extern uint8_t gHue;
extern CRGB strip1[];
extern CRGB strip2[];
extern bool useOptimizedEffects;
extern bool useRandomTransitions;
extern uint8_t effectSpeed;
extern uint8_t effectIntensity, effectSaturation, effectComplexity, effectVariation;

// Zone Composer
#include "../effects/zones/ZoneComposer.h"
extern ZoneComposer zoneComposer;

// External references from main.cpp
#include "../effects.h"
extern CRGBPalette16 currentPalette;
extern CRGBPalette16 targetPalette;
extern uint8_t currentPaletteIndex;
extern const TProgmemRGBGradientPaletteRef gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

// Forward declarations
void startAdvancedTransition(uint8_t newEffect);

// Global instance
LightwaveWebServer webServer;

LightwaveWebServer::LightwaveWebServer() {
    server = new AsyncWebServer(NetworkConfig::WEB_SERVER_PORT);
    ws = new AsyncWebSocket("/ws");
}

LightwaveWebServer::~LightwaveWebServer() {
    delete ws;
    delete server;
}

bool LightwaveWebServer::begin() {
    Serial.println("[WebServer] Starting web server (network-independent)");
    
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("[WebServer] Failed to mount SPIFFS");
        return false;
    }
    
    // Initialize network services regardless of WiFi state
    return beginNetworkServices();
}

bool LightwaveWebServer::beginNetworkServices() {
    Serial.println("[WebServer] Initializing network services");

    // Setup CORS headers for all responses
    // Allows web apps from any origin to access the API
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, X-OTA-Token");

    // Setup WebSocket handlers
    ws->onEvent(onWsEvent);
    server->addHandler(ws);

    // Setup HTTP routes
    setupRoutes();

    // Start the server immediately - it will work in AP or STA mode
    server->begin();
    isRunning = true;
    
    Serial.printf("[WebServer] Server started on port %d\n", NetworkConfig::WEB_SERVER_PORT);
    
    // Start mDNS if we have a connection
    startMDNS();
    
    // Start monitoring WiFi state changes
    startWiFiMonitor();
    
    return true;
}

void LightwaveWebServer::startWiFiMonitor() {
    // Create a task to monitor WiFi state and update services accordingly
    xTaskCreate(
        [](void* param) {
            LightwaveWebServer* server = static_cast<LightwaveWebServer*>(param);
            WiFiManager& wifi = WiFiManager::getInstance();
            WiFiManager::WiFiState lastState = WiFiManager::STATE_WIFI_INIT;
            
            while (true) {
                WiFiManager::WiFiState currentState = wifi.getState();
                
                // Handle state changes
                if (currentState != lastState) {
                    Serial.printf("[WebServer] WiFi state changed: %s -> %s\n",
                                  wifi.getStateString().c_str(), 
                                  wifi.getStateString().c_str());
                    
                    switch (currentState) {
                        case WiFiManager::STATE_WIFI_CONNECTED:
                            server->onWiFiConnected();
                            break;
                            
                        case WiFiManager::STATE_WIFI_AP_MODE:
                            server->onAPModeStarted();
                            break;
                            
                        case WiFiManager::STATE_WIFI_DISCONNECTED:
                        case WiFiManager::STATE_WIFI_FAILED:
                            server->onWiFiDisconnected();
                            break;
                            
                        default:
                            break;
                    }
                    
                    lastState = currentState;
                }
                
                vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
            }
        },
        "WebServerWiFiMon",
        8192,  // Increased from 2048 - needs more stack for mDNS and JSON operations
        this,
        1,
        nullptr
    );
}

void LightwaveWebServer::onWiFiConnected() {
    Serial.printf("[WebServer] WiFi connected - IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Restart mDNS with new IP
    startMDNS();
    
    // Broadcast connection status to all WebSocket clients
    StaticJsonDocument<256> doc;
    doc["type"] = "network";
    doc["event"] = "wifi_connected";
    doc["ip"] = WiFi.localIP().toString();
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void LightwaveWebServer::onAPModeStarted() {
    Serial.printf("[WebServer] AP mode active - IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    // Broadcast AP status to all WebSocket clients
    StaticJsonDocument<256> doc;
    doc["type"] = "network";
    doc["event"] = "ap_mode";
    doc["ip"] = WiFi.softAPIP().toString();
    doc["ssid"] = NetworkConfig::AP_SSID;
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void LightwaveWebServer::onWiFiDisconnected() {
    Serial.println("[WebServer] WiFi disconnected");
    
    // Stop mDNS when disconnected
    MDNS.end();
    
    // Broadcast disconnection to WebSocket clients
    StaticJsonDocument<256> doc;
    doc["type"] = "network";
    doc["event"] = "wifi_disconnected";
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void LightwaveWebServer::startMDNS() {
    // Only start mDNS if we have a valid IP
    if (WiFi.status() == WL_CONNECTED || WiFi.getMode() & WIFI_MODE_AP) {
        // If mDNS is already running, stop it first to allow clean re-initialization
        if (mdnsStarted) {
            MDNS.end();
            mdnsStarted = false;
            delay(100); // Brief delay to ensure clean shutdown
        }

        // Initialize mDNS
        if (MDNS.begin(NetworkConfig::MDNS_HOSTNAME)) {
            Serial.printf("[WebServer] mDNS responder started: http://%s.local\n", NetworkConfig::MDNS_HOSTNAME);

            // Add service to mDNS-SD
            MDNS.addService("http", "tcp", NetworkConfig::WEB_SERVER_PORT);
            MDNS.addService("ws", "tcp", NetworkConfig::WEB_SERVER_PORT);

            // Add some TXT records
            MDNS.addServiceTxt("http", "tcp", "version", "1.0.0");
            MDNS.addServiceTxt("http", "tcp", "board", "ESP32-S3");

            mdnsStarted = true;
        } else {
            Serial.println("[WebServer] Error starting mDNS");
        }
    }
}

void LightwaveWebServer::setupRoutes() {
    // Setup v1 API routes first
    setupV1Routes();

    // Serve static files from SPIFFS
    server->serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    
    // API endpoints
    server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot();
        
        WiFiManager& wifi = WiFiManager::getInstance();
        
        root["effect"] = currentEffect;
        root["brightness"] = FastLED.getBrightness();
        root["gHue"] = gHue;
        root["optimized"] = useOptimizedEffects;
        root["transitions"] = useRandomTransitions;
        root["freeHeap"] = ESP.getFreeHeap();
        
        // Network status
        JsonObject network = root.createNestedObject("network");
        network["state"] = wifi.getStateString();
        network["connected"] = wifi.isConnected();
        network["ap_mode"] = wifi.isAPMode();
        
        if (wifi.isConnected()) {
            network["ssid"] = WiFi.SSID();
            network["ip"] = WiFi.localIP().toString();
            network["rssi"] = WiFi.RSSI();
            network["channel"] = WiFi.channel();
            network["uptime"] = wifi.getUptimeSeconds();
        } else if (wifi.isAPMode()) {
            network["ap_ip"] = WiFi.softAPIP().toString();
            network["ap_clients"] = WiFi.softAPgetStationNum();
        }
        
        response->setLength();
        request->send(response);
    });
    
    // Effect control
    server->on("/api/effect", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("effect")) {
                uint8_t newEffect = doc["effect"];
                if (newEffect >= NUM_EFFECTS) {
                    request->send(400, "application/json", "{\"error\":\"Invalid effect ID\"}");
                    return;
                }
                startAdvancedTransition(newEffect);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
            request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
        }
    );
    
    // Brightness control
    server->on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("brightness")) {
                uint8_t brightness = doc["brightness"];
                FastLED.setBrightness(brightness);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
            request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
        }
    );
    
    // Network control endpoints
    server->on("/api/network/scan", HTTP_GET, [](AsyncWebServerRequest *request){
        WiFiManager& wifi = WiFiManager::getInstance();
        wifi.scanNetworks();
        request->send(200, "application/json", "{\"status\":\"scanning\"}");
    });
    
    server->on("/api/network/connect", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("ssid") && doc.containsKey("password")) {
                WiFiManager& wifi = WiFiManager::getInstance();
                wifi.setCredentials(doc["ssid"], doc["password"]);
                wifi.reconnect();
                request->send(200, "application/json", "{\"status\":\"connecting\"}");
                return;
            }
            request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
        }
    );
    
    // OTA update endpoint (secured with token authentication)
    server->on("/update", HTTP_POST,
        [this](AsyncWebServerRequest *request){
            shouldReboot = !Update.hasError();
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain",
                shouldReboot ? "OK" : "FAIL");
            response->addHeader("Connection", "close");
            request->send(response);
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            // Token authentication check on first chunk
            if(!index){
                // Check for X-OTA-Token header
                if (!request->hasHeader("X-OTA-Token") ||
                    request->header("X-OTA-Token") != NetworkConfig::OTA_UPDATE_TOKEN) {
                    Serial.println("[OTA] Unauthorized update attempt blocked!");
                    request->send(401, "text/plain", "Unauthorized: Invalid or missing OTA token");
                    return;
                }
                Serial.printf("[OTA] Authorized update start: %s\n", filename.c_str());
                if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
                    Update.printError(Serial);
                }
            }
            if(!Update.hasError()){
                if(Update.write(data, len) != len){
                    Update.printError(Serial);
                }
            }
            if(final){
                if(Update.end(true)){
                    Serial.printf("[OTA] Update Success: %uB\n", index+len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    // ========== NEW WEBAPP API ENDPOINTS ==========

    // Speed control
    server->on("/api/speed", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("speed")) {
                uint8_t newSpeed = doc["speed"];
                if (newSpeed >= 1 && newSpeed <= 50) {
                    effectSpeed = newSpeed;
                    request->send(200, "application/json", "{\"status\":\"ok\"}");
                } else {
                    request->send(400, "application/json", "{\"error\":\"Speed must be 1-50\"}");
                }
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Palette control
    server->on("/api/palette", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("paletteId")) {
                uint8_t paletteId = doc["paletteId"];
                if (paletteId >= gGradientPaletteCount) {
                    request->send(400, "application/json", "{\"error\":\"Invalid palette ID\"}");
                    return;
                }
                currentPaletteIndex = paletteId;
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
            request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
        }
    );

    // Get palettes list
    server->on("/api/palettes", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot();
        JsonArray palettes = root.createNestedArray("palettes");

        extern const uint8_t gGradientPaletteCount;
        extern const char* const paletteNames[];
        for (uint8_t i = 0; i < gGradientPaletteCount; i++) {
            JsonObject palette = palettes.createNestedObject();
            palette["id"] = i;
            palette["name"] = paletteNames[i];
        }

        response->setLength();
        request->send(response);
    });

    // Get effects list (paginated to avoid 2KB JSON limit)
    server->on("/api/effects", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot();
        JsonArray effectsList = root.createNestedArray("effects");

        // Add effects in chunks
        extern const uint8_t NUM_EFFECTS;
        extern Effect effects[];

        uint8_t start = request->hasParam("start") ? request->getParam("start")->value().toInt() : 0;
        uint8_t count = request->hasParam("count") ? request->getParam("count")->value().toInt() : 20;
        uint8_t end = min((uint8_t)(start + count), NUM_EFFECTS);

        for (uint8_t i = start; i < end; i++) {
            JsonObject effect = effectsList.createNestedObject();
            effect["id"] = i;
            effect["name"] = effects[i].name;
            // Category determined by ID ranges
            if (i <= 4) effect["category"] = "Classic";
            else if (i <= 7) effect["category"] = "Shockwave";
            else if (i <= 11) effect["category"] = "LGP Interference";
            else if (i <= 14) effect["category"] = "LGP Geometric";
            else if (i <= 20) effect["category"] = "LGP Advanced";
            else if (i <= 23) effect["category"] = "LGP Organic";
            else if (i <= 32) effect["category"] = "LGP Quantum";
            else if (i <= 34) effect["category"] = "LGP Color Mixing";
            else if (i <= 40) effect["category"] = "LGP Physics";
            else if (i <= 45) effect["category"] = "LGP Novel Physics";
            else effect["category"] = "Audio";
        }

        root["total"] = NUM_EFFECTS;
        root["start"] = start;
        root["count"] = end - start;

        response->setLength();
        request->send(response);
    });

    // Visual pipeline control
    server->on("/api/pipeline", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error) {
                bool updated = false;
                if (doc.containsKey("intensity")) {
                    effectIntensity = doc["intensity"];
                    updated = true;
                }
                if (doc.containsKey("saturation")) {
                    effectSaturation = doc["saturation"];
                    updated = true;
                }
                if (doc.containsKey("complexity")) {
                    effectComplexity = doc["complexity"];
                    updated = true;
                }
                if (doc.containsKey("variation")) {
                    effectVariation = doc["variation"];
                    updated = true;
                }

                if (updated) {
                    request->send(200, "application/json", "{\"status\":\"ok\"}");
                } else {
                    request->send(400, "application/json", "{\"error\":\"No valid parameters\"}");
                }
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Transitions control
    server->on("/api/transitions", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("enabled")) {
                extern bool useRandomTransitions;
                useRandomTransitions = doc["enabled"];
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
            request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
        }
    );

    // ========== ZONE COMPOSER API ENDPOINTS ==========

    // Get zone status
    server->on("/api/zone/status", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<512> doc;

        doc["enabled"] = zoneComposer.isEnabled();
        doc["zoneCount"] = zoneComposer.getZoneCount();

        JsonArray zones = doc.createNestedArray("zones");
        for (uint8_t i = 0; i < zoneComposer.getZoneCount(); i++) {
            JsonObject zone = zones.createNestedObject();
            zone["id"] = i;
            zone["enabled"] = zoneComposer.isZoneEnabled(i);
            zone["effectId"] = zoneComposer.getZoneEffect(i);
            zone["brightness"] = zoneComposer.getZoneBrightness(i);
            zone["speed"] = zoneComposer.getZoneSpeed(i);
        }

        serializeJson(doc, *response);
        request->send(response);
    });

    // Enable/disable zone mode
    server->on("/api/zone/enable", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("enabled")) {
                bool enable = doc["enabled"];
                if (enable) {
                    zoneComposer.enable();
                } else {
                    zoneComposer.disable();
                }
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Configure individual zone
    server->on("/api/zone/config", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("zoneId")) {
                uint8_t zoneId = doc["zoneId"];
                if (zoneId >= HardwareConfig::MAX_ZONES) {
                    request->send(400, "application/json", "{\"error\":\"Invalid zone ID\"}");
                    return;
                }

                if (doc.containsKey("effectId")) {
                    uint8_t effectId = doc["effectId"];
                    if (effectId >= NUM_EFFECTS) {
                        request->send(400, "application/json", "{\"error\":\"Invalid effect ID\"}");
                        return;
                    }
                    zoneComposer.setZoneEffect(zoneId, effectId);
                }

                if (doc.containsKey("enabled")) {
                    bool enabled = doc["enabled"];
                    zoneComposer.enableZone(zoneId, enabled);
                }

                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Set zone count
    server->on("/api/zone/count", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("count")) {
                uint8_t count = doc["count"];
                zoneComposer.setZoneCount(count);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Get zone presets
    server->on("/api/zone/presets", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<512> doc;

        JsonArray presets = doc.createNestedArray("presets");
        for (uint8_t i = 0; i < 5; i++) {  // 5 presets (0-4)
            JsonObject preset = presets.createNestedObject();
            preset["id"] = i;
            preset["name"] = String("Preset ") + String(i);
        }

        serializeJson(doc, *response);
        request->send(response);
    });

    // Load zone preset
    server->on("/api/zone/preset/load", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("presetId")) {
                uint8_t presetId = doc["presetId"];
                if (zoneComposer.loadPreset(presetId)) {
                    request->send(200, "application/json", "{\"status\":\"ok\"}");
                } else {
                    request->send(400, "application/json", "{\"error\":\"Invalid preset ID\"}");
                }
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Save zone config to NVS
    server->on("/api/zone/config/save", HTTP_POST, [](AsyncWebServerRequest *request){
        if (zoneComposer.saveConfig()) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
        }
    });

    // Load zone config from NVS
    server->on("/api/zone/config/load", HTTP_POST, [](AsyncWebServerRequest *request){
        if (zoneComposer.loadConfig()) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        }
    });

    // Set zone brightness
    server->on("/api/zone/brightness", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("zoneId") && doc.containsKey("brightness")) {
                uint8_t zoneId = doc["zoneId"];
                if (zoneId >= HardwareConfig::MAX_ZONES) {
                    request->send(400, "application/json", "{\"error\":\"Invalid zone ID\"}");
                    return;
                }
                uint8_t brightness = doc["brightness"];
                zoneComposer.setZoneBrightness(zoneId, brightness);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Set zone speed
    server->on("/api/zone/speed", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("zoneId") && doc.containsKey("speed")) {
                uint8_t zoneId = doc["zoneId"];
                if (zoneId >= HardwareConfig::MAX_ZONES) {
                    request->send(400, "application/json", "{\"error\":\"Invalid zone ID\"}");
                    return;
                }
                uint8_t speed = doc["speed"];
                zoneComposer.setZoneSpeed(zoneId, speed);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

#if FEATURE_ENHANCEMENT_ENGINES
    // =============== ENHANCEMENT ENGINE CONTROLS ===============

    // ColorEngine: Enable/disable cross-palette blending
    server->on("/api/enhancement/color/blend", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            extern ColorEngine* g_colorEngine;
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("enabled")) {
                bool enabled = doc["enabled"];
                ColorEngine::getInstance().enableCrossBlend(enabled);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // ColorEngine: Set diffusion amount
    server->on("/api/enhancement/color/diffusion", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("amount")) {
                uint8_t amount = doc["amount"];
                ColorEngine::getInstance().setDiffusionAmount(amount);
                ColorEngine::getInstance().enableDiffusion(amount > 0);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // ColorEngine: Set temporal rotation speed
    server->on("/api/enhancement/color/rotation", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("speed")) {
                float speed = doc["speed"];
                ColorEngine::getInstance().setRotationSpeed(speed);
                ColorEngine::getInstance().enableTemporalRotation(speed > 0);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // MotionEngine: Set phase offset
    server->on("/api/enhancement/motion/phase", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("offset")) {
                float offset = doc["offset"];
                MotionEngine::getInstance().getPhaseController().setStripPhaseOffset(offset);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // MotionEngine: Enable/disable auto-rotation
    server->on("/api/enhancement/motion/auto-rotate", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error && doc.containsKey("enabled") && doc.containsKey("speed")) {
                bool enabled = doc["enabled"];
                float speed = doc["speed"];

                if (enabled) {
                    MotionEngine::getInstance().getPhaseController().enableAutoRotate(speed);
                } else {
                    MotionEngine::getInstance().getPhaseController().enableAutoRotate(0);
                }
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Get enhancement engine status
    server->on("/api/enhancement/status", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot();

        root["colorEngineActive"] = ColorEngine::getInstance().isActive();
        root["motionEngineEnabled"] = MotionEngine::getInstance().isEnabled();

        response->setLength();
        request->send(response);
    });
#endif

#if FEATURE_AUDIO_SYNC
    // Audio web handlers - separate development track
    // See: src/effects/strip/LGPAudioReactive.cpp for 6 ready effects
    // Enable FEATURE_AUDIO_SYNC in features.h when audio hardware is connected
#endif

    // Handle CORS preflight requests and 404s
    server->onNotFound([](AsyncWebServerRequest *request){
        if (request->method() == HTTP_OPTIONS) {
            // Preflight CORS request - headers already set globally
            request->send(204);
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
}

void LightwaveWebServer::stop() {
    if (isRunning) {
        server->end();
        isRunning = false;
        Serial.println("[WebServer] Server stopped");
    }
}

void LightwaveWebServer::update() {
    if (!isRunning) return;

    // Clean up WebSocket clients
    ws->cleanupClients();

    // Handle reboot if needed
    if(shouldReboot){
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
    }

    // Periodic status broadcast
    static uint32_t lastBroadcast = 0;
    if (millis() - lastBroadcast > 5000) {
        lastBroadcast = millis();
        broadcastStatus();
    }

    // Idle connection cleanup (check every 30 seconds)
    static uint32_t lastIdleCheck = 0;
    if (millis() - lastIdleCheck > 30000) {
        lastIdleCheck = millis();

        uint32_t idleClients[ConnectionManager::MAX_WS_CLIENTS];
        uint8_t idleCount = 0;
        connectionMgr.checkIdleConnections(idleClients, idleCount);

        for (uint8_t i = 0; i < idleCount; i++) {
            AsyncWebSocketClient* client = ws->client(idleClients[i]);
            if (client) {
                Serial.printf("[WebSocket] Closing idle connection: %u\n", idleClients[i]);
                client->close(1000, "Idle timeout");
            }
            connectionMgr.onDisconnect(idleClients[i]);
        }
    }
}

void LightwaveWebServer::broadcastStatus() {
    if (ws->count() == 0) return;
    
    StaticJsonDocument<512> doc;
    WiFiManager& wifi = WiFiManager::getInstance();
    
    doc["type"] = "status";
    doc["effect"] = currentEffect;
    doc["brightness"] = FastLED.getBrightness();
    doc["gHue"] = gHue;
    doc["freeHeap"] = ESP.getFreeHeap();
    
    // Network status
    JsonObject network = doc.createNestedObject("network");
    network["state"] = wifi.getStateString();
    network["connected"] = wifi.isConnected();
    
    if (wifi.isConnected()) {
        network["ssid"] = WiFi.SSID();
        network["rssi"] = WiFi.RSSI();
    }
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

// Broadcast zone configuration to all connected clients
void LightwaveWebServer::broadcastZoneState() {
    if (ws->count() == 0) return;

    StaticJsonDocument<768> doc;
    doc["type"] = "zone.state";
    doc["enabled"] = zoneComposer.isEnabled();
    doc["zoneCount"] = zoneComposer.getZoneCount();

    JsonArray zones = doc.createNestedArray("zones");
    for (uint8_t i = 0; i < 4; i++) {
        JsonObject zone = zones.createNestedObject();
        zone["id"] = i;
        zone["enabled"] = zoneComposer.isZoneEnabled(i);
        zone["effectId"] = zoneComposer.getZoneEffect(i);
        zone["brightness"] = zoneComposer.getZoneBrightness(i);
        zone["speed"] = zoneComposer.getZoneSpeed(i);
        zone["paletteId"] = zoneComposer.getZonePalette(i);
    }

    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

// WebSocket event handler
void LightwaveWebServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        IPAddress clientIP = client->remoteIP();

        // Check if we can accept this connection
        if (!webServer.connectionMgr.canAcceptConnection(clientIP)) {
            Serial.printf("[WebSocket] Connection rejected from %s: limit exceeded\n",
                          clientIP.toString().c_str());
            client->close(1008, "Connection limit exceeded");
            return;
        }

        // Register the connection
        webServer.connectionMgr.onConnect(clientIP, client->id());
        Serial.printf("[WebSocket] Client connected: %s (id: %u, total: %d)\n",
                      clientIP.toString().c_str(), client->id(),
                      webServer.connectionMgr.getActiveCount());

        // Send initial status to new client
        webServer.broadcastStatus();
        webServer.broadcastZoneState();  // Send zone state to new client

    } else if (type == WS_EVT_DISCONNECT) {
        webServer.connectionMgr.onDisconnect(client->id());
        Serial.printf("[WebSocket] Client disconnected (id: %u, remaining: %d)\n",
                      client->id(), webServer.connectionMgr.getActiveCount());

    } else if (type == WS_EVT_DATA) {
        IPAddress clientIP = client->remoteIP();

        // Rate limit check
        if (!webServer.rateLimiter.checkWebSocket(clientIP)) {
            Serial.printf("[WebSocket] Rate limited: %s\n", clientIP.toString().c_str());
            String errorMsg = buildWsError(ErrorCodes::RATE_LIMITED,
                                           "Too many messages, please slow down");
            client->text(errorMsg);
            return;
        }

        // Record activity for idle timeout tracking
        webServer.connectionMgr.onActivity(client->id());
        // Handle WebSocket data - use stack buffer to avoid heap fragmentation
        if (len > 511) {
            Serial.println("[WebSocket] Message too large, rejected");
            return;
        }
        char messageBuffer[512];
        memcpy(messageBuffer, data, len);
        messageBuffer[len] = '\0';

        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, messageBuffer);

        if (!error) {
            // Check for new "type" format first
            if (doc.containsKey("type")) {
                String type = doc["type"].as<String>();

                // Global effect control
                if (type == "setEffect") {
                    uint8_t effectId = doc["effectId"];
                    if (effectId < NUM_EFFECTS) {
                        startAdvancedTransition(effectId);
                    }
                } else if (type == "nextEffect") {
                    extern uint8_t currentEffect;
                    currentEffect = (currentEffect + 1) % NUM_EFFECTS;
                    startAdvancedTransition(currentEffect);
                } else if (type == "prevEffect") {
                    extern uint8_t currentEffect;
                    currentEffect = (currentEffect - 1 + NUM_EFFECTS) % NUM_EFFECTS;
                    startAdvancedTransition(currentEffect);
                }

                // Global parameters
                else if (type == "setBrightness") {
                    uint8_t value = doc["value"];
                    FastLED.setBrightness(value);
                } else if (type == "setSpeed") {
                    uint8_t value = doc["value"];
                    if (value >= 1 && value <= 50) {
                        effectSpeed = value;
                    }
                } else if (type == "setPalette") {
                    uint8_t paletteId = doc["paletteId"];
                    if (paletteId < gGradientPaletteCount) {
                        currentPaletteIndex = paletteId;
                        targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
                    }
                } else if (type == "setPipeline") {
                    if (doc.containsKey("intensity")) effectIntensity = doc["intensity"];
                    if (doc.containsKey("saturation")) effectSaturation = doc["saturation"];
                    if (doc.containsKey("complexity")) effectComplexity = doc["complexity"];
                    if (doc.containsKey("variation")) effectVariation = doc["variation"];
                } else if (type == "toggleTransitions") {
                    extern bool useRandomTransitions;
                    useRandomTransitions = doc["enabled"];
                }

                // Zone Composer control - broadcast state after each change
                else if (type == "zone.enable") {
                    bool enable = doc["enable"];
                    if (enable) zoneComposer.enable();
                    else zoneComposer.disable();
                    webServer.broadcastZoneState();
                } else if (type == "zone.setCount") {
                    uint8_t count = doc["count"];
                    zoneComposer.setZoneCount(count);
                    webServer.broadcastZoneState();
                } else if (type == "zone.setEffect") {
                    uint8_t zoneId = doc["zoneId"];
                    uint8_t effectId = doc["effectId"];
                    zoneComposer.setZoneEffect(zoneId, effectId);
                    webServer.broadcastZoneState();
                } else if (type == "zone.enableZone") {
                    uint8_t zoneId = doc["zoneId"];
                    bool enabled = doc["enabled"];
                    zoneComposer.enableZone(zoneId, enabled);
                    webServer.broadcastZoneState();
                } else if (type == "zone.loadPreset") {
                    uint8_t presetId = doc["presetId"];
                    zoneComposer.loadPreset(presetId);
                    webServer.broadcastZoneState();
                } else if (type == "zone.save") {
                    zoneComposer.saveConfig();
                    // No broadcast needed - save doesn't change runtime state
                } else if (type == "zone.load") {
                    zoneComposer.loadConfig();
                    webServer.broadcastZoneState();
                } else if (type == "zone.setBrightness") {
                    uint8_t zoneId = doc["zoneId"];
                    uint8_t brightness = doc["brightness"];
                    zoneComposer.setZoneBrightness(zoneId, brightness);
                    webServer.broadcastZoneState();
                } else if (type == "zone.setSpeed") {
                    uint8_t zoneId = doc["zoneId"];
                    uint8_t speed = doc["speed"];
                    zoneComposer.setZoneSpeed(zoneId, speed);
                    webServer.broadcastZoneState();
                } else if (type == "zone.setPalette") {
                    uint8_t zoneId = doc["zoneId"];
                    uint8_t paletteId = doc["paletteId"];
                    if (paletteId <= gGradientPaletteCount) {
                        zoneComposer.setZonePalette(zoneId, paletteId);
                        webServer.broadcastZoneState();
                    }
                }

                // ============================================================
                // v1 API Parity - Transitions
                // ============================================================
                else if (type == "transition.getTypes") {
                    // Return transition types list via WebSocket
                    StaticJsonDocument<1024> response;
                    response["type"] = "transition.types";
                    response["success"] = true;

                    JsonArray types = response.createNestedArray("types");
                    const char* transNames[] = {
                        "FADE", "WIPE_OUT", "WIPE_IN", "DISSOLVE", "PHASE_SHIFT",
                        "PULSEWAVE", "IMPLOSION", "IRIS", "NUCLEAR", "STARGATE",
                        "KALEIDOSCOPE", "MANDALA"
                    };
                    for (uint8_t i = 0; i < 12; i++) {
                        JsonObject t = types.createNestedObject();
                        t["id"] = i;
                        t["name"] = transNames[i];
                    }

                    String output;
                    serializeJson(response, output);
                    client->text(output);
                }
                else if (type == "transition.trigger") {
                    uint8_t toEffect = doc["toEffect"] | 0;
                    if (toEffect < NUM_EFFECTS) {
                        startAdvancedTransition(toEffect);
                        webServer.broadcastStatus();
                    }
                }
                else if (type == "transition.config") {
                    if (doc.containsKey("enabled")) {
                        useRandomTransitions = doc["enabled"];
                    }
                    webServer.broadcastStatus();
                }

                // ============================================================
                // v1 API Parity - Batch Operations
                // ============================================================
                else if (type == "batch") {
                    if (doc.containsKey("operations") && doc["operations"].is<JsonArray>()) {
                        JsonArray ops = doc["operations"];
                        uint8_t processed = 0;
                        uint8_t failed = 0;

                        for (JsonVariant op : ops) {
                            String action = op["action"] | "";
                            if (webServer.executeBatchAction(action, op)) {
                                processed++;
                            } else {
                                failed++;
                            }
                        }

                        // Send batch result
                        StaticJsonDocument<256> response;
                        response["type"] = "batch.result";
                        response["success"] = true;
                        response["processed"] = processed;
                        response["failed"] = failed;

                        String output;
                        serializeJson(response, output);
                        client->text(output);
                    }
                }

                // ============================================================
                // v1 API Parity - Device Info
                // ============================================================
                else if (type == "device.getStatus") {
                    StaticJsonDocument<512> response;
                    response["type"] = "device.status";
                    response["success"] = true;

                    JsonObject data = response.createNestedObject("data");
                    data["uptime"] = millis() / 1000;
                    data["freeHeap"] = ESP.getFreeHeap();
                    data["cpuFreq"] = ESP.getCpuFreqMHz();
                    data["wsClients"] = webServer.ws->count();

                    String output;
                    serializeJson(response, output);
                    client->text(output);
                }
                else if (type == "effects.getCurrent") {
                    extern Effect effects[];
                    StaticJsonDocument<256> response;
                    response["type"] = "effects.current";
                    response["success"] = true;

                    JsonObject data = response.createNestedObject("data");
                    data["effectId"] = currentEffect;
                    data["name"] = effects[currentEffect].name;
                    data["brightness"] = FastLED.getBrightness();
                    data["speed"] = effectSpeed;
                    data["paletteId"] = currentPaletteIndex;

                    String output;
                    serializeJson(response, output);
                    client->text(output);
                }
                else if (type == "effects.getMetadata") {
                    uint8_t effectId = doc["effectId"] | 0;
                    extern Effect effects[];

                    StaticJsonDocument<512> response;
                    response["type"] = "effects.metadata";

                    if (effectId >= NUM_EFFECTS) {
                        response["success"] = false;
                        response["error"] = "Effect ID out of range";
                    } else {
                        response["success"] = true;
                        JsonObject data = response.createNestedObject("data");
                        data["effectId"] = effectId;
                        data["name"] = effects[effectId].name;

                        EffectMeta meta = getEffectMeta(effectId);
                        char categoryBuf[24];
                        char descBuf[64];

                        getEffectCategoryName(effectId, categoryBuf, sizeof(categoryBuf));
                        data["category"] = categoryBuf;
                        data["categoryId"] = meta.category;

                        getEffectDescription(effectId, descBuf, sizeof(descBuf));
                        data["description"] = descBuf;

                        JsonObject features = data.createNestedObject("features");
                        features["centerOrigin"] = (meta.features & EffectFeatures::CENTER_ORIGIN) != 0;
                        features["usesSpeed"] = (meta.features & EffectFeatures::USES_SPEED) != 0;
                        features["usesPalette"] = (meta.features & EffectFeatures::USES_PALETTE) != 0;
                        features["zoneAware"] = (meta.features & EffectFeatures::ZONE_AWARE) != 0;
                        features["dualStrip"] = (meta.features & EffectFeatures::DUAL_STRIP) != 0;
                        features["physicsBased"] = (meta.features & EffectFeatures::PHYSICS_BASED) != 0;
                    }

                    String output;
                    serializeJson(response, output);
                    client->text(output);
                }
                else if (type == "effects.getCategories") {
                    StaticJsonDocument<512> response;
                    response["type"] = "effects.categories";
                    response["success"] = true;

                    JsonArray categories = response.createNestedArray("categories");
                    char categoryBuf[24];

                    for (uint8_t c = 0; c < CAT_COUNT; c++) {
                        strncpy_P(categoryBuf, (char*)pgm_read_ptr(&CATEGORY_NAMES[c]), sizeof(categoryBuf) - 1);
                        categoryBuf[sizeof(categoryBuf) - 1] = '\0';

                        JsonObject cat = categories.createNestedObject();
                        cat["id"] = c;
                        cat["name"] = categoryBuf;
                    }

                    String output;
                    serializeJson(response, output);
                    client->text(output);
                }
                else if (type == "parameters.get") {
                    StaticJsonDocument<256> response;
                    response["type"] = "parameters";
                    response["success"] = true;

                    JsonObject data = response.createNestedObject("data");
                    data["brightness"] = FastLED.getBrightness();
                    data["speed"] = effectSpeed;
                    data["paletteId"] = currentPaletteIndex;
                    data["intensity"] = effectIntensity;
                    data["saturation"] = effectSaturation;
                    data["complexity"] = effectComplexity;
                    data["variation"] = effectVariation;

                    String output;
                    serializeJson(response, output);
                    client->text(output);
                }
            }
            // Legacy "cmd" format for backward compatibility
            else if (doc.containsKey("cmd")) {
                String cmd = doc["cmd"];

                if (cmd == "effect") {
                    uint8_t effect = doc["value"];
                    if (effect < NUM_EFFECTS) {
                        startAdvancedTransition(effect);
                    }
                } else if (cmd == "brightness") {
                    uint8_t brightness = doc["value"];
                    FastLED.setBrightness(brightness);
                } else if (cmd == "palette") {
                    uint8_t palette = doc["value"];
                    if (palette < gGradientPaletteCount) {
                        currentPaletteIndex = palette;
                        targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
                    }
                }
            }

            // Broadcast updated status
            webServer.broadcastStatus();
        }
    }
}

// ============================================================================
// API v1 Implementation
// ============================================================================

bool LightwaveWebServer::checkRateLimit(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();
    if (!rateLimiter.checkHTTP(clientIP)) {
        sendErrorResponse(request, HttpStatus::TOO_MANY_REQUESTS,
                          ErrorCodes::RATE_LIMITED,
                          "Too many requests, please slow down");
        return false;
    }
    return true;
}

void LightwaveWebServer::setupV1Routes() {
    // ============================================================
    // API V1 ENDPOINTS
    // ============================================================

    // API Discovery - GET /api/v1/
    server->on("/api/v1/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleApiDiscovery(request);
    });

    // Device endpoints
    server->on("/api/v1/device/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleDeviceStatus(request);
    });

    server->on("/api/v1/device/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleDeviceInfo(request);
    });

    // Effects endpoints
    server->on("/api/v1/effects", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleEffectsList(request);
    });

    server->on("/api/v1/effects/current", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleEffectsCurrent(request);
    });

    server->on("/api/v1/effects/metadata", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleEffectMetadata(request);
    });

    server->on("/api/v1/effects/set", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!checkRateLimit(request)) return;
            handleEffectsSet(request, data, len);
        }
    );

    // Parameters endpoints
    server->on("/api/v1/parameters", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleParametersGet(request);
    });

    server->on("/api/v1/parameters", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!checkRateLimit(request)) return;
            handleParametersSet(request, data, len);
        }
    );

    // Transitions endpoints
    server->on("/api/v1/transitions/types", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleTransitionTypes(request);
    });

    server->on("/api/v1/transitions/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handleTransitionConfigGet(request);
    });

    server->on("/api/v1/transitions/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!checkRateLimit(request)) return;
            handleTransitionConfigSet(request, data, len);
        }
    );

    server->on("/api/v1/transitions/trigger", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!checkRateLimit(request)) return;
            handleTransitionTrigger(request, data, len);
        }
    );

    // Batch endpoint
    server->on("/api/v1/batch", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!checkRateLimit(request)) return;
            handleBatch(request, data, len);
        }
    );

    // OpenAPI Specification endpoint - serves JSON from PROGMEM
    server->on("/api/v1/openapi.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        // Stream the OpenAPI spec directly from PROGMEM
        AsyncWebServerResponse* response = request->beginResponse_P(
            200, "application/json",
            reinterpret_cast<const uint8_t*>(OPENAPI_SPEC),
            strlen_P(OPENAPI_SPEC));
        response->addHeader("Cache-Control", "public, max-age=3600");
        request->send(response);
    });
}

// ============================================================
// API Discovery
// ============================================================
void LightwaveWebServer::handleApiDiscovery(AsyncWebServerRequest* request) {
    sendSuccessResponseLarge(request, [](JsonObject& data) {
        data["name"] = "LightwaveOS";
        data["apiVersion"] = API_VERSION;
        data["description"] = "ESP32-S3 LED Control System";

        // Hardware info
        JsonObject hw = data.createNestedObject("hardware");
        hw["ledsTotal"] = HardwareConfig::TOTAL_LEDS;
        hw["strips"] = HardwareConfig::NUM_STRIPS;
        hw["centerPoint"] = HardwareConfig::STRIP_CENTER_POINT;
        hw["maxZones"] = HardwareConfig::MAX_ZONES;

        // HATEOAS links
        JsonObject links = data.createNestedObject("_links");
        links["self"] = "/api/v1/";
        links["device"] = "/api/v1/device/status";
        links["effects"] = "/api/v1/effects";
        links["parameters"] = "/api/v1/parameters";
        links["transitions"] = "/api/v1/transitions/types";
        links["batch"] = "/api/v1/batch";
        links["openapi"] = "/api/v1/openapi.json";
        links["websocket"] = "ws://lightwaveos.local/ws";
    }, 1024);
}

// ============================================================
// Device Endpoints
// ============================================================
void LightwaveWebServer::handleDeviceStatus(AsyncWebServerRequest* request) {
    WiFiManager& wifi = WiFiManager::getInstance();
    size_t wsClientCount = ws->count();

    sendSuccessResponse(request, [&wifi, wsClientCount](JsonObject& data) {
        data["uptime"] = millis() / 1000;
        data["freeHeap"] = ESP.getFreeHeap();
        data["heapSize"] = ESP.getHeapSize();
        data["cpuFreq"] = ESP.getCpuFreqMHz();

        JsonObject network = data.createNestedObject("network");
        network["connected"] = wifi.isConnected();
        network["apMode"] = wifi.isAPMode();
        if (wifi.isConnected()) {
            network["ip"] = WiFi.localIP().toString();
            network["rssi"] = WiFi.RSSI();
        }

        data["wsClients"] = wsClientCount;
    });
}

void LightwaveWebServer::handleDeviceInfo(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["firmware"] = "1.0.0";
        data["board"] = "ESP32-S3-DevKitC-1";
        data["sdk"] = ESP.getSdkVersion();
        data["flashSize"] = ESP.getFlashChipSize();
        data["sketchSize"] = ESP.getSketchSize();
        data["freeSketch"] = ESP.getFreeSketchSpace();
    });
}

// ============================================================
// Effects Endpoints
// ============================================================
void LightwaveWebServer::handleEffectsList(AsyncWebServerRequest* request) {
    extern Effect effects[];

    // Support pagination
    uint8_t start = 0;
    uint8_t count = 20;
    if (request->hasParam("start")) {
        start = request->getParam("start")->value().toInt();
    }
    if (request->hasParam("count")) {
        count = request->getParam("count")->value().toInt();
    }
    uint8_t end = min((uint8_t)(start + count), NUM_EFFECTS);

    // Check if detailed metadata requested
    bool includeDetails = request->hasParam("details");

    sendSuccessResponseLarge(request, [&](JsonObject& data) {
        data["total"] = NUM_EFFECTS;
        data["start"] = start;
        data["count"] = end - start;

        JsonArray effectsArr = data.createNestedArray("effects");
        char categoryBuf[24];
        char descBuf[64];

        for (uint8_t i = start; i < end; i++) {
            JsonObject effect = effectsArr.createNestedObject();
            effect["id"] = i;
            effect["name"] = effects[i].name;

            // Get metadata from PROGMEM
            EffectMeta meta = getEffectMeta(i);
            getEffectCategoryName(i, categoryBuf, sizeof(categoryBuf));
            effect["category"] = categoryBuf;
            effect["categoryId"] = meta.category;

            // Feature flags
            effect["centerOrigin"] = (meta.features & EffectFeatures::CENTER_ORIGIN) != 0;

            if (includeDetails) {
                getEffectDescription(i, descBuf, sizeof(descBuf));
                effect["description"] = descBuf;
                effect["usesSpeed"] = (meta.features & EffectFeatures::USES_SPEED) != 0;
                effect["usesPalette"] = (meta.features & EffectFeatures::USES_PALETTE) != 0;
                effect["zoneAware"] = (meta.features & EffectFeatures::ZONE_AWARE) != 0;
                effect["dualStrip"] = (meta.features & EffectFeatures::DUAL_STRIP) != 0;
                effect["physicsBased"] = (meta.features & EffectFeatures::PHYSICS_BASED) != 0;
            }
        }

        // Include category list for filtering UI
        JsonArray categories = data.createNestedArray("categories");
        for (uint8_t c = 0; c < CAT_COUNT; c++) {
            strncpy_P(categoryBuf, (char*)pgm_read_ptr(&CATEGORY_NAMES[c]), sizeof(categoryBuf) - 1);
            categoryBuf[sizeof(categoryBuf) - 1] = '\0';

            JsonObject cat = categories.createNestedObject();
            cat["id"] = c;
            cat["name"] = categoryBuf;
        }
    }, 3072);  // Larger buffer for detailed responses
}

void LightwaveWebServer::handleEffectsCurrent(AsyncWebServerRequest* request) {
    extern Effect effects[];

    sendSuccessResponse(request, [](JsonObject& data) {
        extern Effect effects[];
        data["effectId"] = currentEffect;
        data["name"] = effects[currentEffect].name;
        data["brightness"] = FastLED.getBrightness();
        data["speed"] = effectSpeed;
        data["paletteId"] = currentPaletteIndex;
    });
}

void LightwaveWebServer::handleEffectMetadata(AsyncWebServerRequest* request) {
    extern Effect effects[];

    // Get effect ID from query parameter
    if (!request->hasParam("id")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Effect ID required", "id");
        return;
    }

    uint8_t effectId = request->getParam("id")->value().toInt();
    if (effectId >= NUM_EFFECTS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "id");
        return;
    }

    char categoryBuf[24];
    char descBuf[64];

    sendSuccessResponse(request, [effectId, &categoryBuf, &descBuf](JsonObject& data) {
        extern Effect effects[];

        data["effectId"] = effectId;
        data["name"] = effects[effectId].name;

        // Get metadata from PROGMEM
        EffectMeta meta = getEffectMeta(effectId);

        getEffectCategoryName(effectId, categoryBuf, sizeof(categoryBuf));
        data["category"] = categoryBuf;
        data["categoryId"] = meta.category;

        getEffectDescription(effectId, descBuf, sizeof(descBuf));
        data["description"] = descBuf;

        // Feature flags object
        JsonObject features = data.createNestedObject("features");
        features["centerOrigin"] = (meta.features & EffectFeatures::CENTER_ORIGIN) != 0;
        features["usesSpeed"] = (meta.features & EffectFeatures::USES_SPEED) != 0;
        features["usesPalette"] = (meta.features & EffectFeatures::USES_PALETTE) != 0;
        features["zoneAware"] = (meta.features & EffectFeatures::ZONE_AWARE) != 0;
        features["dualStrip"] = (meta.features & EffectFeatures::DUAL_STRIP) != 0;
        features["physicsBased"] = (meta.features & EffectFeatures::PHYSICS_BASED) != 0;
        features["audioReactive"] = (meta.features & EffectFeatures::AUDIO_REACTIVE) != 0;

        // Raw features byte for compact storage
        data["featureFlags"] = meta.features;
    });
}

void LightwaveWebServer::handleEffectsSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    VALIDATE_REQUEST(doc, RequestSchemas::SetEffect, RequestSchemas::SetEffectSize, request);

    uint8_t effectId = doc["effectId"];
    if (effectId >= NUM_EFFECTS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "effectId");
        return;
    }

    startAdvancedTransition(effectId);

    sendSuccessResponse(request, [effectId](JsonObject& respData) {
        extern Effect effects[];
        respData["effectId"] = effectId;
        respData["name"] = effects[effectId].name;
    });

    broadcastStatus();
}

// ============================================================
// Parameters Endpoints
// ============================================================
void LightwaveWebServer::handleParametersGet(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["brightness"] = FastLED.getBrightness();
        data["speed"] = effectSpeed;
        data["paletteId"] = currentPaletteIndex;
        data["intensity"] = effectIntensity;
        data["saturation"] = effectSaturation;
        data["complexity"] = effectComplexity;
        data["variation"] = effectVariation;
    });
}

void LightwaveWebServer::handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    // Validate and apply parameters
    bool updated = false;

    if (doc.containsKey("brightness")) {
        uint8_t val = doc["brightness"];
        FastLED.setBrightness(val);
        updated = true;
    }

    if (doc.containsKey("speed")) {
        uint8_t val = doc["speed"];
        if (val >= 1 && val <= 50) {
            effectSpeed = val;
            updated = true;
        } else {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE, "Speed must be 1-50", "speed");
            return;
        }
    }

    if (doc.containsKey("paletteId")) {
        uint8_t val = doc["paletteId"];
        if (val < gGradientPaletteCount) {
            currentPaletteIndex = val;
            targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
            updated = true;
        } else {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE, "Invalid palette ID", "paletteId");
            return;
        }
    }

    if (doc.containsKey("intensity")) {
        effectIntensity = doc["intensity"];
        updated = true;
    }
    if (doc.containsKey("saturation")) {
        effectSaturation = doc["saturation"];
        updated = true;
    }
    if (doc.containsKey("complexity")) {
        effectComplexity = doc["complexity"];
        updated = true;
    }
    if (doc.containsKey("variation")) {
        effectVariation = doc["variation"];
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

// ============================================================
// Transitions Endpoints
// ============================================================
void LightwaveWebServer::handleTransitionTypes(AsyncWebServerRequest* request) {
    sendSuccessResponseLarge(request, [](JsonObject& data) {
        // Transition types
        JsonArray types = data.createNestedArray("types");
        const char* transNames[] = {
            "FADE", "WIPE_OUT", "WIPE_IN", "DISSOLVE", "PHASE_SHIFT",
            "PULSEWAVE", "IMPLOSION", "IRIS", "NUCLEAR", "STARGATE",
            "KALEIDOSCOPE", "MANDALA"
        };
        const char* transDescs[] = {
            "CENTER ORIGIN crossfade - radiates from center",
            "Wipe from center outward",
            "Wipe from edges inward",
            "Random pixel transition",
            "Frequency-based morph",
            "Concentric energy pulses from center",
            "Particles converge to center",
            "Mechanical aperture from center",
            "Chain reaction explosion from center",
            "Event horizon portal at center",
            "Symmetric crystal patterns",
            "Sacred geometry from center"
        };
        bool centerOrigin[] = {true, true, true, false, false, true, true, true, true, true, true, true};

        for (uint8_t i = 0; i < 12; i++) {
            JsonObject t = types.createNestedObject();
            t["id"] = i;
            t["name"] = transNames[i];
            t["description"] = transDescs[i];
            t["centerOrigin"] = centerOrigin[i];
        }

        // Easing curves
        JsonArray easing = data.createNestedArray("easingCurves");
        const char* easeNames[] = {
            "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
            "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
            "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC",
            "IN_BOUNCE", "OUT_BOUNCE",
            "IN_BACK", "OUT_BACK", "IN_OUT_BACK"
        };
        for (uint8_t i = 0; i < 15; i++) {
            JsonObject e = easing.createNestedObject();
            e["id"] = i;
            e["name"] = easeNames[i];
        }
    }, 2048);
}

void LightwaveWebServer::handleTransitionConfigGet(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["enabled"] = useRandomTransitions;
        data["defaultDuration"] = 1000;
        data["defaultType"] = 0;  // FADE
        data["defaultEasing"] = 3;  // IN_OUT_QUAD
    });
}

void LightwaveWebServer::handleTransitionConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    if (doc.containsKey("enabled")) {
        useRandomTransitions = doc["enabled"];
    }

    sendSuccessResponse(request);
    broadcastStatus();
}

void LightwaveWebServer::handleTransitionTrigger(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Malformed JSON");
        return;
    }

    VALIDATE_REQUEST(doc, RequestSchemas::TriggerTransition, RequestSchemas::TriggerTransitionSize, request);

    uint8_t toEffect = doc["toEffect"];
    if (toEffect >= NUM_EFFECTS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "toEffect");
        return;
    }

    uint8_t transType = doc["type"] | 0;
    uint32_t duration = doc["duration"] | 1000;

    // Use standard transition mechanism
    startAdvancedTransition(toEffect);

    sendSuccessResponse(request, [toEffect, transType, duration](JsonObject& respData) {
        extern Effect effects[];
        respData["effectId"] = toEffect;
        respData["name"] = effects[toEffect].name;
        respData["transitionType"] = transType;
        respData["duration"] = duration;
    });

    broadcastStatus();
}

// ============================================================
// Batch Operations
// ============================================================
void LightwaveWebServer::handleBatch(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
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
    if (ops.size() > 10) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Max 10 operations per batch");
        return;
    }

    StaticJsonDocument<1024> resultDoc;
    resultDoc["success"] = true;
    JsonObject resultData = resultDoc.createNestedObject("data");
    resultData["processed"] = 0;
    resultData["failed"] = 0;
    JsonArray results = resultData.createNestedArray("results");

    for (JsonVariant op : ops) {
        String action = op["action"] | "";
        JsonObject result = results.createNestedObject();
        result["action"] = action;

        bool success = executeBatchAction(action, op);
        result["success"] = success;

        if (success) {
            resultData["processed"] = resultData["processed"].as<int>() + 1;
        } else {
            resultData["failed"] = resultData["failed"].as<int>() + 1;
        }
    }

    resultDoc["timestamp"] = millis();
    resultDoc["version"] = API_VERSION;

    String output;
    serializeJson(resultDoc, output);
    request->send(HttpStatus::OK, "application/json", output);

    broadcastStatus();
}

bool LightwaveWebServer::executeBatchAction(const String& action, JsonVariant params) {
    if (action == "setBrightness") {
        if (!params.containsKey("value")) return false;
        uint8_t val = params["value"];
        FastLED.setBrightness(val);
        return true;
    }
    else if (action == "setSpeed") {
        if (!params.containsKey("value")) return false;
        uint8_t val = params["value"];
        if (val < 1 || val > 50) return false;
        effectSpeed = val;
        return true;
    }
    else if (action == "setEffect") {
        if (!params.containsKey("effectId")) return false;
        uint8_t id = params["effectId"];
        if (id >= NUM_EFFECTS) return false;
        startAdvancedTransition(id);
        return true;
    }
    else if (action == "setPalette") {
        if (!params.containsKey("paletteId")) return false;
        uint8_t id = params["paletteId"];
        if (id >= gGradientPaletteCount) return false;
        currentPaletteIndex = id;
        targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
        return true;
    }
    else if (action == "setZoneEffect") {
        if (!params.containsKey("zoneId") || !params.containsKey("effectId")) return false;
        uint8_t zoneId = params["zoneId"];
        uint8_t effectId = params["effectId"];
        if (zoneId >= HardwareConfig::MAX_ZONES || effectId >= NUM_EFFECTS) return false;
        zoneComposer.setZoneEffect(zoneId, effectId);
        return true;
    }
    else if (action == "setZoneBrightness") {
        if (!params.containsKey("zoneId") || !params.containsKey("brightness")) return false;
        uint8_t zoneId = params["zoneId"];
        uint8_t brightness = params["brightness"];
        if (zoneId >= HardwareConfig::MAX_ZONES) return false;
        zoneComposer.setZoneBrightness(zoneId, brightness);
        return true;
    }
    else if (action == "setZoneSpeed") {
        if (!params.containsKey("zoneId") || !params.containsKey("speed")) return false;
        uint8_t zoneId = params["zoneId"];
        uint8_t speed = params["speed"];
        if (zoneId >= HardwareConfig::MAX_ZONES || speed < 1 || speed > 50) return false;
        zoneComposer.setZoneSpeed(zoneId, speed);
        return true;
    }

    return false;
}

#endif // FEATURE_WEB_SERVER
