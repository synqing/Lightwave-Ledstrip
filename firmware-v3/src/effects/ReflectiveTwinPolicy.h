/**
 * @file ReflectiveTwinPolicy.h
 * @brief Phase 3 Move 3.1 Reflective Twin contract guard.
 *
 * Legacy K1 effects render one 320-LED virtual panel. RendererActor then
 * mirrors that unified framebuffer to the two physical strips, so the LGP
 * reads as one centre-origin object.
 *
 * Dual-strip asymmetric effects are allowed, but only when they explicitly
 * declare EffectRoleFlags::DUAL_CHANNEL. This keeps accidental direct strip
 * writes from bypassing the Reflective Twin path.
 */

#pragma once

#include "plugins/api/IEffect.h"

namespace lightwaveos {
namespace effects {
namespace reflective_twin {

static inline bool hasRoleFlag(plugins::EffectRoleFlags flags,
                               plugins::EffectRoleFlags flag) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

static inline bool declaresDualChannel(const plugins::EffectMetadata& meta) {
    return hasRoleFlag(meta.roleFlags, plugins::EffectRoleFlags::DUAL_CHANNEL);
}

static inline bool allowDualChannel(const plugins::EffectMetadata& meta,
                                    bool requestedDualChannel) {
    return requestedDualChannel && declaresDualChannel(meta);
}

static inline bool shouldMirrorUnified(const plugins::EffectMetadata& meta,
                                       bool requestedDualChannel) {
    return !allowDualChannel(meta, requestedDualChannel);
}

}  // namespace reflective_twin
}  // namespace effects
}  // namespace lightwaveos
