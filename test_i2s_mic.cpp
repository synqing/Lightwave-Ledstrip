/**
 * I2S Microphone Test
 * Simple test to verify I2S microphone functionality
 * Upload this separately to test the mic hardware
 */

#include <Arduino.h>
#include <driver/i2s.h>

// I2S pin configuration for SPH0645
#define I2S_BCLK_PIN  16  // Bit Clock
#define I2S_DOUT_PIN  10  // Data Out (from mic)
#define I2S_LRCL_PIN   4  // Left/Right Clock

// I2S configuration
#define I2S_PORT      I2S_NUM_0
#define SAMPLE_RATE   44100
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN   512
#define SAMPLE_SIZE   1024

int32_t samples[SAMPLE_SIZE];
bool micInitialized = false;

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== I2S Microphone Test ===");
    Serial.println("Testing SPH0645 I2S microphone...");
    
    // Configure I2S
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    // Pin configuration
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCL_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DOUT_PIN
    };
    
    // Install and start I2S driver
    esp_err_t result = i2s_driver_install((i2s_port_t)I2S_PORT, &i2s_config, 0, NULL);
    if (result != ESP_OK) {
        Serial.printf("Failed to install I2S driver: %s\\n", esp_err_to_name(result));
        return;
    }
    
    result = i2s_set_pin((i2s_port_t)I2S_PORT, &pin_config);
    if (result != ESP_OK) {
        Serial.printf("Failed to set I2S pins: %s\\n", esp_err_to_name(result));
        return;
    }
    
    result = i2s_start((i2s_port_t)I2S_PORT);
    if (result != ESP_OK) {
        Serial.printf("Failed to start I2S: %s\\n", esp_err_to_name(result));
        return;
    }
    
    Serial.println("✅ I2S microphone initialized successfully!");
    Serial.println("Pin Configuration:");
    Serial.printf("  BCLK: GPIO %d\\n", I2S_BCLK_PIN);
    Serial.printf("  DOUT: GPIO %d\\n", I2S_DOUT_PIN);
    Serial.printf("  LRCL: GPIO %d\\n", I2S_LRCL_PIN);
    Serial.println();
    Serial.println("Listening for audio... (make some noise!)");
    Serial.println("Expected output: Audio level readings every second");
    
    micInitialized = true;
}

void loop() {
    if (!micInitialized) {
        delay(1000);
        return;
    }
    
    // Read audio samples
    size_t bytesRead = 0;
    esp_err_t result = i2s_read((i2s_port_t)I2S_PORT, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
    
    if (result != ESP_OK) {
        Serial.printf("I2S read error: %s\\n", esp_err_to_name(result));
        delay(1000);
        return;
    }
    
    // Calculate audio level
    uint32_t samplesRead = bytesRead / sizeof(int32_t);
    if (samplesRead == 0) {
        Serial.println("⚠️  No samples read from microphone");
        delay(1000);
        return;
    }
    
    // Analyze audio level
    uint64_t sum = 0;
    uint32_t peak = 0;
    uint32_t validSamples = 0;
    
    for (uint32_t i = 0; i < samplesRead; i++) {
        // Convert 32-bit sample to 16-bit
        int16_t sample = samples[i] >> 16;
        uint32_t level = abs(sample);
        
        sum += level;
        if (level > peak) peak = level;
        if (level > 100) validSamples++; // Count samples above noise floor
    }
    
    float avgLevel = (float)sum / samplesRead;
    float peakLevel = (float)peak / 32768.0f * 100.0f; // As percentage
    float avgPercent = avgLevel / 32768.0f * 100.0f;
    float activity = (float)validSamples / samplesRead * 100.0f;
    
    // Display results
    Serial.printf("Audio: Avg=%.2f%% Peak=%.2f%% Activity=%.1f%% [%d samples]\\n", 
                  avgPercent, peakLevel, activity, samplesRead);
    
    // Visual level indicator
    if (peakLevel > 10.0f) {
        Serial.print("🔊 LOUD: ");
        int bars = (int)(peakLevel / 5.0f);
        for (int i = 0; i < bars && i < 20; i++) {
            Serial.print("█");
        }
        Serial.println();
    } else if (peakLevel > 1.0f) {
        Serial.print("🔉 Audio detected: ");
        int bars = (int)(peakLevel);
        for (int i = 0; i < bars && i < 10; i++) {
            Serial.print("▓");
        }
        Serial.println();
    } else if (avgLevel > 50) {
        Serial.println("🔇 Noise floor detected");
    } else {
        Serial.println("🔇 SILENCE - No audio detected!");
    }
    
    delay(500); // Update twice per second
}