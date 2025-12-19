/*
------------------------------------
            _      _   _                _
           | |    | | | |              | |
 _ __   __ _| | ___| |_| |_ ___  ___   | |__
| '_ \ / _` | |/ _ \ __| __/ _ \/ __|  | '_ \
| |_) | (_| | |  __/ |_| ||  __/\__ \  | | | |
| .__/ \__,_|_|\___|\__|\__\___||___/  |_| |_|
| |
|_|

33 curated gradient palettes ported from Lightwave.
Each palette is stored as PROGMEM arrays of keyframe data.
Format: {position_0_255, R, G, B, position, R, G, B, ...}
*/

#pragma once
#include "types.h"

// ============================================================================
// PALETTE DATA - 33 gradient palettes from cpt-city collection
// ============================================================================

// Palette 0: Sunset Real
const uint8_t palette_sunset_real[] PROGMEM = {
	0, 120, 0, 0,
	22, 179, 22, 0,
	51, 255, 104, 0,
	85, 167, 22, 18,
	135, 100, 0, 103,
	198, 16, 0, 130,
	255, 0, 0, 160
};

// Palette 1: Rivendell
const uint8_t palette_rivendell[] PROGMEM = {
	0, 1, 14, 5,
	101, 16, 36, 14,
	165, 56, 68, 30,
	242, 150, 156, 99,
	255, 150, 156, 99
};

// Palette 2: Ocean Breeze 036
const uint8_t palette_ocean_breeze_036[] PROGMEM = {
	0, 1, 6, 7,
	89, 1, 99, 111,
	153, 144, 209, 255,
	255, 0, 73, 82
};

// Palette 3: RGI 15
const uint8_t palette_rgi_15[] PROGMEM = {
	0, 4, 1, 31,
	31, 55, 1, 16,
	63, 197, 3, 7,
	95, 59, 2, 17,
	127, 6, 2, 34,
	159, 39, 6, 33,
	191, 112, 13, 32,
	223, 56, 9, 35,
	255, 22, 6, 38
};

// Palette 4: Retro 2
const uint8_t palette_retro2[] PROGMEM = {
	0, 188, 135, 1,
	255, 46, 7, 1
};

// Palette 5: Analogous 1
const uint8_t palette_analogous_1[] PROGMEM = {
	0, 3, 0, 255,
	63, 23, 0, 255,
	127, 67, 0, 255,
	191, 142, 0, 45,
	255, 255, 0, 0
};

// Palette 6: Pink Splash 08
const uint8_t palette_pinksplash_08[] PROGMEM = {
	0, 126, 11, 255,
	127, 197, 1, 22,
	175, 210, 157, 172,
	221, 157, 3, 112,
	255, 157, 3, 112
};

// Palette 7: Coral Reef
const uint8_t palette_coral_reef[] PROGMEM = {
	0, 40, 199, 197,
	50, 10, 152, 155,
	96, 1, 111, 120,
	96, 43, 127, 162,
	139, 10, 73, 111,
	255, 1, 34, 71
};

// Palette 8: Ocean Breeze 068
const uint8_t palette_ocean_breeze_068[] PROGMEM = {
	0, 100, 156, 153,
	51, 1, 99, 137,
	101, 1, 68, 84,
	104, 35, 142, 168,
	178, 0, 63, 117,
	255, 1, 10, 10
};

// Palette 9: Pink Splash 07
const uint8_t palette_pinksplash_07[] PROGMEM = {
	0, 229, 1, 1,
	61, 242, 4, 63,
	101, 255, 12, 255,
	127, 249, 81, 252,
	153, 255, 11, 235,
	193, 244, 5, 68,
	255, 232, 1, 5
};

// Palette 10: Vintage 01
const uint8_t palette_vintage_01[] PROGMEM = {
	0, 4, 1, 1,
	51, 16, 0, 1,
	76, 97, 104, 3,
	101, 255, 131, 19,
	127, 67, 9, 4,
	153, 16, 0, 1,
	229, 4, 1, 1,
	255, 4, 1, 1
};

// Palette 11: Departure
const uint8_t palette_departure[] PROGMEM = {
	0, 8, 3, 0,
	42, 23, 7, 0,
	63, 75, 38, 6,
	84, 169, 99, 38,
	106, 213, 169, 119,
	116, 255, 255, 255,
	138, 135, 255, 138,
	148, 22, 255, 24,
	170, 0, 255, 0,
	191, 0, 136, 0,
	212, 0, 55, 0,
	255, 0, 55, 0
};

// Palette 12: Landscape 64
const uint8_t palette_landscape_64[] PROGMEM = {
	0, 0, 0, 0,
	37, 2, 25, 1,
	76, 15, 115, 5,
	127, 79, 213, 1,
	128, 126, 211, 47,
	130, 188, 209, 247,
	153, 144, 182, 205,
	204, 59, 117, 250,
	255, 1, 37, 192
};

// Palette 13: Landscape 33
const uint8_t palette_landscape_33[] PROGMEM = {
	0, 1, 5, 0,
	19, 32, 23, 1,
	38, 161, 55, 1,
	63, 229, 144, 1,
	66, 39, 142, 74,
	255, 1, 4, 1
};

// Palette 14: Rainbow Sherbet
const uint8_t palette_rainbowsherbet[] PROGMEM = {
	0, 255, 33, 4,
	43, 255, 68, 25,
	86, 255, 7, 25,
	127, 255, 82, 103,
	170, 255, 255, 242,
	209, 42, 255, 22,
	255, 87, 255, 65
};

// Palette 15: GR65 Hult
const uint8_t palette_gr65_hult[] PROGMEM = {
	0, 247, 176, 247,
	48, 255, 136, 255,
	89, 220, 29, 226,
	160, 7, 82, 178,
	216, 1, 124, 109,
	255, 1, 124, 109
};

// Palette 16: GR64 Hult
const uint8_t palette_gr64_hult[] PROGMEM = {
	0, 1, 124, 109,
	66, 1, 93, 79,
	104, 52, 65, 1,
	130, 115, 127, 1,
	150, 52, 65, 1,
	201, 1, 86, 72,
	239, 0, 55, 45,
	255, 0, 55, 45
};

// Palette 17: GMT Dry Wet
const uint8_t palette_gmt_drywet[] PROGMEM = {
	0, 47, 30, 2,
	42, 213, 147, 24,
	84, 103, 219, 52,
	127, 3, 219, 207,
	170, 1, 48, 214,
	212, 1, 1, 111,
	255, 1, 7, 33
};

// Palette 18: IB Jul01
const uint8_t palette_ib_jul01[] PROGMEM = {
	0, 194, 1, 1,
	94, 1, 29, 18,
	132, 57, 131, 28,
	255, 113, 1, 1
};

// Palette 19: Vintage 57
const uint8_t palette_vintage_57[] PROGMEM = {
	0, 2, 1, 1,
	53, 18, 1, 0,
	104, 69, 29, 1,
	153, 167, 135, 10,
	255, 46, 56, 4
};

// Palette 20: IB15
const uint8_t palette_ib15[] PROGMEM = {
	0, 113, 91, 147,
	72, 157, 88, 78,
	89, 208, 85, 33,
	107, 255, 29, 11,
	141, 137, 31, 39,
	255, 59, 33, 89
};

// Palette 21: Fuschia 7
const uint8_t palette_fuschia_7[] PROGMEM = {
	0, 43, 3, 153,
	63, 100, 4, 103,
	127, 188, 5, 66,
	191, 161, 11, 115,
	255, 135, 20, 182
};

// Palette 22: Emerald Dragon
const uint8_t palette_emerald_dragon[] PROGMEM = {
	0, 97, 255, 1,
	101, 47, 133, 1,
	178, 13, 43, 1,
	255, 2, 10, 1
};

// Palette 23: Lava
const uint8_t palette_lava[] PROGMEM = {
	0, 0, 0, 0,
	46, 18, 0, 0,
	96, 113, 0, 0,
	108, 142, 3, 1,
	119, 175, 17, 1,
	146, 213, 44, 2,
	174, 255, 82, 4,
	188, 255, 115, 4,
	202, 255, 156, 4,
	218, 255, 203, 4,
	234, 255, 255, 4,
	244, 255, 255, 71,
	255, 255, 255, 255
};

// Palette 24: Fire
const uint8_t palette_fire[] PROGMEM = {
	0, 1, 1, 0,
	76, 32, 5, 0,
	146, 192, 24, 0,
	197, 220, 105, 5,
	240, 252, 255, 31,
	250, 252, 255, 111,
	255, 255, 255, 255
};

// Palette 25: Colorful
const uint8_t palette_colorful[] PROGMEM = {
	0, 10, 85, 5,
	25, 29, 109, 18,
	60, 59, 138, 42,
	93, 83, 99, 52,
	106, 110, 66, 64,
	109, 123, 49, 65,
	113, 139, 35, 66,
	116, 192, 117, 98,
	124, 255, 255, 137,
	168, 100, 180, 155,
	255, 22, 121, 174
};

// Palette 26: Magenta Evening
const uint8_t palette_magenta_evening[] PROGMEM = {
	0, 71, 27, 39,
	31, 130, 11, 51,
	63, 213, 2, 64,
	70, 232, 1, 66,
	76, 252, 1, 69,
	108, 123, 2, 51,
	255, 46, 9, 35
};

// Palette 27: Pink Purple
const uint8_t palette_pink_purple[] PROGMEM = {
	0, 19, 2, 39,
	25, 26, 4, 45,
	51, 33, 6, 52,
	76, 68, 62, 125,
	102, 118, 187, 240,
	109, 163, 215, 247,
	114, 217, 244, 255,
	122, 159, 149, 221,
	149, 113, 78, 188,
	183, 128, 57, 155,
	255, 146, 40, 123
};

// Palette 28: Autumn 19
const uint8_t palette_autumn_19[] PROGMEM = {
	0, 26, 1, 1,
	51, 67, 4, 1,
	84, 118, 14, 1,
	104, 137, 152, 52,
	112, 113, 65, 1,
	122, 133, 149, 59,
	124, 137, 152, 52,
	135, 113, 65, 1,
	142, 139, 154, 46,
	163, 113, 13, 1,
	204, 55, 3, 1,
	249, 17, 1, 1,
	255, 17, 1, 1
};

// Palette 29: Blue Magenta White
const uint8_t palette_blue_magenta_white[] PROGMEM = {
	0, 0, 0, 0,
	42, 0, 0, 45,
	84, 0, 0, 255,
	127, 42, 0, 255,
	170, 255, 0, 255,
	212, 255, 55, 255,
	255, 255, 255, 255
};

// Palette 30: Black Magenta Red
const uint8_t palette_black_magenta_red[] PROGMEM = {
	0, 0, 0, 0,
	63, 42, 0, 45,
	127, 255, 0, 255,
	191, 255, 0, 45,
	255, 255, 0, 0
};

// Palette 31: Red Magenta Yellow
const uint8_t palette_red_magenta_yellow[] PROGMEM = {
	0, 0, 0, 0,
	42, 42, 0, 0,
	84, 255, 0, 0,
	127, 255, 0, 45,
	170, 255, 0, 255,
	212, 255, 55, 45,
	255, 255, 255, 0
};

// Palette 32: Blue Cyan Yellow
const uint8_t palette_blue_cyan_yellow[] PROGMEM = {
	0, 0, 0, 255,
	63, 0, 55, 255,
	127, 0, 255, 255,
	191, 42, 255, 45,
	255, 255, 255, 0
};

// ============================================================================
// PALETTE METADATA
// ============================================================================

// Palette names for display
const char* const palette_names[] = {
	"Sunset Real",
	"Rivendell",
	"Ocean Breeze 036",
	"RGI 15",
	"Retro 2",
	"Analogous 1",
	"Pink Splash 08",
	"Coral Reef",
	"Ocean Breeze 068",
	"Pink Splash 07",
	"Vintage 01",
	"Departure",
	"Landscape 64",
	"Landscape 33",
	"Rainbow Sherbet",
	"GR65 Hult",
	"GR64 Hult",
	"GMT Dry Wet",
	"IB Jul01",
	"Vintage 57",
	"IB15",
	"Fuschia 7",
	"Emerald Dragon",
	"Lava",
	"Fire",
	"Colorful",
	"Magenta Evening",
	"Pink Purple",
	"Autumn 19",
	"Blue Magenta White",
	"Black Magenta Red",
	"Red Magenta Yellow",
	"Blue Cyan Yellow"
};

const uint8_t NUM_PALETTES = 33;

// Palette lookup table (pointers to each palette + entry count)
struct PaletteInfo {
	const uint8_t* data;
	uint8_t num_entries;  // Number of keyframes (position + RGB = 4 bytes per entry)
};

const PaletteInfo palette_table[] PROGMEM = {
	{palette_sunset_real, 7},
	{palette_rivendell, 5},
	{palette_ocean_breeze_036, 4},
	{palette_rgi_15, 9},
	{palette_retro2, 2},
	{palette_analogous_1, 5},
	{palette_pinksplash_08, 5},
	{palette_coral_reef, 6},
	{palette_ocean_breeze_068, 6},
	{palette_pinksplash_07, 7},
	{palette_vintage_01, 8},
	{palette_departure, 12},
	{palette_landscape_64, 9},
	{palette_landscape_33, 6},
	{palette_rainbowsherbet, 7},
	{palette_gr65_hult, 6},
	{palette_gr64_hult, 8},
	{palette_gmt_drywet, 7},
	{palette_ib_jul01, 4},
	{palette_vintage_57, 5},
	{palette_ib15, 6},
	{palette_fuschia_7, 5},
	{palette_emerald_dragon, 4},
	{palette_lava, 13},
	{palette_fire, 7},
	{palette_colorful, 11},
	{palette_magenta_evening, 7},
	{palette_pink_purple, 11},
	{palette_autumn_19, 13},
	{palette_blue_magenta_white, 7},
	{palette_black_magenta_red, 5},
	{palette_red_magenta_yellow, 7},
	{palette_blue_cyan_yellow, 5}
};

// ============================================================================
// COLOR FROM PALETTE - Replaces hsv() function
// ============================================================================

CRGBF color_from_palette(uint8_t palette_index, float progress, float brightness) {
	// Clamp inputs
	palette_index = palette_index % NUM_PALETTES;
	progress = fmodf(progress, 1.0f);
	if (progress < 0.0f) progress += 1.0f;

	// Convert progress to 0-255 range
	uint8_t pos = (uint8_t)(progress * 255.0f);

	// Get palette info
	PaletteInfo info;
	memcpy_P(&info, &palette_table[palette_index], sizeof(PaletteInfo));

	// Find bracketing keyframes
	uint8_t entry1_idx = 0, entry2_idx = 0;
	uint8_t pos1 = 0, pos2 = 255;

	// Read all entries and find the right interpolation range
	for (uint8_t i = 0; i < info.num_entries - 1; i++) {
		uint8_t p1 = pgm_read_byte(&info.data[i * 4 + 0]);
		uint8_t p2 = pgm_read_byte(&info.data[(i + 1) * 4 + 0]);

		if (pos >= p1 && pos <= p2) {
			entry1_idx = i;
			entry2_idx = i + 1;
			pos1 = p1;
			pos2 = p2;
			break;
		}
	}

	// Read keyframe RGB data
	uint8_t r1 = pgm_read_byte(&info.data[entry1_idx * 4 + 1]);
	uint8_t g1 = pgm_read_byte(&info.data[entry1_idx * 4 + 2]);
	uint8_t b1 = pgm_read_byte(&info.data[entry1_idx * 4 + 3]);

	uint8_t r2 = pgm_read_byte(&info.data[entry2_idx * 4 + 1]);
	uint8_t g2 = pgm_read_byte(&info.data[entry2_idx * 4 + 2]);
	uint8_t b2 = pgm_read_byte(&info.data[entry2_idx * 4 + 3]);

	// Interpolate between keyframes
	float blend = 0.0f;
	if (pos2 > pos1) {
		blend = (float)(pos - pos1) / (float)(pos2 - pos1);
	}

	float r = (r1 * (1.0f - blend) + r2 * blend) / 255.0f;
	float g = (g1 * (1.0f - blend) + g2 * blend) / 255.0f;
	float b = (b1 * (1.0f - blend) + b2 * blend) / 255.0f;

	// Apply brightness
	return {r * brightness, g * brightness, b * brightness};
}
