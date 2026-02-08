/**
 * @file EffectPostProcessor.h
 * @brief Universal post-processing layer for LED effects
 *
 * Provides the visual enhancements that make LED visualisations "pop":
 *   1. Temporal blending - Trails via 90-95% previous frame retention
 *   2. Dynamic fade - Fade amount scales with audio amplitude
 *   3. Bloom - Box blur + additive blend for glow
 *   4. Motion blur - Smooth position transitions
 *
 * Based on analysis of LightwaveOS_Official reference implementation which uses:
 *   - CRGB16 (float 0-1) for accumulation without clipping
 *   - Multiple buffers (leds_16_prev, leds_16_fx, leds_16_temp)
 *   - Post-processing pipeline after each mode renders
 *   - apply_enhanced_visuals() function for bloom and wave modulation
 *
 * Architecture:
 * This processor can be integrated at two levels:
 *   1. Per-effect: Effect holds its own processor instance
 *   2. Global: RendererActor applies to all effects after render()
 *
 * The global approach is recommended for consistent visual quality across
 * all effect families without modifying each effect individually.
 *
 * IMPORTANT: Include <FastLED.h> before this header on embedded builds.
 * For native/test builds, include the fastled mock first.
 *
 * @note Operates on uint8 CRGB buffers to match current architecture.
 * Float accumulation would be ideal but requires larger refactor.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>

namespace lightwaveos {
namespace effects {

/**
 * @brief Post-processor configuration
 *
 * Default values are tuned for general use. Effects can override
 * with custom configs for different visual styles.
 */
struct PostProcessConfig {
    // Temporal blending (trails)
    float temporalRetention = 0.92f;    ///< How much of previous frame to keep (0.85-0.95 = nice trails)
    bool enableTemporal = true;          ///< Enable temporal blending

    // Dynamic fade (amplitude-driven)
    float dynamicFadeMin = 0.70f;       ///< Minimum retention when audio is loud (faster trails)
    float dynamicFadeMax = 0.96f;       ///< Maximum retention when audio is quiet (longer trails)
    bool enableDynamicFade = true;      ///< Enable amplitude-driven fade

    // Bloom (glow effect)
    float bloomAmount = 0.18f;          ///< Bloom intensity (0 = off, 0.1-0.3 = subtle glow)
    int bloomKernelSize = 3;            ///< Blur kernel half-width (1-5)
    bool enableBloom = true;            ///< Enable bloom effect

    // Wave modulation (subtle brightness animation)
    float waveAmount = 0.08f;           ///< Wave modulation intensity (0 = off)
    float waveSpeed = 0.03f;            ///< Wave animation speed
    bool enableWave = false;            ///< Enable wave modulation (subtle, optional)

    /**
     * @brief Preset for party/beat effects (fast, punchy)
     */
    static PostProcessConfig party() {
        PostProcessConfig cfg;
        cfg.temporalRetention = 0.88f;
        cfg.dynamicFadeMin = 0.65f;
        cfg.dynamicFadeMax = 0.92f;
        cfg.bloomAmount = 0.22f;
        cfg.enableWave = false;
        return cfg;
    }

    /**
     * @brief Preset for ambient effects (slow, dreamy)
     */
    static PostProcessConfig ambient() {
        PostProcessConfig cfg;
        cfg.temporalRetention = 0.96f;
        cfg.dynamicFadeMin = 0.85f;
        cfg.dynamicFadeMax = 0.98f;
        cfg.bloomAmount = 0.25f;
        cfg.enableWave = true;
        cfg.waveAmount = 0.12f;
        return cfg;
    }

    /**
     * @brief Preset for minimal processing (clean, crisp)
     */
    static PostProcessConfig minimal() {
        PostProcessConfig cfg;
        cfg.temporalRetention = 0.75f;
        cfg.enableDynamicFade = false;
        cfg.bloomAmount = 0.10f;
        cfg.enableWave = false;
        return cfg;
    }
};

/**
 * @brief RGB color struct for post-processing
 *
 * Uses simple struct compatible with FastLED's CRGB layout.
 * This allows the post-processor to work on any buffer of r,g,b triplets.
 */
struct PostProcessPixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    PostProcessPixel() : r(0), g(0), b(0) {}
    PostProcessPixel(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};

/**
 * @brief Universal post-processing layer for LED effects
 *
 * Applies temporal blending, dynamic fade, bloom, and optional wave
 * modulation to transform basic renders into rich, dynamic visualisations.
 *
 * Thread Safety: NOT thread-safe. Each renderer should have its own instance.
 *
 * @tparam PixelType Color pixel type (must have r, g, b uint8_t members)
 */
template<typename PixelType = PostProcessPixel>
class EffectPostProcessorT {
public:
    static constexpr uint16_t MAX_LEDS = 320;

    EffectPostProcessorT() : m_initialised(false), m_ledCount(0), m_wavePhase(0.0f) {}

    /**
     * @brief Initialise the processor for a given LED count
     * @param ledCount Number of LEDs in the strip
     */
    void init(uint16_t ledCount) {
        m_ledCount = (ledCount > MAX_LEDS) ? MAX_LEDS : ledCount;
        memset(m_prevFrame, 0, sizeof(PixelType) * m_ledCount);
        memset(m_blurBuffer, 0, sizeof(PixelType) * m_ledCount);
        m_wavePhase = 0.0f;
        m_initialised = true;
    }

    /**
     * @brief Apply post-processing to the LED buffer
     * @param leds LED buffer (modified in place)
     * @param ledCount Number of LEDs
     * @param audioLevel Current audio RMS level (0.0-1.0) for dynamic fade
     * @param config Post-processing configuration
     *
     * Call this after effect render() and before FastLED.show().
     */
    void process(PixelType* leds, uint16_t ledCount, float audioLevel,
                 const PostProcessConfig& config = PostProcessConfig{}) {
        if (!leds || ledCount == 0) return;

        // Lazy initialisation
        if (!m_initialised || m_ledCount != ledCount) {
            init(ledCount);
        }

        // 1. Calculate effective fade retention
        float fadeRetention = config.temporalRetention;
        if (config.enableDynamicFade) {
            // Louder audio = less retention (faster trails)
            // Quieter audio = more retention (longer, dreamy trails)
            float audioFactor = 1.0f - clamp01(audioLevel);
            fadeRetention = config.dynamicFadeMin +
                           (config.dynamicFadeMax - config.dynamicFadeMin) * audioFactor;
        }

        // 2. Temporal blending - combine current with previous frame
        if (config.enableTemporal) {
            for (uint16_t i = 0; i < ledCount; i++) {
                // Fade previous frame
                uint8_t prevR = static_cast<uint8_t>(m_prevFrame[i].r * fadeRetention);
                uint8_t prevG = static_cast<uint8_t>(m_prevFrame[i].g * fadeRetention);
                uint8_t prevB = static_cast<uint8_t>(m_prevFrame[i].b * fadeRetention);

                // MAX blend: keep brighter of current or faded previous
                // This creates trails without dimming new content
                leds[i].r = (leds[i].r > prevR) ? leds[i].r : prevR;
                leds[i].g = (leds[i].g > prevG) ? leds[i].g : prevG;
                leds[i].b = (leds[i].b > prevB) ? leds[i].b : prevB;
            }
        }

        // 3. Bloom effect - box blur + additive blend
        if (config.enableBloom && config.bloomAmount > 0.01f) {
            applyBloom(leds, ledCount, config.bloomAmount, config.bloomKernelSize);
        }

        // 4. Wave modulation (optional subtle brightness animation)
        if (config.enableWave && config.waveAmount > 0.01f) {
            applyWaveModulation(leds, ledCount, config.waveAmount, config.waveSpeed);
        }

        // 5. Store current frame for next iteration
        memcpy(m_prevFrame, leds, sizeof(PixelType) * ledCount);
    }

    /**
     * @brief Reset the frame history (call when switching effects)
     */
    void reset() {
        memset(m_prevFrame, 0, sizeof(m_prevFrame));
        memset(m_blurBuffer, 0, sizeof(m_blurBuffer));
        m_wavePhase = 0.0f;
    }

    /**
     * @brief Check if processor is initialised
     */
    bool isInitialised() const { return m_initialised; }

private:
    PixelType m_prevFrame[MAX_LEDS];     ///< Previous frame for temporal blending
    PixelType m_blurBuffer[MAX_LEDS];    ///< Scratch buffer for blur
    bool m_initialised;
    uint16_t m_ledCount;
    float m_wavePhase;                    ///< Wave animation phase

    /**
     * @brief Clamp float to [0, 1] range
     */
    static float clamp01(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    /**
     * @brief Apply bloom effect (box blur + additive blend)
     */
    void applyBloom(PixelType* leds, uint16_t ledCount, float amount, int kernelSize) {
        if (kernelSize < 1) kernelSize = 1;
        if (kernelSize > 5) kernelSize = 5;

        // 1. Create blurred version in scratch buffer
        for (uint16_t i = 0; i < ledCount; i++) {
            uint16_t sumR = 0, sumG = 0, sumB = 0;
            int count = 0;

            for (int j = -kernelSize; j <= kernelSize; j++) {
                int idx = static_cast<int>(i) + j;
                if (idx >= 0 && idx < static_cast<int>(ledCount)) {
                    sumR += leds[idx].r;
                    sumG += leds[idx].g;
                    sumB += leds[idx].b;
                    count++;
                }
            }

            if (count > 0) {
                m_blurBuffer[i].r = sumR / count;
                m_blurBuffer[i].g = sumG / count;
                m_blurBuffer[i].b = sumB / count;
            } else {
                m_blurBuffer[i] = leds[i];
            }
        }

        // 2. Additive blend: original + (blur * amount)
        for (uint16_t i = 0; i < ledCount; i++) {
            uint16_t r = leds[i].r + static_cast<uint16_t>(m_blurBuffer[i].r * amount);
            uint16_t g = leds[i].g + static_cast<uint16_t>(m_blurBuffer[i].g * amount);
            uint16_t b = leds[i].b + static_cast<uint16_t>(m_blurBuffer[i].b * amount);

            // Saturate to 255
            leds[i].r = (r > 255) ? 255 : static_cast<uint8_t>(r);
            leds[i].g = (g > 255) ? 255 : static_cast<uint8_t>(g);
            leds[i].b = (b > 255) ? 255 : static_cast<uint8_t>(b);
        }
    }

    /**
     * @brief Apply subtle wave brightness modulation
     */
    void applyWaveModulation(PixelType* leds, uint16_t ledCount, float amount, float speed) {
        m_wavePhase += speed;
        if (m_wavePhase > 6.28318f) m_wavePhase -= 6.28318f;

        for (uint16_t i = 0; i < ledCount; i++) {
            // Only modulate pixels that have content
            if (leds[i].r > 5 || leds[i].g > 5 || leds[i].b > 5) {
                float position = static_cast<float>(i) / static_cast<float>(ledCount);
                float wave = sinf(m_wavePhase + position * 6.28318f);
                float boost = 1.0f + (wave * amount);

                // Apply boost with saturation
                uint16_t r = static_cast<uint16_t>(leds[i].r * boost);
                uint16_t g = static_cast<uint16_t>(leds[i].g * boost);
                uint16_t b = static_cast<uint16_t>(leds[i].b * boost);

                leds[i].r = (r > 255) ? 255 : static_cast<uint8_t>(r);
                leds[i].g = (g > 255) ? 255 : static_cast<uint8_t>(g);
                leds[i].b = (b > 255) ? 255 : static_cast<uint8_t>(b);
            }
        }
    }
};

// Type alias for CRGB usage (FastLED compatible)
// CRGB has the same memory layout as PostProcessPixel (r, g, b uint8_t members)
using EffectPostProcessor = EffectPostProcessorT<PostProcessPixel>;

} // namespace effects
} // namespace lightwaveos
