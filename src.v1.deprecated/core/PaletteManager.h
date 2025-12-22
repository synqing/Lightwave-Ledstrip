#ifndef PALETTE_MANAGER_H
#define PALETTE_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include "../config/hardware_config.h"
#include "../config/features.h"
#include "../Palettes_Master.h"  // Use master palette system

class PaletteManager {
private:
    CRGBPalette16 currentPalette;
    CRGBPalette16 targetPalette;
    uint8_t currentPaletteIndex;
    uint8_t blendSpeed;

public:
    PaletteManager() :
        currentPaletteIndex(0),
        blendSpeed(24) {}

    void begin() {
        // Initialize with first palette
        setPalette(0);
    }

    void setPalette(uint8_t index) {
        if (index >= gMasterPaletteCount) return;

        currentPaletteIndex = index;
        targetPalette = CRGBPalette16(gMasterPalettes[index]);

        #if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[PALETTE] Changed to: "));
        Serial.print(index);
        Serial.print(F(" - "));
        Serial.println(MasterPaletteNames[index]);
        #endif
    }

    void nextPalette() {
        setPalette((currentPaletteIndex + 1) % gMasterPaletteCount);
    }

    void prevPalette() {
        setPalette((currentPaletteIndex == 0) ? gMasterPaletteCount - 1 : currentPaletteIndex - 1);
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
    const char* getCurrentName() const { return MasterPaletteNames[currentPaletteIndex]; }

    static const char* const* getPaletteNames() { return MasterPaletteNames; }
    static uint8_t getPaletteCount() { return gMasterPaletteCount; }
};

#endif // PALETTE_MANAGER_H
