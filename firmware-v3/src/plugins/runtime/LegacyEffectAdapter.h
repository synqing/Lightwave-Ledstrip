// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LegacyEffectAdapter.h
 * @brief Adapter to wrap legacy function-pointer effects as IEffect instances
 * 
 * This allows all effects to use the IEffect path while legacy effects
 * are gradually migrated to native IEffect implementations.
 */

#pragma once

#include "../api/IEffect.h"
#include "../api/EffectContext.h"
#include "../../core/actors/RendererActor.h"

namespace lightwaveos {
namespace plugins {
namespace runtime {

/**
 * @brief Adapter that wraps a legacy function-pointer effect as an IEffect
 */
class LegacyEffectAdapter : public IEffect {
public:
    /**
     * @brief Construct adapter for a legacy effect
     * @param name Effect name
     * @param fn Legacy function pointer
     */
    LegacyEffectAdapter(const char* name, actors::EffectRenderFn fn);
    
    ~LegacyEffectAdapter() override = default;

    // IEffect interface
    bool init(EffectContext& ctx) override;
    void render(EffectContext& ctx) override;
    void cleanup() override;
    const EffectMetadata& getMetadata() const override;

private:
    const char* m_name;
    actors::EffectRenderFn m_fn;
    mutable EffectMetadata m_metadata;
    
    // Temporary RenderContext for conversion
    // Note: palette pointer is set directly from EffectContext each frame
    actors::RenderContext m_tempRenderContext;
};

} // namespace runtime
} // namespace plugins
} // namespace lightwaveos

