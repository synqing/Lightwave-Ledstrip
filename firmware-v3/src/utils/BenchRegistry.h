// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file BenchRegistry.h
 * @brief Surface 7 — runtime A/B toggle registry for hypothesis testing.
 *
 * Allows agents to flip canonical/shipping behaviours on or off via the serial
 * CLI mid-session, fire trace markers in the same JSON timeline, and
 * post-process per-variant frame-time histograms.
 *
 * Design decisions (resolves Master OQ #6 and #7):
 *   - Backing variables are `volatile bool` (NOT std::atomic<bool>). On
 *     ESP32-S3 sequential consistency for naturally-aligned bool reads is
 *     implicit; volatile is zero-cost and sufficient for cross-core flag
 *     visibility. Switch to atomic only if write-ordering bugs surface.
 *   - Each consumer SNAPSHOTS the toggle into a local bool ONCE per
 *     frame/tick so a toggle changed mid-frame applies to the next frame,
 *     never the current frame. Prevents pipeline inconsistencies.
 *
 * Hot-path invariant: `*toggle_ptr` is a single volatile bool read. NO
 * string lookups, NO heap, NO map traversal. The registry is consulted only
 * by the `bench list/toggle/reset` serial commands, never from render() or
 * audio tick.
 *
 * The bench framework is ALWAYS live, regardless of FEATURE_MABUTRACE. Only
 * the trace-marker instants (bench_begin / bench_split / bench_end /
 * bench_toggle_set / bench_reset) are gated by FEATURE_MABUTRACE — they
 * compile to no-ops when tracing is disabled. The toggles themselves still
 * function in canonical (non-_trace) builds.
 *
 * Wave 1 catalogue:
 *   - render.color_correction (wired in RendererActor.cpp)
 *   - render.async_rmt (visibility stub — Wave 2 LedDriver disentangle)
 *   - render.dual_strip_parallel (visibility stub — Wave 2)
 *   - audio.lookahead (wired for ControlBus UpdateFromHop backends)
 *   - audio.zone_agc (wired for ControlBus UpdateFromHop backends)
 *   - audio.chroma_zone_agc (wired for ControlBus UpdateFromHop backends)
 *   - effect.fade_to_black (opt-in via FadeOverride.h helper)
 *   - effect.subpixel (visibility stub — SubpixelRenderer not yet integrated)
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace lightwaveos {
namespace bench {

// Maximum number of toggles registrable across the firmware. Fixed-size
// array; no heap allocation. Wave 1 catalogue is 8 entries.
constexpr size_t BENCH_REGISTRY_MAX = 32;

// ---------------------------------------------------------------------------
// Toggle descriptor — stored BY VALUE in the fixed-size registry array. The
// `value` pointer references the consumer-owned `volatile bool` backing
// variable. The registry never owns the storage — the consumer guarantees
// the lifetime is static.
// ---------------------------------------------------------------------------
struct ToggleDescriptor {
    const char*    name;          // string literal (e.g. "render.async_rmt")
    volatile bool* value;         // pointer to consumer's volatile bool
    bool           default_value; // canonical/shipping value
    const char*    description;   // shown by `bench list`
};

class BenchRegistry {
public:
    static void registerToggle(const ToggleDescriptor& desc);
    static const ToggleDescriptor* findByName(const char* name);
    static size_t count();
    static const ToggleDescriptor& at(size_t i);
    static void resetAll();

private:
    static ToggleDescriptor s_registry[BENCH_REGISTRY_MAX];
    static size_t           s_count;
};

// ---------------------------------------------------------------------------
// Hot-path helper — guaranteed inline, single volatile bool read. Consumers
// hold a `volatile bool*` snapshot at boot (the same pointer they passed to
// the macro) and call this helper once per frame/tick. NEVER pass the
// toggle name string here — that would force a linear scan in the hot path.
// ---------------------------------------------------------------------------
inline bool isToggleEnabled(const volatile bool* toggle_ptr) {
    return *toggle_ptr;
}

// ---------------------------------------------------------------------------
// Centralised toggle storage. All Wave 1 backing volatile bools live in
// BenchRegistry.cpp; consumers extern-reference here. Avoids per-consumer
// file-scope variable additions.
// ---------------------------------------------------------------------------
extern volatile bool g_bench_render_color_correction;
extern volatile bool g_bench_render_async_rmt;
extern volatile bool g_bench_render_dual_strip_parallel;
extern volatile bool g_bench_audio_lookahead;
extern volatile bool g_bench_audio_zone_agc;
extern volatile bool g_bench_audio_chroma_zone_agc;
extern volatile bool g_bench_effect_fade_to_black;
extern volatile bool g_bench_effect_subpixel;

// ---------------------------------------------------------------------------
// Static-init registration macro — invoke at TU scope (not inside a function).
// __LINE__ disambiguates multiple registrations per translation unit.
// ---------------------------------------------------------------------------
#define BENCH_REGISTER_TOGGLE_INNER(name_lit, var, def, desc, line) \
    namespace { \
        struct _BenchAutoReg_##line { \
            _BenchAutoReg_##line() { \
                ::lightwaveos::bench::ToggleDescriptor _td = { \
                    (name_lit), \
                    &(var), \
                    (def), \
                    (desc) \
                }; \
                ::lightwaveos::bench::BenchRegistry::registerToggle(_td); \
            } \
        }; \
        static _BenchAutoReg_##line _bench_auto_reg_##line; \
    }

#define BENCH_REGISTER_TOGGLE_PASTE(name, var, def, desc, line) \
    BENCH_REGISTER_TOGGLE_INNER(name, var, def, desc, line)

#define BENCH_REGISTER_TOGGLE(name, var, def, desc) \
    BENCH_REGISTER_TOGGLE_PASTE(name, var, def, desc, __LINE__)

} // namespace bench
} // namespace lightwaveos
