#!/usr/bin/env python3
"""
LightwaveOS Frame Capture Tool - TAP A/B/C Pipeline
=====================================================

Captures LED frames from running LightwaveOS firmware via serial TAP points:
- TAP A: Pre-correction (after effect render, before ColorCorrectionEngine)
- TAP B: Post-correction (after ColorCorrectionEngine, before FastLED.show())
- TAP C: Post-output (after FastLED.show(), actual WS2812 values)

Saves frames with metadata for offline analysis and replay in Jupyter notebooks.

Requirements:
    pip install pyserial numpy

Usage:
    # Enable capture in firmware (serial command):
    capture on 0x07  # Enable all taps (A=0x01, B=0x02, C=0x04)
    
    # Run capture tool:
    python capture_frames.py --port /dev/cu.usbmodem21401 --frames 100 --output ./captures
    
    # Disable capture:
    capture off
"""

import serial
import numpy as np
import argparse
import json
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, List, Tuple
import struct
import time

# =============================================================================
# CONSTANTS
# =============================================================================

LED_COUNT = 320
BYTES_PER_LED = 3  # RGB
FRAME_BYTES = LED_COUNT * BYTES_PER_LED  # 960 bytes

# TAP identifiers (from RendererNode.h)
TAP_A_PRE_CORRECTION = 0x01
TAP_B_POST_CORRECTION = 0x02
TAP_C_POST_OUTPUT = 0x04

TAP_NAMES = {
    TAP_A_PRE_CORRECTION: 'TAP_A_PRE_CORRECTION',
    TAP_B_POST_CORRECTION: 'TAP_B_POST_CORRECTION',
    TAP_C_POST_OUTPUT: 'TAP_C_POST_OUTPUT',
}

# Serial protocol markers (assuming custom protocol for frame capture)
FRAME_START_MARKER = b'<FRAME>'
FRAME_END_MARKER = b'</FRAME>'
METADATA_START = b'<META>'
METADATA_END = b'</META>'

# =============================================================================
# SERIAL CAPTURE
# =============================================================================

class FrameCapture:
    """Capture frames from LightwaveOS firmware via serial TAP points."""
    
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 5.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None
        
    def connect(self):
        """Open serial connection."""
        print(f"Connecting to {self.port} at {self.baudrate} baud...")
        self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
        time.sleep(2)  # Wait for ESP32 to settle
        print("✓ Connected")
        
    def disconnect(self):
        """Close serial connection."""
        if self.ser:
            self.ser.close()
            self.ser = None
            print("✓ Disconnected")
    
    def enable_capture(self, tap_mask: int = 0x07):
        """
        Enable frame capture in firmware.
        
        Args:
            tap_mask: Bitmask of taps to enable (A=0x01, B=0x02, C=0x04)
        """
        if not self.ser:
            raise RuntimeError("Not connected")
        
        cmd = f"capture on {tap_mask:#x}\n"
        self.ser.write(cmd.encode())
        time.sleep(0.5)
        response = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
        print(f"✓ Capture enabled (mask={tap_mask:#x})")
        if response:
            print(f"  Response: {response.strip()}")
    
    def disable_capture(self):
        """Disable frame capture in firmware."""
        if not self.ser:
            return
        
        cmd = "capture off\n"
        self.ser.write(cmd.encode())
        time.sleep(0.5)
        response = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
        print("✓ Capture disabled")
        if response:
            print(f"  Response: {response.strip()}")
    
    def read_frame(self) -> Optional[Dict]:
        """
        Read a single frame from serial (blocking).
        
        Returns:
            Frame dict with keys: 'tap_id', 'frame_index', 'effect_id', 
            'palette_id', 'brightness', 'speed', 'timestamp', 'data'
            or None if timeout/error
        """
        if not self.ser:
            raise RuntimeError("Not connected")
        
        # Wait for frame start marker
        while True:
            line = self.ser.readline()
            if FRAME_START_MARKER in line:
                break
            if not line:  # Timeout
                return None
        
        # Read metadata
        metadata_raw = b''
        while True:
            line = self.ser.readline()
            if METADATA_END in line:
                metadata_raw += line.split(METADATA_END)[0]
                break
            metadata_raw += line
        
        try:
            metadata = json.loads(metadata_raw.decode('utf-8'))
        except json.JSONDecodeError:
            print(f"  Warning: Failed to parse metadata: {metadata_raw}")
            return None
        
        # Read frame data (960 bytes as hex or binary)
        # Assuming binary transfer for efficiency
        data_bytes = self.ser.read(FRAME_BYTES)
        
        if len(data_bytes) != FRAME_BYTES:
            print(f"  Warning: Incomplete frame data ({len(data_bytes)}/{FRAME_BYTES} bytes)")
            return None
        
        # Convert to numpy array
        frame_data = np.frombuffer(data_bytes, dtype=np.uint8).reshape((LED_COUNT, 3))
        
        # Read frame end marker
        self.ser.read_until(FRAME_END_MARKER)
        
        return {
            'tap_id': metadata.get('tap_id', 0),
            'tap_name': TAP_NAMES.get(metadata.get('tap_id', 0), 'UNKNOWN'),
            'frame_index': metadata.get('frame_index', 0),
            'effect_id': metadata.get('effect_id', 0),
            'palette_id': metadata.get('palette_id', 0),
            'brightness': metadata.get('brightness', 0),
            'speed': metadata.get('speed', 0),
            'timestamp': metadata.get('timestamp', 0),
            'data': frame_data
        }
    
    def capture_sequence(self, n_frames: int) -> List[Dict]:
        """
        Capture a sequence of frames.
        
        Args:
            n_frames: Number of frames to capture
            
        Returns:
            List of frame dicts
        """
        frames = []
        print(f"Capturing {n_frames} frames...")
        
        for i in range(n_frames):
            frame = self.read_frame()
            if frame is None:
                print(f"  Warning: Frame {i} capture failed (timeout)")
                continue
            
            frames.append(frame)
            
            if (i + 1) % 10 == 0:
                print(f"  Progress: {i + 1}/{n_frames} frames")
        
        print(f"✓ Captured {len(frames)} frames")
        return frames


# =============================================================================
# SAVE/LOAD
# =============================================================================

def save_capture(frames: List[Dict], output_dir: Path, session_name: str = None):
    """
    Save captured frames to disk.
    
    Args:
        frames: List of frame dicts
        output_dir: Output directory
        session_name: Optional session name (default: timestamp)
    """
    if not session_name:
        session_name = datetime.now().strftime('%Y%m%d_%H%M%S')
    
    session_dir = output_dir / session_name
    session_dir.mkdir(parents=True, exist_ok=True)
    
    # Save metadata
    metadata = {
        'session_name': session_name,
        'capture_time': datetime.now().isoformat(),
        'n_frames': len(frames),
        'led_count': LED_COUNT,
        'frames': []
    }
    
    # Save individual frames
    for i, frame in enumerate(frames):
        frame_filename = f"frame_{i:04d}_{frame['tap_name']}.npy"
        frame_path = session_dir / frame_filename
        
        # Save frame data as numpy array
        np.save(frame_path, frame['data'])
        
        # Add to metadata
        metadata['frames'].append({
            'index': i,
            'filename': frame_filename,
            'tap_id': frame['tap_id'],
            'tap_name': frame['tap_name'],
            'frame_index': frame['frame_index'],
            'effect_id': frame['effect_id'],
            'palette_id': frame['palette_id'],
            'brightness': frame['brightness'],
            'speed': frame['speed'],
            'timestamp': frame['timestamp'],
        })
    
    # Save metadata as JSON
    metadata_path = session_dir / 'metadata.json'
    with open(metadata_path, 'w') as f:
        json.dump(metadata, f, indent=2)
    
    print(f"✓ Saved {len(frames)} frames to {session_dir}")
    print(f"  Metadata: {metadata_path}")


def load_capture(session_dir: Path) -> Tuple[Dict, List[Dict]]:
    """
    Load captured frames from disk.
    
    Args:
        session_dir: Session directory
        
    Returns:
        (metadata dict, list of frame dicts)
    """
    metadata_path = session_dir / 'metadata.json'
    with open(metadata_path, 'r') as f:
        metadata = json.load(f)
    
    frames = []
    for frame_meta in metadata['frames']:
        frame_path = session_dir / frame_meta['filename']
        frame_data = np.load(frame_path)
        
        frames.append({
            **frame_meta,
            'data': frame_data
        })
    
    print(f"✓ Loaded {len(frames)} frames from {session_dir}")
    return metadata, frames


# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='LightwaveOS Frame Capture Tool')
    parser.add_argument('--port', type=str, required=True,
                       help='Serial port (e.g., /dev/cu.usbmodem21401)')
    parser.add_argument('--baudrate', type=int, default=115200,
                       help='Serial baudrate (default: 115200)')
    parser.add_argument('--frames', type=int, default=100,
                       help='Number of frames to capture (default: 100)')
    parser.add_argument('--taps', type=str, default='0x07',
                       help='TAP mask in hex (A=0x01, B=0x02, C=0x04, default: 0x07 for all)')
    parser.add_argument('--output', type=str, default='./captures',
                       help='Output directory (default: ./captures)')
    parser.add_argument('--session', type=str, default=None,
                       help='Session name (default: timestamp)')
    
    args = parser.parse_args()
    
    output_dir = Path(args.output)
    tap_mask = int(args.taps, 16)
    
    # Create capture instance
    capture = FrameCapture(args.port, args.baudrate)
    
    try:
        # Connect
        capture.connect()
        
        # Enable capture
        capture.enable_capture(tap_mask)
        
        # Capture frames
        frames = capture.capture_sequence(args.frames)
        
        # Save
        save_capture(frames, output_dir, args.session)
        
    except KeyboardInterrupt:
        print("\n✗ Capture interrupted by user")
    except Exception as e:
        print(f"\n✗ Error: {e}")
    finally:
        # Disable capture and disconnect
        capture.disable_capture()
        capture.disconnect()


if __name__ == '__main__':
    main()
