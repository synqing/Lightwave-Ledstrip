/**
 * @file InputMergeLayer.h
 * @brief P1 Input Merge Layer — arbitrates multiple input sources onto shared parameters.
 *
 * Four sources (MANUAL, AUDIO, AI_AGENT, GESTURE) write to 10 shared parameters.
 * Each parameter has a merge mode (HTP or LTP) and per-source IIR smoothing.
 * Stale sources are automatically excluded via configurable timeouts.
 *
 * Header-only, zero heap allocation, ~400 bytes RAM total.
 *
 * @note Gated by FEATURE_INPUT_MERGE_LAYER
 */

#pragma once

#if FEATURE_INPUT_MERGE_LAYER

#include <cstdint>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace merge {

// ============================================================================
// Constants
// ============================================================================

static constexpr uint8_t kParamCount = 10;
static constexpr uint8_t kSourceCount = 4;

// Parameter indices
static constexpr uint8_t kParamBrightness  = 0;
static constexpr uint8_t kParamSpeed       = 1;
static constexpr uint8_t kParamIntensity   = 2;
static constexpr uint8_t kParamSaturation  = 3;
static constexpr uint8_t kParamComplexity  = 4;
static constexpr uint8_t kParamVariation   = 5;
static constexpr uint8_t kParamHue         = 6;
static constexpr uint8_t kParamMood        = 7;
static constexpr uint8_t kParamFadeAmount  = 8;
static constexpr uint8_t kParamPaletteIdx  = 9;

// ============================================================================
// Enums
// ============================================================================

enum class SourceId : uint8_t {
    MANUAL   = 0,
    AUDIO    = 1,
    AI_AGENT = 2,
    GESTURE  = 3
};

enum class MergeMode : uint8_t {
    HTP,  // Highest Takes Precedence
    LTP   // Latest Takes Precedence
};

// ============================================================================
// Source descriptor (compile-time config)
// ============================================================================

struct SourceConfig {
    uint8_t  priority;       // Higher = wins LTP ties and HTP weight
    uint32_t staleTimeoutMs; // 0 = never stale
    float    tauSeconds;     // IIR time constant (0 = passthrough)
};

// Per-source, per-parameter runtime state
struct SourceParamState {
    uint8_t  rawValue;       // Last written value
    float    smoothedValue;  // IIR-filtered value
    bool     written;        // Has this param been written at least once?
};

// Per-source runtime state
struct SourceState {
    SourceParamState params[kParamCount];
    uint32_t lastUpdateMs;   // Timestamp of last updateSource/updateSourceAll
    bool     valid;          // False when stale
};

// ============================================================================
// InputMergeLayer
// ============================================================================

class InputMergeLayer {
public:
    void init() {
        memset(m_sources, 0, sizeof(m_sources));
        memset(m_merged, 0, sizeof(m_merged));

        // Default merge modes:
        //   HTP: brightness(0), intensity(2), saturation(3)
        //   LTP: everything else
        for (uint8_t i = 0; i < kParamCount; ++i) {
            m_mergeMode[i] = MergeMode::LTP;
        }
        m_mergeMode[kParamBrightness] = MergeMode::HTP;
        m_mergeMode[kParamIntensity]  = MergeMode::HTP;
        m_mergeMode[kParamSaturation] = MergeMode::HTP;

        // Mark MANUAL as always valid
        m_sources[static_cast<uint8_t>(SourceId::MANUAL)].valid = true;
    }

    /// Update a single parameter from a source
    void updateSource(SourceId src, uint8_t paramIndex, uint8_t value) {
        if (paramIndex >= kParamCount) return;
        uint8_t si = static_cast<uint8_t>(src);
        if (si >= kSourceCount) return;

        auto& s = m_sources[si];
        s.params[paramIndex].rawValue = value;
        s.params[paramIndex].written = true;
        s.lastUpdateMs = m_lastNowMs;  // Use most recent known time
        s.valid = true;
    }

    /// Update all 10 parameters from a source in one call
    void updateSourceAll(SourceId src, const uint8_t values[kParamCount]) {
        uint8_t si = static_cast<uint8_t>(src);
        if (si >= kSourceCount) return;

        auto& s = m_sources[si];
        for (uint8_t i = 0; i < kParamCount; ++i) {
            s.params[i].rawValue = values[i];
            s.params[i].written = true;
        }
        s.lastUpdateMs = m_lastNowMs;
        s.valid = true;
    }

    /// Run the merge pass: staleness check, IIR smoothing, mode-based merge
    void merge(uint32_t nowMs, float dtSeconds) {
        m_lastNowMs = nowMs;

        // --- Phase 1: Staleness check ---
        for (uint8_t si = 0; si < kSourceCount; ++si) {
            const auto& cfg = kSourceConfigs[si];
            auto& s = m_sources[si];

            if (cfg.staleTimeoutMs == 0) {
                // Never stale (MANUAL)
                continue;
            }
            if (s.valid && (nowMs - s.lastUpdateMs) > cfg.staleTimeoutMs) {
                s.valid = false;
            }
        }

        // --- Phase 2: IIR smoothing per source per parameter ---
        for (uint8_t si = 0; si < kSourceCount; ++si) {
            const auto& cfg = kSourceConfigs[si];
            auto& s = m_sources[si];
            if (!s.valid) continue;

            for (uint8_t pi = 0; pi < kParamCount; ++pi) {
                auto& p = s.params[pi];
                if (!p.written) continue;

                float target = static_cast<float>(p.rawValue);

                if (cfg.tauSeconds <= 0.0f || dtSeconds <= 0.0f) {
                    // Passthrough — no smoothing
                    p.smoothedValue = target;
                } else {
                    float alpha = 1.0f - expf(-dtSeconds / cfg.tauSeconds);
                    p.smoothedValue += alpha * (target - p.smoothedValue);
                }
            }
        }

        // --- Phase 3: Per-parameter merge ---
        for (uint8_t pi = 0; pi < kParamCount; ++pi) {
            if (m_mergeMode[pi] == MergeMode::HTP) {
                mergeHTP(pi);
            } else {
                mergeLTP(pi);
            }
        }
    }

    /// Get the final merged value for a parameter
    uint8_t getMerged(uint8_t paramIndex) const {
        if (paramIndex >= kParamCount) return 0;
        return m_merged[paramIndex];
    }

    /// Override merge mode for a parameter at runtime
    void setMergeMode(uint8_t paramIndex, MergeMode mode) {
        if (paramIndex >= kParamCount) return;
        m_mergeMode[paramIndex] = mode;
    }

private:
    // HTP: highest smoothed value among valid sources wins
    void mergeHTP(uint8_t pi) {
        float best = 0.0f;
        bool any = false;

        for (uint8_t si = 0; si < kSourceCount; ++si) {
            const auto& s = m_sources[si];
            if (!s.valid || !s.params[pi].written) continue;

            float v = s.params[pi].smoothedValue;
            if (!any || v > best) {
                best = v;
                any = true;
            }
        }

        if (any) {
            m_merged[pi] = clampU8(best);
        }
    }

    // LTP: valid source with highest priority wins (priority as tiebreaker for "latest")
    // Among valid sources, the one updated most recently wins.
    // If timestamps match, higher priority wins.
    void mergeLTP(uint8_t pi) {
        uint32_t latestMs = 0;
        uint8_t  bestPriority = 0;
        float    bestValue = 0.0f;
        bool     any = false;

        for (uint8_t si = 0; si < kSourceCount; ++si) {
            const auto& s = m_sources[si];
            if (!s.valid || !s.params[pi].written) continue;

            bool wins = false;
            if (!any) {
                wins = true;
            } else if (s.lastUpdateMs > latestMs) {
                wins = true;
            } else if (s.lastUpdateMs == latestMs &&
                       kSourceConfigs[si].priority > bestPriority) {
                wins = true;
            }

            if (wins) {
                latestMs = s.lastUpdateMs;
                bestPriority = kSourceConfigs[si].priority;
                bestValue = s.params[pi].smoothedValue;
                any = true;
            }
        }

        if (any) {
            m_merged[pi] = clampU8(bestValue);
        }
    }

    static uint8_t clampU8(float v) {
        if (v <= 0.0f) return 0;
        if (v >= 255.0f) return 255;
        return static_cast<uint8_t>(v + 0.5f);
    }

    // --- Source configurations (compile-time) ---
    // Index matches SourceId enum values
    static constexpr SourceConfig kSourceConfigs[kSourceCount] = {
        { /* MANUAL   */ 50,  0,     0.0f  },  // priority=50,  never stale, no smoothing
        { /* AUDIO    */ 100, 200,   0.0f  },  // priority=100, 200ms stale, no smoothing
        { /* AI_AGENT */ 80,  10000, 2.0f  },  // priority=80,  10s stale,   tau=2.0s
        { /* GESTURE  */ 150, 500,   0.15f },  // priority=150, 500ms stale, tau=0.15s
    };

    // --- Runtime state ---
    SourceState m_sources[kSourceCount];   // ~4 * (10 * 6 + 5) = ~260 bytes
    uint8_t     m_merged[kParamCount];     // 10 bytes
    MergeMode   m_mergeMode[kParamCount];  // 10 bytes
    uint32_t    m_lastNowMs = 0;           // 4 bytes
    // Total: ~284 bytes (well under 400 byte budget)
};

// Static constexpr member definition (required for C++14 ODR-use)
constexpr SourceConfig InputMergeLayer::kSourceConfigs[kSourceCount];

} // namespace merge
} // namespace lightwaveos

#endif // FEATURE_INPUT_MERGE_LAYER
