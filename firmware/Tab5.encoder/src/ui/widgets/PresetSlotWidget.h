#pragma once
// ============================================================================
// PresetSlotWidget - Compact preset bank indicator (160x80)
// ============================================================================
// Displays preset bank status with Effect ID + Palette ID.
// Designed to sit below the gauge row, matching ENC-B physical position.
//
// Layout:
//   ┌──────────────────────────────────┐
//   │  P1              [●] OCCUPIED    │  (Slot # + state)
//   │  ────────────────────────────────│
//   │  E:42  P:15                      │  (Effect + Palette IDs)
//   │  [████████████░░░░░░] 180        │  (Brightness bar + value)
//   └──────────────────────────────────┘
// ============================================================================

#include <M5GFX.h>
#include "../Theme.h"

// Preset slot visual state (reuse from PresetBankWidget if needed)
enum class PresetSlotState : uint8_t {
    EMPTY,     // No preset stored
    OCCUPIED,  // Preset stored, not active
    ACTIVE,    // Preset stored and last recalled
    SAVING,    // Feedback: save in progress
    DELETING   // Feedback: delete in progress
};

class PresetSlotWidget {
public:
    PresetSlotWidget(M5GFX* display, int32_t x, int32_t y, uint8_t slotIndex);
    ~PresetSlotWidget();

    // State management
    void setState(PresetSlotState state);
    void setOccupied(bool occupied);
    void setActive(bool active);

    // Preset data - compact: just IDs + brightness
    void setPresetInfo(uint8_t effectId, uint8_t paletteId, uint8_t brightness);

    // Animation triggers
    void showSaveFeedback();
    void showRecallFeedback();
    void showDeleteFeedback();

    // Rendering
    void markDirty() { _dirty = true; }
    void render();
    void update();  // Call from loop for animations

    uint8_t getSlotIndex() const { return _slotIndex; }

private:
    M5GFX* _display;
    M5Canvas _sprite;

    int32_t _x, _y;
    uint8_t _slotIndex;

    PresetSlotState _state = PresetSlotState::EMPTY;
    bool _occupied = false;
    bool _active = false;
    bool _dirty = true;

    // Preset data
    uint8_t _effectId = 0;
    uint8_t _paletteId = 0;
    uint8_t _brightness = 128;

    // Animation
    uint32_t _animStart = 0;
    static constexpr uint32_t FEEDBACK_DURATION_MS = 500;

    void drawBackground();
    void drawHeader();
    void drawPresetInfo();
    void drawBrightnessBar();

    uint16_t getStateColor() const;
    const char* getStateLabel() const;
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline PresetSlotWidget::PresetSlotWidget(M5GFX* display, int32_t x, int32_t y, uint8_t slotIndex)
    : _display(display)
    , _x(x)
    , _y(y)
    , _slotIndex(slotIndex)
{
    _sprite.createSprite(Theme::PRESET_SLOT_W, Theme::PRESET_SLOT_H);
}

inline PresetSlotWidget::~PresetSlotWidget() {
    _sprite.deleteSprite();
}

inline void PresetSlotWidget::setState(PresetSlotState state) {
    if (_state != state) {
        _state = state;
        _dirty = true;
    }
}

inline void PresetSlotWidget::setOccupied(bool occupied) {
    if (_occupied != occupied) {
        _occupied = occupied;
        _state = occupied ? PresetSlotState::OCCUPIED : PresetSlotState::EMPTY;
        _dirty = true;
    }
}

inline void PresetSlotWidget::setActive(bool active) {
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

inline void PresetSlotWidget::setPresetInfo(uint8_t effectId, uint8_t paletteId, uint8_t brightness) {
    bool changed = (_effectId != effectId) || (_paletteId != paletteId) || (_brightness != brightness);
    _effectId = effectId;
    _paletteId = paletteId;
    _brightness = brightness;
    if (changed) _dirty = true;
}

inline void PresetSlotWidget::showSaveFeedback() {
    _state = PresetSlotState::SAVING;
    _animStart = millis();
    _dirty = true;
}

inline void PresetSlotWidget::showRecallFeedback() {
    _active = true;
    _state = PresetSlotState::ACTIVE;
    _animStart = millis();
    _dirty = true;
}

inline void PresetSlotWidget::showDeleteFeedback() {
    _state = PresetSlotState::DELETING;
    _animStart = millis();
    _dirty = true;
}

inline void PresetSlotWidget::update() {
    if (_animStart > 0) {
        uint32_t elapsed = millis() - _animStart;
        if (elapsed >= FEEDBACK_DURATION_MS) {
            _animStart = 0;
            if (_state == PresetSlotState::SAVING) {
                _occupied = true;
                _state = PresetSlotState::ACTIVE;
            } else if (_state == PresetSlotState::DELETING) {
                _occupied = false;
                _active = false;
                _state = PresetSlotState::EMPTY;
                _effectId = 0;
                _paletteId = 0;
                _brightness = 0;
            }
            _dirty = true;
        }
    }
}

inline uint16_t PresetSlotWidget::getStateColor() const {
    switch (_state) {
        case PresetSlotState::EMPTY:     return Theme::PRESET_EMPTY;
        case PresetSlotState::OCCUPIED:  return Theme::PRESET_OCCUPIED;
        case PresetSlotState::ACTIVE:    return Theme::PRESET_ACTIVE;
        case PresetSlotState::SAVING:    return Theme::PRESET_SAVING;
        case PresetSlotState::DELETING:  return Theme::PRESET_DELETING;
        default:                         return Theme::PRESET_EMPTY;
    }
}

inline const char* PresetSlotWidget::getStateLabel() const {
    switch (_state) {
        case PresetSlotState::EMPTY:     return "EMPTY";
        case PresetSlotState::OCCUPIED:  return "SAVED";
        case PresetSlotState::ACTIVE:    return "ACTIVE";
        case PresetSlotState::SAVING:    return "SAVING";
        case PresetSlotState::DELETING:  return "DELETE";
        default:                         return "";
    }
}

inline void PresetSlotWidget::drawBackground() {
    uint16_t borderColor = getStateColor();
    uint16_t bgColor = Theme::BG_PANEL;

    // Slightly brighter background for active state
    if (_state == PresetSlotState::ACTIVE) {
        bgColor = Theme::dimColor(Theme::PRESET_ACTIVE, 30);
    }

    _sprite.fillRect(0, 0, Theme::PRESET_SLOT_W, Theme::PRESET_SLOT_H, bgColor);

    // 2-pixel border
    _sprite.drawRect(0, 0, Theme::PRESET_SLOT_W, Theme::PRESET_SLOT_H, borderColor);
    _sprite.drawRect(1, 1, Theme::PRESET_SLOT_W - 2, Theme::PRESET_SLOT_H - 2, borderColor);
}

inline void PresetSlotWidget::drawHeader() {
    uint16_t stateColor = getStateColor();

    // Slot number (left side)
    char slotLabel[4];
    snprintf(slotLabel, sizeof(slotLabel), "P%d", _slotIndex + 1);
    _sprite.setTextColor(Theme::TEXT_BRIGHT);
    _sprite.setTextSize(1);
    _sprite.setTextDatum(TL_DATUM);
    _sprite.drawString(slotLabel, 6, 6);

    // State indicator circle + label (right side)
    int circleX = Theme::PRESET_SLOT_W - 50;
    int circleY = 12;

    if (_state == PresetSlotState::EMPTY) {
        // Empty: outline circle
        _sprite.drawCircle(circleX, circleY, 6, stateColor);
    } else {
        // Filled circle
        _sprite.fillCircle(circleX, circleY, 6, stateColor);
    }

    // State label
    _sprite.setTextColor(stateColor);
    _sprite.setTextDatum(TL_DATUM);
    _sprite.drawString(getStateLabel(), circleX + 10, 6);

    // Separator line
    _sprite.drawLine(4, 22, Theme::PRESET_SLOT_W - 4, 22, Theme::dimColor(stateColor, 100));
}

inline void PresetSlotWidget::drawPresetInfo() {
    int centerY = 38;

    if (!_occupied) {
        // Empty slot - show dashes
        _sprite.setTextColor(Theme::TEXT_DIM);
        _sprite.setTextSize(1);
        _sprite.setTextDatum(MC_DATUM);
        _sprite.drawString("--", Theme::PRESET_SLOT_W / 2, centerY);
        return;
    }

    // Effect ID and Palette ID
    char infoStr[16];
    snprintf(infoStr, sizeof(infoStr), "E:%d  P:%d", _effectId, _paletteId);

    _sprite.setTextColor(Theme::TEXT_BRIGHT);
    _sprite.setTextSize(1);
    _sprite.setTextDatum(MC_DATUM);
    _sprite.drawString(infoStr, Theme::PRESET_SLOT_W / 2, centerY);
}

inline void PresetSlotWidget::drawBrightnessBar() {
    int barY = Theme::PRESET_SLOT_H - 20;
    int barH = 10;
    int margin = 8;
    int barW = Theme::PRESET_SLOT_W - margin * 2 - 30;  // Leave room for value

    // Bar background
    _sprite.fillRect(margin, barY, barW, barH, Theme::BG_DARK);

    if (_occupied) {
        // Filled portion
        int fillW = (barW * _brightness) / 255;
        uint16_t fillColor = Theme::dimColor(getStateColor(), 200);
        _sprite.fillRect(margin, barY, fillW, barH, fillColor);

        // Brightness value
        char valStr[8];
        snprintf(valStr, sizeof(valStr), "%d", _brightness);
        _sprite.setTextColor(Theme::TEXT_DIM);
        _sprite.setTextSize(1);
        _sprite.setTextDatum(TL_DATUM);
        _sprite.drawString(valStr, margin + barW + 4, barY);
    }

    // Bar outline
    _sprite.drawRect(margin, barY, barW, barH, Theme::dimColor(getStateColor(), 80));
}

inline void PresetSlotWidget::render() {
    if (!_dirty) return;
    _dirty = false;

    drawBackground();
    drawHeader();
    drawPresetInfo();
    drawBrightnessBar();

    _sprite.pushSprite(_display, _x, _y);
}
