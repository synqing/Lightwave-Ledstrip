#include "WebServer.h"
#include "../config/network_config.h"
#include "../config/hardware_config.h"
#include <FastLED.h>
#include <ESPmDNS.h>
#include <Update.h>

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
extern uint8_t currentPaletteIndex;
extern const TProgmemRGBGradientPaletteRef gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

// Forward declarations
void startAdvancedTransition(uint8_t newEffect);
void selectPalette(uint8_t paletteIndex);

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
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return false;
    }
    
    // Initialize WiFi
    // Pre-connection setup to avoid common ESP32 issues
    WiFi.disconnect(true);  // Clear any previous connection
    delay(100);
    WiFi.setHostname("LightwaveOS");  // Set hostname before connection
    
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(true);
    
    // Force power saving off for better connectivity
    WiFi.setSleep(false);
    
    // Set TX power to maximum for better range
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    
    // Enable WiFi debugging
    Serial.println("\n=== WiFi Debug Info ===");
    Serial.printf("Attempting to connect to SSID: %s\n", NetworkConfig::WIFI_SSID);
    Serial.printf("Password length: %d characters\n", strlen(NetworkConfig::WIFI_PASSWORD));
    Serial.printf("Password (hidden): ");
    for(int i = 0; i < strlen(NetworkConfig::WIFI_PASSWORD); i++) {
        Serial.print(i < 2 || i >= strlen(NetworkConfig::WIFI_PASSWORD)-2 ? 
                    NetworkConfig::WIFI_PASSWORD[i] : '*');
    }
    Serial.println();
    Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
    
    WiFi.begin(NetworkConfig::WIFI_SSID, NetworkConfig::WIFI_PASSWORD);
    
    Serial.print("Connecting to WiFi");
    int attempts = 0;
    bool tryBSSID = false;
    uint8_t targetBSSID[6] = {0};
    
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {  // Increased attempts
        delay(500);
        
        // Print detailed status every 2 seconds
        if (attempts % 4 == 0) {
            wl_status_t status = WiFi.status();
            Serial.printf("\n[%d sec] Status: ", attempts/2);
            switch(status) {
                case WL_NO_SHIELD: Serial.print("NO_SHIELD"); break;
                case WL_IDLE_STATUS: Serial.print("IDLE"); break;
                case WL_NO_SSID_AVAIL: Serial.print("NO_SSID_AVAIL"); break;
                case WL_SCAN_COMPLETED: Serial.print("SCAN_COMPLETED"); break;
                case WL_CONNECTED: Serial.print("CONNECTED"); break;
                case WL_CONNECT_FAILED: Serial.print("CONNECT_FAILED"); break;
                case WL_CONNECTION_LOST: Serial.print("CONNECTION_LOST"); break;
                case WL_DISCONNECTED: Serial.print("DISCONNECTED"); break;
                default: Serial.printf("UNKNOWN(%d)", status); break;
            }
            
            // Scan for networks to check signal strength
            int n = WiFi.scanNetworks(false, true, false, 300);
            bool networkFound = false;
            for (int i = 0; i < n; i++) {
                if (WiFi.SSID(i) == NetworkConfig::WIFI_SSID) {
                    networkFound = true;
                    Serial.printf("\n  Found target network:");
                    Serial.printf("\n    RSSI: %d dBm (Signal: %s)", WiFi.RSSI(i), 
                        WiFi.RSSI(i) > -50 ? "Excellent" :
                        WiFi.RSSI(i) > -60 ? "Good" :
                        WiFi.RSSI(i) > -70 ? "Fair" : "Weak");
                    Serial.printf("\n    Channel: %d", WiFi.channel(i));
                    Serial.printf("\n    BSSID: %s", WiFi.BSSIDstr(i).c_str());
                    
                    // Store BSSID for alternative connection attempt
                    if (!tryBSSID && attempts > 12) {  // After 6 seconds, try BSSID method
                        uint8_t* bssid = WiFi.BSSID(i);
                        if (bssid) {
                            memcpy(targetBSSID, bssid, 6);
                        }
                        tryBSSID = true;
                    }
                    
                    Serial.printf("\n    Encryption: ");
                    switch(WiFi.encryptionType(i)) {
                        case WIFI_AUTH_OPEN: Serial.print("Open"); break;
                        case WIFI_AUTH_WEP: Serial.print("WEP"); break;
                        case WIFI_AUTH_WPA_PSK: Serial.print("WPA-PSK"); break;
                        case WIFI_AUTH_WPA2_PSK: Serial.print("WPA2-PSK"); break;
                        case WIFI_AUTH_WPA_WPA2_PSK: Serial.print("WPA/WPA2-PSK"); break;
                        case WIFI_AUTH_WPA2_ENTERPRISE: Serial.print("WPA2-Enterprise"); break;
                        case WIFI_AUTH_WPA3_PSK: Serial.print("WPA3-PSK"); break;
                        case WIFI_AUTH_WPA2_WPA3_PSK: Serial.print("WPA2/WPA3-PSK"); break;
                        default: Serial.printf("Unknown(%d)", WiFi.encryptionType(i)); break;
                    }
                    break;
                }
            }
            
            if (!networkFound) {
                Serial.printf("\n  ⚠️  Network '%s' NOT FOUND in scan!", NetworkConfig::WIFI_SSID);
                Serial.printf("\n  Networks found: %d", n);
                if (n > 0 && n <= 5) {
                    Serial.println("\n  Available networks:");
                    for (int i = 0; i < n; i++) {
                        Serial.printf("    - %s (RSSI: %d, Ch: %d)\n", 
                                    WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
                    }
                }
            }
            
            // Try alternative connection with BSSID
            if (tryBSSID && attempts == 16) {
                Serial.println("\n  Trying connection with explicit BSSID...");
                WiFi.disconnect();
                delay(100);
                WiFi.begin(NetworkConfig::WIFI_SSID, NetworkConfig::WIFI_PASSWORD, 0, targetBSSID);
            }
            
            Serial.print("\nContinuing");
        } else {
            Serial.print(".");
        }
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n\n=== WiFi Connection Failed ===");
        Serial.printf("Final status: %d\n", WiFi.status());
        Serial.println("Common causes:");
        Serial.println("  1. Incorrect password (check for spaces/typos)");
        Serial.println("  2. MAC filtering enabled on router");
        Serial.println("  3. WPA3-only network (ESP32 needs WPA2)");
        Serial.println("  4. 5GHz-only network (ESP32 needs 2.4GHz)");
        Serial.println("  5. Router rejecting due to DHCP pool exhaustion");
        // Start AP mode as fallback
        WiFi.mode(WIFI_AP);
        WiFi.softAP(NetworkConfig::AP_SSID, NetworkConfig::AP_PASSWORD);
        Serial.println("\n=== Access Point Started ===");
        Serial.printf("SSID: %s\n", NetworkConfig::AP_SSID);
        Serial.printf("Password: %s\n", NetworkConfig::AP_PASSWORD);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
        // Note: mDNS is not started in AP mode
    } else {
        Serial.println("\n\n=== WiFi Connected Successfully ===");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("Subnet: %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("Channel: %d\n", WiFi.channel());
        Serial.printf("BSSID: %s\n", WiFi.BSSIDstr().c_str());
        Serial.printf("Hostname: %s\n", WiFi.getHostname());

        // === mDNS (Bonjour) Initialization ===
        if (MDNS.begin(NetworkConfig::MDNS_HOSTNAME)) {
            Serial.printf("mDNS responder started: http://%s.local\n", NetworkConfig::MDNS_HOSTNAME);
            MDNS.addService("http", "tcp", NetworkConfig::WEB_SERVER_PORT);
        } else {
            Serial.println("Error setting up mDNS responder!");
        }
    }
    
    // Setup WebSocket
    ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
        onWebSocketEvent(server, client, type, arg, data, len);
    });
    server->addHandler(ws);
    
    // Serve static files
    server->serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    
    // Register OTA update endpoint
    server->on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool shouldReboot = !Update.hasError();
        request->send(200, "text/plain", shouldReboot ? "OK" : "FAIL");
        if (shouldReboot) {
            Serial.println("[OTA] Rebooting after successful update...");
            delay(1000);
            ESP.restart();
        }
    }, std::bind(&LightwaveWebServer::handleOTAUpdate, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    
    // Start server
    server->begin();
    Serial.println("Web server started on port 80");
    
    return true;
}

void LightwaveWebServer::stop() {
    server->end();
    WiFi.disconnect();
}

void LightwaveWebServer::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                             AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", 
                         client->id(), client->remoteIP().toString().c_str());
            // Send initial state to new client
            broadcastState();
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
            
        case WS_EVT_ERROR:
            Serial.printf("WebSocket error from client #%u: %s\n", client->id(), (char*)data);
            break;
            
        case WS_EVT_DATA:
            {
                // Parse JSON command
                DynamicJsonDocument doc(JSON_DOC_SIZE);
                DeserializationError error = deserializeJson(doc, data, len);
                
                if (error) {
                    Serial.print("JSON parse error: ");
                    Serial.println(error.c_str());
                    notifyError("Invalid JSON");
                    return;
                }
                
                handleCommand(client, doc);
            }
            break;
    }
}

void LightwaveWebServer::handleCommand(AsyncWebSocketClient* client, const JsonDocument& doc) {
    const char* cmd = doc["command"];
    if (!cmd) {
        notifyError("Missing command");
        return;
    }
    
    Serial.printf("WebSocket command: %s\n", cmd);
    
    if (strcmp(cmd, "get_state") == 0) {
        broadcastState();
    }
    else if (strcmp(cmd, "set_parameter") == 0) {
        handleSetParameter(doc);
    }
    else if (strcmp(cmd, "set_effect") == 0) {
        handleSetEffect(doc);
    }
    else if (strcmp(cmd, "set_palette") == 0) {
        handleSetPalette(doc);
    }
    else if (strcmp(cmd, "toggle_power") == 0) {
        handleTogglePower();
    }
    else if (strcmp(cmd, "emergency_stop") == 0) {
        handleEmergencyStop();
    }
    else if (strcmp(cmd, "save_preset") == 0) {
        handleSavePreset(doc);
    }
    else if (strcmp(cmd, "toggle_sync") == 0) {
        // Handle sync toggle
        bool enabled = doc["enabled"] | false;
        Serial.printf("Sync %s\n", enabled ? "enabled" : "disabled");
    }
}

void LightwaveWebServer::handleSetParameter(const JsonDocument& doc) {
    const char* param = doc["parameter"];
    if (!param) return;
    
    if (strcmp(param, "brightness") == 0) {
        uint8_t value = doc["value"];
        FastLED.setBrightness(value);
        Serial.printf("Brightness set to %d\n", value);
    }
    else if (strcmp(param, "speed") == 0) {
        // Handle speed parameter
        uint8_t value = doc["value"];
        // Apply to appropriate effect parameter
    }
    else if (strcmp(param, "random-transitions") == 0) {
        useRandomTransitions = doc["value"];
        Serial.printf("Random transitions %s\n", useRandomTransitions ? "enabled" : "disabled");
    }
    else if (strcmp(param, "optimized-effects") == 0) {
        useOptimizedEffects = doc["value"];
        Serial.printf("Optimized effects %s\n", useOptimizedEffects ? "enabled" : "disabled");
    }
    
    broadcastState();
}

void LightwaveWebServer::handleSetEffect(const JsonDocument& doc) {
    uint8_t effectId = doc["effect"];
    if (effectId < NUM_EFFECTS) {
        startAdvancedTransition(effectId);
        notifyEffectChange(effectId);
    }
}

void LightwaveWebServer::handleSetPalette(const JsonDocument& doc) {
    uint8_t paletteId = doc["palette"];
    if (paletteId < gGradientPaletteCount) {
        currentPaletteIndex = paletteId;
        currentPalette = CRGBPalette16(gGradientPalettes[paletteId]);
        Serial.printf("Palette set to %d\n", paletteId);
        broadcastState();
    }
}

void LightwaveWebServer::handleTogglePower() {
    static bool powerOn = true;
    powerOn = !powerOn;
    
    if (powerOn) {
        FastLED.setBrightness(HardwareConfig::STRIP_BRIGHTNESS);
    } else {
        FastLED.setBrightness(0);
    }
    
    Serial.printf("Power %s\n", powerOn ? "ON" : "OFF");
    broadcastState();
}

void LightwaveWebServer::handleEmergencyStop() {
    FastLED.clear(true);
    FastLED.show();
    Serial.println("Emergency stop activated");
}

void LightwaveWebServer::handleSavePreset(const JsonDocument& doc) {
    const char* name = doc["name"];
    if (name) {
        // Save preset to SPIFFS
        Serial.printf("Saving preset: %s\n", name);
        // TODO: Implement preset saving
    }
}

void LightwaveWebServer::broadcastState() {
    if (!hasClients()) return;
    
    DynamicJsonDocument doc(JSON_DOC_SIZE);
    doc["type"] = "state";
    doc["currentEffect"] = currentEffect;
    doc["brightness"] = FastLED.getBrightness();
    doc["randomTransitions"] = useRandomTransitions;
    doc["optimizedEffects"] = useOptimizedEffects;
    doc["paletteIndex"] = currentPaletteIndex;
    doc["fps"] = 0; // Will be updated by performance metrics
    doc["heap"] = ESP.getFreeHeap();
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void LightwaveWebServer::broadcastPerformance() {
    if (!hasClients()) return;
    
    static uint32_t lastBroadcast = 0;
    if (millis() - lastBroadcast < 1000) return; // Limit to 1Hz
    lastBroadcast = millis();
    
    DynamicJsonDocument doc(512);
    doc["type"] = "performance";
    
    // Calculate performance metrics
    static uint32_t lastFrameTime = 0;
    uint32_t frameTime = millis() - lastFrameTime;
    lastFrameTime = millis();
    
    doc["frameTime"] = frameTime;
    doc["cpuUsage"] = 0; // TODO: Calculate actual CPU usage
    doc["optimizationGain"] = useOptimizedEffects ? 1.5 : 1.0;
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void LightwaveWebServer::broadcastLEDData() {
    if (!hasClients()) return;
    
    static uint32_t lastBroadcast = 0;
    if (millis() - lastBroadcast < 50) return; // Limit to 20Hz
    lastBroadcast = millis();
    
    DynamicJsonDocument doc(8192); // Large buffer for LED data
    doc["type"] = "led_data";
    
    JsonArray leds = doc.createNestedArray("leds");
    
    // Send every 4th LED to reduce data size (80 LEDs total)
    for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i += 4) {
        JsonObject led = leds.createNestedObject();
        led["r"] = strip1[i].r;
        led["g"] = strip1[i].g;
        led["b"] = strip1[i].b;
    }
    
    for (int i = 0; i < HardwareConfig::STRIP2_LED_COUNT; i += 4) {
        JsonObject led = leds.createNestedObject();
        led["r"] = strip2[i].r;
        led["g"] = strip2[i].g;
        led["b"] = strip2[i].b;
    }
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void LightwaveWebServer::update() {
    ws->cleanupClients();
    
    // Send periodic updates
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 100) {
        lastUpdate = millis();
        broadcastPerformance();
        broadcastLEDData();
    }
}

void LightwaveWebServer::sendLEDUpdate() {
    broadcastLEDData();
}

void LightwaveWebServer::notifyEffectChange(uint8_t effectId) {
    if (!hasClients()) return;
    
    DynamicJsonDocument doc(256);
    doc["type"] = "effect_change";
    doc["effect"] = effectId;
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void LightwaveWebServer::notifyError(const String& message) {
    if (!hasClients()) return;
    
    DynamicJsonDocument doc(256);
    doc["type"] = "error";
    doc["message"] = message;
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

// OTA firmware update handler (chunked upload)
void LightwaveWebServer::handleOTAUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("[OTA] Update Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }
    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    if (final) {
        if (Update.end(true)) {
            Serial.printf("[OTA] Update Success: %u bytes\n", index + len);
        } else {
            Update.printError(Serial);
        }
    }
} 