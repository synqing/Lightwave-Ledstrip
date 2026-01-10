#!/usr/bin/env python3
"""
Palette Color Extractor for Tab5.encoder LED Display

Extracts 9 representative colors from each of the 75 palettes defined in
firmware/v2/src/palettes/Palettes_MasterData.cpp and generates a C++ header
file with PROGMEM array for fast LED color lookup.
Colors are sampled evenly from the visible range, then sorted by hue for visual cohesion.

Usage:
    python3 tools/palette_extractor.py > firmware/Tab5.encoder/src/config/PaletteLedData.h
"""

import re
import sys
import math
from pathlib import Path

# Number of colors to extract per palette (for 9 LEDs)
COLORS_PER_PALETTE = 9

# Path to palette data file
PALETTE_DATA_FILE = Path(__file__).parent.parent / "firmware" / "v2" / "src" / "palettes" / "Palettes_MasterData.cpp"

# Path to master header to get palette order
PALETTE_MASTER_HEADER = Path(__file__).parent.parent / "firmware" / "v2" / "src" / "palettes" / "Palettes_Master.h"


def parse_gradient_palette(palette_text):
    """
    Parse a DEFINE_GRADIENT_PALETTE block and extract color stops.
    Returns list of (position, r, g, b) tuples.
    """
    # Extract all position, r, g, b entries
    pattern = r'(\d+),\s*(\d+),\s*(\d+),\s*(\d+)'
    matches = re.findall(pattern, palette_text)
    
    stops = []
    for match in matches:
        pos = int(match[0])
        r = int(match[1])
        g = int(match[2])
        b = int(match[3])
        stops.append((pos, r, g, b))
    
    return sorted(stops)  # Ensure sorted by position


def interpolate_color(stops, position):
    """
    Interpolate color at given position from gradient stops.
    stops: list of (position, r, g, b) tuples, sorted by position
    position: target position (0-255)
    """
    if not stops:
        return (0, 0, 0)
    
    # Find surrounding stops
    for i in range(len(stops) - 1):
        pos1, r1, g1, b1 = stops[i]
        pos2, r2, g2, b2 = stops[i + 1]
        
        if pos1 <= position <= pos2:
            # Interpolate between these two stops
            if pos2 == pos1:
                return (r1, g1, b1)
            
            ratio = (position - pos1) / (pos2 - pos1)
            r = int(r1 + (r2 - r1) * ratio)
            g = int(g1 + (g2 - g1) * ratio)
            b = int(b1 + (b2 - b1) * ratio)
            return (r, g, b)
    
    # Position beyond last stop - use last stop color
    return stops[-1][1:4]


def get_color_brightness(r, g, b):
    """
    Calculate color brightness (max of RGB components).
    Returns maximum of R, G, B values.
    """
    return max(r, g, b)


def rgb_to_hsv(r, g, b):
    """
    Convert RGB color to HSV color space.
    
    Args:
        r, g, b: RGB components (0-255)
    
    Returns:
        (h, s, v) tuple where:
        - h: Hue in degrees (0-360)
        - s: Saturation (0.0-1.0)
        - v: Value/Brightness (0.0-1.0)
    """
    # Normalize RGB to 0-1 range
    r_norm = r / 255.0
    g_norm = g / 255.0
    b_norm = b / 255.0
    
    max_val = max(r_norm, g_norm, b_norm)
    min_val = min(r_norm, g_norm, b_norm)
    delta = max_val - min_val
    
    # Value (brightness)
    v = max_val
    
    # Saturation
    s = delta / max_val if max_val > 0 else 0.0
    
    # Hue
    if delta == 0:
        h = 0  # Undefined (grayscale)
    elif max_val == r_norm:
        h = 60 * (((g_norm - b_norm) / delta) % 6)
    elif max_val == g_norm:
        h = 60 * (((b_norm - r_norm) / delta) + 2)
    else:  # max_val == b_norm
        h = 60 * (((r_norm - g_norm) / delta) + 4)
    
    # Ensure hue is in 0-360 range
    h = h % 360
    if h < 0:
        h += 360
    
    return (h, s, v)


def get_hue(r, g, b):
    """
    Get hue value (0-360 degrees) from RGB color.
    Returns hue in degrees, or 0 for grayscale colors.
    """
    h, _, _ = rgb_to_hsv(r, g, b)
    return h


def is_white_color(r, g, b, threshold=200):
    """
    Check if a color is white or near-white (very bright with low saturation).
    WS2812 LEDs don't display white well, so we skip these.
    
    Args:
        r, g, b: RGB components (0-255)
        threshold: Minimum value for components to be considered white
    
    Returns:
        True if color is white-like (very bright with low saturation)
    """
    max_val = max(r, g, b)
    min_val = min(r, g, b)
    
    # Very bright (max > 240) and low saturation (min is close to max)
    if max_val > 240:
        # If min is also high, it's white
        if min_val >= threshold:
            return True
        # If two components are very high and third is moderately high, it's near-white
        sorted_vals = sorted([r, g, b], reverse=True)
        if sorted_vals[0] >= 240 and sorted_vals[1] >= 240 and sorted_vals[2] >= 150:
            return True
    
    # Traditional check: all components above threshold
    return r >= threshold and g >= threshold and b >= threshold


def boost_saturation(r, g, b, boost_factor=1.3):
    """
    Boost color saturation by increasing the dominant channel
    and reducing others proportionally.
    
    Args:
        r, g, b: RGB components (0-255)
        boost_factor: How much to boost saturation (1.0 = no change, 1.3 = 30% boost)
    
    Returns:
        (r, g, b) tuple with boosted saturation
    """
    # Find dominant channel
    max_val = max(r, g, b)
    if max_val == 0:
        return (0, 0, 0)
    
    # Calculate current saturation (how much the color deviates from gray)
    min_val = min(r, g, b)
    saturation = (max_val - min_val) / max_val if max_val > 0 else 0.0
    
    # Boost saturation
    new_saturation = min(1.0, saturation * boost_factor)
    
    # Calculate new RGB values
    # Formula: new = gray + (original - gray) * (new_sat / old_sat)
    gray = (r + g + b) / 3.0
    
    if saturation > 0:
        r_new = int(gray + (r - gray) * (new_saturation / saturation))
        g_new = int(gray + (g - gray) * (new_saturation / saturation))
        b_new = int(gray + (b - gray) * (new_saturation / saturation))
    else:
        # If no saturation, keep original (can't boost gray)
        r_new, g_new, b_new = r, g, b
    
    # Clamp to valid range
    r_new = max(0, min(255, r_new))
    g_new = max(0, min(255, g_new))
    b_new = max(0, min(255, b_new))
    
    return (r_new, g_new, b_new)


def find_visible_range(stops, min_brightness=10, white_threshold=200):
    """
    Find the visible color range in a palette by skipping black/dim and white endpoints.
    WS2812 LEDs don't display white well, so we skip white colors.
    
    Args:
        stops: list of (position, r, g, b) tuples, sorted by position
        min_brightness: minimum brightness threshold (max RGB component) to consider visible
        white_threshold: RGB threshold for considering a color white (all components >= this)
    
    Returns:
        (start_pos, end_pos) tuple representing the visible range
        If no visible colors found, returns (0, 255)
    """
    if not stops:
        return (0, 255)
    
    # Find first position with visible, non-white color
    start_pos = 0
    for pos, r, g, b in stops:
        brightness = get_color_brightness(r, g, b)
        if brightness > min_brightness and not is_white_color(r, g, b, white_threshold):
            start_pos = pos
            break
    
    # Find last position with visible, non-white color (scan backwards)
    end_pos = 255
    for pos, r, g, b in reversed(stops):
        brightness = get_color_brightness(r, g, b)
        if brightness > min_brightness and not is_white_color(r, g, b, white_threshold):
            end_pos = pos
            break
    
    # If entire palette is black/dim/white, use full range
    if start_pos >= end_pos:
        return (0, 255)
    
    # Ensure minimum range (at least 36 positions for 9 LEDs)
    if (end_pos - start_pos) < 36:
        # Expand range symmetrically
        center = (start_pos + end_pos) // 2
        start_pos = max(0, center - 18)
        end_pos = min(255, center + 18)
    
    return (start_pos, end_pos)


def extract_palette_colors(palette_data_path, palette_order):
    """
    Extract palette definitions in the order specified by gMasterPalettes array.
    Uses visible range detection to ensure all 9 LEDs show visible colors.
    Colors are sampled evenly from the visible range, then sorted by hue for visual cohesion.
    Returns list of palette data: [(name, [9 RGB tuples]), ...] in correct order.
    """
    if not palette_data_path.exists():
        print(f"Error: Palette data file not found: {palette_data_path}", file=sys.stderr)
        sys.exit(1)
    
    content = palette_data_path.read_text()
    
    # Find all DEFINE_GRADIENT_PALETTE blocks and create a dictionary
    pattern = r'DEFINE_GRADIENT_PALETTE\((\w+)\)\s*\{([^}]+)\}'
    matches = re.findall(pattern, content, re.DOTALL)
    
    # Create dictionary for fast lookup
    palette_dict = {}
    for name, body in matches:
        stops = parse_gradient_palette(body)
        if not stops:
            print(f"Warning: No stops found in palette {name}", file=sys.stderr)
            continue
        
        # Find visible range (skip black/dim and white start/end)
        start_pos, end_pos = find_visible_range(stops, min_brightness=10, white_threshold=200)
        
        # Calculate 9 evenly-spaced positions across visible range
        # Formula: start + i * (end - start) / 8 for i in 0..8
        visible_range = end_pos - start_pos
        if visible_range == 0:
            # Fallback: use full range if visible range is invalid
            start_pos, end_pos = 0, 255
            visible_range = 255
        
        sample_positions = []
        for i in range(COLORS_PER_PALETTE):
            if i == 0:
                pos = start_pos
            elif i == COLORS_PER_PALETTE - 1:
                pos = end_pos
            else:
                pos = int(start_pos + (i * visible_range / (COLORS_PER_PALETTE - 1.0)))
            sample_positions.append(pos)
        
        # Sample colors at calculated positions and boost saturation
        colors_with_pos = []  # Store (hue, r, g, b, original_pos) for sorting
        for pos in sample_positions:
            r, g, b = interpolate_color(stops, pos)
            
            # Skip white colors (replace with nearby non-white color if possible)
            if is_white_color(r, g, b, threshold=200):
                # Try to find a nearby non-white color
                # Sample slightly before and after this position
                for offset in [-5, -10, 5, 10, -15, 15]:
                    test_pos = max(0, min(255, pos + offset))
                    r_test, g_test, b_test = interpolate_color(stops, test_pos)
                    if not is_white_color(r_test, g_test, b_test, threshold=200):
                        r, g, b = r_test, g_test, b_test
                        break
                # If still white, desaturate it slightly
                if is_white_color(r, g, b, threshold=200):
                    # Reduce to a warm off-white
                    r = min(255, int(r * 0.85))
                    g = min(255, int(g * 0.85))
                    b = min(255, int(b * 0.85))
            
            # Boost saturation for better WS2812 display
            r, g, b = boost_saturation(r, g, b, boost_factor=1.3)
            
            # Final check: if still white-like after saturation boost, desaturate it
            # Check for very bright colors with low saturation (near-white)
            # This includes colors like (255, 255, 34) which look white-ish on WS2812
            max_val = max(r, g, b)
            min_val = min(r, g, b)
            sorted_vals = sorted([r, g, b], reverse=True)
            
            # Very bright (max > 240) with low saturation
            if max_val > 240:
                # Case 1: All components high (pure white)
                if min_val > 100:
                    r = min(255, int(r * 0.70))
                    g = min(255, int(g * 0.70))
                    b = min(255, int(b * 0.70))
                # Case 2: Two components very high, third moderately high (near-white like 255,255,34)
                elif sorted_vals[0] >= 240 and sorted_vals[1] >= 240 and sorted_vals[2] >= 30:
                    # Desaturate by reducing the two high components more
                    if r >= 240:
                        r = min(255, int(r * 0.75))
                    if g >= 240:
                        g = min(255, int(g * 0.75))
                    if b >= 240:
                        b = min(255, int(b * 0.75))
                    # Boost the lower component to add color character
                    if sorted_vals[2] < 100:
                        if r == sorted_vals[2]:
                            r = min(255, int(r * 1.2))
                        elif g == sorted_vals[2]:
                            g = min(255, int(g * 1.2))
                        elif b == sorted_vals[2]:
                            b = min(255, int(b * 1.2))
            elif is_white_color(r, g, b, threshold=220):
                # Traditional white check
                r = min(255, int(r * 0.75))
                g = min(255, int(g * 0.75))
                b = min(255, int(b * 0.75))
            
            # Calculate hue for sorting
            hue = get_hue(r, g, b)
            colors_with_pos.append((hue, r, g, b, pos))
        
        # Sort by hue (0-360 degrees) for visual cohesion
        # If two colors have the same hue, maintain gradient position order as tiebreaker
        colors_with_pos.sort(key=lambda x: (x[0], x[4]))
        
        # Extract just the RGB values in hue-sorted order
        colors = [(r, g, b) for (hue, r, g, b, pos) in colors_with_pos]
        
        palette_dict[name] = colors
    
    # Extract palettes in the order specified by gMasterPalettes array
    palettes = []
    for name in palette_order:
        if name in palette_dict:
            palettes.append((name, palette_dict[name]))
        else:
            print(f"Warning: Palette {name} from gMasterPalettes not found in definitions", file=sys.stderr)
            # Add empty colors as placeholder
            palettes.append((name, [(0, 0, 0)] * COLORS_PER_PALETTE))
    
    return palettes


def get_palette_order():
    """
    Read palette order from gMasterPalettes array in Palettes_MasterData.cpp.
    Returns list of palette names in order (matching array indices 0-74).
    """
    if not PALETTE_DATA_FILE.exists():
        print("Error: Palette data file not found", file=sys.stderr)
        return None
    
    content = PALETTE_DATA_FILE.read_text()
    
    # Find gMasterPalettes array definition
    # Pattern: const TProgmemRGBGradientPaletteRef gMasterPalettes[] = { ... }
    array_pattern = r'const\s+TProgmemRGBGradientPaletteRef\s+gMasterPalettes\[\]\s*=\s*\{([^}]+)\}'
    match = re.search(array_pattern, content, re.DOTALL)
    
    if not match:
        print("Error: Could not find gMasterPalettes array", file=sys.stderr)
        return None
    
    array_content = match.group(1)
    
    # Extract palette names (pattern: name_gp, or name_gp,)
    # Handle both with and without trailing comma
    palette_pattern = r'(\w+_gp)\s*,?'
    palette_names = re.findall(palette_pattern, array_content)
    
    if len(palette_names) != 75:
        print(f"Warning: Expected 75 palettes in array, found {len(palette_names)}", file=sys.stderr)
    
    return palette_names


def generate_header(palettes):
    """
    Generate C++ header file with PROGMEM array.
    """
    header = """#pragma once
// ============================================================================
// PaletteLedData - Pre-computed palette colors for LED display
// ============================================================================
// Auto-generated by tools/palette_extractor.py
// Contains 9 representative colors per palette (75 palettes total)
// Colors sampled across visible range (skips black/dim endpoints)
// Colors are sorted by hue for visual cohesion (rainbow-like progression)
// Ensures all 9 LEDs display visible colors for every palette
// ============================================================================

#include <stdint.h>
#include <Arduino.h>

// Number of palettes
constexpr uint8_t PALETTE_LED_COUNT = 75;

// Number of colors per palette (for 9 LEDs)
constexpr uint8_t PALETTE_COLORS_PER_PALETTE = 9;

// PROGMEM array: paletteLedColors[paletteId][ledIndex][r/g/b]
// paletteId: 0-74 (75 palettes)
// ledIndex: 0-8 (9 LEDs)
// RGB: 0-255
// Colors are sorted by hue (0-360°) for visual cohesion
const uint8_t paletteLedColors[PALETTE_LED_COUNT][PALETTE_COLORS_PER_PALETTE][3] PROGMEM = {
"""
    
    for i, (name, colors) in enumerate(palettes):
        comment = f"    // Palette {i}: {name}"
        header += comment + "\n"
        header += "    {\n"
        
        for j, (r, g, b) in enumerate(colors):
            comma = "," if j < len(colors) - 1 else ""
            # Calculate brightness for comment
            brightness = max(r, g, b)
            header += f"        {{{r:3}, {g:3}, {b:3}}}{comma}  // LED {j} (brightness: {brightness})\n"
        
        comma = "," if i < len(palettes) - 1 else ""
        header += f"    }}{comma}\n"
    
    header += """};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get RGB color for a specific palette and LED index
 * @param paletteId Palette ID (0-74)
 * @param ledIndex LED index (0-8)
 * @param r Output: Red component (0-255)
 * @param g Output: Green component (0-255)
 * @param b Output: Blue component (0-255)
 */
inline void getPaletteColor(uint8_t paletteId, uint8_t ledIndex, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (paletteId >= PALETTE_LED_COUNT || ledIndex >= PALETTE_COLORS_PER_PALETTE) {
        r = g = b = 0;
        return;
    }
    
    r = pgm_read_byte(&paletteLedColors[paletteId][ledIndex][0]);
    g = pgm_read_byte(&paletteLedColors[paletteId][ledIndex][1]);
    b = pgm_read_byte(&paletteLedColors[paletteId][ledIndex][2]);
}

/**
 * Get RGB color for a specific palette and LED index (returns as struct)
 */
struct PaletteColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

inline PaletteColor getPaletteColor(uint8_t paletteId, uint8_t ledIndex) {
    PaletteColor color;
    getPaletteColor(paletteId, ledIndex, color.r, color.g, color.b);
    return color;
}
"""
    
    return header


def main():
    """Main entry point."""
    # All debug output goes to stderr
    print("Extracting palette colors...", file=sys.stderr)
    
    # Get palette order from gMasterPalettes array
    palette_order = get_palette_order()
    if not palette_order:
        print("Error: Could not determine palette order from gMasterPalettes array", file=sys.stderr)
        sys.exit(1)
    
    print(f"Found {len(palette_order)} palettes in gMasterPalettes array", file=sys.stderr)
    
    # Extract colors in the correct order
    palettes = extract_palette_colors(PALETTE_DATA_FILE, palette_order)
    
    if len(palettes) != 75:
        print(f"Warning: Expected 75 palettes, found {len(palettes)}", file=sys.stderr)
    
    print(f"Extracted {len(palettes)} palettes in correct order", file=sys.stderr)
    
    header = generate_header(palettes)
    # Write header to stdout (for redirection to file)
    # CRITICAL: Only write header content, no debug messages to stdout
    sys.stdout.write(header)
    sys.stdout.flush()


if __name__ == "__main__":
    main()

