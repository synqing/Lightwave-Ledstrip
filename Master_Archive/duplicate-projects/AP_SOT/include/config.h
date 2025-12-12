#ifndef CONFIG_H
#define CONFIG_H

// Audio processing configuration
#define SAMPLE_RATE 16000       // Hz
#define CHUNK_SIZE 128          // Samples per frame
#define FREQUENCY_BINS 96       // Number of Goertzel frequency bins

// I2S Configuration
#define I2S_BCLK_PIN 16
#define I2S_LRCLK_PIN 4
#define I2S_DIN_PIN 10

// DC Offset for SPH0645
#define DEFAULT_DC_OFFSET -5200

// Performance targets
#define TARGET_LATENCY_MS 8     // Maximum processing time per frame
#define TARGET_FPS 125          // Target frame rate

#endif // CONFIG_H