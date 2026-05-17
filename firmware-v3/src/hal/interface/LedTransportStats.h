#pragma once

#include <cstdint>

namespace lightwaveos {
namespace hal {

struct LedTransportTimingSample {
    uint32_t totalShowUs = 0;
    uint32_t outputPrepUs = 0;
    uint32_t fastLedShowCallUs = 0;
    uint32_t rmtFenceUs = 0;
    uint32_t latchWaitUs = 0;
};

struct LedDriverStats {
    uint32_t frameCount = 0;             ///< Total frames rendered
    uint32_t showSkips = 0;              ///< Frames skipped by RMT safety guard
    uint32_t lastShowUs = 0;             ///< Last show() duration in microseconds
    uint32_t avgShowUs = 0;              ///< Average show() duration
    uint32_t maxShowUs = 0;              ///< Maximum show() duration
    uint32_t lastOutputPrepUs = 0;       ///< Last driver-side TX buffer preparation duration
    uint32_t avgOutputPrepUs = 0;        ///< Average driver-side TX buffer preparation duration
    uint32_t lastFastLedShowCallUs = 0;  ///< Last FastLED.show() call-return duration
    uint32_t avgFastLedShowCallUs = 0;   ///< Average FastLED.show() call-return duration
    uint32_t lastRmtFenceUs = 0;         ///< Last post-show wire-time fence duration
    uint32_t avgRmtFenceUs = 0;          ///< Average post-show wire-time fence duration
    uint32_t lastLatchWaitUs = 0;        ///< Last pre-copy/pre-show latch wait duration
    uint32_t avgLatchWaitUs = 0;         ///< Average pre-copy/pre-show latch wait duration
    uint32_t ledShowFailures = 0;        ///< show() calls that failed before FastLED/RMT dispatch
    uint32_t rmtErrors = 0;              ///< Hardware RMT error count if exposed by backend
    uint32_t rmtUnderruns = 0;           ///< Hardware RMT underrun count if exposed by backend
    uint8_t currentBrightness = 0;       ///< Current brightness setting
};

inline uint32_t smoothLedTransportUs(uint32_t previous, uint32_t sample, uint32_t frameCount) {
    return (frameCount <= 1U) ? sample : ((previous * 7U + sample) / 8U);
}

inline void recordLedTransportSample(LedDriverStats& stats,
                                     const LedTransportTimingSample& sample) {
    stats.frameCount++;
    stats.lastShowUs = sample.totalShowUs;
    if (sample.totalShowUs > stats.maxShowUs) {
        stats.maxShowUs = sample.totalShowUs;
    }
    stats.avgShowUs = smoothLedTransportUs(stats.avgShowUs, sample.totalShowUs, stats.frameCount);

    stats.lastOutputPrepUs = sample.outputPrepUs;
    stats.avgOutputPrepUs = smoothLedTransportUs(stats.avgOutputPrepUs, sample.outputPrepUs, stats.frameCount);

    stats.lastFastLedShowCallUs = sample.fastLedShowCallUs;
    stats.avgFastLedShowCallUs = smoothLedTransportUs(stats.avgFastLedShowCallUs,
                                                      sample.fastLedShowCallUs,
                                                      stats.frameCount);

    stats.lastRmtFenceUs = sample.rmtFenceUs;
    stats.avgRmtFenceUs = smoothLedTransportUs(stats.avgRmtFenceUs, sample.rmtFenceUs, stats.frameCount);

    stats.lastLatchWaitUs = sample.latchWaitUs;
    stats.avgLatchWaitUs = smoothLedTransportUs(stats.avgLatchWaitUs, sample.latchWaitUs, stats.frameCount);
}

} // namespace hal
} // namespace lightwaveos
