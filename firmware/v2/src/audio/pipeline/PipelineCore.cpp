#include "PipelineCore.h"

#include <cmath>
#include <cstring>

static float clamp01(float x) {
  if (x < 0.0f) return 0.0f;
  if (x > 1.0f) return 1.0f;
  return x;
}

// Chroma frequencies: 4 octaves x 12 pitch classes (C2-B5)
static constexpr float kChromaFreqs[PipelineCore::kChroma * PipelineCore::kOctaves] = {
  65.41f, 69.30f, 73.42f, 77.78f, 82.41f, 87.31f, 92.50f, 98.00f, 103.83f, 110.00f, 116.54f, 123.47f,
  130.81f, 138.59f, 146.83f, 155.56f, 164.81f, 174.61f, 185.00f, 196.00f, 207.65f, 220.00f, 233.08f, 246.94f,
  261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f, 369.99f, 392.00f, 415.30f, 440.00f, 466.16f, 493.88f,
  523.25f, 554.37f, 587.33f, 622.25f, 659.25f, 698.46f, 739.99f, 783.99f, 830.61f, 880.00f, 932.33f, 987.77f
};

// Band edge frequencies (Hz) for dynamic bin mapping
static constexpr float kBandEdges[PipelineCore::kBands + 1] = {
  20.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 20000.0f
};

// ─── Constructor / Reset ───────────────────────────────────────────

PipelineCore::PipelineCore() {
  reset();
}

void PipelineCore::reset() {
  m_hopIndex = 0;
  m_windowIndex = 0;
  m_windowFilled = false;
  m_frameReady = false;
  m_seq = 0;
  m_dcEstimate = 0.0f;
  m_hasPrevMag = false;
  m_onsetMean = 0.0f;
  m_onsetVar = 0.0f;

  // Peak picker state
  m_peakWriteIdx = 0;
  m_peakFrameCounter = 0;
  m_peakLastEventFrame = 0;
  m_peakHasEvent = false;
  for (size_t i = 0; i < kPeakBufSize; ++i) m_peakBuf[i] = 0.0f;

  // Spectral state
  for (size_t i = 0; i < kNumBins; ++i) {
    m_prevLogMag[i] = 0.0f;
    m_magSpectrum[i] = 0.0f;
    m_whitenPeak[i] = 0.0f;
  }

  initBinMapping();

#if defined(__ARM_ARCH_7EM__)
  arm_rfft_fast_init_f32(&m_rfftInst, 512);
#endif

  m_beatTracker.setConfig(m_cfg.beat, m_cfg.sampleRate, m_cfg.hopSize);
}

void PipelineCore::setConfig(const PipelineConfig& cfg) {
  m_cfg = cfg;
  reset();
}

const PipelineConfig& PipelineCore::getConfig() const {
  return m_cfg;
}

// ─── Hot-reload parameter access (no reset) ───────────────────────

bool PipelineCore::setParamFloat(const char* name, float value) {
  // ── peak.* ──
  if (std::strncmp(name, "peak.", 5) == 0) {
    const char* field = name + 5;
    if (std::strcmp(field, "preMax") == 0)  { m_cfg.peakPick.preMax  = static_cast<uint16_t>(value); return true; }
    if (std::strcmp(field, "postMax") == 0) { m_cfg.peakPick.postMax = static_cast<uint16_t>(value); return true; }
    if (std::strcmp(field, "preAvg") == 0)  { m_cfg.peakPick.preAvg  = static_cast<uint16_t>(value); return true; }
    if (std::strcmp(field, "postAvg") == 0) { m_cfg.peakPick.postAvg = static_cast<uint16_t>(value); return true; }
    if (std::strcmp(field, "delta") == 0)   { m_cfg.peakPick.delta   = value; return true; }
    if (std::strcmp(field, "wait") == 0)    { m_cfg.peakPick.wait    = static_cast<uint16_t>(value); return true; }
    return false;
  }

  // ── onset.* ──
  if (std::strncmp(name, "onset.", 6) == 0) {
    const char* field = name + 6;
    if (std::strcmp(field, "k") == 0)         { m_cfg.onsetK         = value; return true; }
    if (std::strcmp(field, "meanAlpha") == 0)  { m_cfg.onsetMeanAlpha = value; return true; }
    if (std::strcmp(field, "varAlpha") == 0)   { m_cfg.onsetVarAlpha  = value; return true; }
    if (std::strcmp(field, "gateRms") == 0)   { m_cfg.onsetGateRms   = value; return true; }
    return false;
  }

  // ── dc.* ──
  if (std::strncmp(name, "dc.", 3) == 0) {
    const char* field = name + 3;
    if (std::strcmp(field, "alpha") == 0) { m_cfg.dcAlpha = value; return true; }
    return false;
  }

  // ── beat.* → delegate to BeatTracker ──
  if (std::strncmp(name, "beat.", 5) == 0) {
    const char* field = name + 5;
    bool ok = m_beatTracker.setParamFloat(field, value);
    if (ok) {
      BeatConfig& bc = m_cfg.beat;
      if (std::strcmp(field, "minBeatFactor") == 0)  bc.minBeatFactor  = value;
      if (std::strcmp(field, "cbssAlpha") == 0)       bc.cbssAlpha      = value;
      if (std::strcmp(field, "tempoPriorBpm") == 0)   bc.tempoPriorBpm  = value;
      if (std::strcmp(field, "tempoPriorWidth") == 0)  bc.tempoPriorWidth = value;
      if (std::strcmp(field, "tempoDecay") == 0)       bc.tempoDecay     = value;
      if (std::strcmp(field, "tempoMinBpm") == 0)      bc.tempoMinBpm    = value;
      if (std::strcmp(field, "tempoMaxBpm") == 0)      bc.tempoMaxBpm    = value;
    }
    return ok;
  }

  return false;
}

bool PipelineCore::getParamFloat(const char* name, float* out) const {
  // ── peak.* ──
  if (std::strncmp(name, "peak.", 5) == 0) {
    const char* field = name + 5;
    if (std::strcmp(field, "preMax") == 0)  { *out = static_cast<float>(m_cfg.peakPick.preMax);  return true; }
    if (std::strcmp(field, "postMax") == 0) { *out = static_cast<float>(m_cfg.peakPick.postMax); return true; }
    if (std::strcmp(field, "preAvg") == 0)  { *out = static_cast<float>(m_cfg.peakPick.preAvg);  return true; }
    if (std::strcmp(field, "postAvg") == 0) { *out = static_cast<float>(m_cfg.peakPick.postAvg); return true; }
    if (std::strcmp(field, "delta") == 0)   { *out = m_cfg.peakPick.delta;   return true; }
    if (std::strcmp(field, "wait") == 0)    { *out = static_cast<float>(m_cfg.peakPick.wait);    return true; }
    return false;
  }

  // ── onset.* ──
  if (std::strncmp(name, "onset.", 6) == 0) {
    const char* field = name + 6;
    if (std::strcmp(field, "k") == 0)         { *out = m_cfg.onsetK;         return true; }
    if (std::strcmp(field, "meanAlpha") == 0)  { *out = m_cfg.onsetMeanAlpha; return true; }
    if (std::strcmp(field, "varAlpha") == 0)   { *out = m_cfg.onsetVarAlpha;  return true; }
    if (std::strcmp(field, "gateRms") == 0)   { *out = m_cfg.onsetGateRms;   return true; }
    return false;
  }

  // ── dc.* ──
  if (std::strncmp(name, "dc.", 3) == 0) {
    const char* field = name + 3;
    if (std::strcmp(field, "alpha") == 0) { *out = m_cfg.dcAlpha; return true; }
    return false;
  }

  // ── beat.* ──
  if (std::strncmp(name, "beat.", 5) == 0) {
    const char* field = name + 5;
    return m_beatTracker.getParamFloat(field, out);
  }

  return false;
}

// ─── Bin Mapping (dynamic, sample-rate-aware) ───────────────────────

void PipelineCore::initBinMapping() {
  // Hann window coefficients
  constexpr float kPi = 3.14159265358979323846f;
  const size_t win = m_cfg.windowSize;
  const float denom = win > 1 ? static_cast<float>(win - 1) : 1.0f;
  for (size_t i = 0; i < kMaxWindow; ++i) {
    if (i < win) {
      m_windowCoeffs[i] = 0.5f - 0.5f * cosf((2.0f * kPi * static_cast<float>(i)) / denom);
    } else {
      m_windowCoeffs[i] = 0.0f;
    }
  }

  // Dynamic band definitions: compute bin indices from frequency edges
  const float binHz = static_cast<float>(m_cfg.sampleRate) / static_cast<float>(m_cfg.windowSize);
  for (size_t b = 0; b < kBands; ++b) {
    m_bandDefs[b].binLo = static_cast<uint16_t>(kBandEdges[b] / binHz + 0.5f);
    m_bandDefs[b].binHi = static_cast<uint16_t>(kBandEdges[b + 1] / binHz + 0.5f);
    if (m_bandDefs[b].binHi > kNumBins) m_bandDefs[b].binHi = static_cast<uint16_t>(kNumBins);
  }

  // Map chroma frequencies to nearest FFT bin index
  for (size_t i = 0; i < kChroma * kOctaves; ++i) {
    m_chromaBins[i] = static_cast<uint16_t>(
      roundf(kChromaFreqs[i] * static_cast<float>(m_cfg.windowSize) /
                 static_cast<float>(m_cfg.sampleRate)));
  }
}

// ─── Public Interface ──────────────────────────────────────────────

bool PipelineCore::pushSamples(const int16_t* samples, size_t count, uint32_t timestamp_us) {
  bool ready = false;
  const double usPerSample = m_cfg.sampleRate > 0 ? (1000000.0 / static_cast<double>(m_cfg.sampleRate)) : 0.0;
  for (size_t i = 0; i < count; ++i) {
    if (m_hopIndex < m_cfg.hopSize) {
      m_hopBuffer[m_hopIndex++] = samples[i];
    }
    if (m_hopIndex >= m_cfg.hopSize) {
      const double remaining = static_cast<double>((count - 1) - i);
      const double hopTime = static_cast<double>(timestamp_us) - remaining * usPerSample;
      const uint32_t hopTimestamp = static_cast<uint32_t>(llroundf(static_cast<float>(hopTime)));
      processHop(hopTimestamp);
      m_hopIndex = 0;
      ready = true;
    }
  }
  return ready;
}

bool PipelineCore::pullFrame(FeatureFrame& out) {
  if (!m_frameReady) return false;
  out = m_frame;
  m_frame.dropped_blocks_now = 0;
  m_frameReady = false;
  return true;
}

void PipelineCore::setLastProcessUs(uint32_t us) {
  m_frame.process_us = us;
  if (us > m_frame.max_process_us) {
    m_frame.max_process_us = us;
  }
}

void PipelineCore::addDroppedFrame() {
  m_frame.dropped_blocks_total++;
  m_frame.dropped_blocks_now++;
}

// ─── Stage E.3: RMS ────────────────────────────────────────────────

float PipelineCore::computeRms(const int16_t* samples, size_t count) const {
  if (count == 0) return 0.0f;
  double sumSq = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const float s = static_cast<float>(samples[i]);
    sumSq += s * s;
  }
  return clamp01(static_cast<float>(sqrt(sumSq / static_cast<double>(count))) / 32768.0f);
}

// ─── Stage C: Window Assembly + Hann ───────────────────────────────

void PipelineCore::buildWindow() {
  const size_t win = m_cfg.windowSize;
  if (!m_windowFilled && m_windowIndex < win) {
    for (size_t i = 0; i < win; ++i) {
      if (i < m_windowIndex) {
        m_windowTemp[i] = static_cast<float>(m_windowBuffer[i]) * m_windowCoeffs[i];
      } else {
        m_windowTemp[i] = 0.0f;
      }
    }
    return;
  }
  const size_t start = m_windowIndex % win;
  size_t idx = 0;
  for (size_t i = start; i < win; ++i) {
    m_windowTemp[idx] = static_cast<float>(m_windowBuffer[i]) * m_windowCoeffs[idx];
    idx++;
  }
  for (size_t i = 0; i < start; ++i) {
    m_windowTemp[idx] = static_cast<float>(m_windowBuffer[i]) * m_windowCoeffs[idx];
    idx++;
  }
}

// ─── Stage D: FFT / DFT ───────────────────────────────────────────

void PipelineCore::computeMagnitudeSpectrum() {
#if defined(__ARM_ARCH_7EM__)
  // CMSIS-DSP FFT (destroys m_windowTemp input buffer)
  arm_rfft_fast_f32(&m_rfftInst, m_windowTemp, m_fftOut, 0);
  m_magSpectrum[0] = fabsf(m_fftOut[0]);
  arm_cmplx_mag_f32(&m_fftOut[2], &m_magSpectrum[1], kNumBins - 1);
#elif defined(ESP_PLATFORM)
  // Lightweight radix-2 Cooley-Tukey FFT for ESP32-S3
  fft::rfft(m_windowTemp, m_cfg.windowSize);
  fft::magnitudes(m_windowTemp, m_magSpectrum, m_cfg.windowSize);
#else
  // Simple DFT for host tests (O(N*N/2), fine for offline testing).
  constexpr double kPi = 3.14159265358979323846;
  const size_t N = m_cfg.windowSize;
  for (size_t k = 0; k < kNumBins; ++k) {
    double re = 0.0, im = 0.0;
    for (size_t n = 0; n < N; ++n) {
      const double angle = 2.0 * kPi * static_cast<double>(k) * static_cast<double>(n) / static_cast<double>(N);
      re += static_cast<double>(m_windowTemp[n]) * cos(angle);
      im -= static_cast<double>(m_windowTemp[n]) * sin(angle);
    }
    m_magSpectrum[k] = static_cast<float>(sqrt(re * re + im * im));
  }
#endif
}

// ─── Stage E.1: Band Energies ──────────────────────────────────────

void PipelineCore::extractBands(float* bands) const {
  const float norm = 1.0f / (static_cast<float>(m_cfg.windowSize) * 32768.0f);
  for (size_t b = 0; b < kBands; ++b) {
    float sum = 0.0f;
    for (uint16_t k = m_bandDefs[b].binLo; k < m_bandDefs[b].binHi; ++k) {
      sum += m_magSpectrum[k];
    }
    bands[b] = clamp01(sum * norm);
  }
}

// ─── Stage E.2: Chroma ─────────────────────────────────────────────

void PipelineCore::extractChroma(float* chroma) const {
  for (size_t i = 0; i < kChroma; ++i) chroma[i] = 0.0f;
  for (size_t i = 0; i < kChroma * kOctaves; ++i) {
    const uint16_t bin = m_chromaBins[i];
    if (bin < kNumBins) {
      chroma[i % kChroma] += m_magSpectrum[bin];
    }
  }
  float maxC = 0.0f;
  for (size_t i = 0; i < kChroma; ++i) {
    if (chroma[i] > maxC) maxC = chroma[i];
  }
  if (maxC > 0.0f) {
    const float inv = 1.0f / maxC;
    for (size_t i = 0; i < kChroma; ++i) chroma[i] *= inv;
  }
}

// ─── Stage F.1: Log-Magnitude Spectral Flux ────────────────────────

float PipelineCore::computeLogFlux() {
  // Normalise magnitudes before differencing so flux scale is stable across
  // window/sample-rate variants. This keeps novelty in a usable dynamic range
  // for BeatTracker and ControlBus.
  const float magNorm = 1.0f / (static_cast<float>(m_cfg.windowSize) * 32768.0f);
  constexpr float kMagFloor = 1e-6f;

  if (!m_hasPrevMag) {
    m_prevLogMag[0] = 0.0f;  // ignore DC bin
    for (size_t k = 1; k < kNumBins; ++k) {
      const float mag = fmaxf(m_magSpectrum[k] * magNorm, kMagFloor);
      m_prevLogMag[k] = log1pf(mag);
    }
    m_hasPrevMag = true;
    return 0.0f;
  }

  float flux = 0.0f;
  for (size_t k = 1; k < kNumBins; ++k) {
    const float mag = fmaxf(m_magSpectrum[k] * magNorm, kMagFloor);
    const float logMag = log1pf(mag);
    const float diff = logMag - m_prevLogMag[k];
    if (diff > 0.0f) flux += diff;
    m_prevLogMag[k] = logMag;
  }

  // Average over bins to avoid sum-scaling with FFT size.
  return flux / static_cast<float>(kNumBins - 1);
}

// ─── Stage F.3: Adaptive Threshold → Onset Envelope ────────────────

float PipelineCore::computeOnsetEnv(float flux) {
  m_onsetMean += m_cfg.onsetMeanAlpha * (flux - m_onsetMean);
  const float diff = flux - m_onsetMean;
  m_onsetVar += m_cfg.onsetVarAlpha * (diff * diff - m_onsetVar);
  const float stddev = sqrtf(fmaxf(m_onsetVar, 0.0f));
  const float threshold = m_onsetMean + m_cfg.onsetK * stddev;
  const float env = flux - threshold;
  return env > 0.0f ? env : 0.0f;
}

// ─── Stage G: Peak Picking (isolated, canonical 3-condition) ───────

float PipelineCore::peakPickUpdate(float onsetEnv) {
  const auto& cfg = m_cfg.peakPick;

  m_peakBuf[m_peakWriteIdx] = onsetEnv;
  m_peakWriteIdx = (m_peakWriteIdx + 1) % kPeakBufSize;
  m_peakFrameCounter++;

  const uint16_t delay = cfg.postMax > cfg.postAvg ? cfg.postMax : cfg.postAvg;
  if (m_peakFrameCounter <= delay) return 0.0f;

  auto idx = [&](int offset) -> size_t {
    const int base = static_cast<int>(m_peakWriteIdx) - static_cast<int>(delay) + offset;
    return static_cast<size_t>(((base % static_cast<int>(kPeakBufSize)) + static_cast<int>(kPeakBufSize)) % static_cast<int>(kPeakBufSize));
  };

  const float candidate = m_peakBuf[idx(0)];
  if (candidate <= 0.0f) return 0.0f;

  for (int i = -static_cast<int>(cfg.preMax); i <= static_cast<int>(cfg.postMax); ++i) {
    if (i == 0) continue;
    if (m_peakBuf[idx(i)] > candidate) return 0.0f;
  }

  float sum = 0.0f;
  const int avgCount = static_cast<int>(cfg.preAvg) + static_cast<int>(cfg.postAvg) + 1;
  for (int i = -static_cast<int>(cfg.preAvg); i <= static_cast<int>(cfg.postAvg); ++i) {
    sum += m_peakBuf[idx(i)];
  }
  const float localMean = sum / static_cast<float>(avgCount);
  if (candidate < localMean + cfg.delta) return 0.0f;

  const uint32_t candidateFrame = m_peakFrameCounter - delay;
  if (m_peakHasEvent && (candidateFrame - m_peakLastEventFrame) < cfg.wait) return 0.0f;

  m_peakLastEventFrame = candidateFrame;
  m_peakHasEvent = true;
  return candidate;
}

// ─── Main Processing Pipeline ──────────────────────────────────────

void PipelineCore::processHop(uint32_t timestamp_us) {
  m_seq++;
  m_frame.seq = m_seq;
  m_frame.timestamp_us = timestamp_us;

  // ── Stage B: DC Removal (in-place on hop buffer) ──
  if (m_cfg.stages.enableDc) {
    for (size_t i = 0; i < m_cfg.hopSize; ++i) {
      float x = static_cast<float>(m_hopBuffer[i]);
      m_dcEstimate += m_cfg.dcAlpha * (x - m_dcEstimate);
      x -= m_dcEstimate;
      int32_t xi = static_cast<int32_t>(lroundf(x));
      if (xi < -32768) xi = -32768;
      if (xi > 32767) xi = 32767;
      m_hopBuffer[i] = static_cast<int16_t>(xi);
    }
  }

  // ── Stage E.3: RMS (from DC-removed hop buffer) ──
  m_frame.rms = m_cfg.stages.enableRms ? computeRms(m_hopBuffer, m_cfg.hopSize) : 0.0f;

  // ── Stage E.4: Peak (max |sample| in hop) ──
  int32_t peakVal = 0;
  for (size_t i = 0; i < m_cfg.hopSize; ++i) {
    const int32_t absx = m_hopBuffer[i] < 0 ? -static_cast<int32_t>(m_hopBuffer[i]) : static_cast<int32_t>(m_hopBuffer[i]);
    if (absx > peakVal) peakVal = absx;
  }
  m_frame.peak = clamp01(static_cast<float>(peakVal) / 32768.0f);

  // ── Stage C: Window Assembly (circular buffer + Hann) ──
  for (size_t i = 0; i < m_cfg.hopSize; ++i) {
    m_windowBuffer[m_windowIndex++] = m_hopBuffer[i];
    if (m_windowIndex >= m_cfg.windowSize) {
      m_windowIndex = 0;
      m_windowFilled = true;
    }
  }
  buildWindow();

  // ── Stage D: FFT → magnitude spectrum ──
  computeMagnitudeSpectrum();

  // ── Stage F.2: Adaptive whitening (optional) ──
  if (m_cfg.stages.enableWhitening) {
    for (size_t k = 0; k < kNumBins; ++k) {
      m_whitenPeak[k] = fmaxf(m_magSpectrum[k], m_cfg.whitenDecay * m_whitenPeak[k]);
      m_whitenPeak[k] = fmaxf(m_whitenPeak[k], m_cfg.whitenFloor);
      m_magSpectrum[k] /= m_whitenPeak[k];
    }
  }

  // ── Stage E.1: Band energies ──
  float bands[kBands] = {0};
  if (m_cfg.stages.enableBands) {
    extractBands(bands);
  }
  for (size_t i = 0; i < kBands; ++i) m_frame.bands[i] = bands[i];

  // ── Stage E.2: Chroma ──
  float chroma[kChroma] = {0};
  if (m_cfg.stages.enableChroma) {
    extractChroma(chroma);
  }
  for (size_t i = 0; i < kChroma; ++i) m_frame.chroma[i] = chroma[i];

  // ── Stage F: Onset Envelope (log-magnitude spectral flux) ──
  const float flux = computeLogFlux();
  m_frame.flux = flux;
  const float rawOnsetEnv = computeOnsetEnv(flux);
  const float onsetEnv = (m_cfg.onsetGateRms > 0.0f && m_frame.rms < m_cfg.onsetGateRms)
      ? 0.0f : rawOnsetEnv;
  m_frame.onset_env = onsetEnv;

  // ── Stage G: Peak Picking (discrete events) ──
  m_frame.onset_event = peakPickUpdate(onsetEnv);

  // ── Stage H: Beat Tracking ──
  m_beatTracker.update(onsetEnv);
  m_frame.tempo_bpm  = m_beatTracker.tempoBpm();
  m_frame.tempo_confidence = m_beatTracker.tempoConfidence();
  m_frame.tempo_locked = m_beatTracker.tempoLocked() ? 1.0f : 0.0f;
  m_frame.beat_phase = m_beatTracker.beatPhase();
  m_frame.beat_event = m_beatTracker.beatEvent() ? 1.0f : 0.0f;

  m_frameReady = true;
}
