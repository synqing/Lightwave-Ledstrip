/**
 * Network Debug Helper
 * Add this to help diagnose connection issues
 */

#include <WiFi.h>
#include <ESPmDNS.h>

void debugNetworkStatus() {
    Serial.println("\n=== NETWORK DEBUG INFO ===");
    
    // WiFi Status
    Serial.print("WiFi Status: ");
    switch(WiFi.status()) {
        case WL_NO_SHIELD: Serial.println("NO_SHIELD"); break;
        case WL_IDLE_STATUS: Serial.println("IDLE"); break;
        case WL_NO_SSID_AVAIL: Serial.println("NO_SSID_AVAIL - Network not found!"); break;
        case WL_SCAN_COMPLETED: Serial.println("SCAN_COMPLETED"); break;
        case WL_CONNECTED: Serial.println("CONNECTED ✅"); break;
        case WL_CONNECT_FAILED: Serial.println("CONNECT_FAILED - Check password!"); break;
        case WL_CONNECTION_LOST: Serial.println("CONNECTION_LOST"); break;
        case WL_DISCONNECTED: Serial.println("DISCONNECTED"); break;
        default: Serial.printf("UNKNOWN(%d)\n", WiFi.status());
    }
    
    // Connection Details
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Connected to: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
        Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
        
        // mDNS Status
        if (MDNS.begin("lightwaveos")) {
            Serial.println("mDNS: Active at lightwaveos.local");
        } else {
            Serial.println("mDNS: Failed to start");
        }
        
        // URLs
        Serial.println("\n📱 Access URLs:");
        Serial.printf("Main UI: http://%s/\n", WiFi.localIP().toString().c_str());
        Serial.printf("Audio Sync: http://%s/audio-sync/\n", WiFi.localIP().toString().c_str());
        Serial.printf("WebSocket: ws://%s:81/\n", WiFi.localIP().toString().c_str());
        
    } else if (WiFi.getMode() == WIFI_AP) {
        Serial.println("\nAccess Point Mode Active:");
        Serial.printf("SSID: %s\n", WiFi.softAPSSID().c_str());
        Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("Connected Clients: %d\n", WiFi.softAPgetStationNum());
    }
    
    // Network Scan
    Serial.println("\nScanning for networks...");
    int n = WiFi.scanNetworks();
    Serial.printf("Found %d networks:\n", n);
    for (int i = 0; i < min(n, 10); i++) {
        Serial.printf("  %d. %s (Ch:%d, %ddBm) %s\n", 
                     i+1, 
                     WiFi.SSID(i).c_str(), 
                     WiFi.channel(i),
                     WiFi.RSSI(i),
                     WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURED");
    }
    
    Serial.println("========================\n");
}

// Call this from main.cpp by adding:
// extern void debugNetworkStatus();
// Then in loop() or setup(), call: debugNetworkStatus();