// Hub coordinator integration stub (to be included in Tab5 main.cpp)
#ifndef HUB_INTEGRATION_H
#define HUB_INTEGRATION_H

#include "hub/hub_main.h"
#include "config/AnsiColors.h"

// Global Hub instance
static HubMain* g_hubMain = nullptr;

// Get Hub instance (for main loop dashboard update)
static HubMain* getHubInstance() {
    return g_hubMain;
}

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

// FreeRTOS Task: Hub main loop (WS cleanup, registry maintenance - NO DISPLAY)
static void hubMainLoopTask(void* pvParameters) {
    while (true) {
        if (g_hubMain) {
            g_hubMain->loopNoDisplay();  // Network tasks only, no display rendering
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // 20Hz
    }
}

// Initialize Hub coordinator
static bool initHubCoordinator() {
    LOG_HUB("Initialising LightwaveOS Hub coordinator...");
    
    g_hubMain = new HubMain();
    if (!g_hubMain) {
        LOG_HUB("ERROR: Failed to allocate HubMain");
        return false;
    }
    
    if (!g_hubMain->init("LightwaveOS-AP", "SpectraSynq")) {
        LOG_HUB("ERROR: Hub init failed");
        delete g_hubMain;
        g_hubMain = nullptr;
        return false;
    }
    
    LOG_HUB("Hub coordinator initialised");
    
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
        LOG_HUB("ERROR: Failed to create UDP fanout task");
        return false;
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
        LOG_HUB("ERROR: Failed to create main loop task");
        return false;
    }
    
    LOG_HUB("FreeRTOS tasks created");
    LOG_HUB("===== Hub Ready =====");
    LOG_HUB("  SSID: LightwaveOS-AP");
    LOG_HUB("  Pass: SpectraSynq");
    LOG_HUB("  IP:   192.168.4.1");
    LOG_HUB("  WS:   ws://192.168.4.1/ws");
    LOG_HUB("  UDP:  192.168.4.1:49152");
    LOG_HUB("=======================");
    
    return true;
}

#endif // HUB_INTEGRATION_H
