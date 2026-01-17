#pragma once
// ============================================================================
// PresetBankWidget - Preset slot display widget
// ============================================================================
// Displays a single preset bank slot with visual state indication:
//   - EMPTY: Gray, dimmed appearance
//   - OCCUPIED: Blue, normal appearance with preset info
//   - ACTIVE: Green, highlighted (last recalled preset)
//   - SAVING: Yellow flash animation
//   - DELETING: Red flash animation
//
// Layout:
//   ┌────────────────────────────────────┐
//   │  PRESET 1                          │
//   │  ┌────────────────────────────┐    │
//   │  │      [STATE ICON]           │    │
//   │  │      EMPTY / Effect Name    │    │
//   │  └────────────────────────────┘    │
//   │  [Brightness bar if occupied]       │
//   └────────────────────────────────────┘
// ============================================================================

#include <M5GFX.h>
#include <Arduino.h>
#include "../Theme.h"
#include "../../config/Config.h"

// Preset slot visual state
enum class PresetSlotState : uint8_t {
    EMPTY,     // No preset stored
    OCCUPIED,  // Preset stored, not active
    ACTIVE,    // Preset stored and last recalled
    SAVING,    // Feedback: save in progress
    DELETING   // Feedback: delete in progress
};

class PresetBankWidget {
public:
    /**
     * Constructor
     * @param display Pointer to M5GFX display
     * @param x X position
     * @param y Y position
     * @param w Width
     * @param h Height
     * @param slotIndex Preset slot index (0-7)
     */
    PresetBankWidget(M5GFX* display, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t slotIndex);
    ~PresetBankWidget();

    /**
     * Set slot state (empty, occupied, active, etc.)
     */
    void setState(PresetSlotState state);

    /**
     * Set whether this slot is occupied
     */
    void setOccupied(bool occupied);

    /**
     * Set whether this slot is active (last recalled)
     */
    void setActive(bool active);

    /**
     * Set preset info for display (when occupied)
     * @param effectId Effect ID stored in preset
     * @param effectName Effect name (can be null)
     * @param brightness Brightness value stored in preset
     */
    void setPresetInfo(uint8_t effectId, const char* effectName, uint8_t brightness);

    /**
     * Show feedback animation for save action
     */
    void showSaveFeedback();

    /**
     * Show feedback animation for recall action
     */
    void showRecallFeedback();

    /**
     * Show feedback animation for delete action
     */
    void showDeleteFeedback();

    /**
     * Mark widget as needing redraw
     */
    void markDirty() { _dirty = true; }

    /**
     * Render the widget
     */
    void render();

    /**
     * Update animation state (call from loop)
     */
    void update();

    /**
     * Get slot index
     */
    uint8_t getSlotIndex() const { return _slotIndex; }

private:
    M5GFX* _display;
    M5Canvas _sprite;
    bool _spriteOk = false;

    int32_t _x, _y, _w, _h;
    uint8_t _slotIndex;

    PresetSlotState _state = PresetSlotState::EMPTY;
    bool _occupied = false;
    bool _active = false;
    bool _dirty = true;

    // Preset info
    uint8_t _effectId = 0;
    char _effectName[16] = {0};
    uint8_t _brightness = 128;

    // Animation state
    uint32_t _animStart = 0;
    static constexpr uint32_t FEEDBACK_DURATION_MS = 500;

    void drawBackground();
    void drawSlotNumber();
    void drawStateIndicator();
    void drawPresetInfo();
    void drawBrightnessBar();

    uint16_t getStateColor() const;
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline PresetBankWidget::PresetBankWidget(M5GFX* display, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t slotIndex)
    : _display(display)
    , _sprite(display)
    , _x(x)
    , _y(y)
    , _w(w)
    , _h(h)
    , _slotIndex(slotIndex)
{
    // Create sprite for flicker-free rendering
    _sprite.setColorDepth(16);
    _sprite.setPsram(true);
    _spriteOk = _sprite.createSprite(w, h);
    _sprite.setTextDatum(MC_DATUM);

#if ENABLE_UI_DIAGNOSTICS
    Serial.printf("[UI] PresetBankWidget sprite idx=%u ok=%u bytes=%u\n",
                  static_cast<unsigned>(slotIndex),
                  _spriteOk ? 1u : 0u,
                  static_cast<unsigned>(w * h * 2));
#endif
}

inline PresetBankWidget::~PresetBankWidget() {
    _sprite.deleteSprite();
}

inline void PresetBankWidget::setState(PresetSlotState state) {
    if (_state != state) {
        _state = state;
        _dirty = true;
    }
}

inline void PresetBankWidget::setOccupied(bool occupied) {
    if (_occupied != occupied) {
        _occupied = occupied;
        _state = occupied ? PresetSlotState::OCCUPIED : PresetSlotState::EMPTY;
        _dirty = true;
    }
}

inline void PresetBankWidget::setActive(bool active) {
    if (_active != active) {
        _active = active;
        if (_occupied && active) {
            _state = PresetSlotState::ACTIVE;
        } else if (_occupied) {
            _state = PresetSlotState::OCCUPIED;
        }
        _dirty = true;
    }
}

inline void PresetBankWidget::setPresetInfo(uint8_t effectId, const char* effectName, uint8_t brightness) {
    _effectId = effectId;
    _brightness = brightness;

    if (effectName && strlen(effectName) > 0) {
        strncpy(_effectName, effectName, sizeof(_effectName) - 1);
        _effectName[sizeof(_effectName) - 1] = '\0';
    } else {
        snprintf(_effectName, sizeof(_effectName), "Effect %d", effectId);
    }

    _dirty = true;
}

inline void PresetBankWidget::showSaveFeedback() {
    _state = PresetSlotState::SAVING;
    _animStart = millis();
    _dirty = true;
}

inline void PresetBankWidget::showRecallFeedback() {
    _active = true;
    _state = PresetSlotState::ACTIVE;
    _animStart = millis();
    _dirty = true;
}

inline void PresetBankWidget::showDeleteFeedback() {
    _state = PresetSlotState::DELETING;
    _animStart = millis();
    _dirty = true;
}

inline void PresetBankWidget::update() {
    if (_animStart > 0) {
        uint32_t elapsed = millis() - _animStart;
        if (elapsed >= FEEDBACK_DURATION_MS) {
            // Animation complete - restore normal state
            _animStart = 0;
            if (_state == PresetSlotState::SAVING) {
                _occupied = true;
                _state = PresetSlotState::ACTIVE;
            } else if (_state == PresetSlotState::DELETING) {
                _occupied = false;
                _active = false;
                _state = PresetSlotState::EMPTY;
            }
            _dirty = true;
        }
    }
}

inline uint16_t PresetBankWidget::getStateColor() const {
    switch (_state) {
        case PresetSlotState::EMPTY:     return Theme::PRESET_EMPTY;
        case PresetSlotState::OCCUPIED:  return Theme::PRESET_OCCUPIED;
        case PresetSlotState::ACTIVE:    return Theme::PRESET_ACTIVE;
        case PresetSlotState::SAVING:    return Theme::PRESET_SAVING;
        case PresetSlotState::DELETING:  return Theme::PRESET_DELETING;
        default:                         return Theme::PRESET_EMPTY;
    }
}

inline void PresetBankWidget::drawBackground() {
    uint16_t bgColor = Theme::BG_PANEL;
    uint16_t borderColor = getStateColor();

    // Fill background
    _sprite.fillRect(0, 0, _w, _h, bgColor);

    // Draw border with state color
    _sprite.drawRect(0, 0, _w, _h, borderColor);
    _sprite.drawRect(1, 1, _w - 2, _h - 2, borderColor);
}

inline void PresetBankWidget::drawSlotNumber() {
    // Slot label in top-left
    char label[12];
    snprintf(label, sizeof(label), "PRESET %d", _slotIndex + 1);

    _sprite.setTextColor(Theme::TEXT_DIM);
    _sprite.setTextSize(1);
    _sprite.setTextDatum(TL_DATUM);
    _sprite.drawString(label, 8, 6);
}

inline void PresetBankWidget::drawStateIndicator() {
    uint16_t color = getStateColor();
    int centerX = _w / 2;
    int centerY = _h / 2 - 10;

    if (_state == PresetSlotState::EMPTY) {
        // Draw empty circle
        _sprite.drawCircle(centerX, centerY, 20, color);
        _sprite.setTextColor(color);
        _sprite.setTextSize(1);
        _sprite.setTextDatum(MC_DATUM);
        _sprite.drawString("EMPTY", centerX, centerY + 35);
    } else {
        // Draw filled circle for occupied states
        _sprite.fillCircle(centerX, centerY, 20, color);

        // Draw icon based on state
        _sprite.setTextColor(Theme::BG_DARK);
        _sprite.setTextSize(2);
        _sprite.setTextDatum(MC_DATUM);

        if (_state == PresetSlotState::SAVING) {
            _sprite.drawString("S", centerX, centerY);
        } else if (_state == PresetSlotState::DELETING) {
            _sprite.drawString("X", centerX, centerY);
        } else {
            // Occupied or Active - show checkmark
            _sprite.drawString("*", centerX, centerY);
        }
    }
}

inline void PresetBankWidget::drawPresetInfo() {
    if (!_occupied) return;

    int centerX = _w / 2;

    // Effect name/ID
    _sprite.setTextColor(Theme::TEXT_BRIGHT);
    _sprite.setTextSize(1);
    _sprite.setTextDatum(MC_DATUM);
    _sprite.drawString(_effectName, centerX, _h / 2 + 25);
}

inline void PresetBankWidget::drawBrightnessBar() {
    if (!_occupied) return;

    // Simple brightness indicator bar at bottom
    int barY = _h - 20;
    int barH = 8;
    int barMargin = 10;
    int barW = _w - (barMargin * 2);

    // Background
    _sprite.fillRect(barMargin, barY, barW, barH, Theme::BG_DARK);

    // Fill based on brightness
    int fillW = (barW * _brightness) / 255;
    uint16_t fillColor = Theme::dimColor(getStateColor(), 180);
    _sprite.fillRect(barMargin, barY, fillW, barH, fillColor);
}

inline void PresetBankWidget::render() {
    if (!_dirty) return;
    _dirty = false;

    if (!_spriteOk || !_display) {
        return;
    }

    drawBackground();
    drawSlotNumber();
    drawStateIndicator();
    drawPresetInfo();
    drawBrightnessBar();

    // Push to display
    _sprite.pushSprite(_display, _x, _y);
}
