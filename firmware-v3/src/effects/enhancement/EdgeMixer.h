/**
 * @file EdgeMixer.h
 * @brief Composable 3-stage post-processor for Light Guide Plate colour differentiation
 *
 * The EdgeMixer takes the rendered strip buffers and applies colour transforms
 * to Strip 2, creating perceived depth through the LGP. The engine composes
 * three independent stages:
 *
 *   Stage T (Temporal):  Per-frame modulation (static or RMS-gated)
 *   Stage C (Colour):    Per-pixel 3x3 RGB matrix transform (5 modes)
 *   Stage S (Spatial):   Per-pixel positional mask (uniform or centre gradient)
 *
 * Effective strength = scale8(scale8(globalStrength, temporalAmount), spatialAmount)
 * — multiplicative composition, mathematically sane, no ad hoc overrides.
 *
 * Stage C uses a pre-computed 3x3 linear RGB transformation matrix that
 * combines hue rotation (around the (1,1,1) grey axis) and desaturation
 * (linear blend toward luminance-weighted grey point). The matrix is computed
 * at config time and applied per-pixel as 9 integer multiply-adds in Q8.8
 * fixed-point. No HSV round-trip — deterministic, luminance-preserving.
 *
 * Performance: ~22us for 160 pixels (33 cycles/pixel at 240 MHz).
 * Thread safety: Call only from render thread (Core 1).
 */

#pragma once

#include <FastLED.h>
#include <Preferences.h>
#include <cmath>
#include <cstring>
#include "../../audio/contracts/ControlBus.h"

namespace lightwaveos {
namespace enhancement {

/**
 * @brief Edge mixer colour transform mode
 */
enum class EdgeMixerMode : uint8_t {
    MIRROR              = 0,  ///< No processing (default, current behaviour)
    ANALOGOUS           = 1,  ///< Hue shift +spread on Strip 2
    COMPLEMENTARY       = 2,  ///< 180deg hue shift with saturation reduction on Strip 2
    SPLIT_COMPLEMENTARY = 3,  ///< +150deg near-complement with lighter saturation
    SATURATION_VEIL     = 4,  ///< Desaturate Strip 2 by spread amount
    TRIADIC             = 5,  ///< 120deg hue shift; spread controls saturation (0=full, 60=70%)
    TETRADIC            = 6,  ///< 90deg hue shift; spread controls saturation (0=full, 60=70%)
    STM_DUAL            = 7   ///< Edge A follows temporal STM, Edge B follows spectral STM
};

/**
 * @brief Edge mixer spatial mask mode
 */
enum class EdgeMixerSpatial : uint8_t {
    UNIFORM         = 0,  ///< All pixels equal weight (default)
    CENTRE_GRADIENT = 1   ///< 0 at LED 79, 255 at edges
};

/**
 * @brief Edge mixer temporal modulation mode
 */
enum class EdgeMixerTemporal : uint8_t {
    STATIC   = 0,  ///< No modulation (default)
    RMS_GATE = 1   ///< Strength tracks smoothed audio RMS
};

/**
 * @class EdgeMixer
 * @brief Singleton post-processor for dual-edge LGP colour differentiation
 *
 * Inserted in showLeds() between silence gates and TAP C capture.
 * MIRROR mode returns immediately with zero overhead.
 */
class EdgeMixer {
public:
    static EdgeMixer& getInstance() {
        static EdgeMixer instance;
        return instance;
    }

    /**
     * @brief Apply 3-stage composable pipeline to strip buffer in-place
     *
     * All legacy modes only modify strip2. STM_DUAL modulates both strips.
     *
     * @param strip1 Pointer to LEDS_PER_STRIP (160) CRGB pixels for Edge A
     * @param strip2 Pointer to LEDS_PER_STRIP (160) CRGB pixels
     * @param count Number of pixels
     * @param audio Current audio state for temporal modulation
     */
    void process(CRGB* strip1, CRGB* strip2, uint16_t count, const audio::ControlBusFrame& audio) {
        if (m_mode == EdgeMixerMode::MIRROR || count == 0) {
            return;
        }

        if (m_mode == EdgeMixerMode::STM_DUAL) {
            processStmDual(strip1, strip2, count, audio);
            return;
        }

        if (!strip2) {
            return;
        }

        // Safety net: ensure matrix is current (should already be, but one
        // branch per frame is negligible insurance against stale state).
        if (m_matrixDirty) recomputeMatrix();

        // Stage T: Temporal modulation (per-frame)
        const uint8_t temporalAmount = computeTemporal(audio);
        const uint8_t effectiveGlobal = scale8(m_strength, temporalAmount);
        if (effectiveGlobal == 0) return;

        // Per-pixel: Stage C (colour transform) + Stage S (spatial mask)
        for (uint16_t i = 0; i < count; ++i) {
            // Near-black skip: the RGB matrix is well-defined at all values,
            // but blending near-black pixels wastes cycles and the threshold
            // of 2 prevents trail fragmentation from sub-pixel blended trails.
            const uint8_t maxC = (strip2[i].r > strip2[i].g)
                ? ((strip2[i].r > strip2[i].b) ? strip2[i].r : strip2[i].b)
                : ((strip2[i].g > strip2[i].b) ? strip2[i].g : strip2[i].b);
            if (maxC < 2) continue;

            // Stage C: Colour transform (3x3 RGB matrix, no HSV round-trip)
            CRGB target = computeTransform(strip2[i]);

            // Stage S: Spatial mask (per-pixel)
            const uint8_t spatialAmount = computeSpatial(i, count);
            const uint8_t pixelStrength = scale8(effectiveGlobal, spatialAmount);

            // Blend original → target by pixelStrength
            if (pixelStrength >= 255) {
                strip2[i] = target;
            } else if (pixelStrength > 0) {
                strip2[i] = blendPixel(strip2[i], target, pixelStrength);
            }
        }
    }

    // === Configuration ===

    void setMode(EdgeMixerMode mode) {
        m_mode = mode;
        recomputeMatrix();
    }
    EdgeMixerMode getMode() const { return m_mode; }

    void setSpatial(EdgeMixerSpatial spatial) { m_spatial = spatial; }
    EdgeMixerSpatial getSpatial() const { return m_spatial; }

    void setTemporal(EdgeMixerTemporal temporal) { m_temporal = temporal; }
    EdgeMixerTemporal getTemporal() const { return m_temporal; }

    /**
     * @brief Get human-readable name for a mode
     */
    static const char* modeName(EdgeMixerMode mode) {
        static const char* const names[] = {
            "mirror", "analogous", "complementary",
            "split_complementary", "saturation_veil",
            "triadic", "tetradic", "stm_dual"
        };
        const uint8_t idx = static_cast<uint8_t>(mode);
        return (idx <= 7) ? names[idx] : "unknown";
    }
    const char* modeName() const { return modeName(m_mode); }

    /**
     * @brief Get human-readable name for spatial mode
     */
    static const char* spatialName(EdgeMixerSpatial spatial) {
        static const char* const names[] = {"uniform", "centre_gradient"};
        const uint8_t idx = static_cast<uint8_t>(spatial);
        return (idx <= 1) ? names[idx] : "unknown";
    }
    const char* spatialName() const { return spatialName(m_spatial); }

    /**
     * @brief Get human-readable name for temporal mode
     */
    static const char* temporalName(EdgeMixerTemporal temporal) {
        static const char* const names[] = {"static", "rms_gate"};
        const uint8_t idx = static_cast<uint8_t>(temporal);
        return (idx <= 1) ? names[idx] : "unknown";
    }
    const char* temporalName() const { return temporalName(m_temporal); }

    /**
     * @brief Set analogous hue spread in degrees (0-60)
     *
     * Converted internally to FastLED 0-255 hue units.
     * Also computes m_satScale for Saturation Veil mode.
     * Triggers matrix recomputation.
     * Default: 30deg = 21 in FastLED units.
     */
    void setSpread(uint8_t spreadDegrees) {
        if (spreadDegrees > 60) spreadDegrees = 60;
        m_spreadDegrees = spreadDegrees;
        // Convert degrees to FastLED hue units: degrees * 256 / 360
        // Use integer maths to avoid float in config path
        m_spreadHue = static_cast<uint8_t>((static_cast<uint16_t>(spreadDegrees) * 256 + 180) / 360);
        // Saturation scale for veil mode: spread 0 → 255 (no change), spread 60 → 25 (near-greyscale)
        // Formula: 255 - (spreadDegrees * 230 / 60)
        m_satScale = static_cast<uint8_t>(255 - (static_cast<uint16_t>(spreadDegrees) * 230 / 60));
        recomputeMatrix();
    }
    uint8_t getSpread() const { return m_spreadDegrees; }

    /**
     * @brief Set mix strength (0-255)
     *
     * 0 = no effect (identical to MIRROR), 255 = full hue shift.
     * Intermediate values blend between original and shifted colour.
     */
    void setStrength(uint8_t strength) { m_strength = strength; }
    uint8_t getStrength() const { return m_strength; }

    // === NVS Persistence ===

    void saveToNVS() {
        Preferences prefs;
        if (prefs.begin("edgemixer", false)) {
            prefs.putUChar("mode", static_cast<uint8_t>(m_mode));
            prefs.putUChar("spread", m_spreadDegrees);
            prefs.putUChar("strength", m_strength);
            prefs.putUChar("spatial", static_cast<uint8_t>(m_spatial));
            prefs.putUChar("temporal", static_cast<uint8_t>(m_temporal));
            prefs.end();
        }
    }

    void loadFromNVS() {
        Preferences prefs;
        if (prefs.begin("edgemixer", true)) {
            uint8_t mode = prefs.getUChar("mode", 0);
            if (mode <= 7) {
                m_mode = static_cast<EdgeMixerMode>(mode);
            }
            setSpread(prefs.getUChar("spread", 30));
            m_strength = prefs.getUChar("strength", 255);
            uint8_t spatial = prefs.getUChar("spatial", 0);
            if (spatial <= 1) {
                m_spatial = static_cast<EdgeMixerSpatial>(spatial);
            }
            uint8_t temporal = prefs.getUChar("temporal", 0);
            if (temporal <= 1) {
                m_temporal = static_cast<EdgeMixerTemporal>(temporal);
            }
            prefs.end();
        }
        recomputeMatrix();
    }

private:
    EdgeMixer()
        : m_mode(EdgeMixerMode::MIRROR)
        , m_spatial(EdgeMixerSpatial::UNIFORM)
        , m_temporal(EdgeMixerTemporal::STATIC)
        , m_spreadDegrees(30)
        , m_spreadHue(21)    // 30deg in FastLED units: (30*256+180)/360 = 21
        , m_satScale(140)    // 255 - (30 * 230 / 60) = 140
        , m_strength(255)
        , m_rmsSmooth(0.0f)
        , m_matrixDirty(true)
    {
        recomputeMatrix();
    }

    EdgeMixer(const EdgeMixer&) = delete;
    EdgeMixer& operator=(const EdgeMixer&) = delete;

    // ========================================================================
    // Stage T: Temporal modulation (per-frame)
    //
    // POLICY — null-audio behaviour:
    //   STATIC mode:   temporalAmount = 255 always. Audio state is irrelevant.
    //                  Non-reactive modes MUST NOT be suppressed by null audio.
    //   RMS_GATE mode: temporalAmount tracks smoothed RMS with a floor of
    //                  kTemporalFloor (~6%). This prevents hard-kill during
    //                  silence which caused asymmetric strip behaviour: Strip 1
    //                  retains trails normally while Strip 2 snaps to unprocessed
    //                  colours when temporalAmount hits 0. The floor preserves
    //                  a minimal coherent transform during quiet passages.
    // ========================================================================

    static constexpr uint8_t kTemporalFloor = 15;  // ~6% — attenuate, don't kill

    uint8_t computeTemporal(const audio::ControlBusFrame& audio) {
        switch (m_temporal) {
            case EdgeMixerTemporal::RMS_GATE: {
                // Smooth RMS with ~55ms time constant at 120 FPS
                m_rmsSmooth += 0.15f * (audio.rms - m_rmsSmooth);
                // Map rms 0.0-0.3 → kTemporalFloor-255
                float scaled = m_rmsSmooth * 850.0f;
                if (scaled > 255.0f) scaled = 255.0f;
                uint8_t raw = static_cast<uint8_t>(scaled);
                return (raw < kTemporalFloor) ? kTemporalFloor : raw;
            }
            case EdgeMixerTemporal::STATIC:
            default:
                return 255;
        }
    }

    // ========================================================================
    // Stage S: Spatial mask (per-pixel)
    // ========================================================================

    /**
     * @brief Centre gradient LUT: 0 at index 76 (centre), ramps to 255 at edges
     *
     * Symmetric around index 76, saturates at 255 from index 154 outward.
     * Step size ~3.3 LSB/LED (alternating +3/+4 due to integer quantisation).
     * 160 bytes in flash, zero RAM.
     */
    static constexpr uint8_t kCentreGradient[160] = {
        251, 248, 244, 241, 238, 234, 231, 228, 224, 221,  //  0- 9
        218, 214, 211, 208, 204, 201, 198, 194, 191, 188,  // 10-19
        184, 181, 178, 174, 171, 168, 164, 161, 158, 154,  // 20-29
        151, 148, 144, 141, 138, 134, 131, 128, 124, 121,  // 30-39
        118, 114, 111, 108, 104, 101,  98,  94,  91,  88,  // 40-49
         84,  81,  78,  74,  71,  68,  64,  61,  58,  54,  // 50-59
         51,  48,  44,  41,  38,  35,  31,  28,  25,  22,  // 60-69
         19,  15,  12,   9,   6,   3,   0,   3,   6,   9,  // 70-79
         12,  15,  19,  22,  25,  28,  31,  35,  38,  41,  // 80-89
         44,  48,  51,  54,  58,  61,  64,  68,  71,  74,  // 90-99
         78,  81,  84,  88,  91,  94,  98, 101, 104, 108,  //100-109
        111, 114, 118, 121, 124, 128, 131, 134, 138, 141,  //110-119
        144, 148, 151, 154, 158, 161, 164, 168, 171, 174,  //120-129
        178, 181, 184, 188, 191, 194, 198, 201, 204, 208,  //130-139
        211, 214, 218, 221, 224, 228, 231, 234, 238, 241,  //140-149
        244, 248, 251, 254, 255, 255, 255, 255, 255, 255   //150-159
    };

    uint8_t computeSpatial(uint16_t i, uint16_t count) const {
        (void)count;
        switch (m_spatial) {
            case EdgeMixerSpatial::CENTRE_GRADIENT:
                return (i < 160) ? kCentreGradient[i] : 255;
            case EdgeMixerSpatial::UNIFORM:
            default:
                return 255;
        }
    }

    static uint8_t energyToScale(float energy) {
        if (energy <= 0.0f) return 0;
        if (energy >= 1.0f) return 255;
        return static_cast<uint8_t>(lroundf(energy * 255.0f));
    }

    uint8_t mixScaleWithStrength(uint8_t baseScale) const {
        if (m_strength == 255) return baseScale;
        if (m_strength == 0) return 255;
        const uint16_t attenuation = static_cast<uint16_t>(255U - baseScale) * static_cast<uint16_t>(m_strength);
        return static_cast<uint8_t>(255U - ((attenuation + 127U) / 255U));
    }

    static void scalePixelInPlace(CRGB& pixel, uint8_t scale) {
        pixel.r = scale8(pixel.r, scale);
        pixel.g = scale8(pixel.g, scale);
        pixel.b = scale8(pixel.b, scale);
    }

    static CRGB blendPixel(const CRGB& from, const CRGB& to, uint8_t amount) {
        if (amount == 0) return from;
        if (amount >= 255) return to;

        CRGB result;
        const uint16_t inverse = static_cast<uint16_t>(255U - amount);
        result.r = static_cast<uint8_t>(
            (static_cast<uint16_t>(from.r) * inverse + static_cast<uint16_t>(to.r) * amount + 127U) / 255U);
        result.g = static_cast<uint8_t>(
            (static_cast<uint16_t>(from.g) * inverse + static_cast<uint16_t>(to.g) * amount + 127U) / 255U);
        result.b = static_cast<uint8_t>(
            (static_cast<uint16_t>(from.b) * inverse + static_cast<uint16_t>(to.b) * amount + 127U) / 255U);
        return result;
    }

    void processStmDual(CRGB* strip1, CRGB* strip2, uint16_t count, const audio::ControlBusFrame& audio) {
        if (!audio.stmReady) {
            return;
        }

        const uint8_t temporalScale = mixScaleWithStrength(energyToScale(audio.stmTemporalEnergy));
        const uint8_t spectralScale = mixScaleWithStrength(energyToScale(audio.stmSpectralEnergy));

        if (strip1) {
            for (uint16_t i = 0; i < count; ++i) {
                scalePixelInPlace(strip1[i], temporalScale);
            }
        }

        if (strip2) {
            for (uint16_t i = 0; i < count; ++i) {
                scalePixelInPlace(strip2[i], spectralScale);
            }
        }
    }

    // ========================================================================
    // Stage C: Colour transform — 3x3 RGB matrix (per-pixel)
    //
    // Hue rotation is a rotation around the (1,1,1) grey axis in RGB space.
    // Desaturation is a linear blend toward the luminance-weighted grey point.
    // Both compose into a single 3x3 matrix, pre-computed at config time,
    // applied per-pixel as 9 int32 multiply-adds in Q8.8 fixed-point.
    //
    // No HSV round-trip. Deterministic. Luminance-preserving.
    // ~33 cycles/pixel vs ~86 for the old rgb2hsv→hsv2rgb path.
    // ========================================================================

    /**
     * @brief Apply pre-computed 3x3 RGB transform matrix to a pixel
     *
     * Q8.8 fixed-point: int32 accumulation avoids overflow (max intermediate
     * value = 256 * 255 * 3 = 195,840, well within int32 range).
     */
    CRGB computeTransform(const CRGB& pixel) const {
        const int32_t r = pixel.r;
        const int32_t g = pixel.g;
        const int32_t b = pixel.b;

        // Q8.8 matrix multiply with rounding bias (+128 = +0.5 before truncation)
        int32_t r_out = (m_matrix[0] * r + m_matrix[1] * g + m_matrix[2] * b + 128) >> 8;
        int32_t g_out = (m_matrix[3] * r + m_matrix[4] * g + m_matrix[5] * b + 128) >> 8;
        int32_t b_out = (m_matrix[6] * r + m_matrix[7] * g + m_matrix[8] * b + 128) >> 8;

        // Clamp to [0, 255]
        CRGB result;
        result.r = (r_out < 0) ? 0 : ((r_out > 255) ? 255 : static_cast<uint8_t>(r_out));
        result.g = (g_out < 0) ? 0 : ((g_out > 255) ? 255 : static_cast<uint8_t>(g_out));
        result.b = (b_out < 0) ? 0 : ((b_out > 255) ? 255 : static_cast<uint8_t>(b_out));
        return result;
    }

    /**
     * @brief Recompute the 3x3 RGB transform matrix for current mode and spread
     *
     * Called when mode or spread changes. Uses float maths (acceptable —
     * config changes are human-interaction-rate, not per-frame).
     *
     * Mathematics:
     *   Hue rotation H(theta) = rotation around (1,1,1) axis in RGB space
     *   Desaturation D(s) = linear blend toward luminance-weighted grey
     *   Combined M = D(s) x H(theta)
     *   Stored as Q8.8 fixed-point: entry_q8 = round(float_entry * 256)
     */
    void recomputeMatrix() {
        // Start with identity
        float mat[9] = {1, 0, 0,  0, 1, 0,  0, 0, 1};

        float theta = 0.0f;
        float satRetain = 1.0f;  // 1.0 = no desaturation

        switch (m_mode) {
            case EdgeMixerMode::ANALOGOUS:
                theta = static_cast<float>(m_spreadDegrees) * (M_PI / 180.0f);
                break;
            case EdgeMixerMode::COMPLEMENTARY:
                theta = M_PI;  // 180 degrees
                satRetain = 217.0f / 255.0f;  // 0.851
                break;
            case EdgeMixerMode::SPLIT_COMPLEMENTARY:
                theta = 150.0f * (M_PI / 180.0f);
                satRetain = 230.0f / 255.0f;  // 0.902
                break;
            case EdgeMixerMode::SATURATION_VEIL:
                // No hue rotation, desaturation only
                satRetain = static_cast<float>(m_satScale) / 255.0f;
                break;
            case EdgeMixerMode::TRIADIC:
                theta = 120.0f * (M_PI / 180.0f);
                satRetain = 1.0f - (static_cast<float>(m_spreadDegrees) / 60.0f) * 0.30f;
                break;
            case EdgeMixerMode::TETRADIC:
                theta = 90.0f * (M_PI / 180.0f);
                satRetain = 1.0f - (static_cast<float>(m_spreadDegrees) / 60.0f) * 0.30f;
                break;
            case EdgeMixerMode::MIRROR:
            default:
                // Identity — MIRROR early-returns in process(), but set
                // identity matrix anyway for safety.
                break;
        }

        // Build hue rotation matrix H(theta)
        if (theta != 0.0f) {
            const float cosT = cosf(theta);
            const float sinT = sinf(theta);
            const float oneMinusCos = 1.0f - cosT;
            const float third = oneMinusCos / 3.0f;
            const float sqrt13 = 0.57735026919f;  // sqrt(1/3)
            const float sinTerm = sqrt13 * sinT;

            mat[0] = cosT + third;           // H[0][0]
            mat[1] = third - sinTerm;        // H[0][1]
            mat[2] = third + sinTerm;        // H[0][2]
            mat[3] = third + sinTerm;        // H[1][0]
            mat[4] = cosT + third;           // H[1][1]
            mat[5] = third - sinTerm;        // H[1][2]
            mat[6] = third - sinTerm;        // H[2][0]
            mat[7] = third + sinTerm;        // H[2][1]
            mat[8] = cosT + third;           // H[2][2]
        }

        // Apply desaturation: M = D(s) x H
        if (satRetain < 1.0f) {
            // Perceived luminance weights (ITU-R BT.601)
            const float Lr = 0.299f;
            const float Lg = 0.587f;
            const float Lb = 0.114f;
            const float inv = 1.0f - satRetain;

            // D(s) matrix entries
            float d[9] = {
                satRetain + inv * Lr,  inv * Lg,              inv * Lb,
                inv * Lr,              satRetain + inv * Lg,  inv * Lb,
                inv * Lr,              inv * Lg,              satRetain + inv * Lb
            };

            // M = D x mat (3x3 multiply)
            float combined[9];
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    combined[row * 3 + col] =
                        d[row * 3 + 0] * mat[0 * 3 + col] +
                        d[row * 3 + 1] * mat[1 * 3 + col] +
                        d[row * 3 + 2] * mat[2 * 3 + col];
                }
            }
            memcpy(mat, combined, sizeof(mat));
        }

        // Convert to Q8.8 fixed-point
        for (int i = 0; i < 9; ++i) {
            m_matrix[i] = static_cast<int16_t>(roundf(mat[i] * 256.0f));
        }
        m_matrixDirty = false;
    }

    // Configuration (NVS-persisted)
    EdgeMixerMode     m_mode;
    EdgeMixerSpatial  m_spatial;
    EdgeMixerTemporal m_temporal;
    uint8_t m_spreadDegrees;  ///< User-facing spread in degrees (0-60)
    uint8_t m_spreadHue;      ///< Spread converted to FastLED hue units (0-42)
    uint8_t m_satScale;       ///< Saturation scale for veil mode (derived from spread)
    uint8_t m_strength;       ///< Mix strength (0-255)

    // Runtime (not persisted, Core 1 only — no mutex needed)
    float m_rmsSmooth;        ///< Smoothed RMS for temporal gating

    // Pre-computed 3x3 colour transform matrix in Q8.8 fixed-point.
    // Combines hue rotation + desaturation for the current mode/spread.
    // Computed at config time (setMode/setSpread), applied per-pixel.
    int16_t m_matrix[9];      ///< Row-major: [r_from_r, r_from_g, r_from_b, g_from_r, ...]
    bool m_matrixDirty;       ///< True when mode or spread changed, matrix needs recomputing
};

} // namespace enhancement
} // namespace lightwaveos
