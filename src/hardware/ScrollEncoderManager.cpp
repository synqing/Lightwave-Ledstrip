#include "ScrollEncoderManager.h"
#include <FastLED.h>
#include <math.h>

// Global instance
ScrollEncoderManager scrollManager;

// Parameter colors for LED feedback
const uint32_t PARAM_COLORS[PARAM_COUNT] = {
    0xFF0000,  // Effect - Red
    0xFFFF00,  // Brightness - Yellow  
    0x00FF00,  // Palette - Green
    0x00FFFF,  // Speed - Cyan
    0x0080FF,  // Intensity - Light Blue
    0xFF00FF,  // Saturation - Magenta
    0xFF8000,  // Complexity - Orange
    0x8000FF   // Variation - Purple
};

// Parameter names for display
const char* PARAM_NAMES[PARAM_COUNT] = {
    "Effect", "Brightness", "Palette", "Speed",
    "Intensity", "Saturation", "Complexity", "Variation"
};

// External dependencies
extern uint8_t currentEffect;
extern const uint8_t NUM_EFFECTS;
extern uint8_t currentPaletteIndex;
// External palette count
extern const uint8_t gGradientPaletteCount;
// External transition function
extern void startAdvancedTransition(uint8_t targetEffect);
extern SemaphoreHandle_t i2cMutex;

// ============== METRICS IMPLEMENTATION ==============
void ScrollMetrics::recordRead(bool success, uint32_t responseTime) {
    totalReads++;
    if (success) {
        successfulReads++;
        totalResponseTime += responseTime;
        maxResponseTime = max(maxResponseTime, responseTime);
        minResponseTime = min(minResponseTime, responseTime);
        avgResponseTime = (float)totalResponseTime / successfulReads;
    } else {
        failedReads++;
    }
}

void ScrollMetrics::printReport() {
    if (totalReads == 0) return;
    
    float successRate = (float)successfulReads / totalReads * 100.0f;
    
    Serial.println("\n📊 Scroll Encoder Metrics Report");
    Serial.println("════════════════════════════════");
    Serial.printf("Total Reads: %u (%.1f%% success)\n", totalReads, successRate);
    Serial.printf("Response Time: avg %.1fµs (min %uµs, max %uµs)\n", 
                  avgResponseTime, minResponseTime, maxResponseTime);
    Serial.printf("Button Presses: %u\n", buttonPresses);
    Serial.printf("Connection Issues: %u losses, %u recoveries\n", 
                  connectionLosses, recoveries);
    Serial.printf("I2C Errors: %u\n", i2cErrors);
    Serial.println("════════════════════════════════\n");
}

void ScrollMetrics::reset() {
    totalReads = 0;
    successfulReads = 0;
    failedReads = 0;
    buttonPresses = 0;
    totalResponseTime = 0;
    maxResponseTime = 0;
    minResponseTime = UINT32_MAX;
    avgResponseTime = 0;
    connectionLosses = 0;
    recoveries = 0;
    i2cErrors = 0;
    lastReportTime = millis();
}

// ============== ACCELERATION IMPLEMENTATION ==============
int16_t ScrollAcceleration::processValue(int16_t rawDelta, ScrollParameter param) {
    uint32_t now = millis();
    uint32_t timeDelta = now - lastChangeTime;
    
    SensitivityProfile& profile = profiles[param];
    
    // Calculate acceleration based on rotation speed
    if (timeDelta < profile.accelerationWindow && 
        ((rawDelta > 0 && lastDelta > 0) || (rawDelta < 0 && lastDelta < 0))) {
        // Same direction, within acceleration window
        acceleration = min(acceleration + profile.accelerationRate, 
                         profile.maxAcceleration);
    } else {
        // Direction change or timeout - reset acceleration
        acceleration = 1.0f;
    }
    
    // Apply sensitivity and acceleration
    float finalMultiplier = profile.baseMultiplier * acceleration;
    int16_t processedDelta = round(rawDelta * finalMultiplier);
    
    // Additional smoothing for LGP effects - prevent single-tick jumps
    if (abs(processedDelta) == 1 && param >= PARAM_INTENSITY && param <= PARAM_VARIATION) {
        // For visual parameters, accumulate small changes
        static float accumulator = 0;
        accumulator += rawDelta * finalMultiplier;
        if (abs(accumulator) >= 1.0f) {
            processedDelta = (int16_t)accumulator;
            accumulator -= processedDelta;
        } else {
            processedDelta = 0;
        }
    }
    
    // Update state
    lastChangeTime = now;
    lastDelta = rawDelta;
    
    return processedDelta;
}

void ScrollAcceleration::reset() {
    acceleration = 1.0f;
    lastChangeTime = 0;
    lastDelta = 0;
}

// ============== LED ANIMATOR IMPLEMENTATION ==============
uint32_t ScrollLEDAnimator::Animation::getCurrentColor(uint32_t now) {
    if (!active || now > startTime + duration) {
        active = false;
        return (targetR << 16) | (targetG << 8) | targetB;
    }
    
    float progress = (float)(now - startTime) / duration;
    // Apply easing curve (ease-in-out cubic)
    progress = progress < 0.5f ? 4 * progress * progress * progress : 
               1 - pow(-2 * progress + 2, 3) / 2;
    
    auto lerp = [](uint8_t start, uint8_t end, float t) -> uint8_t {
        return start + (end - start) * t;
    };
    
    uint8_t r = lerp(startR, targetR, progress);
    uint8_t g = lerp(startG, targetG, progress);
    uint8_t b = lerp(startB, targetB, progress);
    
    return (r << 16) | (g << 8) | b;
}

void ScrollLEDAnimator::startTransition(uint32_t fromColor, uint32_t toColor, uint32_t duration) {
    uint32_t now = millis();
    currentAnim = {
        now, duration,
        (uint8_t)((fromColor >> 16) & 0xFF), 
        (uint8_t)((fromColor >> 8) & 0xFF), 
        (uint8_t)(fromColor & 0xFF),
        (uint8_t)((toColor >> 16) & 0xFF), 
        (uint8_t)((toColor >> 8) & 0xFF), 
        (uint8_t)(toColor & 0xFF),
        true
    };
    baseColor = toColor;
}

void ScrollLEDAnimator::flashColor(uint32_t color, uint32_t duration) {
    uint32_t currentColor = getCurrentColor();
    startTransition(color, currentColor, duration);
}

uint32_t ScrollLEDAnimator::getCurrentColor() {
    uint32_t now = millis();
    
    // Get animated color if active
    uint32_t color = currentAnim.active ? 
                    currentAnim.getCurrentColor(now) : baseColor;
    
    // Apply subtle pulse effect
    pulsePhase += 0.05f;
    float pulseMod = (sin(pulsePhase) + 1) * 0.1f + 0.9f; // 90-100% brightness
    
    uint8_t r = ((color >> 16) & 0xFF) * pulseMod;
    uint8_t g = ((color >> 8) & 0xFF) * pulseMod;
    uint8_t b = (color & 0xFF) * pulseMod;
    
    return (r << 16) | (g << 8) | b;
}

ScrollEncoderManager::ScrollEncoderManager() {
    scrollEncoder = nullptr;
    isAvailable = false;
    panicMode.init(this);
}

ScrollEncoderManager::~ScrollEncoderManager() {
    if (scrollEncoder) {
        delete scrollEncoder;
        scrollEncoder = nullptr;
    }
}

bool ScrollEncoderManager::begin() {
    Serial.println("\n🔌 Initializing M5Unit-Scroll...");
    
    // Create encoder instance
    scrollEncoder = new M5UnitScroll();
    if (!scrollEncoder) {
        Serial.println("❌ Failed to allocate memory for scroll encoder");
        return false;
    }
    
    // Initialize with correct pins for your wiring
    bool initSuccess = scrollEncoder->begin(
        &Wire,                              // Use main Wire instance
        HardwareConfig::M5UNIT_SCROLL_ADDR, // 0x40
        HardwareConfig::I2C_SDA_SCROLL,     // GPIO11 - your SDA wiring
        HardwareConfig::I2C_SCL_SCROLL,     // GPIO12 - your SCL wiring
        400000                              // 400kHz
    );
    
    // Add small delay after initialization to let device stabilize
    delay(50);
    
    if (initSuccess) {
        isAvailable = true;
        Serial.println("✅ M5Unit-Scroll connected successfully!");
        
        // Get firmware version
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint8_t version = scrollEncoder->getFirmwareVersion();
            Serial.printf("   Firmware: V%d\n", version);
            xSemaphoreGive(i2cMutex);
        }
        
        // Test LED with startup animation
        Serial.println("   Testing LED...");
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Rainbow startup sequence
            scrollEncoder->setLEDColor(0xFF0000); // Red
            xSemaphoreGive(i2cMutex);
            delay(200);
            
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                scrollEncoder->setLEDColor(0x00FF00); // Green
                xSemaphoreGive(i2cMutex);
            }
            delay(200);
            
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                scrollEncoder->setLEDColor(0x0000FF); // Blue
                xSemaphoreGive(i2cMutex);
            }
            delay(200);
        }
        
        // Reset encoder value
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            scrollEncoder->setEncoderValue(0);
            xSemaphoreGive(i2cMutex);
        }
        
        // Initialize state
        state.value = 0;
        state.lastValue = 0;
        state.currentParam = PARAM_EFFECT;
        
        // Load saved parameters
        loadParameters();
        
        // Initialize subsystems
        metrics.reset();
        acceleration.reset();
        ledAnimator.setBaseColor(PARAM_COLORS[state.currentParam]);
        watchdog.feed();
        
        // Set initial LED color
        updateLED();
        
        // Initialize last successful read time
        lastSuccessfulRead = millis();
        
        return true;
    } else {
        delete scrollEncoder;
        scrollEncoder = nullptr;
        isAvailable = false;
        
        Serial.println("\n❌ M5Unit-Scroll initialization failed!");
        Serial.println("   Possible issues:");
        Serial.printf("   1. Check wiring (SDA=GPIO%d, SCL=GPIO%d)\n", 
                      HardwareConfig::I2C_SDA_SCROLL, HardwareConfig::I2C_SCL_SCROLL);
        Serial.println("   2. Verify I2C address is 0x40");
        Serial.println("   3. Ensure device is powered");
        Serial.println("   4. Check for I2C conflicts with other devices");
        Serial.println("   5. Try power cycling the scroll encoder");
        
        // Perform I2C scan to help diagnose
        Serial.println("\n   Scanning I2C bus for diagnostics...");
        Wire.beginTransmission(0x40);
        uint8_t error = Wire.endTransmission();
        Serial.printf("   Device at 0x40: %s (error code: %d)\n", 
                      error == 0 ? "FOUND" : "NOT FOUND", error);
        
        return false;
    }
}

void ScrollEncoderManager::update() {
    if (!scrollEncoder) return;
    
    uint32_t now = millis();
    
    // Check panic conditions first
    panicMode.checkPanicConditions(lastSuccessfulRead, watchdog.getTimeSinceLastFeed());
    
    if (panicMode.isActive()) {
        // Skip normal update in panic mode
        return;
    }
    
    // Check connection health
    checkConnectionHealth(now);
    
    // Attempt recovery if needed
    if (needsRecovery && (now - lastRecoveryAttempt > RECOVERY_INTERVAL)) {
        if (attemptReconnection()) {
            Serial.println("✅ Scroll encoder recovered!");
            panicMode.resetFailures();
        } else {
            panicMode.incrementFailures();
        }
        lastRecoveryAttempt = now;
    }
    
    // Skip normal update if unhealthy
    if (!isAvailable) return;
    
    // Rate limit to 50Hz
    if (now - state.lastUpdate < 20) return;
    state.lastUpdate = now;
    
    // Measure response time
    uint32_t startTime = micros();
    
    // Thread-safe I2C access with health check
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        metrics.recordI2CError();
        return;
    }
    
    bool success = false;
    int16_t delta = 0;
    bool buttonPressed = false;
    
    // Check if device is still connected
    Wire.beginTransmission(HardwareConfig::M5UNIT_SCROLL_ADDR);
    if (Wire.endTransmission() == 0) {
        // Read encoder values
        delta = scrollEncoder->getIncEncoderValue();
        buttonPressed = scrollEncoder->getButtonStatus();
        success = true;
    } else {
        metrics.recordI2CError();
    }
    
    xSemaphoreGive(i2cMutex);
    
    // Calculate response time
    uint32_t responseTime = micros() - startTime;
    
    // Update health status
    updateHealthStatus(success, responseTime);
    
    if (!success) return;
    
    // Feed watchdog
    watchdog.feed();
    
    // Handle encoder rotation with acceleration
    if (delta != 0) {
        state.lastValue = state.value;
        state.value += delta;
        
        // Apply acceleration processing
        int16_t processedDelta = acceleration.processValue(delta, state.currentParam);
        handleValueChange(processedDelta);
        
        // Animate LED on change
        ledAnimator.flashColor(0xFFFFFF, 100); // White flash
    }
    
    // Check for manual panic trigger
    panicMode.checkManualTrigger(buttonPressed);
    
    // Handle button press (with debouncing)
    if (buttonPressed && !state.buttonPressed && (now - state.lastButtonPress > 200)) {
        state.buttonPressed = true;
        state.lastButtonPress = now;
        metrics.recordButtonPress();
        nextParameter();
        
        Serial.printf("Scroll: Switched to %s mode\n", PARAM_NAMES[state.currentParam]);
        
        // Animate parameter change
        ledAnimator.startTransition(ledAnimator.getCurrentColor(), 
                                  PARAM_COLORS[state.currentParam], 300);
        
        // Save parameters on mode change
        saveParameters();
    } else if (!buttonPressed && state.buttonPressed) {
        state.buttonPressed = false;
    }
    
    // Update LED
    updateLED();
    
    // Periodic metrics report
    if (now - metrics.lastReportTime > ScrollMetrics::REPORT_INTERVAL) {
        metrics.printReport();
        metrics.lastReportTime = now;
    }
}

void ScrollEncoderManager::handleValueChange(int32_t delta) {
    ScrollParameter param = state.currentParam;
    uint8_t oldValue = state.paramValues[param];
    int16_t newValue = oldValue;
    
    switch (param) {
        case PARAM_EFFECT:
            // Special handling for effect selection (no acceleration)
            if (delta > 0) {
                uint8_t newEffect = (currentEffect + 1) % NUM_EFFECTS;
                if (onEffectChange) {
                    onEffectChange(newEffect);
                } else {
                    startAdvancedTransition(newEffect);
                }
            } else if (delta < 0) {
                uint8_t newEffect = currentEffect > 0 ? currentEffect - 1 : NUM_EFFECTS - 1;
                if (onEffectChange) {
                    onEffectChange(newEffect);
                } else {
                    startAdvancedTransition(newEffect);
                }
            }
            return; // Don't update paramValues for effect
            
        case PARAM_BRIGHTNESS:
            // Fine control for brightness with acceleration
            // Limit delta to prevent large jumps
            newValue += constrain(delta, -4, 4);
            break;
            
        case PARAM_PALETTE:
            // Cycle through palettes (no acceleration)
            if (delta > 0) {
                newValue = (oldValue + 1) % gGradientPaletteCount;
            } else if (delta < 0) {
                newValue = oldValue > 0 ? oldValue - 1 : gGradientPaletteCount - 1;
            }
            break;
            
        default:
            // Standard increment/decrement with acceleration
            // Limit delta to prevent large jumps that distort LGP effects
            newValue += constrain(delta, -3, 3);
            break;
    }
    
    // Clamp to valid range
    newValue = constrain(newValue, 0, 255);
    state.paramValues[param] = newValue;
    
    // Trigger callback
    if (onParamChange && newValue != oldValue) {
        onParamChange(param, newValue);
    }
    
    // Debug output
    #if FEATURE_DEBUG_OUTPUT
    if (newValue != oldValue) {
        Serial.printf("Scroll: %s = %d\n", PARAM_NAMES[param], newValue);
    }
    #endif
}

void ScrollEncoderManager::nextParameter() {
    state.currentParam = (ScrollParameter)((state.currentParam + 1) % PARAM_COUNT);
    updateLED();
}

void ScrollEncoderManager::updateLED() {
    if (!isAvailable || !scrollEncoder) return;
    
    // Get animated color from LED animator
    uint32_t color = ledAnimator.getCurrentColor();
    
    // Thread-safe LED update
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        scrollEncoder->setLEDColor(color);
        xSemaphoreGive(i2cMutex);
    }
}

void ScrollEncoderManager::updateVisualParams(VisualParams& params) {
    params.intensity = state.paramValues[PARAM_INTENSITY];
    params.saturation = state.paramValues[PARAM_SATURATION];
    params.complexity = state.paramValues[PARAM_COMPLEXITY];
    params.variation = state.paramValues[PARAM_VARIATION];
}

// ============== RECOVERY IMPLEMENTATION ==============
void ScrollEncoderManager::performI2CBusRecovery() {
    Serial.println("[RECOVERY] Performing I2C bus recovery...");
    
    // Step 1: Release mutex if held
    if (xSemaphoreGetMutexHolder(i2cMutex) == xTaskGetCurrentTaskHandle()) {
        xSemaphoreGive(i2cMutex);
    }
    
    // Step 2: Generate clock pulses to release stuck bus
    pinMode(HardwareConfig::I2C_SCL_SCROLL, OUTPUT);
    pinMode(HardwareConfig::I2C_SDA_SCROLL, INPUT_PULLUP);
    
    // Clock out any stuck data
    for (int i = 0; i < 9; i++) {
        digitalWrite(HardwareConfig::I2C_SCL_SCROLL, HIGH);
        delayMicroseconds(5);
        digitalWrite(HardwareConfig::I2C_SCL_SCROLL, LOW);
        delayMicroseconds(5);
    }
    
    // Step 3: Generate STOP condition
    pinMode(HardwareConfig::I2C_SDA_SCROLL, OUTPUT);
    digitalWrite(HardwareConfig::I2C_SDA_SCROLL, LOW);
    delayMicroseconds(5);
    digitalWrite(HardwareConfig::I2C_SCL_SCROLL, HIGH);
    delayMicroseconds(5);
    digitalWrite(HardwareConfig::I2C_SDA_SCROLL, HIGH);
    delayMicroseconds(5);
    
    // Step 4: Reinitialize I2C
    Wire.end();
    delay(100);
    Wire.begin(HardwareConfig::I2C_SDA_SCROLL, HardwareConfig::I2C_SCL_SCROLL);
    Wire.setClock(400000);
    
    Serial.println("\u2705 I2C bus recovery complete");
}

bool ScrollEncoderManager::attemptReconnection() {
    Serial.println("[RECONNECT] Attempting scroll encoder reconnection...");
    
    // State machine for recovery
    switch (recoveryState) {
        case RECOVERY_IDLE:
            recoveryState = RECOVERY_BUS_RESET;
            performI2CBusRecovery();
            return false;
            
        case RECOVERY_BUS_RESET:
            recoveryState = RECOVERY_REINIT;
            // Delete old instance
            if (scrollEncoder) {
                delete scrollEncoder;
                scrollEncoder = nullptr;
            }
            // Create new instance
            scrollEncoder = new M5UnitScroll();
            return false;
            
        case RECOVERY_REINIT:
            recoveryState = RECOVERY_VERIFY;
            // Try to initialize
            if (scrollEncoder && scrollEncoder->begin(&Wire, 
                                                     HardwareConfig::M5UNIT_SCROLL_ADDR,
                                                     HardwareConfig::I2C_SDA_SCROLL,
                                                     HardwareConfig::I2C_SCL_SCROLL, 
                                                     400000)) {
                return true;
            }
            return false;
            
        case RECOVERY_VERIFY:
            // Verify connection
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                Wire.beginTransmission(HardwareConfig::M5UNIT_SCROLL_ADDR);
                bool success = (Wire.endTransmission() == 0);
                xSemaphoreGive(i2cMutex);
                
                if (success) {
                    recoveryState = RECOVERY_IDLE;
                    isAvailable = true;
                    needsRecovery = false;
                    consecutiveErrors = 0;
                    metrics.recordRecovery();
                    
                    // Reset encoder value and update LED
                    scrollEncoder->setEncoderValue(0);
                    ledAnimator.setBaseColor(PARAM_COLORS[state.currentParam]);
                    updateLED();
                    
                    Serial.println("\u2705 Scroll encoder reconnected successfully!");
                    return true;
                }
            }
            recoveryState = RECOVERY_FAILED;
            return false;
            
        case RECOVERY_FAILED:
            // Wait before retrying
            recoveryState = RECOVERY_IDLE;
            Serial.println("\u274c Scroll encoder recovery failed - will retry");
            return false;
    }
    
    return false;
}

void ScrollEncoderManager::checkConnectionHealth(uint32_t now) {
    if (!isAvailable) return;
    
    // Check for timeout
    if (now - lastSuccessfulRead > HEALTH_TIMEOUT) {
        consecutiveErrors++;
        Serial.printf("\u26a0\ufe0f Scroll encoder timeout - %u consecutive errors\n", consecutiveErrors);
        
        if (consecutiveErrors >= MAX_ERRORS) {
            needsRecovery = true;
            isAvailable = false;
            metrics.recordConnectionLoss();
            Serial.println("[ALERT] Scroll encoder marked unhealthy - recovery needed");
        }
    }
    
    // Check watchdog
    if (watchdog.check()) {
        Serial.println("[ALERT] Scroll encoder watchdog triggered!");
        needsRecovery = true;
        isAvailable = false;
        metrics.recordConnectionLoss();
    }
}

void ScrollEncoderManager::updateHealthStatus(bool success, uint32_t responseTime) {
    if (success) {
        lastSuccessfulRead = millis();
        consecutiveErrors = 0;
        metrics.recordRead(true, responseTime);
        panicMode.resetFailures();  // Reset panic failure counter on success
        
        if (needsRecovery) {
            needsRecovery = false;
            Serial.println("\u2705 Scroll encoder health restored");
        }
        
        // Try to exit panic mode if we're in it
        if (panicMode.isActive()) {
            panicMode.exitPanicMode();
        }
    } else {
        consecutiveErrors++;
        metrics.recordRead(false, 0);
        panicMode.incrementFailures();  // Increment panic failure counter
    }
}

// ============== PARAMETER PERSISTENCE ==============
bool ScrollEncoderManager::saveParameters() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("scroll_params", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        Serial.printf("\u274c NVS open failed: %s\n", esp_err_to_name(err));
        return false;
    }
    
    // Save all parameters
    const char* keys[PARAM_COUNT] = {
        "effect", "brightness", "palette", "speed",
        "intensity", "saturation", "complexity", "variation"
    };
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        err = nvs_set_u8(handle, keys[i], state.paramValues[i]);
        if (err != ESP_OK) {
            Serial.printf("\u274c Failed to save %s: %s\n", keys[i], esp_err_to_name(err));
        }
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err == ESP_OK) {
        Serial.println("[SAVE] Scroll parameters saved");
        return true;
    }
    return false;
}

bool ScrollEncoderManager::loadParameters() {
    // Ensure NVS is initialized
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open("scroll_params", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        Serial.println("[LOAD] No saved parameters found - using defaults");
        return false;
    } else if (err != ESP_OK) {
        Serial.printf("\u274c NVS open failed: %s\n", esp_err_to_name(err));
        return false;
    }
    
    // Load all parameters
    const char* keys[PARAM_COUNT] = {
        "effect", "brightness", "palette", "speed",
        "intensity", "saturation", "complexity", "variation"
    };
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        size_t length = sizeof(uint8_t);
        err = nvs_get_u8(handle, keys[i], &state.paramValues[i]);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            // Keep default value
            Serial.printf("[LOAD] %s not found - using default\n", keys[i]);
        } else if (err != ESP_OK) {
            Serial.printf("\u274c Failed to load %s: %s\n", keys[i], esp_err_to_name(err));
        } else {
            // Validate loaded values
            if (i == PARAM_PALETTE && state.paramValues[i] >= gGradientPaletteCount) {
                Serial.printf("\u26a0\ufe0f Invalid palette index %d, resetting to 0\n", state.paramValues[i]);
                state.paramValues[i] = 0;
            }
        }
    }
    
    nvs_close(handle);
    Serial.println("\u2705 Scroll parameters loaded");
    return true;
}

// ============== PANIC MODE IMPLEMENTATION ==============
void PanicMode::checkPanicConditions(uint32_t lastSuccessfulRead, uint32_t watchdogTime) {
    // Check for extended watchdog timeout
    if (watchdogTime > 10000) {
        Serial.println("🚨 PANIC: Extended watchdog timeout!");
        enterPanicMode(PANIC_FULL_RESET);
        return;
    }
    
    // Check for repeated failures
    if (consecutiveFailures >= PANIC_THRESHOLD) {
        Serial.println("🚨 PANIC: Too many consecutive failures!");
        enterPanicMode(PANIC_RESTORE_DEFAULTS);
        return;
    }
    
    // Check for persistent I2C failure
    if (millis() - lastSuccessfulRead > PANIC_TIMEOUT) {
        Serial.println("🚨 PANIC: Persistent I2C failure!");
        enterPanicMode(PANIC_BYPASS_ENCODER);
        return;
    }
}

void PanicMode::checkManualTrigger(bool buttonPressed) {
    if (buttonPressed) {
        if (!panicButtonPressed) {
            panicButtonPressStart = millis();
            panicButtonPressed = true;
        } else if (millis() - panicButtonPressStart > PANIC_BUTTON_DURATION) {
            Serial.println("🚨 PANIC: Manual trigger activated!");
            enterPanicMode(PANIC_DIAGNOSTIC_MODE);
            panicButtonPressed = false; // Reset to prevent re-trigger
        }
    } else {
        panicButtonPressed = false;
    }
}

void PanicMode::enterPanicMode(PanicAction action) {
    isPanicMode = true;
    panicStartTime = millis();
    
    Serial.println("\n🚨🚨🚨 PANIC MODE ACTIVATED 🚨🚨🚨");
    
    // Visual indication - rapid red flashing
    if (manager && manager->scrollEncoder && manager->isAvailable) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < 10; i++) {
                manager->scrollEncoder->setLEDColor(0xFF0000);
                xSemaphoreGive(i2cMutex);
                delay(100);
                
                if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    manager->scrollEncoder->setLEDColor(0x000000);
                    xSemaphoreGive(i2cMutex);
                }
                delay(100);
            }
        }
    }
    
    executePanicAction(action);
}

void PanicMode::executePanicAction(PanicAction action) {
    switch (action) {
        case PANIC_FULL_RESET:
            Serial.println("🚨 Executing FULL SYSTEM RESET...");
            // 1. Force I2C bus reset
            forceBusReset();
            // 2. Clear all NVS parameters
            nvs_flash_erase();
            nvs_flash_init();
            // 3. Restart ESP32
            ESP.restart();
            break;
            
        case PANIC_RESTORE_DEFAULTS:
            Serial.println("🚨 Restoring DEFAULT parameters...");
            // Reset all parameters to safe defaults
            if (manager) {
                for (int i = 0; i < PARAM_COUNT; i++) {
                    manager->state.paramValues[i] = getDefaultValue((ScrollParameter)i);
                }
                manager->saveParameters();
                // Attempt recovery
                manager->attemptReconnection();
            }
            break;
            
        case PANIC_BYPASS_ENCODER:
            Serial.println("🚨 BYPASSING encoder - using defaults...");
            // Disable encoder updates
            if (manager) {
                manager->isAvailable = false;
                // Use default values
                applyDefaultParameters();
            }
            break;
            
        case PANIC_DIAGNOSTIC_MODE:
            Serial.println("🚨 Entering DIAGNOSTIC mode...");
            runDiagnostics();
            break;
    }
}

void PanicMode::forceBusReset() {
    Serial.println("🔧 Aggressive I2C bus recovery...");
    
    // More aggressive recovery
    digitalWrite(HardwareConfig::I2C_SCL_SCROLL, LOW);
    digitalWrite(HardwareConfig::I2C_SDA_SCROLL, LOW);
    delay(100);
    
    // Generate 20 clock pulses (double normal)
    pinMode(HardwareConfig::I2C_SCL_SCROLL, OUTPUT);
    pinMode(HardwareConfig::I2C_SDA_SCROLL, OUTPUT);
    
    for (int i = 0; i < 20; i++) {
        digitalWrite(HardwareConfig::I2C_SCL_SCROLL, HIGH);
        delayMicroseconds(10);
        digitalWrite(HardwareConfig::I2C_SCL_SCROLL, LOW);
        delayMicroseconds(10);
    }
    
    // Force STOP condition
    digitalWrite(HardwareConfig::I2C_SDA_SCROLL, LOW);
    delayMicroseconds(10);
    digitalWrite(HardwareConfig::I2C_SCL_SCROLL, HIGH);
    delayMicroseconds(10);
    digitalWrite(HardwareConfig::I2C_SDA_SCROLL, HIGH);
    delay(10);
    
    // Complete reinit
    Wire.end();
    delay(500);
    Wire.begin(HardwareConfig::I2C_SDA_SCROLL, HardwareConfig::I2C_SCL_SCROLL);
    Wire.setClock(400000);
}

void PanicMode::runDiagnostics() {
    Serial.println("\n📊 SCROLL ENCODER DIAGNOSTICS");
    Serial.println("================================");
    
    // Test I2C bus
    Serial.print("I2C Bus Scan: ");
    Wire.beginTransmission(HardwareConfig::M5UNIT_SCROLL_ADDR);
    uint8_t error = Wire.endTransmission();
    Serial.printf("Device at 0x40: %s\n", error == 0 ? "FOUND" : "NOT FOUND");
    
    // Check voltage levels
    Serial.printf("SDA Pin %d: %s\n", HardwareConfig::I2C_SDA_SCROLL,
                  digitalRead(HardwareConfig::I2C_SDA_SCROLL) ? "HIGH" : "LOW");
    Serial.printf("SCL Pin %d: %s\n", HardwareConfig::I2C_SCL_SCROLL,
                  digitalRead(HardwareConfig::I2C_SCL_SCROLL) ? "HIGH" : "LOW");
    
    // Memory status
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Min Free Heap: %d bytes\n", ESP.getMinFreeHeap());
    
    // Metrics dump
    if (manager) {
        manager->metrics.printReport();
    }
    
    // Attempt communication test
    if (manager && manager->scrollEncoder) {
        for (int i = 0; i < 5; i++) {
            Serial.printf("Communication test %d/5: ", i+1);
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                bool success = manager->scrollEncoder->getDevStatus();
                xSemaphoreGive(i2cMutex);
                Serial.println(success ? "SUCCESS" : "FAILED");
            } else {
                Serial.println("MUTEX TIMEOUT");
            }
            delay(1000);
        }
    }
}

bool PanicMode::exitPanicMode() {
    if (!manager || !manager->scrollEncoder) return false;
    
    // Verify encoder is responsive
    bool responsive = false;
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        responsive = manager->scrollEncoder->getDevStatus();
        xSemaphoreGive(i2cMutex);
    }
    
    if (!responsive) {
        return false;
    }
    
    // Reset panic state
    isPanicMode = false;
    consecutiveFailures = 0;
    
    // Restore normal LED
    if (manager) {
        manager->updateLED();
    }
    
    Serial.println("✅ Exited panic mode - normal operation resumed");
    return true;
}

uint8_t PanicMode::getDefaultValue(ScrollParameter param) {
    switch (param) {
        case PARAM_EFFECT: return 0;
        case PARAM_BRIGHTNESS: return 96;
        case PARAM_PALETTE: return 0;
        case PARAM_SPEED: return 128;
        case PARAM_INTENSITY: return 128;
        case PARAM_SATURATION: return 128;
        case PARAM_COMPLEXITY: return 128;
        case PARAM_VARIATION: return 128;
        default: return 128;
    }
}

void PanicMode::applyDefaultParameters() {
    if (!manager) return;
    
    // Apply all default values
    for (int i = 0; i < PARAM_COUNT; i++) {
        ScrollParameter param = (ScrollParameter)i;
        uint8_t defaultValue = getDefaultValue(param);
        manager->state.paramValues[param] = defaultValue;
        
        // Trigger callbacks
        if (manager->onParamChange) {
            manager->onParamChange(param, defaultValue);
        }
    }
    
    Serial.println("✅ Default parameters applied");
}