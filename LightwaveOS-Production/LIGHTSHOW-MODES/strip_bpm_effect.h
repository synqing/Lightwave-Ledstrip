#ifndef STRIP_BPM_EFFECT_H
#define STRIP_BPM_EFFECT_H

#include "../src/globals.h"
#include "../src/constants.h"
#include "../src/led_utilities.h"
#include "../src/Palettes.h"
#include "../src/GDFT.h"

// Strip BPM effect - beat-synchronized patterns with audio reactivity
// Creates pulsing waves that sync to detected beats and bass energy
void light_mode_strip_bpm();

#endif // STRIP_BPM_EFFECT_H