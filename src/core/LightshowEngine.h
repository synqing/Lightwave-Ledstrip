#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_EFFECTS && FEATURE_AUDIO_SYNC
#include <FastLED.h>
#include <functional>
#include <vector>
#include "audio/GoertzelUtils.h"
#include "config/hardware_config.h"

// -----------------------------------------------------------------------------
// LightshowEngine
// -----------------------------------------------------------------------------
// Converts the 96-bin musical Goertzel spectrum into LED colours using a pluggable
// colour-provider.  The engine performs these steps each frame:
//   1. Sample the latest bin magnitudes via GoertzelUtils.
//   2. Aggregate them into a chosen number of perceptual zones (logarithmic).
//   3. Map each LED to a zone (and optionally to an individual bin).
//   4. Query the colour provider for a base colour at that LED index.
//   5. Scale colour brightness by the zone (or bin) magnitude.
//
// Thread-safe: designed to be called from the render/effect callback on Core-1.
// -----------------------------------------------------------------------------
class LightshowEngine {
public:
    using ColorProvider = std::function<CRGB(uint16_t ledIdx, uint16_t ledCount, uint8_t zoneIdx, uint8_t zoneCount)>;

    // Configure engine.  Must be called once before the first update.
    void configure(uint8_t zoneCount, ColorProvider provider, bool logarithmic = true) {
        _zoneCount = zoneCount;
        _logarithmic = logarithmic;
        _colorProvider = provider;
        _zones.resize(zoneCount);
    }

    // Pull bins and compute zone amplitudes.  Call once per frame *before* render().
    void update() {
        const float* bins = GoertzelUtils::getBins96();
        if (!bins) return;  // Early startup

        GoertzelUtils::mapBinsToZones(_zoneCount, _zones.data(), _logarithmic);
    }

    // Render into provided LED buffer (length == STRIP_LENGTH).  Returns µs spent.
    uint32_t render(CRGB* leds, uint16_t ledCount) {
        uint32_t t0 = micros();
        if (!_colorProvider) return 0;

        for (uint16_t i = 0; i < ledCount; ++i) {
            float normPos = static_cast<float>(i) / ledCount;
            uint16_t zoneIdx = static_cast<uint16_t>(normPos * _zoneCount);
            if (zoneIdx >= _zoneCount) zoneIdx = _zoneCount - 1;

            CRGB base = _colorProvider(i, ledCount, zoneIdx, _zoneCount);
            float amp = _zones[zoneIdx];  // 0-1
            base.nscale8(static_cast<uint8_t>(amp * 255));
            leds[i] = base;
        }
        return micros() - t0;
    }

private:
    uint8_t _zoneCount = 16;
    bool _logarithmic = true;
    ColorProvider _colorProvider;
    std::vector<float> _zones;  // length == _zoneCount
}; 

#endif // FEATURE_AUDIO_EFFECTS && FEATURE_AUDIO_SYNC
