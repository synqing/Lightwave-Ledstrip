#include "WebServer.h"

#if FEATURE_WEB_SERVER

#include "WiFiManager.h"
#include "../config/network_config.h"
#include "../config/hardware_config.h"
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
            DeserializationError error = deserializeJson(doc, data);
            
            if (!error && doc.containsKey("effect")) {
                uint8_t newEffect = doc["effect"];
                if (newEffect < NUM_EFFECTS) {
                    startAdvancedTransition(newEffect);
                    request->send(200, "application/json", "{\"status\":\"ok\"}");
                    return;
                }
            }
            request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
        }
    );
    
    // Brightness control
    server->on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest *request){}, 
        NULL, 
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data);
            
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
            DeserializationError error = deserializeJson(doc, data);
            
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
    
    // OTA update endpoint
    server->on("/update", HTTP_POST, 
        [this](AsyncWebServerRequest *request){
            shouldReboot = !Update.hasError();
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", 
                shouldReboot ? "OK" : "FAIL");
            response->addHeader("Connection", "close");
            request->send(response);
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            if(!index){
                Serial.printf("Update Start: %s\n", filename.c_str());
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
                    Serial.printf("Update Success: %uB\n", index+len);
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
            DeserializationError error = deserializeJson(doc, data);

            if (!error && doc.containsKey("paletteId")) {
                extern uint8_t currentPaletteIndex;
                currentPaletteIndex = doc["paletteId"];
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

        // TODO: Get actual palette count
        extern const uint8_t gGradientPaletteCount;
        for (uint8_t i = 0; i < gGradientPaletteCount; i++) {
            JsonObject palette = palettes.createNestedObject();
            palette["id"] = i;
            palette["name"] = String("Palette ") + i;
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
            DeserializationError error = deserializeJson(doc, data);

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

                if (doc.containsKey("effectId")) {
                    uint8_t effectId = doc["effectId"];
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
    // Register audio-specific web handlers
    // TODO: Implement registerAudioWebHandlers
    // registerAudioWebHandlers(server, ws);
#endif

    // 404 handler
    server->onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
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

// WebSocket event handler
void LightwaveWebServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WebSocket] Client connected: %s\n", client->remoteIP().toString().c_str());
        // Send initial status to new client
        webServer.broadcastStatus();
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WebSocket] Client disconnected\n");
    } else if (type == WS_EVT_DATA) {
        // Handle WebSocket data
        String message = "";
        for (size_t i = 0; i < len; i++) {
            message += (char)data[i];
        }
        
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, message);

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

                // Zone Composer control
                else if (type == "zone.enable") {
                    bool enable = doc["enable"];
                    if (enable) zoneComposer.enable();
                    else zoneComposer.disable();
                } else if (type == "zone.setCount") {
                    uint8_t count = doc["count"];
                    zoneComposer.setZoneCount(count);
                } else if (type == "zone.setEffect") {
                    uint8_t zoneId = doc["zoneId"];
                    uint8_t effectId = doc["effectId"];
                    zoneComposer.setZoneEffect(zoneId, effectId);
                } else if (type == "zone.enableZone") {
                    uint8_t zoneId = doc["zoneId"];
                    bool enabled = doc["enabled"];
                    zoneComposer.enableZone(zoneId, enabled);
                } else if (type == "zone.loadPreset") {
                    uint8_t presetId = doc["presetId"];
                    zoneComposer.loadPreset(presetId);
                } else if (type == "zone.save") {
                    zoneComposer.saveConfig();
                } else if (type == "zone.load") {
                    zoneComposer.loadConfig();
                } else if (type == "zone.setBrightness") {
                    uint8_t zoneId = doc["zoneId"];
                    uint8_t brightness = doc["brightness"];
                    zoneComposer.setZoneBrightness(zoneId, brightness);
                } else if (type == "zone.setSpeed") {
                    uint8_t zoneId = doc["zoneId"];
                    uint8_t speed = doc["speed"];
                    zoneComposer.setZoneSpeed(zoneId, speed);
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
#endif // FEATURE_WEB_SERVER
