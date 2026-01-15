// ============================================================================
// K1 Node Test Firmware - Minimal Node Coordinator Test
// ============================================================================
// Tests the Node coordinator in isolation:
// - Connects to Hub's SoftAP (LightwaveOS-AP)
// - Performs HELLO/WELCOME handshake
// - Receives UDP packets and tracks sequence/loss
// - Maintains time sync
// - Reports status to serial
//
// This is a CLEAN TEST BUILD with only the new Node coordinator.
// ============================================================================

#include <Arduino.h>

// Node coordinator (from our new implementation)
#include "node/node_main.h"

// Global Node instance (defined in node_main.cpp)
extern NodeMain g_nodeMain;

// FreeRTOS Task: Node main loop (WiFi, WS, UDP RX, time sync)
static void nodeMainLoopTask(void* pvParameters) {
    while (true) {
        g_nodeMain.loop();
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n========================================");
    Serial.println("  K1 Node Test Firmware");
    Serial.println("  Testing Node Coordinator");
    Serial.println("========================================\n");

    Serial.println("[NODE] Initializing Node coordinator...");

    if (!g_nodeMain.init("LightwaveOS-AP", "SpectraSynq")) {
        Serial.println("[NODE] ERROR: Node init failed");
        while(1) delay(1000);
    }

    Serial.println("[NODE] Node coordinator initialized");

    // Create FreeRTOS task for Node main loop
    // Note: UDP RX and WS should run at high priority to not starve render
    BaseType_t result = xTaskCreatePinnedToCore(
        nodeMainLoopTask,
        "NodeMain",
        8192,  // Larger stack for networking
        nullptr,
        4,     // Priority (high, but below render)
        nullptr,
        0      // Core 0 (keep rendering on Core 1)
    );

    if (result != pdPASS) {
        Serial.println("[NODE] ERROR: Failed to create main loop task");
        while(1) delay(1000);
    }

    Serial.println("[NODE] FreeRTOS task created");
    Serial.println("[NODE] ===== Node Ready =====");
    Serial.printf("[NODE]   Target: LightwaveOS-AP\n");
    Serial.printf("[NODE]   Hub IP: 192.168.4.1\n");
    Serial.printf("[NODE]   State: Connecting...\n");
    Serial.println("[NODE] =========================");
}

void loop() {
    // Minimal status output
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus >= 2000) {
        Serial.printf("[NODE] Status: Running (heap=%u, tasks=%d)\n",
                     ESP.getFreeHeap(), uxTaskGetNumberOfTasks());
        lastStatus = millis();
    }

    delay(100);
}