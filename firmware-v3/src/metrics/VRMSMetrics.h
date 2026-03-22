#pragma once

/**
 * @file VRMSMetrics.h
 * @brief VRMS (Visual RMS) production compute engine
 *
 * Computes 7 perceptual metrics from a 320-pixel CRGB frame buffer
 * using a fused single-pass architecture. Cross-core safe via double-buffered output.
 *
 * Gated behind FEATURE_VRMS_METRICS.
 * Zero heap allocation. All state pre-allocated.
 */

#include "../config/features.h"

#if FEATURE_VRMS_METRICS

#include <cstdint>
#include <cmath>
#include <cstring>
#include <atomic>
#include <FastLED.h>

namespace metrics {

/// Pre-computed sin/cos LUT for circular mean hue (Q15 fixed-point)
struct HueSinCosLUT {
    int16_t sinTable[256];
    int16_t cosTable[256];

    void init() {
        for (int i = 0; i < 256; i++) {
            float angle = (float)i * (2.0f * M_PI / 256.0f);
            sinTable[i] = (int16_t)(sinf(angle) * 32767.0f);
            cosTable[i] = (int16_t)(cosf(angle) * 32767.0f);
        }
    }
};

/// VRMS metric output vector (32 bytes — 8 floats)
struct VRMSVector {
    float dominantHue;        // 0-255 (HSV hue space)
    float colourVariance;     // 0-1 (normalized circular std dev)
    float spatialCentroid;    // 0-319 (pixel index, ideal=159.5 for full strip)
    float symmetryScore;      // -1 to 1 (Pearson correlation)
    float brightnessMean;     // 0-255
    float brightnessVariance; // 0-65025
    float temporalFreq;       // 0-1 (normalized frame-diff magnitude)
    float audioVisualCorr;    // -1 to 1 (Pearson correlation)
};

/// Per-section timing (for optional benchmark overlay)
struct VRMSTimings {
    uint32_t fusedPassUs;
    uint32_t symmetryScoreUs;
    uint32_t audioVisualCorrUs;
    uint32_t totalUs;
};

/**
 * @brief Production VRMS compute engine with cross-core safe output
 *
 * Call init() once at boot. Call compute() at 60Hz (every other frame).
 * Call getLatestVector() from any core — returns a consistent snapshot.
 *
 * Double-buffer pattern matches RendererActor::BandsDebugSnapshot.
 */
class VRMSMetricsEngine {
public:
    static constexpr uint16_t NUM_LEDS = 320;
    static constexpr uint16_t HALF_LEDS = 160;
    static constexpr uint8_t CORR_WINDOW = 32;

    void init() {
        m_lut.init();
        memset(m_prevBrightness, 0, sizeof(m_prevBrightness));
        memset(m_audioRmsHistory, 0, sizeof(m_audioRmsHistory));
        memset(m_frameBrightnessHistory, 0, sizeof(m_frameBrightnessHistory));
        m_historyIndex = 0;
        m_historyFilled = false;
        m_buffer[0] = {};
        m_buffer[1] = {};
        m_writeIdx.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Compute all 7 VRMS metrics (fused single-pass)
     *
     * Call from Core 1 render loop at 60Hz decimation.
     * Results available via getLatestVector() from any core.
     */
    void compute(const CRGB* leds, float audioRms) {
        VRMSVector v = {};
        uint8_t wIdx = m_writeIdx.load(std::memory_order_relaxed);

        // =============================================================
        // FUSED PASS: Metrics 1,2,3,5,6 — one loop, one HSV per pixel
        // =============================================================

        int32_t hueWeightedSin = 0, hueWeightedCos = 0;
        uint32_t hueTotalWeight = 0;
        int32_t varSumSin = 0, varSumCos = 0;
        uint16_t varCount = 0;
        uint32_t centroidWeightedSum = 0, centroidTotalBrt = 0;
        uint32_t brtSum = 0, brtSumSq = 0;
        uint32_t tempDiffSum = 0;

        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            uint8_t val = leds[i].r;
            if (leds[i].g > val) val = leds[i].g;
            if (leds[i].b > val) val = leds[i].b;

            centroidWeightedSum += (uint32_t)i * val;
            centroidTotalBrt += val;
            brtSum += val;
            brtSumSq += (uint32_t)val * val;

            int16_t diff = (int16_t)val - (int16_t)m_prevBrightness[i];
            tempDiffSum += (uint32_t)(diff < 0 ? -diff : diff);
            m_prevBrightness[i] = val;

            if (val < 4) continue;

            CHSV hsv = rgb2hsv_approximate(leds[i]);
            uint8_t hue = hsv.hue;

            hueWeightedSin += (int32_t)m_lut.sinTable[hue] * val;
            hueWeightedCos += (int32_t)m_lut.cosTable[hue] * val;
            hueTotalWeight += val;

            varSumSin += m_lut.sinTable[hue];
            varSumCos += m_lut.cosTable[hue];
            varCount++;
        }

        // Metric 1: Dominant Hue
        if (hueTotalWeight == 0) {
            v.dominantHue = 0;
        } else {
            float meanAngle = atan2f((float)hueWeightedSin, (float)hueWeightedCos);
            if (meanAngle < 0) meanAngle += 2.0f * M_PI;
            v.dominantHue = meanAngle * (256.0f / (2.0f * M_PI));
        }

        // Metric 2: Colour Variance
        if (varCount < 2) {
            v.colourVariance = 0;
        } else {
            float meanSin = (float)varSumSin / varCount;
            float meanCos = (float)varSumCos / varCount;
            float R = sqrtf(meanSin * meanSin + meanCos * meanCos) / 32767.0f;
            v.colourVariance = 1.0f - R;
        }

        // Metric 3: Spatial Centroid
        v.spatialCentroid = (centroidTotalBrt > 0)
            ? (float)centroidWeightedSum / centroidTotalBrt
            : 159.5f;

        // Metric 5: Brightness Distribution
        float mean = (float)brtSum / NUM_LEDS;
        v.brightnessMean = mean;
        v.brightnessVariance = (float)brtSumSq / NUM_LEDS - mean * mean;

        // Metric 6: Temporal Frequency
        v.temporalFreq = (float)tempDiffSum / 81600.0f;

        float frameBrightness = mean;  // reuse for metric 7

        // =============================================================
        // Metric 4: Symmetry Score (separate pass — mirrored access)
        // =============================================================
        {
            int32_t sumL = 0, sumR = 0;
            int32_t sumLL = 0, sumRR = 0, sumLR = 0;

            for (uint16_t i = 0; i < HALF_LEDS; i++) {
                uint8_t vL = leds[i].r;
                if (leds[i].g > vL) vL = leds[i].g;
                if (leds[i].b > vL) vL = leds[i].b;

                uint16_t mirrorIdx = NUM_LEDS - 1 - i;
                uint8_t vR = leds[mirrorIdx].r;
                if (leds[mirrorIdx].g > vR) vR = leds[mirrorIdx].g;
                if (leds[mirrorIdx].b > vR) vR = leds[mirrorIdx].b;

                sumL += vL; sumR += vR;
                sumLL += (int32_t)vL * vL;
                sumRR += (int32_t)vR * vR;
                sumLR += (int32_t)vL * vR;
            }

            float n = HALF_LEDS;
            float num = n * sumLR - (float)sumL * sumR;
            float denL = n * sumLL - (float)sumL * sumL;
            float denR = n * sumRR - (float)sumR * sumR;
            float den = sqrtf(denL * denR);
            v.symmetryScore = (den > 1e-6f) ? (num / den) : 0.0f;
        }

        // =============================================================
        // Metric 7: Audio-Visual Correlation (sliding window)
        // =============================================================
        {
            m_audioRmsHistory[m_historyIndex] = audioRms;
            m_frameBrightnessHistory[m_historyIndex] = frameBrightness;
            m_historyIndex = (m_historyIndex + 1) % CORR_WINDOW;
            if (m_historyIndex == 0) m_historyFilled = true;

            uint8_t n = m_historyFilled ? CORR_WINDOW : m_historyIndex;
            if (n < 4) {
                v.audioVisualCorr = 0;
            } else {
                float sA = 0, sB = 0, sAA = 0, sBB = 0, sAB = 0;
                for (uint8_t j = 0; j < n; j++) {
                    float a = m_audioRmsHistory[j];
                    float b = m_frameBrightnessHistory[j];
                    sA += a; sB += b;
                    sAA += a * a; sBB += b * b;
                    sAB += a * b;
                }
                float num = n * sAB - sA * sB;
                float denA = n * sAA - sA * sA;
                float denB = n * sBB - sB * sB;
                float den = sqrtf(denA * denB);
                v.audioVisualCorr = (den > 1e-6f) ? (num / den) : 0.0f;
            }
        }

        // Write to double buffer and flip index
        m_buffer[wIdx] = v;
        m_writeIdx.store(1 - wIdx, std::memory_order_release);
    }

    /**
     * @brief Get the latest VRMS vector (cross-core safe)
     *
     * Reads from the buffer slot the writer is NOT currently targeting.
     * Safe to call from any core without locks.
     */
    VRMSVector getLatestVector() const {
        uint8_t rIdx = 1 - m_writeIdx.load(std::memory_order_acquire);
        return m_buffer[rIdx];
    }

private:
    HueSinCosLUT m_lut;
    VRMSVector m_buffer[2] = {};
    std::atomic<uint8_t> m_writeIdx{0};

    uint8_t m_prevBrightness[NUM_LEDS] = {};
    float m_audioRmsHistory[CORR_WINDOW] = {};
    float m_frameBrightnessHistory[CORR_WINDOW] = {};
    uint8_t m_historyIndex = 0;
    bool m_historyFilled = false;
};

}  // namespace metrics

#endif  // FEATURE_VRMS_METRICS
