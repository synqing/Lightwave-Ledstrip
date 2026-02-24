// ============================================================================
// Tab5.encoder - M5Stack Tab5 (ESP32-P4) Dual ROTATE8 Encoder Controller
// ============================================================================
// Milestone F: Network Control Plane (WiFi + WebSocket + LightwaveOS Sync)
//
// This firmware reads 16 rotary encoders via TWO M5ROTATE8 units on the
// same I2C bus (Grove Port.A) using different addresses, and synchronizes
// parameter changes with LightwaveOS v2 firmware over WebSocket.
//
// Hardware:
//   - M5Stack Tab5 (ESP32-P4)
//   - M5ROTATE8 Unit A @ 0x42 (reprogrammed via register 0xFF)
//   - M5ROTATE8 Unit B @ 0x41 (factory default)
//   - Both connected to Grove Port.A via hub or daisy-chain
//
// Grove Port.A I2C:
//   - SDA: GPIO 53
//   - SCL: GPIO 54
//   - Unit A: 0x42 (encoders 0-7) - Core parameters
//   - Unit B: 0x41 (encoders 8-15) - Zone parameters
//
// Network:
//   - WiFi: Connects to configured AP
//   - mDNS: Resolves lightwaveos.local
//   - WebSocket: Bidirectional sync with v2 firmware
//
// I2C RECOVERY (Phase G.2):
// This firmware includes SOFTWARE-LEVEL I2C recovery for the external
// Grove Port.A bus. It uses SCL toggling and Wire reinit - NOT aggressive
// hardware resets (periph_module_reset, i2cDeinit) which differ on ESP32-P4.
//
// CRITICAL SAFETY NOTE:
// Tab5's INTERNAL I2C bus is shared with display/touch/audio - NEVER touch it.
// The external I2C on Grove Port.A is isolated and safe for recovery.
// ============================================================================

#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <esp_task_wdt.h>  // For watchdog timer

#include "config/Config.h"
#include <ESP.h>   // For heap monitoring (used in debug logs)
#if ENABLE_WIFI
#include <WiFi.h>  // For WiFi.setPins() - must be called before M5.begin()
#endif
#include "config/network_config.h"
#include "input/DualEncoderService.h"
#include "input/I2CRecovery.h"
#include "input/TouchHandler.h"
#include "input/ButtonHandler.h"
#include "input/CoarseModeManager.h"
#include "network/WiFiManager.h"
#include "network/WiFiAntenna.h"
#include "network/WebSocketClient.h"
#include "network/WsMessageRouter.h"
#include "network/OtaHandler.h"
#include <ESPAsyncWebServer.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "parameters/ParameterHandler.h"
#include "parameters/ParameterMap.h"
#include "storage/NvsStorage.h"
#include "ui/LedFeedback.h"
// #include "ui/PaletteLedDisplay.h"  // DISABLED - causing encoder regression
#include "ui/DisplayUI.h"
#include "ui/LoadingScreen.h"
#include "ui/Theme.h"
#include "ui/lvgl_bridge.h"
#include "hal/EspHal.h"
#include "input/ClickDetector.h"
#include "presets/PresetManager.h"

// ============================================================================
// Agent tracing (compile-time, default off)
// ============================================================================
#ifndef TAB5_AGENT_TRACE
#define TAB5_AGENT_TRACE 0
#endif

#if TAB5_AGENT_TRACE
#define TAB5_AGENT_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define TAB5_AGENT_PRINTF(...) ((void)0)
#endif

static void formatIPv4(const IPAddress& ip, char out[16]) {
    snprintf(out, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

// ============================================================================
// I2C Addresses for Dual Unit Setup (canonical source: Config.h I2C namespace)
// ============================================================================

constexpr uint8_t ADDR_UNIT_A = I2C::ADDR_UNIT_A;
constexpr uint8_t ADDR_UNIT_B = I2C::ADDR_UNIT_B;

// NOTE: Color palette and dimColor() moved to ui/DisplayUI.h

// ============================================================================
// Global State
// ============================================================================

// Dual encoder service (initialized in setup)
DualEncoderService* g_encoders = nullptr;

// Network components (Milestone F)
WiFiManager g_wifiManager;
WebSocketClient g_wsClient;
ParameterHandler* g_paramHandler = nullptr;

// OTA HTTP Server (runs alongside WebSocket)
AsyncWebServer* g_otaServer = nullptr;

// WebSocket connection state
bool g_wsConfigured = false;  // true after wsClient.begin() called
static bool s_wsConnectedEdgeState = false;

// Connection status LED feedback (Phase F.5)
LedFeedback g_ledFeedback;

// DISABLED - PaletteLedDisplay causing encoder regression
// Palette color display on Unit B LEDs 0-7
// PaletteLedDisplay g_paletteLedDisplay;

// Coarse mode manager for ENC-A acceleration
CoarseModeManager g_coarseModeManager;

// Touch screen handler (Phase G.3)
TouchHandler g_touchHandler;

// Cyberpunk UI (Phase H)
DisplayUI* g_ui = nullptr;

// Button handler for zone mode and speed/palette toggles
ButtonHandler* g_buttonHandler = nullptr;

// Preset system (8 slots for Unit-B encoders)
PresetManager* g_presetManager = nullptr;
ClickDetector g_clickDetectors[8];  // One per Unit-B encoder
uint8_t g_activePresetSlot = 255;   // Currently active preset (255 = none)

// ============================================================================
// Effect / Palette name caches (from LightwaveOS via WebSocket lists)
// ============================================================================
static constexpr uint16_t EFFECT_NAME_MAX = 48;
static constexpr uint16_t PALETTE_NAME_MAX = 48;
static constexpr uint16_t MAX_EFFECTS = 256;
static constexpr uint8_t MAX_PALETTES = 80;  // v2 has 75 palettes, allow headroom

static char s_effectNames[MAX_EFFECTS][EFFECT_NAME_MAX] = {{0}};
static bool s_effectKnown[MAX_EFFECTS] = {false};
static uint16_t s_effectIds[MAX_EFFECTS] = {0};   // position index → hex effectId
static uint16_t s_effectCount = 0;                  // total effects received across all pages
static uint8_t s_effectPages = 0;
static uint8_t s_effectNextPage = 1;

static char s_paletteNames[MAX_PALETTES][PALETTE_NAME_MAX] = {{0}};
static bool s_paletteKnown[MAX_PALETTES] = {false};

static uint8_t s_palettePages = 0;
static uint8_t s_paletteNextPage = 1;

static bool s_requestedLists = false;
static bool s_uiInitialized = false;  // Track if DisplayUI::begin() has been called

// ============================================================================
// Touch Action Row (Colour Correction Controls)
// ============================================================================

static bool nextGammaState(bool enabled, float current, float& nextValue) {
    // Gamma order: OFF -> 2.2 (ON) -> 2.5 -> 2.8 -> OFF
    // Array order matches the cycle after starting at 2.2
    static const float kGammaSteps[] = {2.2f, 2.5f, 2.8f};
    static constexpr size_t kGammaCount = sizeof(kGammaSteps) / sizeof(kGammaSteps[0]);

    if (!enabled) {
        // OFF -> start at 2.2 (default)
        nextValue = 2.2f;
        return true;
    }

    // Find current position in the array
    size_t currentIdx = 0;
    float best = 1000.0f;
    for (size_t i = 0; i < kGammaCount; ++i) {
        float diff = fabsf(current - kGammaSteps[i]);
        if (diff < best) {
            best = diff;
            currentIdx = i;
        }
    }

    // Move to next step
    size_t nextIdx = (currentIdx + 1) % kGammaCount;
    
    // If we've wrapped around (nextIdx == 0 and we were at the last step), disable gamma
    if (nextIdx == 0 && currentIdx == kGammaCount - 1) {
        nextValue = current;
        return false;  // Disable gamma
    }

    nextValue = kGammaSteps[nextIdx];
    return true;  // Keep gamma enabled
}

// Deferred flag — set inside WebSocket callbacks, processed in main loop.
// Calling UI updates synchronously from a network callback can crash the
// LVGL rendering pipeline (Core 1 register dump after colour-correction sync).
static volatile bool s_colourCorrectionSyncPending = false;

static void syncColourCorrectionUi() {
    if (g_ui && s_uiInitialized) {
        g_ui->setColourCorrectionState(g_wsClient.getColorCorrectionState());
    }
}

static void handleActionButton(uint8_t buttonIndex) {
    // Debounce: prevent rapid repeated calls (300ms debounce)
    static uint32_t s_lastActionTime[4] = {0, 0, 0, 0};
    uint32_t now = millis();
    if (buttonIndex < 4 && (now - s_lastActionTime[buttonIndex]) < 300) {
        return;  // Ignore rapid repeated clicks
    }
    s_lastActionTime[buttonIndex] = now;
    
    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H2,H5\",\"location\":\"main.cpp:200\",\"message\":\"handleActionButton.entry\",\"data\":{\"buttonIndex\":%d,\"wsConnected\":%d,\"wsStatus\":%d},\"timestamp\":%lu}\n",
                  // buttonIndex, g_wsClient.isConnected() ? 1 : 0, (int)g_wsClient.getStatus(), (unsigned long)now);
        // #endregion
    
    // Get current state (use defaults if not valid)
    ColorCorrectionState cc = g_wsClient.getColorCorrectionState();
    
    // #region agent log (DISABLED)
    // FILE* logFile = fopen("/Users/spectrasynq/Workspace_Management/Software/PRISM.tab5/.cursor/debug.log", "a");
    // if (logFile != nullptr) {
    //     fprintf(logFile, "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"main.cpp:215\",\"message\":\"handleActionButton.stateBefore\",\"data\":{\"buttonIndex\":%d,\"gammaEnabled\":%d,\"gammaValue\":%.1f,\"aeEnabled\":%d,\"aeTarget\":%d,\"brownEnabled\":%d,\"mode\":%d,\"valid\":%d},\"timestamp\":%lu}\n",
    //             buttonIndex, cc.gammaEnabled ? 1 : 0, cc.gammaValue, cc.autoExposureEnabled ? 1 : 0,
    //             cc.autoExposureTarget, cc.brownGuardrailEnabled ? 1 : 0, cc.mode, cc.valid ? 1 : 0, (unsigned long)millis());
    //     fclose(logFile);
    // }
        // #endregion
    if (!cc.valid) {
        // Initialize with defaults if not synced yet
        cc.valid = true;
        cc.gammaEnabled = true;
        cc.gammaValue = 2.2f;
        cc.mode = 2;  // RGB
        cc.autoExposureEnabled = false;
        cc.autoExposureTarget = 110;
        cc.brownGuardrailEnabled = false;
        
        // Request config from server if connected (but don't block UI update)
        if (g_wsClient.isConnected()) {
            static uint32_t lastRequestTime = 0;
            uint32_t now = millis();
            if (now - lastRequestTime > 2000) {  // Throttle to once per 2 seconds
                g_wsClient.requestColorCorrectionConfig();
                lastRequestTime = now;
                Serial.println("[TOUCH] Colour correction not synced yet - requested config");
            }
        }
    }

    // Calculate next state optimistically (update UI immediately)
    bool stateChanged = false;
    switch (buttonIndex) {
        case 0: {  // Gamma mode (cycle)
            float nextValue = cc.gammaValue;
            bool nextEnabled = nextGammaState(cc.gammaEnabled, cc.gammaValue, nextValue);
            cc.gammaEnabled = nextEnabled;
            cc.gammaValue = nextValue;
            stateChanged = true;
            break;
        }
        case 1: {  // Colour correction mode (cycle)
            cc.mode = static_cast<uint8_t>((cc.mode + 1) % 4);
            stateChanged = true;
            break;
        }
        case 2: {  // Auto exposure (toggle)
            uint8_t target = (cc.autoExposureTarget == 0) ? 110 : cc.autoExposureTarget;
            cc.autoExposureEnabled = !cc.autoExposureEnabled;
            cc.autoExposureTarget = target;
            stateChanged = true;
            break;
        }
        case 3: {  // Brown guardrail (toggle)
            cc.brownGuardrailEnabled = !cc.brownGuardrailEnabled;
            stateChanged = true;
            break;
        }
        default:
            return;
    }

    if (stateChanged) {
        // Update local state cache optimistically (even if WS not connected)
        g_wsClient.setColorCorrectionState(cc);
        
        // Update UI immediately (optimistic update)
        syncColourCorrectionUi();
        
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"main.cpp:269\",\"message\":\"handleActionButton.stateAfter\",\"data\":{\"buttonIndex\":%d,\"gammaEnabled\":%d,\"gammaValue\":%.1f,\"aeEnabled\":%d,\"aeTarget\":%d,\"brownEnabled\":%d,\"mode\":%d},\"timestamp\":%lu}\n",
                      // buttonIndex, cc.gammaEnabled ? 1 : 0, cc.gammaValue, cc.autoExposureEnabled ? 1 : 0,
                      // cc.autoExposureTarget, cc.brownGuardrailEnabled ? 1 : 0, cc.mode, (unsigned long)millis());
                // #endregion
        
        // Try to send command to server (if connected)
        if (g_wsClient.isConnected()) {
            // #region agent log (DISABLED)
            // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H2\",\"location\":\"main.cpp:277\",\"message\":\"handleActionButton.sendingCommand\",\"data\":{\"buttonIndex\":%d,\"commandType\":\"%s\"},\"timestamp\":%lu}\n",
                          // buttonIndex, 
                          // buttonIndex == 0 ? "setConfig" : (buttonIndex == 1 ? "setMode" : (buttonIndex == 2 ? "setConfig" : "setConfig")),
                          // (unsigned long)millis());
                        // #endregion
            
            switch (buttonIndex) {
                case 0:
                    // Gamma: Use setConfig with all fields including mode
                    Serial.printf("[TOUCH] Gamma button: enabled=%s value=%.1f\n", 
                                  cc.gammaEnabled ? "true" : "false", cc.gammaValue);
                    g_wsClient.sendColorCorrectionConfig(
                        cc.gammaEnabled, cc.gammaValue,
                        cc.autoExposureEnabled, cc.autoExposureTarget,
                        cc.brownGuardrailEnabled, cc.mode);
                    break;
                case 1:
                    // Colour mode: Use dedicated setMode command
                    Serial.printf("[TOUCH] Colour button: mode=%d\n", cc.mode);
                    g_wsClient.sendColourCorrectionMode(cc.mode);
                    break;
                case 2:
                    // Auto exposure: Use setConfig with all fields including mode
                    Serial.printf("[TOUCH] Exposure button: enabled=%s target=%d\n", 
                                  cc.autoExposureEnabled ? "true" : "false", cc.autoExposureTarget);
                    g_wsClient.sendColorCorrectionConfig(
                        cc.gammaEnabled, cc.gammaValue,
                        cc.autoExposureEnabled, cc.autoExposureTarget,
                        cc.brownGuardrailEnabled, cc.mode);
                    break;
                case 3:
                    // Brown guardrail: Use setConfig with all fields including mode
                    Serial.printf("[TOUCH] Brown button: enabled=%s\n", 
                                  cc.brownGuardrailEnabled ? "true" : "false");
                    g_wsClient.sendColorCorrectionConfig(
                        cc.gammaEnabled, cc.gammaValue,
                        cc.autoExposureEnabled, cc.autoExposureTarget,
                        cc.brownGuardrailEnabled, cc.mode);
                    break;
            }
        } else {
            // #region agent log (DISABLED)
            // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"main.cpp:304\",\"message\":\"handleActionButton.wsNotConnected\",\"data\":{\"buttonIndex\":%d,\"wsStatus\":%d},\"timestamp\":%lu}\n",
                          // buttonIndex, (int)g_wsClient.getStatus(), (unsigned long)millis());
                        // #endregion
            Serial.println("[TOUCH] WS not connected - UI updated optimistically, command will sync when connected");
        }
    }
}

const char* lookupEffectName(uint8_t index) {
    // index is a position index (0-160), not a hex effectId
    if (index < MAX_EFFECTS && s_effectKnown[index] && s_effectNames[index][0]) return s_effectNames[index];
    return nullptr;
}

// Translate position index → hex effectId (returns 0xFFFF if invalid)
uint16_t effectIdFromIndex(uint8_t index) {
    if (index >= s_effectCount) return 0xFFFF;
    return s_effectIds[index];
}

// Translate hex effectId → position index (returns 0xFF if not found)
uint8_t indexFromEffectId(uint16_t effectId) {
    for (uint16_t i = 0; i < s_effectCount; i++) {
        if (s_effectIds[i] == effectId) return static_cast<uint8_t>(i);
    }
    return 0xFF;
}

const char* lookupPaletteName(uint8_t id) {
    if (id < MAX_PALETTES && s_paletteKnown[id] && s_paletteNames[id][0]) return s_paletteNames[id];
    return nullptr;
}

// Cache a palette name from status message (called from WsMessageRouter)
void cachePaletteName(uint8_t id, const char* name) {
    if (id < MAX_PALETTES && name && name[0]) {
        strncpy(s_paletteNames[id], name, PALETTE_NAME_MAX - 1);
        s_paletteNames[id][PALETTE_NAME_MAX - 1] = '\0';
        s_paletteKnown[id] = true;
    }
}

static void updateUiEffectPaletteLabels() {
    if (!g_ui || !g_encoders || !s_uiInitialized) return;  // Only update if UI is initialized
    uint8_t effectIndex = static_cast<uint8_t>(g_encoders->getValue(0));  // position index
    uint8_t paletteId = static_cast<uint8_t>(g_encoders->getValue(1));  // FIXED: Palette is encoder 1, not 2

    char effectBuf[EFFECT_NAME_MAX];
    char paletteBuf[PALETTE_NAME_MAX];

    const char* en = lookupEffectName(effectIndex);
    const char* pn = lookupPaletteName(paletteId);

    if (en) {
        strncpy(effectBuf, en, sizeof(effectBuf) - 1);
        effectBuf[sizeof(effectBuf) - 1] = '\0';
    } else {
        snprintf(effectBuf, sizeof(effectBuf), "#%u", (unsigned)effectIndex);
    }

    if (pn) {
        strncpy(paletteBuf, pn, sizeof(paletteBuf) - 1);
        paletteBuf[sizeof(paletteBuf) - 1] = '\0';
    } else {
        snprintf(paletteBuf, sizeof(paletteBuf), "#%u", (unsigned)paletteId);
    }

    g_ui->setCurrentEffect(effectIndex, effectBuf);
    g_ui->setCurrentPalette(paletteId, paletteBuf);
}

// ============================================================================
// Serial Commands
// ============================================================================
static void handleSerialCommand(const char* cmd) {
    if (!cmd || cmd[0] == '\0') return;

    if (strcmp(cmd, "ppa on") == 0) {
        LoadingScreen::setPpaEnabled(true);
        Serial.println("[PPA] Enabled");
        return;
    }
    if (strcmp(cmd, "ppa off") == 0) {
        LoadingScreen::setPpaEnabled(false);
        Serial.println("[PPA] Disabled");
        return;
    }
    if (strcmp(cmd, "ppa toggle") == 0) {
        bool next = !LoadingScreen::isPpaEnabled();
        LoadingScreen::setPpaEnabled(next);
        Serial.printf("[PPA] %s\n", next ? "Enabled" : "Disabled");
        return;
    }
    if (strncmp(cmd, "ppa bench", 9) == 0) {
        uint16_t iterations = 60;
        unsigned int parsed = 0;
        if (sscanf(cmd, "ppa bench %u", &parsed) == 1 && parsed > 0) {
            iterations = static_cast<uint16_t>(parsed);
        }
        Serial.printf("[PPA] Benchmark: %u iterations\n", iterations);
        uint32_t cpuUs = LoadingScreen::benchmarkLogo(M5.Display, iterations, false);
        uint32_t ppaUs = LoadingScreen::benchmarkLogo(M5.Display, iterations, true);
        Serial.printf("[PPA] Logo avg: CPU=%lu us, PPA=%lu us\n",
                      static_cast<unsigned long>(cpuUs),
                      static_cast<unsigned long>(ppaUs));
        return;
    }

    // Palette animation commands
    // DISABLED - PaletteLedDisplay causing encoder regression
    /*
    if (strncmp(cmd, "paletteanim ", 12) == 0 || strncmp(cmd, "pa ", 3) == 0) {
        const char* modeStr = (cmd[0] == 'p' && cmd[1] == 'a' && cmd[2] == 'l') ? cmd + 12 : cmd + 3;
        
        // Parse mode string
        AnimationMode newMode = AnimationMode::ROTATE;
        bool valid = false;
        
        if (strcmp(modeStr, "static") == 0) {
            newMode = AnimationMode::STATIC;
            valid = true;
        } else if (strcmp(modeStr, "rotate") == 0) {
            newMode = AnimationMode::ROTATE;
            valid = true;
        } else if (strcmp(modeStr, "wave") == 0) {
            newMode = AnimationMode::WAVE;
            valid = true;
        } else if (strcmp(modeStr, "breathing") == 0) {
            newMode = AnimationMode::BREATHING;
            valid = true;
        } else if (strcmp(modeStr, "scroll") == 0) {
            newMode = AnimationMode::SCROLL;
            valid = true;
        }
        
        if (valid) {
            g_paletteLedDisplay.setAnimationMode(newMode);
            Serial.printf("[PaletteAnim] Mode set to: %s\n", g_paletteLedDisplay.getAnimationModeString());
        } else {
            Serial.println("[PaletteAnim] Invalid mode. Use: static, rotate, wave, breathing, scroll");
        }
        return;
    }
    
    // Shortcut: "pa" cycles through modes
    if (strcmp(cmd, "pa") == 0) {
        AnimationMode current = g_paletteLedDisplay.getAnimationMode();
        const uint8_t modeCount = static_cast<uint8_t>(AnimationMode::MODE_COUNT);
        AnimationMode next = static_cast<AnimationMode>((static_cast<uint8_t>(current) + 1) % modeCount);
        g_paletteLedDisplay.setAnimationMode(next);
        Serial.printf("[PaletteAnim] Mode cycled to: %s\n", g_paletteLedDisplay.getAnimationModeString());
        return;
    }
    */

    // Network commands (similar to v2 firmware)
    if (strncmp(cmd, "net ", 4) == 0) {
        const char* args = cmd + 4;
        
        if (strcmp(args, "status") == 0) {
            #if ENABLE_WIFI
            Serial.println(g_wifiManager.getStatusString());
            #else
            Serial.println("[WiFi] WiFi disabled (ENABLE_WIFI=0)");
            #endif
            return;
        }
        
        if (strcmp(args, "sta") == 0) {
            Serial.println("[WiFi] Manual switching not supported (Auto-managed)");
            return;
        }
        
        if (strcmp(args, "ap") == 0) {
            Serial.println("[WiFi] Manual switching not supported (Auto-managed)");
            return;
        }
        
        if (strcmp(args, "help") == 0 || args[0] == '\0') {
            Serial.println("[NET] Commands:");
            Serial.println("  net status  - Show WiFi status");
            Serial.println("  net sta     - Switch to fallback network (STA mode)");
            Serial.println("  net ap      - Switch to primary network (AP mode)");
            return;
        }
        
        Serial.println("[NET] Unknown command. Try: net help");
        return;
    }

#if ENABLE_WIFI
    if (strncmp(cmd, "antenna", 7) == 0) {
        const char* args = cmd + 7;
        while (*args == ' ') args++;
        if (args[0] == '\0' || strcmp(args, "?") == 0 || strcmp(args, "status") == 0) {
            Serial.printf("[Antenna] %s\n", isWiFiAntennaExternal() ? "external (MMCX)" : "internal (3D)");
            return;
        }
        if (strcmp(args, "ext") == 0 || strcmp(args, "external") == 0 || strcmp(args, "1") == 0) {
            setWiFiAntenna(true);
            Serial.println("[Antenna] Switched to external MMCX (may require WiFi reconnect to take effect)");
            return;
        }
        if (strcmp(args, "int") == 0 || strcmp(args, "internal") == 0 || strcmp(args, "0") == 0) {
            setWiFiAntenna(false);
            Serial.println("[Antenna] Switched to internal 3D (may require WiFi reconnect to take effect)");
            return;
        }
        Serial.println("[Antenna] Usage: antenna | antenna ext | antenna int | antenna 1 | antenna 0");
        return;
    }
#endif

    if (strcmp(cmd, "help") == 0) {
        Serial.println("[HELP] Commands:");
        Serial.println("  ppa on | ppa off | ppa toggle | ppa bench [N]");
        Serial.println("  net status | net sta | net ap");
#if ENABLE_WIFI
        Serial.println("  antenna | antenna ext | antenna int");
#endif
        return;
    }
}

static void pollSerialCommands() {
    static char s_buf[96];
    static uint8_t s_len = 0;

    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (s_len > 0) {
                s_buf[s_len] = '\0';
                handleSerialCommand(s_buf);
                s_len = 0;
            }
            continue;
        }

        if (s_len < sizeof(s_buf) - 1) {
            s_buf[s_len++] = c;
        }
    }
}

// ============================================================================
// I2C Scanner Utility
// ============================================================================

/**
 * Scan the I2C bus and print discovered devices.
 * @param wire The TwoWire instance to scan
 * @param busName Name for logging (e.g., "External I2C")
 * @return Number of devices found
 */
uint8_t scanI2CBus(TwoWire& wire, const char* busName) {
    Serial.printf("\n=== Scanning %s ===\n", busName);
    uint8_t deviceCount = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        wire.beginTransmission(addr);
        uint8_t error = wire.endTransmission();

        if (error == 0) {
            Serial.printf("  Found device at 0x%02X", addr);

            // Identify known devices
            if (addr == ADDR_UNIT_A) {
                Serial.print(" (M5ROTATE8 Unit A)");
            } else if (addr == ADDR_UNIT_B) {
                Serial.print(" (M5ROTATE8 Unit B)");
            }

            Serial.println();
            deviceCount++;
        } else if (error == 4) {
            Serial.printf("  Unknown error at 0x%02X\n", addr);
        }
    }

    if (deviceCount == 0) {
        Serial.println("  No devices found!");
    } else {
        Serial.printf("  Total: %d device(s)\n", deviceCount);
    }

    return deviceCount;
}

// ============================================================================
// Encoder Change Callback
// ============================================================================

// Rate limiter for Serial logging (prevents IO from dominating callback latency)
static uint32_t s_lastEncoderLogTime = 0;
static constexpr uint32_t ENCODER_LOG_INTERVAL_MS = 100;  // Log at most every 100ms

// Track previous value and accumulator for encoder 15 (ENC-B encoder 7) for animation mode cycling
static uint16_t s_prevEncoder15Value = 0;
static bool s_encoder15Initialized = false;
static int16_t s_encoder15Accumulator = 0;  // Accumulates detents (requires ±2 to cycle mode)

// Track previous encoder values for delta calculation (all 16 encoders)
static uint16_t s_prevEncoderValues[16] = {0};
static bool s_encodersInitialized[16] = {false};

/**
 * Called when any encoder value changes
 * @param index Encoder index (0-15)
 * @param value New parameter value
 * @param wasReset true if this was a button-press reset to default
 */
void onEncoderChange(uint8_t index, uint16_t value, bool wasReset) {
    // Zone Composer routing: only ENC-B (indices 8-15) when Zone Composer is active.
    // ENC-A (0-7) always works as global parameters regardless of active screen.
    if (index >= 8 && g_ui && s_uiInitialized && g_ui->getCurrentScreen() == UIScreen::ZONE_COMPOSER) {
        ZoneComposerUI* zoneUI = g_ui->getZoneComposerUI();
        if (zoneUI) {
            // Calculate delta for Zone Composer
            if (!s_encodersInitialized[index]) {
                // First time - just store the value, don't send delta
                s_prevEncoderValues[index] = value;
                s_encodersInitialized[index] = true;
                return;
            }

            // Calculate delta (handle wrap-around)
            int32_t delta = static_cast<int32_t>(value) - static_cast<int32_t>(s_prevEncoderValues[index]);

            // Handle wrap-around: if delta is very large, it wrapped the other direction
            if (delta > 128) {
                delta -= 256;  // Wrapped forward, treat as backward
            } else if (delta < -128) {
                delta += 256;  // Wrapped backward, treat as forward
            }

            // Route ENC-B to Zone Composer with local index (0-7)
            zoneUI->handleEncoderChange(index - 8, delta);

            // Update previous value
            s_prevEncoderValues[index] = value;
            return;  // Don't process as global parameter
        }
    }

    // Initialize encoder tracking for first use
    if (!s_encodersInitialized[index]) {
        s_prevEncoderValues[index] = value;
        s_encodersInitialized[index] = true;
    } else {
        s_prevEncoderValues[index] = value;
    }
    // DISABLED - PaletteLedDisplay causing encoder regression
    // Special handling for encoder 15 (ENC-B encoder 7): Cycle palette animation modes
    /*
    if (index == 15) {
        if (!s_encoder15Initialized) {
            // First time - just store the value, don't cycle
            s_prevEncoder15Value = value;
            s_encoder15Initialized = true;
            s_encoder15Accumulator = 0;
            return;
        }
        
        // Calculate delta (handle wrap-around)
        int32_t delta = static_cast<int32_t>(value) - static_cast<int32_t>(s_prevEncoder15Value);
        
        // Handle wrap-around: if delta is very large, it wrapped the other direction
        if (delta > 128) {
            delta -= 256;  // Wrapped forward, treat as backward
        } else if (delta < -128) {
            delta += 256;  // Wrapped backward, treat as forward
        }
        
        // Accumulate detents (encoder changes by 1 per detent)
        s_encoder15Accumulator += delta;
        
        // Only cycle mode when accumulator reaches ±2 detents (debounce + intentional movement)
        if (abs(s_encoder15Accumulator) >= 2) {
            // Only process if dashboard is loaded (LEDs enabled)
            if (s_uiInitialized) {
                AnimationMode currentMode = g_paletteLedDisplay.getAnimationMode();
                uint8_t currentModeIndex = static_cast<uint8_t>(currentMode);
                
                // Cycle based on accumulator direction
                const uint8_t modeCount = static_cast<uint8_t>(AnimationMode::MODE_COUNT);
                if (s_encoder15Accumulator > 0) {
                    // Forward: next mode
                    currentModeIndex = (currentModeIndex + 1) % modeCount;
                } else {
                    // Backward: previous mode (with wrap)
                    if (currentModeIndex == 0) {
                        currentModeIndex = modeCount - 1;
                    } else {
                        currentModeIndex--;
                    }
                }
                
                AnimationMode newMode = static_cast<AnimationMode>(currentModeIndex);
                g_paletteLedDisplay.setAnimationMode(newMode);
                
                Serial.printf("[ENC-B:7] Palette animation mode: %s\n", g_paletteLedDisplay.getAnimationModeString());
            }
            
            // Reset accumulator (subtract the 2 detents we just processed)
            int8_t sign = (s_encoder15Accumulator > 0) ? 1 : -1;
            s_encoder15Accumulator -= (sign * 2);
        }
        
        // Update previous value
        s_prevEncoder15Value = value;
        return;  // Don't process as normal parameter
    }
    */
    
    Parameter param = static_cast<Parameter>(index);
    const char* name = getParameterName(param);
    const char* unit = (index < 8) ? "A" : "B";
    uint8_t localIdx = index % 8;

    // Mark this parameter as locally changed FIRST (for anti-snapback holdoff).
    // This prevents incoming server status echoes from reverting our change.
    if (g_paramHandler) {
        g_paramHandler->markLocalChange(index);
    }

    // Rate-limited logging (always log resets, but throttle normal changes)
    uint32_t now = millis();
    bool shouldLog = wasReset || (now - s_lastEncoderLogTime >= ENCODER_LOG_INTERVAL_MS);
    
    if (shouldLog) {
        // Resolve human-readable name for Effect (index 0) and Palette (index 1)
        const char* resolvedName = nullptr;
        if (index == 0) resolvedName = lookupEffectName(static_cast<uint8_t>(value));
        else if (index == 1) resolvedName = lookupPaletteName(static_cast<uint8_t>(value));

        if (wasReset) {
            if (resolvedName) {
                Serial.printf("[%s:%d] %s reset to %d (%s)\n", unit, localIdx, name, value, resolvedName);
            } else {
                Serial.printf("[%s:%d] %s reset to %d\n", unit, localIdx, name, value);
            }
        } else {
            if (resolvedName) {
                Serial.printf("[%s:%d] %s: → %d (%s)\n", unit, localIdx, name, value, resolvedName);
            } else {
                Serial.printf("[%s:%d] %s: → %d\n", unit, localIdx, name, value);
            }
        }
        s_lastEncoderLogTime = now;
    }

    // Update display with new value (fast, non-blocking)
    if (g_ui && s_uiInitialized) {  // Only update if UI is initialized
        g_ui->updateValue(index, value);
    }

    // DISABLED - PaletteLedDisplay causing encoder regression
    // Update palette LED display when palette parameter changes (index 1)
    /*
    if (index == 1) {  // Palette parameter
        // #region agent log (DISABLED)
        // bool enabled = g_paletteLedDisplay.isEnabled();
        // bool available = g_encoders ? g_encoders->isUnitBAvailable() : false;
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"palette-latch\",\"hypothesisId\":\"A,B,E\",\"location\":\"main.cpp:onEncoderChange\",\"message\":\"palette.change.trigger\",\"data\":{\"paletteId\":%u,\"enabled\":%d,\"unitBAvailable\":%d,\"uiInit\":%d,\"timestamp\":%lu}}\n",
            // value, enabled ? 1 : 0, available ? 1 : 0, s_uiInitialized ? 1 : 0, static_cast<unsigned long>(millis()));
                // #endregion
        bool result = g_paletteLedDisplay.update(static_cast<uint8_t>(value));
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"palette-latch\",\"hypothesisId\":\"A\",\"location\":\"main.cpp:onEncoderChange\",\"message\":\"palette.update.result\",\"data\":{\"result\":%d,\"timestamp\":%lu}}\n",
            // result ? 1 : 0, static_cast<unsigned long>(millis()));
                // #endregion
    }
    */

    // Queue parameter for NVS persistence (debounced to prevent flash wear)
    NvsStorage::requestSave(index, value);

    // Send to LightwaveOS via WebSocket (Milestone F)
    // Note: ParameterHandler also handles this, but we keep this as fallback
    if (g_wsClient.isConnected()) {
        // Unit A (0-7): Global parameters
        if (index < 8) {
            switch (index) {
                case 0: g_wsClient.sendEffectChange(value); break;
                case 1: g_wsClient.sendPaletteChange(value); break;      // FIXED: Encoder 1 = Palette
                case 2: g_wsClient.sendSpeedChange(value); break;         // FIXED: Encoder 2 = Speed
                case 3: g_wsClient.sendMoodChange(value); break;
                case 4: g_wsClient.sendFadeAmountChange(value); break;
                case 5: g_wsClient.sendComplexityChange(value); break;
                case 6: g_wsClient.sendVariationChange(value); break;
                case 7: g_wsClient.sendBrightnessChange(value); break;   // FIXED: Encoder 7 = Brightness
            }
        }
        // Unit B (8-15): Zone parameters
        // Note: Encoder 15 (index 15) is handled specially above for animation mode cycling
        else if (index < 15) {  // Only process 8-14 as zone parameters
            // #region agent log (DISABLED)
            // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"main.cpp:648\",\"message\":\"onEncoderChange.encB\",\"data\":{\"index\":%u,\"value\":%u,\"unit\":\"B\",\"localIdx\":%u,\"timestamp\":%lu}\n",
                // index, value, index % 8, static_cast<unsigned long>(millis()));
                        // #endregion
            
            uint8_t zoneId = ZoneParam::getZoneId(index);
            if (ZoneParam::isZoneEffect(index)) {
                g_wsClient.sendZoneEffect(zoneId, value);
            } else {
                // Zone speed/palette (handled by ButtonHandler mode)
                if (g_buttonHandler && g_buttonHandler->getZoneEncoderMode(zoneId) == SpeedPaletteMode::PALETTE) {
                    g_wsClient.sendZonePalette(zoneId, value);
                } else {
                    g_wsClient.sendZoneSpeed(zoneId, value);
                }
            }
        }
        // Encoder 15 is handled above for animation mode cycling - no WebSocket message needed
    }
}

// NOTE: Display rendering moved to ui/DisplayUI.h/cpp (Cyberpunk UI with radial gauges)

// ============================================================================
// Connection Status LED Feedback (Phase F.5)
// ============================================================================
// Determines connection state from WiFiManager and WebSocketClient,
// then updates both Unit A and Unit B status LEDs via LedFeedback.
//
// State Priority (highest to lowest):
//   1. WS_CONNECTED       - WebSocket connected (green solid)
//   2. WS_RECONNECTING    - WebSocket lost, reconnecting (orange breathing)
//   3. WS_CONNECTING      - WiFi up, WS connecting (yellow breathing)
//   4. WIFI_CONNECTED     - WiFi up, no WS yet (blue solid)
//   5. WIFI_CONNECTING    - WiFi connecting (blue breathing)
//   6. WIFI_DISCONNECTED  - No WiFi (red solid)
// ============================================================================

// Track previous WS connection for reconnection detection
static bool s_wasWsConnected = false;

void updateConnectionLeds() {
    ConnectionState state;

    // Determine current connection state
    if (!g_wifiManager.isConnected()) {
        // No WiFi connection
        WiFiConnectionStatus wifiStatus = g_wifiManager.getStatus();
        if (wifiStatus == WiFiConnectionStatus::CONNECTING) {
            state = ConnectionState::WIFI_CONNECTING;
        } else {
            state = ConnectionState::WIFI_DISCONNECTED;
        }
        // Track disconnection for footer display
        if (s_wasWsConnected && g_ui && s_uiInitialized) {
            // Just disconnected
            g_ui->setWebSocketConnected(false, 0);
            g_ui->updateWebSocketStatus(WebSocketStatus::DISCONNECTED);
        }
        s_wasWsConnected = false;  // Reset WS tracking
    }
    else if (g_wsClient.isConnected()) {
        // Fully connected
        state = ConnectionState::WS_CONNECTED;
        // Track connection time for footer display
        if (!s_wasWsConnected && g_ui && s_uiInitialized) {
            // Just connected - record connection time
            g_ui->setWebSocketConnected(true, millis());
            g_ui->updateWebSocketStatus(WebSocketStatus::CONNECTED);
        }
        s_wasWsConnected = true;
    }
    else if (g_wsClient.isConnecting()) {
        // WebSocket connecting
        if (s_wasWsConnected) {
            // Was connected before, now reconnecting
            state = ConnectionState::WS_RECONNECTING;
        } else {
            // First connection attempt
            state = ConnectionState::WS_CONNECTING;
        }
        // Update footer status
        if (g_ui && s_uiInitialized) {
            g_ui->updateWebSocketStatus(WebSocketStatus::CONNECTING);
        }
    }
    else if (g_wsConfigured) {
        // WS configured but not connecting (between reconnect attempts)
        if (s_wasWsConnected) {
            state = ConnectionState::WS_RECONNECTING;
        } else {
            state = ConnectionState::WS_CONNECTING;
        }
    }
    else {
        // WiFi connected, mDNS not resolved yet or WS not configured
        if (!g_wifiManager.isMDNSResolved()) {
            // Still resolving mDNS - treat as WiFi connected phase
            state = ConnectionState::WIFI_CONNECTED;
        } else {
            // mDNS resolved, WS about to be configured
            state = ConnectionState::WS_CONNECTING;
        }
    }

    // Update LED feedback state
    g_ledFeedback.setState(state);
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    // Initialize watchdog timer FIRST (5-second timeout)
    // This prevents device freeze on blocking operations
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 5000,  // 5 second timeout
        .idle_core_mask = 0,  // Watch both cores (but we're on P4, single core)
        .trigger_panic = true  // Panic on timeout (hard reset)
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);  // Add current task (main loop)
    Serial.begin(115200);
    delay(100);
    Serial.println("[WDT] Watchdog initialized (5s timeout)");

    Serial.println("\n");
    Serial.println("============================================");
    Serial.println("  Tab5.encoder - Milestone F");
    Serial.println("  Dual M5ROTATE8 (16 Encoders) + WiFi");
    Serial.println("============================================");
    
    // Reset watchdog after serial init
    esp_task_wdt_reset();

    // =========================================================================
    // CRITICAL: Configure Tab5 WiFi SDIO pins BEFORE any WiFi initialization
    // =========================================================================
    // Tab5 uses ESP32-C6 WiFi co-processor via SDIO on non-default pins.
    // This MUST be called before M5.begin() or WiFi.begin().
    // See: https://github.com/nikthefix/M5stack_Tab5_Arduino_Wifi_Example
#if ENABLE_WIFI
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Before WiFi.setPins - Heap: free=%u minFree=%u largest=%u\n",
                  // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // Serial.printf("[DEBUG] WiFi pins: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d RST=%d\n",
                  // TAB5_WIFI_SDIO_CLK, TAB5_WIFI_SDIO_CMD, TAB5_WIFI_SDIO_D0,
                  // TAB5_WIFI_SDIO_D1, TAB5_WIFI_SDIO_D2, TAB5_WIFI_SDIO_D3, TAB5_WIFI_SDIO_RST);
        // #endregion
    Serial.println("[WIFI] Configuring Tab5 SDIO pins for ESP32-C6 co-processor...");
    WiFi.setPins(TAB5_WIFI_SDIO_CLK, TAB5_WIFI_SDIO_CMD,
                 TAB5_WIFI_SDIO_D0, TAB5_WIFI_SDIO_D1,
                 TAB5_WIFI_SDIO_D2, TAB5_WIFI_SDIO_D3,
                 TAB5_WIFI_SDIO_RST);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] After WiFi.setPins - Heap: free=%u minFree=%u largest=%u\n",
                  // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // delay(50);  // Allow SDIO pin configuration to stabilize
    // Serial.printf("[DEBUG] After 50ms delay - Heap: free=%u minFree=%u\n",
                  // ESP.getFreeHeap(), ESP.getMinFreeHeap());
        // #endregion
    Serial.println("[WIFI] SDIO pins configured");

    // NOTE: Do NOT call Wire1.begin(31,32) or any I2C here. On Tab5, Wire1 may be
    // the same bus used for Grove Port.A; initialising it here breaks encoder detection.
    // Antenna is set after M5.begin() via setWiFiAntenna(true) (IO expander via M5).
#endif

    // Initialize M5Stack Tab5
    // #region agent log (DISABLED)
// #if ENABLE_WIFI
    // Serial.printf("[DEBUG] Before M5.begin - Heap: free=%u minFree=%u largest=%u\n",
                  // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
// #endif
        // #endregion
    auto cfg = M5.config();
    cfg.external_spk = true;
    M5.begin(cfg);

    // NOTE: setWiFiAntenna() is called AFTER Wire.begin() and encoder detection.
    // M5.getIOExpander(0) accesses the internal I2C bus and can interfere with
    // the external I2C bus (Grove Port.A) if called before Wire.begin(53,54).
    // Only WiFi.setPins() must precede M5.begin(); antenna selection can be deferred.

    // #region agent log (DISABLED)
// #if ENABLE_WIFI
    // Serial.printf("[DEBUG] After M5.begin - Heap: free=%u minFree=%u largest=%u\n",
                  // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // delay(100);  // Allow M5 initialization to complete
    // Serial.printf("[DEBUG] After 100ms delay - Heap: free=%u minFree=%u\n",
                  // ESP.getFreeHeap(), ESP.getMinFreeHeap());
// #endif
        // #endregion

    // Set display orientation (landscape, USB on left)
    M5.Display.setRotation(3);
    
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"main.cpp:688\",\"message\":\"before.setSwapBytes\",\"data\":{\"rotation\":3},\"timestamp\":%lu}\n", (unsigned long)millis());
        // #endregion
    
    // CRITICAL: Set byte swapping for BGR565 format BEFORE LVGL initialization
    // This must match the working implementation in src/src/main.cpp:352
    M5.Display.setSwapBytes(true);  // Swap bytes for BGR565 format
    delay(50);  // Allow display configuration to stabilize (matches working impl)
    
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"main.cpp:696\",\"message\":\"after.setSwapBytes\",\"data\":{\"swapBytes\":true,\"delay\":50},\"timestamp\":%lu}\n", (unsigned long)millis());
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"main.cpp:699\",\"message\":\"display.dimensions\",\"data\":{\"width\":%d,\"height\":%d},\"timestamp\":%lu}\n", 
                  // M5.Display.width(), M5.Display.height(), (unsigned long)millis());
        // #endregion

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    if (!LVGLBridge::init()) {
        Serial.println("[lvgl] LVGLBridge::init failed");
    }
#endif

    Serial.println("\n[INIT] M5Stack Tab5 initialized");

    // Get external I2C pin configuration from M5Unified
    // Tab5 Grove Port.A: SDA=GPIO53, SCL=GPIO54
    int8_t extSDA = M5.Ex_I2C.getSDA();
    int8_t extScl = M5.Ex_I2C.getSCL();

    Serial.printf("[INIT] Tab5 External I2C pins - SDA:%d SCL:%d\n", extSDA, extScl);

    // Verify pins match expected values
    if (extSDA != I2C::EXT_SDA_PIN || extScl != I2C::EXT_SCL_PIN) {
        Serial.println("[WARN] External I2C pins differ from expected!");
        Serial.printf("[WARN] Expected SDA:%d SCL:%d, got SDA:%d SCL:%d\n",
                      I2C::EXT_SDA_PIN, I2C::EXT_SCL_PIN, extSDA, extScl);
    }

    // Initialize Wire on external I2C bus (Grove Port.A)
    // This is ISOLATED from Tab5's internal I2C (display, touch, audio)
    Wire.begin(extSDA, extScl, I2C::FREQ_HZ);
    Wire.setTimeOut(I2C::TIMEOUT_MS);

    Serial.printf("[INIT] Wire initialized at %lu Hz, timeout %u ms\n",
                  (unsigned long)I2C::FREQ_HZ, I2C::TIMEOUT_MS);

    // =========================================================================
    // Initialize I2C Recovery Module (Phase G.2)
    // =========================================================================
    // Software-level bus recovery for external I2C (Grove Port.A)
    // Uses SCL toggling and Wire reinit - NO hardware peripheral resets
    I2CRecovery::init(&Wire, extSDA, extScl, I2C::FREQ_HZ);
    Serial.println("[I2C_RECOVERY] Recovery module initialized for external bus");

    // Allow I2C bus to stabilize
    delay(100);

    // =========================================================================
    // Initialize NVS Storage (Phase G.1)
    // =========================================================================
    Serial.println("\n[NVS] Initialising parameter storage...");
    // Create DisplayUI early for loading screen (but don't initialize full UI yet)
    g_ui = new DisplayUI(M5.Display);
    LoadingScreen::show(M5.Display, "INITIALISING NVS", false, false);
    
    if (!NvsStorage::init()) {
        Serial.println("[NVS] WARNING: NVS init failed - parameters will not persist");
    }

    // Scan external I2C bus for devices
    uint8_t found = scanI2CBus(Wire, "External I2C (Grove Port.A)");

    // Initialize DualEncoderService with both addresses
    // Unit A @ 0x42 (reprogrammed), Unit B @ 0x41 (factory)
    LoadingScreen::update(M5.Display, "INITIALISING ENCODERS...", false, false);
    g_encoders = new DualEncoderService(&Wire, ADDR_UNIT_A, ADDR_UNIT_B);
    g_encoders->setChangeCallback(onEncoderChange);

    // =========================================================================
    // Encoder Detection with Exponential Backoff
    // =========================================================================
    // M5ROTATE8 units contain STM32 MCUs that need boot time (50-200ms).
    // Retry with exponential backoff to handle transient I2C bus issues and
    // slow peripheral startup.
    constexpr uint8_t MAX_DETECTION_ATTEMPTS = 5;
    constexpr uint32_t INITIAL_BACKOFF_MS = 200;
    bool encoderOk = false;

    for (uint8_t attempt = 0; attempt < MAX_DETECTION_ATTEMPTS; attempt++) {
        encoderOk = g_encoders->begin();
        if (encoderOk) {
            if (attempt > 0) {
                Serial.printf("[INIT] Encoder detection succeeded on attempt %d/%d\n",
                              attempt + 1, MAX_DETECTION_ATTEMPTS);
            }
            break;
        }

        uint32_t backoff = INITIAL_BACKOFF_MS << attempt;  // 200, 400, 800, 1600, 3200
        Serial.printf("[INIT] Encoder detection attempt %d/%d failed, retrying in %lu ms\n",
                      attempt + 1, MAX_DETECTION_ATTEMPTS, (unsigned long)backoff);
        LoadingScreen::update(M5.Display,
                              attempt < 2 ? "RETRYING ENCODERS..." : "RETRYING ENCODERS (SLOW)",
                              false, false);
        esp_task_wdt_reset();
        delay(backoff);
        esp_task_wdt_reset();

        // Re-scan I2C bus on retries for diagnostic output
        if (attempt >= 1) {
            scanI2CBus(Wire, "External I2C (retry)");
        }
    }

    if (!encoderOk) {
        Serial.println("[ERROR] All encoder detection attempts failed!");
    }

    // Initialize ButtonHandler (handles Unit-A button resets)
    // NOTE: Unit-B buttons (8-15) are now reserved for Preset System.
    //       Zone mode control has been moved to the webapp.
    g_buttonHandler = new ButtonHandler();
    g_buttonHandler->setWebSocketClient(&g_wsClient);

    // Connect ButtonHandler to encoder service
    g_encoders->setButtonHandler(g_buttonHandler);
    Serial.println("[Button] Button handler initialized (presets on Unit-B)");

    // Initialize and connect CoarseModeManager
    g_encoders->setCoarseModeManager(&g_coarseModeManager);
    Serial.println("[CoarseMode] Coarse mode manager initialized");

    // =========================================================================
    // Initialize LED Feedback (Phase F.5)
    // =========================================================================
    g_ledFeedback.setEncoders(g_encoders);
    g_ledFeedback.begin();
    Serial.println("[LED] Connection status LED feedback initialized");

    // =========================================================================
    // DISABLED - PaletteLedDisplay causing encoder regression
    // Initialize Palette LED Display
    // =========================================================================
    /*
    g_paletteLedDisplay.setEncoders(g_encoders);
    g_paletteLedDisplay.begin();
    Serial.println("[LED] Palette LED display initialized");
    */

    // =========================================================================
    // Load Saved Parameters from NVS (Phase G.1)
    // =========================================================================
    if (NvsStorage::isReady()) {
        LoadingScreen::update(M5.Display, "LOADING PARAMETERS", false, false);
        uint16_t savedValues[16];
        uint8_t loadedCount = NvsStorage::loadAllParameters(savedValues);

        // Apply loaded values to encoder service (without triggering callbacks)
        for (uint8_t i = 0; i < 16; i++) {
            g_encoders->setValue(i, savedValues[i], false);
        }

        if (loadedCount > 0) {
            Serial.printf("[NVS] Restored %d parameters from flash\n", loadedCount);
            
            // DISABLED - PaletteLedDisplay
            // Update palette LED display with restored palette value
            /*
            uint8_t paletteId = static_cast<uint8_t>(savedValues[1]);  // Index 1 = Palette
            g_paletteLedDisplay.update(paletteId);
            */
        }
    }

    // Check unit status
    bool unitA = g_encoders->isUnitAAvailable();
    bool unitB = g_encoders->isUnitBAvailable();

    Serial.printf("\n[INIT] Unit A (0x%02X): %s\n", ADDR_UNIT_A, unitA ? "OK" : "NOT FOUND");
    Serial.printf("[INIT] Unit B (0x%02X): %s\n", ADDR_UNIT_B, unitB ? "OK" : "NOT FOUND");

    if (unitA && unitB) {
        Serial.println("\n[OK] Both units detected - 16 encoders available!");
        Serial.println("[OK] Milestone E: Dual encoder service active");

        // Flash all LEDs green briefly to indicate success
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"boot\",\"hypothesisId\":\"H2\",\"location\":\"main.cpp:flash\",\"message\":\"flashGreen.start\",\"data\":{\"unitA\":true,\"unitB\":true},\"timestamp\":%lu}\n",
                      // static_cast<unsigned long>(millis()));
                // #endregion
        g_encoders->transportA().setAllLEDs(0, 64, 0);
        g_encoders->transportB().setAllLEDs(0, 64, 0);
        delay(200);
        g_encoders->allLedsOff();
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"boot\",\"hypothesisId\":\"H2\",\"location\":\"main.cpp:flash\",\"message\":\"flashGreen.cleared\",\"data\":{},\"timestamp\":%lu}\n",
                      // static_cast<unsigned long>(millis()));
                // #endregion

        // Set status LEDs (both green for connected)
        updateConnectionLeds();

        // DisplayUI object already created earlier (for early loading screen)
        // Update loading screen with encoder status
        LoadingScreen::update(M5.Display, "ENCODERS INITIALISED", unitA, unitB);

    } else if (unitA || unitB) {
        // Partial success - one unit available
        Serial.println("\n[WARN] Only one unit detected - 8 encoders available");
        Serial.println("[WARN] Check wiring for missing unit");

        // Set status LEDs (green for available, red for missing)
        updateConnectionLeds();

        // DisplayUI object already created earlier (for early loading screen)
        // Update loading screen with encoder status
        LoadingScreen::update(M5.Display, "ENCODERS INITIALISED", unitA, unitB);

    } else {
        Serial.println("\n[ERROR] No encoder units found!");
        Serial.println("[ERROR] Check wiring:");
        Serial.println("  - Is Unit A (0x42) connected to Grove Port.A?");
        Serial.println("  - Is Unit B (0x41) connected to Grove Port.A?");
        Serial.println("  - Are the Grove cables properly seated?");

        // DisplayUI object already created earlier (for early loading screen)
        // Update loading screen - encoders not found
        LoadingScreen::update(M5.Display, "INITIALISING ENCODERS", false, false);
    }

    // =========================================================================
    // WiFi Antenna Selection (deferred from before Wire.begin)
    // =========================================================================
    // CRITICAL: Must happen AFTER Wire.begin() and encoder detection.
    // M5.getIOExpander(0) accesses internal I2C and can corrupt external bus state.
#if ENABLE_WIFI
    setWiFiAntenna(true);
    Serial.println("[WiFi] Using external MMCX antenna");
#endif

    // =========================================================================
    // Initialize Network (Milestone F)
    // =========================================================================
#if ENABLE_WIFI
    Serial.println("\n[NETWORK] Initialising WiFi...");

    // Initialize ParameterHandler (bridges encoders ↔ WebSocket ↔ display)
    // CRITICAL FIX: Add null check validation
    if (!g_encoders) {
        Serial.println("[ERROR] ParameterHandler: null encoder service!");
    }
    g_paramHandler = new ParameterHandler(g_encoders, &g_wsClient);
    g_paramHandler->setButtonHandler(g_buttonHandler);
    g_paramHandler->setDisplayCallback([](uint8_t index, uint16_t value) {
        // Called when parameters are updated from WebSocket
        // Update radial gauge display (fast, non-blocking)
        if (g_ui && s_uiInitialized) {  // Only update if UI is initialized
            g_ui->updateValue(index, value, false);
        }
    });

    // =========================================================================
    // Initialize Preset Manager (Phase 8: 8-bank preset system)
    // =========================================================================
    g_presetManager = new PresetManager(g_paramHandler, &g_wsClient);
    if (g_presetManager->init()) {
        Serial.printf("[PRESET] Initialized with %d stored presets\n",
                      g_presetManager->getOccupiedCount());

        // Set up feedback callback for UI updates
        g_presetManager->setFeedbackCallback([](uint8_t slot, PresetAction action, bool success) {
            const char* actionName = "";
            switch (action) {
                case PresetAction::SAVE:   actionName = "SAVE"; break;
                case PresetAction::RECALL: actionName = "RECALL"; break;
                case PresetAction::DELETE: actionName = "DELETE"; break;
                case PresetAction::ERROR:  actionName = "ERROR"; break;
            }
            Serial.printf("[PRESET] Slot %d %s: %s\n", slot, actionName,
                          success ? "OK" : "FAILED");

            // Update active preset slot on successful recall
            if (action == PresetAction::RECALL && success) {
                g_activePresetSlot = slot;
            } else if (action == PresetAction::DELETE && success && g_activePresetSlot == slot) {
                g_activePresetSlot = 255;  // Clear active if deleted
            }

            // Flash LED feedback
            if (g_encoders) {
                uint8_t ledIndex = 8 + slot;  // Unit-B LEDs
                if (success) {
                    if (action == PresetAction::SAVE) {
                        g_encoders->flashLed(ledIndex, 255, 200, 0);   // Yellow for save
                    } else if (action == PresetAction::RECALL) {
                        g_encoders->flashLed(ledIndex, 0, 255, 0);     // Green for recall
                    } else if (action == PresetAction::DELETE) {
                        g_encoders->flashLed(ledIndex, 255, 0, 0);     // Red for delete
                    }
                } else {
                    g_encoders->flashLed(ledIndex, 255, 0, 0);         // Red for error
                }
            }

            // UI feedback for preset slots
            if (g_ui && s_uiInitialized && success) {  // Only update if UI is initialized
                if (action == PresetAction::SAVE) {
                    g_ui->showPresetSaveFeedback(slot);
                    // Refresh slot data after save
                    PresetData preset;
                    if (g_presetManager && g_presetManager->getPreset(slot, preset)) {
                        g_ui->updatePresetSlot(slot, true,
                            preset.effectId, preset.paletteId, preset.brightness);
                    }
                } else if (action == PresetAction::RECALL) {
                    g_ui->setActivePresetSlot(slot);
                    g_ui->showPresetRecallFeedback(slot);
                } else if (action == PresetAction::DELETE) {
                    g_ui->showPresetDeleteFeedback(slot);
                    g_ui->updatePresetSlot(slot, false, 0, 0, 0);
                }
            }
        });

        // Initialize preset slot UI from NVS (deferred until UI is initialized)
        // This will be called after g_ui->begin() in the loop() initialization block
    } else {
        Serial.println("[PRESET] WARNING: Preset manager init failed");
    }

    // Initialize WsMessageRouter (routes incoming WebSocket messages)
    // Pass ZoneComposerUI if available (for zone state updates)
    ZoneComposerUI* zoneUI = (g_ui) ? g_ui->getZoneComposerUI() : nullptr;
    if (zoneUI) {
        zoneUI->setWebSocketClient(&g_wsClient);
    }
    WsMessageRouter::init(g_paramHandler, &g_wsClient, zoneUI, g_ui);

    // Wire ZoneComposerUI to PresetManager for zone state capture
    if (g_presetManager && zoneUI) {
        g_presetManager->setZoneComposerUI(zoneUI);
    }

    // Register WebSocket message callback
    g_wsClient.onMessage([](JsonDocument& doc) {
        // Handle metadata lists (effect/palette names) for UI
        if (doc["type"].is<const char*>()) {
            const char* type = doc["type"];

            if (type && strcmp(type, "effects.list") == 0 && doc["success"].is<bool>() && doc["success"].as<bool>()) {
                JsonObject data = doc["data"].as<JsonObject>();
                JsonArray effects = data["effects"].as<JsonArray>();

                // Parse pagination first to detect page reset
                JsonObject pagination = data["pagination"].as<JsonObject>();
                int currentPage = 0;
                if (pagination["page"].is<int>()) {
                    currentPage = pagination["page"].as<int>();
                }

                // Reset position counter on first page (new list fetch)
                if (currentPage <= 1) {
                    s_effectCount = 0;
                    memset(s_effectIds, 0, sizeof(s_effectIds));
                    memset(s_effectKnown, 0, sizeof(s_effectKnown));
                }

                // Position-indexed storage: effects stored at s_effectCount++
                // effectId is uint16_t (hex-based, e.g. 0x0100 = 256)
                uint16_t effectsOnPage = 0;
                for (JsonObject e : effects) {
                    if (!e["id"].is<int>() || !e["name"].is<const char*>()) continue;
                    int idInt = e["id"].as<int>();
                    if (idInt < 0 || idInt > 0xFFFF) continue;  // Valid effectId range
                    if (s_effectCount >= MAX_EFFECTS) break;     // Capacity limit

                    uint16_t effectId = static_cast<uint16_t>(idInt);
                    const char* name = e["name"].as<const char*>();
                    if (!name) continue;

                    // Store at next position index
                    uint16_t pos = s_effectCount;
                    s_effectIds[pos] = effectId;
                    strncpy(s_effectNames[pos], name, EFFECT_NAME_MAX - 1);
                    s_effectNames[pos][EFFECT_NAME_MAX - 1] = '\0';
                    s_effectKnown[pos] = true;
                    s_effectCount++;
                    effectsOnPage++;
                }
                Serial.printf("[Effects] Page %d: %u effects stored, total=%u\n",
                              currentPage, effectsOnPage, s_effectCount);

                // Update pagination tracking
                if (pagination["pages"].is<int>()) {
                    int pages = pagination["pages"].as<int>();
                    if (pages > 0 && pages <= 255) s_effectPages = static_cast<uint8_t>(pages);
                }
                if (currentPage > 0 && currentPage <= 255 &&
                    static_cast<uint8_t>(currentPage) >= s_effectNextPage) {
                    s_effectNextPage = static_cast<uint8_t>(currentPage) + 1;
                }

                // Update ParameterMap: effect encoder max = last position index
                if (s_effectCount > 0) {
                    uint8_t effectMax = static_cast<uint8_t>(s_effectCount - 1);
                    updateParameterMetadata(0, 0, effectMax);  // EffectId is index 0
                    Serial.printf("[ParamMap] Updated Effect max from effects.list: %u (total=%u)\n",
                                  effectMax, s_effectCount);
                }

                // Also update zone effect max values (indices 8, 10, 12, 14)
                if (g_ui && s_uiInitialized && g_ui->getCurrentScreen() == UIScreen::ZONE_COMPOSER) {
                    uint8_t zoneEffectMax = getParameterMax(0);
                    for (uint8_t i = 8; i <= 14; i += 2) {
                        updateParameterMetadata(i, 0, zoneEffectMax);
                    }
                }

                updateUiEffectPaletteLabels();
                return;
            }

            if (type && strcmp(type, "palettes.list") == 0 && doc["success"].is<bool>() && doc["success"].as<bool>()) {
                JsonObject data = doc["data"].as<JsonObject>();
                JsonArray palettes = data["palettes"].as<JsonArray>();
                for (JsonObject p : palettes) {
                    // ArduinoJson stores small integers as int - use is<int>()
                    if (!p["id"].is<int>() || !p["name"].is<const char*>()) continue;
                    int idInt = p["id"].as<int>();
                    if (idInt < 0 || idInt > 255) continue;  // Invalid ID range
                    uint8_t id = static_cast<uint8_t>(idInt);
                    const char* name = p["name"].as<const char*>();
                    if (id < MAX_PALETTES && name) {
                        strncpy(s_paletteNames[id], name, PALETTE_NAME_MAX - 1);
                        s_paletteNames[id][PALETTE_NAME_MAX - 1] = '\0';
                        s_paletteKnown[id] = true;
                    }
                }

                JsonObject pagination = data["pagination"].as<JsonObject>();
                // ArduinoJson stores small integers as int, not uint8_t - use is<int>()
                if (pagination["pages"].is<int>()) {
                    int pages = pagination["pages"].as<int>();
                    if (pages > 0 && pages <= 255) s_palettePages = static_cast<uint8_t>(pages);
                }
                if (pagination["page"].is<int>()) {
                    int page = pagination["page"].as<int>();
                    if (page > 0 && page <= 255 && static_cast<uint8_t>(page) >= s_paletteNextPage) {
                        s_paletteNextPage = static_cast<uint8_t>(page) + 1;
                    }
                }

                // Extract total palette count and update ParameterMap metadata
                // Palette max = total - 1 (0-indexed)
                // ArduinoJson stores integers as int, not uint16_t - use is<int>()
                if (pagination["total"].is<int>()) {
                    int totalInt = pagination["total"].as<int>();
                    if (totalInt > 0 && totalInt <= 256) {
                        uint8_t paletteMax = (totalInt > 1) ? static_cast<uint8_t>(totalInt - 1) : 0;
                        updateParameterMetadata(1, 0, paletteMax);  // PaletteId is now index 1
                        Serial.printf("[ParamMap] Updated Palette max from palettes.list: %u (total=%d)\n", paletteMax, totalInt);
                    }
                } else if (data["total"].is<int>()) {
                    // Some API versions put total at data level
                    int totalInt = data["total"].as<int>();
                    if (totalInt > 0 && totalInt <= 256) {
                        uint8_t paletteMax = (totalInt > 1) ? static_cast<uint8_t>(totalInt - 1) : 0;
                        updateParameterMetadata(1, 0, paletteMax);  // PaletteId is now index 1
                        Serial.printf("[ParamMap] Updated Palette max from palettes.list: %u (total=%d)\n", paletteMax, totalInt);
                    }
                }
                // #region agent log (DISABLED)
                // Serial.printf("[DEBUG] After palette metadata update - Heap: free=%u minFree=%u largest=%u\n",
                              // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
                                // #endregion

                updateUiEffectPaletteLabels();
                return;
            }

            // Handle colorCorrection.getConfig response
            if (type && strcmp(type, "colorCorrection.getConfig") == 0) {
                if (doc["success"].is<bool>() && doc["success"].as<bool>()) {
                    ColorCorrectionState cc;
                    cc.valid = true;
                    
                    // CRITICAL FIX: Handle both direct fields and nested data object
                    JsonObject data = doc["data"].is<JsonObject>() ? doc["data"].as<JsonObject>() : doc.as<JsonObject>();
                    
                    if (data["gammaEnabled"].is<bool>()) {
                        cc.gammaEnabled = data["gammaEnabled"].as<bool>();
                    }
                    if (data["gammaValue"].is<float>()) {
                        cc.gammaValue = data["gammaValue"].as<float>();
                    }
                    if (data["autoExposureEnabled"].is<bool>()) {
                        cc.autoExposureEnabled = data["autoExposureEnabled"].as<bool>();
                    }
                    if (data["autoExposureTarget"].is<uint8_t>()) {
                        cc.autoExposureTarget = data["autoExposureTarget"].as<uint8_t>();
                    }
                    if (data["brownGuardrailEnabled"].is<bool>()) {
                        cc.brownGuardrailEnabled = data["brownGuardrailEnabled"].as<bool>();
                    }
                    if (data["mode"].is<uint8_t>()) {
                        cc.mode = data["mode"].as<uint8_t>();
                    }
                    if (data["maxGreenPercentOfRed"].is<uint8_t>()) {
                        cc.maxGreenPercentOfRed = data["maxGreenPercentOfRed"].as<uint8_t>();
                    }
                    if (data["maxBluePercentOfRed"].is<uint8_t>()) {
                        cc.maxBluePercentOfRed = data["maxBluePercentOfRed"].as<uint8_t>();
                    }
                    
                    g_wsClient.setColorCorrectionState(cc);
                    s_colourCorrectionSyncPending = true;
                    Serial.println("[WS] Color correction config synced");
                }
                return;
            }

            // CRITICAL FIX: Handle colorCorrection.setConfig response (confirmation)
            if (type && strcmp(type, "colorCorrection.setConfig") == 0) {
                // #region agent log (DISABLED)
                // bool hasSuccess = doc["success"].is<bool>();
                // bool success = hasSuccess && doc["success"].as<bool>();
                // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H4\",\"location\":\"main.cpp:1202\",\"message\":\"response.colorCorrection.setConfig\",\"data\":{\"hasSuccess\":%d,\"success\":%d},\"timestamp\":%lu}\n",
                              // hasSuccess ? 1 : 0, success ? 1 : 0, (unsigned long)millis());
                                // #endregion
                if (doc["success"].is<bool>() && doc["success"].as<bool>()) {
                    // Update local cache from response if provided
                    JsonObject data = doc["data"].is<JsonObject>() ? doc["data"].as<JsonObject>() : doc.as<JsonObject>();
                    ColorCorrectionState cc = g_wsClient.getColorCorrectionState();
                    
                    if (data["mode"].is<uint8_t>()) {
                        cc.mode = data["mode"].as<uint8_t>();
                    }
                    // Other fields already updated optimistically in send methods
                    
                    g_wsClient.setColorCorrectionState(cc);
                    Serial.println("[WS] Color correction config update confirmed");
                } else {
                    Serial.println("[WS] Color correction config update failed");
                }
                return;
            }

            // CRITICAL FIX: Handle colorCorrection.setMode response (confirmation)
            if (type && strcmp(type, "colorCorrection.setMode") == 0) {
                if (doc["success"].is<bool>() && doc["success"].as<bool>()) {
                    JsonObject data = doc["data"].is<JsonObject>() ? doc["data"].as<JsonObject>() : doc.as<JsonObject>();
                    ColorCorrectionState cc = g_wsClient.getColorCorrectionState();
                    
                    if (data["mode"].is<uint8_t>()) {
                        cc.mode = data["mode"].as<uint8_t>();
                    }
                    if (data["modeName"].is<const char*>()) {
                        Serial.printf("[WS] Color correction mode set to: %s\n", data["modeName"].as<const char*>());
                    }
                    
                    g_wsClient.setColorCorrectionState(cc);
                    s_colourCorrectionSyncPending = true;
                    Serial.println("[WS] Color correction mode update confirmed");
                } else {
                    Serial.println("[WS] Color correction mode update failed");
                }
                return;
            }

            // CRITICAL FIX: Handle error responses from v2 firmware
            if (type && strcmp(type, "error") == 0) {
                const char* errorCode = doc["error"]["code"] | "UNKNOWN";
                const char* errorMsg = doc["error"]["message"] | "Unknown error";
                const char* requestId = doc["requestId"] | "";
                Serial.printf("[WS ERROR] Code: %s, Message: %s, RequestId: %s\n",
                              errorCode, errorMsg, requestId);
                // Visual feedback: Flash LED 0 red to indicate error
                if (g_encoders) {
                    g_encoders->flashLed(0, 255, 0, 0);  // Red for error
                }
                return;
            }

            // Extract audio metrics from status messages for footer display
            if (type && strcmp(type, "status") == 0) {
                float bpm = 0.0f;
                const char* key = nullptr;
                float micLevel = -80.0f;  // Default to silence

                // Debug: Log all keys in status message (first time only)
                static bool s_loggedStatusKeys = false;
                if (!s_loggedStatusKeys) {
                    Serial.println("[FOOTER DEBUG] Status message keys:");
                    for (JsonPair kv : doc.as<JsonObject>()) {
                        Serial.printf("  - %s\n", kv.key().c_str());
                    }
                    s_loggedStatusKeys = true;
                }

                // Extract BPM
                if (doc["bpm"].is<float>()) {
                    bpm = doc["bpm"].as<float>();
                } else if (doc["bpm"].is<int>()) {
                    bpm = static_cast<float>(doc["bpm"].as<int>());
                }

                // Extract key
                if (doc["key"].is<const char*>()) {
                    key = doc["key"].as<const char*>();
                }

                // Extract mic level (could be "mic", "micLevel", "micDb", or "inputLevel")
                if (doc["mic"].is<float>()) {
                    micLevel = doc["mic"].as<float>();
                } else if (doc["micLevel"].is<float>()) {
                    micLevel = doc["micLevel"].as<float>();
                } else if (doc["micDb"].is<float>()) {
                    micLevel = doc["micDb"].as<float>();
                } else if (doc["inputLevel"].is<float>()) {
                    micLevel = doc["inputLevel"].as<float>();
                } else if (doc["audioInput"].is<float>()) {
                    micLevel = doc["audioInput"].as<float>();
                }

                // Debug: Log extracted values
                static uint32_t s_lastDebugLog = 0;
                uint32_t now = millis();
                if (now - s_lastDebugLog >= 5000) {  // Log every 5 seconds
                    Serial.printf("[FOOTER DEBUG] Extracted: bpm=%.1f, key=%s, mic=%.1fdB\n",
                                  bpm, key ? key : "null", micLevel);
                    s_lastDebugLog = now;
                }

                // Update footer if UI is initialized
                if (g_ui && s_uiInitialized) {
                    g_ui->updateAudioMetrics(bpm, key, micLevel);
                }

                // DISABLED - PaletteLedDisplay causing encoder regression
                // Update palette LED display if palette parameter is in status message
                // Guard against snapback by ignoring during local-change holdoff
                /*
                if (doc["paletteId"].is<uint8_t>() || doc["paletteId"].is<int>()) {
                    uint8_t paletteId = 0;
                    if (doc["paletteId"].is<uint8_t>()) {
                        paletteId = doc["paletteId"].as<uint8_t>();
                    } else if (doc["paletteId"].is<int>()) {
                        int val = doc["paletteId"].as<int>();
                        if (val >= 0 && val <= 255) {
                            paletteId = static_cast<uint8_t>(val);
                        }
                    }
                    bool holdoff = (g_paramHandler && g_paramHandler->isInLocalHoldoff(1)); // index 1 = Palette
                    // #region agent log (DISABLED)
                    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"palette-latch\",\"hypothesisId\":\"A\",\"location\":\"main.cpp:wsStatus\",\"message\":\"status.palette.update\",\"data\":{\"paletteId\":%u,\"holdoff\":%d,\"timestamp\":%lu}}\n",
                                  // paletteId, holdoff ? 1 : 0, static_cast<unsigned long>(millis()));
                                        // #endregion
                    if (!holdoff) {
                        g_paletteLedDisplay.update(paletteId);
                    }
                }
                */
            }
        }

        WsMessageRouter::route(doc);
    });

    // Start WiFi connection
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Before WiFiManager::begin() - Heap: free=%u minFree=%u largest=%u\n",
                  // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
        // #endregion
    
    // Begin WiFi - connect to v2's AP (deterministic Portable Mode)
    // Primary: LightwaveOS-AP (192.168.4.1), Secondary: user network via build flags
    g_wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] After WiFiManager::begin() - Heap: free=%u minFree=%u largest=%u\n",
                  // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
        // #endregion
    // Note: WiFiManager::begin() starts a scan first - connection happens after scan completes
#else
    // WiFi disabled on ESP32-P4 due to SDIO pin configuration issues
    // See Config.h ENABLE_WIFI flag for details
    Serial.println("\n[NETWORK] WiFi DISABLED - ESP32-P4 SDIO pin config not supported");
    Serial.println("[NETWORK] Encoder functionality available, network sync disabled");
#endif // ENABLE_WIFI

    // =========================================================================
    // Initialize Touch Handler (Phase G.3)
    // =========================================================================
    Serial.println("\n[TOUCH] Initialising touch screen handler...");
    LoadingScreen::update(M5.Display, "INITIALISING TOUCH", 
                         g_encoders ? g_encoders->isUnitAAvailable() : false,
                         g_encoders ? g_encoders->isUnitBAvailable() : false);
    g_touchHandler.init();
    g_touchHandler.setEncoderService(g_encoders);

    // CRITICAL FIX: Gate touch processing to GLOBAL screen only
    // This prevents touch events from firing main dashboard callbacks
    // when Zone Composer or Connectivity Tab are active
    g_touchHandler.setScreenGate([]() {
        return g_ui && g_ui->getCurrentScreen() == UIScreen::GLOBAL;
    });

    // Register long press callback - resets parameter to default
    g_touchHandler.onLongPress([](uint8_t paramIndex) {
        // Parameter reset is handled internally by TouchHandler
        // This callback is for additional actions (e.g., LED feedback, sound)
        Serial.printf("[TOUCH] Long press reset on param %d\n", paramIndex);

        // Flash encoder LED cyan for reset feedback (same as encoder button)
        if (g_encoders) {
            g_encoders->flashLed(paramIndex, 0, 128, 255);
        }
    });

    // Optional: Register tap callback for highlight feedback
    g_touchHandler.onTap([](uint8_t paramIndex) {
        // Flash encoder LED for tap feedback
        if (g_encoders) {
            g_encoders->flashLed(paramIndex, 128, 128, 128);
        }
    });

    // Touch action row (colour controls)
    g_touchHandler.onActionButton([](uint8_t buttonIndex) {
        handleActionButton(buttonIndex);
    });
    

    Serial.println("[TOUCH] Touch handler initialized - long press to reset params");

    Serial.println("\n============================================");
    Serial.println("  Setup complete - turn encoders to test");
    Serial.println("  WiFi connecting in background...");
    Serial.println("  Touch screen: long press to reset params");
    Serial.println("============================================\n");
}

// ============================================================================
// Cleanup Function (for memory leak prevention)
// ============================================================================
// CRITICAL FIX: Cleanup global objects to prevent memory leaks
// Called on shutdown/restart (if implemented) or can be called manually
void cleanup() {
    Serial.println("[CLEANUP] Cleaning up global objects...");
    
    if (g_ui) {
        delete g_ui;
        g_ui = nullptr;
        Serial.println("[CLEANUP] DisplayUI deleted");
    }
    
    if (g_presetManager) {
        delete g_presetManager;
        g_presetManager = nullptr;
        Serial.println("[CLEANUP] PresetManager deleted");
    }
    
    if (g_paramHandler) {
        delete g_paramHandler;
        g_paramHandler = nullptr;
        Serial.println("[CLEANUP] ParameterHandler deleted");
    }
    
    if (g_buttonHandler) {
        delete g_buttonHandler;
        g_buttonHandler = nullptr;
        Serial.println("[CLEANUP] ButtonHandler deleted");
    }
    
    if (g_encoders) {
        delete g_encoders;
        g_encoders = nullptr;
        Serial.println("[CLEANUP] DualEncoderService deleted");
    }
    
    Serial.println("[CLEANUP] Cleanup complete");
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
    // CRITICAL: Reset watchdog at START of every loop iteration
    esp_task_wdt_reset();

    // Update M5Stack (handles button events, touch, etc.)
    M5.update();

    // =========================================================================
    // TOUCH: Process touch events (Phase G.3)
    // =========================================================================
    g_touchHandler.update();

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    LVGLBridge::update();
    esp_task_wdt_reset();  // Reset after LVGL (can block on SPI I/O)
#endif

    // =========================================================================
    // SERIAL: Process simple command input
    // =========================================================================
    pollSerialCommands();

    // =========================================================================
    // NETWORK: Service WebSocket EARLY to prevent TCP timeouts (K1 pattern)
    // =========================================================================
    // #region agent log (DISABLED)
// #if ENABLE_WS_DIAGNOSTICS
    // static uint32_t s_lastWsUpdateLog = 0;
    // if ((uint32_t)(millis() - s_lastWsUpdateLog) >= 1000) {  // Log every 1s
        // Serial.printf("[DEBUG] Before wsClient.update() - Heap: free=%u minFree=%u largest=%u\n",
                      // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
        // s_lastWsUpdateLog = millis();
    // }
// #endif
        // #endregion
    g_wsClient.update();
    esp_task_wdt_reset();  // Reset after WebSocket (can block on network I/O)

    // Deferred UI sync — set by WebSocket callbacks, safe to run here in main loop
    if (s_colourCorrectionSyncPending) {
        s_colourCorrectionSyncPending = false;
        syncColourCorrectionUi();
    }
    // #region agent log (DISABLED)
// #if ENABLE_WS_DIAGNOSTICS
    // if ((uint32_t)(millis() - s_lastWsUpdateLog) >= 1000) {
        // Serial.printf("[DEBUG] After wsClient.update() - Heap: free=%u minFree=%u largest=%u\n",
                      // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // }
// #endif
        // #endregion

    // =========================================================================
    // NETWORK: Update WiFi state machine
    // =========================================================================
    g_wifiManager.update();
    esp_task_wdt_reset();  // Reset after WiFi state machine (can block on network events)

    // =========================================================================
    // LED FEEDBACK: Update connection status LEDs (Phase F.5)
    // =========================================================================
    updateConnectionLeds();

    // =========================================================================
    // COARSE MODE: Poll ENC-A switch state
    // =========================================================================
    static uint8_t s_lastSwitchState = 0;
    if (g_encoders && g_encoders->isUnitAAvailable()) {
        uint8_t currentSwitchState = g_encoders->transportA().getInputSwitch();
        if (currentSwitchState != s_lastSwitchState) {
            Serial.printf("[CoarseMode] Switch state changed: %d -> %d\n", 
                         s_lastSwitchState, currentSwitchState);
            g_coarseModeManager.updateSwitchState(currentSwitchState);
            s_lastSwitchState = currentSwitchState;
        }
    }

    // =========================================================================
    // DISABLED - PaletteLedDisplay causing encoder regression
    // PALETTE LED DISPLAY: Update animation (only after dashboard loads)
    // =========================================================================
    // Only update palette LEDs after UI is initialized (dashboard loaded)
    // This prevents LEDs from activating during boot sequence
    /*
    if (s_uiInitialized) {
        // #region agent log (DISABLED)
        // static uint32_t s_lastLoopLog = 0;
        // uint32_t loopNow = millis();
        // if (loopNow - s_lastLoopLog >= 1000) {  // Log every 1s to track call frequency
            // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"main.cpp:1503\",\"message\":\"loop.updateAnimation.call\",\"data\":{\"loopTime\":%lu},\"timestamp\":%lu}\n",
                // static_cast<unsigned long>(loopNow), static_cast<unsigned long>(loopNow));
            // s_lastLoopLog = loopNow;
        // }
                // #endregion
        g_paletteLedDisplay.updateAnimation();
    }
    */
    g_ledFeedback.update();  // Non-blocking breathing animation

    // =========================================================================
    // UI: Initialize full UI after WiFi connects (deferred from setup)
    // =========================================================================
    // s_uiInitialized is declared at file scope (line ~124)
    if (g_ui && !s_uiInitialized) {
#if ENABLE_WIFI
        if (g_wifiManager.isConnected()) {
#else
        // If WiFi disabled, initialize UI immediately
        {
#endif
            // WiFi is connected (or WiFi disabled) - safe to initialize UI now
            // #region agent log (DISABLED)
            // Serial.printf("[DEBUG] WiFi connected, initialising UI - Heap: free=%u minFree=%u largest=%u\n",
                          // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
                        // #endregion
            
            // Show UI initialization message before hiding loading screen
            bool unitA = g_encoders ? g_encoders->isUnitAAvailable() : false;
            bool unitB = g_encoders ? g_encoders->isUnitBAvailable() : false;
            LoadingScreen::update(M5.Display, "INITIALISING UI", unitA, unitB);
            delay(500); // Brief display of UI initialization message
            
            // Hide loading screen
            LoadingScreen::hide(M5.Display);
            
            // Initialize full UI
            // Serial.printf("[MAIN_TRACE] Before g_ui->begin() @ %lu ms\n", millis());
            esp_task_wdt_reset();
            g_ui->begin();
            // Serial.printf("[MAIN_TRACE] After g_ui->begin() @ %lu ms\n", millis());
            esp_task_wdt_reset();
            // #region agent log (DISABLED)
            // Serial.printf("[DEBUG] After DisplayUI::begin() - Heap: free=%u minFree=%u largest=%u\n",
                          // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
                        // #endregion
            
            // Wire up action button callback for LVGL
            #if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
            g_ui->setActionButtonCallback(handleActionButton);
            g_ui->setRetryButtonCallback([]() {
                #if ENABLE_WIFI
                g_wifiManager.triggerRetry();
                #endif
            });
            
            // Initialize WebSocket status in footer
            #if ENABLE_WIFI
            g_ui->updateWebSocketStatus(g_wsClient.getStatus());
            if (g_wsClient.isConnected()) {
                g_ui->setWebSocketConnected(true, millis());
            }
            #endif
            #endif
            
            // Initialize OTA HTTP server (once, after WiFi connects)
            #if ENABLE_WIFI
            if (!g_otaServer && g_wifiManager.isConnected()) {
                g_otaServer = new AsyncWebServer(80);
                
                // Register OTA endpoints
                g_otaServer->on("/api/v1/firmware/version", HTTP_GET, 
                                OtaHandler::handleVersion);
                g_otaServer->on("/api/v1/firmware/update", HTTP_POST,
                                [](AsyncWebServerRequest* request) {
                                    OtaHandler::handleV1Update(request);
                                },
                                [](AsyncWebServerRequest* request, const String& filename,
                                   size_t index, uint8_t* data, size_t len, bool final) {
                                    OtaHandler::handleUpload(request, filename, index, data, len, final);
                                });
                g_otaServer->on("/update", HTTP_POST,
                                [](AsyncWebServerRequest* request) {
                                    OtaHandler::handleLegacyUpdate(request);
                                },
                                [](AsyncWebServerRequest* request, const String& filename,
                                   size_t index, uint8_t* data, size_t len, bool final) {
                                    OtaHandler::handleUpload(request, filename, index, data, len, final);
                                });
                
                g_otaServer->begin();
                Serial.println("[OTA] HTTP server started on port 80");
                Serial.printf("[OTA] Endpoints: GET /api/v1/firmware/version, POST /api/v1/firmware/update, POST /update\n");
            }
            #endif
            
            // Update connection state (reuse unitA and unitB from above)
            g_ui->setConnectionState(
#if ENABLE_WIFI
                g_wifiManager.isConnected(),
#else
                false,
#endif
                false, unitA, unitB);
            
            // Show initial values on radial gauges
            if (g_encoders) {
                for (uint8_t i = 0; i < 16; i++) {
                    g_ui->updateValue(i, g_encoders->getValue(i), false);
                }
            }
            
            // Initialize preset slot UI from NVS (now that UI is ready)
            if (g_presetManager) {
                g_ui->refreshAllPresetSlots(g_presetManager);
                Serial.println("[PRESET] UI slots refreshed from NVS");
            }
            
            s_uiInitialized = true;
            Serial.println("[UI] Full UI initialized after WiFi connection");

            // Wire ZoneComposerUI to WebSocket client NOW (deferred from setup()
            // because ZoneComposerUI doesn't exist until after WiFi connects)
            ZoneComposerUI* zoneUI = g_ui->getZoneComposerUI();
            if (zoneUI) {
                zoneUI->setWebSocketClient(&g_wsClient);
                Serial.println("[UI] ZoneComposerUI wired to WebSocket client");

                // Re-init WsMessageRouter with actual ZoneComposerUI pointer
                // (setup() passed nullptr because UI didn't exist yet)
                WsMessageRouter::init(g_paramHandler, &g_wsClient, zoneUI, g_ui);
                Serial.println("[UI] WsMessageRouter re-initialised with ZoneComposerUI");

                // Wire PresetManager to ZoneComposerUI
                if (g_presetManager) {
                    g_presetManager->setZoneComposerUI(zoneUI);
                }
            } else {
                Serial.println("[UI] WARNING: ZoneComposerUI not available after UI init");
            }
            
            // DISABLED - PaletteLedDisplay
            // Enable palette LED display now that dashboard is loaded
            // g_paletteLedDisplay.setEnabled(true);
            // Serial.println("[LED] Palette LED display enabled (dashboard ready)");
        }
    }
    
    // =========================================================================
    // LOADING SCREEN: Update animation and message while waiting
    // =========================================================================
    if (g_ui && !s_uiInitialized) {
        // Update loading screen with current state (encoders -> WiFi -> host)
        bool unitA = g_encoders ? g_encoders->isUnitAAvailable() : false;
        bool unitB = g_encoders ? g_encoders->isUnitBAvailable() : false;
        
        const char* message = nullptr;  // No fallback - will be set by conditions below
        
        // Priority 1: Encoder initialization status
        if (!g_encoders || (!unitA && !unitB)) {
            message = "INITIALISING ENCODERS";
        } else {
            // Encoders are available - show encoder status first, then network status
            static bool s_encodersStatusShown = false;
            static uint32_t s_encodersStatusTime = 0;
            
            if (!s_encodersStatusShown) {
                s_encodersStatusShown = true;
                s_encodersStatusTime = millis();
                message = "ENCODERS INITIALISED";
            } else if (millis() - s_encodersStatusTime < 2000) {
                // Show encoder status for 2 seconds
                message = "ENCODERS INITIALISED";
            } else {
                // Move to network status after encoder status shown
#if ENABLE_WIFI
                WiFiConnectionStatus wifiStatus = g_wifiManager.getStatus();
                if (wifiStatus == WiFiConnectionStatus::CONNECTING) {
                    // Show detailed WiFi sub-states during connection
                    wl_status_t wifiRawStatus = WiFi.status();
                    IPAddress ip = WiFi.localIP();
                    
                    if (wifiRawStatus == WL_IDLE_STATUS || wifiRawStatus == WL_DISCONNECTED) {
                        message = "SCANNING NETWORKS";
                    } else if (wifiRawStatus != WL_CONNECTED) {
                        // Not connected yet - check if we have an IP to distinguish states
                        if (ip == INADDR_NONE || ip == IPAddress(0, 0, 0, 0)) {
                            message = "AUTHENTICATING";
                        } else {
                            message = "OBTAINING IP";
                        }
                    } else {
                        message = "CONNECTING TO WIFI";
                    }
                } else if (wifiStatus == WiFiConnectionStatus::CONNECTED) {
                    message = "CONNECTED TO WIFI";
                } else if (wifiStatus == WiFiConnectionStatus::MDNS_RESOLVING) {
                    message = "RESOLVING HOST";
                } else if (wifiStatus == WiFiConnectionStatus::MDNS_RESOLVED) {
                    if (!g_wsConfigured) {
                        message = "CONNECTING TO HOST";  // Animated dots will be shown
                    } else {
                        message = "CONNECTED TO HOST";
                    }
                }
#endif
            }
        }
        
        // Only show message if one was set (no fallback "INITIALISING...")
        if (message) {
            LoadingScreen::update(M5.Display, message, unitA, unitB);
        }
    }

    // =========================================================================
    // NETWORK: Handle mDNS resolution and WebSocket connection
    // =========================================================================
#if ENABLE_WIFI
    static bool s_wifiWasConnected = false;
    static bool s_mdnsLogged = false;
    static uint32_t s_lastListRequestMs = 0;
    static uint32_t s_lastNetworkDebugMs = 0;
    uint32_t nowMs = millis();

    // Debug network status every 5 seconds
    if (nowMs - s_lastNetworkDebugMs > 5000) {
        s_lastNetworkDebugMs = nowMs;
        #ifdef ENABLE_VERBOSE_DEBUG
        Serial.printf("[NETWORK DEBUG] WiFi:%d mDNS:%d(%d/%d) manualIP:%d fallback:%d WS:%d(%s)\n",
                      g_wifiManager.isConnected() ? 1 : 0,
                      g_wifiManager.isMDNSResolved() ? 1 : 0,
                      g_wifiManager.getMDNSAttemptCount(),
                      NetworkConfig::MDNS_MAX_ATTEMPTS,
                      g_wifiManager.shouldUseManualIP() ? 1 : 0,
                      g_wifiManager.isMDNSTimeoutExceeded() ? 1 : 0,
                      g_wsClient.isConnected() ? 1 : 0,
                      g_wsClient.getStatusString());
        #endif // ENABLE_VERBOSE_DEBUG
    }

    if (g_wifiManager.isConnected()) {
        // Log WiFi connection once
        if (!s_wifiWasConnected) {
            s_wifiWasConnected = true;
            char localIpStr[16];
            formatIPv4(g_wifiManager.getLocalIP(), localIpStr);
            Serial.printf("[NETWORK] WiFi connected! IP: %s\n", localIpStr);
        }

        // Multi-tier fallback strategy:
        // Priority 1: If we are on the v2 SoftAP subnet (192.168.4.0/24), connect to the AP gateway
        // Priority 2: Manual IP from NVS (if configured and enabled)
        // Priority 3: mDNS resolution (with timeout fallback)
        // Priority 4: Timeout-based fallback (default fallback IP)

        // Priority 1: v2 SoftAP subnet (gateway 192.168.4.1)
        if (!g_wsConfigured &&
            WiFi.gatewayIP()[0] == 192 && WiFi.gatewayIP()[1] == 168 && WiFi.gatewayIP()[2] == 4) {
            IPAddress serverIP;
#ifdef LIGHTWAVE_IP
            if (!serverIP.fromString(LIGHTWAVE_IP)) {
                Serial.printf("[NETWORK] Invalid LIGHTWAVE_IP: %s\n", LIGHTWAVE_IP);
            } else {
                g_wsConfigured = true;
                char ipStr[16];
                formatIPv4(serverIP, ipStr);
                Serial.printf("[NETWORK] On SoftAP subnet (gw=%s) - using AP IP: %s\n",
                              WiFi.gatewayIP().toString().c_str(), ipStr);
                Serial.printf("[NETWORK] Connecting WebSocket to %s:%d%s\n",
                              ipStr,
                              LIGHTWAVE_PORT,
                              LIGHTWAVE_WS_PATH);
                g_wsClient.begin(serverIP, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH);
            }
#else
            // Fallback if LIGHTWAVE_IP macro is not defined
            serverIP = IPAddress(192, 168, 4, 1);
            g_wsConfigured = true;
            Serial.printf("[NETWORK] On SoftAP subnet - using default AP IP: 192.168.4.1\n");
            g_wsClient.begin(serverIP, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH);
#endif
        }

        // Priority 2: Manual IP from NVS (if configured and enabled)
        if (!g_wsConfigured && g_wifiManager.shouldUseManualIP()) {
            IPAddress manualIP = g_wifiManager.getManualIP();
            if (manualIP != INADDR_NONE) {
                char ipStr[16];
                formatIPv4(manualIP, ipStr);
                Serial.printf("[NETWORK] Using manual IP: %s\n", ipStr);
                g_wsConfigured = true;
                g_wsClient.begin(manualIP, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH);
            }
        }
        
        // Priority 3: mDNS resolution (with timeout fallback)
        if (!g_wsConfigured) {
            // Check if mDNS timeout exceeded OR resolved (not both AND - fixes broken logic)
            if (g_wifiManager.isMDNSTimeoutExceeded() || 
                g_wifiManager.isMDNSResolved()) {
                IPAddress fallbackIP = g_wifiManager.getResolvedIP();
                
                // If timeout exceeded but no resolved IP, use configured fallback
                if (fallbackIP == INADDR_NONE && g_wifiManager.isMDNSTimeoutExceeded()) {
                    IPAddress configuredIP;
                    IPAddress localIP = WiFi.localIP();
                    bool useConfigured = configuredIP.fromString(NetworkConfig::MDNS_FALLBACK_IP_PRIMARY);

                    // IMPORTANT: Check if configured fallback IP is the local IP (common mistake)
                    if (useConfigured && configuredIP == localIP) {
                        Serial.printf("[NETWORK] WARNING: Configured fallback IP %s is THIS DEVICE's IP!\n",
                                      NetworkConfig::MDNS_FALLBACK_IP_PRIMARY);
                        Serial.printf("[NETWORK] To find v2 IP: check v2 serial output or router DHCP list.\n");
                        useConfigured = false;
                    }

                    if (useConfigured) {
                        fallbackIP = configuredIP;
                        Serial.printf("[NETWORK] mDNS timeout, using configured fallback IP: %s\n",
                                      NetworkConfig::MDNS_FALLBACK_IP_PRIMARY);
                    } else {
                        Serial.printf("[NETWORK] ERROR: mDNS failed and no valid fallback IP!\n");
                        Serial.printf("[NETWORK] Update MDNS_FALLBACK_IP_PRIMARY in network_config.h\n");
                    }
                }
                
                if (fallbackIP != INADDR_NONE) {
                    char ipStr[16];
                    formatIPv4(fallbackIP, ipStr);
                    Serial.printf("[NETWORK] Using fallback IP: %s\n", ipStr);
                    g_wsConfigured = true;
                    g_wsClient.begin(fallbackIP, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH);
                }
            } else {
                // Normal mDNS resolution attempt
                if (!g_wifiManager.isMDNSResolved()) {
                    TAB5_AGENT_PRINTF(
                        "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"MDNS2\",\"location\":\"main.cpp\",\"message\":\"mdns.resolve.attempt\",\"data\":{\"hostname\":\"lightwaveos\"},\"timestamp\":%lu}\n",
                        (unsigned long)millis()
                    );
                    g_wifiManager.resolveMDNS("lightwaveos");
                }
                
                // Once mDNS resolved, configure WebSocket (ONCE)
                if (g_wifiManager.isMDNSResolved() && !g_wsConfigured) {
                    g_wsConfigured = true;
                    IPAddress serverIP = g_wifiManager.getResolvedIP();
                    char ipStr[16];
                    formatIPv4(serverIP, ipStr);

                    if (!s_mdnsLogged) {
                        s_mdnsLogged = true;
                        Serial.printf("[NETWORK] mDNS resolved: lightwaveos.local -> %s\n", ipStr);
                    }

                    Serial.printf("[NETWORK] Connecting WebSocket to %s:%d%s\n",
                                  ipStr,
                                  LIGHTWAVE_PORT,
                                  LIGHTWAVE_WS_PATH);
                    TAB5_AGENT_PRINTF(
                        "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS3\",\"location\":\"main.cpp\",\"message\":\"ws.begin.fromMdns\",\"data\":{\"ip\":\"%s\",\"port\":%d,\"path\":\"%s\"},\"timestamp\":%lu}\n",
                        ipStr, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH, (unsigned long)millis()
                    );

                    g_wsClient.begin(serverIP, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH);
                }
            }
        }

        // Detect WS connection edges for logging only.
        // Keep this independent from LED/UI state so we don't spam logs during reconnect.
        const bool isWsConnected = g_wsClient.isConnected();
        if (s_wsConnectedEdgeState && !isWsConnected) {
            Serial.printf("[NETWORK] WebSocket disconnected, library will auto-reconnect (state=%s, delay=%lu ms)\n",
                          g_wsClient.getStatusString(),
                          g_wsClient.getReconnectDelay());
        } else if (!s_wsConnectedEdgeState && isWsConnected) {
            Serial.println("[NETWORK] WebSocket reconnected");
        }
        s_wsConnectedEdgeState = isWsConnected;

        // Once WebSocket is connected, request effect/palette name lists (paged, non-blocking)
        if (g_wsClient.isConnected()) {
            uint32_t nowMs = millis();

            if (!s_requestedLists) {
                // Start paging from 1
                s_effectNextPage = 1;
                s_paletteNextPage = 1;
                s_effectPages = 0;
                s_palettePages = 0;
                s_requestedLists = true;
                s_lastListRequestMs = 0;
            }

            // Rate-limit list requests to avoid spamming the server
            if (nowMs - s_lastListRequestMs >= 250) {
                // Request palettes first (75 palettes -> 4 pages @ 20/page)
                if (s_palettePages == 0 || s_paletteNextPage <= s_palettePages) {
                    g_wsClient.requestPalettesList(s_paletteNextPage, 20, "tab5.palettes");
                    s_lastListRequestMs = nowMs;
                }
                // Then effects (count may vary; server caps 50 per page; Tab5 requests smaller pages)
                else if (s_effectPages == 0 || s_effectNextPage <= s_effectPages) {
                    g_wsClient.requestEffectsList(s_effectNextPage, 20, "tab5.effects");
                    s_lastListRequestMs = nowMs;
                }
            }
        } else {
            // Not connected yet - allow fresh list fetch after reconnect
            s_requestedLists = false;
        }
    } else {
        // WiFi disconnected - reset state for reconnection
        if (s_wifiWasConnected) {
            s_wifiWasConnected = false;
            s_mdnsLogged = false;
            g_wsConfigured = false;
            s_requestedLists = false;
            s_wsConnectedEdgeState = false;
            Serial.println("[NETWORK] WiFi disconnected");
        }
    }
#endif // ENABLE_WIFI

    // =========================================================================
    // I2C RECOVERY: Update recovery state machine (Phase G.2)
    // =========================================================================
    // Non-blocking - advances one step per call when recovering
    // Safe to call every loop iteration
    I2CRecovery::update();

    // After recovery completes, attempt to reinitialize encoder transports
    static bool s_wasRecovering = false;
    bool isRecovering = I2CRecovery::isRecovering();

    if (s_wasRecovering && !isRecovering) {
        // Recovery just completed - wait for I2C bus to settle before reinit
        Serial.println("[I2C_RECOVERY] Recovery complete - waiting for I2C bus to settle...");

        // Allow I2C bus to fully settle before reinit (prevents failed reinit after recovery)
        esp_task_wdt_reset();
        delay(50);
        esp_task_wdt_reset();

        if (g_encoders) {
            bool unitAOk = false;
            bool unitBOk = false;

            // Retry reinit up to 3 times with delays (increases success rate after recovery)
            for (uint8_t attempt = 0; attempt < 3 && (!unitAOk || !unitBOk); attempt++) {
                if (!unitAOk) unitAOk = g_encoders->transportA().reinit();
                if (!unitBOk) unitBOk = g_encoders->transportB().reinit();

                if (!unitAOk || !unitBOk) {
                    Serial.printf("[I2C_RECOVERY] Reinit attempt %d: A=%s B=%s\n",
                                  attempt + 1, unitAOk ? "OK" : "FAIL", unitBOk ? "OK" : "FAIL");
                    if (attempt < 2) {  // Don't delay after last attempt
                        delay(100);
                        esp_task_wdt_reset();
                    }
                }
            }

            Serial.printf("[I2C_RECOVERY] Final: Unit A=%s, Unit B=%s\n",
                          unitAOk ? "OK" : "FAIL", unitBOk ? "OK" : "FAIL");

            // Update status LEDs
            updateConnectionLeds();
        }
    }
    s_wasRecovering = isRecovering;

    // =========================================================================
    // ENCODER RE-PROBE: Periodically check for missing/reconnected encoders
    // =========================================================================
    // Bridges the gap where I2CRecovery can't trigger because no I2C errors
    // are generated when encoders were never detected (_available == false).
    static uint32_t s_lastReprobeMs = 0;
    constexpr uint32_t REPROBE_INTERVAL_MS = 10000;  // Every 10 seconds

    if (g_encoders && !g_encoders->areBothAvailable() && !isRecovering) {
        uint32_t nowReprobe = millis();
        if (nowReprobe - s_lastReprobeMs >= REPROBE_INTERVAL_MS) {
            s_lastReprobeMs = nowReprobe;
            bool changed = false;

            if (!g_encoders->isUnitAAvailable()) {
                bool ok = g_encoders->transportA().reinit();
                if (ok) {
                    Serial.printf("[REPROBE] Unit A (0x%02X) reconnected!\n", ADDR_UNIT_A);
                    changed = true;
                }
            }
            if (!g_encoders->isUnitBAvailable()) {
                bool ok = g_encoders->transportB().reinit();
                if (ok) {
                    Serial.printf("[REPROBE] Unit B (0x%02X) reconnected!\n", ADDR_UNIT_B);
                    changed = true;
                }
            }

            if (changed) {
                updateConnectionLeds();
            } else {
                Serial.println("[REPROBE] Encoders still unavailable");
            }
        }
    }

    // =========================================================================
    // ENCODERS: Skip processing if service not available + reboot timer
    // =========================================================================
    static uint32_t s_noEncoderSince = 0;
    constexpr uint32_t ENCODER_REBOOT_TIMEOUT_MS = 120000;  // 2 minutes

    if (!g_encoders || !g_encoders->isAnyAvailable()) {
        // Start or continue the reboot timer
        if (s_noEncoderSince == 0) {
            s_noEncoderSince = millis();
            Serial.println("[ENCODER] All encoders unavailable - starting 2-minute reboot timer");
        }

        uint32_t elapsed = millis() - s_noEncoderSince;
        if (elapsed >= ENCODER_REBOOT_TIMEOUT_MS) {
            Serial.println("[ENCODER] No encoders for 2 minutes - rebooting!");
            Serial.flush();
            delay(100);
            esp_restart();
        }

        // Log progress every 30 seconds
        static uint32_t s_lastRebootLog = 0;
        if (millis() - s_lastRebootLog >= 30000) {
            s_lastRebootLog = millis();
            Serial.printf("[ENCODER] Reboot in %lu seconds\n",
                          (unsigned long)((ENCODER_REBOOT_TIMEOUT_MS - elapsed) / 1000));
        }

        esp_task_wdt_reset();
        delay(100);
        return;
    }

    // Reset reboot timer when encoders are available
    s_noEncoderSince = 0;

    // =========================================================================
    // ENCODERS: Never touch I2C devices while recovery is running
    // =========================================================================
    // Prevents collisions between the recovery state machine (Wire.end/begin, SCL toggling)
    // and normal I2C traffic, which can otherwise trigger ESP_ERR_INVALID_STATE.
    if (isRecovering) {
        esp_task_wdt_reset();  // CRITICAL: Prevent watchdog timeout during I2C recovery
        return;
    }

    // Reset watchdog before encoder update (critical path)
    esp_task_wdt_reset();

    // Reset watchdog before encoder update (critical path)
    esp_task_wdt_reset();

    // Update encoder service (polls all 16 encoders, handles debounce, fires callbacks)
    // The callback (onEncoderChange) handles display updates with highlighting
    g_encoders->update();
    
    // Reset watchdog after encoder update
    esp_task_wdt_reset();
    
    // Reset watchdog after encoder update
    esp_task_wdt_reset();

    // =========================================================================
    // PRESETS: Process Unit-B button click patterns (Main Dashboard only)
    // =========================================================================
    // When in Main Dashboard mode, Unit-B buttons (8-15) act as 8 preset banks:
    //   - SINGLE_CLICK: Recall preset
    //   - DOUBLE_CLICK: Save current state to preset
    //   - LONG_HOLD: Delete preset
    // When in Zone Composer mode, buttons retain their zone control functions.
    if (g_presetManager && g_ui && s_uiInitialized && g_ui->getCurrentScreen() == UIScreen::GLOBAL) {
        uint32_t now = millis();

        // Poll Unit-B button states and run through click detectors
        if (g_encoders->isUnitBAvailable()) {
            for (uint8_t slot = 0; slot < 8; slot++) {
                bool isPressed = g_encoders->transportB().getKeyPressed(slot);
                ClickType click = g_clickDetectors[slot].update(isPressed, now);

                if (click != ClickType::NONE) {
                    switch (click) {
                        case ClickType::SINGLE_CLICK:
                            // Recall preset from this slot
                            if (g_presetManager->isSlotOccupied(slot)) {
                                g_presetManager->recallPreset(slot);
                            } else {
                                // Slot is empty - flash red to indicate
                                g_encoders->flashLed(8 + slot, 255, 64, 0);
                                Serial.printf("[PRESET] Slot %d is empty\n", slot);
                            }
                            break;

                        case ClickType::DOUBLE_CLICK:
                            // Save current state to this slot
                            g_presetManager->savePreset(slot);
                            break;

                        case ClickType::LONG_HOLD:
                            // Delete preset from this slot
                            if (g_presetManager->isSlotOccupied(slot)) {
                                g_presetManager->deletePreset(slot);
                            }
                            break;

                        default:
                            break;
                    }
                }
            }
        }
    }

    // =========================================================================
    // NVS: Process pending parameter saves (debounced writes)
    // =========================================================================
    NvsStorage::update();

    // =========================================================================
    // UI: Update system monitor animation and connection status
    // =========================================================================
    if (g_ui && s_uiInitialized) {  // Only update if UI is fully initialized
        // Sync connection state to display
        bool wifiOk = false;
        bool wsOk = false;
        bool unitA = false;
        bool unitB = false;
        
#if ENABLE_WIFI
        wifiOk = g_wifiManager.isConnected();
#endif
        wsOk = g_wsClient.isConnected();
        
        if (g_encoders) {
            unitA = g_encoders->isUnitAAvailable();
            unitB = g_encoders->isUnitBAvailable();
        }
        
        g_ui->setConnectionState(wifiOk, wsOk, unitA, unitB);

        // Update WebSocket status for footer display
#if ENABLE_WIFI
        g_ui->updateWebSocketStatus(g_wsClient.getStatus());
#endif

        // Update WiFi details for header display
#if ENABLE_WIFI
        if (wifiOk) {
            // Avoid per-frame heap churn (e.g. IPAddress::toString() + String copies).
            static uint32_t s_lastWiFiInfoMs = 0;
            static char s_ipBuf[48] = {0};  // Expanded for "IP > targeting v2" display
            static char s_ssidBuf[33] = {0};  // 32 + NUL (802.11 SSID max)
            static int32_t s_rssi = 0;

            const uint32_t now = millis();
            if (now - s_lastWiFiInfoMs >= 2000 || s_ipBuf[0] == '\0') {
                s_lastWiFiInfoMs = now;

                const IPAddress ip = g_wifiManager.getLocalIP();
                char ipOnly[16];
                formatIPv4(ip, ipOnly);

                if (g_wsConfigured) {
                    snprintf(s_ipBuf, sizeof(s_ipBuf), "%s", ipOnly);
                } else {
                    snprintf(s_ipBuf, sizeof(s_ipBuf), "%s > targeting v2", ipOnly);
                }

                const String ssid = g_wifiManager.getSSID();
                ssid.toCharArray(s_ssidBuf, sizeof(s_ssidBuf));

                s_rssi = g_wifiManager.getRSSI();
            }

            g_ui->setWiFiInfo(s_ipBuf, s_ssidBuf, s_rssi);
        } else {
            g_ui->setWiFiInfo("", "", 0);
        }
        
        // Update retry button visibility
        #if ENABLE_WIFI
        g_ui->updateRetryButton(g_wifiManager.shouldShowRetryButton());
        #else
        g_ui->updateRetryButton(false);
        #endif
#else
        g_ui->setWiFiInfo("", "", 0);
        g_ui->updateRetryButton(false);
#endif

        // Ensure Effect/Palette labels stay updated (names best-effort)
        updateUiEffectPaletteLabels();

        // Animate system monitor waveform
        g_ui->loop();
        esp_task_wdt_reset();  // Reset after UI loop (can involve display updates)
    }

    // =========================================================================
    // PERIODIC STATUS: Every 10 seconds (now includes network status)
    // =========================================================================
    static uint32_t lastStatus = 0;
    uint32_t now = millis();

    if (now - lastStatus >= 10000) {
        lastStatus = now;

        bool unitA = g_encoders->isUnitAAvailable();
        bool unitB = g_encoders->isUnitBAvailable();

        // Network status
        const char* wifiStatus = g_wifiManager.isConnected() ? "OK" : "DISC";
        const char* wsStatus = g_wsClient.isConnected() ? "OK" :
                              (g_wsClient.isConnecting() ? "CONN" : "DISC");

        // NVS pending saves
        uint8_t nvsPending = NvsStorage::getPendingCount();

        // I2C recovery stats
        uint8_t i2cErrors = I2CRecovery::getErrorCount();
        uint16_t i2cRecoveries = I2CRecovery::getRecoverySuccesses();

        Serial.printf("[STATUS] A:%s B:%s WiFi:%s WS:%s wsConn:%lu wsDisc:%lu wsErr:%lu wsReconn:%lu wsDupDisc:%lu wsDelay:%lu NVS:%d I2C_err:%d I2C_rec:%d heap:%u\n",
                      unitA ? "OK" : "FAIL",
                      unitB ? "OK" : "FAIL",
                      wifiStatus,
                      wsStatus,
                      static_cast<unsigned long>(g_wsClient.getConnectedCount()),
                      static_cast<unsigned long>(g_wsClient.getDisconnectCount()),
                      static_cast<unsigned long>(g_wsClient.getErrorCount()),
                      static_cast<unsigned long>(g_wsClient.getReconnectAttemptCount()),
                      static_cast<unsigned long>(g_wsClient.getDuplicateDisconnectCount()),
                      static_cast<unsigned long>(g_wsClient.getReconnectDelay()),
                      nvsPending,
                      i2cErrors,
                      i2cRecoveries,
                      ESP.getFreeHeap());

        // Update status LEDs in case connection state changed
        updateConnectionLeds();
    }

    // Reset watchdog at end of loop iteration
    esp_task_wdt_reset();

    delay(5);  // ~200Hz polling
}
