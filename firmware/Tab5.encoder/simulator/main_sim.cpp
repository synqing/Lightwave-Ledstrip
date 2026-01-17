// ============================================================================
// Tab5.encoder UI Simulator - Main Entry Point
// ============================================================================
// Test harness for DisplayUI running in SDL2 window
// ============================================================================

#ifdef SIMULATOR_BUILD

#include "M5GFX_Mock.h"
#include "../src/ui/DisplayUI.h"
#include "../src/hal/SimHal.h"
#include "../src/ui/Theme.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// Forward declaration - stub for ParameterMap::getParameterMax (used by DisplayUI)
// In simulator, all parameters default to 255 max
extern "C" uint8_t getParameterMax(uint8_t index) {
    // Return 255 for all parameters in simulator
    (void)index;
    return 255;
}

// Test data simulation
struct TestData {
    int32_t encoderValues[8];
    bool wifiConnected;
    bool wsConnected;
    bool encAConnected;
    bool encBConnected;
    int8_t batteryLevel;
    bool charging;
    float voltage;
    uint32_t lastUpdate;
    
    TestData() 
        : encoderValues{128, 192, 64, 200, 100, 150, 80, 180}
        , wifiConnected(true)
        , wsConnected(true)
        , encAConnected(true)
        , encBConnected(true)
        , batteryLevel(85)
        , charging(false)
        , voltage(4.1f)
        , lastUpdate(0)
    {}
    
    void update(uint32_t now) {
        // Animate encoder values slowly
        if (now - lastUpdate >= 100) {  // Update every 100ms
            lastUpdate = now;
            
            // Cycle values with sine wave
            float t = now / 1000.0f;
            for (int i = 0; i < 8; i++) {
                float phase = t + (i * 0.5f);
                encoderValues[i] = 128 + (int32_t)(127.0f * sin(phase));
                if (encoderValues[i] < 0) encoderValues[i] = 0;
                if (encoderValues[i] > 255) encoderValues[i] = 255;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("Tab5.encoder UI Simulator\n");
    printf("========================================\n");
    printf("Initializing SDL2...\n");
    
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    printf("Creating M5GFX mock display...\n");
    
    // Create M5GFX mock (this will create SDL window)
    M5GFX display;
    display.begin();
    
    if (!display.getSDLWindow()) {
        fprintf(stderr, "Failed to create SDL window\n");
        SDL_Quit();
        return 1;
    }
    
    printf("Creating DisplayUI...\n");
    
    // Create DisplayUI instance
    DisplayUI* ui = new DisplayUI(display);
    
    printf("Initializing DisplayUI...\n");
    
    // Initialize UI
    ui->begin();
    
    // Set initial connection state
    ui->setConnectionState(
        true,   // WiFi
        true,   // WebSocket
        true,   // Encoder A
        true    // Encoder B
    );
    
    // Set initial encoder values
    TestData testData;
    for (int i = 0; i < 8; i++) {
        ui->updateEncoder(i, testData.encoderValues[i], false);
    }
    
    // Set initial preset slots (some occupied, some empty)
    for (int i = 0; i < 8; i++) {
        if (i < 3) {
            // First 3 slots occupied
            ui->updatePresetSlot(i, true, 10 + i, 5 + i, 180 + i * 5);
        } else {
            // Rest empty
            ui->updatePresetSlot(i, false, 0, 0, 0);
        }
    }
    
    printf("Entering main loop...\n");
    printf("Controls:\n");
    printf("  - Close window to exit\n");
    printf("  - Encoder values animate automatically\n");
    printf("========================================\n");
    
    // Main event loop
    bool running = true;
    uint32_t frameCount = 0;
    uint32_t lastFpsTime = EspHal::millis();
    
    while (running) {
        // Handle SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }
        
        // Update test data
        uint32_t now = EspHal::millis();
        testData.update(now);
        
        // Update encoder values
        for (int i = 0; i < 8; i++) {
            ui->updateEncoder(i, testData.encoderValues[i], false);
        }
        
        // Update connection state (simulate occasional disconnects for testing)
        static uint32_t lastConnUpdate = 0;
        if (now - lastConnUpdate >= 5000) {  // Toggle every 5 seconds
            lastConnUpdate = now;
            testData.wsConnected = !testData.wsConnected;
            ui->setConnectionState(
                testData.wifiConnected,
                testData.wsConnected,
                testData.encAConnected,
                testData.encBConnected
            );
        }
        
        // Update battery (simulate slow drain)
        static uint32_t lastBatteryUpdate = 0;
        if (now - lastBatteryUpdate >= 1000) {  // Update every second
            lastBatteryUpdate = now;
            // Simulate battery drain (very slow)
            if (!testData.charging && testData.batteryLevel > 0) {
                testData.batteryLevel--;
            }
        }
        
        // Update UI
        ui->loop();
        
        // Update display
        display.update();
        
        // FPS counter
        frameCount++;
        if (now - lastFpsTime >= 1000) {
            printf("FPS: %u\n", frameCount);
            frameCount = 0;
            lastFpsTime = now;
        }
        
        // Small delay to prevent 100% CPU usage
        SDL_Delay(16);  // ~60 FPS
    }
    
    printf("\nShutting down...\n");
    
    // Cleanup
    delete ui;
    
    // M5GFX destructor will clean up SDL
    SDL_Quit();
    
    printf("Simulator exited successfully\n");
    return 0;
}

#endif  // SIMULATOR_BUILD

