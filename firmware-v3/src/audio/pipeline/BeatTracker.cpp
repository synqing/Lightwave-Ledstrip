#include "BeatTracker.h"

#include <cmath>
#include <cstring>

namespace {
inline float clamp01(float x) {
  if (x < 0.0f) return 0.0f;
  if (x > 1.0f) return 1.0f;
  return x;
}
}

BeatTracker::BeatTracker() {
  std::memset(m_oss, 0, sizeof(m_oss));
  std::memset(m_histogram, 0, sizeof(m_histogram));
  std::memset(m_cbss, 0, sizeof(m_cbss));
}

void BeatTracker::reset() {
  std::memset(m_oss, 0, sizeof(m_oss));
  std::memset(m_histogram, 0, sizeof(m_histogram));
  std::memset(m_cbss, 0, sizeof(m_cbss));
  m_ossIdx = 0;
  m_cbssIdx = 0;
  m_cbssPrev = 0.0f;
  m_cbssPrevPrev = 0.0f;
  m_tempoBpm = m_cfg.tempoPriorBpm;
  m_beatPeriodHops = static_cast<int>(roundf(60.0f * m_hopRate / m_tempoBpm));
  m_beatPhase = 0.0f;
  m_beatEvent = false;
  m_tempoConfidence = 0.0f;
  m_tempoLocked = false;
  m_beatEventCount = 0;
  m_totalHops = 0;
  m_lastBeatHop = 0;
  m_hopsSinceBeat = 0;
  m_hopCount = 0;
}

void BeatTracker::setConfig(const BeatConfig& cfg, uint32_t sampleRate, uint16_t hopSize) {
  m_cfg = cfg;
  m_hopRate = static_cast<float>(sampleRate) / static_cast<float>(hopSize);

  recalcLagBounds();

  m_tempoInterval = static_cast<uint32_t>(1.5f * m_hopRate);
  reset();
}

// ── Lag bounds recalculation (shared by setConfig and hot-reload) ──

void BeatTracker::recalcLagBounds() {
  m_maxLag = static_cast<int>(60.0f * m_hopRate / m_cfg.tempoMinBpm);
  m_minLag = static_cast<int>(60.0f * m_hopRate / m_cfg.tempoMaxBpm);
  if (m_minLag < 2) m_minLag = 2;
  if (m_maxLag >= static_cast<int>(kMaxLag)) m_maxLag = static_cast<int>(kMaxLag) - 1;
  if (m_maxLag >= static_cast<int>(kOssLen) / 2) m_maxLag = static_cast<int>(kOssLen) / 2 - 1;
}

// ── Hot-reload parameter accessors ────────────────────────────────

bool BeatTracker::setParamFloat(const char* name, float value) {
  if (std::strcmp(name, "minBeatFactor") == 0) {
    m_cfg.minBeatFactor = value;
    return true;
  }
  if (std::strcmp(name, "cbssAlpha") == 0) {
    m_cfg.cbssAlpha = value;
    return true;
  }
  if (std::strcmp(name, "tempoPriorBpm") == 0) {
    m_cfg.tempoPriorBpm = value;
    return true;
  }
  if (std::strcmp(name, "tempoPriorWidth") == 0) {
    m_cfg.tempoPriorWidth = value;
    return true;
  }
  if (std::strcmp(name, "tempoDecay") == 0) {
    m_cfg.tempoDecay = value;
    return true;
  }
  if (std::strcmp(name, "tempoMinBpm") == 0) {
    m_cfg.tempoMinBpm = value;
    recalcLagBounds();
    return true;
  }
  if (std::strcmp(name, "tempoMaxBpm") == 0) {
    m_cfg.tempoMaxBpm = value;
    recalcLagBounds();
    return true;
  }
  return false;
}

bool BeatTracker::getParamFloat(const char* name, float* out) const {
  if (std::strcmp(name, "minBeatFactor") == 0) { *out = m_cfg.minBeatFactor; return true; }
  if (std::strcmp(name, "cbssAlpha") == 0)      { *out = m_cfg.cbssAlpha;      return true; }
  if (std::strcmp(name, "tempoPriorBpm") == 0)   { *out = m_cfg.tempoPriorBpm;  return true; }
  if (std::strcmp(name, "tempoPriorWidth") == 0) { *out = m_cfg.tempoPriorWidth; return true; }
  if (std::strcmp(name, "tempoDecay") == 0)      { *out = m_cfg.tempoDecay;     return true; }
  if (std::strcmp(name, "tempoMinBpm") == 0)     { *out = m_cfg.tempoMinBpm;    return true; }
  if (std::strcmp(name, "tempoMaxBpm") == 0)     { *out = m_cfg.tempoMaxBpm;    return true; }
  return false;
}

// ── Beat tracking core ────────────────────────────────────────────

void BeatTracker::update(float onset_env) {
  if (onset_env < 0.0f || !std::isfinite(onset_env)) onset_env = 0.0f;

  // Store in onset signal buffer
  m_oss[m_ossIdx] = onset_env;
  m_ossIdx = (m_ossIdx + 1) % kOssLen;
  m_totalHops++;
  m_hopsSinceBeat++;

  // ── CBSS beat phase tracking ──

  m_beatEvent = false;
  const float phaseInc = 1.0f / static_cast<float>(m_beatPeriodHops);
  m_beatPhase += phaseInc;

  // CBSS update: cbss[n] = alpha * cbss[n - T] + (1 - alpha) * onset_env
  int prevOffset = static_cast<int>(m_cbssIdx) - m_beatPeriodHops;
  if (prevOffset < 0) prevOffset += static_cast<int>(kCbssLen);
  const float prevCbss = m_cbss[static_cast<size_t>(prevOffset) % kCbssLen];
  const float cbssNew = m_cfg.cbssAlpha * prevCbss +
                        (1.0f - m_cfg.cbssAlpha) * onset_env;
  m_cbss[m_cbssIdx] = cbssNew;
  m_cbssIdx = (m_cbssIdx + 1) % kCbssLen;

  // Detect CBSS peak (1-hop delay to see both neighbors)
  const bool cbssPeak = (m_cbssPrev > m_cbssPrevPrev &&
                         m_cbssPrev > cbssNew &&
                         m_cbssPrev > 1e-3f);

  // Refractory: minimum inter-beat interval as fraction of beat period.
  const uint32_t minInterval = static_cast<uint32_t>(
      static_cast<float>(m_beatPeriodHops) * m_cfg.minBeatFactor);

  if (cbssPeak) {
    if ((m_totalHops - m_lastBeatHop) > minInterval) {
      m_beatEvent = true;
      m_beatPhase = 0.0f;
      m_lastBeatHop = m_totalHops;
      m_beatEventCount++;
      m_hopsSinceBeat = 0;
    } else {
      m_cbssPrevPrev = cbssNew;
      m_cbssPrev = cbssNew;
      const size_t peakPos = (m_cbssIdx + kCbssLen - 2) % kCbssLen;
      m_cbss[peakPos] = 0.0f;
    }
  }

  // Clamp phase (prevents runaway in silence)
  if (m_beatPhase > 1.5f) m_beatPhase = 1.0f;

  if (!cbssPeak || m_beatEvent) {
    m_cbssPrevPrev = m_cbssPrev;
    m_cbssPrev = cbssNew;
  }

  // ── Periodic tempo estimation ──

  m_hopCount++;
  if (m_hopCount >= m_tempoInterval) {
    m_hopCount = 0;
    updateTempoEstimate();
  }
}

void BeatTracker::updateTempoEstimate() {
  // Linearize ring buffer (oldest → newest)
  float linear[kOssLen];
  for (size_t i = 0; i < kOssLen; ++i) {
    linear[i] = m_oss[(m_ossIdx + i) % kOssLen];
  }

  // Direct autocorrelation over candidate lag range
  float acfMax = 0.0f;
  float acf[kMaxLag] = {};
  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    float sum = 0.0f;
    for (size_t n = static_cast<size_t>(lag); n < kOssLen; ++n) {
      sum += linear[n] * linear[n - static_cast<size_t>(lag)];
    }
    acf[lag] = sum;
    if (sum > acfMax) acfMax = sum;
  }

  if (acfMax <= 0.0f) {
    m_tempoConfidence *= 0.95f;
    if (m_tempoConfidence < 0.25f) m_tempoLocked = false;
    if (!m_tempoLocked) {
      m_tempoBpm = m_cfg.tempoPriorBpm;
      m_beatPeriodHops = static_cast<int>(roundf(60.0f * m_hopRate / m_tempoBpm));
    }
    return;
  }

  // ── Harmonic-enhanced autocorrelation ──
  float acfEnh[kMaxLag] = {};
  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    float score = acf[lag];
    const int half = lag / 2;
    if (half >= m_minLag) score += 0.15f * acf[half];
    const int third = lag / 3;
    if (third >= m_minLag) score += 0.05f * acf[third];
    acfEnh[lag] = score;
  }

  // Decay existing histogram
  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    m_histogram[lag] *= m_cfg.tempoDecay;
  }

  // Find peaks in enhanced ACF and add weighted entries to histogram
  float enhMax = 0.0f;
  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    if (acfEnh[lag] > enhMax) enhMax = acfEnh[lag];
  }
  const float threshold = enhMax * 0.3f;
  const float widthSq = m_cfg.tempoPriorWidth * m_cfg.tempoPriorWidth;

  for (int lag = m_minLag + 1; lag < m_maxLag; ++lag) {
    if (acfEnh[lag] > acfEnh[lag - 1] && acfEnh[lag] > acfEnh[lag + 1] && acfEnh[lag] > threshold) {
      const float bpm = 60.0f * m_hopRate / static_cast<float>(lag);
      const float logRatio = log2f(bpm / m_cfg.tempoPriorBpm);
      const float prior = expf(-logRatio * logRatio / (2.0f * widthSq));
      m_histogram[lag] += acfEnh[lag] * prior;
    }
  }

  // Extract tempo from histogram peak
  float bestVal = 0.0f;
  float secondVal = 0.0f;
  int bestLag = m_beatPeriodHops;
  float histSum = 0.0f;
  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    const float v = m_histogram[lag];
    histSum += v;
    if (v > bestVal) {
      secondVal = bestVal;
      bestVal = v;
      bestLag = lag;
    } else if (v > secondVal) {
      secondVal = v;
    }
  }

  const bool hasRecentBeat = m_hopsSinceBeat <= static_cast<uint32_t>(m_beatPeriodHops * 4);
  if (bestVal > 0.0f && hasRecentBeat) {
    const float bpm = 60.0f * m_hopRate / static_cast<float>(bestLag);
    if (std::isfinite(bpm)) {
      m_tempoBpm = bpm;
      m_beatPeriodHops = bestLag;
    }
  }

  const float histShare = bestVal / (histSum + 1e-9f);
  const float histShareNorm = clamp01((histShare - 0.04f) / 0.24f);
  const float prominence = clamp01((bestVal - secondVal) / (bestVal + 1e-9f));
  const float periodicity = clamp01(acfEnh[bestLag] / (enhMax + 1e-9f));
  const float qualityInstant = clamp01(
      0.40f * histShareNorm +
      0.35f * prominence +
      0.25f * periodicity);

  // Smooth confidence to avoid frame-to-frame lock chatter.
  m_tempoConfidence = m_tempoConfidence * 0.88f + qualityInstant * 0.12f;

  if (!hasRecentBeat) {
    m_tempoConfidence *= 0.88f;
    if (!m_tempoLocked) {
      m_tempoBpm = m_cfg.tempoPriorBpm;
      m_beatPeriodHops = static_cast<int>(roundf(60.0f * m_hopRate / m_tempoBpm));
    }
  }

  const bool enoughWarmup = m_totalHops >= static_cast<uint32_t>(m_hopRate * 2.0f);
  const bool enoughBeats = m_beatEventCount >= 2;
  if (!m_tempoLocked) {
    if (enoughWarmup && enoughBeats && m_tempoConfidence >= 0.50f) {
      m_tempoLocked = true;
    }
  } else {
    if (m_tempoConfidence < 0.30f) {
      m_tempoLocked = false;
    }
  }

  if (m_tempoBpm < m_cfg.tempoMinBpm || m_tempoBpm > m_cfg.tempoMaxBpm) {
    m_tempoLocked = false;
    m_tempoConfidence *= 0.8f;
  }
}
