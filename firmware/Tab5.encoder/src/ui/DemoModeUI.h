/**
 * @file DemoModeUI.h
 * @brief Interactive Demo Mode Screen for Tab5.encoder
 *
 * 3-Lane touch-first interface optimized for in-person demos:
 * - Lane A: Scene Cards + Watch Demo button
 * - Lane B: Feel Controls (Energy/Flow/Brightness)
 * - Lane C: Palette Picker + Saturation
 *
 * Design principles:
 * - UI feedback < 100ms (highlight immediately)
 * - Light response < 500ms (visual change on LEDs)
 * - Status bar shows truthful ACK-based state
 */

#pragma once

#include <M5GFX.h>
#include <lvgl.h>
#include <cstdint>
#include "Theme.h"

// Forward declarations
class WebSocketClient;

namespace demo {
struct DemoScene;
}

/**
 * Apply state for ACK-based status display
 */
enum class ApplyState : uint8_t {
    IDLE,           // Showing last successful apply
    APPLYING,       // Sent command, awaiting ACK
    TIMEOUT,        // ACK not received in time
};

/**
 * Demo mode connection/sync status
 */
struct DemoStatus {
    bool connected = false;
    ApplyState applyState = ApplyState::IDLE;
    uint32_t commandSent_ms = 0;       // When we sent the command
    uint32_t lastAcked_ms = 0;         // When node confirmed (ACK received)
    const char* pendingName = nullptr; // What we're trying to apply
    const char* appliedName = nullptr; // What was last ACKed
    uint8_t fps = 0;
    int8_t syncQuality = 0;            // -1=Poor, 0=OK, 1=Good
};

class DemoModeUI {
public:
    DemoModeUI(M5GFX& display);
    ~DemoModeUI();

    /**
     * Initialize the demo mode screen
     * @param parent Parent LVGL screen object
     */
    void begin(lv_obj_t* parent = nullptr);

    /**
     * Main loop - handle animations and status updates
     */
    void loop();

    /**
     * Set WebSocket client for sending commands
     * @param wsClient WebSocketClient instance
     */
    void setWebSocketClient(WebSocketClient* wsClient) { _wsClient = wsClient; }

    /**
     * Set callback for Back button (returns to GLOBAL screen)
     * @param callback Function to call when Back button pressed
     */
    typedef void (*BackButtonCallback)();
    void setBackButtonCallback(BackButtonCallback callback) { _backButtonCallback = callback; }

    /**
     * Update connection state
     * @param connected WebSocket connected
     * @param fps Current FPS from node
     * @param syncQuality -1=Poor, 0=OK, 1=Good
     */
    void updateConnectionState(bool connected, uint8_t fps, int8_t syncQuality);

    /**
     * Called when node ACKs a scene change
     * @param sceneName Name of the applied scene
     */
    void onNodeAck(const char* sceneName);

    /**
     * Mark UI as dirty (needs redraw)
     */
    void markDirty() { _dirty = true; }

    /**
     * Force immediate dirty state
     */
    void forceDirty() { _dirty = true; _lastRenderTime = 0; }

    // ========================================================================
    // Scene Card Actions (Lane A)
    // ========================================================================

    /**
     * Apply a scene by index
     * @param sceneIndex Index into DEMO_SCENES array (0-7)
     */
    void applyScene(uint8_t sceneIndex);

    /**
     * Start/stop Watch Demo playback
     */
    void toggleWatchDemo();

    /**
     * Reset to signature scene (Ocean Depths)
     */
    void resetToSignature();

    // ========================================================================
    // Feel Controls (Lane B)
    // ========================================================================

    /**
     * Set energy level (maps to speed+intensity+complexity)
     * @param energy 0-100%
     */
    void setEnergy(uint8_t energy);

    /**
     * Set flow level (maps to mood+variation+fade)
     * @param flow 0-100%
     */
    void setFlow(uint8_t flow);

    /**
     * Set brightness level (direct mapping)
     * @param brightness 0-100%
     */
    void setBrightness(uint8_t brightness);

    // ========================================================================
    // Palette Controls (Lane C)
    // ========================================================================

    /**
     * Apply palette group by index
     * @param groupIndex Index into PALETTE_GROUPS (0-7)
     */
    void applyPaletteGroup(uint8_t groupIndex);

    /**
     * Set saturation level
     * @param saturation 0-100%
     */
    void setSaturation(uint8_t saturation);

private:
    M5GFX& _display;
    WebSocketClient* _wsClient = nullptr;
    BackButtonCallback _backButtonCallback = nullptr;

    // Screen and container objects
    lv_obj_t* _screen = nullptr;
    lv_obj_t* _statusBar = nullptr;
    lv_obj_t* _laneA = nullptr;  // Scene cards
    lv_obj_t* _laneB = nullptr;  // Feel controls
    lv_obj_t* _laneC = nullptr;  // Palette picker

    // Status bar widgets
    lv_obj_t* _connDot = nullptr;
    lv_obj_t* _syncLabel = nullptr;
    lv_obj_t* _lastAppliedLabel = nullptr;
    lv_obj_t* _fpsLabel = nullptr;
    lv_obj_t* _resetButton = nullptr;

    // Scene card widgets
    lv_obj_t* _sceneCards[8] = {nullptr};
    lv_obj_t* _watchDemoButton = nullptr;
    uint8_t _selectedSceneIndex = 0;

    // Feel control sliders
    lv_obj_t* _energySlider = nullptr;
    lv_obj_t* _flowSlider = nullptr;
    lv_obj_t* _brightnessSlider = nullptr;

    // Palette widgets
    lv_obj_t* _paletteChips[8] = {nullptr};
    lv_obj_t* _saturationSlider = nullptr;
    uint8_t _selectedPaletteIndex = 0;

    // State
    DemoStatus _status;
    bool _dirty = true;
    uint32_t _lastRenderTime = 0;
    static constexpr uint32_t FRAME_INTERVAL_MS = 33;  // ~30 FPS

    // Watch Demo playback state
    bool _watchDemoPlaying = false;
    uint8_t _watchDemoStep = 0;
    uint32_t _watchDemoStepStart = 0;

    // Slider state for rate limiting
    uint32_t _lastSliderSend = 0;
    static constexpr uint32_t SLIDER_UPDATE_INTERVAL_MS = 40;  // 25 Hz

    // Current values (cached)
    uint8_t _currentEnergy = 50;
    uint8_t _currentFlow = 50;
    uint8_t _currentBrightness = 75;
    uint8_t _currentSaturation = 80;

    // ACK timeout
    static constexpr uint32_t ACK_TIMEOUT_MS = 1000;

    // ========================================================================
    // Private Methods
    // ========================================================================

    /**
     * Create the complete UI layout
     */
    void createUI(lv_obj_t* parent);

    /**
     * Create status bar (connection, sync, last applied, reset)
     */
    void createStatusBar(lv_obj_t* parent);

    /**
     * Create Lane A: Scene cards row
     */
    void createLaneA(lv_obj_t* parent);

    /**
     * Create Lane B: Feel controls row
     */
    void createLaneB(lv_obj_t* parent);

    /**
     * Create Lane C: Palette picker row
     */
    void createLaneC(lv_obj_t* parent);

    /**
     * Create single scene card
     */
    lv_obj_t* createSceneCard(lv_obj_t* parent, uint8_t index);

    /**
     * Create feel slider
     */
    lv_obj_t* createFeelSlider(lv_obj_t* parent, const char* label, uint8_t initialValue);

    /**
     * Create palette chip
     */
    lv_obj_t* createPaletteChip(lv_obj_t* parent, uint8_t index);

    /**
     * Update status bar display
     */
    void updateStatusBar();

    /**
     * Update Watch Demo progress
     */
    void updateWatchDemo();

    /**
     * Send parameter to node via WebSocket
     */
    void sendParameter(const char* key, uint8_t value);

    /**
     * Send scene bundle to node
     */
    void sendSceneBundle(const demo::DemoScene& scene);

    // ========================================================================
    // LVGL Callbacks (static)
    // ========================================================================

    static void sceneCardTouchCb(lv_event_t* e);
    static void watchDemoButtonCb(lv_event_t* e);
    static void resetButtonCb(lv_event_t* e);
    static void energySliderCb(lv_event_t* e);
    static void flowSliderCb(lv_event_t* e);
    static void brightnessSliderCb(lv_event_t* e);
    static void paletteChipTouchCb(lv_event_t* e);
    static void saturationSliderCb(lv_event_t* e);
    static void backButtonCb(lv_event_t* e);
};
