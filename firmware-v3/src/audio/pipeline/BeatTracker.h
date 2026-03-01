#pragma once

#include <cstddef>
#include <cstdint>

struct BeatConfig {
  float tempoMinBpm    = 60.0f;    // minimum tracked tempo
  float tempoMaxBpm    = 240.0f;   // maximum tracked tempo
  float tempoPriorBpm  = 120.0f;   // log-Gaussian prior center (resolves octave ambiguity)
  float tempoPriorWidth = 50.0f;   // flat prior (no octave bias)
  float tempoDecay     = 0.95f;    // fast histogram adaptation
  float cbssAlpha      = 0.9f;     // CBSS historical contribution weight
  float minBeatFactor  = 0.75f;    // refractory = factor * T  (blocks comb harmonic at T/2)

  // Watchdog: force re-acquire after N consecutive low-confidence tempo updates
  uint8_t watchdogCycles = 4;      // cycles at low confidence before histogram reset (~6s)
  float   watchdogThresh = 0.08f;  // confidence below this triggers watchdog counter

};

class BeatTracker {
public:
  static constexpr size_t kOssLen      = 512;  // ~3s of onset history at 172 Hz hop rate
  static constexpr size_t kMaxLag      = 256;  // max autocorrelation lag
  static constexpr size_t kCbssLen     = 256;  // CBSS ring buffer length

  BeatTracker();

  void reset();
  void setConfig(const BeatConfig& cfg, uint32_t sampleRate, uint16_t hopSize);

  // Call once per hop with onset envelope and bass onset.
  // onset_env: full-band adaptive-threshold onset (used for CBSS phase)
  // bass_onset: low-frequency onset strength (used for tempo estimation)
  void update(float onset_env, float bass_onset = -1.0f);

  // ── Hot-reload parameter setter (no reset) ──────────────────────
  bool setParamFloat(const char* name, float value);

  // Read back current config value by name.
  bool getParamFloat(const char* name, float* out) const;

  // Output accessors
  float tempoBpm()  const { return m_tempoBpm; }
  float beatPhase() const { return m_beatPhase; }
  bool  beatEvent() const { return m_beatEvent; }
  float tempoConfidence() const { return m_tempoConfidence; }
  bool  tempoLocked() const { return m_tempoLocked; }

private:
  void updateTempoEstimate();
  void recalcLagBounds();

  BeatConfig m_cfg{};
  float m_hopRate = 172.265625f;  // default: 44100 / 256

  // Onset signal ring buffer (for autocorrelation)
  float m_oss[kOssLen];
  size_t m_ossIdx = 0;

  // Tempo histogram (decaying Gaussian mixture, indexed by lag)
  float m_histogram[kMaxLag];
  int m_minLag = 0;
  int m_maxLag = 0;

  // Cumulative Beat Strength Signal ring buffer
  float m_cbss[kCbssLen];
  size_t m_cbssIdx = 0;
  float m_cbssPrev = 0.0f;
  float m_cbssPrevPrev = 0.0f;

  // Beat state
  float m_tempoBpm = 120.0f;
  int m_beatPeriodHops = 86;
  float m_beatPhase = 0.0f;
  bool m_beatEvent = false;
  float m_tempoConfidence = 0.0f;
  bool m_tempoLocked = false;
  uint32_t m_beatEventCount = 0;

  // Refractory: minimum interval between beat events
  uint32_t m_totalHops = 0;
  uint32_t m_lastBeatHop = 0;
  uint32_t m_hopsSinceBeat = 0;

  // Onset sparsification (adaptive threshold for OSS storage)
  float m_ossMean = 0.0f;

  // Watchdog: consecutive low-confidence tempo update cycles
  uint8_t m_watchdogCount = 0;

  // Scheduling
  uint32_t m_hopCount = 0;
  uint32_t m_tempoInterval = 258;  // ~1.5s between tempo updates
};
