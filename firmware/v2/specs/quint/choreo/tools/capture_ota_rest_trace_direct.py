#!/usr/bin/env python3
"""
Direct serial capture for OTA REST traces - reads serial port directly and runs OTA REST HTTP scenarios

Captures raw serial output (with ESP_LOG prefixes) and post-processes through extract_jsonl.py
to guarantee valid JSONL even with log prefixes.
"""
import serial
import sys
import time
import threading
import subprocess
from pathlib import Path

# Try to import requests, fallback to urllib
try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False
    import urllib.request
    import urllib.parse

def capture_serial_direct(port, raw_output_file, duration=30):
    """Capture raw serial output directly using pyserial (includes ESP_LOG prefixes)"""
    try:
        # Open serial port with explicit settings
        ser = serial.Serial(port, 115200, timeout=1, write_timeout=1)
        lines = []
        start_time = time.time()
        
        # Flush any existing input
        ser.reset_input_buffer()
        
        print(f"Serial port {port} opened, capturing for {duration}s...", file=sys.stderr)
        
        while time.time() - start_time < duration:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore')
                if line:
                    lines.append(line)
                    # Print first few lines for debugging
                    if len(lines) <= 5:
                        print(f"Captured: {line[:80]}", file=sys.stderr)
            else:
                time.sleep(0.1)  # Small delay when no data
        
        ser.close()
        
        # Write raw capture (will be post-processed with extract_jsonl.py)
        if lines:
            with open(raw_output_file, 'w') as f:
                for line in lines:
                    f.write(line)
            print(f"Captured {len(lines)} lines to {raw_output_file}", file=sys.stderr)
        else:
            # Create empty file so post-processing doesn't fail
            Path(raw_output_file).touch()
            print(f"WARNING: No lines captured, created empty file {raw_output_file}", file=sys.stderr)
        
        return len(lines)
    except serial.SerialException as e:
        print(f"Serial port error: {e}", file=sys.stderr)
        print(f"  Port: {port}", file=sys.stderr)
        print(f"  Make sure device is connected and port is not in use", file=sys.stderr)
        return 0
    except Exception as e:
        print(f"Serial error: {e}", file=sys.stderr)
        return 0

def read_pio_firmware_binary():
    """Read the latest PlatformIO build artifact firmware.bin"""
    import os
    # Path to PIO build artifact (relative to workspace root or script location)
    script_dir = Path(__file__).parent
    # Navigate to workspace root: script is at firmware/v2/specs/quint/choreo/tools/
    workspace_root = script_dir.parent.parent.parent.parent
    firmware_path = workspace_root / ".pio" / "build" / "esp32dev_audio" / "firmware.bin"
    
    if not firmware_path.exists():
        raise FileNotFoundError(
            f"Firmware binary not found at {firmware_path}\n"
            f"  Please build the firmware first: cd firmware/v2 && pio run -e esp32dev_audio"
        )
    
    if firmware_path.stat().st_size == 0:
        raise ValueError(
            f"Firmware binary is empty at {firmware_path}\n"
            f"  Please rebuild the firmware: cd firmware/v2 && pio run -e esp32dev_audio"
        )
    
    with open(firmware_path, 'rb') as f:
        data = f.read()
    
    print(f"Loaded firmware binary: {len(data)} bytes from {firmware_path}", file=sys.stderr)
    return data

def create_fake_firmware_binary(size_bytes=10000):
    """Create a fake firmware binary for testing (deprecated - use read_pio_firmware_binary instead)"""
    pattern = b'\x00\x01\x02\x03' * (size_bytes // 4 + 1)
    return pattern[:size_bytes]

def run_ota_rest_happy_path(host, token):
    """Scenario 1: ota_rest_happy_path - POST /api/v1/ota/upload with valid X-OTA-Token"""
    print("Executing ota_rest_happy_path scenario...", file=sys.stderr)
    
    url = f"http://{host}/api/v1/ota/upload"
    try:
        firmware_data = read_pio_firmware_binary()
    except (FileNotFoundError, ValueError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        raise
    
    if HAS_REQUESTS:
        # Use requests library (cleaner API)
        try:
            files = {'update': ('firmware.bin', firmware_data, 'application/octet-stream')}
            headers = {'X-OTA-Token': token}
            
            print(f"POSTing {len(firmware_data)} bytes to {url}...", file=sys.stderr)
            response = requests.post(url, files=files, headers=headers, timeout=30)
            print(f"Response status: {response.status_code}", file=sys.stderr)
            if response.status_code == 200:
                print("OTA upload successful (device should reboot)", file=sys.stderr)
            else:
                print(f"Response: {response.text[:200]}", file=sys.stderr)
        except Exception as e:
            print(f"HTTP error: {e}", file=sys.stderr)
    else:
        # Fallback to urllib (no multipart support easily, so use a simpler approach)
        print("WARNING: requests library not available, using urllib", file=sys.stderr)
        print("  urllib doesn't easily support multipart/form-data", file=sys.stderr)
        print("  Please install requests: pip install requests", file=sys.stderr)
        # Could implement urllib multipart, but requests is cleaner
        raise ImportError("requests library required for REST OTA capture")

def run_ota_rest_invalid_token(host, token):
    """Scenario 2: ota_rest_invalid_token - POST /api/v1/ota/upload with invalid/missing token"""
    print("Executing ota_rest_invalid_token scenario...", file=sys.stderr)
    
    url = f"http://{host}/api/v1/ota/upload"
    firmware_data = create_fake_firmware_binary(10000)  # Small chunk for invalid token test
    
    if HAS_REQUESTS:
        try:
            files = {'update': ('firmware.bin', firmware_data, 'application/octet-stream')}
            # Use invalid token
            headers = {'X-OTA-Token': 'INVALID_TOKEN_12345'}
            
            print(f"POSTing with invalid token to {url}...", file=sys.stderr)
            response = requests.post(url, files=files, headers=headers, timeout=10)
            print(f"Response status: {response.status_code} (expected 401 or 403)", file=sys.stderr)
            print(f"Response: {response.text[:200]}", file=sys.stderr)
        except Exception as e:
            print(f"HTTP error: {e}", file=sys.stderr)
    else:
        raise ImportError("requests library required for REST OTA capture")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: capture_ota_rest_trace_direct.py <scenario> <output_dir> [host] [token]", file=sys.stderr)
        print("  scenario: ota_rest_happy_path | ota_rest_invalid_token", file=sys.stderr)
        print("  output_dir: directory for JSONL output", file=sys.stderr)
        print("  host: HTTP host (default: lightwaveos.local)", file=sys.stderr)
        print("  token: OTA token (default: LW-OTA-2024-SecureUpdate)", file=sys.stderr)
        sys.exit(1)
    
    scenario = sys.argv[1]
    output_dir = Path(sys.argv[2])
    host = sys.argv[3] if len(sys.argv) > 3 else "lightwaveos.local"
    token = sys.argv[4] if len(sys.argv) > 4 else "LW-OTA-2024-SecureUpdate"
    
    output_dir.mkdir(parents=True, exist_ok=True)
    raw_output = output_dir / f"{scenario}_raw.txt"
    jsonl_output = output_dir / f"{scenario}.jsonl"
    
    # Validate scenario name
    valid_scenarios = ["ota_rest_happy_path", "ota_rest_invalid_token"]
    if scenario not in valid_scenarios:
        print(f"ERROR: Invalid scenario '{scenario}'. Must be one of: {', '.join(valid_scenarios)}", file=sys.stderr)
        sys.exit(1)
    
    # Check for requests library
    if not HAS_REQUESTS:
        print("ERROR: requests library not found. Install with: pip install requests", file=sys.stderr)
        sys.exit(1)
    
    # Start serial capture in background thread
    print(f"Starting serial capture from /dev/cu.usbmodem1101 to {raw_output}", file=sys.stderr)
    lines_captured = [0]
    capture_error = [None]
    
    def capture_thread():
        try:
            # Longer duration for OTA (reboots take time)
            duration = 45 if scenario == "ota_rest_happy_path" else 20
            lines_captured[0] = capture_serial_direct('/dev/cu.usbmodem1101', str(raw_output), duration)
        except Exception as e:
            capture_error[0] = e
            print(f"Capture thread error: {e}", file=sys.stderr)
    
    thread = threading.Thread(target=capture_thread, daemon=False)
    thread.start()
    
    time.sleep(2)  # Let serial capture start
    
    # Run scenario
    print(f"Running scenario: {scenario}", file=sys.stderr)
    try:
        if scenario == "ota_rest_happy_path":
            run_ota_rest_happy_path(host, token)
        elif scenario == "ota_rest_invalid_token":
            run_ota_rest_invalid_token(host, token)
        
        # Note: happy_path will reboot device, so wait longer
        if scenario == "ota_rest_happy_path":
            time.sleep(5)  # Wait for reboot
        
    except Exception as e:
        print(f"HTTP error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
    
    time.sleep(2)  # Let events flush
    
    # Wait for capture thread to complete
    thread.join(timeout=60)
    
    if capture_error[0]:
        print(f"ERROR: Capture failed: {capture_error[0]}", file=sys.stderr)
        sys.exit(1)
    
    if not raw_output.exists() or raw_output.stat().st_size == 0:
        print(f"WARNING: Raw capture file is empty or not created: {raw_output}", file=sys.stderr)
        print(f"  This may indicate:", file=sys.stderr)
        print(f"  1. Device not connected at /dev/cu.usbmodem1101", file=sys.stderr)
        print(f"  2. Serial port in use by another process", file=sys.stderr)
        print(f"  3. Device not outputting telemetry events", file=sys.stderr)
        if lines_captured[0] == 0:
            print(f"  No lines were captured from serial port.", file=sys.stderr)
            sys.exit(1)
    
    print(f"Done. Captured {lines_captured[0]} lines to {raw_output}", file=sys.stderr)
    
    # Post-process with extract_jsonl.py
    print(f"Extracting JSONL events...", file=sys.stderr)
    script_dir = Path(__file__).parent
    extract_script = script_dir / "extract_jsonl.py"
    
    try:
        result = subprocess.run(
            [sys.executable, str(extract_script), str(raw_output)],
            capture_output=True,
            text=True,
            check=True
        )
        
        # Write extracted JSONL to output file
        with open(jsonl_output, 'w') as f:
            f.write(result.stdout)
        
        # Print extraction stats (from stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        
        print(f"JSONL extracted to {jsonl_output}", file=sys.stderr)
        
    except subprocess.CalledProcessError as e:
        print(f"ERROR: extract_jsonl.py failed: {e}", file=sys.stderr)
        if e.stderr:
            print(e.stderr, file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: Failed to run extract_jsonl.py: {e}", file=sys.stderr)
        sys.exit(1)
