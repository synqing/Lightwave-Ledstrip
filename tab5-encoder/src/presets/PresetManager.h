// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// PresetManager - Core Preset Logic for Tab5.encoder
// ============================================================================
// Coordinates preset save/recall/delete operations:
//   - Captures current state from ParameterHandler and WebSocketClient
//   - Stores presets via PresetStorage (NVS persistence)
//   - Applies presets via WebSocketClient commands
//   - Provides feedback callbacks for UI updates
//
// Interaction Model:
//   - SINGLE_CLICK on Unit-B button: Recall preset from that slot
//   - DOUBLE_CLICK on Unit-B button: Save current state to that slot
//   - LONG_HOLD on Unit-B button: Delete preset from that slot
//
// Usage:
//   PresetManager presetMgr(&paramHandler, &wsClient);
//   presetMgr.setFeedbackCallback([](uint8_t slot, PresetAction action, bool success) { ... });
//   presetMgr.init();
//
//   // On click events from ClickDetector:
//   presetMgr.recallPreset(0);  // Single-click on slot 0
//   presetMgr.savePreset(0);    // Double-click on slot 0
//   presetMgr.deletePreset(0);  // Long-hold on slot 0
// ============================================================================

#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include <Arduino.h>
#include <functional>
#include "../storage/PresetData.h"
#include "../storage/PresetStorage.h"

// Forward declarations
class ParameterHandler;
class WebSocketClient;
class ZoneComposerUI;

// Preset action types for feedback
enum class PresetAction : uint8_t {
    SAVE,     // Preset saved to slot
    RECALL,   // Preset recalled from slot
    DELETE,   // Preset deleted from slot
    ERROR     // Operation failed
};

// Feedback callback signature
// @param slot Preset slot (0-7)
// @param action Action that was performed
// @param success Whether action succeeded
using PresetFeedbackCallback = std::function<void(uint8_t slot, PresetAction action, bool success)>;

class PresetManager {
public:
    /**
     * Constructor
     * @param paramHandler ParameterHandler for current state access
     * @param wsClient WebSocketClient for applying presets
     */
    PresetManager(ParameterHandler* paramHandler, WebSocketClient* wsClient);

    /**
     * Initialize preset manager and storage
     * @return true if initialization succeeded
     */
    bool init();

    // ========================================================================
    // Preset Operations
    // ========================================================================

    /**
     * Save current state to a preset slot
     * Captures state from ParameterHandler and stores in NVS
     * @param slot Slot index (0-7)
     * @return true if save succeeded
     */
    bool savePreset(uint8_t slot);

    /**
     * Recall preset from a slot and apply to device
     * Loads from NVS and sends commands via WebSocket
     * @param slot Slot index (0-7)
     * @return true if recall succeeded
     */
    bool recallPreset(uint8_t slot);

    /**
     * Delete preset from a slot
     * Clears slot in NVS
     * @param slot Slot index (0-7)
     * @return true if delete succeeded
     */
    bool deletePreset(uint8_t slot);

    // ========================================================================
    // State Query
    // ========================================================================

    /**
     * Check if a slot is occupied
     * @param slot Slot index (0-7)
     * @return true if slot contains a valid preset
     */
    bool isSlotOccupied(uint8_t slot) const;

    /**
     * Get occupancy mask for all slots
     * @return Bitmask where bit N indicates slot N is occupied
     */
    uint8_t getOccupancyMask() const;

    /**
     * Get number of occupied slots
     * @return Count of occupied slots (0-8)
     */
    uint8_t getOccupiedCount() const;

    /**
     * Get preset data from a slot (read-only)
     * @param slot Slot index (0-7)
     * @param preset PresetData to fill
     * @return true if slot was occupied and data valid
     */
    bool getPreset(uint8_t slot, PresetData& preset) const;

    // ========================================================================
    // Callback Registration
    // ========================================================================

    /**
     * Set feedback callback for preset operations
     * Called after save/recall/delete with result
     * @param callback Feedback function
     */
    void setFeedbackCallback(PresetFeedbackCallback callback) {
        _feedbackCallback = callback;
    }

    /**
     * Set ZoneComposerUI for zone state capture
     * @param zoneUI ZoneComposerUI instance
     */
    void setZoneComposerUI(ZoneComposerUI* zoneUI) {
        _zoneUI = zoneUI;
    }

    // ========================================================================
    // State Capture (for external use)
    // ========================================================================

    /**
     * Capture current state into a PresetData struct
     * Does not save to NVS - use savePreset() for that
     * @param preset PresetData to fill with current state
     */
    void captureCurrentState(PresetData& preset);

    /**
     * Apply preset state to device via WebSocket
     * Does not load from NVS - use recallPreset() for that
     * @param preset PresetData to apply
     * @return true if commands were sent (not guaranteed delivery)
     */
    bool applyPresetState(const PresetData& preset);

private:
    ParameterHandler* _paramHandler;
    WebSocketClient* _wsClient;
    ZoneComposerUI* _zoneUI = nullptr;
    PresetFeedbackCallback _feedbackCallback;

    // Cached occupancy mask (updated on init and after operations)
    mutable uint8_t _cachedOccupancy = 0;
    mutable bool _occupancyCacheValid = false;

    /**
     * Update cached occupancy mask
     */
    void updateOccupancyCache() const;

    /**
     * Send feedback to callback if registered
     */
    void sendFeedback(uint8_t slot, PresetAction action, bool success);
};

#endif // PRESET_MANAGER_H
