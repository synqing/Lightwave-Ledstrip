/**
 * Enhanced WebServer with WiFi Optimization
 * 
 * This is an enhanced version of the begin() method that uses
 * WiFi optimization techniques for better reception.
 */

#include "WebServer.h"
#include "WiFiOptimizer.h"
#include "../config/network_config.h"
#include <ESPmDNS.h>

bool LightwaveWebServer::beginEnhanced() {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return false;
    }
    
    // Apply WiFi optimizations for better reception
    WiFiOptimizer::optimizeForReception();
    
    // Try enhanced connection
    bool connected = WiFiOptimizer::connectWithEnhancedReliability(
        NetworkConfig::WIFI_SSID,
        NetworkConfig::WIFI_PASSWORD,
        60  // 30 seconds timeout
    );
    
    if (!connected) {
        Serial.println("\n=== Starting Access Point Mode ===");
        
        // Configure AP with optimizations
        WiFi.mode(WIFI_AP);
        
        // Set AP to use channel 6 (usually less congested)
        WiFi.softAP(NetworkConfig::AP_SSID, 
                    NetworkConfig::AP_PASSWORD,
                    6,     // Channel
                    false, // Hidden SSID
                    4);    // Max connections
        
        // Set max TX power for AP mode too
        WiFi.setTxPower(WIFI_POWER_20_5dBm);
        
        Serial.printf("AP SSID: %s\n", NetworkConfig::AP_SSID);
        Serial.printf("AP Password: %s\n", NetworkConfig::AP_PASSWORD);
        Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.println("Connect to this network to configure WiFi");
    } else {
        // Start mDNS for easier access
        if (MDNS.begin(NetworkConfig::MDNS_HOSTNAME)) {
            Serial.printf("✅ mDNS responder started: http://%s.local\n", NetworkConfig::MDNS_HOSTNAME);
            
            // Add service for discovery
            MDNS.addService("http", "tcp", NetworkConfig::WEB_SERVER_PORT);
            MDNS.addService("ws", "tcp", NetworkConfig::WEBSOCKET_PORT);
            
            // Add service text
            MDNS.addServiceTxt("http", "tcp", "version", "2.0");
            MDNS.addServiceTxt("http", "tcp", "type", "lightwaveos");
        }
    }
    
    // Configure web server
    configureRoutes();
    
    // Configure WebSocket
    ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
        handleWebSocketEvent(server, client, type, arg, data, len);
    });
    
    server->addHandler(ws);
    
    // Start server
    server->begin();
    
    Serial.println("\n✅ Enhanced Web Server Started");
    Serial.println("📱 Access URLs:");
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("   Main: http://%s/\n", WiFi.localIP().toString().c_str());
        Serial.printf("   Main: http://%s.local/\n", NetworkConfig::MDNS_HOSTNAME);
        Serial.printf("   Audio: http://%s/audio-sync/\n", WiFi.localIP().toString().c_str());
        Serial.printf("   WebSocket: ws://%s:%d/\n", WiFi.localIP().toString().c_str(), 
                     NetworkConfig::WEBSOCKET_PORT);
    } else {
        Serial.printf("   Main: http://%s/\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("   Setup: http://%s/wifi-setup\n", WiFi.softAPIP().toString().c_str());
    }
    
    return true;
}