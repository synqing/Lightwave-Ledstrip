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
  m_ossMean = 0.0f;
  m_watchdogCount = 0;
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
  if (std::strcmp(name, "watchdogThresh") == 0) {
    m_cfg.watchdogThresh = value;
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
  if (std::strcmp(name, "watchdogThresh") == 0)    { *out = m_cfg.watchdogThresh;   return true; }
  return false;
}

// ── Beat tracking core ────────────────────────────────────────────

void BeatTracker::update(float onset_env, float bass_onset) {
  if (onset_env < 0.0f || !std::isfinite(onset_env)) onset_env = 0.0f;
  if (bass_onset < 0.0f) bass_onset = onset_env;  // fallback: use full-band
  if (!std::isfinite(bass_onset)) bass_onset = 0.0f;

  // Store bass onset in OSS for tempo estimation.  Bass (kick drum) energy
  // provides cleaner periodicity than full-band onset which includes hi-hats.
  // No sparsification: bass flux is naturally sparse (only nonzero during
  // low-frequency transients). Thresholding hurts real audio scoring.
  m_oss[m_ossIdx] = bass_onset;
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

  // ── Comb filter scoring with on-beat/off-beat contrast ──
  // For each candidate lag, measure how much stronger the onset energy
  // is at regular beat positions vs between beats.  Contrast-based scoring
  // is robust to dense onset patterns (hi-hats, tremolo) that inflate
  // raw comb sums at short lags.

  // Compute total onset energy for contrast denominator
  float totalEnergy = 0.0f;
  for (size_t i = 0; i < kOssLen; ++i) totalEnergy += linear[i];
  const float meanEnergy = totalEnergy / static_cast<float>(kOssLen);

  float comb[kMaxLag] = {};
  float combMax = 0.0f;

  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    float onBeat = 0.0f;
    int teeth = 0;
    // Walk backward from most recent, one period at a time
    for (int k = 1; ; ++k) {
      int center = static_cast<int>(kOssLen) - 1 - k * lag;
      if (center < 2) break;
      // ±4 hop max-pool around each comb tooth (handles ~32ms groove jitter)
      float v = linear[center];
      for (int j = -4; j <= 4; ++j) {
        int idx = center + j;
        if (idx >= 0 && idx < static_cast<int>(kOssLen) && linear[idx] > v)
          v = linear[idx];
      }
      onBeat += v;
      teeth++;
    }
    if (teeth >= 3) {
      // Contrast: how much of the onset energy concentrates on beat positions
      float meanOnBeat = onBeat / static_cast<float>(teeth);
      comb[lag] = meanOnBeat / (meanEnergy + 1e-9f);
    }
    if (comb[lag] > combMax) combMax = comb[lag];
  }

  if (combMax <= 1.0f) {  // contrast ratio <= 1 means no periodicity
    m_tempoConfidence *= 0.95f;
    if (m_tempoConfidence < 0.25f) m_tempoLocked = false;
    if (!m_tempoLocked) {
      m_tempoBpm = m_cfg.tempoPriorBpm;
      m_beatPeriodHops = static_cast<int>(roundf(60.0f * m_hopRate / m_tempoBpm));
    }
    return;
  }

  // ── Harmonic-enhanced comb scores ──
  // Boost candidates whose integer multiples also show periodicity.
  // Penalize candidates that look like harmonics of a longer period.
  float combEnh[kMaxLag] = {};
  float enhMax = 0.0f;
  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    float score = comb[lag];
    // Reinforce with 2× and 3× period (sub-harmonics in time)
    if (lag * 2 <= m_maxLag) score += 0.5f * comb[lag * 2];
    if (lag * 3 <= m_maxLag) score += 0.25f * comb[lag * 3];
    // Penalize if half-period also scores well (we may be a sub-harmonic)
    const int halfLag = lag / 2;
    if (halfLag >= m_minLag && comb[halfLag] > comb[lag] * 0.7f) {
      score *= 0.6f;
    }
    combEnh[lag] = score;
    if (score > enhMax) enhMax = score;
  }

  // Decay existing histogram
  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    m_histogram[lag] *= m_cfg.tempoDecay;
  }

  // Add comb scores to histogram (all candidates, weighted by prior)
  const float widthSq = m_cfg.tempoPriorWidth * m_cfg.tempoPriorWidth;
  for (int lag = m_minLag; lag <= m_maxLag; ++lag) {
    if (combEnh[lag] > enhMax * 0.1f) {
      const float bpm = 60.0f * m_hopRate / static_cast<float>(lag);
      const float logRatio = log2f(bpm / m_cfg.tempoPriorBpm);
      const float prior = expf(-logRatio * logRatio / (2.0f * widthSq));
      m_histogram[lag] += combEnh[lag] * prior;
    }
  }

  // ── Extract tempo from histogram peak ──

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

  // ── Confidence from histogram shape ──
  const float histShare = bestVal / (histSum + 1e-9f);
  const float histShareNorm = clamp01((histShare - 0.04f) / 0.24f);
  const float prominence = clamp01((bestVal - secondVal) / (bestVal + 1e-9f));
  const float periodicity = clamp01(
      combEnh[m_beatPeriodHops] / (enhMax + 1e-9f));
  const float qualityInstant = clamp01(
      0.40f * histShareNorm +
      0.35f * prominence +
      0.25f * periodicity);

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

  // ── Watchdog: force re-acquire if stuck at low confidence ──
  if (m_tempoConfidence < m_cfg.watchdogThresh) {
    m_watchdogCount++;
    if (m_watchdogCount >= m_cfg.watchdogCycles) {
      std::memset(m_histogram, 0, sizeof(m_histogram));
      m_tempoLocked = false;
      m_tempoBpm = m_cfg.tempoPriorBpm;
      m_beatPeriodHops = static_cast<int>(roundf(60.0f * m_hopRate / m_tempoBpm));
      m_watchdogCount = 0;
    }
  } else {
    m_watchdogCount = 0;
  }
}
