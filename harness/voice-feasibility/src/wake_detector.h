/**
 * @file wake_detector.h
 * @brief Lightweight wake word detector using TFLite Micro + microWakeWord models
 *
 * Ported from microWakeWord (Kevin Ahrendt / OHF-Voice) standalone component.
 * Stripped of ESPHome scaffolding — plain C++ for Arduino/PlatformIO.
 *
 * Usage:
 *   WakeDetector detector;
 *   detector.begin(model_data, model_size, arena_size, 0.97f, 5, 10);
 *   // In audio loop:
 *   detector.feedSamples(pcm16_buf, num_samples);
 *   if (detector.detected()) { ... detector.reset(); }
 */
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>

// Audio frontend from ESPMicroSpeechFeatures
#include <frontend.h>
#include <frontend_util.h>

class WakeDetector {
public:
    WakeDetector() = default;
    ~WakeDetector();

    /**
     * @brief Initialise the detector with a TFLite model
     * @param modelData      Pointer to the .tflite model bytes (in flash/PROGMEM)
     * @param tensorArenaSize Size of tensor arena in bytes (from model JSON)
     * @param probCutoff     Detection threshold (0.0-1.0, e.g. 0.97)
     * @param slidingWindowSize Number of recent probabilities to average
     * @param featureStepMs  Feature step size in ms (usually 10)
     * @return true if initialisation succeeded
     */
    bool begin(const uint8_t *modelData, size_t tensorArenaSize,
               float probCutoff = 0.97f, size_t slidingWindowSize = 5,
               int featureStepMs = 10);

    /**
     * @brief Feed raw 16-bit PCM audio samples (16 kHz, mono)
     * @param samples Pointer to int16_t audio buffer
     * @param count   Number of samples (not bytes)
     */
    void feedSamples(const int16_t *samples, size_t count);

    /**
     * @brief Check if wake word was detected since last reset
     * @return true if detected
     */
    bool detected() const { return detected_; }

    /**
     * @brief Get the current sliding window probability (0.0-1.0)
     */
    float currentProbability() const { return lastProb_; }

    /**
     * @brief Reset detection state to listen again
     */
    void reset();

    /**
     * @brief Check if detector is initialised and ready
     */
    bool ready() const { return ready_; }

private:
    static constexpr uint8_t  FEATURE_SIZE = 40;
    static constexpr uint8_t  FEATURE_DURATION_MS = 30;
    static constexpr uint16_t SAMPLE_RATE = 16000;
    static constexpr size_t   RING_BUF_MS = 200;  // 200ms ring buffer
    static constexpr size_t   RING_BUF_SAMPLES = SAMPLE_RATE * RING_BUF_MS / 1000;
    static constexpr size_t   VAR_ARENA_SIZE = 1024;
    static constexpr int      MIN_SLICES_BEFORE_DETECTION = 20;

    bool registerOps();
    bool generateFeatures(int8_t features[FEATURE_SIZE]);
    bool runInference(const int8_t features[FEATURE_SIZE]);
    bool checkDetection();
    size_t newSamplesToGet() const;

    // Model
    const uint8_t *modelData_ = nullptr;
    uint8_t *tensorArena_ = nullptr;
    uint8_t *varArena_ = nullptr;
    size_t   tensorArenaSize_ = 0;
    tflite::MicroMutableOpResolver<20> opResolver_;
    std::unique_ptr<tflite::MicroInterpreter> interpreter_;
    tflite::MicroResourceVariables *mrv_ = nullptr;
    tflite::MicroAllocator *ma_ = nullptr;

    // Audio frontend
    FrontendConfig frontendConfig_ = {};
    FrontendState  frontendState_ = {};
    int featureStepMs_ = 10;

    // Ring buffer (simple circular buffer for PCM samples)
    int16_t *ringBuf_ = nullptr;
    size_t   ringHead_ = 0;  // write position
    size_t   ringTail_ = 0;  // read position
    size_t   ringCount_ = 0; // samples available

    // Preprocessor buffer
    int16_t *preprocBuf_ = nullptr;

    // Detection state
    float probCutoff_ = 0.97f;
    size_t slidingWindowSize_ = 5;
    std::vector<uint8_t> recentProbs_;
    size_t probIndex_ = 0;
    uint8_t strideStep_ = 0;
    int ignoreWindows_ = 0;
    float lastProb_ = 0.0f;
    bool detected_ = false;
    bool ready_ = false;
};
