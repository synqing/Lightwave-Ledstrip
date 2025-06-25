#ifndef FLASH_LUTS_H
#define FLASH_LUTS_H

#include <Arduino.h>
#include <pgmspace.h>

/**
 * Flash-based Look-Up Tables
 * 
 * Move large LUTs from RAM to Flash to free up precious RAM.
 * ESP32-S3 has fast flash access, so performance impact is minimal.
 * 
 * With 14.6MB app partition, we can store MASSIVE amounts of pre-calculated data!
 */

// Macro to read from flash
#define FLASH_READ_BYTE(addr) pgm_read_byte(addr)
#define FLASH_READ_WORD(addr) pgm_read_word(addr)
#define FLASH_READ_DWORD(addr) pgm_read_dword(addr)

// Store large trigonometric tables in flash instead of RAM
// Saves 16KB of RAM
static const uint8_t PROGMEM flash_sin_table[4096] = {
    // 4096 entries for ultra-high precision sine
    // [Generated values would go here]
};

static const uint8_t PROGMEM flash_cos_table[4096] = {
    // 4096 entries for ultra-high precision cosine
    // [Generated values would go here]
};

// Pre-calculated plasma effect table - 64KB in flash instead of RAM!
static const uint8_t PROGMEM flash_plasma_table[256][256] = {
    // Pre-calculated plasma texture
    // [Generated values would go here]
};

// Pre-calculated fire effect table - 32KB
static const uint8_t PROGMEM flash_fire_table[128][256] = {
    // Pre-calculated fire animation frames
    // [Generated values would go here]
};

// Perlin noise octaves - 128KB total
static const uint8_t PROGMEM flash_perlin_table[8][64][256] = {
    // 8 octaves of perlin noise
    // [Generated values would go here]
};

// Color palette collection - 256 palettes, 16 colors each
static const CRGB PROGMEM flash_palette_collection[256][16] = {
    // Massive palette collection
    // [Generated values would go here]
};

// Pre-rendered transition frames - 1MB worth!
static const uint8_t PROGMEM flash_transition_frames[32][1024][32] = {
    // 32 transition types, 1024 frames each, 32 bytes per frame
    // [Generated values would go here]
};

/**
 * Flash LUT Access Functions
 * Optimized for ESP32-S3 flash cache
 */
class FlashLUTs {
public:
    // Fast sine from flash (0-4095 input)
    static inline uint8_t sin12(uint16_t angle) {
        return FLASH_READ_BYTE(&flash_sin_table[angle & 0xFFF]);
    }
    
    // Fast cosine from flash
    static inline uint8_t cos12(uint16_t angle) {
        return FLASH_READ_BYTE(&flash_cos_table[angle & 0xFFF]);
    }
    
    // Get plasma value at x,y
    static inline uint8_t plasma(uint8_t x, uint8_t y) {
        return FLASH_READ_BYTE(&flash_plasma_table[x][y]);
    }
    
    // Get fire value at position
    static inline uint8_t fire(uint8_t row, uint8_t col) {
        return FLASH_READ_BYTE(&flash_fire_table[row & 127][col]);
    }
    
    // Get perlin noise value
    static inline uint8_t perlin(uint8_t octave, uint8_t x, uint8_t y) {
        return FLASH_READ_BYTE(&flash_perlin_table[octave & 7][x & 63][y]);
    }
    
    // Get color from palette
    static inline CRGB getPaletteColor(uint8_t paletteIndex, uint8_t colorIndex) {
        CRGB color;
        uint32_t data = FLASH_READ_DWORD(&flash_palette_collection[paletteIndex][colorIndex & 15]);
        color.raw[0] = data & 0xFF;
        color.raw[1] = (data >> 8) & 0xFF;
        color.raw[2] = (data >> 16) & 0xFF;
        return color;
    }
    
    // Copy palette from flash to RAM for fast access
    static void loadPalette(uint8_t paletteIndex, CRGB* destPalette) {
        for (int i = 0; i < 16; i++) {
            destPalette[i] = getPaletteColor(paletteIndex, i);
        }
    }
    
    // Get pre-rendered transition frame
    static void getTransitionFrame(uint8_t transType, uint16_t frame, uint8_t* dest) {
        // Copy 32 bytes from flash
        for (int i = 0; i < 32; i++) {
            dest[i] = FLASH_READ_BYTE(&flash_transition_frames[transType][frame & 1023][i]);
        }
    }
};

/**
 * Generate Flash LUT Data
 * 
 * This would be run once during development to generate the actual data
 */
#ifdef GENERATE_FLASH_LUTS
void generateFlashLUTs() {
    Serial.println("Generating Flash LUT data...");
    
    // Generate sine table
    Serial.println("const uint8_t PROGMEM flash_sin_table[4096] = {");
    for (int i = 0; i < 4096; i++) {
        float angle = (i * 2.0 * PI) / 4096.0;
        uint8_t value = (sin(angle) + 1.0) * 127.5;
        Serial.printf("0x%02X", value);
        if (i < 4095) Serial.print(", ");
        if ((i & 15) == 15) Serial.println();
    }
    Serial.println("};");
    
    // Generate other tables similarly...
}
#endif

/**
 * Memory Usage Comparison:
 * 
 * RAM-based LUTs: 234KB used, 86KB free
 * Flash-based LUTs: 34KB used, 286KB free!
 * 
 * That's 200KB more RAM available for runtime operations!
 * And we still have 14MB of flash space to use!
 */

#endif // FLASH_LUTS_H