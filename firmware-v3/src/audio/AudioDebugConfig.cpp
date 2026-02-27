// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioDebugConfig.cpp
 * @brief Audio debug verbosity configuration singleton
 */

#include "AudioDebugConfig.h"

namespace lightwaveos {
namespace audio {

static AudioDebugConfig s_audioDebugConfig;

AudioDebugConfig& getAudioDebugConfig() {
    return s_audioDebugConfig;
}

} // namespace audio
} // namespace lightwaveos
