#pragma once

#include "hal/interface/ILedDriver.h"
#include "config/chip_config.h"

#ifndef NATIVE_BUILD
class CLEDController;
#endif

namespace lightwaveos {
namespace hal {

class LedDriver_P4 : public ILedDriver {
public:
    LedDriver_P4();
    ~LedDriver_P4() override = default;

    bool init(const LedStripConfig& config) override;
    bool initDual(const LedStripConfig& config1, const LedStripConfig& config2) override;
    void deinit() override;

    CRGB* getBuffer() override;
    CRGB* getBuffer(uint8_t stripIndex) override;

    uint16_t getTotalLedCount() const override;
    uint16_t getLedCount(uint8_t stripIndex) const override;

    void show() override;
    void setBrightness(uint8_t brightness) override;
    uint8_t getBrightness() const override;
    void setMaxPower(uint8_t volts, uint16_t milliamps) override;

    void clear(bool show = false) override;
    void fill(CRGB color, bool show = false) override;
    void setPixel(uint16_t index, CRGB color) override;

    bool isInitialized() const override { return m_initialized; }
    const LedDriverStats& getStats() const override { return m_stats; }
    void resetStats() override;

private:
    static constexpr uint16_t kMaxLedsPerStrip = 160;

    LedStripConfig m_config1{};
    LedStripConfig m_config2{};
    uint16_t m_stripCounts[2] = {0, 0};
    uint16_t m_totalLeds = 0;
    uint8_t m_brightness = 0;
    bool m_initialized = false;
    bool m_dual = false;

    CRGB m_strip1[kMaxLedsPerStrip];
    CRGB m_strip2[kMaxLedsPerStrip];

#ifndef NATIVE_BUILD
    CLEDController* m_ctrl1 = nullptr;
    CLEDController* m_ctrl2 = nullptr;
#endif

    LedDriverStats m_stats{};

    void updateShowStats(uint32_t showUs);
    void applyColorCorrection(const LedStripConfig& config);
};

} // namespace hal
} // namespace lightwaveos
