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
#include <cstring>
#include "parameters/ParameterHandler.h"
#include "parameters/ParameterMap.h"
#include "storage/NvsStorage.h"
#include "ui/LedFeedback.h"
#include "ui/DisplayUI.h"

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

// WebSocket connection state
bool g_wsConfigured = false;  // true after wsClient.begin() called

// Connection status LED feedback (Phase F.5)
LedFeedback g_ledFeedback;

// Touch screen handler (Phase G.3)
TouchHandler g_touchHandler;

// Cyberpunk UI (Phase H)
DisplayUI* g_ui = nullptr;

// Button handler for zone mode and speed/palette toggles
ButtonHandler* g_buttonHandler = nullptr;

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

static const char* lookupEffectName(uint8_t id) {
    if (id < MAX_EFFECTS && s_effectKnown[id] && s_effectNames[id][0]) return s_effectNames[id];
    return nullptr;
}

static const char* lookupPaletteName(uint8_t id) {
    if (id < MAX_PALETTES && s_paletteKnown[id] && s_paletteNames[id][0]) return s_paletteNames[id];
    return nullptr;
}

static void updateUiEffectPaletteLabels() {
    if (!g_ui || !g_encoders) return;
    uint8_t effectId = static_cast<uint8_t>(g_encoders->getValue(0));
    uint8_t paletteId = static_cast<uint8_t>(g_encoders->getValue(2));

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

/**
 * Called when any encoder value changes
 * @param index Encoder index (0-15)
 * @param value New parameter value
 * @param wasReset true if this was a button-press reset to default
 */
void onEncoderChange(uint8_t index, uint16_t value, bool wasReset) {
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
    if (g_ui) {
        g_ui->updateValue(index, value);
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
                case 1: g_wsClient.sendBrightnessChange(value); break;
                case 2: g_wsClient.sendPaletteChange(value); break;
                case 3: g_wsClient.sendSpeedChange(value); break;
                case 4: g_wsClient.sendMoodChange(value); break;
                case 5: g_wsClient.sendFadeAmountChange(value); break;
                case 6: g_wsClient.sendComplexityChange(value); break;
                case 7: g_wsClient.sendVariationChange(value); break;
            }
        }
        // Unit B (8-15): Zone parameters
        else {
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
        s_wasWsConnected = false;  // Reset WS tracking
    }
    else if (g_wsClient.isConnected()) {
        // Fully connected
        state = ConnectionState::WS_CONNECTED;
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
    Serial.println("[WIFI] Configuring Tab5 SDIO pins for ESP32-C6 co-processor...");
    WiFi.setPins(TAB5_WIFI_SDIO_CLK, TAB5_WIFI_SDIO_CMD,
                 TAB5_WIFI_SDIO_D0, TAB5_WIFI_SDIO_D1,
                 TAB5_WIFI_SDIO_D2, TAB5_WIFI_SDIO_D3,
                 TAB5_WIFI_SDIO_RST);
    Serial.println("[WIFI] SDIO pins configured");
#endif

    // Initialize M5Stack Tab5
    auto cfg = M5.config();
    cfg.external_spk = true;
    M5.begin(cfg);

    // Set display orientation (landscape, USB on left)
    M5.Display.setRotation(3);

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
    Serial.println("\n[NVS] Initializing parameter storage...");
    if (!NvsStorage::init()) {
        Serial.println("[NVS] WARNING: NVS init failed - parameters will not persist");
    }

    // Scan external I2C bus for devices
    uint8_t found = scanI2CBus(Wire, "External I2C (Grove Port.A)");

    // Initialize DualEncoderService with both addresses
    // Unit A @ 0x42 (reprogrammed), Unit B @ 0x41 (factory)
    g_encoders = new DualEncoderService(&Wire, ADDR_UNIT_A, ADDR_UNIT_B);
    g_encoders->setChangeCallback(onEncoderChange);
    bool encoderOk = g_encoders->begin();

    // Initialize ButtonHandler for zone mode and speed/palette toggles
    g_buttonHandler = new ButtonHandler();
    g_buttonHandler->setWebSocketClient(&g_wsClient);
    
    // Set up callbacks
    g_buttonHandler->onZoneModeToggle([](bool enabled) {
        Serial.printf("[Button] Zone mode toggled: %s\n", enabled ? "ON" : "OFF");
        // Switch UI screen when zone mode is enabled
        if (g_ui) {
            g_ui->setScreen(enabled ? UIScreen::ZONE_COMPOSER : UIScreen::GLOBAL);
        }
    });
    
    g_buttonHandler->onSpeedPaletteToggle([](uint8_t zoneId, SpeedPaletteMode mode) {
        const char* modeName = (mode == SpeedPaletteMode::SPEED) ? "SPEED" : "PALETTE";
        Serial.printf("[Button] Zone %u encoder mode: %s\n", zoneId, modeName);
        // TODO: Update UI to show current mode
    });
    
    // Connect ButtonHandler to encoder service
    g_encoders->setButtonHandler(g_buttonHandler);
    Serial.println("[Button] Button handler initialized");

    // =========================================================================
    // Initialize LED Feedback (Phase F.5)
    // =========================================================================
    g_ledFeedback.setEncoders(g_encoders);
    g_ledFeedback.begin();
    Serial.println("[LED] Connection status LED feedback initialized");

    // =========================================================================
    // Load Saved Parameters from NVS (Phase G.1)
    // =========================================================================
    if (NvsStorage::isReady()) {
        uint16_t savedValues[16];
        uint8_t loadedCount = NvsStorage::loadAllParameters(savedValues);

        // Apply loaded values to encoder service (without triggering callbacks)
        for (uint8_t i = 0; i < 16; i++) {
            g_encoders->setValue(i, savedValues[i], false);
        }

        if (loadedCount > 0) {
            Serial.printf("[NVS] Restored %d parameters from flash\n", loadedCount);
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

        // Initialize Cyberpunk UI (Phase H)
        g_ui = new DisplayUI(M5.Display);
        g_ui->begin();
        g_ui->setConnectionState(false, false, unitA, unitB);

        // Show initial values on radial gauges
        for (uint8_t i = 0; i < 16; i++) {
            g_ui->updateValue(i, g_encoders->getValue(i));
        }

    } else if (unitA || unitB) {
        // Partial success - one unit available
        Serial.println("\n[WARN] Only one unit detected - 8 encoders available");
        Serial.println("[WARN] Check wiring for missing unit");

        // Set status LEDs (green for available, red for missing)
        updateConnectionLeds();

        // Initialize Cyberpunk UI (Phase H)
        g_ui = new DisplayUI(M5.Display);
        g_ui->begin();
        g_ui->setConnectionState(false, false, unitA, unitB);

        // Show initial values on radial gauges
        for (uint8_t i = 0; i < 16; i++) {
            g_ui->updateValue(i, g_encoders->getValue(i));
        }

    } else {
        Serial.println("\n[ERROR] No encoder units found!");
        Serial.println("[ERROR] Check wiring:");
        Serial.println("  - Is Unit A (0x42) connected to Grove Port.A?");
        Serial.println("  - Is Unit B (0x41) connected to Grove Port.A?");
        Serial.println("  - Are the Grove cables properly seated?");

        // Initialize Cyberpunk UI even without encoders (shows system status)
        g_ui = new DisplayUI(M5.Display);
        g_ui->begin();
        g_ui->setConnectionState(false, false, false, false);
    }

    // =========================================================================
    // Initialize Network (Milestone F)
    // =========================================================================
#if ENABLE_WIFI
    Serial.println("\n[NETWORK] Initializing WiFi...");

    // Initialize ParameterHandler (bridges encoders ↔ WebSocket ↔ display)
    g_paramHandler = new ParameterHandler(g_encoders, &g_wsClient);
    g_paramHandler->setButtonHandler(g_buttonHandler);
    g_paramHandler->setDisplayCallback([](uint8_t index, uint16_t value) {
        // Called when parameters are updated from WebSocket
        // Update radial gauge display (fast, non-blocking)
        if (g_ui) {
            // #region agent log
            if (index == 15 || index == 3 || index == 0) {
                FILE* f = fopen("/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log", "a");
                if (f) {
                    fprintf(f,
                            "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"main.cpp:displayCallback\",\"message\":\"ws_display_update\",\"data\":{\"index\":%u,\"value\":%u},\"timestamp\":%lu}\n",
                            (unsigned)index,
                            (unsigned)value,
                            (unsigned long)millis());
                    fclose(f);
                }
            }
            // #endregion
            g_ui->updateValue(index, value);
        }
    });

    // Initialize WsMessageRouter (routes incoming WebSocket messages)
    // Pass ZoneComposerUI if available (for zone state updates)
    ZoneComposerUI* zoneUI = (g_ui) ? g_ui->getZoneComposerUI() : nullptr;
    if (zoneUI) {
        zoneUI->setWebSocketClient(&g_wsClient);
    }
    WsMessageRouter::init(g_paramHandler, &g_wsClient, zoneUI);

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
                    if (id < MAX_EFFECTS && name) {
                        strncpy(s_effectNames[id], name, EFFECT_NAME_MAX - 1);
                        s_effectNames[id][EFFECT_NAME_MAX - 1] = '\0';
                        s_effectKnown[id] = true;
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
                if (pagination["total"].is<uint16_t>()) {
                    uint16_t total = pagination["total"].as<uint16_t>();
                    if (total > 0) {
                        uint8_t paletteMax = (total > 1) ? (total - 1) : 0;  // 0-indexed, clamp to 0
                        updateParameterMetadata(2, 0, paletteMax);  // PaletteId is index 2
                        Serial.printf("[ParamMap] Updated Palette max from palettes.list: %u (total=%u)\n", paletteMax, total);
                    }
                } else if (data["total"].is<uint16_t>()) {
                    // Some API versions put total at data level
                    uint16_t total = data["total"].as<uint16_t>();
                    if (total > 0) {
                        uint8_t paletteMax = (total > 1) ? (total - 1) : 0;
                        updateParameterMetadata(2, 0, paletteMax);
                        Serial.printf("[ParamMap] Updated Palette max from palettes.list: %u (total=%u)\n", paletteMax, total);
                    }
                }

                updateUiEffectPaletteLabels();
                return;
            }
        }

        WsMessageRouter::route(doc);
    });

    // Start WiFi connection
    g_wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[NETWORK] Connecting to '%s'...\n", WIFI_SSID);
#else
    // WiFi disabled on ESP32-P4 due to SDIO pin configuration issues
    // See Config.h ENABLE_WIFI flag for details
    Serial.println("\n[NETWORK] WiFi DISABLED - ESP32-P4 SDIO pin config not supported");
    Serial.println("[NETWORK] Encoder functionality available, network sync disabled");
#endif // ENABLE_WIFI

    // =========================================================================
    // Initialize Touch Handler (Phase G.3)
    // =========================================================================
    Serial.println("\n[TOUCH] Initializing touch screen handler...");
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
    
    // Register status bar touch callback for Zone Composer screen
    g_touchHandler.onStatusBarTouch([](int16_t x, int16_t y) {
        // Forward to DisplayUI for Zone Composer touch handling
        if (g_ui && g_ui->getCurrentScreen() == UIScreen::ZONE_COMPOSER) {
            g_ui->handleTouch(x, y);
        }
    });

    Serial.println("[TOUCH] Touch handler initialized - long press to reset params");

    Serial.println("\n============================================");
    Serial.println("  Setup complete - turn encoders to test");
    Serial.println("  WiFi connecting in background...");
    Serial.println("  Touch screen: long press to reset params");
    Serial.println("============================================\n");
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
    
    // Forward touch to Zone Composer UI if on that screen
    if (g_ui && g_ui->getCurrentScreen() == UIScreen::ZONE_COMPOSER) {
        auto touch = M5.Touch.getDetail();
        if (touch.isPressed()) {
            g_ui->handleTouch(touch.x, touch.y);
        }
    }

    // =========================================================================
    // NETWORK: Service WebSocket EARLY to prevent TCP timeouts (K1 pattern)
    // =========================================================================
    g_wsClient.update();

    // =========================================================================
    // NETWORK: Update WiFi state machine
    // =========================================================================
    g_wifiManager.update();

    // =========================================================================
    // LED FEEDBACK: Update connection status LEDs (Phase F.5)
    // =========================================================================
    updateConnectionLeds();
    g_ledFeedback.update();  // Non-blocking breathing animation

    // =========================================================================
    // NETWORK: Handle mDNS resolution and WebSocket connection
    // =========================================================================
#if ENABLE_WIFI
    static bool s_wifiWasConnected = false;
    static bool s_mdnsLogged = false;
    static uint32_t s_lastListRequestMs = 0;

    if (g_wifiManager.isConnected()) {
        // Log WiFi connection once
        if (!s_wifiWasConnected) {
            s_wifiWasConnected = true;
            Serial.printf("[NETWORK] WiFi connected! IP: %s\n",
                          g_wifiManager.getLocalIP().toString().c_str());
        }

        // Try mDNS resolution (with internal backoff)
        if (!g_wifiManager.isMDNSResolved()) {
            g_wifiManager.resolveMDNS("lightwaveos");
        }

        // Once mDNS resolved, configure WebSocket (ONCE)
        if (g_wifiManager.isMDNSResolved() && !g_wsConfigured) {
            g_wsConfigured = true;
            IPAddress serverIP = g_wifiManager.getResolvedIP();

            if (!s_mdnsLogged) {
                s_mdnsLogged = true;
                Serial.printf("[NETWORK] mDNS resolved: lightwaveos.local -> %s\n",
                              serverIP.toString().c_str());
            }

            Serial.printf("[NETWORK] Connecting WebSocket to %s:%d%s\n",
                          serverIP.toString().c_str(),
                          LIGHTWAVE_PORT,
                          LIGHTWAVE_WS_PATH);

            g_wsClient.begin(serverIP, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH);
        }

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
                // Request palettes first (small, deterministic: 64 -> 2 pages)
                if (s_palettePages == 0 || s_paletteNextPage <= s_palettePages) {
                    g_wsClient.requestPalettesList(s_paletteNextPage, 50, "tab5.palettes");
                    s_lastListRequestMs = nowMs;
                }
                // Then effects (count may vary; server caps 50 per page)
                else if (s_effectPages == 0 || s_effectNextPage <= s_effectPages) {
                    g_wsClient.requestEffectsList(s_effectNextPage, 50, "tab5.effects");
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
        Serial.println("[I2C_RECOVERY] Recovery complete - reinitializing encoders...");

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
    // NVS: Process pending parameter saves (debounced writes)
    // =========================================================================
    NvsStorage::update();

    // =========================================================================
    // UI: Update system monitor animation and connection status
    // =========================================================================
    if (g_ui) {
        // Sync connection state to display
        bool wifiOk = g_wifiManager.isConnected();
        bool wsOk = g_wsClient.isConnected();
        bool unitA = g_encoders->isUnitAAvailable();
        bool unitB = g_encoders->isUnitBAvailable();
        g_ui->setConnectionState(wifiOk, wsOk, unitA, unitB);

        // Update WiFi details for header display
#if ENABLE_WIFI
        if (wifiOk) {
            IPAddress ip = g_wifiManager.getLocalIP();
            String ssid = g_wifiManager.getSSID();
            int32_t rssi = g_wifiManager.getRSSI();
            g_ui->setWiFiInfo(ip.toString().c_str(), ssid.c_str(), rssi);
        } else {
            g_ui->setWiFiInfo("", "", 0);
        }
#else
        g_ui->setWiFiInfo("", "", 0);
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
