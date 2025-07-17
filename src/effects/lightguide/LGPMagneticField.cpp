// Light Guide Plate Magnetic Field Lines Effect
// Visualizes magnetic field patterns between opposing "magnets" at edges

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
#include "LightGuideEffect.h"
#include "LGPPhysicsEngine.h"

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGB leds[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// Magnetic field line tracer
struct FieldLine {
    float startX, startY;    // Starting position
    float currentX, currentY; // Current position along line
    float length;            // Total line length
    uint8_t brightness;      // Line brightness
    bool active;
};

// Magnetic field effect class
class LGPMagneticFieldEffect : public LightGuideEffect {
private:
    LGPPhysicsEngine physics;
    FieldLine fieldLines[16];
    uint32_t lastFieldUpdate;
    float magnetAngle;
    
    // Trace a magnetic field line
    void traceFieldLine(FieldLine& line, float stepSize = 0.01f) {
        if (!line.active) return;
        
        // Move along field direction
        float x = line.currentX;
        float y = line.currentY;
        
        // Simple dipole field calculation
        // North pole at (0, magnetY1), South pole at (1, magnetY2)
        float magnetY1 = 0.5f + 0.3f * sin(magnetAngle);
        float magnetY2 = 0.5f + 0.3f * cos(magnetAngle * 1.3f);
        
        // Vector from position to north pole
        float dx1 = 0.0f - x;
        float dy1 = magnetY1 - y;
        float r1 = sqrt(dx1*dx1 + dy1*dy1) + 0.01f;
        
        // Vector from position to south pole
        float dx2 = 1.0f - x;
        float dy2 = magnetY2 - y;
        float r2 = sqrt(dx2*dx2 + dy2*dy2) + 0.01f;
        
        // Magnetic field direction (simplified dipole)
        float Bx = -dx1/(r1*r1*r1) + dx2/(r2*r2*r2);
        float By = -dy1/(r1*r1*r1) + dy2/(r2*r2*r2);
        
        // Normalize
        float B = sqrt(Bx*Bx + By*By) + 0.001f;
        Bx /= B;
        By /= B;
        
        // Step along field line
        line.currentX += Bx * stepSize;
        line.currentY += By * stepSize;
        line.length += stepSize;
        
        // Check bounds
        if (line.currentX < 0 || line.currentX > 1 || 
            line.currentY < 0 || line.currentY > 1 ||
            line.length > 2.0f) {
            line.active = false;
        }
    }
    
public:
    LGPMagneticFieldEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Magnetic Field", s1, s2),
        lastFieldUpdate(0),
        magnetAngle(0) {
        physics.reset();
        
        // Initialize field lines
        for (auto& line : fieldLines) {
            line.active = false;
        }
    }
    
    void render() override {
        uint32_t now = millis();
        
        // Update magnet positions
        magnetAngle += paletteSpeed * 0.001f;
        
        // Spawn new field lines periodically
        if (now - lastFieldUpdate > 200) {
            // Find inactive line
            for (auto& line : fieldLines) {
                if (!line.active) {
                    // Start from various positions around magnets
                    float angle = random8() * TWO_PI / 255.0f;
                    float radius = 0.05f + random8() * 0.05f / 255.0f;
                    
                    if (random8() < 128) {
                        // Start near left magnet
                        line.startX = 0.0f + radius * cos(angle);
                        line.startY = 0.5f + 0.3f * sin(magnetAngle) + radius * sin(angle);
                    } else {
                        // Start near right magnet
                        line.startX = 1.0f + radius * cos(angle);
                        line.startY = 0.5f + 0.3f * cos(magnetAngle * 1.3f) + radius * sin(angle);
                    }
                    
                    line.currentX = line.startX;
                    line.currentY = line.startY;
                    line.length = 0;
                    line.brightness = 255;
                    line.active = true;
                    break;
                }
            }
            lastFieldUpdate = now;
        }
        
        // Clear strips
        fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 30);
        fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 30);
        
        // Update magnetic field strength based on complexity
        float fieldStrength = visualParams.getIntensityNorm();
        physics.setupMagneticField(fieldStrength * visualParams.getComplexityNorm() * 0.5f);
        
        // Render field magnitude as background glow
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float y = (float)i / HardwareConfig::STRIP_LENGTH;
            
            // Distance to magnets
            float magnetY1 = 0.5f + 0.3f * sin(magnetAngle);
            float magnetY2 = 0.5f + 0.3f * cos(magnetAngle * 1.3f);
            
            float dist1 = abs(y - magnetY1);
            float dist2 = abs(y - magnetY2);
            
            // Field intensity falls off with distance
            uint8_t glow1 = (uint8_t)(exp(-dist1 * 10) * 100 * visualParams.getSaturationNorm());
            uint8_t glow2 = (uint8_t)(exp(-dist2 * 10) * 100 * visualParams.getSaturationNorm());
            
            // North pole = red, South pole = blue
            strip1[i] = blend(strip1[i], CHSV(0, 255, glow1), 64);    // Red glow
            strip2[i] = blend(strip2[i], CHSV(160, 255, glow2), 64);  // Blue glow
        }
        
        // Trace and render field lines
        for (auto& line : fieldLines) {
            if (line.active) {
                // Trace line forward
                for (int step = 0; step < 5; step++) {
                    traceFieldLine(line, 0.02f);
                }
                
                // Render line segment
                if (line.active) {
                    // Map to LED position
                    uint16_t ledY = line.currentY * HardwareConfig::STRIP_LENGTH;
                    
                    if (ledY < HardwareConfig::STRIP_LENGTH) {
                        // Brightness based on field line density
                        uint8_t lineBright = scale8(line.brightness, 255 - line.length * 50);
                        
                        // Color based on position along line
                        uint8_t hue = gHue + line.length * 100;
                        CRGB color = ColorFromPalette(currentPalette, hue, lineBright);
                        
                        // Render to appropriate edge with anti-aliasing
                        float edgeDist = min(line.currentX, 1.0f - line.currentX) * 2.0f;
                        uint8_t edgeBright = 255 - (uint8_t)(edgeDist * 255);
                        
                        if (line.currentX < 0.5f) {
                            strip1[ledY] = blend(strip1[ledY], color, edgeBright);
                        } else {
                            strip2[ledY] = blend(strip2[ledY], color, edgeBright);
                        }
                    }
                }
                
                // Fade out old lines
                line.brightness = scale8(line.brightness, 250);
                if (line.brightness < 10) {
                    line.active = false;
                }
            }
        }
        
        // Add "magnetic pole" indicators
        uint16_t northPos = (0.5f + 0.3f * sin(magnetAngle)) * HardwareConfig::STRIP_LENGTH;
        uint16_t southPos = (0.5f + 0.3f * cos(magnetAngle * 1.3f)) * HardwareConfig::STRIP_LENGTH;
        
        if (northPos < HardwareConfig::STRIP_LENGTH) {
            strip1[northPos] = CRGB::Red;  // North pole
        }
        if (southPos < HardwareConfig::STRIP_LENGTH) {
            strip2[southPos] = CRGB::Blue; // South pole
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPMagneticFieldEffect* magneticFieldInstance = nullptr;

// Effect function for main loop
void lgpMagneticField() {
    if (!magneticFieldInstance) {
        magneticFieldInstance = new LGPMagneticFieldEffect(strip1, strip2);
    }
    magneticFieldInstance->render();
}