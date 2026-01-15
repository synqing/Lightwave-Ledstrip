/**
 * @file FeatureBus.h
 * @brief Feature bus for cross-core access (ControlBus-style)
 *
 * Single-writer snapshot for publishing AudioFeatureFrame.
 * No heap allocations.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include "K1Types.h"
#include <cstdint>

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Feature bus for cross-core feature frame access
 *
 * Single-writer, multi-reader snapshot pattern.
 * Thread-safe for ESP32-S3 dual-core access.
 */
class FeatureBus {
public:
    /**
     * @brief Constructor
     */
    FeatureBus();

    /**
     * @brief Publish feature frame (single writer)
     *
     * @param frame Feature frame to publish
     */
    void publish(const AudioFeatureFrame& frame);

    /**
     * @brief Get current feature frame snapshot (multi-reader)
     *
     * @param frame_out Output frame (snapshot copy)
     */
    void getSnapshot(AudioFeatureFrame* frame_out) const;

    /**
     * @brief Get pointer to current frame (read-only, volatile)
     *
     * WARNING: Frame may be updated by writer. Use getSnapshot() for safe access.
     */
    const AudioFeatureFrame* getCurrentFrame() const { return &m_frame; }

private:
    AudioFeatureFrame m_frame;           ///< Current feature frame
    uint32_t m_sequence;                  ///< Sequence number (for change detection)
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

