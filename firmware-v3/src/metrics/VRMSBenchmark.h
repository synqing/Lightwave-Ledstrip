#pragma once

/**
 * @file VRMSBenchmark.h
 * @brief VRMS Experiment 4.1 — Cycle-count benchmark overlay
 *
 * Adds per-section timing, EMA/peak tracking, and serial reporting
 * on top of the production VRMSMetrics.h compute engine.
 *
 * Gated behind FEATURE_VRMS_BENCHMARK. Does NOT replace VRMSMetrics.h;
 * it reuses the struct definitions and provides its own timed compute path.
 */

#include "VRMSMetrics.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <FastLED.h>
#include <esp_timer.h>

namespace metrics {

// HueSinCosLUT, VRMSVector, VRMSTimings defined in VRMSMetrics.h

/**
 * @brief VRMS benchmark engine (v2 — fused single-pass)
 *
 * Call init() once at boot. Call compute() every other frame (60Hz decimation).
 * Hook after renderFrame() + color correction, before showLeds().
 *
 * Zero heap allocation. All state pre-allocated.
 */
class VRMSBenchmark {
public:
    static constexpr uint16_t NUM_LEDS = 320;
    static constexpr uint16_t HALF_LEDS = 160;
    static constexpr uint16_t REPORT_INTERVAL = 60;  // Every 60 computes (~1s at 60Hz)
    static constexpr uint8_t CORR_WINDOW = 32;

    void init() {
        m_lut.init();
        m_computeCount = 0;
        memset(m_prevBrightness, 0, sizeof(m_prevBrightness));
        memset(m_audioRmsHistory, 0, sizeof(m_audioRmsHistory));
        memset(m_frameBrightnessHistory, 0, sizeof(m_frameBrightnessHistory));
        m_historyIndex = 0;
        m_historyFilled = false;
        m_emaTimings = {};
        m_peakTimings = {};
    }

    /**
     * @brief Compute all 7 VRMS metrics with fused single-pass architecture
     *
     * @param leds     CRGB[320] frame buffer (post-render, post-correction)
     * @param audioRms Current audio RMS from ControlBus (0.0-1.0)
     * @return VRMSTimings with per-section microsecond measurements
     */
    VRMSTimings compute(const CRGB* leds, float audioRms) {
        VRMSTimings t = {};
        uint32_t totalStart = (uint32_t)esp_timer_get_time();

        // =================================================================
        // FUSED PASS: Metrics 1,2,3,5,6 in a single loop over 320 pixels
        // One rgb2hsv_approximate() call per pixel. One brightness extraction.
        // =================================================================
        {
            uint32_t start = (uint32_t)esp_timer_get_time();

            // Accumulators for dominant hue (metric 1) — brightness-weighted circular mean
            int32_t hueWeightedSin = 0;
            int32_t hueWeightedCos = 0;
            uint32_t hueTotalWeight = 0;

            // Accumulators for colour variance (metric 2) — unweighted circular variance
            int32_t varSumSin = 0;
            int32_t varSumCos = 0;
            uint16_t varCount = 0;

            // Accumulators for spatial centroid (metric 3)
            uint32_t centroidWeightedSum = 0;
            uint32_t centroidTotalBrightness = 0;

            // Accumulators for brightness distribution (metric 5)
            uint32_t brtSum = 0;
            uint32_t brtSumSq = 0;

            // Accumulators for temporal frequency (metric 6)
            uint32_t tempDiffSum = 0;

            // Also accumulate mean brightness for audio-visual correlation (metric 7)
            // (avoids a separate loop in computeAudioVisualCorrelation)
            uint32_t corrBrtSum = 0;

            for (uint16_t i = 0; i < NUM_LEDS; i++) {
                // Brightness: max(r,g,b) — computed ONCE per pixel
                uint8_t v = leds[i].r;
                if (leds[i].g > v) v = leds[i].g;
                if (leds[i].b > v) v = leds[i].b;

                // Metric 3: spatial centroid
                centroidWeightedSum += (uint32_t)i * v;
                centroidTotalBrightness += v;

                // Metric 5: brightness distribution
                brtSum += v;
                brtSumSq += (uint32_t)v * v;

                // Metric 6: temporal frequency
                int16_t diff = (int16_t)v - (int16_t)m_prevBrightness[i];
                tempDiffSum += (uint32_t)(diff < 0 ? -diff : diff);
                m_prevBrightness[i] = v;

                // Metric 7 accumulator (brightness sum for mean)
                corrBrtSum += v;

                // Metrics 1 & 2: HSV conversion only for non-black pixels
                if (v < 4) continue;

                CHSV hsv = rgb2hsv_approximate(leds[i]);
                uint8_t hue = hsv.hue;

                // Metric 1: brightness-weighted circular accumulation
                hueWeightedSin += (int32_t)m_lut.sinTable[hue] * v;
                hueWeightedCos += (int32_t)m_lut.cosTable[hue] * v;
                hueTotalWeight += v;

                // Metric 2: unweighted circular accumulation
                varSumSin += m_lut.sinTable[hue];
                varSumCos += m_lut.cosTable[hue];
                varCount++;
            }

            // --- Finalize metric 1: Dominant Hue ---
            if (hueTotalWeight == 0) {
                m_vector.dominantHue = 0;
            } else {
                float meanAngle = atan2f((float)hueWeightedSin, (float)hueWeightedCos);
                if (meanAngle < 0) meanAngle += 2.0f * M_PI;
                m_vector.dominantHue = meanAngle * (256.0f / (2.0f * M_PI));
            }

            // --- Finalize metric 2: Colour Variance ---
            if (varCount < 2) {
                m_vector.colourVariance = 0;
            } else {
                float meanSin = (float)varSumSin / varCount;
                float meanCos = (float)varSumCos / varCount;
                float R = sqrtf(meanSin * meanSin + meanCos * meanCos) / 32767.0f;
                m_vector.colourVariance = 1.0f - R;
            }

            // --- Finalize metric 3: Spatial Centroid ---
            m_vector.spatialCentroid = (centroidTotalBrightness > 0)
                ? (float)centroidWeightedSum / centroidTotalBrightness
                : 159.5f;

            // --- Finalize metric 5: Brightness Distribution ---
            float mean = (float)brtSum / NUM_LEDS;
            m_vector.brightnessMean = mean;
            m_vector.brightnessVariance = (float)brtSumSq / NUM_LEDS - mean * mean;

            // --- Finalize metric 6: Temporal Frequency ---
            m_vector.temporalFreq = (float)tempDiffSum / 81600.0f;

            // --- Store mean brightness for metric 7 ---
            m_cachedFrameBrightness = (float)corrBrtSum / NUM_LEDS;

            t.fusedPassUs = (uint32_t)esp_timer_get_time() - start;
        }

        // =================================================================
        // Metric 4: Symmetry Score (separate pass — mirrored index access)
        // =================================================================
        {
            uint32_t start = (uint32_t)esp_timer_get_time();

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

                sumL += vL;
                sumR += vR;
                sumLL += (int32_t)vL * vL;
                sumRR += (int32_t)vR * vR;
                sumLR += (int32_t)vL * vR;
            }

            float n = HALF_LEDS;
            float num = n * sumLR - (float)sumL * sumR;
            float denL = n * sumLL - (float)sumL * sumL;
            float denR = n * sumRR - (float)sumR * sumR;
            float den = sqrtf(denL * denR);

            m_vector.symmetryScore = (den > 1e-6f) ? (num / den) : 0.0f;

            t.symmetryScoreUs = (uint32_t)esp_timer_get_time() - start;
        }

        // =================================================================
        // Metric 7: Audio-Visual Correlation (sliding window, no pixel loop)
        // =================================================================
        {
            uint32_t start = (uint32_t)esp_timer_get_time();

            // Store in circular buffer (brightness already computed in fused pass)
            m_audioRmsHistory[m_historyIndex] = audioRms;
            m_frameBrightnessHistory[m_historyIndex] = m_cachedFrameBrightness;
            m_historyIndex = (m_historyIndex + 1) % CORR_WINDOW;
            if (m_historyIndex == 0) m_historyFilled = true;

            uint8_t n = m_historyFilled ? CORR_WINDOW : m_historyIndex;
            if (n < 4) {
                m_vector.audioVisualCorr = 0;
            } else {
                float sumA = 0, sumB = 0, sumAA = 0, sumBB = 0, sumAB = 0;
                for (uint8_t j = 0; j < n; j++) {
                    float a = m_audioRmsHistory[j];
                    float b = m_frameBrightnessHistory[j];
                    sumA += a;
                    sumB += b;
                    sumAA += a * a;
                    sumBB += b * b;
                    sumAB += a * b;
                }

                float num = n * sumAB - sumA * sumB;
                float denA = n * sumAA - sumA * sumA;
                float denB = n * sumBB - sumB * sumB;
                float den = sqrtf(denA * denB);

                m_vector.audioVisualCorr = (den > 1e-6f) ? (num / den) : 0.0f;
            }

            t.audioVisualCorrUs = (uint32_t)esp_timer_get_time() - start;
        }

        t.totalUs = (uint32_t)esp_timer_get_time() - totalStart;

        updateTimingStats(t);

        m_computeCount++;
        if (m_computeCount % REPORT_INTERVAL == 0) {
            printReport();
        }

        return t;
    }

    const VRMSVector& getVector() const { return m_vector; }
    const VRMSTimings& getEmaTimings() const { return m_emaTimings; }
    const VRMSTimings& getPeakTimings() const { return m_peakTimings; }

private:
    void updateTimingStats(const VRMSTimings& t) {
        constexpr float alpha = 0.1f;
        auto ema = [alpha](uint32_t& smoothed, uint32_t raw) {
            if (smoothed == 0) smoothed = raw;
            else smoothed = (uint32_t)((1.0f - alpha) * smoothed + alpha * raw);
        };

        ema(m_emaTimings.fusedPassUs, t.fusedPassUs);
        ema(m_emaTimings.symmetryScoreUs, t.symmetryScoreUs);
        ema(m_emaTimings.audioVisualCorrUs, t.audioVisualCorrUs);
        ema(m_emaTimings.totalUs, t.totalUs);

        auto peak = [](uint32_t& p, uint32_t raw) { if (raw > p) p = raw; };
        peak(m_peakTimings.fusedPassUs, t.fusedPassUs);
        peak(m_peakTimings.symmetryScoreUs, t.symmetryScoreUs);
        peak(m_peakTimings.audioVisualCorrUs, t.audioVisualCorrUs);
        peak(m_peakTimings.totalUs, t.totalUs);
    }

    void printReport() {
        Serial.println("\n=== VRMS Benchmark v2 (Experiment 4.1) ===");
        Serial.printf("Compute #%lu | Decimated: 60Hz\n", m_computeCount);
        Serial.println("--- EMA (us) ---");
        Serial.printf("  Fused pass (M1-3,5,6): %4lu us\n", m_emaTimings.fusedPassUs);
        Serial.printf("  Symmetry (M4):         %4lu us\n", m_emaTimings.symmetryScoreUs);
        Serial.printf("  AV Correlation (M7):   %4lu us\n", m_emaTimings.audioVisualCorrUs);
        Serial.printf("  TOTAL per-compute:     %4lu us\n", m_emaTimings.totalUs);
        Serial.printf("  Amortised @60Hz:       %4lu us  %s\n",
            m_emaTimings.totalUs / 2,
            m_emaTimings.totalUs / 2 <= 300 ? "PASS" : "FAIL");
        Serial.println("--- Peak (us) ---");
        Serial.printf("  TOTAL PEAK:            %4lu us\n", m_peakTimings.totalUs);
        Serial.printf("  Amortised PEAK @60Hz:  %4lu us  %s\n",
            m_peakTimings.totalUs / 2,
            m_peakTimings.totalUs / 2 <= 300 ? "PASS" : "FAIL");
        Serial.println("--- Metrics ---");
        Serial.printf("  Hue=%.1f Var=%.3f Centroid=%.1f Sym=%.3f\n",
            m_vector.dominantHue, m_vector.colourVariance,
            m_vector.spatialCentroid, m_vector.symmetryScore);
        Serial.printf("  BrtMean=%.1f BrtVar=%.0f TempFreq=%.3f AVCorr=%.3f\n",
            m_vector.brightnessMean, m_vector.brightnessVariance,
            m_vector.temporalFreq, m_vector.audioVisualCorr);
        Serial.println("==========================================\n");
    }

    // State — all pre-allocated, zero heap
    HueSinCosLUT m_lut;
    VRMSVector m_vector = {};
    VRMSTimings m_emaTimings = {};
    VRMSTimings m_peakTimings = {};
    uint32_t m_computeCount = 0;

    uint8_t m_prevBrightness[NUM_LEDS] = {};
    float m_cachedFrameBrightness = 0;

    float m_audioRmsHistory[CORR_WINDOW] = {};
    float m_frameBrightnessHistory[CORR_WINDOW] = {};
    uint8_t m_historyIndex = 0;
    bool m_historyFilled = false;
};

}  // namespace metrics
