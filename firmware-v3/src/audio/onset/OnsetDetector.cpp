/**
 * @file OnsetDetector.cpp
 * @brief FFT-based onset detection — implementation.
 *
 * Architecture: gate is OUTPUT PERMISSION ONLY.
 * Detector statistics (threshold history, per-band EMA, envelope ring) always
 * track REAL spectral flux values. The activity gate and warmup only suppress
 * the emitted onset_event and band triggers — they never inject zeros or scale
 * the ODF. This prevents the gate from corrupting the detector's baseline model.
 *
 * Algorithm (all parameters from published research):
 *   1. Hann window -> 1024 float samples
 *   2. Real FFT (esp-dsp half-length complex trick on device, Cooley-Tukey on native)
 *   3. Magnitude spectrum (512 bins, 31.25 Hz spacing @ 32kHz)
 *   4. Log-spectral flux: SF(t) = sum_k max(0, log(mag[t,k]) - log(mag[t-1,k]))
 *      — Bello et al. (2005), Dixon (2006). Raw sum, NOT normalised by bin count.
 *   5. Adaptive threshold: max(median(flux, 13 frames) * 1.5, 0.01)
 *      — Median-based (Dixon 2006): onset peaks don't inflate the baseline
 *   6. Peak picking: causal local max (pre_max = 4 frames) + refractory gate (4 frames)
 *      — Boeck, Krebs, Widmer (2012): pre_max = 30ms, combine = 30ms
 *   7. Per-band triggers: kick / snare / hihat with per-band statistics
 *      — Boeck et al. (2012) Section 3.2: per-band independent thresholding
 *
 * @see OnsetDetector.h for interface documentation.
 */

#include "OnsetDetector.h"

#ifndef NATIVE_BUILD
#include <esp_timer.h>
#include <esp_dsp.h>
#else
#include <chrono>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lightwaveos::audio {

// ============================================================================
// Platform timer
// ============================================================================

uint32_t OnsetDetector::getTimeUs() {
#ifndef NATIVE_BUILD
    return static_cast<uint32_t>(esp_timer_get_time());
#else
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()
    );
#endif
}

// ============================================================================
// Native build: simple radix-2 Cooley-Tukey FFT for real-valued input
// ============================================================================

#ifdef NATIVE_BUILD

namespace {

void fft_complex_inplace(float* data, size_t N) {
    size_t j = 0;
    for (size_t i = 0; i < N; ++i) {
        if (j > i) {
            float tmpR = data[2 * j];
            float tmpI = data[2 * j + 1];
            data[2 * j]     = data[2 * i];
            data[2 * j + 1] = data[2 * i + 1];
            data[2 * i]     = tmpR;
            data[2 * i + 1] = tmpI;
        }
        size_t m = N >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    for (size_t step = 1; step < N; step <<= 1) {
        float angle = static_cast<float>(-M_PI / step);
        float wR = cosf(angle);
        float wI = sinf(angle);

        for (size_t group = 0; group < N; group += step << 1) {
            float twR = 1.0f, twI = 0.0f;
            for (size_t pair = 0; pair < step; ++pair) {
                size_t a = group + pair;
                size_t b = a + step;
                float tR = twR * data[2 * b] - twI * data[2 * b + 1];
                float tI = twR * data[2 * b + 1] + twI * data[2 * b];
                data[2 * b]     = data[2 * a] - tR;
                data[2 * b + 1] = data[2 * a + 1] - tI;
                data[2 * a]     += tR;
                data[2 * a + 1] += tI;
                float newTwR = twR * wR - twI * wI;
                twI = twR * wI + twI * wR;
                twR = newTwR;
            }
        }
    }
}

void rfft_native(float* data, size_t N) {
    size_t halfN = N / 2;
    fft_complex_inplace(data, halfN);

    float* d = data;
    float dcR = d[0] + d[1];
    float nyR = d[0] - d[1];

    for (size_t k = 1; k < halfN / 2; ++k) {
        size_t mk = halfN - k;
        float eR = 0.5f * (d[2 * k] + d[2 * mk]);
        float eI = 0.5f * (d[2 * k + 1] - d[2 * mk + 1]);
        float oR = 0.5f * (d[2 * k + 1] + d[2 * mk + 1]);
        float oI = 0.5f * -(d[2 * k] - d[2 * mk]);

        float angle = static_cast<float>(-2.0 * M_PI * k / N);
        float wR = cosf(angle);
        float wI = sinf(angle);

        float tR = oR * wR - oI * wI;
        float tI = oR * wI + oI * wR;

        d[2 * k]      = eR + tR;
        d[2 * k + 1]  = eI + tI;
        d[2 * mk]     = eR - tR;
        d[2 * mk + 1] = -(eI - tI);
    }

    if (halfN > 1) {
        size_t mid = halfN / 2;
        d[2 * mid + 1] = -d[2 * mid + 1];
    }

    d[0] = dcR;
    d[1] = nyR;
}

} // anonymous namespace

#endif // NATIVE_BUILD

// ============================================================================
// Initialisation
// ============================================================================

void OnsetDetector::init() {
    init(Config{});
}

void OnsetDetector::init(const Config& cfg) {
    m_cfg = cfg;
    m_warmupFrames = cfg.warmupFrames;

    if (m_cfg.fftSize > MAX_FFT_SIZE) {
        m_cfg.fftSize = MAX_FFT_SIZE;
    }

    computeHannLut();

#ifndef NATIVE_BUILD
    const uint16_t halfN = m_cfg.fftSize / 2;
    esp_err_t err1 = dsps_fft2r_init_fc32(NULL, halfN);
    esp_err_t err2 = dsps_fft4r_init_fc32(NULL, halfN);
    if ((err1 != ESP_OK && err1 != ESP_ERR_INVALID_STATE) ||
        (err2 != ESP_OK && err2 != ESP_ERR_INVALID_STATE)) {
        return;
    }
#endif

    reset();
    m_initialised = true;
}

void OnsetDetector::reset() {
    std::memset(m_magnitude, 0, sizeof(m_magnitude));
    std::memset(m_prevMagnitude, 0, sizeof(m_prevMagnitude));
    m_hasPrevMag = false;
    m_rmsNoiseFloor = m_cfg.rmsGate;
    m_activityHoldCounter = 0;

    std::memset(m_fluxRing, 0, sizeof(m_fluxRing));
    m_fluxWriteIdx = 0;
    m_fluxCount = 0;

    std::memset(m_envRing, 0, sizeof(m_envRing));
    m_envWriteIdx = 0;
    m_lastEventFrame = 0;
    m_frameCount = 0;

    m_bassState = {};
    m_midState  = {};
    m_highState = {};
}

// ============================================================================
// Hann window precomputation
// ============================================================================

void OnsetDetector::computeHannLut() {
    const uint16_t N = m_cfg.fftSize;
    for (uint16_t i = 0; i < N; ++i) {
        m_hannLut[i] = 0.5f * (1.0f - cosf(2.0f * static_cast<float>(M_PI) * i / (N - 1)));
    }
    for (uint16_t i = N; i < MAX_FFT_SIZE; ++i) {
        m_hannLut[i] = 0.0f;
    }
}

// ============================================================================
// FFT computation (platform-specific)
// ============================================================================

void OnsetDetector::computeFFT() {
    const uint16_t N = m_cfg.fftSize;

#ifndef NATIVE_BUILD
    const uint16_t halfN = N / 2;
    dsps_fft2r_fc32(m_fftBuf, halfN);
    dsps_bit_rev2r_fc32(m_fftBuf, halfN);
    dsps_cplx2real_fc32(m_fftBuf, halfN);
#else
    rfft_native(m_fftBuf, N);
#endif
}

// ============================================================================
// Magnitude extraction
// ============================================================================

void OnsetDetector::computeMagnitudes() {
    const uint16_t halfN = m_cfg.fftSize / 2;
    m_magnitude[0] = fabsf(m_fftBuf[0]);
    for (uint16_t k = 1; k < halfN; ++k) {
        float re = m_fftBuf[2 * k];
        float im = m_fftBuf[2 * k + 1];
        m_magnitude[k] = sqrtf(re * re + im * im);
    }
}

// ============================================================================
// Band-limited spectral flux (log domain, half-wave rectified, raw sum)
// ============================================================================

float OnsetDetector::computeBandFlux(uint16_t binLo, uint16_t binHi) const {
    const uint16_t halfN = m_cfg.fftSize / 2;
    if (binHi > halfN) binHi = halfN;
    if (binLo >= binHi) return 0.0f;

    constexpr float kEps = 1e-10f;
    float flux = 0.0f;
    for (uint16_t k = binLo; k < binHi; ++k) {
        float diff = logf(fmaxf(kEps, m_magnitude[k]))
                   - logf(fmaxf(kEps, m_prevMagnitude[k]));
        if (diff > 0.0f) {
            flux += diff;
        }
    }
    return flux;
}

// ============================================================================
// Activity gate: hold + slow release ambient follower
//
// This is OUTPUT PERMISSION ONLY. The returned activity value decides whether
// onset_event and band triggers are emitted. It does NOT scale flux, inject
// zeros into threshold/band stats, or rewrite detector readiness state.
// ============================================================================

float OnsetDetector::computeActivity(float currentRms) {
    if (m_rmsNoiseFloor < m_cfg.activityFloor) {
        m_rmsNoiseFloor = m_cfg.activityFloor;
    }

    const float ambientCeil = fmaxf(m_cfg.activityFloor,
                                    m_rmsNoiseFloor * m_cfg.noiseAdaptK);

    // Only adapt the ambient floor while the signal remains plausibly ambient.
    if (currentRms <= ambientCeil) {
        if (currentRms > m_rmsNoiseFloor) {
            // Ambient floor rising: track changing room/self-noise.
            m_rmsNoiseFloor += m_cfg.noiseFloorRise * (currentRms - m_rmsNoiseFloor);
        } else {
            // Ambient floor falling: hold first, then release slowly.
            if (m_activityHoldCounter > 0) {
                --m_activityHoldCounter;
            } else {
                m_rmsNoiseFloor += m_cfg.noiseFloorFall * (currentRms - m_rmsNoiseFloor);
            }
        }
    }

    if (m_rmsNoiseFloor < m_cfg.activityFloor) {
        m_rmsNoiseFloor = m_cfg.activityFloor;
    }

    const float gateStart = m_rmsNoiseFloor * m_cfg.activityGateK;

    // Any frame above gateStart refreshes the hold timer, preventing immediate
    // threshold collapse after programme audio drops.
    if (currentRms > gateStart) {
        m_activityHoldCounter = m_cfg.activityHoldFrames;
    }

    const float gateRange = fmaxf(m_cfg.activityRangeMin,
                                  m_rmsNoiseFloor * m_cfg.activityRangeK);
    float activity = (currentRms - gateStart) / gateRange;
    if (activity < 0.0f) return 0.0f;
    if (activity > 1.0f) return 1.0f;
    return activity;
}

// ============================================================================
// Median computation for small ring buffers
// ============================================================================

float OnsetDetector::computeMedian(const float* ring, uint8_t ringSize, uint8_t count) {
    if (count == 0) return 0.0f;

    const uint8_t n = (count < ringSize) ? count : ringSize;
    float tmp[FLUX_RING_SIZE];
    for (uint8_t i = 0; i < n; ++i) tmp[i] = ring[i];

    for (uint8_t i = 1; i < n; ++i) {
        float key = tmp[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && tmp[j] > key) {
            tmp[j + 1] = tmp[j];
            --j;
        }
        tmp[j + 1] = key;
    }

    if (n & 1) {
        return tmp[n / 2];
    } else {
        return 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
    }
}

// ============================================================================
// Adaptive threshold: median-based (Dixon 2006)
// ============================================================================

float OnsetDetector::applyAdaptiveThreshold(float flux) {
    m_fluxRing[m_fluxWriteIdx] = flux;
    m_fluxWriteIdx = (m_fluxWriteIdx + 1) % FLUX_RING_SIZE;
    if (m_fluxCount < FLUX_RING_SIZE) ++m_fluxCount;

    const uint8_t windowSize = (m_fluxCount < m_cfg.thresholdFrames)
                               ? m_fluxCount : m_cfg.thresholdFrames;
    float median = computeMedian(m_fluxRing, FLUX_RING_SIZE, windowSize);
    float threshold = median * m_cfg.thresholdMultiplier;
    if (threshold < m_cfg.thresholdFloor) {
        threshold = m_cfg.thresholdFloor;
    }

    float env = flux - threshold;
    return (env > 0.0f) ? env : 0.0f;
}

// ============================================================================
// Peak picker: CPJKU causal local maximum + refractory gate
//
// Always keeps envelope history current. Only emits when emitEnabled.
// ============================================================================

float OnsetDetector::peakPick(float env, bool emitEnabled) {
    m_envRing[m_envWriteIdx] = env;
    const uint8_t candIdx = m_envWriteIdx;
    m_envWriteIdx = (m_envWriteIdx + 1) % ENV_RING_SIZE;

    if (!emitEnabled) return 0.0f;
    if (env <= 0.0f) return 0.0f;
    if (m_frameCount < static_cast<uint32_t>(m_cfg.preMaxFrames)) return 0.0f;

    for (uint8_t i = 1; i <= m_cfg.preMaxFrames; ++i) {
        int idx = (static_cast<int>(candIdx) - i + ENV_RING_SIZE) % ENV_RING_SIZE;
        if (m_envRing[idx] >= env) return 0.0f;
    }

    if (m_frameCount - m_lastEventFrame < m_cfg.peakWait) return 0.0f;

    m_lastEventFrame = m_frameCount;
    return env;
}

// ============================================================================
// Per-band trigger: EMA threshold + 3-frame local max + refractory
//
// Always keeps per-band statistics current on REAL flux.
// Only emits when emitEnabled.
// ============================================================================

bool OnsetDetector::bandTrigger(BandState& state, float flux,
                                float thresholdK, float emaAlpha,
                                uint8_t refractoryFrames, bool emitEnabled) {
    // Always update statistics on real flux — never zeros.
    state.fluxMean += emaAlpha * (flux - state.fluxMean);

    float threshold = state.fluxMean * (1.0f + thresholdK);

    bool localMax = (state.prevFlux > state.prev2Flux) && (state.prevFlux > flux);
    bool aboveThreshold = (state.prevFlux > threshold);
    bool refractoryOpen = (m_frameCount - state.lastTriggerFrame) >=
                          static_cast<uint32_t>(refractoryFrames);

    state.prev2Flux = state.prevFlux;
    state.prevFlux  = flux;

    if (!emitEnabled) return false;

    if (localMax && aboveThreshold && refractoryOpen) {
        state.lastTriggerFrame = m_frameCount;
        return true;
    }
    return false;
}

// ============================================================================
// Main processing function — called every hop (125 Hz)
// ============================================================================

OnsetResult OnsetDetector::process(const float* samples, float currentRms) {
    OnsetResult result = {};
    if (!m_initialised) return result;
    const uint32_t startUs = getTimeUs();

    const uint16_t fftSize = m_cfg.fftSize;
    const uint16_t halfN   = fftSize / 2;

    const float activity = computeActivity(currentRms);
    result.input_rms = currentRms;
    result.noise_floor = m_rmsNoiseFloor;
    result.activity = activity;

    // Absolute near-zero gate: no meaningful spectrum.
    // Do NOT inject zeros into detector statistics. Force a clean re-prime
    // on the next meaningful frame so we never compare against stale spectra.
    if (currentRms < m_cfg.rmsGate) {
        result.gate_flags |= ONSET_GATE_ABS_RMS;
        m_hasPrevMag = false;
        m_frameCount++;
        result.process_us = static_cast<uint16_t>(getTimeUs() - startUs);
        return result;
    }

    // Step 1: Hann window
    for (uint16_t i = 0; i < fftSize; ++i) {
        m_fftBuf[i] = samples[i] * m_hannLut[i];
    }

    // Step 2: FFT
    computeFFT();

    // Step 3: Magnitude spectrum
    computeMagnitudes();

    // First frame: prime the spectral reference, no flux comparison possible.
    if (!m_hasPrevMag) {
        result.gate_flags |= ONSET_GATE_NO_PREV;
        std::memcpy(m_prevMagnitude, m_magnitude, halfN * sizeof(float));
        m_hasPrevMag = true;
        m_frameCount++;
        result.process_us = static_cast<uint16_t>(getTimeUs() - startUs);
        return result;
    }

    // Determine emission permission: warmup and activity are output gates only.
    const bool warmupActive = (m_frameCount < m_warmupFrames);
    const bool activityBlocked = (activity < m_cfg.activityCutoff);
    const bool emitEnabled = !warmupActive && !activityBlocked;

    if (warmupActive) result.gate_flags |= ONSET_GATE_WARMUP;
    if (activityBlocked) result.gate_flags |= ONSET_GATE_ACTIVITY;

    // Step 4: Log-spectral flux (Bello 2005 / Dixon 2006)
    // NO activity weighting — gate is permission only, not ODF gain control.
    result.flux = computeBandFlux(m_cfg.musicalBinLo, m_cfg.musicalBinHi);

    result.bass_flux = computeBandFlux(m_cfg.bassBinLo, m_cfg.bassBinHi);
    result.mid_flux  = computeBandFlux(m_cfg.snareBinLo, m_cfg.snareBinHi);
    result.high_flux = computeBandFlux(m_cfg.hihatBinLo, m_cfg.hihatBinHi);

    // Step 5: Adaptive threshold always tracks REAL flux.
    result.onset_env = applyAdaptiveThreshold(result.flux);

    // Step 6: Peak picker always tracks REAL envelope, emits only when enabled.
    result.onset_event = peakPick(result.onset_env, emitEnabled);

    // Step 7: Per-band triggers always track REAL band flux, emit only when enabled.
    const float bassNorm = static_cast<float>(m_cfg.bassBinHi - m_cfg.bassBinLo);
    const float midNorm  = static_cast<float>(m_cfg.snareBinHi - m_cfg.snareBinLo);
    const float hiNorm   = static_cast<float>(m_cfg.hihatBinHi - m_cfg.hihatBinLo);

    float bassFluxNorm = (bassNorm > 0.0f) ? result.bass_flux / bassNorm : 0.0f;
    float midFluxNorm  = (midNorm  > 0.0f) ? result.mid_flux  / midNorm  : 0.0f;
    float hiFluxNorm   = (hiNorm   > 0.0f) ? result.high_flux / hiNorm   : 0.0f;

    result.kick_trigger  = bandTrigger(m_bassState, bassFluxNorm,
                                       m_cfg.kickThresholdK, m_cfg.kickEmaAlpha,
                                       m_cfg.kickRefractory, emitEnabled);
    result.snare_trigger = bandTrigger(m_midState, midFluxNorm,
                                       m_cfg.snareThresholdK, m_cfg.snareEmaAlpha,
                                       m_cfg.snareRefractory, emitEnabled);
    result.hihat_trigger = bandTrigger(m_highState, hiFluxNorm,
                                       m_cfg.hihatThresholdK, m_cfg.hihatEmaAlpha,
                                       m_cfg.hihatRefractory, emitEnabled);

    // Step 8: Store magnitudes for next frame
    std::memcpy(m_prevMagnitude, m_magnitude, halfN * sizeof(float));
    m_hasPrevMag = true;

    m_frameCount++;
    result.process_us = static_cast<uint16_t>(getTimeUs() - startUs);
    return result;
}

} // namespace lightwaveos::audio
