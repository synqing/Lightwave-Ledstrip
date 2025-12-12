/*
 * I2SInputNode - Pluggable I2S Audio Input Module
 * ================================================
 * 
 * Handles audio input from SPH0645 I2S microphone.
 * First node in the pipeline - generates audio samples.
 * 
 * HARDWARE:
 * - SPH0645 I2S MEMS microphone
 * - 18-bit data, LEFT channel
 * - Configurable sample rate (default 16kHz)
 * 
 * GPIO PINS:
 * - BCLK: GPIO 16
 * - LRCLK: GPIO 4 
 * - DIN: GPIO 10
 */

#ifndef I2S_INPUT_NODE_H
#define I2S_INPUT_NODE_H

#include "../audio_node.h"
#include <driver/i2s.h>

class I2SInputNode : public AudioNode {
public:
    I2SInputNode() : AudioNode("I2SInput", AudioNodeType::SOURCE) {
        // Allocate buffers
        i2s_buffer = (uint32_t*)malloc(512 * sizeof(uint32_t));
    }
    
    ~I2SInputNode() {
        if (initialized) {
            i2s_driver_uninstall(I2S_NUM_0);
        }
        free(i2s_buffer);
    }
    
    // Initialize I2S hardware
    bool init() override {
        // SPH0645-specific I2S configuration
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = sample_rate,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,    // SPH0645 is on LEFT channel!
            .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = chunk_size / 4,  // 128/4 = 32, matches working version
            .use_apll = false,
            .tx_desc_auto_clear = false,
            .fixed_mclk = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bits_per_chan = I2S_BITS_PER_CHAN_32BIT
        };
        
        i2s_pin_config_t pin_config = {
            .bck_io_num = 16,      // BCLK
            .ws_io_num = 4,        // LRCLK/WS
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = 10      // DIN
        };
        
        esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
        if (err != ESP_OK) {
            Serial.printf("I2SInputNode: Failed to install driver: %d\n", err);
            return false;
        }
        
        err = i2s_set_pin(I2S_NUM_0, &pin_config);
        if (err != ESP_OK) {
            Serial.printf("I2SInputNode: Failed to set pins: %d\n", err);
            return false;
        }
        
        // Clear DMA buffer
        i2s_zero_dma_buffer(I2S_NUM_0);
        
        initialized = true;
        Serial.printf("I2SInputNode: Initialized at %dHz, %d samples/chunk\n", 
                     sample_rate, chunk_size);
        return true;
    }
    
    // Read audio samples from I2S
    bool process(AudioBuffer& input, AudioBuffer& output) override {
        if (!enabled || !initialized) return false;
        
        uint32_t start = micros();
        
        // Read from I2S
        size_t bytes_read = 0;
        esp_err_t err = i2s_read(I2S_NUM_0, i2s_buffer, 
                                chunk_size * sizeof(uint32_t), 
                                &bytes_read, 100 / portTICK_PERIOD_MS);
        
        if (err != ESP_OK || bytes_read == 0) {
            return false;
        }
        
        // Convert 32-bit I2S data to float samples
        // SPH0645 outputs 18-bit data in 32-bit container
        size_t samples_read = bytes_read / sizeof(uint32_t);
        
        // Debug: Print first few raw samples to understand data format
        static int raw_debug = 0;
        if (raw_debug++ % 500 == 0 && samples_read > 4) {
            Serial.printf("RAW I2S data (hex): ");
            for (int j = 0; j < 4; j++) {
                Serial.printf("0x%08X ", i2s_buffer[j]);
            }
            Serial.printf("\n");
            
            // Try different extraction methods to understand the format
            Serial.printf("Extraction tests on first sample 0x%08X:\n", i2s_buffer[0]);
            int32_t test1 = (int32_t)i2s_buffer[0] >> 14;  // Current method
            int32_t test2 = (int32_t)i2s_buffer[0] >> 16;  // Alternative: 16-bit in upper half
            int32_t test3 = (int32_t)(i2s_buffer[0] & 0xFFFFC000) >> 14; // Mask then shift
            Serial.printf("  >>14: %d, >>16: %d, mask>>14: %d\n", test1, test2, test3);
        }
        
        int32_t min_val = INT32_MAX, max_val = INT32_MIN;
        float sum = 0.0f;
        
        for (size_t i = 0; i < samples_read && i < 512; i++) {
            // SPH0645 outputs 18-bit data left-justified in 32-bit word
            // The data is in bits 31:14, with bits 13:0 typically zeros
            
            int32_t raw32 = (int32_t)i2s_buffer[i];
            
            // Extract 18-bit signed value from upper bits
            // Method: Arithmetic shift right by 14 to get 18-bit value with sign extension
            int32_t raw = raw32 >> 14;
            
            if (raw > max_val) max_val = raw;
            if (raw < min_val) min_val = raw;
            sum += raw;
            
            // Convert to float but keep the FUCKING RAW VALUES like the working system!
            // The rest of the pipeline expects values in int16_t range!
            output.data[i] = (float)raw;
        }
        
        // Calculate and display statistics
        static int stats_debug = 0;
        if (stats_debug++ % 250 == 0) {
            float avg = sum / samples_read;
            Serial.printf("I2S Stats: samples=%d, range=[%d,%d], avg=%.1f\n", 
                         samples_read, min_val, max_val, avg);
        }
        
        // Set output buffer metadata
        output.size = samples_read;  // Mono mode - all samples are from LEFT channel
        output.timestamp = millis();
        output.is_silence = false;  // TODO: Implement silence detection
        output.metadata.sample_rate = sample_rate;
        
        // Debug I2S data
        if (debug_counter++ % 2500 == 0) {  // Every 20 seconds at 125Hz
            int32_t min_val = INT32_MAX, max_val = INT32_MIN;
            for (size_t i = 0; i < samples_read; i++) {
                int32_t val = (int32_t)i2s_buffer[i];
                if (val < min_val) min_val = val;
                if (val > max_val) max_val = val;
            }
            Serial.printf("I2SInputNode: raw range [%d, %d], samples=%d\n", 
                         min_val, max_val, samples_read);
        }
        
        measureProcessTime(start);
        return true;
    }
    
    // Configure from JSON
    bool configure(JsonObject& config) override {
        if (config.containsKey("enabled")) {
            enabled = config["enabled"];
        }
        
        if (config.containsKey("sample_rate")) {
            uint32_t new_rate = config["sample_rate"];
            if (new_rate != sample_rate && initialized) {
                // Need to reinitialize I2S with new sample rate
                i2s_driver_uninstall(I2S_NUM_0);
                sample_rate = new_rate;
                initialized = false;
                init();
            }
        }
        
        if (config.containsKey("chunk_size")) {
            chunk_size = config["chunk_size"];
        }
        
        return true;
    }
    
    // Get current configuration
    void getConfig(JsonObject& config) override {
        AudioNode::getConfig(config);
        config["sample_rate"] = sample_rate;
        config["chunk_size"] = chunk_size;
        config["bits_per_sample"] = 18;
        config["microphone"] = "SPH0645";
    }
    
private:
    bool initialized = false;
    uint32_t sample_rate = 16000;
    size_t chunk_size = 128;
    uint32_t* i2s_buffer;
    int debug_counter = 0;
};

#endif // I2S_INPUT_NODE_H