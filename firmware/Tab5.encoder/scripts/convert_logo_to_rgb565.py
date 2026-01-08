#!/usr/bin/env python3
"""
Convert JPG logo image to RGB565 format for LVGL loading screen.

Converts a JPG image to RGB565 (16-bit color format) and generates a C header file
with the pixel data in PROGMEM format.
"""

import sys
import os
from PIL import Image

def rgb888_to_rgb565(r, g, b):
    """Convert 24-bit RGB to 16-bit RGB565 format."""
    # Convert very dark pixels (near black) to loading screen background color (0x0A0A0B = RGB565 0x0841)
    # This ensures logo background matches the loading screen background
    # Use brightness threshold: if average brightness < 48 (dark gray/black), make it 0x0841
    # This handles JPEG compression artifacts that make black backgrounds slightly gray
    brightness = (r + g + b) / 3
    if brightness < 48:
        # Loading screen background: RGB888 0x0A0A0B = RGB565 0x0841
        # R=0x0A (10), G=0x0A (10), B=0x0B (11)
        # RGB565: (10>>3)<<11 | (10>>2)<<5 | (11>>3) = 0<<11 | 2<<5 | 1 = 0x0841
        return 0x0841
    
    # RGB565: 5 bits Red, 6 bits Green, 5 bits Blue
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def convert_image_to_rgb565(input_path, output_path, target_size=None):
    """
    Convert JPG image to RGB565 format and generate C header file.
    
    Args:
        input_path: Path to input JPG image
        output_path: Path to output C header file
        target_size: Optional tuple (width, height) for resizing. If None, uses native size.
    """
    # Load image
    print(f"Loading image: {input_path}")
    img = Image.open(input_path)
    
    # Convert to RGB if needed (handles RGBA, P, etc.)
    if img.mode != 'RGB':
        print(f"Converting from {img.mode} to RGB...")
        # Create white background for transparency
        rgb_img = Image.new('RGB', img.size, (0, 0, 0))  # Black background
        if img.mode == 'RGBA':
            rgb_img.paste(img, mask=img.split()[3])  # Use alpha channel as mask
        else:
            rgb_img.paste(img)
        img = rgb_img
    
    original_size = img.size
    print(f"Original size: {original_size[0]}x{original_size[1]}")
    
    # Resize if target size specified
    if target_size:
        print(f"Resizing to: {target_size[0]}x{target_size[1]}")
        img = img.resize(target_size, Image.Resampling.LANCZOS)
    else:
        # Use native resolution
        target_size = original_size
        print(f"Using native resolution: {target_size[0]}x{target_size[1]}")
    
    width, height = img.size
    total_pixels = width * height
    
    print(f"Converting {total_pixels} pixels to RGB565...")
    
    # Convert to RGB565
    rgb565_data = []
    pixels = img.load()
    
    for y in range(height):
        for x in range(width):
            r, g, b = pixels[x, y]
            rgb565 = rgb888_to_rgb565(r, g, b)
            rgb565_data.append(rgb565)
    
    # Generate C header file
    print(f"Generating C header file: {output_path}")
    
    array_size = len(rgb565_data)
    header_content = f"""#pragma once

#include <cstdint>
#ifdef SIMULATOR_BUILD
    // PROGMEM not available in simulator - just use regular const
    #define PROGMEM
#else
    #include <pgmspace.h>
#endif

#define SPECTRASYNQ_LOGO_SMALL_HEIGHT {height}
#define SPECTRASYNQ_LOGO_SMALL_WIDTH {width}

// array size is {array_size}
static const uint16_t SpectraSynq_Logo_Small[] PROGMEM = {{
"""
    
    # Format data: 16 values per line
    values_per_line = 16
    for i in range(0, len(rgb565_data), values_per_line):
        line_values = rgb565_data[i:i + values_per_line]
        line = '  ' + ', '.join(f'0x{rgb565:04X}' for rgb565 in line_values)
        if i + values_per_line < len(rgb565_data):
            line += ','
        header_content += line + '\n'
    
    header_content += '};\n'
    
    # Write header file
    with open(output_path, 'w') as f:
        f.write(header_content)
    
    print(f"Conversion complete!")
    print(f"  Output size: {width}x{height}")
    print(f"  Total pixels: {total_pixels}")
    print(f"  Array size: {array_size}")
    print(f"  Data size: {array_size * 2} bytes ({array_size * 2 / 1024:.2f} KB)")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: convert_logo_to_rgb565.py <input.jpg> <output.h> [width] [height]")
        print("  If width/height not specified, uses native image resolution")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    
    target_size = None
    if len(sys.argv) >= 4:
        width = int(sys.argv[3])
        if len(sys.argv) >= 5:
            height = int(sys.argv[4])
            target_size = (width, height)
        else:
            # Maintain aspect ratio if only width specified
            img = Image.open(input_path)
            aspect = img.height / img.width
            height = int(width * aspect)
            target_size = (width, height)
            print(f"Maintaining aspect ratio: {width}x{height}")
    
    if not os.path.exists(input_path):
        print(f"Error: Input file not found: {input_path}")
        sys.exit(1)
    
    convert_image_to_rgb565(input_path, output_path, target_size)

