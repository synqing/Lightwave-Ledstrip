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
#include "network/WiFiManager.h"
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
#include "ui/PaletteLedDisplay.h"
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
// I2C Addresses for Dual Unit Setup
// ============================================================================

constexpr uint8_t ADDR_UNIT_A = 0x42;  // Reprogrammed via register 0xFF
constexpr uint8_t ADDR_UNIT_B = 0x41;  // Factory default

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

// Connection status LED feedback (Phase F.5)
LedFeedback g_ledFeedback;

// Palette color display on Unit B LEDs 0-7
PaletteLedDisplay g_paletteLedDisplay;

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
static constexpr uint8_t MAX_PALETTES = 64;  // Tab5 encoder range is 0-63

static char s_effectNames[MAX_EFFECTS][EFFECT_NAME_MAX] = {{0}};
static bool s_effectKnown[MAX_EFFECTS] = {false};
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
    
    // #region agent log
    Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H2,H5\",\"location\":\"main.cpp:200\",\"message\":\"handleActionButton.entry\",\"data\":{\"buttonIndex\":%d,\"wsConnected\":%d,\"wsStatus\":%d},\"timestamp\":%lu}\n",
                  buttonIndex, g_wsClient.isConnected() ? 1 : 0, (int)g_wsClient.getStatus(), (unsigned long)now);
    // #endregion
    
    // Get current state (use defaults if not valid)
    ColorCorrectionState cc = g_wsClient.getColorCorrectionState();
    
    // #region agent log
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
        
        // #region agent log
        Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"main.cpp:269\",\"message\":\"handleActionButton.stateAfter\",\"data\":{\"buttonIndex\":%d,\"gammaEnabled\":%d,\"gammaValue\":%.1f,\"aeEnabled\":%d,\"aeTarget\":%d,\"brownEnabled\":%d,\"mode\":%d},\"timestamp\":%lu}\n",
                      buttonIndex, cc.gammaEnabled ? 1 : 0, cc.gammaValue, cc.autoExposureEnabled ? 1 : 0,
                      cc.autoExposureTarget, cc.brownGuardrailEnabled ? 1 : 0, cc.mode, (unsigned long)millis());
        // #endregion
        
        // Try to send command to server (if connected)
        if (g_wsClient.isConnected()) {
            // #region agent log
            Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H2\",\"location\":\"main.cpp:277\",\"message\":\"handleActionButton.sendingCommand\",\"data\":{\"buttonIndex\":%d,\"commandType\":\"%s\"},\"timestamp\":%lu}\n",
                          buttonIndex, 
                          buttonIndex == 0 ? "setConfig" : (buttonIndex == 1 ? "setMode" : (buttonIndex == 2 ? "setConfig" : "setConfig")),
                          (unsigned long)millis());
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
            // #region agent log
            Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"main.cpp:304\",\"message\":\"handleActionButton.wsNotConnected\",\"data\":{\"buttonIndex\":%d,\"wsStatus\":%d},\"timestamp\":%lu}\n",
                          buttonIndex, (int)g_wsClient.getStatus(), (unsigned long)millis());
            // #endregion
            Serial.println("[TOUCH] WS not connected - UI updated optimistically, command will sync when connected");
        }
    }
}

const char* lookupEffectName(uint8_t id) {
    // Note: id is uint8_t (0-255), MAX_EFFECTS is 256, so id < MAX_EFFECTS is always true
    // But we also check id <= 87 to enforce actual effect range
    if (id <= 87 && s_effectKnown[id] && s_effectNames[id][0]) return s_effectNames[id];
    return nullptr;
}

const char* lookupPaletteName(uint8_t id) {
    if (id < MAX_PALETTES && s_paletteKnown[id] && s_paletteNames[id][0]) return s_paletteNames[id];
    return nullptr;
}

static void updateUiEffectPaletteLabels() {
    if (!g_ui || !g_encoders || !s_uiInitialized) return;  // Only update if UI is initialized
    uint8_t effectId = static_cast<uint8_t>(g_encoders->getValue(0));
    uint8_t paletteId = static_cast<uint8_t>(g_encoders->getValue(1));  // FIXED: Palette is encoder 1, not 2

    char effectBuf[EFFECT_NAME_MAX];
    char paletteBuf[PALETTE_NAME_MAX];

    const char* en = lookupEffectName(effectId);
    const char* pn = lookupPaletteName(paletteId);

    if (en) {
        strncpy(effectBuf, en, sizeof(effectBuf) - 1);
        effectBuf[sizeof(effectBuf) - 1] = '\0';
    } else {
        snprintf(effectBuf, sizeof(effectBuf), "#%u", (unsigned)effectId);
    }

    if (pn) {
        strncpy(paletteBuf, pn, sizeof(paletteBuf) - 1);
        paletteBuf[sizeof(paletteBuf) - 1] = '\0';
    } else {
        snprintf(paletteBuf, sizeof(paletteBuf), "#%u", (unsigned)paletteId);
    }

    g_ui->setCurrentEffect(effectId, effectBuf);
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

    if (strcmp(cmd, "help") == 0) {
        Serial.println("[HELP] Commands:");
        Serial.println("  ppa on | ppa off | ppa toggle | ppa bench [N]");
        Serial.println("  net status | net sta | net ap");
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

// Track previous value for encoder 15 (ENC-B encoder 7) for animation mode cycling
static uint16_t s_prevEncoder15Value = 0;
static bool s_encoder15Initialized = false;

/**
 * Called when any encoder value changes
 * @param index Encoder index (0-15)
 * @param value New parameter value
 * @param wasReset true if this was a button-press reset to default
 */
void onEncoderChange(uint8_t index, uint16_t value, bool wasReset) {
    // Special handling for encoder 15 (ENC-B encoder 7): Cycle palette animation modes
    if (index == 15) {
        if (!s_encoder15Initialized) {
            // First time - just store the value, don't cycle
            s_prevEncoder15Value = value;
            s_encoder15Initialized = true;
            return;
        }
        
        // Determine direction of change (handle wrap-around)
        int16_t delta = static_cast<int16_t>(value) - static_cast<int16_t>(s_prevEncoder15Value);
        
        // Handle wrap-around: if delta is very large, it wrapped the other direction
        if (delta > 128) {
            delta -= 256;  // Wrapped forward, treat as backward
        } else if (delta < -128) {
            delta += 256;  // Wrapped backward, treat as forward
        }
        
        // Only cycle if there was actual movement (ignore tiny jitter)
        if (abs(delta) >= 2) {
            AnimationMode currentMode = g_paletteLedDisplay.getAnimationMode();
            uint8_t currentModeIndex = static_cast<uint8_t>(currentMode);
            
            // Cycle based on direction
            const uint8_t modeCount = static_cast<uint8_t>(AnimationMode::MODE_COUNT);
            if (delta > 0) {
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
        
        // Update previous value
        s_prevEncoder15Value = value;
        return;  // Don't process as normal parameter
    }
    
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
        if (wasReset) {
            Serial.printf("[%s:%d] %s reset to %d\n", unit, localIdx, name, value);
        } else {
            Serial.printf("[%s:%d] %s: → %d\n", unit, localIdx, name, value);
        }
        s_lastEncoderLogTime = now;
    }

    // Update display with new value (fast, non-blocking)
    if (g_ui && s_uiInitialized) {  // Only update if UI is initialized
        g_ui->updateValue(index, value);
    }

    // Update palette LED display when palette parameter changes (index 1)
    if (index == 1) {  // Palette parameter
        // #region agent log
        Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"main.cpp:625\",\"message\":\"onEncoderChange.paletteChange\",\"data\":{\"paletteId\":%u,\"timestamp\":%lu}\n",
            value, static_cast<unsigned long>(millis()));
        // #endregion
        g_paletteLedDisplay.update(static_cast<uint8_t>(value));
    }

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
            // #region agent log
            Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"main.cpp:648\",\"message\":\"onEncoderChange.encB\",\"data\":{\"index\":%u,\"value\":%u,\"unit\":\"B\",\"localIdx\":%u,\"timestamp\":%lu}\n",
                index, value, index % 8, static_cast<unsigned long>(millis()));
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
    // Initialize serial first for early logging
    Serial.begin(115200);
    delay(100);

    Serial.println("\n");
    Serial.println("============================================");
    Serial.println("  Tab5.encoder - Milestone F");
    Serial.println("  Dual M5ROTATE8 (16 Encoders) + WiFi");
    Serial.println("============================================");

    // =========================================================================
    // CRITICAL: Configure Tab5 WiFi SDIO pins BEFORE any WiFi initialization
    // =========================================================================
    // Tab5 uses ESP32-C6 WiFi co-processor via SDIO on non-default pins.
    // This MUST be called before M5.begin() or WiFi.begin().
    // See: https://github.com/nikthefix/M5stack_Tab5_Arduino_Wifi_Example
#if ENABLE_WIFI
    // #region agent log
    Serial.printf("[DEBUG] Before WiFi.setPins - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    Serial.printf("[DEBUG] WiFi pins: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d RST=%d\n",
                  TAB5_WIFI_SDIO_CLK, TAB5_WIFI_SDIO_CMD, TAB5_WIFI_SDIO_D0,
                  TAB5_WIFI_SDIO_D1, TAB5_WIFI_SDIO_D2, TAB5_WIFI_SDIO_D3, TAB5_WIFI_SDIO_RST);
    // #endregion
    Serial.println("[WIFI] Configuring Tab5 SDIO pins for ESP32-C6 co-processor...");
    WiFi.setPins(TAB5_WIFI_SDIO_CLK, TAB5_WIFI_SDIO_CMD,
                 TAB5_WIFI_SDIO_D0, TAB5_WIFI_SDIO_D1,
                 TAB5_WIFI_SDIO_D2, TAB5_WIFI_SDIO_D3,
                 TAB5_WIFI_SDIO_RST);
    // #region agent log
    Serial.printf("[DEBUG] After WiFi.setPins - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    delay(50);  // Allow SDIO pin configuration to stabilize
    Serial.printf("[DEBUG] After 50ms delay - Heap: free=%u minFree=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // #endregion
    Serial.println("[WIFI] SDIO pins configured");
#endif

    // Initialize M5Stack Tab5
    // #region agent log
#if ENABLE_WIFI
    Serial.printf("[DEBUG] Before M5.begin - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
#endif
    // #endregion
    auto cfg = M5.config();
    cfg.external_spk = true;
    M5.begin(cfg);
    // #region agent log
#if ENABLE_WIFI
    Serial.printf("[DEBUG] After M5.begin - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    delay(100);  // Allow M5 initialization to complete
    Serial.printf("[DEBUG] After 100ms delay - Heap: free=%u minFree=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
#endif
    // #endregion

    // Set display orientation (landscape, USB on left)
    M5.Display.setRotation(3);
    
    // #region agent log
    Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"main.cpp:688\",\"message\":\"before.setSwapBytes\",\"data\":{\"rotation\":3},\"timestamp\":%lu}\n", (unsigned long)millis());
    // #endregion
    
    // CRITICAL: Set byte swapping for BGR565 format BEFORE LVGL initialization
    // This must match the working implementation in src/src/main.cpp:352
    M5.Display.setSwapBytes(true);  // Swap bytes for BGR565 format
    delay(50);  // Allow display configuration to stabilize (matches working impl)
    
    // #region agent log
    Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"main.cpp:696\",\"message\":\"after.setSwapBytes\",\"data\":{\"swapBytes\":true,\"delay\":50},\"timestamp\":%lu}\n", (unsigned long)millis());
    Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"main.cpp:699\",\"message\":\"display.dimensions\",\"data\":{\"width\":%d,\"height\":%d},\"timestamp\":%lu}\n", 
                  M5.Display.width(), M5.Display.height(), (unsigned long)millis());
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
    bool encoderOk = g_encoders->begin();

    // Initialize ButtonHandler (handles Unit-A button resets)
    // NOTE: Unit-B buttons (8-15) are now reserved for Preset System.
    //       Zone mode control has been moved to the webapp.
    g_buttonHandler = new ButtonHandler();
    g_buttonHandler->setWebSocketClient(&g_wsClient);

    // Connect ButtonHandler to encoder service
    g_encoders->setButtonHandler(g_buttonHandler);
    Serial.println("[Button] Button handler initialized (presets on Unit-B)");

    // =========================================================================
    // Initialize LED Feedback (Phase F.5)
    // =========================================================================
    g_ledFeedback.setEncoders(g_encoders);
    g_ledFeedback.begin();
    Serial.println("[LED] Connection status LED feedback initialized");

    // =========================================================================
    // Initialize Palette LED Display
    // =========================================================================
    g_paletteLedDisplay.setEncoders(g_encoders);
    g_paletteLedDisplay.begin();
    Serial.println("[LED] Palette LED display initialized");

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
            
            // Update palette LED display with restored palette value
            uint8_t paletteId = static_cast<uint8_t>(savedValues[1]);  // Index 1 = Palette
            g_paletteLedDisplay.update(paletteId);
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
        g_encoders->transportA().setAllLEDs(0, 64, 0);
        g_encoders->transportB().setAllLEDs(0, 64, 0);
        delay(200);
        g_encoders->allLedsOff();

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
                for (JsonObject e : effects) {
                    if (!e["id"].is<uint8_t>() || !e["name"].is<const char*>()) continue;
                    uint8_t id = e["id"].as<uint8_t>();
                    const char* name = e["name"].as<const char*>();
                    // CRITICAL FIX: Stricter bounds check - enforce actual effect range (0-87)
                    if (id < MAX_EFFECTS && id <= 87 && name) {
                        strncpy(s_effectNames[id], name, EFFECT_NAME_MAX - 1);
                        s_effectNames[id][EFFECT_NAME_MAX - 1] = '\0';
                        s_effectKnown[id] = true;
                    } else if (id >= MAX_EFFECTS) {
                        Serial.printf("[WARN] Invalid effect ID %u (max=%u), ignoring\n", id, MAX_EFFECTS - 1);
                    }
                }

                JsonObject pagination = data["pagination"].as<JsonObject>();
                if (pagination["pages"].is<uint8_t>()) {
                    s_effectPages = pagination["pages"].as<uint8_t>();
                }
                if (pagination["page"].is<uint8_t>()) {
                    uint8_t page = pagination["page"].as<uint8_t>();
                    if (page >= s_effectNextPage) s_effectNextPage = page + 1;
                }

                // Extract total effect count and update ParameterMap metadata
                // Effect max = total - 1 (0-indexed)
                if (pagination["total"].is<uint16_t>()) {
                    uint16_t total = pagination["total"].as<uint16_t>();
                    if (total > 0) {
                        uint8_t effectMax = (total > 1) ? (total - 1) : 0;  // 0-indexed, clamp to 0
                        updateParameterMetadata(0, 0, effectMax);  // EffectId is index 0
                        Serial.printf("[ParamMap] Updated Effect max from effects.list: %u (total=%u)\n", effectMax, total);
                    }
                } else if (data["total"].is<uint16_t>()) {
                    // Some API versions put total at data level
                    uint16_t total = data["total"].as<uint16_t>();
                    if (total > 0) {
                        uint8_t effectMax = (total > 1) ? (total - 1) : 0;
                        updateParameterMetadata(0, 0, effectMax);
                        Serial.printf("[ParamMap] Updated Effect max from effects.list: %u (total=%u)\n", effectMax, total);
                    }
                }

                // Also update zone effect max values (indices 8, 10, 12, 14)
                uint8_t zoneEffectMax = getParameterMax(0);  // Use same max as global effect
                for (uint8_t i = 8; i <= 14; i += 2) {
                    updateParameterMetadata(i, 0, zoneEffectMax);
                }

                updateUiEffectPaletteLabels();
                return;
            }

            if (type && strcmp(type, "palettes.list") == 0 && doc["success"].is<bool>() && doc["success"].as<bool>()) {
                JsonObject data = doc["data"].as<JsonObject>();
                JsonArray palettes = data["palettes"].as<JsonArray>();
                for (JsonObject p : palettes) {
                    if (!p["id"].is<uint8_t>() || !p["name"].is<const char*>()) continue;
                    uint8_t id = p["id"].as<uint8_t>();
                    const char* name = p["name"].as<const char*>();
                    if (id < MAX_PALETTES && name) {
                        strncpy(s_paletteNames[id], name, PALETTE_NAME_MAX - 1);
                        s_paletteNames[id][PALETTE_NAME_MAX - 1] = '\0';
                        s_paletteKnown[id] = true;
                    }
                }

                JsonObject pagination = data["pagination"].as<JsonObject>();
                if (pagination["pages"].is<uint8_t>()) {
                    s_palettePages = pagination["pages"].as<uint8_t>();
                }
                if (pagination["page"].is<uint8_t>()) {
                    uint8_t page = pagination["page"].as<uint8_t>();
                    if (page >= s_paletteNextPage) s_paletteNextPage = page + 1;
                }

                // Extract total palette count and update ParameterMap metadata
                // Palette max = total - 1 (0-indexed)
                // #region agent log
                Serial.printf("[DEBUG] Before palette metadata update - Heap: free=%u minFree=%u largest=%u\n",
                              ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
                // #endregion
                if (pagination["total"].is<uint16_t>()) {
                    uint16_t total = pagination["total"].as<uint16_t>();
                    if (total > 0) {
                        uint8_t paletteMax = (total > 1) ? (total - 1) : 0;  // 0-indexed, clamp to 0
                        updateParameterMetadata(1, 0, paletteMax);  // PaletteId is now index 1
                        Serial.printf("[ParamMap] Updated Palette max from palettes.list: %u (total=%u)\n", paletteMax, total);
                    }
                } else if (data["total"].is<uint16_t>()) {
                    // Some API versions put total at data level
                    uint16_t total = data["total"].as<uint16_t>();
                    if (total > 0) {
                        uint8_t paletteMax = (total > 1) ? (total - 1) : 0;  // 0-indexed, clamp to 0
                        updateParameterMetadata(1, 0, paletteMax);  // PaletteId is now index 1
                        Serial.printf("[ParamMap] Updated Palette max from palettes.list: %u (total=%u)\n", paletteMax, total);
                    }
                }
                // #region agent log
                Serial.printf("[DEBUG] After palette metadata update - Heap: free=%u minFree=%u largest=%u\n",
                              ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
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
                    syncColourCorrectionUi();
                    Serial.println("[WS] Color correction config synced");
                }
                return;
            }

            // CRITICAL FIX: Handle colorCorrection.setConfig response (confirmation)
            if (type && strcmp(type, "colorCorrection.setConfig") == 0) {
                // #region agent log
                bool hasSuccess = doc["success"].is<bool>();
                bool success = hasSuccess && doc["success"].as<bool>();
                Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H4\",\"location\":\"main.cpp:1202\",\"message\":\"response.colorCorrection.setConfig\",\"data\":{\"hasSuccess\":%d,\"success\":%d},\"timestamp\":%lu}\n",
                              hasSuccess ? 1 : 0, success ? 1 : 0, (unsigned long)millis());
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
                    syncColourCorrectionUi();
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
                // TODO: Could flash LED or show error on display
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

                // Update palette LED display if palette parameter is in status message
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
                    g_paletteLedDisplay.update(paletteId);
                }
            }
        }

        WsMessageRouter::route(doc);
    });

    // Start WiFi connection
    // #region agent log
    Serial.printf("[DEBUG] Before WiFiManager::begin() - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // #endregion
    
    // Begin WiFi with primary and secondary networks
    g_wifiManager.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_SSID2, WIFI_PASSWORD2);
    
    // #region agent log
    Serial.printf("[DEBUG] After WiFiManager::begin() - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
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
    // Update M5Stack (handles button events, touch, etc.)
    M5.update();

    // =========================================================================
    // TOUCH: Process touch events (Phase G.3)
    // =========================================================================
    g_touchHandler.update();

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    LVGLBridge::update();
#endif

    // =========================================================================
    // SERIAL: Process simple command input
    // =========================================================================
    pollSerialCommands();

    // =========================================================================
    // NETWORK: Service WebSocket EARLY to prevent TCP timeouts (K1 pattern)
    // =========================================================================
    // #region agent log
#if ENABLE_WS_DIAGNOSTICS
    static uint32_t s_lastWsUpdateLog = 0;
    if ((uint32_t)(millis() - s_lastWsUpdateLog) >= 1000) {  // Log every 1s
        Serial.printf("[DEBUG] Before wsClient.update() - Heap: free=%u minFree=%u largest=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
        s_lastWsUpdateLog = millis();
    }
#endif
    // #endregion
    g_wsClient.update();
    // #region agent log
#if ENABLE_WS_DIAGNOSTICS
    if ((uint32_t)(millis() - s_lastWsUpdateLog) >= 1000) {
        Serial.printf("[DEBUG] After wsClient.update() - Heap: free=%u minFree=%u largest=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    }
#endif
    // #endregion

    // =========================================================================
    // NETWORK: Update WiFi state machine
    // =========================================================================
    g_wifiManager.update();

    // =========================================================================
    // LED FEEDBACK: Update connection status LEDs (Phase F.5)
    // =========================================================================
    updateConnectionLeds();

    // =========================================================================
    // PALETTE LED DISPLAY: Update animation (if enabled)
    // =========================================================================
    // #region agent log
    static uint32_t s_lastLoopLog = 0;
    uint32_t loopNow = millis();
    if (loopNow - s_lastLoopLog >= 1000) {  // Log every 1s to track call frequency
        Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"main.cpp:1503\",\"message\":\"loop.updateAnimation.call\",\"data\":{\"loopTime\":%lu},\"timestamp\":%lu}\n",
            static_cast<unsigned long>(loopNow), static_cast<unsigned long>(loopNow));
        s_lastLoopLog = loopNow;
    }
    // #endregion
    g_paletteLedDisplay.updateAnimation();
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
            // #region agent log
            Serial.printf("[DEBUG] WiFi connected, initialising UI - Heap: free=%u minFree=%u largest=%u\n",
                          ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
            // #endregion
            
            // Show UI initialization message before hiding loading screen
            bool unitA = g_encoders ? g_encoders->isUnitAAvailable() : false;
            bool unitB = g_encoders ? g_encoders->isUnitBAvailable() : false;
            LoadingScreen::update(M5.Display, "INITIALISING UI", unitA, unitB);
            delay(500); // Brief display of UI initialization message
            
            // Hide loading screen
            LoadingScreen::hide(M5.Display);
            
            // Initialize full UI
            g_ui->begin();
            // #region agent log
            Serial.printf("[DEBUG] After DisplayUI::begin() - Heap: free=%u minFree=%u largest=%u\n",
                          ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
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
        Serial.printf("[NETWORK DEBUG] WiFi connected: %d, mDNS resolved: %d, WS configured: %d, WS connected: %d, WS status: %s\n",
                      g_wifiManager.isConnected() ? 1 : 0,
                      g_wifiManager.isMDNSResolved() ? 1 : 0,
                      g_wsConfigured ? 1 : 0,
                      g_wsClient.isConnected() ? 1 : 0,
                      g_wsClient.getStatusString());
    }

    if (g_wifiManager.isConnected()) {
        // Log WiFi connection once
        if (!s_wifiWasConnected) {
            s_wifiWasConnected = true;
            char localIpStr[16];
            formatIPv4(g_wifiManager.getLocalIP(), localIpStr);
            Serial.printf("[NETWORK] WiFi connected! IP: %s\n", localIpStr);
        }

        // Direct IP mode (skip mDNS entirely)
#ifdef LIGHTWAVE_IP
        if (!g_wsConfigured) {
            g_wsConfigured = true;
            IPAddress serverIP;
            if (!serverIP.fromString(LIGHTWAVE_IP)) {
                Serial.printf("[NETWORK] Invalid LIGHTWAVE_IP: %s\n", LIGHTWAVE_IP);
            } else {
                char ipStr[16];
                formatIPv4(serverIP, ipStr);
                Serial.printf("[NETWORK] Connecting WebSocket to %s:%d%s\n",
                              ipStr,
                              LIGHTWAVE_PORT,
                              LIGHTWAVE_WS_PATH);
                g_wsClient.begin(serverIP, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH);
            }
        }
#else
        // Try mDNS resolution (with internal backoff)
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
#endif

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
                // Request palettes first (small, deterministic: 64 -> 4 pages @ 20/page)
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
        // Recovery just completed - try to reinit encoder transports
        Serial.println("[I2C_RECOVERY] Recovery complete - reinitialising encoders...");

        if (g_encoders) {
            // Try to reinit both transports
            bool unitAOk = g_encoders->transportA().reinit();
            bool unitBOk = g_encoders->transportB().reinit();

            Serial.printf("[I2C_RECOVERY] Post-recovery: Unit A=%s, Unit B=%s\n",
                          unitAOk ? "OK" : "FAIL",
                          unitBOk ? "OK" : "FAIL");

            // Update status LEDs
            updateConnectionLeds();
        }
    }
    s_wasRecovering = isRecovering;

    // =========================================================================
    // ENCODERS: Skip processing if service not available
    // =========================================================================
    if (!g_encoders || !g_encoders->isAnyAvailable()) {
        delay(100);
        return;
    }

    // =========================================================================
    // ENCODERS: Never touch I2C devices while recovery is running
    // =========================================================================
    // Prevents collisions between the recovery state machine (Wire.end/begin, SCL toggling)
    // and normal I2C traffic, which can otherwise trigger ESP_ERR_INVALID_STATE.
    if (isRecovering) {
        return;
    }

    // Update encoder service (polls all 16 encoders, handles debounce, fires callbacks)
    // The callback (onEncoderChange) handles display updates with highlighting
    g_encoders->update();

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
            static char s_ipBuf[16] = {0};
            static char s_ssidBuf[33] = {0};  // 32 + NUL (802.11 SSID max)
            static int32_t s_rssi = 0;

            const uint32_t now = millis();
            if (now - s_lastWiFiInfoMs >= 2000 || s_ipBuf[0] == '\0') {
                s_lastWiFiInfoMs = now;

                const IPAddress ip = g_wifiManager.getLocalIP();
                formatIPv4(ip, s_ipBuf);

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

        Serial.printf("[STATUS] A:%s B:%s WiFi:%s WS:%s NVS:%d I2C_err:%d I2C_rec:%d heap:%u\n",
                      unitA ? "OK" : "FAIL",
                      unitB ? "OK" : "FAIL",
                      wifiStatus,
                      wsStatus,
                      nvsPending,
                      i2cErrors,
                      i2cRecoveries,
                      ESP.getFreeHeap());

        // Update status LEDs in case connection state changed
        updateConnectionLeds();
    }

    delay(5);  // ~200Hz polling
}
