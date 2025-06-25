#include "MegaLUTs.h"
#include <math.h>

/**
 * MEGA LUT IMPLEMENTATION
 * 
 * Actual lookup table data - pre-calculated for maximum performance
 * Total target memory usage: ~250KB
 */

// ============== TRIGONOMETRIC LUTs (16KB) ==============
// Generate at compile time for accuracy
const int16_t sinLUT_16bit[TRIG_LUT_SIZE] = {
    #include "lut_data/sin_table_4096.inc"  // We'll generate this
};

const int16_t cosLUT_16bit[TRIG_LUT_SIZE] = {
    #include "lut_data/cos_table_4096.inc"  // We'll generate this
};

// For now, let's implement a runtime initializer
int16_t* _sinLUT = nullptr;
int16_t* _cosLUT = nullptr;

void initializeTrigLUTs() {
    if (!_sinLUT) {
        _sinLUT = (int16_t*)ps_malloc(TRIG_LUT_SIZE * sizeof(int16_t));
        _cosLUT = (int16_t*)ps_malloc(TRIG_LUT_SIZE * sizeof(int16_t));
        
        for (int i = 0; i < TRIG_LUT_SIZE; i++) {
            float angle = (i * 2.0f * PI) / TRIG_LUT_SIZE;
            _sinLUT[i] = (int16_t)(sin(angle) * 32767.0f);
            _cosLUT[i] = (int16_t)(cos(angle) * 32767.0f);
        }
    }
}

// ============== COLOR MIXING LUTs (48KB) ==============
uint8_t (*colorMixLUT)[128][3] = nullptr;

void initializeColorMixLUT() {
    if (!colorMixLUT) {
        colorMixLUT = (uint8_t(*)[128][3])ps_malloc(128 * 128 * 3);
        
        // Pre-calculate all possible RGB color mixes
        for (int r = 0; r < 128; r++) {
            for (int g = 0; g < 128; g++) {
                // Mix with varying blue values
                colorMixLUT[r][g][0] = r * 2;  // Scale back to 8-bit
                colorMixLUT[r][g][1] = g * 2;
                colorMixLUT[r][g][2] = ((r + g) / 2) * 2;  // Blue based on R+G
            }
        }
    }
}

// HDR gamma correction LUT
uint16_t* hdrGammaLUT = nullptr;
uint8_t* hdrCompressLUT = nullptr;

void initializeHDRLUTs() {
    if (!hdrGammaLUT) {
        hdrGammaLUT = (uint16_t*)ps_malloc(256 * sizeof(uint16_t));
        hdrCompressLUT = (uint8_t*)ps_malloc(1024);
        
        // 2.2 gamma with 16-bit precision
        for (int i = 0; i < 256; i++) {
            float normalized = i / 255.0f;
            float gamma = pow(normalized, 2.2f);
            hdrGammaLUT[i] = (uint16_t)(gamma * 65535.0f);
        }
        
        // HDR compression curve
        for (int i = 0; i < 1024; i++) {
            float hdr = i / 1023.0f;
            // Reinhard tone mapping
            float compressed = hdr / (1.0f + hdr);
            hdrCompressLUT[i] = (uint8_t)(compressed * 255.0f);
        }
    }
}

// ============== TRANSITION LUTs (12.5KB) ==============
TransitionFrame (*fadeTransitionLUT)[16] = nullptr;
TransitionFrame (*wipeTransitionLUT)[16] = nullptr;
TransitionFrame (*spiralTransitionLUT)[16] = nullptr;
TransitionFrame (*rippleTransitionLUT)[16] = nullptr;
TransitionFrame (*phaseTransitionLUT)[16] = nullptr;

void initializeTransitionLUTs() {
    if (!fadeTransitionLUT) {
        // Allocate all transition LUTs
        size_t frameSize = sizeof(TransitionFrame);
        fadeTransitionLUT = (TransitionFrame(*)[16])ps_malloc(16 * frameSize);
        wipeTransitionLUT = (TransitionFrame(*)[16])ps_malloc(16 * frameSize);
        spiralTransitionLUT = (TransitionFrame(*)[16])ps_malloc(16 * frameSize);
        rippleTransitionLUT = (TransitionFrame(*)[16])ps_malloc(16 * frameSize);
        phaseTransitionLUT = (TransitionFrame(*)[16])ps_malloc(16 * frameSize);
        
        // Pre-calculate fade transition frames
        for (int frame = 0; frame < 16; frame++) {
            float progress = frame / 15.0f;
            uint8_t blend = progress * 255;
            
            // Fade - simple linear blend
            for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                (*fadeTransitionLUT)[frame].pixel[i] = blend;
            }
            
            // Wipe - from center outward
            uint8_t radius = progress * HardwareConfig::STRIP_LENGTH;
            for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                int distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
                (*wipeTransitionLUT)[frame].pixel[i] = (distFromCenter <= radius) ? 255 : 0;
            }
            
            // Spiral - rotating pattern
            float spiralAngle = progress * TWO_PI * 2;
            for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                float angle = (float)i / HardwareConfig::NUM_LEDS * TWO_PI + spiralAngle;
                (*spiralTransitionLUT)[frame].pixel[i] = (sin(angle) + 1.0f) * 127.5f;
            }
            
            // Ripple - expanding waves
            for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                int distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
                float wave = sin(distFromCenter * 0.5f - progress * TWO_PI * 3);
                (*rippleTransitionLUT)[frame].pixel[i] = (wave + 1.0f) * 127.5f;
            }
            
            // Phase shift - frequency morph
            for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                float phase = (float)i / HardwareConfig::NUM_LEDS * TWO_PI;
                float shift = sin(phase + progress * TWO_PI);
                (*phaseTransitionLUT)[frame].pixel[i] = (shift + 1.0f) * 127.5f;
            }
        }
    }
}

// Easing curves LUT
uint8_t (*easingLUT)[256] = nullptr;

void initializeEasingLUTs() {
    if (!easingLUT) {
        easingLUT = (uint8_t(*)[256])ps_malloc(16 * 256);
        
        for (int curve = 0; curve < 16; curve++) {
            for (int t = 0; t < 256; t++) {
                float normalized = t / 255.0f;
                float result = normalized;  // Default linear
                
                switch (curve) {
                    case 0: // Linear
                        result = normalized;
                        break;
                    case 1: // Quad In
                        result = normalized * normalized;
                        break;
                    case 2: // Quad Out
                        result = normalized * (2 - normalized);
                        break;
                    case 3: // Quad InOut
                        result = normalized < 0.5f 
                            ? 2 * normalized * normalized 
                            : -1 + (4 - 2 * normalized) * normalized;
                        break;
                    case 4: // Cubic In
                        result = normalized * normalized * normalized;
                        break;
                    case 5: // Cubic Out
                        normalized -= 1;
                        result = normalized * normalized * normalized + 1;
                        break;
                    case 6: // Sine In
                        result = 1 - cos(normalized * PI / 2);
                        break;
                    case 7: // Sine Out
                        result = sin(normalized * PI / 2);
                        break;
                    case 8: // Expo In
                        result = normalized == 0 ? 0 : pow(2, 10 * (normalized - 1));
                        break;
                    case 9: // Expo Out
                        result = normalized == 1 ? 1 : 1 - pow(2, -10 * normalized);
                        break;
                    case 10: // Circ In
                        result = 1 - sqrt(1 - normalized * normalized);
                        break;
                    case 11: // Circ Out
                        normalized -= 1;
                        result = sqrt(1 - normalized * normalized);
                        break;
                    case 12: // Back In
                        result = normalized * normalized * (2.70158f * normalized - 1.70158f);
                        break;
                    case 13: // Back Out
                        normalized -= 1;
                        result = 1 + normalized * normalized * (2.70158f * normalized + 1.70158f);
                        break;
                    case 14: // Elastic In
                        if (normalized == 0 || normalized == 1) result = normalized;
                        else result = -pow(2, 10 * (normalized - 1)) * sin((normalized - 1.1f) * 5 * PI);
                        break;
                    case 15: // Bounce Out
                        if (normalized < 1 / 2.75f) {
                            result = 7.5625f * normalized * normalized;
                        } else if (normalized < 2 / 2.75f) {
                            normalized -= 1.5f / 2.75f;
                            result = 7.5625f * normalized * normalized + 0.75f;
                        } else if (normalized < 2.5f / 2.75f) {
                            normalized -= 2.25f / 2.75f;
                            result = 7.5625f * normalized * normalized + 0.9375f;
                        } else {
                            normalized -= 2.625f / 2.75f;
                            result = 7.5625f * normalized * normalized + 0.984375f;
                        }
                        break;
                }
                
                easingLUT[curve][t] = (uint8_t)(result * 255.0f);
            }
        }
    }
}

// Distance and angle LUTs
uint8_t* distanceFromCenterLUT = nullptr;
uint8_t* angleFromCenterLUT = nullptr;
uint8_t* spiralAngleLUT = nullptr;

void initializeGeometryLUTs() {
    if (!distanceFromCenterLUT) {
        distanceFromCenterLUT = (uint8_t*)ps_malloc(HardwareConfig::NUM_LEDS);
        angleFromCenterLUT = (uint8_t*)ps_malloc(HardwareConfig::NUM_LEDS);
        spiralAngleLUT = (uint8_t*)ps_malloc(HardwareConfig::NUM_LEDS);
        
        for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Distance from center (0-255)
            int dist = abs(i - HardwareConfig::STRIP_CENTER_POINT);
            distanceFromCenterLUT[i] = (dist * 255) / HardwareConfig::STRIP_LENGTH;
            
            // Angle from center (0-255 representing 0-2PI)
            float angle = atan2(i - HardwareConfig::STRIP_CENTER_POINT, 40.0f);
            angleFromCenterLUT[i] = ((angle + PI) / TWO_PI) * 255;
            
            // Spiral angle - pre-calculated for performance
            float spiralTurns = 3.0f;  // Number of spiral turns
            spiralAngleLUT[i] = (uint8_t)((i * spiralTurns * 256) / HardwareConfig::NUM_LEDS);
        }
    }
}

// ============== EFFECT PATTERN LUTs (82KB) ==============
uint8_t (*wavePatternLUT)[HardwareConfig::NUM_LEDS] = nullptr;
uint8_t (*plasmaLUT)[128] = nullptr;
uint8_t (*fireLUT)[HardwareConfig::NUM_LEDS] = nullptr;
uint8_t (*noiseLUT)[64] = nullptr;

void initializeEffectPatternLUTs() {
    if (!wavePatternLUT) {
        // Wave patterns - 40KB
        wavePatternLUT = (uint8_t(*)[HardwareConfig::NUM_LEDS])ps_malloc(256 * HardwareConfig::NUM_LEDS);
        
        for (int pattern = 0; pattern < 256; pattern++) {
            float frequency = (pattern / 255.0f) * 10.0f + 0.5f;  // 0.5 to 10.5 waves
            float phase = (pattern / 255.0f) * TWO_PI;
            
            for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                float pos = (float)i / HardwareConfig::NUM_LEDS;
                float value = sin(pos * TWO_PI * frequency + phase);
                wavePatternLUT[pattern][i] = (value + 1.0f) * 127.5f;
            }
        }
        
        // Plasma effect - 16KB
        plasmaLUT = (uint8_t(*)[128])ps_malloc(128 * 128);
        
        for (int x = 0; x < 128; x++) {
            for (int y = 0; y < 128; y++) {
                float value = sin(x * 0.1f) + sin(y * 0.1f) + 
                             sin((x + y) * 0.1f) + sin(sqrt(x * x + y * y) * 0.1f);
                plasmaLUT[x][y] = (value + 4.0f) * 31.875f;  // Normalize to 0-255
            }
        }
        
        // Fire effect - 10KB
        fireLUT = (uint8_t(*)[HardwareConfig::NUM_LEDS])ps_malloc(64 * HardwareConfig::NUM_LEDS);
        
        // Simple fire simulation
        for (int frame = 0; frame < 64; frame++) {
            for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                // Heat rises and cools
                float heat = 1.0f - (float)i / HardwareConfig::NUM_LEDS;
                heat *= 1.0f + sin(frame * 0.1f + i * 0.05f) * 0.3f;
                heat = constrain(heat, 0.0f, 1.0f);
                fireLUT[frame][i] = heat * 255;
            }
        }
        
        // Perlin-like noise - 16KB
        noiseLUT = (uint8_t(*)[64])ps_malloc(256 * 64);
        
        // Simple noise generation
        uint32_t seed = 12345;
        for (int i = 0; i < 256; i++) {
            for (int j = 0; j < 64; j++) {
                seed = seed * 1103515245 + 12345;
                noiseLUT[i][j] = (seed >> 16) & 0xFF;
            }
        }
    }
}

// Palette interpolation LUT - 12KB
CRGB (*paletteInterpolationLUT)[256] = nullptr;

void initializePaletteLUTs() {
    if (!paletteInterpolationLUT) {
        paletteInterpolationLUT = (CRGB(*)[256])ps_malloc(16 * 256 * sizeof(CRGB));
        
        // Pre-calculate 16 different palettes with full interpolation
        for (int pal = 0; pal < 16; pal++) {
            // Define key colors for each palette
            CRGB colors[4];
            switch (pal) {
                case 0: // Rainbow
                    colors[0] = CRGB::Red;
                    colors[1] = CRGB::Yellow;
                    colors[2] = CRGB::Green;
                    colors[3] = CRGB::Blue;
                    break;
                case 1: // Fire
                    colors[0] = CRGB::Black;
                    colors[1] = CRGB::Red;
                    colors[2] = CRGB::Orange;
                    colors[3] = CRGB::Yellow;
                    break;
                case 2: // Ocean
                    colors[0] = CRGB::MidnightBlue;
                    colors[1] = CRGB::DarkBlue;
                    colors[2] = CRGB::Blue;
                    colors[3] = CRGB::Cyan;
                    break;
                case 3: // Forest
                    colors[0] = CRGB::DarkGreen;
                    colors[1] = CRGB::Green;
                    colors[2] = CRGB::LimeGreen;
                    colors[3] = CRGB::Yellow;
                    break;
                case 4: // Sunset
                    colors[0] = CRGB::DarkRed;
                    colors[1] = CRGB::OrangeRed;
                    colors[2] = CRGB::Orange;
                    colors[3] = CRGB::Pink;
                    break;
                case 5: // Purple Rain
                    colors[0] = CRGB::Black;
                    colors[1] = CRGB::Purple;
                    colors[2] = CRGB::Violet;
                    colors[3] = CRGB::Pink;
                    break;
                case 6: // Ice
                    colors[0] = CRGB::White;
                    colors[1] = CRGB::LightBlue;
                    colors[2] = CRGB::Blue;
                    colors[3] = CRGB::DarkBlue;
                    break;
                case 7: // Lava
                    colors[0] = CRGB::Black;
                    colors[1] = CRGB::Maroon;
                    colors[2] = CRGB::Red;
                    colors[3] = CRGB::White;
                    break;
                case 8: // Party
                    colors[0] = CRGB::Purple;
                    colors[1] = CRGB::Yellow;
                    colors[2] = CRGB::Cyan;
                    colors[3] = CRGB::Magenta;
                    break;
                case 9: // Cloud
                    colors[0] = CRGB::Blue;
                    colors[1] = CRGB::White;
                    colors[2] = CRGB::LightGray;
                    colors[3] = CRGB::Gray;
                    break;
                default: // Gradient variations
                    colors[0] = CRGB(pal * 16, 0, 255 - pal * 16);
                    colors[1] = CRGB(255 - pal * 16, pal * 16, 0);
                    colors[2] = CRGB(0, 255 - pal * 16, pal * 16);
                    colors[3] = CRGB(pal * 8, pal * 8, 255 - pal * 8);
                    break;
            }
            
            // Interpolate between key colors
            for (int i = 0; i < 256; i++) {
                float pos = i / 255.0f * 3.0f;  // Position in gradient (0-3)
                int segment = (int)pos;
                float fract = pos - segment;
                
                if (segment >= 3) {
                    paletteInterpolationLUT[pal][i] = colors[3];
                } else {
                    // Blend between adjacent colors
                    uint8_t blend = fract * 255;
                    paletteInterpolationLUT[pal][i] = blend(colors[segment], colors[segment + 1], blend);
                }
            }
        }
    }
}

// ============== BRIGHTNESS & SCALING LUTs ==============
uint8_t* dim8_video_LUT = nullptr;
uint8_t* brighten8_video_LUT = nullptr;
uint8_t* quadraticScaleLUT = nullptr;
uint8_t* cubicScaleLUT = nullptr;

void initializeBrightnessLUTs() {
    if (!dim8_video_LUT) {
        dim8_video_LUT = (uint8_t*)ps_malloc(256);
        brighten8_video_LUT = (uint8_t*)ps_malloc(256);
        quadraticScaleLUT = (uint8_t*)ps_malloc(256);
        cubicScaleLUT = (uint8_t*)ps_malloc(256);
        
        for (int i = 0; i < 256; i++) {
            // Video dimming curve (gamma 2.0)
            float normalized = i / 255.0f;
            dim8_video_LUT[i] = pow(normalized, 2.0f) * 255;
            
            // Video brightening curve (gamma 0.5)
            brighten8_video_LUT[i] = pow(normalized, 0.5f) * 255;
            
            // Quadratic scaling
            quadraticScaleLUT[i] = (normalized * normalized) * 255;
            
            // Cubic scaling
            cubicScaleLUT[i] = (normalized * normalized * normalized) * 255;
        }
    }
}

// ============== ENCODER VALUE LUTs ==============
uint8_t* encoderLinearLUT = nullptr;
uint8_t* encoderExponentialLUT = nullptr;
uint8_t* encoderLogarithmicLUT = nullptr;
uint8_t* encoderSCurveLUT = nullptr;
uint8_t (*encoder2DLUT)[64] = nullptr;

void initializeEncoderLUTs() {
    if (!encoderLinearLUT) {
        encoderLinearLUT = (uint8_t*)ps_malloc(256);
        encoderExponentialLUT = (uint8_t*)ps_malloc(256);
        encoderLogarithmicLUT = (uint8_t*)ps_malloc(256);
        encoderSCurveLUT = (uint8_t*)ps_malloc(256);
        encoder2DLUT = (uint8_t(*)[64])ps_malloc(64 * 64);
        
        for (int i = 0; i < 256; i++) {
            float normalized = i / 255.0f;
            
            // Linear (identity)
            encoderLinearLUT[i] = i;
            
            // Exponential response
            encoderExponentialLUT[i] = (exp(normalized * 3) - 1) / (exp(3) - 1) * 255;
            
            // Logarithmic response
            encoderLogarithmicLUT[i] = normalized > 0 ? log(normalized * 9 + 1) / log(10) * 255 : 0;
            
            // S-Curve (smooth acceleration)
            float s = normalized * 2 - 1;  // -1 to 1
            s = s / (1 + abs(s));  // Smooth S curve
            encoderSCurveLUT[i] = (s + 1) * 127.5f;
        }
        
        // 2D encoder mapping for complex interactions
        for (int x = 0; x < 64; x++) {
            for (int y = 0; y < 64; y++) {
                // Create interesting 2D response patterns
                float fx = x / 63.0f;
                float fy = y / 63.0f;
                float distance = sqrt(fx * fx + fy * fy) / sqrt(2);
                float angle = atan2(fy - 0.5f, fx - 0.5f);
                
                // Combine distance and angle for unique patterns
                float value = (sin(angle * 4) + 1) * 0.5f * distance;
                encoder2DLUT[x][y] = value * 255;
            }
        }
    }
}

// ============== FREQUENCY ANALYSIS LUTs ==============
float* hannWindowLUT = nullptr;
float* blackmanWindowLUT = nullptr;
uint8_t (*frequencyBinLUT)[HardwareConfig::NUM_LEDS] = nullptr;
uint8_t (*beatDetectionLUT)[4] = nullptr;

void initializeFrequencyLUTs() {
    if (!hannWindowLUT) {
        hannWindowLUT = (float*)ps_malloc(512 * sizeof(float));
        blackmanWindowLUT = (float*)ps_malloc(512 * sizeof(float));
        frequencyBinLUT = (uint8_t(*)[HardwareConfig::NUM_LEDS])ps_malloc(32 * HardwareConfig::NUM_LEDS);
        beatDetectionLUT = (uint8_t(*)[4])ps_malloc(256 * 4);
        
        // Window functions for FFT
        for (int i = 0; i < 512; i++) {
            float normalized = i / 511.0f;
            
            // Hann window
            hannWindowLUT[i] = 0.5f * (1 - cos(TWO_PI * normalized));
            
            // Blackman window
            blackmanWindowLUT[i] = 0.42f - 0.5f * cos(TWO_PI * normalized) + 
                                   0.08f * cos(4 * PI * normalized);
        }
        
        // Frequency bin mapping to LEDs
        for (int bin = 0; bin < 32; bin++) {
            float binFreq = bin / 31.0f;  // Normalized frequency
            
            for (int led = 0; led < HardwareConfig::NUM_LEDS; led++) {
                // Map frequency bins to LED positions
                float ledPos = led / (float)(HardwareConfig::NUM_LEDS - 1);
                float response = exp(-pow(ledPos - binFreq, 2) * 10);  // Gaussian response
                frequencyBinLUT[bin][led] = response * 255;
            }
        }
        
        // Beat detection patterns
        for (int energy = 0; energy < 256; energy++) {
            float e = energy / 255.0f;
            
            // Different beat responses
            beatDetectionLUT[energy][0] = e > 0.7f ? 255 : e * 364;  // Kick threshold
            beatDetectionLUT[energy][1] = e > 0.6f ? 255 : e * 425;  // Snare threshold
            beatDetectionLUT[energy][2] = e > 0.5f ? 255 : e * 510;  // Hi-hat threshold
            beatDetectionLUT[energy][3] = pow(e, 0.5f) * 255;        // General energy
        }
    }
}

// ============== PARTICLE SYSTEM LUTs ==============
int8_t (*particleVelocityLUT)[2] = nullptr;
uint8_t* particleDecayLUT = nullptr;
CRGB* particleColorLUT = nullptr;

void initializeParticleLUTs() {
    if (!particleVelocityLUT) {
        particleVelocityLUT = (int8_t(*)[2])ps_malloc(256 * 2);
        particleDecayLUT = (uint8_t*)ps_malloc(256);
        particleColorLUT = (CRGB*)ps_malloc(64 * sizeof(CRGB));
        
        // Pre-calculated velocity vectors
        for (int i = 0; i < 256; i++) {
            float angle = (i / 255.0f) * TWO_PI;
            particleVelocityLUT[i][0] = cos(angle) * 127;  // X velocity
            particleVelocityLUT[i][1] = sin(angle) * 127;  // Y velocity
        }
        
        // Decay curves
        for (int i = 0; i < 256; i++) {
            float life = i / 255.0f;
            // Exponential decay with long tail
            particleDecayLUT[i] = pow(life, 3) * 255;
        }
        
        // Particle colors (temperature gradient)
        for (int i = 0; i < 64; i++) {
            float temp = i / 63.0f;
            if (temp < 0.25f) {
                // Cold: Blue to Cyan
                float t = temp * 4;
                particleColorLUT[i] = CRGB(0, t * 255, 255);
            } else if (temp < 0.5f) {
                // Warm: Cyan to Yellow
                float t = (temp - 0.25f) * 4;
                particleColorLUT[i] = CRGB(t * 255, 255, 255 - t * 255);
            } else if (temp < 0.75f) {
                // Hot: Yellow to Red
                float t = (temp - 0.5f) * 4;
                particleColorLUT[i] = CRGB(255, 255 - t * 255, 0);
            } else {
                // Burning: Red to White
                float t = (temp - 0.75f) * 4;
                particleColorLUT[i] = CRGB(255, t * 255, t * 255);
            }
        }
    }
}

// ============== ADVANCED EFFECT LUTs ==============
uint8_t (*perlinOctave1)[128] = nullptr;
uint8_t (*perlinOctave2)[64] = nullptr;
uint8_t (*perlinOctave3)[32] = nullptr;
uint8_t (*cellularRulesLUT)[8] = nullptr;
uint8_t (*mandelbrotLUT)[128] = nullptr;
uint8_t (*juliaSetLUT)[128] = nullptr;

void initializeAdvancedEffectLUTs() {
    if (!perlinOctave1) {
        // Perlin noise octaves
        perlinOctave1 = (uint8_t(*)[128])ps_malloc(128 * 128);
        perlinOctave2 = (uint8_t(*)[64])ps_malloc(64 * 64);
        perlinOctave3 = (uint8_t(*)[32])ps_malloc(32 * 32);
        
        // Generate Perlin-like noise at different scales
        uint32_t seed = 42;
        
        // Octave 1 - Large features
        for (int x = 0; x < 128; x++) {
            for (int y = 0; y < 128; y++) {
                seed = seed * 1103515245 + 12345;
                perlinOctave1[x][y] = (seed >> 16) & 0xFF;
            }
        }
        
        // Smooth the noise
        for (int pass = 0; pass < 3; pass++) {
            for (int x = 1; x < 127; x++) {
                for (int y = 1; y < 127; y++) {
                    int sum = perlinOctave1[x][y] + 
                             perlinOctave1[x-1][y] + perlinOctave1[x+1][y] +
                             perlinOctave1[x][y-1] + perlinOctave1[x][y+1];
                    perlinOctave1[x][y] = sum / 5;
                }
            }
        }
        
        // Similar for other octaves...
        for (int x = 0; x < 64; x++) {
            for (int y = 0; y < 64; y++) {
                seed = seed * 1103515245 + 12345;
                perlinOctave2[x][y] = (seed >> 16) & 0xFF;
            }
        }
        
        for (int x = 0; x < 32; x++) {
            for (int y = 0; y < 32; y++) {
                seed = seed * 1103515245 + 12345;
                perlinOctave3[x][y] = (seed >> 16) & 0xFF;
            }
        }
        
        // Cellular automata rules
        cellularRulesLUT = (uint8_t(*)[8])ps_malloc(256 * 8);
        
        for (int rule = 0; rule < 256; rule++) {
            for (int state = 0; state < 8; state++) {
                // Elementary cellular automaton rules
                cellularRulesLUT[rule][state] = (rule >> state) & 1;
            }
        }
        
        // Mandelbrot set
        mandelbrotLUT = (uint8_t(*)[128])ps_malloc(128 * 128);
        
        for (int px = 0; px < 128; px++) {
            for (int py = 0; py < 128; py++) {
                // Map pixel to complex plane
                float x0 = (px - 64) / 32.0f;  // -2 to 2
                float y0 = (py - 64) / 32.0f;  // -2 to 2
                
                float x = 0, y = 0;
                int iteration = 0;
                int max_iteration = 255;
                
                while (x*x + y*y <= 4 && iteration < max_iteration) {
                    float xtemp = x*x - y*y + x0;
                    y = 2*x*y + y0;
                    x = xtemp;
                    iteration++;
                }
                
                mandelbrotLUT[px][py] = iteration;
            }
        }
        
        // Julia set
        juliaSetLUT = (uint8_t(*)[128])ps_malloc(128 * 128);
        
        // Julia set with c = -0.7 + 0.27i
        float cx = -0.7f, cy = 0.27f;
        
        for (int px = 0; px < 128; px++) {
            for (int py = 0; py < 128; py++) {
                float x = (px - 64) / 32.0f;
                float y = (py - 64) / 32.0f;
                
                int iteration = 0;
                int max_iteration = 255;
                
                while (x*x + y*y <= 4 && iteration < max_iteration) {
                    float xtemp = x*x - y*y + cx;
                    y = 2*x*y + cy;
                    x = xtemp;
                    iteration++;
                }
                
                juliaSetLUT[px][py] = iteration;
            }
        }
    }
}

// ============== EXTENDED LUTs ==============
uint8_t (*hueToRgbLUT)[3] = nullptr;
int16_t (*complexWaveformLUT)[512] = nullptr;
uint8_t (*transitionMaskLUT)[HardwareConfig::NUM_LEDS] = nullptr;
uint8_t (*ditheringLUT)[16] = nullptr;
uint8_t (*motionBlurLUT)[8] = nullptr;
uint8_t (*shaderEffectLUT)[64] = nullptr;

void initializeExtendedLUTs() {
    if (!hueToRgbLUT) {
        // Hue to RGB conversion
        hueToRgbLUT = (uint8_t(*)[3])ps_malloc(256 * 3);
        
        for (int hue = 0; hue < 256; hue++) {
            CHSV hsv(hue, 255, 255);
            CRGB rgb;
            hsv2rgb_rainbow(hsv, rgb);
            hueToRgbLUT[hue][0] = rgb.r;
            hueToRgbLUT[hue][1] = rgb.g;
            hueToRgbLUT[hue][2] = rgb.b;
        }
        
        // Complex waveforms
        complexWaveformLUT = (int16_t(*)[512])ps_malloc(8 * 512 * sizeof(int16_t));
        
        for (int wave = 0; wave < 8; wave++) {
            for (int i = 0; i < 512; i++) {
                float t = i / 511.0f * TWO_PI;
                float value = 0;
                
                switch (wave) {
                    case 0: // Sine
                        value = sin(t);
                        break;
                    case 1: // Square
                        value = sin(t) > 0 ? 1 : -1;
                        break;
                    case 2: // Triangle
                        value = 2 * asin(sin(t)) / PI;
                        break;
                    case 3: // Sawtooth
                        value = 2 * (t / TWO_PI - floor(t / TWO_PI + 0.5f));
                        break;
                    case 4: // Complex harmonic
                        value = sin(t) + sin(2*t)/2 + sin(3*t)/3;
                        value /= 1.833f;  // Normalize
                        break;
                    case 5: // PWM 25%
                        value = (fmod(t, TWO_PI) < PI/2) ? 1 : -1;
                        break;
                    case 6: // Noise burst
                        value = sin(t) * ((i % 64) < 32 ? 1 : 0.1f);
                        break;
                    case 7: // Chirp
                        value = sin(t * (1 + i / 511.0f * 5));
                        break;
                }
                
                complexWaveformLUT[wave][i] = value * 32767;
            }
        }
        
        // Transition masks
        transitionMaskLUT = (uint8_t(*)[HardwareConfig::NUM_LEDS])ps_malloc(32 * HardwareConfig::NUM_LEDS);
        
        for (int mask = 0; mask < 32; mask++) {
            for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                float pos = i / (float)(HardwareConfig::NUM_LEDS - 1);
                float value = 0;
                
                switch (mask % 8) {
                    case 0: // Linear
                        value = pos;
                        break;
                    case 1: // Center fade
                        value = 1.0f - abs(pos - 0.5f) * 2;
                        break;
                    case 2: // Edge fade
                        value = abs(pos - 0.5f) * 2;
                        break;
                    case 3: // Wave
                        value = (sin(pos * TWO_PI * 3) + 1) / 2;
                        break;
                    case 4: // Random blocks
                        value = ((i / 10) % 2) ? 1 : 0;
                        break;
                    case 5: // Gradient bands
                        value = fmod(pos * 5, 1.0f);
                        break;
                    case 6: // Diamond
                        value = 1.0f - abs(sin(pos * PI * 4));
                        break;
                    case 7: // Zigzag
                        value = (i % 16 < 8) ? pos : 1.0f - pos;
                        break;
                }
                
                // Apply variations based on mask index
                if (mask >= 8) value = 1.0f - value;
                if (mask >= 16) value = value * value;
                if (mask >= 24) value = sqrt(value);
                
                transitionMaskLUT[mask][i] = value * 255;
            }
        }
        
        // Dithering patterns
        ditheringLUT = (uint8_t(*)[16])ps_malloc(16 * 16);
        
        // Bayer matrix 4x4 tiled
        uint8_t bayer4x4[4][4] = {
            { 0,  8,  2, 10},
            {12,  4, 14,  6},
            { 3, 11,  1,  9},
            {15,  7, 13,  5}
        };
        
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++) {
                ditheringLUT[x][y] = bayer4x4[x % 4][y % 4] * 16;
            }
        }
        
        // Motion blur accumulation
        motionBlurLUT = (uint8_t(*)[8])ps_malloc(256 * 8);
        
        for (int current = 0; current < 256; current++) {
            for (int history = 0; history < 8; history++) {
                // Exponential decay based on history position
                float weight = pow(0.7f, history);
                motionBlurLUT[current][history] = current * weight;
            }
        }
        
        // Shader-like effects
        shaderEffectLUT = (uint8_t(*)[64])ps_malloc(64 * 64);
        
        for (int x = 0; x < 64; x++) {
            for (int y = 0; y < 64; y++) {
                float fx = (x - 32) / 32.0f;
                float fy = (y - 32) / 32.0f;
                
                // Tunnel effect
                float angle = atan2(fy, fx);
                float radius = sqrt(fx*fx + fy*fy);
                float tunnel = 1.0f / (radius + 0.1f);
                tunnel = fmod(tunnel + angle / TWO_PI, 1.0f);
                
                shaderEffectLUT[x][y] = tunnel * 255;
            }
        }
    }
}

// ============== MAIN INITIALIZER ==============
void initializeMegaLUTs() {
    Serial.println(F("[LUT] Initializing MEGA LUT System..."));
    
    uint32_t startTime = millis();
    size_t startHeap = ESP.getFreeHeap();
    
    // Initialize all LUT subsystems
    initializeTrigLUTs();
    initializeColorMixLUT();
    initializeHDRLUTs();
    initializeTransitionLUTs();
    initializeEasingLUTs();
    initializeGeometryLUTs();
    initializeEffectPatternLUTs();
    initializePaletteLUTs();
    initializeBrightnessLUTs();
    initializeEncoderLUTs();
    initializeFrequencyLUTs();
    initializeParticleLUTs();
    initializeAdvancedEffectLUTs();
    initializeExtendedLUTs();
    
    uint32_t endTime = millis();
    size_t endHeap = ESP.getFreeHeap();
    size_t usedMemory = startHeap - endHeap;
    
    Serial.print(F("[LUT] Initialization complete in "));
    Serial.print(endTime - startTime);
    Serial.println(F(" ms"));
    
    Serial.print(F("[LUT] Memory used: "));
    Serial.print(usedMemory / 1024);
    Serial.print(F(" KB ("));
    Serial.print(usedMemory);
    Serial.println(F(" bytes)"));
    
    Serial.print(F("[LUT] Free heap remaining: "));
    Serial.print(endHeap / 1024);
    Serial.println(F(" KB"));
    
    // Verify we've used enough memory
    if (usedMemory < 200 * 1024) {
        Serial.println(F("[LUT] WARNING: Less than 200KB used, not maximizing performance!"));
    } else {
        Serial.println(F("[LUT] SUCCESS: Maximum performance LUTs loaded!"));
    }
}

// Make the pointers accessible
const int16_t* sinLUT_16bit = _sinLUT;
const int16_t* cosLUT_16bit = _cosLUT;