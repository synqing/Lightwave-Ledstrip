/**
 * @file TransitionRuntimeConfig.h
 * @brief NVS-backed runtime controls for the transition engine.
 */

#pragma once

namespace lightwaveos {
namespace transitions {

bool transitionsEnabled();
bool setTransitionsEnabled(bool enabled);

} // namespace transitions
} // namespace lightwaveos
