// ============================================================================
// Tab5 Hub Test Firmware - Minimal Hub Coordinator Test
// ============================================================================
// Tests the Hub coordinator in isolation:
// - Starts SoftAP at 192.168.4.1
// - Serves /health endpoint
// - Waits for Node connections on /ws
// - Sends UDP ticks to connected nodes
//
// This is a CLEAN TEST BUILD with no legacy WiFi infrastructure.
// ============================================================================

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

// Hub coordinator (from our new implementation)
#include "hub/hub_main.h"
#include "config/Config.h"  // For TAB5_WIFI_SDIO_* pins

// Global Hub instance
static HubMain* g_hubMain = nullptr;

// FreeRTOS Task: UDP Fanout (100Hz)
static void hubUdpFanoutTask(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 100Hz = 10ms period

    while (true) {
        if (g_hubMain) {
            g_hubMain->udpTick();
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// FreeRTOS Task: Hub main loop (WS cleanup, registry maintenance)
static void hubMainLoopTask(void* pvParameters) {
    while (true) {
        if (g_hubMain) {
            g_hubMain->loop();
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // 20Hz
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n========================================");
    Serial.println("  Tab5 Hub Test Firmware");
    Serial.println("  Testing Hub Coordinator");
    Serial.println("========================================\n");

    // =========================================================================
    // CRITICAL: Configure Tab5 WiFi SDIO pins BEFORE M5.begin()
    // =========================================================================
    // Tab5 uses ESP32-C6 WiFi co-processor via SDIO on non-default pins.
    // This MUST be called before M5.begin() or WiFi.begin().
    // See: https://github.com/nikthefix/M5stack_Tab5_Arduino_Wifi_Example
    Serial.println("[HUB] Configuring Tab5 SDIO pins for ESP32-C6 WiFi co-processor...");
    WiFi.setPins(TAB5_WIFI_SDIO_CLK, TAB5_WIFI_SDIO_CMD,
                 TAB5_WIFI_SDIO_D0, TAB5_WIFI_SDIO_D1,
                 TAB5_WIFI_SDIO_D2, TAB5_WIFI_SDIO_D3,
                 TAB5_WIFI_SDIO_RST);
    delay(150);  // ESP-Hosted SDIO stabilization
    Serial.println("[HUB] SDIO pins configured (150ms stabilization)");

    // Initialize M5Stack (for any dependencies)
    M5.begin();

    Serial.println("[HUB] Initializing Hub coordinator...");

    g_hubMain = new HubMain();
    if (!g_hubMain) {
        Serial.println("[HUB] ERROR: Failed to allocate HubMain");
        while(1) delay(1000);
    }

    if (!g_hubMain->init("LightwaveOS-AP", "SpectraSynq")) {
        Serial.println("[HUB] ERROR: Hub init failed");
        delete g_hubMain;
        g_hubMain = nullptr;
        while(1) delay(1000);
    }

    Serial.println("[HUB] Hub coordinator initialized");

    // Create FreeRTOS tasks
    BaseType_t result;

    // Task 1: UDP Fanout (high priority, 100Hz)
    result = xTaskCreatePinnedToCore(
        hubUdpFanoutTask,
        "HubUDP",
        4096,  // Stack size
        nullptr,
        5,     // Priority (high)
        nullptr,
        1      // Core 1
    );
    if (result != pdPASS) {
        Serial.println("[HUB] ERROR: Failed to create UDP fanout task");
        while(1) delay(1000);
    }

    // Task 2: Main loop (medium priority, 20Hz)
    result = xTaskCreatePinnedToCore(
        hubMainLoopTask,
        "HubMain",
        4096,  // Stack size
        nullptr,
        3,     // Priority (medium)
        nullptr,
        1      // Core 1
    );
    if (result != pdPASS) {
        Serial.println("[HUB] ERROR: Failed to create main loop task");
        while(1) delay(1000);
    }

    Serial.println("[HUB] FreeRTOS tasks created");
    Serial.println("[HUB] ===== Hub Ready =====");
    Serial.printf("[HUB]   SSID: LightwaveOS-AP\n");
    Serial.printf("[HUB]   Pass: SpectraSynq\n");
    Serial.printf("[HUB]   IP:   192.168.4.1\n");
    Serial.printf("[HUB]   WS:   ws://192.168.4.1/ws\n");
    Serial.printf("[HUB]   UDP:  192.168.4.1:49152\n");
    Serial.println("[HUB] =======================");
}

void loop() {
    // M5Stack update
    M5.update();

    // Minimal status output
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus >= 5000) {
        Serial.printf("[HUB] Status: Running (heap=%u, tasks=%d)\n",
                     ESP.getFreeHeap(), uxTaskGetNumberOfTasks());
        lastStatus = millis();
    }

    delay(100);
}