// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioNode.h
 * @brief Compatibility header - forwards to AudioActor.h
 *
 * This header provides backward compatibility for code that references
 * AudioNode (the old name) instead of AudioActor (the new name).
 */

#pragma once

#include "AudioActor.h"

namespace lightwaveos {
namespace audio {
    // Type alias for backward compatibility
    using AudioNode = AudioActor;
} // namespace audio
} // namespace lightwaveos
