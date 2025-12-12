#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

// Forward declarations for the component modules that AudioPipeline orchestrates.
// This avoids pulling the full class definitions into this high-level header.
class I2SHandler;
class SignalProcessor;
class MetricsUpdater;

/**
 * @class AudioPipeline
 * @brief Orchestrates the entire audio processing flow.
 *
 * This class is the central coordinator for the audio pipeline. It owns and
 * manages the individual processing stages, from raw I2S data ingestion to
 * the final update of audio-reactive metrics. It ensures that data flows
 * correctly between each stage in the correct sequence.
 */
class AudioPipeline {
public:
    AudioPipeline();
    ~AudioPipeline();

    /**
     * @brief Initializes the pipeline and all its component modules.
     * This sets up I2S, allocates buffers, and prepares the processors.
     */
    void begin();

    /**
     * @brief Executes one full cycle of the audio processing pipeline.
     * This should be called repeatedly in the main application loop.
     */
    void update();

private:
    // Pointers to the component modules.
    // We use pointers to control the lifetime and initialization sequence precisely.
    I2SHandler*       m_i2s_handler;
    SignalProcessor*  m_signal_processor;
    MetricsUpdater*   m_metrics_updater;

    // TODO: Define internal buffers for data transfer between modules,
    // e.g., raw sample buffer, FFT input buffer, etc.
};

#endif // AUDIO_PIPELINE_H 