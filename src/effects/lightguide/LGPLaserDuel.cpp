// Light Guide Plate Laser Duel
// Opposing laser beams fight with deflections, sparks, and power struggles!

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
#include "LightGuideEffect.h"

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGB leds[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// Laser Duel effect class
class LGPLaserDuelEffect : public LightGuideEffect {
private:
    // Dueling laser states
    struct DuelLaser {
        float power;         // Current power level (0-1)
        float position;      // Beam front position
        CRGB color;         // Laser color
        float chargeRate;    // How fast it charges
        bool firing;        // Currently firing?
        float hitFlash;     // Flash when hit
    };
    
    DuelLaser leftLaser;   // Red team
    DuelLaser rightLaser;  // Blue team
    
    // Clash point where beams meet
    float clashPoint;
    float clashIntensity;
    
    // Spark effects
    struct Spark {
        float x, y;
        float vx, vy;
        CRGB color;
        float life;
        bool active;
    };
    
    static constexpr uint8_t MAX_SPARKS = 50;
    Spark sparks[MAX_SPARKS];
    
    // Battle parameters
    float battleIntensity;
    uint32_t lastClashTime;
    
public:
    LGPLaserDuelEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Laser Duel", s1, s2),
        clashPoint(HardwareConfig::STRIP_CENTER_POINT),
        clashIntensity(0), battleIntensity(0), lastClashTime(0) {
        
        // Initialize duelists
        leftLaser = {0.5f, 0, CRGB(255, 0, 0), 0.02f, false, 0};      // Red
        rightLaser = {0.5f, HardwareConfig::STRIP_LENGTH - 1, CRGB(0, 100, 255), 0.02f, false, 0}; // Blue
        
        // Clear sparks
        for (auto& spark : sparks) spark.active = false;
    }
    
    void createSparks(float pos, int count) {
        for (int i = 0; i < count && i < MAX_SPARKS; i++) {
            for (auto& spark : sparks) {
                if (!spark.active) {
                    spark.x = pos;
                    spark.y = 0;  // We'll use y for vertical spread visualization
                    spark.vx = random(-50, 51) / 10.0f;
                    spark.vy = random(0, 50) / 10.0f;
                    
                    // Sparks are mix of both laser colors plus white
                    uint8_t sparkType = random8();
                    if (sparkType < 85) {
                        spark.color = leftLaser.color;
                    } else if (sparkType < 170) {
                        spark.color = rightLaser.color;
                    } else {
                        spark.color = CRGB(255, 255, 100);  // Hot white/yellow
                    }
                    
                    spark.life = 1.0f;
                    spark.active = true;
                    break;
                }
            }
        }
    }
    
    void render() override {
        uint32_t now = millis();
        
        // Update battle intensity based on speed
        battleIntensity = paletteSpeed / 255.0f;
        
        // Charge lasers
        leftLaser.chargeRate = 0.01f + battleIntensity * 0.03f;
        rightLaser.chargeRate = 0.01f + battleIntensity * 0.03f;
        
        if (!leftLaser.firing) {
            leftLaser.power += leftLaser.chargeRate;
            if (leftLaser.power >= 1.0f) {
                leftLaser.power = 1.0f;
                leftLaser.firing = true;
                leftLaser.position = 0;
            }
        }
        
        if (!rightLaser.firing) {
            rightLaser.power += rightLaser.chargeRate;
            if (rightLaser.power >= 1.0f) {
                rightLaser.power = 1.0f;
                rightLaser.firing = true;
                rightLaser.position = HardwareConfig::STRIP_LENGTH - 1;
            }
        }
        
        // Move laser beams toward each other
        if (leftLaser.firing) {
            leftLaser.position += (2.0f + leftLaser.power * 3.0f) * visualParams.getIntensityNorm();
        }
        
        if (rightLaser.firing) {
            rightLaser.position -= (2.0f + rightLaser.power * 3.0f) * visualParams.getIntensityNorm();
        }
        
        // Check for clash
        if (leftLaser.firing && rightLaser.firing && 
            abs(leftLaser.position - rightLaser.position) < 10) {
            
            // CLASH! Power struggle at meeting point
            clashPoint = (leftLaser.position + rightLaser.position) / 2;
            
            // Stronger laser pushes the clash point
            float powerDiff = leftLaser.power - rightLaser.power + (random(-10, 11) / 100.0f);
            clashPoint += powerDiff * 5;
            
            // Update positions based on power struggle
            leftLaser.position = clashPoint - 5;
            rightLaser.position = clashPoint + 5;
            
            // Drain power during clash
            leftLaser.power -= 0.02f;
            rightLaser.power -= 0.02f;
            
            // Intense sparks at clash
            if (now - lastClashTime > 50) {
                createSparks(clashPoint, 5 + visualParams.getComplexityNorm() * 10);
                lastClashTime = now;
                clashIntensity = 1.0f;
                
                // Flash on hit
                leftLaser.hitFlash = 0.5f;
                rightLaser.hitFlash = 0.5f;
            }
            
            // End firing when power depleted
            if (leftLaser.power <= 0) {
                leftLaser.firing = false;
                leftLaser.power = 0;
            }
            if (rightLaser.power <= 0) {
                rightLaser.firing = false;
                rightLaser.power = 0;
            }
        }
        
        // Reset if beam reaches opposite end
        if (leftLaser.position >= HardwareConfig::STRIP_LENGTH - 5) {
            leftLaser.firing = false;
            leftLaser.power = 0;
            rightLaser.hitFlash = 1.0f;  // Got hit!
        }
        if (rightLaser.position <= 5) {
            rightLaser.firing = false;
            rightLaser.power = 0;
            leftLaser.hitFlash = 1.0f;  // Got hit!
        }
        
        // Fade background
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            strip1[i].fadeToBlackBy(30);
            strip2[i].fadeToBlackBy(30);
        }
        
        // Render laser beams
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Left laser (red team)
            if (leftLaser.firing && i <= leftLaser.position) {
                float distance = leftLaser.position - i;
                float intensity = 1.0f;
                
                // Beam gets thinner toward the front
                if (distance < 10) {
                    intensity = 1.0f - distance / 20.0f;
                }
                
                CRGB beamColor = leftLaser.color;
                beamColor.nscale8(intensity * 255 * leftLaser.power);
                strip1[i] += beamColor;
                
                // Different pattern on strip2 - pulsing
                if ((i % 4) < 2) {
                    strip2[i] += beamColor;
                }
            }
            
            // Right laser (blue team)
            if (rightLaser.firing && i >= rightLaser.position) {
                float distance = i - rightLaser.position;
                float intensity = 1.0f;
                
                if (distance < 10) {
                    intensity = 1.0f - distance / 20.0f;
                }
                
                CRGB beamColor = rightLaser.color;
                beamColor.nscale8(intensity * 255 * rightLaser.power);
                strip1[i] += beamColor;
                
                // Different pattern on strip2
                if ((i % 4) >= 2) {
                    strip2[i] += beamColor;
                }
            }
            
            // Power charge visualization on edges
            if (!leftLaser.firing && i < 10) {
                uint8_t chargeBright = leftLaser.power * 100;
                strip1[i] += CRGB(chargeBright, 0, 0);
                strip2[i] += CRGB(chargeBright/2, 0, 0);
            }
            
            if (!rightLaser.firing && i > HardwareConfig::STRIP_LENGTH - 10) {
                uint8_t chargeBright = rightLaser.power * 100;
                strip1[i] += CRGB(0, 0, chargeBright);
                strip2[i] += CRGB(0, 0, chargeBright/2);
            }
        }
        
        // Render clash point
        if (clashIntensity > 0) {
            clashIntensity -= 0.05f;
            
            for (int i = -10; i <= 10; i++) {
                int pos = clashPoint + i;
                if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                    float dist = abs(i) / 10.0f;
                    float intensity = (1.0f - dist) * clashIntensity;
                    
                    // White hot clash core
                    uint8_t clashBright = intensity * 255;
                    CRGB clashColor = CRGB(clashBright, clashBright, clashBright);
                    
                    // Add beam colors at edges
                    if (i < 0) {
                        clashColor += leftLaser.color.scale8(intensity * 128);
                    } else {
                        clashColor += rightLaser.color.scale8(intensity * 128);
                    }
                    
                    strip1[pos] += clashColor;
                    strip2[pos] += clashColor;
                }
            }
        }
        
        // Update and render sparks
        for (auto& spark : sparks) {
            if (!spark.active) continue;
            
            spark.x += spark.vx;
            spark.y += spark.vy;
            spark.vy -= 0.2f;  // Gravity
            spark.life -= 0.05f;
            
            if (spark.life <= 0 || spark.x < 0 || spark.x >= HardwareConfig::STRIP_LENGTH) {
                spark.active = false;
            } else {
                int pos = (int)spark.x;
                CRGB sparkColor = spark.color;
                sparkColor.nscale8(spark.life * 255);
                
                // Vertical spread visualization using brightness
                float yEffect = 1.0f - min(abs(spark.y) / 20.0f, 1.0f);
                sparkColor.nscale8(yEffect * 255);
                
                strip1[pos] += sparkColor;
                strip2[pos] += sparkColor;
            }
        }
        
        // Hit flash effects
        if (leftLaser.hitFlash > 0) {
            leftLaser.hitFlash -= 0.1f;
            for (int i = 0; i < 20; i++) {
                strip1[i] += CRGB(255, 100, 100).scale8(leftLaser.hitFlash * 255);
                strip2[i] += CRGB(255, 100, 100).scale8(leftLaser.hitFlash * 255);
            }
        }
        
        if (rightLaser.hitFlash > 0) {
            rightLaser.hitFlash -= 0.1f;
            for (int i = HardwareConfig::STRIP_LENGTH - 20; i < HardwareConfig::STRIP_LENGTH; i++) {
                strip1[i] += CRGB(100, 100, 255).scale8(rightLaser.hitFlash * 255);
                strip2[i] += CRGB(100, 100, 255).scale8(rightLaser.hitFlash * 255);
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPLaserDuelEffect* laserDuelInstance = nullptr;

// Effect function for main loop
void lgpLaserDuel() {
    if (!laserDuelInstance) {
        laserDuelInstance = new LGPLaserDuelEffect(strip1, strip2);
    }
    laserDuelInstance->render();
}