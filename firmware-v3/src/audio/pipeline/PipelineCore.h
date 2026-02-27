#pragma once

#include <cstddef>
#include <cstdint>

// Platform-specific FFT headers
#if defined(__ARM_ARCH_7EM__)
  #include <arm_math.h>
  #include <arm_const_structs.h>
#elif defined(ESP_PLATFORM)
  #include "FFT.h"
#endif

#include "BeatTracker.h"

// Stage enable flags (onset envelope and peak picking are always enabled)
struct StageFlags {
  bool enableDc       = true;
  bool enableBands    = true;
  bool enableChroma   = true;
  bool enableRms      = true;
  bool enableWhitening = false;
};

// Peak picker configuration (canonical 3-condition test)
struct PeakPickConfig {
  uint16_t preMax  = 3;     // local max lookback (frames)
  uint16_t postMax = 1;     // local max lookahead (= latency in frames)
  uint16_t preAvg  = 10;    // local mean lookback (frames)
  uint16_t postAvg = 1;     // local mean lookahead (frames)
  float    delta   = 2.0f;  // absolute onset_env floor (suppresses leakage noise)
  uint16_t wait    = 8;     // minimum inter-event spacing (frames)
};

struct PipelineConfig {
  // Framing (architectural constants)
  uint32_t sampleRate  = 44100;
  uint16_t hopSize     = 256;
  uint16_t windowSize  = 512;

  // DC removal
  float dcAlpha = 0.001f;

  // Onset envelope
  float onsetMeanAlpha = 0.01f;
  float onsetVarAlpha  = 0.01f;
  float onsetK         = 1.5f;
  float onsetGateRms   = 0.0018f; // RMS gate (~-55 dBFS), 0 = disabled

  // Adaptive whitening (optional)
  float whitenDecay = 0.997f;
  float whitenFloor = 1e-6f;

  // Peak picking
  PeakPickConfig peakPick{};

  // Beat tracking
  BeatConfig beat{};

  // Stage enables
  StageFlags stages{};
};

struct FeatureFrame {
  // Identity
  uint32_t seq          = 0;
  uint32_t timestamp_us = 0;

  // Time-domain features
  float rms  = 0.0f;   // [0, 1]
  float peak = 0.0f;   // [0, 1]

  // Spectral features
  float bands[8]   = {0};  // band energies
  float chroma[12] = {0};  // chroma profile

  // Onset detection (the critical semantic split)
  float flux        = 0.0f;  // raw log spectral flux (before threshold)
  float onset_env   = 0.0f;  // continuous onset strength (thresholded)
  float onset_event = 0.0f;  // discrete: 0 unless peak detected, then = env value

  // Beat tracking
  float tempo_bpm   = 120.0f;  // estimated tempo (BPM)
  float tempo_confidence = 0.0f; // tempo confidence [0,1]
  float tempo_locked = 0.0f;   // 1.0 when lock is stable, else 0.0
  float beat_phase  = 0.0f;    // beat phase [0, 1), resets on beat
  float beat_event  = 0.0f;    // 0 unless beat detected, then 1.0

  // Diagnostics
  uint32_t process_us           = 0;
  uint32_t max_process_us       = 0;
  uint32_t dropped_blocks_total = 0;
  uint32_t dropped_blocks_now   = 0;
};

// FFT bin range for each frequency band
struct BandDef {
  uint16_t binLo;  // inclusive
  uint16_t binHi;  // exclusive
};

class PipelineCore {
public:
  static constexpr size_t kBands      = 8;
  static constexpr size_t kChroma     = 12;
  static constexpr size_t kOctaves    = 4;
  static constexpr size_t kMaxHop     = 256;
  static constexpr size_t kMaxWindow  = 512;
  static constexpr size_t kNumBins    = kMaxWindow / 2;  // 256 FFT magnitude bins
  static constexpr size_t kPeakBufSize = 32;

  PipelineCore();

  void reset();
  void setConfig(const PipelineConfig& cfg);
  const PipelineConfig& getConfig() const;

  bool pushSamples(const int16_t* samples, size_t count, uint32_t timestamp_us);
  bool pullFrame(FeatureFrame& out);
  void setLastProcessUs(uint32_t us);
  void addDroppedFrame();

  bool setParamFloat(const char* name, float value);
  bool getParamFloat(const char* name, float* out) const;

  // Raw buffer accessors — needed by PipelineAdapter for full-resolution
  // spectrum queries and waveform forwarding. Only valid after pullFrame()
  // returns true (buffers hold data from the most recently processed hop).
  const float*   getMagnitudeSpectrum() const { return m_magSpectrum; }
  const int16_t* getHopBuffer()        const { return m_hopBuffer; }

private:
  void initBinMapping();
  void processHop(uint32_t timestamp_us);
  float computeRms(const int16_t* samples, size_t count) const;
  void buildWindow();
  void computeMagnitudeSpectrum();
  void extractBands(float* bands) const;
  void extractChroma(float* chroma) const;
  float computeLogFlux();
  float computeOnsetEnv(float flux);
  float peakPickUpdate(float onsetEnv);

  PipelineConfig m_cfg{};
  FeatureFrame m_frame{};
  bool m_frameReady = false;

  // Stage A: Hop accumulator
  int16_t m_hopBuffer[kMaxHop];
  size_t m_hopIndex = 0;

  // Stage B: DC removal
  float m_dcEstimate = 0.0f;

  // Stage C: Window assembly
  int16_t m_windowBuffer[kMaxWindow];
  size_t m_windowIndex = 0;
  bool m_windowFilled = false;
  float m_windowTemp[kMaxWindow];
  float m_windowCoeffs[kMaxWindow];

  // Stage D: FFT
#if defined(__ARM_ARCH_7EM__)
  arm_rfft_fast_instance_f32 m_rfftInst;
#endif
  float m_fftOut[kMaxWindow];
  float m_magSpectrum[kNumBins];

  // Stage E: Feature extraction (bin mapping)
  BandDef m_bandDefs[kBands];              // dynamic: computed from sample rate
  uint16_t m_chromaBins[kChroma * kOctaves];

  // Stage F: Onset envelope (log-magnitude spectral flux)
  float m_prevLogMag[kNumBins];
  bool m_hasPrevMag = false;
  float m_onsetMean = 0.0f;
  float m_onsetVar  = 0.0f;

  // Stage F.2: Adaptive whitening (optional)
  float m_whitenPeak[kNumBins];

  // Stage G: Peak picker (isolated state)
  float m_peakBuf[kPeakBufSize];
  size_t m_peakWriteIdx = 0;
  uint32_t m_peakFrameCounter = 0;
  uint32_t m_peakLastEventFrame = 0;
  bool m_peakHasEvent = false;

  uint32_t m_seq = 0;

  // Stage H: Beat tracker
  BeatTracker m_beatTracker;
};
