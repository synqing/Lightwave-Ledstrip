#include "EsV11Buffers.h"

#include <cstdlib>

#if !defined(NATIVE_BUILD)
#include <esp_heap_caps.h>
#endif

float* sample_history = nullptr;
float* window_lookup = nullptr;

float* novelty_curve = nullptr;
float* novelty_curve_normalized = nullptr;
float* vu_curve = nullptr;
float* vu_curve_normalized = nullptr;

tempo* tempi = nullptr;
freq* frequencies_musical = nullptr;

float (*spectrogram_average)[NUM_FREQS] = nullptr;
float (*noise_history)[NUM_FREQS] = nullptr;

namespace {

constexpr size_t kSpectrogramAverageSamples = 12;
constexpr size_t kNoiseHistorySamples = 10;

#if !defined(NATIVE_BUILD)
void* esv11_calloc(size_t count, size_t size) {
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void esv11_free(void* ptr) {
    if (ptr) {
        heap_caps_free(ptr);
    }
}
#else
void* esv11_calloc(size_t count, size_t size) {
    return std::calloc(count, size);
}

void esv11_free(void* ptr) {
    std::free(ptr);
}
#endif

} // namespace

void esv11_free_buffers() {
    esv11_free(noise_history);
    noise_history = nullptr;

    esv11_free(spectrogram_average);
    spectrogram_average = nullptr;

    esv11_free(frequencies_musical);
    frequencies_musical = nullptr;

    esv11_free(tempi);
    tempi = nullptr;

    esv11_free(vu_curve_normalized);
    vu_curve_normalized = nullptr;

    esv11_free(vu_curve);
    vu_curve = nullptr;

    esv11_free(novelty_curve_normalized);
    novelty_curve_normalized = nullptr;

    esv11_free(novelty_curve);
    novelty_curve = nullptr;

    esv11_free(window_lookup);
    window_lookup = nullptr;

    esv11_free(sample_history);
    sample_history = nullptr;
}

bool esv11_init_buffers() {
    const bool ready = (sample_history != nullptr) && (window_lookup != nullptr) &&
                       (novelty_curve != nullptr) && (novelty_curve_normalized != nullptr) &&
                       (vu_curve != nullptr) && (vu_curve_normalized != nullptr) && (tempi != nullptr) &&
                       (frequencies_musical != nullptr) && (spectrogram_average != nullptr) && (noise_history != nullptr);
    if (ready) {
        return true;
    }

    esv11_free_buffers();

    sample_history = static_cast<float*>(esv11_calloc(SAMPLE_HISTORY_LENGTH, sizeof(float)));
    if (!sample_history) goto fail;

    window_lookup = static_cast<float*>(esv11_calloc(SAMPLE_HISTORY_LENGTH, sizeof(float)));
    if (!window_lookup) goto fail;

    novelty_curve = static_cast<float*>(esv11_calloc(NOVELTY_HISTORY_LENGTH, sizeof(float)));
    if (!novelty_curve) goto fail;

    novelty_curve_normalized = static_cast<float*>(esv11_calloc(NOVELTY_HISTORY_LENGTH, sizeof(float)));
    if (!novelty_curve_normalized) goto fail;

    vu_curve = static_cast<float*>(esv11_calloc(NOVELTY_HISTORY_LENGTH, sizeof(float)));
    if (!vu_curve) goto fail;

    vu_curve_normalized = static_cast<float*>(esv11_calloc(NOVELTY_HISTORY_LENGTH, sizeof(float)));
    if (!vu_curve_normalized) goto fail;

    tempi = static_cast<tempo*>(esv11_calloc(NUM_TEMPI, sizeof(tempo)));
    if (!tempi) goto fail;

    frequencies_musical = static_cast<freq*>(esv11_calloc(NUM_FREQS, sizeof(freq)));
    if (!frequencies_musical) goto fail;

    spectrogram_average = static_cast<float (*)[NUM_FREQS]>(
        esv11_calloc(kSpectrogramAverageSamples, sizeof(float[NUM_FREQS])));
    if (!spectrogram_average) goto fail;

    noise_history = static_cast<float (*)[NUM_FREQS]>(esv11_calloc(kNoiseHistorySamples, sizeof(float[NUM_FREQS])));
    if (!noise_history) goto fail;

    return true;

fail:
    esv11_free_buffers();
    return false;
}
