#ifndef PALETTE_MANAGER_H
#define PALETTE_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include "../config/hardware_config.h"
#include "../config/features.h"

// Forward declare the gradient palettes array
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

class PaletteManager {
private:
    CRGBPalette16 currentPalette;
    CRGBPalette16 targetPalette;
    uint8_t currentPaletteIndex;
    uint8_t blendSpeed;
    
    // Palette names for UI display
    static const char* paletteNames[];
    static const uint8_t NUM_PALETTES = 33;
    
public:
    PaletteManager() : 
        currentPaletteIndex(0),
        blendSpeed(24) {}
    
    void begin() {
        // Initialize with first palette
        setPalette(0);
    }
    
    void setPalette(uint8_t index) {
        if (index >= NUM_PALETTES) return;
        
        currentPaletteIndex = index;
        targetPalette = CRGBPalette16(gGradientPalettes[index]);
        
        #if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[PALETTE] Changed to: "));
        Serial.print(index);
        Serial.print(F(" - "));
        Serial.println(paletteNames[index]);
        #endif
    }
    
    void nextPalette() {
        setPalette((currentPaletteIndex + 1) % NUM_PALETTES);
    }
    
    void prevPalette() {
        setPalette((currentPaletteIndex == 0) ? NUM_PALETTES - 1 : currentPaletteIndex - 1);
    }
    
    void updatePaletteBlending() {
        nblendPaletteTowardPalette(currentPalette, targetPalette, blendSpeed);
    }
    
    void setBlendSpeed(uint8_t speed) {
        blendSpeed = speed;
    }
    
    // Getters
    CRGBPalette16& getCurrentPalette() { return currentPalette; }
    CRGBPalette16& getTargetPalette() { return targetPalette; }
    uint8_t getCurrentIndex() const { return currentPaletteIndex; }
    const char* getCurrentName() const { return paletteNames[currentPaletteIndex]; }
    
    static const char** getPaletteNames() { return paletteNames; }
    static uint8_t getPaletteCount() { return NUM_PALETTES; }
};

// Define palette names
const char* PaletteManager::paletteNames[] = {
    "Sunset_Real", "es_rivendell_15", "es_ocean_breeze_036", "rgi_15", "retro2_16",
    "Analogous_1", "es_pinksplash_08", "Coral_reef", "es_ocean_breeze_068", "es_pinksplash_07",
    "es_vintage_01", "departure", "es_landscape_64", "es_landscape_33", "rainbowsherbet",
    "gr65_hult", "gr64_hult", "GMT_drywet", "ib_jul01", "es_vintage_57",
    "ib15", "Fuschia_7", "es_emerald_dragon_08", "lava", "fire",
    "Colorfull", "Magenta_Evening", "Pink_Purple", "Sunset_Real", "es_autumn_19",
    "BlacK_Blue_Magenta_White", "BlacK_Magenta_Red", "BlacK_Red_Magenta_Yellow"
};


#endif // PALETTE_MANAGER_H