#pragma once
// ============================================================================
// DisplayUI - Main UI controller for Tab5.encoder
// ============================================================================
// 4x4 grid layout with status bar
// Clean, readable, no bullshit animations
// ============================================================================

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    #include <M5GFX.h>
    #include <lvgl.h>
    #include "lvgl_bridge.h"
    #include "../network/WebSocketClient.h"
#else
    #ifdef SIMULATOR_BUILD
        #include "M5GFX_Mock.h"
    #else
        #include <M5GFX.h>
    #endif
    #include "Theme.h"
    #include "widgets/GaugeWidget.h"
    #include "widgets/UIHeader.h"
    #include "widgets/PresetSlotWidget.h"
    #ifndef SIMULATOR_BUILD
    #include "widgets/ActionRowWidget.h"
    #include "../network/WebSocketClient.h"
    #endif
#endif

// Forward declarations
#ifndef SIMULATOR_BUILD
class ZoneComposerUI;
class ConnectivityTab;
class PresetManager;
#endif
class UIHeader;

/**
 * UI screen types
 */
enum class UIScreen : uint8_t {
    GLOBAL = 0,        // Default: 16-parameter gauge view
    ZONE_COMPOSER = 1, // Zone composer dashboard
    CONNECTIVITY = 2   // Network connectivity management
};

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
typedef void (*ActionButtonCallback)(uint8_t index);
typedef void (*RetryButtonCallback)();
#endif

class DisplayUI {
public:
    DisplayUI(M5GFX& display);
    ~DisplayUI();

    void begin();
    void loop();

    // Data updates
    void updateEncoder(uint8_t index, int32_t value, bool highlight);
    void updateValue(uint8_t index, int32_t value, bool highlight = true) { updateEncoder(index, value, highlight); }  // Alias for compat
    void setConnectionState(bool wifi, bool ws, bool encA, bool encB);

    // Metadata display (for effect/palette names from server)
    void setCurrentEffect(uint8_t id, const char* name);
    void setCurrentPalette(uint8_t id, const char* name);
    void setWiFiInfo(const char* ip, const char* ssid, int32_t rssi);
    void updateRetryButton(bool shouldShow);  // Update retry button visibility

    // Footer metrics updates
    void updateAudioMetrics(float bpm, const char* key, float micLevel);
    void updateHostUptime(uint32_t uptimeSeconds);
    void setWebSocketConnected(bool connected, uint32_t connectTime = 0);
    void updateWebSocketStatus(WebSocketStatus status);

    // Screen switching
    void setScreen(UIScreen screen);
    UIScreen getCurrentScreen() const { return _currentScreen; }
    
    // Get zone composer UI (for router initialization)
    #ifndef SIMULATOR_BUILD
    #if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL)
    ZoneComposerUI* getZoneComposerUI() { return _zoneComposer; }
    ConnectivityTab* getConnectivityTab() { return _connectivityTab; }
    #else
    ZoneComposerUI* getZoneComposerUI() { return _zoneComposer; }
    ConnectivityTab* getConnectivityTab() { return _connectivityTab; }
    #endif
    #endif
    
    // Get header instance (for ZoneComposerUI)
    #if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    UIHeader* getHeader() { return nullptr; }
    #else
    UIHeader* getHeader() { return _header; }
    #endif

    // Preset bank UI
    void updatePresetSlot(uint8_t slot, bool occupied, uint8_t effectId, uint8_t paletteId, uint8_t brightness);
    void setActivePresetSlot(uint8_t slot);
    void showPresetSaveFeedback(uint8_t slot);
    void showPresetRecallFeedback(uint8_t slot);
    void showPresetDeleteFeedback(uint8_t slot);

    // Network configuration UI
    void showNetworkConfigScreen();
    void hideNetworkConfigScreen();
    bool isNetworkConfigVisible() const;
    #ifndef SIMULATOR_BUILD
    void refreshAllPresetSlots(PresetManager* pm);
    #endif

    // Colour correction state (touch action row)
    #ifndef SIMULATOR_BUILD
    void setColourCorrectionState(const ColorCorrectionState& state);
    #if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL)
    void setActionButtonCallback(ActionButtonCallback cb) { _action_callback = cb; }
    void setRetryButtonCallback(RetryButtonCallback cb) { _retry_callback = cb; }
    #endif
    #endif

private:
    M5GFX& _display;

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    // Static callback for zone composer Back button
    static DisplayUI* s_instance;
    static void onZoneComposerBackButton();
    static void onConnectivityTabBackButton();
    uint8_t _activePresetSlot = 0xFF;
    UIScreen _currentScreen = UIScreen::GLOBAL;

    lv_obj_t* _screen_global = nullptr;
    lv_obj_t* _screen_zone = nullptr;
    lv_obj_t* _screen_connectivity = nullptr;

    lv_obj_t* _header = nullptr;
    lv_obj_t* _header_title_main = nullptr;
    lv_obj_t* _header_title_os = nullptr;
    lv_obj_t* _header_retry_button = nullptr;
    lv_obj_t* _header_effect_container = nullptr;
    lv_obj_t* _header_effect = nullptr;
    lv_obj_t* _header_palette_container = nullptr;
    lv_obj_t* _header_palette = nullptr;
    lv_obj_t* _header_net_container = nullptr;  // Clickable container for network info
    lv_obj_t* _header_net_ip = nullptr;
    lv_obj_t* _header_net_ssid = nullptr;
    lv_obj_t* _header_net_rssi = nullptr;

    lv_obj_t* _gauges_container = nullptr;
    lv_obj_t* _gauge_cards[8] = {nullptr};
    lv_obj_t* _gauge_labels[8] = {nullptr};
    lv_obj_t* _gauge_values[8] = {nullptr};
    lv_obj_t* _gauge_bars[8] = {nullptr};

    lv_obj_t* _preset_container = nullptr;
    lv_obj_t* _preset_cards[8] = {nullptr};
    lv_obj_t* _preset_labels[8] = {nullptr};
    lv_obj_t* _preset_values[8] = {nullptr};

    uint32_t _feedback_until_ms[8] = {0};
    uint32_t _feedback_color_hex[8] = {0};

    lv_obj_t* _action_container = nullptr;
    lv_obj_t* _action_buttons[5] = {nullptr};
    lv_obj_t* _action_labels[5] = {nullptr};
    lv_obj_t* _action_values[5] = {nullptr};

    ActionButtonCallback _action_callback = nullptr;
    RetryButtonCallback _retry_callback = nullptr;

    // Zone Composer UI
    ZoneComposerUI* _zoneComposer = nullptr;
    
    // Connectivity Tab UI
    ConnectivityTab* _connectivityTab = nullptr;

    // Footer UI elements
    lv_obj_t* _footer = nullptr;
    // Metric labels: split into title (fixed) and value (changes)
    lv_obj_t* _footer_bpm = nullptr;          // Title: "BPM:"
    lv_obj_t* _footer_bpm_value = nullptr;   // Value: "120" or "--"
    lv_obj_t* _footer_key = nullptr;         // Title: "KEY:"
    lv_obj_t* _footer_key_value = nullptr;   // Value: "C" or "--"
    lv_obj_t* _footer_mic = nullptr;         // Title: "MIC:"
    lv_obj_t* _footer_mic_value = nullptr;   // Value: "-46.4 DB" or "--"
    lv_obj_t* _footer_host_uptime = nullptr; // Title: "UPTIME:"
    lv_obj_t* _footer_uptime_value = nullptr; // Value: "1h 23m" or "--"
    lv_obj_t* _footer_ws_status = nullptr;
    lv_obj_t* _footer_battery = nullptr;
    lv_obj_t* _footer_battery_bar = nullptr;

    // Footer data members
    float _bpm = 0.0f;
    char _key[8] = {'\0'};
    float _micLevel = -80.0f;  // Default to -80dB (silence)
    uint32_t _hostUptime = 0;
    uint32_t _lastFooterUpdate = 0;  // For throttling battery/uptime updates (1Hz)

    // Network configuration screen
    lv_obj_t* _network_config_screen = nullptr;
    lv_obj_t* _network_config_ip_input = nullptr;
    lv_obj_t* _network_config_toggle = nullptr;
    lv_obj_t* _network_config_status_label = nullptr;
    bool _network_config_visible = false;
#else
    UIHeader* _header;
    GaugeWidget* _gauges[16];
    PresetSlotWidget* _presetSlots[8];
    #ifndef SIMULATOR_BUILD
    ActionRowWidget* _actionRow = nullptr;
    #endif
    #ifndef SIMULATOR_BUILD
    ZoneComposerUI* _zoneComposer = nullptr;
    #endif
    uint8_t _activePresetSlot = 0xFF;  // 0xFF = none active

    // Screen state
    UIScreen _currentScreen = UIScreen::GLOBAL;

    // State
    uint32_t _lastStatsUpdate = 0;
    uint8_t _highlightIdx = 255;
    uint32_t _highlightTime = 0;

    void updateStats();
    void updateHeader();
    void renderCurrentScreen();
#endif
};
