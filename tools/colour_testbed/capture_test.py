#!/usr/bin/env python3
"""
Automated Capture Test Script

Connects to device, enables capture mode, sets effect, and dumps frames.
"""

import serial
import time
import sys
import os
import argparse

# Serial dump frame format (matches v2/src/main.cpp)
# Header: [MAGIC][VERSION][TAP][EFFECT][PALETTE][BRIGHTNESS][SPEED][FRAME_INDEX(4)][TIMESTAMP_US(4)][FRAME_LEN(2)]
# => 1+1+1+1+1+1+1 + 4 + 4 + 2 = 17 bytes
FRAME_HEADER = 17
FRAME_PAYLOAD = 320 * 3  # 960
FRAME_TOTAL = FRAME_HEADER + FRAME_PAYLOAD  # 977


def _read_frame(ser, timeout=8.0, retries=2):
    """Read one binary frame starting at magic 0xFD, return bytes or None."""
    for _ in range(retries + 1):
        deadline = time.time() + timeout
        data = bytearray()

        # Seek magic
        while time.time() < deadline:
            if ser.in_waiting:
                b = ser.read(1)
                if b and b[0] == 0xFD:
                    data.append(b[0])
                    break
            time.sleep(0.005)

        if not data:
            continue

        # Read remainder
        while time.time() < deadline and len(data) < FRAME_TOTAL:
            if ser.in_waiting:
                chunk = ser.read(min(ser.in_waiting, FRAME_TOTAL - len(data)))
                if chunk:
                    data.extend(chunk)
            time.sleep(0.002)

        if len(data) == FRAME_TOTAL:
            return bytes(data)

    return None


DEFAULT_EFFECTS = [3, 0, 1, 2, 4, 6, 8, 10, 13, 16, 26, 32, 39, 40, 58, 65, 66, 67]


def _drain_text(ser, max_lines=200, max_time_s=1.0):
    """Drain and print any pending text lines, without blocking too long."""
    deadline = time.time() + max_time_s
    lines = 0
    while time.time() < deadline and ser.in_waiting and lines < max_lines:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f"   {line}")
            lines += 1


def _set_effect(ser, effect_id: int):
    """Set effect using multi-digit safe command implemented in firmware: 'effect <id>'."""
    ser.write(f"effect {effect_id}\n".encode())
    ser.flush()
    time.sleep(0.25)
    _drain_text(ser, max_time_s=1.0)


def capture_one_effect(ser, effect_id: int, output_dir: str):
    """Capture taps A/B/C for a single effect into output_dir/effect_<id>/..."""
    os.makedirs(output_dir, exist_ok=True)
    effect_dir = os.path.join(output_dir, f"effect_{effect_id}")
    os.makedirs(effect_dir, exist_ok=True)

    print(f"\n=== Effect {effect_id} ===")
    _set_effect(ser, effect_id)
    print("   Settled...")
    time.sleep(0.75)
    
    # Dump each tap
    taps = ['a', 'b', 'c']
    tap_names = ['pre_correction', 'post_correction', 'pre_ws2812']

    for tap, tap_name in zip(taps, tap_names):
        print(f"   Dumping Tap {tap.upper()} ({tap_name})...")

        # Clear any text noise before requesting a binary dump
        ser.reset_input_buffer()

        # Send dump command
        ser.write(f"capture dump {tap}\n".encode())
        ser.flush()

        frame_data = _read_frame(ser, timeout=8.0, retries=2)

        if frame_data and len(frame_data) == FRAME_TOTAL:
            filename = os.path.join(effect_dir, f"effect_{effect_id}_tap_{tap_name}.bin")
            with open(filename, 'wb') as f:
                f.write(frame_data)
            print(f"     Saved: {filename} ({len(frame_data)} bytes)")

            # Parse header for verification
            magic, version, tap_id = frame_data[0], frame_data[1], frame_data[2]
            effect_id_actual, palette_id, brightness, speed = frame_data[3:7]
            frame_index = int.from_bytes(frame_data[7:11], 'little')
            timestamp = int.from_bytes(frame_data[11:15], 'little')
            frame_len = int.from_bytes(frame_data[15:17], 'little')

            ok = "OK" if effect_id_actual == effect_id else "MISMATCH"
            print(f"     Header[{ok}]: tap={tap_id}, effect={effect_id_actual}, palette={palette_id}, "
                  f"frame={frame_index}, len={frame_len}")
        else:
            got = len(frame_data) if frame_data else 0
            print(f"     Warning: Incomplete frame (got {got} bytes, expected {FRAME_TOTAL})")
            # Drain any text diagnostics from the device to help explain why no frame arrived
            _drain_text(ser, max_time_s=1.0)

        # Read any remaining text output
        time.sleep(0.2)
        _drain_text(ser, max_time_s=0.5)


def capture_test(port='/dev/tty.usbmodem1101', baud=115200, effects=None, output_dir='captures'):
    """Capture taps A/B/C for a list of effects."""
    effects = effects or DEFAULT_EFFECTS

    os.makedirs(output_dir, exist_ok=True)

    print(f"Connecting to {port} at {baud} baud...")
    ser = serial.Serial(port, baud, timeout=2)

    # Wait for device to be ready
    time.sleep(2)

    # Clear any existing data
    ser.reset_input_buffer()

    print("\n=== Capture Test (Gamut) ===")
    print(f"Effects: {effects}")
    print(f"Output directory: {output_dir}\n")

    # Step 1: Enable capture mode (all taps)
    print("1. Enabling capture mode (all taps)...")
    # Send command with newline and flush
    ser.write(b"capture on\n")
    ser.flush()
    time.sleep(1.0)  # Give device time to process
    
    # Read response
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f"   {line}")
    
    # Step 2: capture each effect
    print("\n2. Capturing effects...")
    for effect_id in effects:
        capture_one_effect(ser, effect_id, output_dir)
    
    # Step 5: Disable capture mode
    print("\n5. Disabling capture mode...")
    ser.write(b"capture off\n")
    time.sleep(0.5)
    
    # Read response
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f"   {line}")
    
    ser.close()
    print("\n=== Capture Test Complete ===")
    print(f"Files saved to: {output_dir}/")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run capture test on device")
    parser.add_argument("--port", default="/dev/tty.usbmodem1101", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--effects", default=",".join(str(x) for x in DEFAULT_EFFECTS),
                        help="Comma-separated effect IDs to capture (default: curated gamut list)")
    parser.add_argument("--output", default="captures/candidate_gamut2", help="Output directory")

    args = parser.parse_args()
    effects = [int(x.strip()) for x in args.effects.split(",") if x.strip() != ""]

    try:
        capture_test(args.port, args.baud, effects, args.output)
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

