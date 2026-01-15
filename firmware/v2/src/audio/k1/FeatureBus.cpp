/**
 * @file FeatureBus.cpp
 * @brief Feature bus implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "FeatureBus.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace lightwaveos {
namespace audio {
namespace k1 {

FeatureBus::FeatureBus()
    : m_sequence(0)
{
    memset(&m_frame, 0, sizeof(m_frame));
}

void FeatureBus::publish(const AudioFeatureFrame& frame) {
    // Single-writer: direct copy (no lock needed if writer is single-threaded)
    // For ESP32-S3, writer is on Core 0, readers on Core 1
    // Use memcpy for atomic copy of struct
    memcpy(&m_frame, &frame, sizeof(AudioFeatureFrame));
    m_sequence++;
}

void FeatureBus::getSnapshot(AudioFeatureFrame* frame_out) const {
    if (frame_out == nullptr) {
        return;
    }

    // Multi-reader: copy snapshot
    memcpy(frame_out, &m_frame, sizeof(AudioFeatureFrame));
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

