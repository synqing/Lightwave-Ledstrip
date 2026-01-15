/**
 * @file audio_pipeline_hw_test.cpp
 * @brief Hardware test for canonical audio pipeline modules
 * 
 * HARDWARE TEST - MINIMAL INTEGRATION
 * 
 * This sketch tests the new canonical modules on real ESP32-S3 hardware
 * with SPH0645 microphone input. It runs ALONGSIDE the existing AudioNode
 * without modifying production code.
 * 
 * WHAT THIS TESTS:
 * 1. GoertzelDFT - 64 semitone-spaced frequency bins
 * 2. Signal Quality - DC offset, clipping, SNR, SPL
 * 3. OnsetDetector - Spectral flux onset detection
 * 4. PLLTracker - Tempo and beat tracking
 * 
 * HARDWARE REQUIREMENTS:
 * - ESP32-S3 DevKit
 * - SPH0645 I2S microphone (GPIO 12=WS, 13=SD, 14=SCK)
 * - Serial monitor @ 115200 baud
 * 
 * USAGE:
 * 1. Build: pio run -e esp32dev_audio
 * 2. Flash: pio run -e esp32dev_audio -t upload
 * 3. Monitor: pio device monitor -b 115200
 * 4. Make some noise near the microphone!
 * 5. Watch serial output for frequency bins, BPM, signal quality
 * 
 * EXPECTED OUTPUT:
 * - Signal quality metrics every 1 second
 * - Frequency bin peaks when audio present
 * - BPM detection when rhythmic music plays
 * - Beat ticks in sync with music
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#include <Arduino.h>
#include <driver/i2s.h>
#include <soc/i2s_reg.h>  // For ESP32-S3 register settings

// New canonical modules
#include "../src/audio/GoertzelDFT.h"
#include "../src/audio/SignalQuality.h"
#include "../src/audio/OnsetDetector.h"
#include "../src/audio/tempo/PLLTracker.h"

using namespace LightwaveOS::Audio;

// ============================================================================
// HARDWARE CONFIGURATION (FROM PRODUCTION AudioCapture.cpp)
// ============================================================================

// I2S pins for SPH0645 microphone (verified in audio_config.h)
#define I2S_WS    12  // Word Select (LRCLK) - GPIO_NUM_12
#define I2S_SD    13  // Serial Data  (DOUT) - GPIO_NUM_13
#define I2S_SCK   14  // Serial Clock (BCLK) - GPIO_NUM_14

// Audio parameters (matching canonical spec)
#define SAMPLE_RATE  16000  // 16kHz
#define CHUNK_SIZE   128    // 128 samples per hop
#define I2S_PORT     I2S_NUM_0

// DMA buffer config (from production)
#define DMA_BUFFER_COUNT   4
#define DMA_BUFFER_SAMPLES 512

// ============================================================================
// GLOBAL STATE
// ============================================================================

// Module instances
GoertzelDFT    goertzel;
SignalQuality  signalQuality;
OnsetDetector  onsetDetector;
PLLTracker     pllTracker;

// Audio buffers - SPH0645 outputs 32-bit samples, we convert to 16-bit
int32_t rawBuffer[DMA_BUFFER_SAMPLES];  // Raw 32-bit I2S samples
int16_t audioBuffer[CHUNK_SIZE];         // Converted 16-bit samples for processing
uint32_t hopCount = 0;
uint32_t lastTelemetryTime = 0;

// ============================================================================
// I2S INITIALIZATION
// ============================================================================

void setupI2S() {
    Serial.println("\n=== Initializing I2S Audio Capture (SPH0645) ===");
    
    // #region agent log
    Serial.printf("[DEBUG] GPIO Config: WS=%d, SD=%d, SCK=%d\n", I2S_WS, I2S_SD, I2S_SCK);
    // #endregion
    
    // =========================================================================
    // CRITICAL: SPH0645 I2S Configuration (FROM PRODUCTION AudioCapture.cpp)
    // =========================================================================
    // ESP32-S3 quirk: SPH0645 SEL=GND outputs LEFT per I2S spec, 
    // but ESP32-S3 reads it as RIGHT channel!
    // =========================================================================
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,    // SPH0645 uses 32-bit slots!
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,    // ESP32-S3 quirk: RIGHT for SEL=GND
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_COUNT,
        .dma_buf_len = DMA_BUFFER_SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,          // MCLK = 256 * Fs
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT,        // 32-bit channel width
    };
    
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,  // SPH0645 doesn't need MCLK
        .bck_io_num = I2S_SCK,            // GPIO 14 - Bit Clock
        .ws_io_num = I2S_WS,              // GPIO 12 - Word Select
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD             // GPIO 13 - Data from mic
    };
    
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    
    // #region agent log
    Serial.printf("[DEBUG] i2s_driver_install result: %d (0=OK)\n", err);
    Serial.println("[DEBUG] Config: 32BIT samples, RIGHT channel, MCLK_256");
    // #endregion
    
    if (err != ESP_OK) {
        Serial.printf("ERROR: I2S driver install failed: %d\n", err);
        return;
    }
    
    // =========================================================================
    // CRITICAL: ESP32-S3 Specific Fixes for SPH0645 (FROM PRODUCTION CODE)
    // =========================================================================
    // 1. Delay sampling by 1 BCLK (timing fix)
    // 2. Enable MSB shift (SPH0645 outputs MSB-first with 1 BCLK delay)
    // 3. WS idle polarity inversion
    // =========================================================================
    
    // #region agent log
    Serial.println("[DEBUG] Applying ESP32-S3 SPH0645 register fixes...");
    // #endregion
    
    REG_SET_BIT(I2S_RX_TIMING_REG(I2S_PORT), BIT(9));     // Timing delay
    REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);  // MSB shift
    REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_WS_IDLE_POL); // WS polarity
    
    // #region agent log
    Serial.println("[DEBUG] ESP32-S3 register fixes applied");
    // #endregion
    
    err = i2s_set_pin(I2S_PORT, &pin_config);
    
    // #region agent log
    Serial.printf("[DEBUG] i2s_set_pin result: %d (0=OK)\n", err);
    // #endregion
    
    if (err != ESP_OK) {
        Serial.printf("ERROR: I2S pin config failed: %d\n", err);
        return;
    }
    
    // #region agent log
    Serial.println("[DEBUG] Calling i2s_start()...");
    // #endregion
    
    i2s_start(I2S_PORT);
    
    // #region agent log
    Serial.println("[DEBUG] I2S started successfully with SPH0645 config");
    // #endregion
    
    Serial.println("✅ I2S initialized (32BIT, RIGHT channel, ESP32-S3 timing fixes)");
    Serial.printf("   Sample rate: %d Hz\n", SAMPLE_RATE);
    Serial.printf("   Chunk size: %d samples\n", CHUNK_SIZE);
    Serial.printf("   Hop period: %.1f ms\n", (CHUNK_SIZE * 1000.0f) / SAMPLE_RATE);
}

// ============================================================================
// MODULE INITIALIZATION
// ============================================================================

void setupModules() {
    Serial.println("\n=== Initializing Audio Modules ===");
    
    if (!goertzel.init()) {
        Serial.println("ERROR: GoertzelDFT init failed");
        return;
    }
    Serial.println("✅ GoertzelDFT initialized (64 semitone bins)");
    
    if (!signalQuality.init()) {
        Serial.println("ERROR: SignalQuality init failed");
        return;
    }
    Serial.println("✅ SignalQuality initialized");
    
    if (!onsetDetector.init()) {
        Serial.println("ERROR: OnsetDetector init failed");
        return;
    }
    Serial.println("✅ OnsetDetector initialized");
    
    if (!pllTracker.init()) {
        Serial.println("ERROR: PLLTracker init failed");
        return;
    }
    Serial.println("✅ PLLTracker initialized (96 tempo bins, 60-156 BPM)");
    
    Serial.println("\n🎤 Ready! Make some noise near the microphone...\n");
}

// ============================================================================
// AUDIO CAPTURE
// ============================================================================

bool captureAudio() {
    // Read 32-bit samples from I2S (SPH0645 native format)
    size_t bytesToRead = CHUNK_SIZE * sizeof(int32_t);  // 128 samples * 4 bytes
    size_t bytesRead = 0;
    
    esp_err_t err = i2s_read(I2S_PORT, rawBuffer, bytesToRead, &bytesRead, portMAX_DELAY);
    
    if (err != ESP_OK || bytesRead != bytesToRead) {
        Serial.printf("ERROR: I2S read failed (err=%d, bytes=%d/%d)\n", err, bytesRead, bytesToRead);
        return false;
    }
    
    // Convert 32-bit to 16-bit with proper scaling
    // SPH0645 outputs 18-bit data in MSB-aligned 32-bit word
    // Data is in upper 18 bits, so shift right by 14 to get 18->16 bit
    for (int i = 0; i < CHUNK_SIZE; i++) {
        // Shift right by 14: keeps top 18 bits, then truncate to 16
        audioBuffer[i] = (int16_t)(rawBuffer[i] >> 14);
    }
    
    // #region agent log
    if (hopCount % 100 == 0) {  // Log every 100 hops to avoid spam
        Serial.printf("[DEBUG] i2s_read: err=%d, bytesRead=%d/%d (32-bit samples)\n", 
                     err, bytesRead, bytesToRead);
        Serial.printf("[DEBUG] Raw32[0-3]: %d, %d, %d, %d\n", 
                     rawBuffer[0], rawBuffer[1], rawBuffer[2], rawBuffer[3]);
        Serial.printf("[DEBUG] Conv16[0-7]: %d,%d,%d,%d,%d,%d,%d,%d\n", 
                     audioBuffer[0], audioBuffer[1], audioBuffer[2], audioBuffer[3],
                     audioBuffer[4], audioBuffer[5], audioBuffer[6], audioBuffer[7]);
    }
    // #endregion
    
    return true;
}

// ============================================================================
// TELEMETRY OUTPUT
// ============================================================================

void printSignalQuality() {
    const auto& metrics = signalQuality.getMetrics();
    
    Serial.println("\n┌─── SIGNAL QUALITY ───────────────────────────────────────┐");
    Serial.printf("│ DC Offset:    %7.1f LSB  ", metrics.dcOffset);
    Serial.println(fabsf(metrics.dcOffset) < 1000 ? "✅ OK" : "⚠️  HIGH");
    
    Serial.printf("│ Clipping:     %5.1f%%       ", metrics.clippingPercent);
    Serial.println(metrics.isClipping ? "❌ CLIPPING!" : "✅ OK");
    
    Serial.printf("│ RMS Level:    %7.1f LSB\n", metrics.rms);
    Serial.printf("│ Peak Level:   %7.1f LSB\n", metrics.peak);
    Serial.printf("│ Crest Factor: %7.1f      ", metrics.crestFactor);
    
    if (metrics.crestFactor > 10.0f) {
        Serial.println("(High dynamics - music?)");
    } else if (metrics.crestFactor < 3.0f) {
        Serial.println("(Low dynamics - noise/tone?)");
    } else {
        Serial.println("(Normal)");
    }
    
    Serial.printf("│ SNR Estimate: %7.1f dB   ", metrics.snrEstimate);
    if (metrics.snrEstimate > 40.0f) Serial.println("✅ Excellent");
    else if (metrics.snrEstimate > 30.0f) Serial.println("✅ Good");
    else if (metrics.snrEstimate > 20.0f) Serial.println("⚠️  Acceptable");
    else Serial.println("❌ Poor");
    
    Serial.printf("│ SPL Estimate: %7.1f dBFS\n", metrics.splEstimate);
    
    Serial.printf("│ Signal:       %s\n", metrics.signalPresent ? "✅ PRESENT" : "⬜ Silent");
    Serial.printf("│ Health:       %s\n", signalQuality.isSignalHealthy() ? "✅ HEALTHY" : "⚠️  ISSUES");
    Serial.println("└──────────────────────────────────────────────────────────┘");
}

void printFrequencyPeaks() {
    Serial.println("\n┌─── FREQUENCY PEAKS (Top 5) ──────────────────────────────┐");
    
    // Find top 5 peaks
    struct Peak { uint16_t bin; float mag; float freq; };
    Peak peaks[5] = {{0, 0.0f, 0.0f}};
    
    for (uint16_t bin = 0; bin < 64; bin++) {
        float mag = goertzel.getMagnitude(bin);
        float freq = goertzel.getBinFrequency(bin);
        
        // Insert into sorted peak list
        for (int i = 0; i < 5; i++) {
            if (mag > peaks[i].mag) {
                // Shift lower peaks down
                for (int j = 4; j > i; j--) {
                    peaks[j] = peaks[j-1];
                }
                peaks[i] = {bin, mag, freq};
                break;
            }
        }
    }
    
    for (int i = 0; i < 5; i++) {
        if (peaks[i].mag > 0.01f) {
            Serial.printf("│ %d. Bin %2d: %7.1f Hz  Mag: %.3f\n", 
                         i+1, peaks[i].bin, peaks[i].freq, peaks[i].mag);
        }
    }
    Serial.println("└──────────────────────────────────────────────────────────┘");
}

void printTempo() {
    float bpm = pllTracker.getDominantBPM();
    float confidence = pllTracker.getConfidence();
    float phase = pllTracker.getBeatPhase();
    bool onBeat = pllTracker.isOnBeat();
    
    Serial.println("\n┌─── TEMPO TRACKING ───────────────────────────────────────┐");
    Serial.printf("│ BPM:        %6.1f    ", bpm);
    Serial.println(confidence > 0.5f ? "✅ Confident" : "⚠️  Uncertain");
    
    Serial.printf("│ Confidence: %6.1f%%\n", confidence * 100.0f);
    Serial.printf("│ Beat Phase: %+6.2f rad\n", phase);
    Serial.printf("│ On Beat:    %s\n", onBeat ? "🥁 BEAT!" : "⬜");
    Serial.printf("│ Novelty:    %.3f\n", onsetDetector.getCurrentNovelty());
    Serial.println("└──────────────────────────────────────────────────────────┘");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial
    
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║   LightwaveOS Audio Pipeline Hardware Test              ║");
    Serial.println("║   Canonical Modules: Goertzel + Onset + Tempo            ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    
    setupI2S();
    setupModules();
    
    lastTelemetryTime = millis();
}

void loop() {
    // Capture audio hop
    if (!captureAudio()) {
        delay(10);
        return;
    }
    
    hopCount++;
    
    // Process audio pipeline
    signalQuality.update(audioBuffer, CHUNK_SIZE);
    goertzel.analyze(audioBuffer, CHUNK_SIZE);
    
    // Update onset detection
    const float* magnitudes = goertzel.getMagnitudes();
    float novelty = onsetDetector.update(magnitudes);
    
    // Update tempo tracking (every 10 hops to match 50Hz novelty rate)
    if (hopCount % 10 == 0) {
        const float* noveltyHistory = onsetDetector.getNoveltyHistory();
        uint16_t historyIndex = onsetDetector.getNoveltyHistoryIndex();
        float deltaMs = 10 * (CHUNK_SIZE * 1000.0f) / SAMPLE_RATE;  // 10 hops
        pllTracker.update(noveltyHistory, historyIndex, deltaMs);
    }
    
    // Print telemetry every 1 second
    uint32_t now = millis();
    if (now - lastTelemetryTime >= 1000) {
        lastTelemetryTime = now;
        
        Serial.printf("\n\n========== HOP %lu ==========\n", hopCount);
        printSignalQuality();
        printFrequencyPeaks();
        printTempo();
        
        Serial.println("\n💡 TIP: Play music with a strong beat to see tempo tracking!");
    }
    
    // Beat indicator (quick visual feedback)
    if (pllTracker.isOnBeat()) {
        Serial.print("🥁 ");
    }
}
