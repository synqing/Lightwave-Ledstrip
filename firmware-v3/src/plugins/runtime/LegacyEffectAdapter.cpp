// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LegacyEffectAdapter.cpp
 * @brief Implementation of LegacyEffectAdapter
 */

#include "LegacyEffectAdapter.h"
#include "../../effects/PatternRegistry.h"
#include <FastLED.h>

namespace lightwaveos {
namespace plugins {
namespace runtime {

LegacyEffectAdapter::LegacyEffectAdapter(const char* name, actors::EffectRenderFn fn)
    : m_name(name)
    , m_fn(fn)
{
    // Initialize metadata from PatternRegistry if available
    // For now, use basic metadata - PatternRegistry integration can be enhanced later
    m_metadata.name = m_name;
    m_metadata.description = "Legacy effect (function pointer)";
    m_metadata.category = EffectCategory::UNCATEGORIZED;
    m_metadata.version = 1;
    m_metadata.author = nullptr;
}

bool LegacyEffectAdapter::init(EffectContext& ctx) {
    // Legacy effects have no initialization - they're stateless functions
    // Palette pointer is set fresh each frame in render() to ensure it's current
    return true;
}

void LegacyEffectAdapter::render(EffectContext& ctx) {
    if (m_fn == nullptr) return;
    
    // Get palette pointer from context - MUST update every frame in case palette changed
    // PaletteRef stores const CRGBPalette16*, but RenderContext needs non-const
    const CRGBPalette16* palettePtr = ctx.palette.isValid() ? ctx.palette.getRaw() : nullptr;
    
    // Convert EffectContext to RenderContext
    m_tempRenderContext.leds = ctx.leds;
    m_tempRenderContext.numLeds = ctx.ledCount;
    m_tempRenderContext.brightness = ctx.brightness;
    m_tempRenderContext.speed = ctx.speed;
    m_tempRenderContext.hue = ctx.gHue;
    m_tempRenderContext.intensity = ctx.intensity;
    m_tempRenderContext.saturation = ctx.saturation;
    m_tempRenderContext.complexity = ctx.complexity;
    m_tempRenderContext.variation = ctx.variation;
    m_tempRenderContext.frameCount = ctx.frameNumber;
    m_tempRenderContext.deltaTimeMs = ctx.deltaTimeMs;
    // Set palette pointer directly - legacy effects expect CRGBPalette16*
    // We need to cast away const because RenderContext uses non-const pointer
    // but the palette is actually mutable (it's m_currentPalette in RendererActor)
    m_tempRenderContext.palette = const_cast<CRGBPalette16*>(palettePtr);
    
    // Call legacy function
    m_fn(m_tempRenderContext);
}

void LegacyEffectAdapter::cleanup() {
    // Legacy effects have no cleanup
}

const EffectMetadata& LegacyEffectAdapter::getMetadata() const {
    // Try to enhance metadata from PatternRegistry if available
    // For now, return basic metadata
    return m_metadata;
}

} // namespace runtime
} // namespace plugins
} // namespace lightwaveos

