// Node coordinator integration stub (to be included in K1 main.cpp)
#ifndef NODE_INTEGRATION_H
#define NODE_INTEGRATION_H

#include "node/node_main.h"

// Global Node instance (defined in main.cpp)
extern NodeMain g_nodeMain;

// Global state variables for renderer integration (UNUSED - renderer uses NodeOrchestrator)
// These are here for legacy RendererApply.cpp compatibility but not actually used
extern uint16_t g_currentEffectId;
extern uint16_t g_currentPaletteId;
extern uint8_t g_brightness;
extern uint8_t g_speed;

// FreeRTOS Task: Node main loop (WiFi, WS, UDP RX, time sync)
static void nodeMainLoopTask(void* pvParameters) {
    while (true) {
        g_nodeMain.loop();
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz
    }
}

// Initialize Node coordinator
// Must be called AFTER NodeOrchestrator::instance().init() and before start()
static bool initNodeCoordinator(lightwaveos::nodes::NodeOrchestrator* orchestrator) {
    Serial.println("[NODE] Initializing LightwaveOS Node coordinator...");
    
    // Wire Node to v2 Actor system BEFORE init
    g_nodeMain.setOrchestrator(orchestrator);
    
    if (!g_nodeMain.init("LightwaveOS-AP", "SpectraSynq")) {
        Serial.println("[NODE] ERROR: Node init failed");
        return false;
    }
    
    Serial.println("[NODE] Node coordinator initialized (wired to v2 Actor system)");
    
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
        return false;
    }
    
    Serial.println("[NODE] FreeRTOS task created");
    Serial.println("[NODE] ===== Node Ready =====");
    Serial.printf("[NODE]   Target: LightwaveOS-AP\n");
    Serial.printf("[NODE]   Hub IP: 192.168.4.1\n");
    Serial.printf("[NODE]   State: Connecting...\n");
    Serial.println("[NODE] =========================");
    
    return true;
}

// Call this BEFORE each render frame (from render loop)
static inline void nodeApplyScheduledCommands() {
    g_nodeMain.renderFrameBoundary();
}

#endif // NODE_INTEGRATION_H
