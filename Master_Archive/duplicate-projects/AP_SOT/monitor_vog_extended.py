#!/usr/bin/env python3
import serial
import time

# Configure serial connection
ser = serial.Serial('/dev/cu.usbmodem1401', 115200, timeout=1)
print("Connected to /dev/cu.usbmodem1401 at 115200 baud")
print("Monitoring VoG output (Extended)...")
print("Press Ctrl+C to exit\n")

# Clear any existing data
ser.reset_input_buffer()

# Track VoG values
vog_history = []
max_vog = 0.0

try:
    # Monitor for 60 seconds
    start_time = time.time()
    while time.time() - start_time < 60:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                # Highlight important lines
                if "VoG DEBUG" in line:
                    print(f">>> {line}")
                    # Extract VoG value
                    if "final=" in line:
                        try:
                            vog_val = float(line.split("final=")[1].split()[0])
                            vog_history.append(vog_val)
                            if vog_val > max_vog:
                                max_vog = vog_val
                                print(f"    NEW MAX VoG: {max_vog:.3f}")
                        except:
                            pass
                elif "VoG SPEAKS" in line:
                    print(f"*** {line} ***")
                elif "BEAT TIMEOUT" in line:
                    print(f"!!! {line} !!!")
                elif "Beat:" in line and "VoG:" in line:
                    # Extract VoG from status line
                    try:
                        vog_part = line.split("VoG:")[1].strip()
                        vog_val = float(vog_part)
                        if vog_val > 0:
                            print(f"==> {line} <== VoG ACTIVE!")
                        else:
                            print(f"==> {line}")
                    except:
                        print(f"==> {line}")
                else:
                    print(line)
        time.sleep(0.001)
        
except KeyboardInterrupt:
    print("\nMonitoring stopped by user")
finally:
    ser.close()
    print(f"\nVoG Statistics:")
    print(f"  Max VoG: {max_vog:.3f}")
    if vog_history:
        print(f"  Average VoG: {sum(vog_history)/len(vog_history):.3f}")
        print(f"  Non-zero readings: {sum(1 for v in vog_history if v > 0)}/{len(vog_history)}")
    print("Serial connection closed")