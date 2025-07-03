/**
 * WiFi Integration Example
 * Shows how to use WiFiOptimizerPro in your LightwaveOS project
 */

#include "WiFiOptimizerPro.h"
#include "WebServer.h"
#include "../config/network_config.h"

// Example of enhanced WebServer begin method
bool LightwaveWebServer::beginOptimized() {
    Serial.println("\n=== LightwaveOS WiFi Initialization ===");
    
    // Initialize SPIFFS first
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ Failed to mount SPIFFS");
        return false;
    }
    Serial.println("✅ SPIFFS mounted");
    
    // Initialize WiFi with advanced optimizations
    if (!WiFiOptimizerPro::initializeAdvanced()) {
        Serial.println("❌ WiFi initialization failed");
        return false;
    }
    
    // Connect with all optimizations
    bool connected = WiFiOptimizerPro::connectOptimized(
        NetworkConfig::WIFI_SSID,
        NetworkConfig::WIFI_PASSWORD
    );
    
    if (!connected) {
        Serial.println("\n❌ Station mode failed, starting AP mode...");
        
        // Fallback to AP mode with optimizations
        WiFi.mode(WIFI_AP);
        
        // Configure AP with optimal settings
        WiFi.softAPConfig(
            IPAddress(192, 168, 4, 1),
            IPAddress(192, 168, 4, 1),
            IPAddress(255, 255, 255, 0)
        );
        
        // Start AP on optimal channel
        uint8_t channel = WiFiOptimizerPro::selectBestChannel();
        if (channel == 0) channel = 6;  // Default if scan failed
        
        WiFi.softAP(
            NetworkConfig::AP_SSID,
            NetworkConfig::AP_PASSWORD,
            channel,
            false,  // Not hidden
            4       // Max connections
        );
        
        // Set AP to max power too
        esp_wifi_set_max_tx_power(80);  // 20 dBm
        
        Serial.println("\n✅ Access Point started");
        Serial.printf("   SSID: %s\n", NetworkConfig::AP_SSID);
        Serial.printf("   Password: %s\n", NetworkConfig::AP_PASSWORD);
        Serial.printf("   Channel: %d\n", channel);
        Serial.printf("   IP: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        // Station mode successful - start mDNS
        if (MDNS.begin(NetworkConfig::MDNS_HOSTNAME)) {
            Serial.printf("✅ mDNS started: http://%s.local\n", NetworkConfig::MDNS_HOSTNAME);
            
            // Add services
            MDNS.addService("http", "tcp", NetworkConfig::WEB_SERVER_PORT);
            MDNS.addService("ws", "tcp", NetworkConfig::WEBSOCKET_PORT);
            
            // Add TXT records for discovery
            MDNS.addServiceTxt("http", "tcp", "version", "2.0");
            MDNS.addServiceTxt("http", "tcp", "features", "audio-sync");
            MDNS.addServiceTxt("http", "tcp", "board", "esp32-s3");
        }
    }
    
    // Configure routes and start web server
    configureRoutes();
    
    // Configure WebSocket with optimized settings
    ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
        handleWebSocketEvent(server, client, type, arg, data, len);
    });
    
    // Enable compression for better performance
    ws->enable(true);
    
    // Add handler
    server->addHandler(ws);
    
    // Start server
    server->begin();
    
    // Print optimization status
    WiFiOptimizerPro::printStatus();
    
    // Print access information
    Serial.println("\n📱 Access Information:");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("├─ URLs:");
        Serial.printf("│  ├─ http://%s/\n", WiFi.localIP().toString().c_str());
        Serial.printf("│  ├─ http://%s.local/\n", NetworkConfig::MDNS_HOSTNAME);
        Serial.printf("│  └─ ws://%s:81/\n", WiFi.localIP().toString().c_str());
        Serial.println("├─ Audio Sync Portal:");
        Serial.printf("│  └─ http://%s/audio-sync/\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("├─ Access Point URLs:");
        Serial.printf("│  ├─ http://%s/\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("│  └─ http://%s/wifi-setup\n", WiFi.softAPIP().toString().c_str());
    }
    Serial.println("└─ Status: Ready ✅\n");
    
    return true;
}

// Example for main.cpp integration
void setupWiFiOptimized() {
    // Enable verbose WiFi debugging during development
    esp_log_level_set("wifi", ESP_LOG_VERBOSE);
    
    // Initialize web server with optimizations
    if (webServer.beginOptimized()) {
        Serial.println("✅ Web server started with WiFi optimizations");
    } else {
        Serial.println("⚠️  Web server failed to start");
    }
}

// Add to main loop for monitoring
void loopWiFiMaintenance() {
    static unsigned long lastCheck = 0;
    static unsigned long lastStatusPrint = 0;
    
    // Run maintenance every 30 seconds
    if (millis() - lastCheck > 30000) {
        lastCheck = millis();
        
        // The adaptive power control runs in its own task
        // But we can add additional monitoring here
        
        if (WiFi.status() == WL_CONNECTED) {
            int rssi = WiFi.RSSI();
            
            // Alert on signal degradation
            static int lastRSSI = 0;
            if (lastRSSI != 0 && rssi < lastRSSI - 10) {
                Serial.printf("⚠️  Signal degraded: %d → %d dBm\n", lastRSSI, rssi);
            }
            lastRSSI = rssi;
            
            // Force reconnect on very poor signal
            if (rssi < -85) {
                Serial.println("📡 Signal too weak, attempting reconnection...");
                WiFi.reconnect();
            }
        }
    }
    
    // Print detailed status every 5 minutes
    if (millis() - lastStatusPrint > 300000) {
        lastStatusPrint = millis();
        WiFiOptimizerPro::printStatus();
    }
}

// Example: Command to test different TX power levels
void testTxPowerLevels() {
    Serial.println("\n=== TX Power Level Test ===");
    
    int8_t testLevels[] = {8, 11, 14, 17, 20};  // dBm
    
    for (int8_t level : testLevels) {
        esp_wifi_set_max_tx_power(level * 4);  // Convert to 0.25 dBm units
        delay(1000);  // Let it stabilize
        
        int rssi = WiFi.RSSI();
        Serial.printf("TX Power: %d dBm → RSSI: %d dBm", level, rssi);
        
        // Estimate power consumption
        float mW = pow(10, level / 10.0);
        Serial.printf(" (%.1f mW)\n", mW);
        
        delay(2000);
    }
    
    // Restore adaptive control
    Serial.println("Restoring adaptive power control...");
}