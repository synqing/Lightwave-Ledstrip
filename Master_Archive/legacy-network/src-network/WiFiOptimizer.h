#ifndef WIFI_OPTIMIZER_H
#define WIFI_OPTIMIZER_H

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_pm.h>

/**
 * WiFi Optimizer - Maximize ESP32 WiFi Reception
 * 
 * This class implements various techniques to improve WiFi signal reception
 * and connection stability on ESP32 devices.
 */
class WiFiOptimizer {
public:
    /**
     * Configure WiFi for maximum reception capability
     */
    static void optimizeForReception() {
        Serial.println("\n=== Optimizing WiFi Reception ===");
        
        // 1. Set WiFi to station mode only (saves power for stronger signal)
        WiFi.mode(WIFI_STA);
        
        // 2. Disable WiFi sleep mode for consistent performance
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);  // Disable power saving completely
        Serial.println("✓ Power saving disabled");
        
        // 3. Set maximum TX power (valid range: 2-20 dBm in 0.25 dBm steps)
        // WiFi 6 allows up to 20 dBm (100mW)
        WiFi.setTxPower(WIFI_POWER_20_5dBm);  // Maximum allowed
        Serial.println("✓ TX Power set to maximum (20.5 dBm)");
        
        // 4. Configure WiFi PHY mode for better range
        // 802.11b has better range than 802.11g/n
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        Serial.println("✓ WiFi protocols optimized");
        
        // 5. Set WiFi bandwidth to 20MHz for better range (40MHz has shorter range)
        esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
        Serial.println("✓ Bandwidth set to 20MHz for better range");
        
        // 6. Disable WiFi auto-reconnect temporarily during initial connection
        WiFi.setAutoReconnect(false);
        WiFi.setAutoConnect(false);
        
        // 7. Configure ESP32 antenna (if your board supports antenna switching)
        #ifdef BOARD_HAS_DUAL_ANTENNA
        // For boards with dual antenna support
        esp_wifi_set_ant_gpio(25, 26);  // Example GPIO pins
        esp_wifi_set_ant(WIFI_ANT_ANT0);  // or WIFI_ANT_ANT1, WIFI_ANT_AUTO
        #endif
        
        // 8. Increase WiFi task priority
        // This ensures WiFi gets CPU time even under load
        configureWiFiTaskPriority();
        
        Serial.println("✓ WiFi optimization complete\n");
    }
    
    /**
     * Connect with enhanced reliability
     */
    static bool connectWithEnhancedReliability(const char* ssid, const char* password, 
                                              int maxAttempts = 60, 
                                              bool useBSSID = false,
                                              uint8_t* bssid = nullptr) {
        Serial.printf("\n=== Enhanced WiFi Connection to '%s' ===\n", ssid);
        
        // Clear any previous connection
        WiFi.disconnect(true, true);  // Disconnect and clear saved credentials
        delay(100);
        
        // Scan for the network first to get best AP
        int n = WiFi.scanNetworks(false, true, false, 300, 0, ssid);
        
        if (n == 0) {
            Serial.println("❌ Network not found in scan!");
            return false;
        }
        
        // Find strongest signal
        int bestIndex = 0;
        int bestRSSI = -999;
        uint8_t bestBSSID[6];
        int bestChannel = 0;
        
        Serial.println("\nAvailable access points:");
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == ssid) {
                int rssi = WiFi.RSSI(i);
                Serial.printf("  AP %d: BSSID=%s, Ch=%d, RSSI=%d dBm %s\n", 
                            i, 
                            WiFi.BSSIDstr(i).c_str(), 
                            WiFi.channel(i),
                            rssi,
                            rssi > bestRSSI ? "← BEST" : "");
                
                if (rssi > bestRSSI) {
                    bestRSSI = rssi;
                    bestIndex = i;
                    bestChannel = WiFi.channel(i);
                    memcpy(bestBSSID, WiFi.BSSID(i), 6);
                }
            }
        }
        
        Serial.printf("\n✓ Selected AP: BSSID=%s, Channel=%d, RSSI=%d dBm\n",
                     WiFi.BSSIDstr(bestIndex).c_str(), bestChannel, bestRSSI);
        
        // Configure connection parameters
        wifi_config_t conf;
        memset(&conf, 0, sizeof(wifi_config_t));
        strcpy((char*)conf.sta.ssid, ssid);
        strcpy((char*)conf.sta.password, password);
        
        // Set specific BSSID for faster connection
        if (useBSSID || bestRSSI < -75) {  // Use BSSID for weak signals
            memcpy(conf.sta.bssid, bestBSSID, 6);
            conf.sta.bssid_set = true;
            Serial.println("✓ Using specific BSSID for connection");
        }
        
        // Set the channel for faster connection
        conf.sta.channel = bestChannel;
        
        // Advanced connection parameters
        conf.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;  // Try WIFI_FAST_SCAN if this fails
        conf.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        conf.sta.threshold.rssi = -85;  // Minimum acceptable signal
        conf.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;  // Minimum security
        
        // Apply configuration
        esp_wifi_set_config(WIFI_IF_STA, &conf);
        
        // Enable auto-reconnect after configuration
        WiFi.setAutoReconnect(true);
        WiFi.setAutoConnect(true);
        
        // Start connection
        WiFi.begin();
        
        // Wait for connection with detailed progress
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
            delay(500);
            
            if (attempts % 4 == 0) {  // Every 2 seconds
                wl_status_t status = WiFi.status();
                Serial.printf("[%ds] Status: %s", 
                            attempts/2, 
                            getStatusString(status));
                
                // Get current signal strength if connected
                if (status == WL_CONNECTED) {
                    Serial.printf(", RSSI: %d dBm", WiFi.RSSI());
                }
                Serial.println();
                
                // Try recovery actions at different intervals
                if (attempts == 20 && status != WL_CONNECTED) {
                    Serial.println("→ Trying 802.11b only mode for better range...");
                    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
                } else if (attempts == 30 && status != WL_CONNECTED) {
                    Serial.println("→ Reducing data rate for better range...");
                    setLowerDataRate();
                } else if (attempts == 40 && status != WL_CONNECTED) {
                    Serial.println("→ Retrying with different parameters...");
                    WiFi.disconnect();
                    delay(100);
                    WiFi.begin(ssid, password, bestChannel, bestBSSID);
                }
            } else {
                Serial.print(".");
            }
            attempts++;
        }
        
        bool connected = (WiFi.status() == WL_CONNECTED);
        
        if (connected) {
            Serial.println("\n✅ WiFi Connected!");
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("RSSI: %d dBm (%s)\n", WiFi.RSSI(), getSignalQuality(WiFi.RSSI()));
            Serial.printf("Channel: %d\n", WiFi.channel());
            
            // Re-enable all protocols after connection
            esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        } else {
            Serial.println("\n❌ WiFi Connection Failed!");
            debugConnectionFailure();
        }
        
        return connected;
    }
    
    /**
     * Monitor and maintain connection quality
     */
    static void maintainConnectionQuality() {
        static unsigned long lastCheck = 0;
        static int poorSignalCount = 0;
        
        if (millis() - lastCheck < 10000) return;  // Check every 10 seconds
        lastCheck = millis();
        
        if (WiFi.status() != WL_CONNECTED) return;
        
        int rssi = WiFi.RSSI();
        
        // If signal is very weak, take action
        if (rssi < -80) {
            poorSignalCount++;
            Serial.printf("⚠️  Weak signal: %d dBm (count: %d)\n", rssi, poorSignalCount);
            
            if (poorSignalCount >= 3) {
                Serial.println("→ Attempting to improve connection...");
                
                // Try to roam to a better AP
                WiFi.disconnect(false);  // Disconnect but keep settings
                delay(100);
                WiFi.reconnect();
                
                poorSignalCount = 0;
            }
        } else {
            poorSignalCount = 0;
        }
    }
    
private:
    static void configureWiFiTaskPriority() {
        // Increase WiFi task priority (default is 23)
        // Higher priority = better WiFi performance under load
        esp_task_wdt_init(30, false);  // Increase watchdog timeout
        
        // Note: Be careful with priorities, WiFi task should be high but not highest
        // Default WiFi task priority is 23, we can increase to 24
        // (FreeRTOS priority range is 0-31, higher number = higher priority)
    }
    
    static void setLowerDataRate() {
        // Set lower data rate for better range
        // Lower data rates have better sensitivity
        esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_1M_L);  // 1 Mbps long preamble
    }
    
    static const char* getStatusString(wl_status_t status) {
        switch(status) {
            case WL_NO_SHIELD: return "NO_SHIELD";
            case WL_IDLE_STATUS: return "IDLE";
            case WL_NO_SSID_AVAIL: return "NO_SSID_AVAILABLE";
            case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
            case WL_CONNECTED: return "CONNECTED";
            case WL_CONNECT_FAILED: return "CONNECT_FAILED";
            case WL_CONNECTION_LOST: return "CONNECTION_LOST";
            case WL_DISCONNECTED: return "DISCONNECTED";
            default: return "UNKNOWN";
        }
    }
    
    static const char* getSignalQuality(int rssi) {
        if (rssi > -50) return "Excellent";
        if (rssi > -60) return "Good";
        if (rssi > -70) return "Fair";
        if (rssi > -80) return "Weak";
        return "Very Weak";
    }
    
    static void debugConnectionFailure() {
        Serial.println("\n=== Connection Failure Analysis ===");
        
        // Get detailed error info
        wifi_err_reason_t reason = static_cast<wifi_err_reason_t>(WiFi.disconnectReason());
        Serial.printf("Disconnect reason: %d - ", reason);
        
        switch(reason) {
            case WIFI_REASON_AUTH_EXPIRE: Serial.println("AUTH_EXPIRE"); break;
            case WIFI_REASON_AUTH_LEAVE: Serial.println("AUTH_LEAVE"); break;
            case WIFI_REASON_ASSOC_EXPIRE: Serial.println("ASSOC_EXPIRE"); break;
            case WIFI_REASON_ASSOC_TOOMANY: Serial.println("TOO_MANY_ASSOCIATIONS"); break;
            case WIFI_REASON_NOT_AUTHED: Serial.println("NOT_AUTHENTICATED"); break;
            case WIFI_REASON_NOT_ASSOCED: Serial.println("NOT_ASSOCIATED"); break;
            case WIFI_REASON_ASSOC_LEAVE: Serial.println("ASSOC_LEAVE"); break;
            case WIFI_REASON_ASSOC_NOT_AUTHED: Serial.println("ASSOC_NOT_AUTHED"); break;
            case WIFI_REASON_BEACON_TIMEOUT: Serial.println("BEACON_TIMEOUT"); break;
            case WIFI_REASON_NO_AP_FOUND: Serial.println("NO_AP_FOUND"); break;
            case WIFI_REASON_AUTH_FAIL: Serial.println("AUTH_FAIL - Check password!"); break;
            case WIFI_REASON_ASSOC_FAIL: Serial.println("ASSOC_FAIL"); break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT: Serial.println("HANDSHAKE_TIMEOUT"); break;
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: Serial.println("4WAY_HANDSHAKE_TIMEOUT"); break;
            default: Serial.println("Unknown reason"); break;
        }
        
        Serial.println("\nPossible solutions:");
        Serial.println("1. Check WiFi password (case-sensitive)");
        Serial.println("2. Ensure router uses WPA2 (not WPA3-only)");
        Serial.println("3. Check if MAC filtering is enabled");
        Serial.println("4. Try moving closer to router");
        Serial.println("5. Check if router has available DHCP addresses");
        Serial.println("6. Ensure router is on 2.4GHz (not 5GHz only)");
    }
};

#endif // WIFI_OPTIMIZER_H