#ifndef BUILTIN_SHOWS_H
#define BUILTIN_SHOWS_H

#include <Arduino.h>
#include <pgmspace.h>
#include "ShowTypes.h"

// ============================================================================
// BUILTIN SHOWS - 10 Choreographed Light Show Presets
// ============================================================================
// All show data stored in PROGMEM to minimize RAM usage.
// Total flash usage: ~2KB
//
// Shows:
// 0. Dawn       - 3 min - Night sky to daylight
// 1. Storm      - 4 min - Calm to tempest to peace
// 2. Meditation - 5 min - Gentle breathing waves
// 3. Celebration- 3 min - Rhythmic energy bursts
// 4. Cosmos     - 5 min - Space journey
// 5. Forest     - 4 min - Dappled light through trees
// 6. Heartbeat  - 2 min - Rest to exertion
// 7. Ocean      - 4 min - Wave cycles
// 8. Energy     - 2 min - Rapid buildup
// 9. Ambient    - 10 min- Continuous gentle (loops)
// ============================================================================

// Helper macros for cue data initialization
// Data layout: {byte0, byte1, byte2, byte3}
// CUE_EFFECT:          {effectId, transitionType, 0, 0}
// CUE_PARAMETER_SWEEP: {paramId, targetValue, durLow, durHigh} (dur in 256ms units, max 65535ms)
// CUE_NARRATIVE:       {phase, tempoLow, tempoHigh, 0}

// Duration encoding: split 16-bit into low/high bytes
#define DUR_LO(ms) ((uint8_t)((ms) & 0xFF))
#define DUR_HI(ms) ((uint8_t)(((ms) >> 8) & 0xFF))

// ============================================================================
// SHOW 0: DAWN (3 minutes = 180,000 ms)
// Story: Night sky -> First light -> Sunrise -> Full daylight
// ============================================================================

static const char PROGMEM DAWN_ID[] = "dawn";
static const char PROGMEM DAWN_NAME[] = "Dawn";
static const char PROGMEM DAWN_CH0_NAME[] = "Night Sky";
static const char PROGMEM DAWN_CH1_NAME[] = "First Light";
static const char PROGMEM DAWN_CH2_NAME[] = "Sunrise";
static const char PROGMEM DAWN_CH3_NAME[] = "Daylight";

static const ShowCue PROGMEM DAWN_CUES[] = {
    // Chapter 0: Night Sky (0-45s) - Effect 6 (Aurora-like), low brightness
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {6, 0, 0, 0}},                    // Aurora effect
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 40, 0, 0}},    // Instant low brightness
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_REST, DUR_LO(8000), DUR_HI(8000), 0}},

    // Chapter 1: First Light (45s-90s) - Gradual brightening
    {45000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(6000), DUR_HI(6000), 0}},
    {45000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 80, DUR_LO(45000), DUR_HI(45000)}},

    // Chapter 2: Sunrise (90s-150s) - Fire effect, peak intensity
    {90000,  CUE_EFFECT,          ZONE_GLOBAL, {0, 2, 0, 0}},                    // Fire with transition
    {90000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(4000), DUR_HI(4000), 0}},
    {90000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 150, DUR_LO(30000), DUR_HI(30000)}},

    // Chapter 3: Daylight (150s-180s) - Settle to stable
    {150000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(5000), DUR_HI(5000), 0}},
    {150000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 120, DUR_LO(15000), DUR_HI(15000)}},
};

static const ShowChapter PROGMEM DAWN_CHAPTERS[] = {
    {DAWN_CH0_NAME, 0,      45000,  SHOW_PHASE_REST,    25,  0, 3},  // Night
    {DAWN_CH1_NAME, 45000,  45000,  SHOW_PHASE_BUILD,   75,  3, 2},  // First Light
    {DAWN_CH2_NAME, 90000,  60000,  SHOW_PHASE_HOLD,    200, 5, 3},  // Sunrise
    {DAWN_CH3_NAME, 150000, 30000,  SHOW_PHASE_RELEASE, 100, 8, 2},  // Daylight
};

// ============================================================================
// SHOW 1: STORM (4 minutes = 240,000 ms)
// Story: Calm -> Building -> Tempest -> Lightning -> Peace
// ============================================================================

static const char PROGMEM STORM_ID[] = "storm";
static const char PROGMEM STORM_NAME[] = "Storm";
static const char PROGMEM STORM_CH0_NAME[] = "Calm";
static const char PROGMEM STORM_CH1_NAME[] = "Building";
static const char PROGMEM STORM_CH2_NAME[] = "Tempest";
static const char PROGMEM STORM_CH3_NAME[] = "Lightning";
static const char PROGMEM STORM_CH4_NAME[] = "Peace";

static const ShowCue PROGMEM STORM_CUES[] = {
    // Calm (0-40s)
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {2, 0, 0, 0}},                    // Ocean
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 20, 0, 0}},
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_REST, DUR_LO(8000), DUR_HI(8000), 0}},

    // Building (40s-90s)
    {40000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(5000), DUR_HI(5000), 0}},
    {40000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 80, DUR_LO(50000), DUR_HI(50000)}},
    {50000,  CUE_EFFECT,          ZONE_GLOBAL, {3, 1, 0, 0}},                    // Ripple

    // Tempest (90s-150s)
    {90000,  CUE_EFFECT,          ZONE_GLOBAL, {8, 3, 0, 0}},                    // Shockwave
    {90000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(2500), DUR_HI(2500), 0}},
    {90000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 200, DUR_LO(20000), DUR_HI(20000)}},
    {120000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_INTENSITY, 255, DUR_LO(15000), DUR_HI(15000)}},

    // Lightning (150s-200s)
    {150000, CUE_EFFECT,          ZONE_GLOBAL, {9, 4, 0, 0}},                    // Collision
    {150000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(2000), DUR_HI(2000), 0}},
    {170000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 180, DUR_LO(15000), DUR_HI(15000)}},

    // Peace (200s-240s)
    {200000, CUE_EFFECT,          ZONE_GLOBAL, {2, 2, 0, 0}},                    // Back to Ocean
    {200000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(6000), DUR_HI(6000), 0}},
    {200000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 25, DUR_LO(30000), DUR_HI(30000)}},
    {200000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 100, DUR_LO(30000), DUR_HI(30000)}},
};

static const ShowChapter PROGMEM STORM_CHAPTERS[] = {
    {STORM_CH0_NAME, 0,      40000,  SHOW_PHASE_REST,    50,  0, 3},
    {STORM_CH1_NAME, 40000,  50000,  SHOW_PHASE_BUILD,   150, 3, 3},
    {STORM_CH2_NAME, 90000,  60000,  SHOW_PHASE_HOLD,    255, 6, 4},
    {STORM_CH3_NAME, 150000, 50000,  SHOW_PHASE_HOLD,    230, 10, 3},
    {STORM_CH4_NAME, 200000, 40000,  SHOW_PHASE_RELEASE, 50,  13, 4},
};

// ============================================================================
// SHOW 2: MEDITATION (5 minutes = 300,000 ms, loops)
// Story: Gentle oceanic breathing waves
// ============================================================================

static const char PROGMEM MEDITATION_ID[] = "meditation";
static const char PROGMEM MEDITATION_NAME[] = "Meditation";
static const char PROGMEM MEDITATION_CH0_NAME[] = "Breathe In";
static const char PROGMEM MEDITATION_CH1_NAME[] = "Hold";
static const char PROGMEM MEDITATION_CH2_NAME[] = "Breathe Out";

static const ShowCue PROGMEM MEDITATION_CUES[] = {
    // Initial setup
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {20, 0, 0, 0}},                   // Benard Convection
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 15, 0, 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 80, 0, 0}},

    // Breathe In (0-100s)
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(12000), DUR_HI(12000), 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 140, DUR_LO(60000), DUR_HI(60000)}},

    // Hold (100s-150s)
    {100000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(10000), DUR_HI(10000), 0}},

    // Breathe Out (150s-300s)
    {150000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(15000), DUR_HI(15000), 0}},
    {150000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 60, DUR_LO(60000), DUR_HI(60000)}},
};

static const ShowChapter PROGMEM MEDITATION_CHAPTERS[] = {
    {MEDITATION_CH0_NAME, 0,      100000, SHOW_PHASE_BUILD,   75,  0, 5},
    {MEDITATION_CH1_NAME, 100000, 50000,  SHOW_PHASE_HOLD,    125, 5, 1},
    {MEDITATION_CH2_NAME, 150000, 150000, SHOW_PHASE_RELEASE, 75,  6, 2},
};

// ============================================================================
// SHOW 3: CELEBRATION (3 minutes = 180,000 ms)
// Story: Rhythmic party energy building to climax
// ============================================================================

static const char PROGMEM CELEBRATION_ID[] = "celebration";
static const char PROGMEM CELEBRATION_NAME[] = "Celebration";
static const char PROGMEM CELEBRATION_CH0_NAME[] = "Intro";
static const char PROGMEM CELEBRATION_CH1_NAME[] = "Build";
static const char PROGMEM CELEBRATION_CH2_NAME[] = "Peak";
static const char PROGMEM CELEBRATION_CH3_NAME[] = "Outro";

static const ShowCue PROGMEM CELEBRATION_CUES[] = {
    // Intro (0-30s)
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {4, 0, 0, 0}},                    // Confetti
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(4000), DUR_HI(4000), 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 60, 0, 0}},

    // Build (30s-90s)
    {30000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(3000), DUR_HI(3000), 0}},
    {30000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 120, DUR_LO(60000), DUR_HI(60000)}},
    {50000,  CUE_EFFECT,          ZONE_GLOBAL, {7, 2, 0, 0}},                    // BPM

    // Peak (90s-150s)
    {90000,  CUE_EFFECT,          ZONE_GLOBAL, {9, 3, 0, 0}},                    // Collision
    {90000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(2000), DUR_HI(2000), 0}},
    {90000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 200, 0, 0}},
    {120000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_INTENSITY, 255, DUR_LO(15000), DUR_HI(15000)}},

    // Outro (150s-180s)
    {150000, CUE_EFFECT,          ZONE_GLOBAL, {5, 1, 0, 0}},                    // Juggle
    {150000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(4000), DUR_HI(4000), 0}},
    {150000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 50, DUR_LO(30000), DUR_HI(30000)}},
};

static const ShowChapter PROGMEM CELEBRATION_CHAPTERS[] = {
    {CELEBRATION_CH0_NAME, 0,      30000,  SHOW_PHASE_BUILD,   100, 0, 3},
    {CELEBRATION_CH1_NAME, 30000,  60000,  SHOW_PHASE_BUILD,   175, 3, 3},
    {CELEBRATION_CH2_NAME, 90000,  60000,  SHOW_PHASE_HOLD,    255, 6, 4},
    {CELEBRATION_CH3_NAME, 150000, 30000,  SHOW_PHASE_RELEASE, 125, 10, 3},
};

// ============================================================================
// SHOW 4: COSMOS (5 minutes = 300,000 ms)
// Story: Space journey - Stars -> Nebula -> Collision
// ============================================================================

static const char PROGMEM COSMOS_ID[] = "cosmos";
static const char PROGMEM COSMOS_NAME[] = "Cosmos";
static const char PROGMEM COSMOS_CH0_NAME[] = "Stars";
static const char PROGMEM COSMOS_CH1_NAME[] = "Drift";
static const char PROGMEM COSMOS_CH2_NAME[] = "Nebula";
static const char PROGMEM COSMOS_CH3_NAME[] = "Collision";
static const char PROGMEM COSMOS_CH4_NAME[] = "Aftermath";

static const ShowCue PROGMEM COSMOS_CUES[] = {
    // Stars (0-60s)
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {25, 0, 0, 0}},                   // Mandelbrot Zoom
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_REST, DUR_LO(10000), DUR_HI(10000), 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 60, 0, 0}},

    // Drift (60s-110s)
    {60000,  CUE_EFFECT,          ZONE_GLOBAL, {24, 1, 0, 0}},                   // Strange Attractor
    {60000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(8000), DUR_HI(8000), 0}},
    {60000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 100, DUR_LO(50000), DUR_HI(50000)}},

    // Nebula (110s-180s)
    {110000, CUE_EFFECT,          ZONE_GLOBAL, {22, 2, 0, 0}},                   // Plasma Pinch
    {110000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(5000), DUR_HI(5000), 0}},
    {140000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_INTENSITY, 200, DUR_LO(40000), DUR_HI(40000)}},

    // Collision (180s-240s)
    {180000, CUE_EFFECT,          ZONE_GLOBAL, {9, 4, 0, 0}},                    // Collision effect
    {180000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(2500), DUR_HI(2500), 0}},
    {180000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 220, DUR_LO(10000), DUR_HI(10000)}},

    // Aftermath (240s-300s)
    {240000, CUE_EFFECT,          ZONE_GLOBAL, {26, 2, 0, 0}},                   // Kuramoto Oscillators
    {240000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(7000), DUR_HI(7000), 0}},
    {240000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 80, DUR_LO(60000), DUR_HI(60000)}},
};

static const ShowChapter PROGMEM COSMOS_CHAPTERS[] = {
    {COSMOS_CH0_NAME, 0,      60000,  SHOW_PHASE_REST,    50,  0, 3},
    {COSMOS_CH1_NAME, 60000,  50000,  SHOW_PHASE_BUILD,   100, 3, 3},
    {COSMOS_CH2_NAME, 110000, 70000,  SHOW_PHASE_BUILD,   150, 6, 3},
    {COSMOS_CH3_NAME, 180000, 60000,  SHOW_PHASE_HOLD,    255, 9, 3},
    {COSMOS_CH4_NAME, 240000, 60000,  SHOW_PHASE_RELEASE, 75,  12, 3},
};

// ============================================================================
// SHOW 5: FOREST (4 minutes = 240,000 ms)
// Story: Dappled sunlight -> Wind -> Dusk
// ============================================================================

static const char PROGMEM FOREST_ID[] = "forest";
static const char PROGMEM FOREST_NAME[] = "Forest";
static const char PROGMEM FOREST_CH0_NAME[] = "Morning";
static const char PROGMEM FOREST_CH1_NAME[] = "Wind";
static const char PROGMEM FOREST_CH2_NAME[] = "Dusk";

static const ShowCue PROGMEM FOREST_CUES[] = {
    // Morning (0-80s)
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {20, 0, 0, 0}},                   // Benard Convection
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(8000), DUR_HI(8000), 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 100, 0, 0}},

    // Wind (80s-160s)
    {80000,  CUE_EFFECT,          ZONE_GLOBAL, {23, 2, 0, 0}},                   // KH Enhanced
    {80000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(5000), DUR_HI(5000), 0}},
    {80000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 80, DUR_LO(40000), DUR_HI(40000)}},
    {120000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 50, DUR_LO(40000), DUR_HI(40000)}},

    // Dusk (160s-240s)
    {160000, CUE_EFFECT,          ZONE_GLOBAL, {0, 1, 0, 0}},                    // Fire (low)
    {160000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(10000), DUR_HI(10000), 0}},
    {160000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 60, DUR_LO(60000), DUR_HI(60000)}},
};

static const ShowChapter PROGMEM FOREST_CHAPTERS[] = {
    {FOREST_CH0_NAME, 0,      80000,  SHOW_PHASE_BUILD,   75,  0, 3},
    {FOREST_CH1_NAME, 80000,  80000,  SHOW_PHASE_HOLD,    150, 3, 4},
    {FOREST_CH2_NAME, 160000, 80000,  SHOW_PHASE_RELEASE, 100, 7, 3},
};

// ============================================================================
// SHOW 6: HEARTBEAT (2 minutes = 120,000 ms)
// Story: Rest -> Exertion -> Recovery
// ============================================================================

static const char PROGMEM HEARTBEAT_ID[] = "heartbeat";
static const char PROGMEM HEARTBEAT_NAME[] = "Heartbeat";
static const char PROGMEM HEARTBEAT_CH0_NAME[] = "Rest";
static const char PROGMEM HEARTBEAT_CH1_NAME[] = "Exertion";
static const char PROGMEM HEARTBEAT_CH2_NAME[] = "Recovery";

static const ShowCue PROGMEM HEARTBEAT_CUES[] = {
    // Rest (0-30s)
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {1, 0, 0, 0}},                    // Pulse
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_REST, DUR_LO(6000), DUR_HI(6000), 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 30, 0, 0}},

    // Exertion (30s-90s)
    {30000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(1500), DUR_HI(1500), 0}},
    {30000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 180, DUR_LO(30000), DUR_HI(30000)}},
    {30000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 180, DUR_LO(20000), DUR_HI(20000)}},
    {60000,  CUE_EFFECT,          ZONE_GLOBAL, {7, 0, 0, 0}},                    // BPM

    // Recovery (90s-120s)
    {90000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(4000), DUR_HI(4000), 0}},
    {90000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 40, DUR_LO(30000), DUR_HI(30000)}},
    {90000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 90, DUR_LO(30000), DUR_HI(30000)}},
};

static const ShowChapter PROGMEM HEARTBEAT_CHAPTERS[] = {
    {HEARTBEAT_CH0_NAME, 0,     30000,  SHOW_PHASE_REST,    50,  0, 3},
    {HEARTBEAT_CH1_NAME, 30000, 60000,  SHOW_PHASE_HOLD,    230, 3, 4},
    {HEARTBEAT_CH2_NAME, 90000, 30000,  SHOW_PHASE_RELEASE, 75,  7, 3},
};

// ============================================================================
// SHOW 7: OCEAN (4 minutes = 240,000 ms)
// Story: Gentle waves -> Swell -> Crash -> Retreat
// ============================================================================

static const char PROGMEM OCEAN_ID[] = "ocean";
static const char PROGMEM OCEAN_NAME[] = "Ocean";
static const char PROGMEM OCEAN_CH0_NAME[] = "Gentle";
static const char PROGMEM OCEAN_CH1_NAME[] = "Swell";
static const char PROGMEM OCEAN_CH2_NAME[] = "Crash";
static const char PROGMEM OCEAN_CH3_NAME[] = "Retreat";

static const ShowCue PROGMEM OCEAN_CUES[] = {
    // Gentle (0-50s)
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {2, 0, 0, 0}},                    // Ocean
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_REST, DUR_LO(8000), DUR_HI(8000), 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 30, 0, 0}},

    // Swell (50s-110s)
    {50000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(5000), DUR_HI(5000), 0}},
    {50000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 80, DUR_LO(60000), DUR_HI(60000)}},
    {80000,  CUE_EFFECT,          ZONE_GLOBAL, {3, 1, 0, 0}},                    // Ripple

    // Crash (110s-180s)
    {110000, CUE_EFFECT,          ZONE_GLOBAL, {21, 3, 0, 0}},                   // Rayleigh-Taylor
    {110000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(2500), DUR_HI(2500), 0}},
    {110000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 200, DUR_LO(20000), DUR_HI(20000)}},
    {150000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_INTENSITY, 220, DUR_LO(30000), DUR_HI(30000)}},

    // Retreat (180s-240s)
    {180000, CUE_EFFECT,          ZONE_GLOBAL, {2, 2, 0, 0}},                    // Back to Ocean
    {180000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(7000), DUR_HI(7000), 0}},
    {180000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 25, DUR_LO(60000), DUR_HI(60000)}},
    {180000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 100, DUR_LO(60000), DUR_HI(60000)}},
};

static const ShowChapter PROGMEM OCEAN_CHAPTERS[] = {
    {OCEAN_CH0_NAME, 0,      50000,  SHOW_PHASE_REST,    75,  0, 3},
    {OCEAN_CH1_NAME, 50000,  60000,  SHOW_PHASE_BUILD,   150, 3, 3},
    {OCEAN_CH2_NAME, 110000, 70000,  SHOW_PHASE_HOLD,    255, 6, 4},
    {OCEAN_CH3_NAME, 180000, 60000,  SHOW_PHASE_RELEASE, 100, 10, 4},
};

// ============================================================================
// SHOW 8: ENERGY (2 minutes = 120,000 ms)
// Story: Rapid buildup to explosion then dissipate
// ============================================================================

static const char PROGMEM ENERGY_ID[] = "energy";
static const char PROGMEM ENERGY_NAME[] = "Energy";
static const char PROGMEM ENERGY_CH0_NAME[] = "Build";
static const char PROGMEM ENERGY_CH1_NAME[] = "Explode";
static const char PROGMEM ENERGY_CH2_NAME[] = "Fade";

static const ShowCue PROGMEM ENERGY_CUES[] = {
    // Build (0-40s)
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {19, 0, 0, 0}},                   // Gray-Scott
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(3000), DUR_HI(3000), 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 80, 0, 0}},
    {10000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 150, DUR_LO(30000), DUR_HI(30000)}},

    // Explode (40s-80s)
    {40000,  CUE_EFFECT,          ZONE_GLOBAL, {21, 4, 0, 0}},                   // Magnetic Reconnection
    {40000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(1800), DUR_HI(1800), 0}},
    {40000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 255, DUR_LO(5000), DUR_HI(5000)}},

    // Fade (80s-120s)
    {80000,  CUE_EFFECT,          ZONE_GLOBAL, {26, 2, 0, 0}},                   // Kuramoto
    {80000,  CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(5000), DUR_HI(5000), 0}},
    {80000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 50, DUR_LO(40000), DUR_HI(40000)}},
    {80000,  CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 30, DUR_LO(40000), DUR_HI(40000)}},
};

static const ShowChapter PROGMEM ENERGY_CHAPTERS[] = {
    {ENERGY_CH0_NAME, 0,     40000,  SHOW_PHASE_BUILD,   100, 0, 4},
    {ENERGY_CH1_NAME, 40000, 40000,  SHOW_PHASE_HOLD,    255, 4, 3},
    {ENERGY_CH2_NAME, 80000, 40000,  SHOW_PHASE_RELEASE, 50,  7, 4},
};

// ============================================================================
// SHOW 9: AMBIENT (10 minutes = 600,000 ms, loops)
// Story: Continuous gentle background evolution
// ============================================================================

static const char PROGMEM AMBIENT_ID[] = "ambient";
static const char PROGMEM AMBIENT_NAME[] = "Ambient";
static const char PROGMEM AMBIENT_CH0_NAME[] = "Phase A";
static const char PROGMEM AMBIENT_CH1_NAME[] = "Phase B";
static const char PROGMEM AMBIENT_CH2_NAME[] = "Phase C";
static const char PROGMEM AMBIENT_CH3_NAME[] = "Phase D";

static const ShowCue PROGMEM AMBIENT_CUES[] = {
    // Phase A (0-150s)
    {0,      CUE_EFFECT,          ZONE_GLOBAL, {20, 0, 0, 0}},                   // Benard
    {0,      CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_REST, DUR_LO(15000), DUR_HI(15000), 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 70, 0, 0}},
    {0,      CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_SPEED, 20, 0, 0}},

    // Phase B (150s-300s)
    {150000, CUE_EFFECT,          ZONE_GLOBAL, {24, 1, 0, 0}},                   // Strange Attractor
    {150000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_BUILD, DUR_LO(12000), DUR_HI(12000), 0}},
    {150000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 90, DUR_LO(60000), DUR_HI(60000)}},

    // Phase C (300s-450s)
    {300000, CUE_EFFECT,          ZONE_GLOBAL, {26, 1, 0, 0}},                   // Kuramoto
    {300000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_HOLD, DUR_LO(12000), DUR_HI(12000), 0}},
    {300000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 80, DUR_LO(60000), DUR_HI(60000)}},

    // Phase D (450s-600s)
    {450000, CUE_EFFECT,          ZONE_GLOBAL, {2, 1, 0, 0}},                    // Ocean
    {450000, CUE_NARRATIVE,       ZONE_GLOBAL, {SHOW_PHASE_RELEASE, DUR_LO(15000), DUR_HI(15000), 0}},
    {450000, CUE_PARAMETER_SWEEP, ZONE_GLOBAL, {PARAM_BRIGHTNESS, 60, DUR_LO(60000), DUR_HI(60000)}},
};

static const ShowChapter PROGMEM AMBIENT_CHAPTERS[] = {
    {AMBIENT_CH0_NAME, 0,      150000, SHOW_PHASE_REST,    50,  0, 4},
    {AMBIENT_CH1_NAME, 150000, 150000, SHOW_PHASE_BUILD,   75,  4, 3},
    {AMBIENT_CH2_NAME, 300000, 150000, SHOW_PHASE_HOLD,    65,  7, 3},
    {AMBIENT_CH3_NAME, 450000, 150000, SHOW_PHASE_RELEASE, 50,  10, 3},
};

// ============================================================================
// MASTER SHOW ARRAY
// ============================================================================

static const ShowDefinition PROGMEM BUILTIN_SHOWS[] = {
    // Show 0: Dawn
    {DAWN_ID, DAWN_NAME, 180000, 4, 10, false, DAWN_CHAPTERS, DAWN_CUES},

    // Show 1: Storm
    {STORM_ID, STORM_NAME, 240000, 5, 17, false, STORM_CHAPTERS, STORM_CUES},

    // Show 2: Meditation
    {MEDITATION_ID, MEDITATION_NAME, 300000, 3, 8, true, MEDITATION_CHAPTERS, MEDITATION_CUES},

    // Show 3: Celebration
    {CELEBRATION_ID, CELEBRATION_NAME, 180000, 4, 13, false, CELEBRATION_CHAPTERS, CELEBRATION_CUES},

    // Show 4: Cosmos
    {COSMOS_ID, COSMOS_NAME, 300000, 5, 15, false, COSMOS_CHAPTERS, COSMOS_CUES},

    // Show 5: Forest
    {FOREST_ID, FOREST_NAME, 240000, 3, 10, false, FOREST_CHAPTERS, FOREST_CUES},

    // Show 6: Heartbeat
    {HEARTBEAT_ID, HEARTBEAT_NAME, 120000, 3, 10, false, HEARTBEAT_CHAPTERS, HEARTBEAT_CUES},

    // Show 7: Ocean
    {OCEAN_ID, OCEAN_NAME, 240000, 4, 14, false, OCEAN_CHAPTERS, OCEAN_CUES},

    // Show 8: Energy
    {ENERGY_ID, ENERGY_NAME, 120000, 3, 11, false, ENERGY_CHAPTERS, ENERGY_CUES},

    // Show 9: Ambient
    {AMBIENT_ID, AMBIENT_NAME, 600000, 4, 13, true, AMBIENT_CHAPTERS, AMBIENT_CUES},
};

static constexpr uint8_t BUILTIN_SHOW_COUNT = sizeof(BUILTIN_SHOWS) / sizeof(BUILTIN_SHOWS[0]);

#endif // BUILTIN_SHOWS_H
