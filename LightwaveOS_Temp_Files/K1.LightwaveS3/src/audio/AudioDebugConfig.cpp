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
