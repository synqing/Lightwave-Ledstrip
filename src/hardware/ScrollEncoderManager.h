#ifndef SCROLL_ENCODER_MANAGER_H
#define SCROLL_ENCODER_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "M5UnitScroll.h"
#include "../config/hardware_config.h"
#include "../core/EffectTypes.h"

// Scroll encoder parameters that can be controlled
enum ScrollParameter : uint8_t {
    PARAM_EFFECT = 0,       // Effect selection
    PARAM_BRIGHTNESS = 1,   // Global brightness
    PARAM_PALETTE = 2,      // Color palette
    PARAM_SPEED = 3,        // Animation speed
    PARAM_INTENSITY = 4,    // Effect intensity
    PARAM_SATURATION = 5,   // Color saturation
    PARAM_COMPLEXITY = 6,   // Pattern complexity
    PARAM_VARIATION = 7,    // Effect variation
    PARAM_COUNT = 8
};

// Parameter colors for LED feedback
extern const uint32_t PARAM_COLORS[PARAM_COUNT];

// Parameter names for display
extern const char* PARAM_NAMES[PARAM_COUNT];

// Performance metrics for scroll encoder
struct ScrollMetrics {
    // Event tracking
    uint32_t totalReads = 0;
    uint32_t successfulReads = 0;
    uint32_t failedReads = 0;
    uint32_t buttonPresses = 0;
    
    // Timing metrics
    uint32_t totalResponseTime = 0;
    uint32_t maxResponseTime = 0;
    uint32_t minResponseTime = UINT32_MAX;
    float avgResponseTime = 0;
    
    // Health metrics
    uint32_t connectionLosses = 0;
    uint32_t recoveries = 0;
    uint32_t i2cErrors = 0;
    
    // Reporting
    uint32_t lastReportTime = 0;
    static constexpr uint32_t REPORT_INTERVAL = 30000; // 30 seconds
    
    void recordRead(bool success, uint32_t responseTime);
    void recordButtonPress() { buttonPresses++; }
    void recordConnectionLoss() { connectionLosses++; }
    void recordRecovery() { recoveries++; }
    void recordI2CError() { i2cErrors++; }
    void printReport();
    void reset();
};

// Acceleration system for smooth control
class ScrollAcceleration {
private:
    uint32_t lastChangeTime = 0;
    int16_t lastDelta = 0;
    float acceleration = 1.0f;
    
    // Sensitivity profiles per parameter
    struct SensitivityProfile {
        float baseMultiplier;
        float maxAcceleration;
        float accelerationRate;
        uint32_t accelerationWindow;
    };
    
    SensitivityProfile profiles[PARAM_COUNT] = {
        {1.0f, 1.0f, 0.0f, 0},     // PARAM_EFFECT - no acceleration
        {0.3f, 2.0f, 0.1f, 500},   // PARAM_BRIGHTNESS - gentle ramping
        {1.0f, 1.0f, 0.0f, 0},     // PARAM_PALETTE - no acceleration
        {0.4f, 2.5f, 0.15f, 400},  // PARAM_SPEED - moderate changes
        {0.3f, 2.0f, 0.1f, 500},   // PARAM_INTENSITY - gentle for LGP
        {0.3f, 2.0f, 0.1f, 500},   // PARAM_SATURATION - gentle for LGP
        {0.3f, 2.0f, 0.1f, 500},   // PARAM_COMPLEXITY - gentle for LGP
        {0.3f, 2.0f, 0.1f, 500}    // PARAM_VARIATION - gentle for LGP
    };
    
public:
    int16_t processValue(int16_t rawDelta, ScrollParameter param);
    void reset();
};

// Enhanced LED animation system
class ScrollLEDAnimator {
private:
    struct Animation {
        uint32_t startTime;
        uint32_t duration;
        uint8_t startR, startG, startB;
        uint8_t targetR, targetG, targetB;
        bool active;
        
        uint32_t getCurrentColor(uint32_t now);
    };
    
    Animation currentAnim;
    uint32_t baseColor;
    float pulsePhase = 0;
    
public:
    void startTransition(uint32_t fromColor, uint32_t toColor, uint32_t duration);
    void flashColor(uint32_t color, uint32_t duration);
    uint32_t getCurrentColor();
    void setBaseColor(uint32_t color) { baseColor = color; }
};

// Scroll encoder state
struct ScrollState {
    int32_t value = 0;
    int32_t lastValue = 0;
    bool buttonPressed = false;
    uint32_t lastButtonPress = 0;
    uint32_t lastUpdate = 0;
    ScrollParameter currentParam = PARAM_EFFECT;
    
    // Parameter values (normalized 0-255)
    // Effect, Brightness, Palette, Speed, Intensity, Saturation, Complexity, Variation
    uint8_t paramValues[PARAM_COUNT] = {0, 96, 0, 128, 128, 128, 128, 128};
};

// Forward declaration
class ScrollEncoderManager;

// Panic mode actions
enum PanicAction : uint8_t {
    PANIC_FULL_RESET,      // Complete system reset
    PANIC_RESTORE_DEFAULTS, // Restore all parameters to defaults
    PANIC_BYPASS_ENCODER,   // Bypass encoder, use defaults
    PANIC_DIAGNOSTIC_MODE   // Enter diagnostic mode
};

// Panic mode handler
class PanicMode {
private:
    bool isPanicMode = false;
    uint32_t panicStartTime = 0;
    uint32_t consecutiveFailures = 0;
    uint32_t panicButtonPressStart = 0;
    bool panicButtonPressed = false;
    
    static constexpr uint32_t PANIC_THRESHOLD = 10;
    static constexpr uint32_t PANIC_TIMEOUT = 30000; // 30 seconds
    static constexpr uint32_t PANIC_BUTTON_DURATION = 10000; // 10 seconds
    
    ScrollEncoderManager* manager = nullptr;
    
public:
    void init(ScrollEncoderManager* mgr) { manager = mgr; }
    bool isActive() const { return isPanicMode; }
    void incrementFailures() { consecutiveFailures++; }
    void resetFailures() { consecutiveFailures = 0; }
    uint32_t getFailureCount() const { return consecutiveFailures; }
    
    void checkPanicConditions(uint32_t lastSuccessfulRead, uint32_t watchdogTime);
    void checkManualTrigger(bool buttonPressed);
    void enterPanicMode(PanicAction action);
    void executePanicAction(PanicAction action);
    void forceBusReset();
    void runDiagnostics();
    bool exitPanicMode();
    uint8_t getDefaultValue(ScrollParameter param);
    void applyDefaultParameters();
};

// Scroll encoder manager class
class ScrollEncoderManager {
    friend class PanicMode;  // Allow panic mode access to private members
    
private:
    M5UnitScroll* scrollEncoder = nullptr;
    bool isAvailable = false;
    ScrollState state;
    
    // Performance and health monitoring
    ScrollMetrics metrics;
    ScrollAcceleration acceleration;
    ScrollLEDAnimator ledAnimator;
    PanicMode panicMode;
    
    // Health monitoring
    uint32_t lastSuccessfulRead = 0;
    uint32_t consecutiveErrors = 0;
    bool needsRecovery = false;
    uint32_t lastRecoveryAttempt = 0;
    static constexpr uint32_t RECOVERY_INTERVAL = 5000; // 5 seconds
    static constexpr uint32_t HEALTH_TIMEOUT = 3000;    // 3 seconds - increased for I2C bus contention
    static constexpr uint32_t MAX_ERRORS = 10;          // Increased tolerance for I2C conflicts
    
    // Watchdog system
    struct Watchdog {
        uint32_t lastFeedTime = 0;
        uint32_t timeout = 5000;  // 5 seconds - increased for I2C bus sharing
        bool triggered = false;
        
        void feed() { 
            lastFeedTime = millis(); 
            triggered = false;
        }
        
        bool check() {
            if (millis() - lastFeedTime > timeout && !triggered) {
                triggered = true;
                return true;
            }
            return false;
        }
        
        uint32_t getTimeSinceLastFeed() {
            return millis() - lastFeedTime;
        }
    } watchdog;
    
    // Recovery state machine
    enum RecoveryState {
        RECOVERY_IDLE,
        RECOVERY_BUS_RESET,
        RECOVERY_REINIT,
        RECOVERY_VERIFY,
        RECOVERY_FAILED
    } recoveryState = RECOVERY_IDLE;
    
    // Callbacks
    void (*onParamChange)(ScrollParameter param, uint8_t value) = nullptr;
    void (*onEffectChange)(uint8_t effect) = nullptr;
    
    // LED animation
    uint32_t ledAnimPhase = 0;
    
public:
    ScrollEncoderManager();
    ~ScrollEncoderManager();
    
    // Initialize the scroll encoder
    bool begin();
    
    // Process encoder input (call from main loop)
    void update();
    
    // Check if available
    bool available() const { return isAvailable; }
    
    // Get current parameter
    ScrollParameter getCurrentParam() const { return state.currentParam; }
    
    // Set current parameter
    void setCurrentParam(ScrollParameter param) {
        if (param < PARAM_COUNT) {
            state.currentParam = param;
            updateLED();  // Update LED to show new parameter color
            Serial.printf("Scroll encoder switched to %s mode\n", PARAM_NAMES[param]);
        }
    }
    
    // Get parameter value
    uint8_t getParamValue(ScrollParameter param) const {
        return state.paramValues[param];
    }
    
    // Set parameter value
    void setParamValue(ScrollParameter param, uint8_t value) {
        if (param < PARAM_COUNT) {
            state.paramValues[param] = value;
        }
    }
    
    // Set callbacks
    void setParamChangeCallback(void (*callback)(ScrollParameter, uint8_t)) {
        onParamChange = callback;
    }
    
    void setEffectChangeCallback(void (*callback)(uint8_t)) {
        onEffectChange = callback;
    }
    
    // Get visual params for effects
    void updateVisualParams(VisualParams& params);
    
    // Health and metrics
    bool isHealthy() const { return isAvailable && !needsRecovery; }
    uint32_t getLastSuccessTime() const { return lastSuccessfulRead; }
    const ScrollMetrics& getMetrics() const { return metrics; }
    void printMetrics() { metrics.printReport(); }
    
    // Parameter persistence
    bool saveParameters();
    bool loadParameters();
    
private:
    // Update LED color based on current parameter
    void updateLED();
    
    // Handle parameter value changes
    void handleValueChange(int32_t delta);
    
    // Cycle through parameters
    void nextParameter();
    
    // Recovery methods
    void performI2CBusRecovery();
    bool attemptReconnection();
    void resetI2CBus();
    
    // Health monitoring
    void checkConnectionHealth(uint32_t now);
    void updateHealthStatus(bool success, uint32_t responseTime);
};

// Global instance
extern ScrollEncoderManager scrollManager;

#endif // SCROLL_ENCODER_MANAGER_H