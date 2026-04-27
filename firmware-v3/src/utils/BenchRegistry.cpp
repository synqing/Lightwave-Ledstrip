// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file BenchRegistry.cpp
 * @brief Surface 7 — runtime A/B toggle registry implementation + storage.
 *
 * See BenchRegistry.h for design rationale. This TU contains:
 *   - Static storage for the fixed-size descriptor array
 *   - The 8 Wave 1 toggle backing volatile bools (centralised here)
 *   - Linear-scan findByName
 *   - Static-init self-registration of all 8 toggles
 *
 * NO heap allocation at any point.
 */

#include "BenchRegistry.h"

#include <string.h>

#ifndef NATIVE_BUILD
#  include "Log.h"
#endif

namespace lightwaveos {
namespace bench {

// ---------------------------------------------------------------------------
// Centralised toggle storage — all Wave 1 backing volatile bools.
// ---------------------------------------------------------------------------
volatile bool g_bench_render_color_correction      = true;
volatile bool g_bench_render_async_rmt             = true;
volatile bool g_bench_render_dual_strip_parallel   = true;
volatile bool g_bench_audio_lookahead              = true;
volatile bool g_bench_audio_zone_agc               = true;
volatile bool g_bench_audio_chroma_zone_agc        = true;
volatile bool g_bench_effect_fade_to_black         = true;
volatile bool g_bench_effect_subpixel              = false;

// ---------------------------------------------------------------------------
// Registry static storage — zero-initialised at .bss reserve time.
// ---------------------------------------------------------------------------
ToggleDescriptor BenchRegistry::s_registry[BENCH_REGISTRY_MAX] = {};
size_t           BenchRegistry::s_count                        = 0;

void BenchRegistry::registerToggle(const ToggleDescriptor& desc) {
    if (s_count >= BENCH_REGISTRY_MAX) {
#ifndef NATIVE_BUILD
        LW_LOGW("[bench] Registry full (%u/%u); dropping '%s'",
                static_cast<unsigned>(s_count),
                static_cast<unsigned>(BENCH_REGISTRY_MAX),
                desc.name ? desc.name : "<null>");
#endif
        return;
    }
    s_registry[s_count] = desc;
    if (desc.value != nullptr) {
        *desc.value = desc.default_value;
    }
    ++s_count;
}

const ToggleDescriptor* BenchRegistry::findByName(const char* name) {
    if (name == nullptr) return nullptr;
    for (size_t i = 0; i < s_count; ++i) {
        if (s_registry[i].name != nullptr &&
            strcmp(s_registry[i].name, name) == 0) {
            return &s_registry[i];
        }
    }
    return nullptr;
}

size_t BenchRegistry::count() {
    return s_count;
}

const ToggleDescriptor& BenchRegistry::at(size_t i) {
    if (i >= s_count) i = (s_count == 0) ? 0 : (s_count - 1);
    return s_registry[i];
}

void BenchRegistry::resetAll() {
    for (size_t i = 0; i < s_count; ++i) {
        if (s_registry[i].value != nullptr) {
            *s_registry[i].value = s_registry[i].default_value;
        }
    }
}

} // namespace bench
} // namespace lightwaveos

// ---------------------------------------------------------------------------
// Static-init self-registration of all 8 Wave 1 toggles.
// Wave 2 will add per-consumer wiring for the visibility stubs.
// ---------------------------------------------------------------------------
BENCH_REGISTER_TOGGLE(
    "render.color_correction",
    ::lightwaveos::bench::g_bench_render_color_correction,
    true,
    "post-effect colour correction pipeline on/off (wired)")

BENCH_REGISTER_TOGGLE(
    "render.async_rmt",
    ::lightwaveos::bench::g_bench_render_async_rmt,
    true,
    "async vs sync FastLED show() (Wave 2: LedDriver disentangle)")

BENCH_REGISTER_TOGGLE(
    "render.dual_strip_parallel",
    ::lightwaveos::bench::g_bench_render_dual_strip_parallel,
    true,
    "parallel vs sequential RMT writes (Wave 2: LedDriver disentangle)")

BENCH_REGISTER_TOGGLE(
    "audio.lookahead",
    ::lightwaveos::bench::g_bench_audio_lookahead,
    true,
    "lookahead spike smoothing (Wave 2: AudioActor observer)")

BENCH_REGISTER_TOGGLE(
    "audio.zone_agc",
    ::lightwaveos::bench::g_bench_audio_zone_agc,
    true,
    "zone AGC on bands[] frequency response (Wave 2: AudioActor observer)")

BENCH_REGISTER_TOGGLE(
    "audio.chroma_zone_agc",
    ::lightwaveos::bench::g_bench_audio_chroma_zone_agc,
    true,
    "zone AGC on chroma[12] pitch-class field (Wave 2: AudioActor observer)")

BENCH_REGISTER_TOGGLE(
    "effect.fade_to_black",
    ::lightwaveos::bench::g_bench_effect_fade_to_black,
    true,
    "fadeToBlackBy on/off (opt-in per effect via FadeOverride.h helper)")

BENCH_REGISTER_TOGGLE(
    "effect.subpixel",
    ::lightwaveos::bench::g_bench_effect_subpixel,
    false,
    "SubpixelRenderer vs integer fallback (Wave 2: not yet implemented)")
