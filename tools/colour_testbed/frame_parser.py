#!/usr/bin/env python3
"""
Frame Parser for Colour Pipeline Testbed

Parses binary frame dumps from serial and WebSocket led_stream captures.
"""

import struct
import numpy as np
from typing import List, Tuple, Optional, Dict
from dataclasses import dataclass

# ============================================================================
# Frame Format Constants
# ============================================================================

SERIAL_MAGIC = 0xFD
SERIAL_VERSION = 0x01
# Serial header is 17 bytes:
# [MAGIC][VERSION][TAP_ID][EFFECT_ID][PALETTE_ID][BRIGHTNESS][SPEED][FRAME_INDEX(4)][TIMESTAMP(4)][FRAME_LEN(2)]
SERIAL_HEADER_SIZE = 17
SERIAL_PAYLOAD_SIZE = 320 * 3  # RGB × 320 LEDs
SERIAL_FRAME_SIZE = SERIAL_HEADER_SIZE + SERIAL_PAYLOAD_SIZE

LED_STREAM_MAGIC = 0xFE
LED_STREAM_VERSION = 0x01
LED_STREAM_HEADER_SIZE = 4  # Magic + Version + NumStrips + LEDsPerStrip
LED_STREAM_STRIP_SIZE = 1 + (160 * 3)  # StripID + RGB×160
LED_STREAM_PAYLOAD_SIZE = 2 * LED_STREAM_STRIP_SIZE  # Both strips
LED_STREAM_FRAME_SIZE = LED_STREAM_HEADER_SIZE + LED_STREAM_PAYLOAD_SIZE

# ============================================================================
# Data Structures
# ============================================================================

@dataclass
class FrameMetadata:
    """Metadata for a captured frame"""
    tap_id: int  # 0=Tap A, 1=Tap B, 2=Tap C
    effect_id: int
    palette_id: int
    brightness: int
    speed: int
    frame_index: int
    timestamp_us: int

@dataclass
class Frame:
    """Complete frame with metadata and RGB data"""
    metadata: FrameMetadata
    rgb_data: np.ndarray  # Shape: (320, 3) - RGB values 0-255
    strip1: np.ndarray     # Shape: (160, 3) - LEDs 0-159
    strip2: np.ndarray    # Shape: (160, 3) - LEDs 160-319

# ============================================================================
# Serial Dump Parser
# ============================================================================

def parse_serial_dump(filepath: str) -> List[Frame]:
    """
    Parse serial dump file containing binary frame data.
    
    Format:
    - Header (16 bytes): [MAGIC][VERSION][TAP_ID][EFFECT_ID][PALETTE_ID][BRIGHTNESS][SPEED][FRAME_INDEX(4)][TIMESTAMP(4)][FRAME_LEN(2)]
    - Payload (960 bytes): [RGB×320]
    
    Args:
        filepath: Path to serial dump file
        
    Returns:
        List of Frame objects
    """
    frames = []
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    offset = 0
    frame_num = 0
    
    while offset + SERIAL_FRAME_SIZE <= len(data):
        # Parse header
        if data[offset] != SERIAL_MAGIC:
            # Skip until we find magic byte
            offset += 1
            continue
        
        if data[offset + 1] != SERIAL_VERSION:
            print(f"Warning: Unknown frame version {data[offset + 1]} at offset {offset}")
            offset += 1
            continue
        
        # Unpack header:
        # 7x uint8 + uint32 + uint32 + uint16
        header = struct.unpack('<BBBBBBBIIH', data[offset:offset + SERIAL_HEADER_SIZE])
        magic, version, tap_id, effect_id, palette_id, brightness, speed = header[0:7]
        frame_index = header[7]
        timestamp_us = header[8]
        frame_len = header[9]
        
        if frame_len != SERIAL_PAYLOAD_SIZE:
            print(f"Warning: Unexpected frame length {frame_len} at offset {offset}")
            offset += 1
            continue
        
        # Parse payload (RGB data)
        payload_start = offset + SERIAL_HEADER_SIZE
        payload = data[payload_start:payload_start + SERIAL_PAYLOAD_SIZE]
        
        # Reshape to (320, 3) RGB array
        rgb_data = np.frombuffer(payload, dtype=np.uint8).reshape(320, 3)
        
        # Split into strips
        strip1 = rgb_data[0:160]  # LEDs 0-159
        strip2 = rgb_data[160:320]  # LEDs 160-319
        
        # Create metadata
        metadata = FrameMetadata(
            tap_id=tap_id,
            effect_id=effect_id,
            palette_id=palette_id,
            brightness=brightness,
            speed=speed,
            frame_index=frame_index,
            timestamp_us=timestamp_us
        )
        
        # Create frame
        frame = Frame(
            metadata=metadata,
            rgb_data=rgb_data,
            strip1=strip1,
            strip2=strip2
        )
        
        frames.append(frame)
        offset += SERIAL_FRAME_SIZE
        frame_num += 1
    
    print(f"Parsed {len(frames)} frames from {filepath}")
    return frames

# ============================================================================
# Led Stream Parser
# ============================================================================

def parse_led_stream(filepath: str, tap_id: Optional[int] = None) -> List[Frame]:
    """
    Parse WebSocket led_stream capture file.
    
    Format (v1):
    - Header (4 bytes): [MAGIC=0xFE][VERSION=0x01][NUM_STRIPS=0x02][LEDS_PER_STRIP=160]
    - Strip 0: [STRIP_ID=0][RGB×160]
    - Strip 1: [STRIP_ID=1][RGB×160]
    
    Args:
        filepath: Path to led_stream capture file
        tap_id: Optional tap ID to assign (if not in frame metadata)
        
    Returns:
        List of Frame objects
    """
    frames = []
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    offset = 0
    frame_num = 0
    
    while offset + LED_STREAM_FRAME_SIZE <= len(data):
        # Parse header
        if data[offset] != LED_STREAM_MAGIC:
            offset += 1
            continue
        
        if data[offset + 1] != LED_STREAM_VERSION:
            print(f"Warning: Unknown led_stream version {data[offset + 1]} at offset {offset}")
            offset += 1
            continue
        
        # Unpack header
        magic, version, num_strips, leds_per_strip = struct.unpack('<BBBB', data[offset:offset + 4])
        
        if num_strips != 2 or leds_per_strip != 160:
            print(f"Warning: Unexpected strip config at offset {offset}")
            offset += 1
            continue
        
        # Parse strips
        payload_start = offset + LED_STREAM_HEADER_SIZE
        
        # Strip 0
        strip0_id = data[payload_start]
        strip0_data = data[payload_start + 1:payload_start + 1 + (160 * 3)]
        strip0_rgb = np.frombuffer(strip0_data, dtype=np.uint8).reshape(160, 3)
        
        # Strip 1
        strip1_start = payload_start + 1 + (160 * 3)
        strip1_id = data[strip1_start]
        strip1_data = data[strip1_start + 1:strip1_start + 1 + (160 * 3)]
        strip1_rgb = np.frombuffer(strip1_data, dtype=np.uint8).reshape(160, 3)
        
        # Combine into unified buffer (strip1 first, then strip2)
        rgb_data = np.vstack([strip0_rgb, strip1_rgb])
        
        # Create metadata (led_stream doesn't include full metadata, use defaults)
        metadata = FrameMetadata(
            tap_id=tap_id if tap_id is not None else 1,  # Default to Tap B (post-correction)
            effect_id=0,  # Unknown from led_stream
            palette_id=0,  # Unknown
            brightness=255,  # Unknown
            speed=10,  # Unknown
            frame_index=frame_num,
            timestamp_us=frame_num * 50000  # Estimate: 20 FPS = 50ms per frame
        )
        
        # Create frame
        frame = Frame(
            metadata=metadata,
            rgb_data=rgb_data,
            strip1=strip0_rgb,  # Note: led_stream strip0 = our strip1
            strip2=strip1_rgb   # led_stream strip1 = our strip2
        )
        
        frames.append(frame)
        offset += LED_STREAM_FRAME_SIZE
        frame_num += 1
    
    print(f"Parsed {len(frames)} frames from {filepath}")
    return frames

# ============================================================================
# Utility Functions
# ============================================================================

def extract_frames_by_effect(frames: List[Frame], effect_id: int) -> List[Frame]:
    """Filter frames by effect ID"""
    return [f for f in frames if f.metadata.effect_id == effect_id]

def extract_frames_by_tap(frames: List[Frame], tap_id: int) -> List[Frame]:
    """Filter frames by tap ID"""
    return [f for f in frames if f.metadata.tap_id == tap_id]

def get_frame_statistics(frames: List[Frame]) -> Dict:
    """Get summary statistics for a frame sequence"""
    if not frames:
        return {}
    
    # Extract RGB data
    all_rgb = np.vstack([f.rgb_data for f in frames])
    
    # Calculate per-channel statistics
    stats = {
        'num_frames': len(frames),
        'mean_r': float(np.mean(all_rgb[:, 0])),
        'mean_g': float(np.mean(all_rgb[:, 1])),
        'mean_b': float(np.mean(all_rgb[:, 2])),
        'std_r': float(np.std(all_rgb[:, 0])),
        'std_g': float(np.std(all_rgb[:, 1])),
        'std_b': float(np.std(all_rgb[:, 2])),
        'min_r': int(np.min(all_rgb[:, 0])),
        'min_g': int(np.min(all_rgb[:, 1])),
        'min_b': int(np.min(all_rgb[:, 2])),
        'max_r': int(np.max(all_rgb[:, 0])),
        'max_g': int(np.max(all_rgb[:, 1])),
        'max_b': int(np.max(all_rgb[:, 2])),
    }
    
    # Calculate luma (BT.601)
    luma = 0.299 * all_rgb[:, 0] + 0.587 * all_rgb[:, 1] + 0.114 * all_rgb[:, 2]
    stats['mean_luma'] = float(np.mean(luma))
    stats['std_luma'] = float(np.std(luma))
    
    return stats

# ============================================================================
# Main (for testing)
# ============================================================================

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: frame_parser.py <dump_file> [tap_id]")
        print("  dump_file: Serial dump or led_stream capture file")
        print("  tap_id: Optional tap ID for led_stream (0=Tap A, 1=Tap B, 2=Tap C)")
        sys.exit(1)
    
    filepath = sys.argv[1]
    tap_id = int(sys.argv[2]) if len(sys.argv) > 2 else None
    
    # Try serial format first
    try:
        frames = parse_serial_dump(filepath)
        if frames:
            print(f"Parsed as serial dump: {len(frames)} frames")
            stats = get_frame_statistics(frames)
            print(f"Statistics: {stats}")
            sys.exit(0)
    except Exception as e:
        print(f"Failed to parse as serial dump: {e}")
    
    # Try led_stream format
    try:
        frames = parse_led_stream(filepath, tap_id)
        if frames:
            print(f"Parsed as led_stream: {len(frames)} frames")
            stats = get_frame_statistics(frames)
            print(f"Statistics: {stats}")
            sys.exit(0)
    except Exception as e:
        print(f"Failed to parse as led_stream: {e}")
    
    print("Failed to parse file in any known format")
    sys.exit(1)

