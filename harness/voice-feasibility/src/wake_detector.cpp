/**
 * @file wake_detector.cpp
 * @brief Lightweight wake word detector — ported from microWakeWord
 *
 * Core detection logic from:
 *   https://github.com/OHF-Voice/micro-wake-word
 *   https://github.com/0xD34D/micro_wake_word_standalone
 *
 * Stripped of ESPHome dependencies. Uses:
 *   - kahrendt/ESPMicroSpeechFeatures (audio frontend)
 *   - johnosbb/MicroTFLite (TFLite Micro interpreter)
 */

#include <Arduino.h>
#include "wake_detector.h"
#include <esp_heap_caps.h>

// TFLite Micro includes
#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_resource_variable.h>
#include <tensorflow/lite/schema/schema_generated.h>

WakeDetector::~WakeDetector() {
    interpreter_.reset();
    if (tensorArena_) { heap_caps_free(tensorArena_); tensorArena_ = nullptr; }
    if (varArena_)    { heap_caps_free(varArena_);    varArena_ = nullptr; }
    if (ringBuf_)     { heap_caps_free(ringBuf_);     ringBuf_ = nullptr; }
    if (preprocBuf_)  { heap_caps_free(preprocBuf_);  preprocBuf_ = nullptr; }
    FrontendFreeStateContents(&frontendState_);
}

bool WakeDetector::begin(const uint8_t *modelData, size_t tensorArenaSize,
                         float probCutoff, size_t slidingWindowSize,
                         int featureStepMs) {
    modelData_ = modelData;
    tensorArenaSize_ = tensorArenaSize;
    probCutoff_ = probCutoff;
    slidingWindowSize_ = slidingWindowSize;
    featureStepMs_ = featureStepMs;

    recentProbs_.resize(slidingWindowSize, 0);
    probIndex_ = 0;
    strideStep_ = 0;
    ignoreWindows_ = -MIN_SLICES_BEFORE_DETECTION;

    // Register TFLite ops
    if (!registerOps()) {
        Serial.println("[MWW] Failed to register TFLite ops");
        return false;
    }

    // Allocate tensor arena in PSRAM
    tensorArena_ = (uint8_t *)heap_caps_malloc(tensorArenaSize_, MALLOC_CAP_SPIRAM);
    if (!tensorArena_) {
        Serial.println("[MWW] Failed to allocate tensor arena in PSRAM");
        return false;
    }

    // Allocate variable arena in PSRAM
    varArena_ = (uint8_t *)heap_caps_malloc(VAR_ARENA_SIZE, MALLOC_CAP_SPIRAM);
    if (!varArena_) {
        Serial.println("[MWW] Failed to allocate variable arena");
        return false;
    }

    ma_ = tflite::MicroAllocator::Create(varArena_, VAR_ARENA_SIZE);
    mrv_ = tflite::MicroResourceVariables::Create(ma_, 20);

    // Create interpreter
    const tflite::Model *model = tflite::GetModel(modelData_);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[MWW] Model schema version %lu != expected %d\n",
                      (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }

    interpreter_ = std::make_unique<tflite::MicroInterpreter>(
        model, opResolver_, tensorArena_, tensorArenaSize_, mrv_);

    if (interpreter_->AllocateTensors() != kTfLiteOk) {
        Serial.println("[MWW] Failed to allocate tensors");
        return false;
    }

    // Verify input tensor: [1, stride, 40]
    TfLiteTensor *input = interpreter_->input(0);
    if (input->dims->size != 3 || input->dims->data[0] != 1 ||
        input->dims->data[2] != FEATURE_SIZE) {
        Serial.printf("[MWW] Unexpected input shape: [%d,%d,%d]\n",
                      input->dims->data[0], input->dims->data[1], input->dims->data[2]);
        return false;
    }
    if (input->type != kTfLiteInt8) {
        Serial.println("[MWW] Input tensor is not int8");
        return false;
    }

    // Verify output tensor: [1, 1]
    TfLiteTensor *output = interpreter_->output(0);
    if (output->dims->size != 2 || output->dims->data[0] != 1 ||
        output->dims->data[1] != 1) {
        Serial.printf("[MWW] Unexpected output shape: [%d,%d]\n",
                      output->dims->data[0], output->dims->data[1]);
        return false;
    }

    Serial.printf("[MWW] Model loaded. Arena used: %zu / %zu bytes. Stride: %d\n",
                  interpreter_->arena_used_bytes(), tensorArenaSize_,
                  input->dims->data[1]);

    // Configure audio frontend
    frontendConfig_.window.size_ms = FEATURE_DURATION_MS;
    frontendConfig_.window.step_size_ms = featureStepMs_;
    frontendConfig_.filterbank.num_channels = FEATURE_SIZE;
    frontendConfig_.filterbank.lower_band_limit = 125.0f;
    frontendConfig_.filterbank.upper_band_limit = 7500.0f;
    frontendConfig_.noise_reduction.smoothing_bits = 10;
    frontendConfig_.noise_reduction.even_smoothing = 0.025f;
    frontendConfig_.noise_reduction.odd_smoothing = 0.06f;
    frontendConfig_.noise_reduction.min_signal_remaining = 0.05f;
    frontendConfig_.pcan_gain_control.enable_pcan = 1;
    frontendConfig_.pcan_gain_control.strength = 0.95f;
    frontendConfig_.pcan_gain_control.offset = 80.0f;
    frontendConfig_.pcan_gain_control.gain_bits = 21;
    frontendConfig_.log_scale.enable_log = 1;
    frontendConfig_.log_scale.scale_shift = 6;

    if (!FrontendPopulateState(&frontendConfig_, &frontendState_, SAMPLE_RATE)) {
        Serial.println("[MWW] Failed to populate frontend state");
        FrontendFreeStateContents(&frontendState_);
        return false;
    }

    // Allocate ring buffer in PSRAM
    ringBuf_ = (int16_t *)heap_caps_malloc(RING_BUF_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!ringBuf_) {
        Serial.println("[MWW] Failed to allocate ring buffer");
        return false;
    }
    ringHead_ = ringTail_ = ringCount_ = 0;

    // Allocate preprocessor buffer
    size_t preprocSamples = newSamplesToGet();
    preprocBuf_ = (int16_t *)heap_caps_malloc(preprocSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!preprocBuf_) {
        Serial.println("[MWW] Failed to allocate preprocessor buffer");
        return false;
    }

    ready_ = true;
    Serial.printf("[MWW] Ready. Feature step: %d ms, prob cutoff: %.2f, window: %zu\n",
                  featureStepMs_, probCutoff_, slidingWindowSize_);
    return true;
}

void WakeDetector::feedSamples(const int16_t *samples, size_t count) {
    if (!ready_) return;

    // Write to ring buffer
    for (size_t i = 0; i < count; i++) {
        ringBuf_[ringHead_] = samples[i];
        ringHead_ = (ringHead_ + 1) % RING_BUF_SAMPLES;
        if (ringCount_ < RING_BUF_SAMPLES) {
            ringCount_++;
        } else {
            // Overwrite oldest — advance tail
            ringTail_ = (ringTail_ + 1) % RING_BUF_SAMPLES;
        }
    }

    // Process as many windows as we have samples for
    size_t needed = newSamplesToGet();
    while (ringCount_ >= needed && !detected_) {
        int8_t features[FEATURE_SIZE];
        if (generateFeatures(features)) {
            runInference(features);
            checkDetection();
        }
    }
}

void WakeDetector::reset() {
    detected_ = false;
    lastProb_ = 0.0f;
    strideStep_ = 0;
    ignoreWindows_ = -MIN_SLICES_BEFORE_DETECTION;
    for (auto &p : recentProbs_) p = 0;
    probIndex_ = 0;
    ringHead_ = ringTail_ = ringCount_ = 0;

    // Reset frontend state for clean start
    FrontendFreeStateContents(&frontendState_);
    FrontendPopulateState(&frontendConfig_, &frontendState_, SAMPLE_RATE);
}

size_t WakeDetector::newSamplesToGet() const {
    return (featureStepMs_ * SAMPLE_RATE) / 1000;  // e.g. 10ms * 16000 / 1000 = 160 samples
}

bool WakeDetector::registerOps() {
    if (opResolver_.AddCallOnce() != kTfLiteOk) return false;
    if (opResolver_.AddVarHandle() != kTfLiteOk) return false;
    if (opResolver_.AddReshape() != kTfLiteOk) return false;
    if (opResolver_.AddReadVariable() != kTfLiteOk) return false;
    if (opResolver_.AddStridedSlice() != kTfLiteOk) return false;
    if (opResolver_.AddConcatenation() != kTfLiteOk) return false;
    if (opResolver_.AddAssignVariable() != kTfLiteOk) return false;
    if (opResolver_.AddConv2D() != kTfLiteOk) return false;
    if (opResolver_.AddMul() != kTfLiteOk) return false;
    if (opResolver_.AddAdd() != kTfLiteOk) return false;
    if (opResolver_.AddMean() != kTfLiteOk) return false;
    if (opResolver_.AddFullyConnected() != kTfLiteOk) return false;
    if (opResolver_.AddLogistic() != kTfLiteOk) return false;
    if (opResolver_.AddQuantize() != kTfLiteOk) return false;
    if (opResolver_.AddDepthwiseConv2D() != kTfLiteOk) return false;
    if (opResolver_.AddAveragePool2D() != kTfLiteOk) return false;
    if (opResolver_.AddMaxPool2D() != kTfLiteOk) return false;
    if (opResolver_.AddPad() != kTfLiteOk) return false;
    if (opResolver_.AddPack() != kTfLiteOk) return false;
    if (opResolver_.AddSplitV() != kTfLiteOk) return false;
    return true;
}

bool WakeDetector::generateFeatures(int8_t features[FEATURE_SIZE]) {
    size_t needed = newSamplesToGet();
    if (ringCount_ < needed) return false;

    // Read from ring buffer into preprocessor buffer
    for (size_t i = 0; i < needed; i++) {
        preprocBuf_[i] = ringBuf_[ringTail_];
        ringTail_ = (ringTail_ + 1) % RING_BUF_SAMPLES;
    }
    ringCount_ -= needed;

    // Run audio frontend
    size_t numSamplesRead;
    struct FrontendOutput frontendOutput = FrontendProcessSamples(
        &frontendState_, preprocBuf_, needed, &numSamplesRead);

    if (frontendOutput.size != FEATURE_SIZE) {
        return false;
    }

    // Scale features to int8 (matches TFLite training quantisation)
    for (size_t i = 0; i < frontendOutput.size; ++i) {
        constexpr int32_t value_scale = 256;
        constexpr int32_t value_div = 666;  // 25.6 * 26.0 rounded
        int32_t value = ((frontendOutput.values[i] * value_scale) + (value_div / 2)) / value_div;
        value -= 128;
        if (value < -128) value = -128;
        if (value > 127) value = 127;
        features[i] = (int8_t)value;
    }

    return true;
}

bool WakeDetector::runInference(const int8_t features[FEATURE_SIZE]) {
    if (!interpreter_) return false;

    TfLiteTensor *input = interpreter_->input(0);
    uint8_t stride = input->dims->data[1];

    // Copy features into the correct stride position
    std::memmove(
        (int8_t *)(tflite::GetTensorData<int8_t>(input)) + FEATURE_SIZE * strideStep_,
        features, FEATURE_SIZE);
    strideStep_++;

    if (strideStep_ >= stride) {
        strideStep_ = 0;

        TfLiteStatus status = interpreter_->Invoke();
        if (status != kTfLiteOk) {
            Serial.println("[MWW] Invoke failed");
            return false;
        }

        TfLiteTensor *output = interpreter_->output(0);
        uint8_t prob = output->data.uint8[0];

        probIndex_++;
        if (probIndex_ >= slidingWindowSize_) probIndex_ = 0;
        recentProbs_[probIndex_] = prob;
    }

    return true;
}

bool WakeDetector::checkDetection() {
    // Increment ignore counter (starts negative)
    if (ignoreWindows_ < 0) {
        ignoreWindows_++;
        return false;
    }

    // Compute sliding window average
    uint32_t sum = 0;
    for (auto &p : recentProbs_) sum += p;
    float avg = (float)sum / (255.0f * slidingWindowSize_);
    lastProb_ = avg;

    if (avg > probCutoff_) {
        detected_ = true;
        return true;
    }
    return false;
}
