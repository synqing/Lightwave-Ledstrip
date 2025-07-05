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

// External references from main.cpp
extern uint8_t currentEffect;
extern uint8_t gHue;
extern CRGB strip1[];
extern CRGB strip2[];
extern bool useOptimizedEffects;
extern bool useRandomTransitions;

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
        2048,
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
        // Initialize mDNS
        if (MDNS.begin(NetworkConfig::MDNS_HOSTNAME)) {
            Serial.printf("[WebServer] mDNS responder started: http://%s.local\n", NetworkConfig::MDNS_HOSTNAME);
            
            // Add service to mDNS-SD
            MDNS.addService("http", "tcp", NetworkConfig::WEB_SERVER_PORT);
            MDNS.addService("ws", "tcp", NetworkConfig::WEB_SERVER_PORT);
            
            // Add some TXT records
            MDNS.addServiceTxt("http", "tcp", "version", "1.0.0");
            MDNS.addServiceTxt("http", "tcp", "board", "ESP32-S3");
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
        
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error) {
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
            
            // Broadcast updated status
            webServer.broadcastStatus();
        }
    }
}
#endif // FEATURE_WEB_SERVER
