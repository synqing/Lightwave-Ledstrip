#pragma once
// ============================================================================
// TouchHandler - Touch Screen Integration for Tab5.encoder
// ============================================================================
// Phase 7 (G.3): Provides touch-based parameter control on Tab5's 5" LCD
// (800x480) with CST816S capacitive touch controller.
//
// Features:
// - TAP on parameter cell: Optional highlight/feedback
// - LONG_PRESS on parameter cell: Reset parameter to default value
// - Touch zone hit testing for 16 parameter cells (2 columns x 8 rows)
// - Debounced touch input to prevent accidental double-taps
//
// Display Layout Reference (from main.cpp):
//   Status bar: y=0-199 (title, milestone info)
//   Parameter grid: y=200-480 (2 columns, 8 rows)
//     - Column 0 (left): indices 0-7  (x=20-300)
//     - Column 1 (right): indices 8-15 (x=340-620)
//     - Each cell: 35px height, ~300px width
//
// Usage:
//   TouchHandler touch;
//   touch.init();
//   touch.setEncoderService(g_encoders);
//   touch.onLongPress([](uint8_t idx) { ... });
//   // In loop:
//   touch.update();
// ============================================================================

#include <Arduino.h>
#include <M5Unified.h>
#include <functional>
#include "../ui/Theme.h"

// Forward declaration
class DualEncoderService;

// ============================================================================
// Touch Event Types
// ============================================================================

enum class TouchEventType : uint8_t {
    NONE = 0,
    TAP,              // Quick tap (< LONG_PRESS_THRESHOLD_MS)
    LONG_PRESS,       // Held for >= LONG_PRESS_THRESHOLD_MS
    RELEASE           // Finger lifted
};

enum class TouchZone : uint8_t {
    NONE = 0,
    STATUS_BAR,       // y=0-199 (title area)
    PARAMETER_GRID,   // y=200-480 (parameter cells)
    ACTION_ROW,       // Touch buttons row
    NAVIGATION        // Reserved for future use
};

// ============================================================================
// Touch Configuration Constants
// ============================================================================

namespace TouchConfig {
    // Display dimensions (Tab5 5" LCD in landscape)
    constexpr int16_t SCREEN_WIDTH = Theme::SCREEN_W;
    constexpr int16_t SCREEN_HEIGHT = Theme::SCREEN_H;

    // Status bar zone
    constexpr int16_t STATUS_BAR_Y_START = 0;
    constexpr int16_t STATUS_BAR_Y_END = Theme::STATUS_BAR_H - 1;

    // Parameter grid layout (disabled by default for Tab5 touch)
    constexpr int16_t GRID_Y_START = Theme::SCREEN_H + 1;
    constexpr int16_t CELL_HEIGHT = 35;
    constexpr int16_t COL_WIDTH = 320;
    constexpr int16_t COL0_X_START = 20;
    constexpr int16_t COL1_X_START = 340;
    constexpr int16_t CELL_WIDTH = 300;

    // Action row layout (third row)
    constexpr int16_t ACTION_ROW_Y_START = Theme::ACTION_ROW_Y;
    constexpr int16_t ACTION_ROW_Y_END = Theme::ACTION_ROW_Y + Theme::ACTION_ROW_H - 1;
    
    // Debug: Log action row bounds at compile time (visible in serial)
    // ACTION_ROW_Y = PRESET_ROW_Y + PRESET_SLOT_H + 20 = 240 + 144 + 20 = 404
    // ACTION_ROW_Y_END = 404 + 120 - 1 = 523
    constexpr uint8_t ACTION_BUTTONS = 4;
    constexpr int16_t ACTION_BUTTON_W = Theme::ACTION_BTN_W;

    // Number of rows per column
    constexpr uint8_t ROWS_PER_COLUMN = 8;
    constexpr uint8_t TOTAL_CELLS = 16;

    // Touch timing thresholds
    constexpr uint32_t LONG_PRESS_THRESHOLD_MS = 500;   // 500ms for long press
    constexpr uint32_t DEBOUNCE_MS = 100;               // 100ms debounce
    constexpr uint32_t TAP_MAX_MS = 300;                // Max duration for tap

    // Visual feedback
    constexpr uint32_t HIGHLIGHT_DURATION_MS = 200;     // Brief highlight on tap
}

// ============================================================================
// TouchHandler Class
// ============================================================================

class TouchHandler {
public:
    // Callback types
    using TapCallback = std::function<void(uint8_t paramIndex)>;
    using LongPressCallback = std::function<void(uint8_t paramIndex)>;
    using StatusBarCallback = std::function<void(int16_t x, int16_t y)>;
    using ActionButtonCallback = std::function<void(uint8_t buttonIndex)>;

    /**
     * Constructor
     */
    TouchHandler();

    /**
     * Initialize touch handling
     * M5.Touch is already initialized by M5.begin(), so this just
     * sets up internal state.
     * @return true always (touch is initialized by M5Unified)
     */
    bool init();

    /**
     * Set reference to encoder service for parameter resets
     * @param service Pointer to DualEncoderService (can be null)
     */
    void setEncoderService(DualEncoderService* service);

    /**
     * Poll touch events and process them
     * Call this in the main loop after M5.update()
     */
    void update();

    // ========================================================================
    // Callback Registration
    // ========================================================================

    /**
     * Register callback for tap events on parameter cells
     * @param callback Function called with parameter index (0-15)
     */
    void onTap(TapCallback callback);

    /**
     * Register callback for long press events on parameter cells
     * @param callback Function called with parameter index (0-15)
     */
    void onLongPress(LongPressCallback callback);

    /**
     * Register callback for status bar touch events
     * @param callback Function called with touch coordinates
     */
    void onStatusBarTouch(StatusBarCallback callback);

    /**
     * Register callback for action row button taps
     * @param callback Function called with button index (0-3)
     */
    void onActionButton(ActionButtonCallback callback);

    /**
     * Set screen gate callback for touch zone isolation
     * The gate callback returns true if touches should be processed,
     * false if they should be ignored (e.g., when not on GLOBAL screen).
     * @param gate Callback that returns true to allow touch processing
     */
    using ScreenGateCallback = std::function<bool()>;
    void setScreenGate(ScreenGateCallback gate);

    // ========================================================================
    // State Query
    // ========================================================================

    /**
     * Check if touch is currently active
     * @return true if finger is currently touching screen
     */
    bool isTouching() const;

    /**
     * Get current touch coordinates (only valid if isTouching())
     * @param x Output X coordinate
     * @param y Output Y coordinate
     * @return true if coordinates are valid
     */
    bool getPosition(int16_t& x, int16_t& y) const;

    /**
     * Get the last touched parameter index
     * @return Parameter index (0-15) or -1 if no parameter cell was touched
     */
    int8_t getLastTouchedParam() const;

    // ========================================================================
    // Hit Testing
    // ========================================================================

    /**
     * Hit test to determine which zone was touched
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     * @return TouchZone enum indicating zone type
     */
    static TouchZone hitTestZone(int16_t x, int16_t y);

    /**
     * Hit test to determine which parameter cell was touched
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     * @return Parameter index (0-15) or -1 if no cell was hit
     */
    static int8_t hitTestParameter(int16_t x, int16_t y);

    /**
     * Hit test to determine which action button was touched
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     * @return Button index (0-3) or -1 if none
     */
    static int8_t hitTestActionButton(int16_t x, int16_t y);

private:
    // ========================================================================
    // Internal State
    // ========================================================================

    // Touch state tracking
    bool m_touching;                 // Currently touching
    bool m_wasPressed;               // Was pressed last frame
    int16_t m_touchX;                // Current/last touch X
    int16_t m_touchY;                // Current/last touch Y
    uint32_t m_touchStartTime;       // When touch began
    int8_t m_touchedParam;           // Parameter being touched (-1 if none)
    int8_t m_touchedAction;          // Action button being touched (-1 if none)
    bool m_longPressTriggered;       // Long press already triggered this touch

    // Debounce
    uint32_t m_lastEventTime;        // Time of last processed event

    // External references
    DualEncoderService* m_encoderService;

    // Callbacks
    TapCallback m_tapCallback;
    LongPressCallback m_longPressCallback;
    StatusBarCallback m_statusBarCallback;
    ActionButtonCallback m_actionButtonCallback;
    ScreenGateCallback m_screenGateCallback;

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * Process touch start event
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     */
    void handleTouchStart(int16_t x, int16_t y);

    /**
     * Process touch hold (check for long press)
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     * @param duration Time since touch started
     */
    void handleTouchHold(int16_t x, int16_t y, uint32_t duration);

    /**
     * Process touch release event
     * @param duration Time since touch started
     */
    void handleTouchRelease(uint32_t duration);

    /**
     * Reset parameter to default value
     * @param paramIndex Parameter index (0-15)
     */
    void resetParameterToDefault(uint8_t paramIndex);
};

// ============================================================================
// Inline Implementation (Simple Methods)
// ============================================================================

inline TouchHandler::TouchHandler()
    : m_touching(false)
    , m_wasPressed(false)
    , m_touchX(0)
    , m_touchY(0)
    , m_touchStartTime(0)
    , m_touchedParam(-1)
    , m_touchedAction(-1)
    , m_longPressTriggered(false)
    , m_lastEventTime(0)
    , m_encoderService(nullptr)
    , m_tapCallback(nullptr)
    , m_longPressCallback(nullptr)
    , m_statusBarCallback(nullptr)
    , m_actionButtonCallback(nullptr)
    , m_screenGateCallback(nullptr)
{
}

inline bool TouchHandler::init() {
    // M5.Touch is initialized by M5.begin()
    // Just reset our internal state
    m_touching = false;
    m_wasPressed = false;
    m_touchStartTime = 0;
    m_touchedParam = -1;
    m_longPressTriggered = false;
    return true;
}

inline void TouchHandler::setEncoderService(DualEncoderService* service) {
    m_encoderService = service;
}

inline void TouchHandler::onTap(TapCallback callback) {
    m_tapCallback = callback;
}

inline void TouchHandler::onLongPress(LongPressCallback callback) {
    m_longPressCallback = callback;
}

inline void TouchHandler::onStatusBarTouch(StatusBarCallback callback) {
    m_statusBarCallback = callback;
}

inline void TouchHandler::onActionButton(ActionButtonCallback callback) {
    m_actionButtonCallback = callback;
}

inline void TouchHandler::setScreenGate(ScreenGateCallback gate) {
    m_screenGateCallback = gate;
}

inline bool TouchHandler::isTouching() const {
    return m_touching;
}

inline bool TouchHandler::getPosition(int16_t& x, int16_t& y) const {
    if (!m_touching) return false;
    x = m_touchX;
    y = m_touchY;
    return true;
}

inline int8_t TouchHandler::getLastTouchedParam() const {
    return m_touchedParam;
}

inline TouchZone TouchHandler::hitTestZone(int16_t x, int16_t y) {
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H3\",\"location\":\"TouchHandler.h:341\",\"message\":\"hitTestZone.entry\",\"data\":{\"x\":%d,\"y\":%d,\"statusBarYEnd\":%d,\"actionRowYStart\":%d,\"actionRowYEnd\":%d,\"gridYStart\":%d,\"screenH\":%d},\"timestamp\":%lu}\n", x, y, TouchConfig::STATUS_BAR_Y_END, TouchConfig::ACTION_ROW_Y_START, TouchConfig::ACTION_ROW_Y_END, TouchConfig::GRID_Y_START, TouchConfig::SCREEN_HEIGHT, (unsigned long)millis());
        // #endregion
    // Status bar zone
    if (y >= TouchConfig::STATUS_BAR_Y_START && y <= TouchConfig::STATUS_BAR_Y_END) {
        // #region agent log (DISABLED)
        // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"TouchHandler.h:344\",\"message\":\"hitTestZone.statusBar\",\"data\":{\"x\":%d,\"y\":%d},\"timestamp\":%lu}\n", x, y, (unsigned long)millis());
                // #endregion
        return TouchZone::STATUS_BAR;
    }

    // Action row zone (check BEFORE parameter grid to avoid conflicts)
    if (y >= TouchConfig::ACTION_ROW_Y_START && y <= TouchConfig::ACTION_ROW_Y_END) {
        // #region agent log (DISABLED)
        // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"TouchHandler.h:352\",\"message\":\"hitTestZone.actionRow\",\"data\":{\"x\":%d,\"y\":%d,\"yStart\":%d,\"yEnd\":%d},\"timestamp\":%lu}\n", x, y, TouchConfig::ACTION_ROW_Y_START, TouchConfig::ACTION_ROW_Y_END, (unsigned long)millis());
                // #endregion
        return TouchZone::ACTION_ROW;
    }

    // Parameter grid zone
    if (y >= TouchConfig::GRID_Y_START && y < TouchConfig::SCREEN_HEIGHT) {
        // #region agent log (DISABLED)
        // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"TouchHandler.h:359\",\"message\":\"hitTestZone.parameterGrid\",\"data\":{\"x\":%d,\"y\":%d},\"timestamp\":%lu}\n", x, y, (unsigned long)millis());
                // #endregion
        return TouchZone::PARAMETER_GRID;
    }

    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"TouchHandler.h:365\",\"message\":\"hitTestZone.none\",\"data\":{\"x\":%d,\"y\":%d},\"timestamp\":%lu}\n", x, y, (unsigned long)millis());
        // #endregion
    return TouchZone::NONE;
}

inline int8_t TouchHandler::hitTestParameter(int16_t x, int16_t y) {
    // Must be in parameter grid zone
    if (y < TouchConfig::GRID_Y_START || y >= TouchConfig::SCREEN_HEIGHT) {
        return -1;
    }

    // Calculate row (0-7)
    int16_t gridY = y - TouchConfig::GRID_Y_START;
    int8_t row = gridY / TouchConfig::CELL_HEIGHT;
    if (row < 0 || row >= TouchConfig::ROWS_PER_COLUMN) {
        return -1;
    }

    // Determine column (0 = left, 1 = right)
    int8_t col = -1;

    // Column 0 (left): x from 20 to 320
    if (x >= TouchConfig::COL0_X_START &&
        x < TouchConfig::COL0_X_START + TouchConfig::CELL_WIDTH) {
        col = 0;
    }
    // Column 1 (right): x from 340 to 640
    else if (x >= TouchConfig::COL1_X_START &&
             x < TouchConfig::COL1_X_START + TouchConfig::CELL_WIDTH) {
        col = 1;
    }

    if (col < 0) {
        return -1;  // Touch in gap between columns
    }

    // Calculate parameter index
    // Column 0: rows 0-7 -> indices 0-7
    // Column 1: rows 0-7 -> indices 8-15
    int8_t paramIndex = (col * TouchConfig::ROWS_PER_COLUMN) + row;

    if (paramIndex < 0 || paramIndex >= TouchConfig::TOTAL_CELLS) {
        return -1;
    }

    return paramIndex;
}

inline int8_t TouchHandler::hitTestActionButton(int16_t x, int16_t y) {
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"TouchHandler.h:403\",\"message\":\"hitTestActionButton.entry\",\"data\":{\"x\":%d,\"y\":%d,\"yStart\":%d,\"yEnd\":%d,\"screenW\":%d,\"buttonW\":%d},\"timestamp\":%lu}\n", x, y, TouchConfig::ACTION_ROW_Y_START, TouchConfig::ACTION_ROW_Y_END, TouchConfig::SCREEN_WIDTH, TouchConfig::ACTION_BUTTON_W, (unsigned long)millis());
        // #endregion
    if (y < TouchConfig::ACTION_ROW_Y_START || y > TouchConfig::ACTION_ROW_Y_END) {
        // #region agent log (DISABLED)
        // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"TouchHandler.h:405\",\"message\":\"hitTestActionButton.yOutOfRange\",\"data\":{\"x\":%d,\"y\":%d,\"yStart\":%d,\"yEnd\":%d},\"timestamp\":%lu}\n", x, y, TouchConfig::ACTION_ROW_Y_START, TouchConfig::ACTION_ROW_Y_END, (unsigned long)millis());
                // #endregion
        return -1;
    }

    if (x < 0 || x >= TouchConfig::SCREEN_WIDTH) {
        // #region agent log (DISABLED)
        // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"TouchHandler.h:410\",\"message\":\"hitTestActionButton.xOutOfRange\",\"data\":{\"x\":%d,\"screenW\":%d},\"timestamp\":%lu}\n", x, TouchConfig::SCREEN_WIDTH, (unsigned long)millis());
                // #endregion
        return -1;
    }

    int8_t idx = x / TouchConfig::ACTION_BUTTON_W;
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"TouchHandler.h:415\",\"message\":\"hitTestActionButton.calculated\",\"data\":{\"x\":%d,\"buttonW\":%d,\"calculatedIdx\":%d,\"maxButtons\":%d},\"timestamp\":%lu}\n", x, TouchConfig::ACTION_BUTTON_W, idx, TouchConfig::ACTION_BUTTONS, (unsigned long)millis());
        // #endregion
    if (idx < 0 || idx >= TouchConfig::ACTION_BUTTONS) {
        // #region agent log (DISABLED)
        // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"TouchHandler.h:417\",\"message\":\"hitTestActionButton.idxOutOfRange\",\"data\":{\"idx\":%d,\"maxButtons\":%d},\"timestamp\":%lu}\n", idx, TouchConfig::ACTION_BUTTONS, (unsigned long)millis());
                // #endregion
        return -1;
    }

    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"TouchHandler.h:422\",\"message\":\"hitTestActionButton.success\",\"data\":{\"idx\":%d},\"timestamp\":%lu}\n", idx, (unsigned long)millis());
        // #endregion
    return idx;
}
