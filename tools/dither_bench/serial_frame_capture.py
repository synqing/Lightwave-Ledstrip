#!/usr/bin/env python3
"""
Serial Frame Capture Receiver for LightwaveOS v2

Receives real-time frame captures from firmware via serial port.
Supports TAP_A (pre-correction), TAP_B (post-correction), TAP_C (pre-WS2812).

Usage:
    python serial_frame_capture.py --port /dev/ttyUSB0 --output captured_frames/ --duration 10
"""

import argparse
import serial
import struct
import numpy as np
import time
import json
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, Any


# Frame format constants (must match firmware)
SYNC_BYTE_1 = 0xFF
SYNC_BYTE_2 = 0xFE
HEADER_SIZE = 16  # Sync(2) + Tap(1) + Reserved(1) + FrameCount(4) + LEDCount(2) + Metadata(6)


class FrameCapture:
    """Container for captured frame data."""
    
    def __init__(self, tap_id: int, frame_count: int, led_count: int,
                 rgb_data: np.ndarray, metadata: Dict[str, Any]):
        self.tap_id = tap_id
        self.tap_name = self._tap_name(tap_id)
        self.frame_count = frame_count
        self.led_count = led_count
        self.rgb_data = rgb_data  # Shape: (led_count, 3), uint8
        self.metadata = metadata
        self.timestamp = datetime.now().isoformat()
    
    @staticmethod
    def _tap_name(tap_id: int) -> str:
        """Convert tap ID to human-readable name."""
        names = {
            0: "TAP_A_PRE_CORRECTION",
            1: "TAP_B_POST_CORRECTION",
            2: "TAP_C_PRE_WS2812"
        }
        return names.get(tap_id, f"UNKNOWN_TAP_{tap_id}")
    
    def save(self, output_dir: Path):
        """Save frame to disk."""
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Save as NPZ (NumPy compressed)
        filename = f"frame_{self.frame_count:06d}_{self.tap_name}.npz"
        npz_path = output_dir / filename
        
        np.savez_compressed(
            npz_path,
            rgb=self.rgb_data,
            metadata=self.metadata,
            tap_id=self.tap_id,
            frame_count=self.frame_count
        )
        
        return npz_path


class SerialFrameReceiver:
    """Receives frame captures from LightwaveOS v2 via serial."""
    
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 1.0):
        """
        Initialize serial receiver.
        
        Args:
            port: Serial port (e.g., /dev/ttyUSB0, COM3)
            baudrate: Baud rate (default 115200)
            timeout: Read timeout in seconds
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None
        
        self.frames_received = 0
        self.sync_errors = 0
        self.checksum_errors = 0
    
    def connect(self):
        """Open serial connection."""
        self.ser = serial.Serial(
            self.port,
            self.baudrate,
            timeout=self.timeout,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )
        
        # Flush buffers
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        
        print(f"✓ Connected to {self.port} @ {self.baudrate} baud")
    
    def disconnect(self):
        """Close serial connection."""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"✓ Disconnected from {self.port}")
    
    def wait_for_sync(self, timeout_sec: float = 5.0) -> bool:
        """
        Wait for sync bytes.
        
        Returns:
            True if sync found, False if timeout
        """
        start_time = time.time()
        
        while (time.time() - start_time) < timeout_sec:
            byte1 = self.ser.read(1)
            if len(byte1) == 0:
                continue
            
            if byte1[0] == SYNC_BYTE_1:
                byte2 = self.ser.read(1)
                if len(byte2) > 0 and byte2[0] == SYNC_BYTE_2:
                    return True
        
        self.sync_errors += 1
        return False
    
    def read_frame(self) -> Optional[FrameCapture]:
        """
        Read a single frame from serial.
        
        Returns:
            FrameCapture object, or None if sync lost/timeout
        """
        if not self.ser or not self.ser.is_open:
            raise RuntimeError("Serial not connected")
        
        # Wait for sync bytes
        if not self.wait_for_sync():
            return None
        
        try:
            # Read header (after sync bytes)
            tap_id = ord(self.ser.read(1))
            reserved = ord(self.ser.read(1))
            frame_count = struct.unpack('<I', self.ser.read(4))[0]
            led_count = struct.unpack('<H', self.ser.read(2))[0]
            
            # Read metadata (effect ID, palette, brightness, speed)
            effect_id = ord(self.ser.read(1))
            palette_id = ord(self.ser.read(1))
            brightness = ord(self.ser.read(1))
            speed = ord(self.ser.read(1))
            timestamp_us = struct.unpack('<I', self.ser.read(4))[0]
            
            metadata = {
                'effect_id': effect_id,
                'palette_id': palette_id,
                'brightness': brightness,
                'speed': speed,
                'timestamp_us': timestamp_us,
            }
            
            # Read RGB data
            expected_bytes = led_count * 3
            rgb_data = self.ser.read(expected_bytes)
            
            if len(rgb_data) != expected_bytes:
                print(f"⚠️  Incomplete frame: expected {expected_bytes} bytes, got {len(rgb_data)}")
                return None
            
            # Parse RGB data
            rgb_array = np.frombuffer(rgb_data, dtype=np.uint8).reshape((led_count, 3))
            
            # Create FrameCapture object
            capture = FrameCapture(tap_id, frame_count, led_count, rgb_array, metadata)
            
            self.frames_received += 1
            return capture
        
        except Exception as e:
            print(f"⚠️  Error reading frame: {e}")
            return None
    
    def capture_sequence(self, duration_sec: float, output_dir: Path,
                        verbose: bool = True) -> int:
        """
        Capture frames for specified duration.
        
        Args:
            duration_sec: Duration to capture in seconds
            output_dir: Directory to save frames
            verbose: Print progress
        
        Returns:
            Number of frames captured
        """
        output_dir.mkdir(parents=True, exist_ok=True)
        
        start_time = time.time()
        frames_saved = 0
        
        if verbose:
            print(f"Capturing frames for {duration_sec}s...")
            print(f"Output: {output_dir}")
        
        while (time.time() - start_time) < duration_sec:
            frame = self.read_frame()
            
            if frame is not None:
                # Save frame
                save_path = frame.save(output_dir)
                frames_saved += 1
                
                if verbose and frames_saved % 10 == 0:
                    elapsed = time.time() - start_time
                    fps = frames_saved / elapsed
                    print(f"  {frames_saved} frames ({fps:.1f} FPS) - {frame.tap_name}")
        
        if verbose:
            print(f"\n✓ Captured {frames_saved} frames in {duration_sec:.1f}s")
            print(f"  Sync errors: {self.sync_errors}")
        
        return frames_saved
    
    def get_stats(self) -> Dict[str, Any]:
        """Get receiver statistics."""
        return {
            'frames_received': self.frames_received,
            'sync_errors': self.sync_errors,
            'checksum_errors': self.checksum_errors,
        }


def main():
    parser = argparse.ArgumentParser(
        description="Receive frame captures from LightwaveOS v2 firmware"
    )
    
    parser.add_argument(
        '--port',
        type=str,
        required=True,
        help='Serial port (e.g., /dev/ttyUSB0, COM3)'
    )
    
    parser.add_argument(
        '--baudrate',
        type=int,
        default=115200,
        help='Baud rate (default: 115200)'
    )
    
    parser.add_argument(
        '--output',
        type=Path,
        default=Path('captured_frames'),
        help='Output directory for captured frames'
    )
    
    parser.add_argument(
        '--duration',
        type=float,
        default=10.0,
        help='Capture duration in seconds (default: 10)'
    )
    
    parser.add_argument(
        '--quiet',
        action='store_true',
        help='Suppress verbose output'
    )
    
    args = parser.parse_args()
    
    # Create receiver
    receiver = SerialFrameReceiver(args.port, args.baudrate)
    
    try:
        # Connect
        receiver.connect()
        
        # Capture sequence
        frames_captured = receiver.capture_sequence(
            args.duration,
            args.output,
            verbose=not args.quiet
        )
        
        # Save session metadata
        session_info = {
            'port': args.port,
            'baudrate': args.baudrate,
            'duration_sec': args.duration,
            'frames_captured': frames_captured,
            'stats': receiver.get_stats(),
            'timestamp': datetime.now().isoformat(),
        }
        
        session_path = args.output / 'session_info.json'
        with open(session_path, 'w') as f:
            json.dump(session_info, f, indent=2)
        
        print(f"\n✓ Session info saved to: {session_path}")
    
    except KeyboardInterrupt:
        print("\n⚠️  Capture interrupted by user")
    
    except Exception as e:
        print(f"\n❌ Error: {e}")
        return 1
    
    finally:
        receiver.disconnect()
    
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
