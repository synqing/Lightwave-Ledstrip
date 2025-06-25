#include "CinematicColorOrchestrator.h"
#include "../Palettes.h"

// Curated theme palettes - each theme tells a visual story
const TProgmemRGBGradientPaletteRef UNDERWATER_PALETTES[] = {
    es_ocean_breeze_036_gp,   // Deep ocean mystery
    es_ocean_breeze_068_gp,   // Turbulent waters
    Coral_reef_gp,            // Living reef colors
    Blue_Cyan_Yellow_gp       // Tropical shallows
};

const TProgmemRGBGradientPaletteRef VOLCANIC_PALETTES[] = {
    lava_gp,                  // Molten rock
    fire_gp,                  // Raging flames
    es_vintage_57_gp,         // Smoldering embers
    Sunset_Real_gp            // Volcanic sunset
};

const TProgmemRGBGradientPaletteRef MYSTICAL_PALETTES[] = {
    es_rivendell_15_gp,       // Elvish magic
    es_emerald_dragon_08_gp,  // Dragon's breath
    departure_gp,             // Otherworldly journey
    Fuschia_7_gp              // Arcane energy
};

const TProgmemRGBGradientPaletteRef SUNSET_PALETTES[] = {
    Sunset_Real_gp,           // Natural sunset
    es_autumn_19_gp,          // Autumn leaves
    Magenta_Evening_gp,       // Twilight magic
    retro2_16_gp             // Golden hour
};

const TProgmemRGBGradientPaletteRef SYNTHWAVE_PALETTES[] = {
    es_pinksplash_07_gp,      // Neon pink
    es_pinksplash_08_gp,      // Electric magenta
    BlacK_Blue_Magenta_White_gp, // Cyberpunk contrast
    gr65_hult_gp              // Retro gradient
};

const TProgmemRGBGradientPaletteRef INDUSTRIAL_PALETTES[] = {
    GMT_drywet_gp,            // Urban decay
    gr64_hult_gp,             // Metallic tones
    es_landscape_64_gp,       // Industrial landscape
    BlacK_Magenta_Red_gp      // Warning lights
};

const TProgmemRGBGradientPaletteRef ORGANIC_PALETTES[] = {
    es_landscape_33_gp,       // Forest canopy
    es_emerald_dragon_08_gp,  // Living green
    Colorfull_gp,             // Biodiversity
    es_vintage_01_gp          // Earth tones
};

const TProgmemRGBGradientPaletteRef COSMIC_PALETTES[] = {
    BlacK_Blue_Magenta_White_gp, // Deep space
    Pink_Purple_gp,           // Nebula colors
    ib15_gp,                  // Stellar phenomena
    BlacK_Red_Magenta_Yellow_gp  // Solar flares
};

// Theme definitions with metadata
const ThemePalettes THEME_PALETTES[THEME_COUNT] = {
    {UNDERWATER_PALETTES, 4, "Underwater Depths"},
    {VOLCANIC_PALETTES, 4, "Volcanic Fire"},
    {MYSTICAL_PALETTES, 4, "Mystical Realms"},
    {SUNSET_PALETTES, 4, "Golden Sunset"},
    {SYNTHWAVE_PALETTES, 4, "Synthwave Retro"},
    {INDUSTRIAL_PALETTES, 4, "Industrial Edge"},
    {ORGANIC_PALETTES, 4, "Living Nature"},
    {COSMIC_PALETTES, 4, "Cosmic Wonder"}
};

// Emotion name strings for debugging
const char* EMOTION_NAMES[EMOTION_COUNT] = {
    "Tranquil", "Building", "Intense", "Explosive", "Resolution"
};

// Global instance
CinematicColorOrchestrator colorOrchestrator;

void CinematicColorOrchestrator::update(uint8_t globalHue) {
    uint32_t now = millis();
    m_lastUpdate = now;
    m_updateCount++;
    
    // Update emotional arc based on global hue
    updateEmotionalArc(globalHue);
    
    // Check if it's time to change themes
    updateThemeProgression();
    
    // Smooth palette blending
    updatePaletteBlending();
}

void CinematicColorOrchestrator::updateEmotionalArc(uint8_t globalHue) {
    // Map globalHue to emotional progression (0-255 = full emotional cycle)
    EmotionalState newEmotion = calculateEmotionalState(globalHue);
    
    if (newEmotion != m_currentEmotion) {
        m_currentEmotion = newEmotion;
        
        // Select new palette based on emotion and current theme
        const TProgmemRGBGradientPaletteRef* newPalette = selectPaletteForEmotion(newEmotion);
        if (newPalette) {
            blendToNewPalette(newPalette);
        }
    }
    
    // Update emotional intensity with custom curve
    float targetIntensity = calculateIntensityCurve(globalHue);
    
    // Smooth intensity changes
    float intensityDelta = targetIntensity - m_emotionalIntensity;
    m_emotionalIntensity += intensityDelta * 0.1f; // Smooth following
    m_emotionalIntensity = constrain(m_emotionalIntensity, 0.0f, 1.0f);
}

EmotionalState CinematicColorOrchestrator::calculateEmotionalState(uint8_t globalHue) {
    // Divide 0-255 range into emotional sections
    // Create dramatic arc: Tranquil -> Building -> Intense -> Explosive -> Resolution
    if (globalHue < 51) return EMOTION_TRANQUIL;      // 0-50: Peace
    if (globalHue < 102) return EMOTION_BUILDING;     // 51-101: Rising tension  
    if (globalHue < 153) return EMOTION_INTENSE;      // 102-152: High drama
    if (globalHue < 204) return EMOTION_EXPLOSIVE;    // 153-203: Peak intensity
    return EMOTION_RESOLUTION;                        // 204-255: Resolution
}

float CinematicColorOrchestrator::calculateIntensityCurve(uint8_t globalHue) {
    // Custom intensity curve - not linear, creates dramatic arc
    float t = globalHue / 255.0f;
    
    // Create asymmetric curve: slow build, sharp peak, gentle resolution
    if (t < 0.6f) {
        // Slow exponential build to 60%
        return pow(t / 0.6f, 2.2f) * 0.7f;
    } else if (t < 0.8f) {
        // Sharp rise to peak (60% to 80%)
        float peakT = (t - 0.6f) / 0.2f;
        return 0.7f + (pow(peakT, 0.8f) * 0.3f);
    } else {
        // Gentle decay after peak (80% to 100%)
        float decayT = (t - 0.8f) / 0.2f;
        return 1.0f - (pow(decayT, 1.8f) * 0.7f);
    }
}

void CinematicColorOrchestrator::updateThemeProgression() {
    uint32_t now = millis();
    
    // Automatic theme progression
    if (now - m_themeStartTime > m_themeDuration) {
        LightShowTheme newTheme = selectNextTheme();
        setTheme(newTheme);
    }
}

LightShowTheme CinematicColorOrchestrator::selectNextTheme() {
    // Intelligent theme progression - avoid repetition, create variety
    static uint8_t themeHistory[3] = {255, 255, 255}; // Recent themes
    
    LightShowTheme candidates[THEME_COUNT];
    uint8_t candidateCount = 0;
    
    // Build list of themes not recently used
    for (uint8_t i = 0; i < THEME_COUNT; i++) {
        bool recentlyUsed = false;
        for (uint8_t h = 0; h < 3; h++) {
            if (themeHistory[h] == i) {
                recentlyUsed = true;
                break;
            }
        }
        if (!recentlyUsed) {
            candidates[candidateCount++] = (LightShowTheme)i;
        }
    }
    
    // If no candidates (shouldn't happen), use any theme
    if (candidateCount == 0) {
        candidateCount = THEME_COUNT;
        for (uint8_t i = 0; i < THEME_COUNT; i++) {
            candidates[i] = (LightShowTheme)i;
        }
    }
    
    // Select random from candidates
    LightShowTheme selected = candidates[random8(candidateCount)];
    
    // Update history
    themeHistory[2] = themeHistory[1];
    themeHistory[1] = themeHistory[0];
    themeHistory[0] = selected;
    
    return selected;
}

void CinematicColorOrchestrator::setTheme(LightShowTheme theme) {
    if (theme >= THEME_COUNT) return;
    
    m_currentTheme = theme;
    m_themeStartTime = millis();
    
    initializeTheme(theme);
    
    Serial.printf("🎬 Theme changed to: %s\n", getCurrentThemeName());
}

void CinematicColorOrchestrator::initializeTheme(LightShowTheme theme) {
    // Start with the first palette of the new theme
    const ThemePalettes& themeData = THEME_PALETTES[theme];
    if (themeData.count > 0) {
        m_activePalette = CRGBPalette16(themeData.palettes[0]);
        m_targetPalette = m_activePalette;
        m_blendProgress = 255; // Fully blended to start
    }
}

const TProgmemRGBGradientPaletteRef* CinematicColorOrchestrator::selectPaletteForEmotion(EmotionalState emotion) {
    const ThemePalettes& themeData = THEME_PALETTES[m_currentTheme];
    
    // Map emotion to palette index within theme
    uint8_t paletteIndex;
    switch (emotion) {
        case EMOTION_TRANQUIL:
            paletteIndex = 0; // First palette - usually most subtle
            break;
        case EMOTION_BUILDING:
            paletteIndex = 1; // Second palette - rising energy
            break;
        case EMOTION_INTENSE:
            paletteIndex = 2; // Third palette - high contrast
            break;
        case EMOTION_EXPLOSIVE:
            paletteIndex = (themeData.count > 3) ? 3 : (themeData.count - 1); // Most intense
            break;
        case EMOTION_RESOLUTION:
            paletteIndex = 0; // Return to calm
            break;
        default:
            paletteIndex = 0;
    }
    
    // Ensure index is valid
    paletteIndex = paletteIndex % themeData.count;
    
    return &themeData.palettes[paletteIndex];
}

void CinematicColorOrchestrator::blendToNewPalette(const TProgmemRGBGradientPaletteRef* newPalette) {
    m_targetPalette = CRGBPalette16(*newPalette);
    m_blendProgress = 0; // Start blending
}

void CinematicColorOrchestrator::updatePaletteBlending() {
    if (m_blendProgress < 255) {
        // Smooth palette transition
        m_blendProgress += 3; // Blend speed
        if (m_blendProgress > 255) m_blendProgress = 255;
        
        // Use FastLED's palette blending
        nblendPaletteTowardPalette(m_activePalette, m_targetPalette, m_blendProgress);
    }
}

CRGB CinematicColorOrchestrator::getEmotionalColor(uint8_t position, uint8_t brightness) {
    // CRITICAL FIX: Get base color without brightness (apply brightness only once)
    CRGB color = ColorFromPalette(m_activePalette, position, 255);
    
    // Apply emotional modifications
    color = applyEmotionalTint(color);
    uint8_t finalBrightness = applyEmotionalBrightness(brightness);
    
    // Apply brightness only once (not twice!)
    color.nscale8(finalBrightness);
    
    return color;
}

uint8_t CinematicColorOrchestrator::applyEmotionalBrightness(uint8_t baseBrightness) {
    // Modify brightness based on emotional intensity
    float intensityMultiplier = 0.4f + (m_emotionalIntensity * 0.6f); // 40% to 100%
    
    // Add some drama - slight pulsing during intense emotions
    if (m_emotionalIntensity > 0.7f) {
        float pulse = sin8(millis() / 50) / 255.0f; // Fast pulse
        intensityMultiplier += pulse * 0.2f * m_emotionalIntensity;
    }
    
    return baseBrightness * intensityMultiplier;
}

CRGB CinematicColorOrchestrator::applyEmotionalTint(CRGB baseColor) {
    // Subtle color temperature shifts based on emotion
    switch (m_currentEmotion) {
        case EMOTION_TRANQUIL:
            // Slightly cooler, more blue
            baseColor.blue = qadd8(baseColor.blue, 10);
            break;
        case EMOTION_BUILDING:
            // Warmer, slight yellow tint  
            baseColor.red = qadd8(baseColor.red, 5);
            baseColor.green = qadd8(baseColor.green, 3);
            break;
        case EMOTION_INTENSE:
            // More saturated, slight red push
            baseColor.red = qadd8(baseColor.red, 8);
            break;
        case EMOTION_EXPLOSIVE:
            // Maximum saturation and slight red boost
            baseColor.red = qadd8(baseColor.red, 15);
            break;
        case EMOTION_RESOLUTION:
            // Return to neutral with slight warmth
            baseColor.green = qadd8(baseColor.green, 3);
            break;
    }
    
    return baseColor;
}

const char* CinematicColorOrchestrator::getCurrentThemeName() const {
    if (m_currentTheme < THEME_COUNT) {
        return THEME_PALETTES[m_currentTheme].name;
    }
    return "Unknown";
}

const char* CinematicColorOrchestrator::getCurrentEmotionName() const {
    if (m_currentEmotion < EMOTION_COUNT) {
        return EMOTION_NAMES[m_currentEmotion];
    }
    return "Unknown";
}

void CinematicColorOrchestrator::forceEmotionalState(EmotionalState emotion) {
    if (emotion < EMOTION_COUNT) {
        m_currentEmotion = emotion;
        
        // Immediately update palette
        const TProgmemRGBGradientPaletteRef* newPalette = selectPaletteForEmotion(emotion);
        if (newPalette) {
            blendToNewPalette(newPalette);
        }
        
        Serial.printf("🎭 Emotion forced to: %s\n", getCurrentEmotionName());
    }
}

void CinematicColorOrchestrator::triggerEmotionalPeak() {
    forceEmotionalState(EMOTION_EXPLOSIVE);
    m_emotionalIntensity = 1.0f; // Maximum intensity
    Serial.println("💥 Emotional peak triggered!");
}