#!/usr/bin/env python3
"""
Audio Pipeline Monitor - Shows real-time audio metrics
"""

import serial
import time
import sys

PORT = '/dev/cu.usbmodem1401'
BAUD = 115200

def main():
    print("AP_SOT Audio Monitor - First Principles Edition")
    print("=" * 60)
    print("Expecting FULL DYNAMIC RANGE (no AGC normalization)")
    print("=" * 60)
    
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
        print(f"Connected to {PORT} at {BAUD} baud")
        print("Waiting for data...\n")
        
        # Clear any buffered data
        ser.reset_input_buffer()
        time.sleep(0.5)
        
        beat_detected = False
        last_energy = 0.0
        energy_changes = []
        
        while True:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8').strip()
                    if line:
                        print(f"{time.strftime('%H:%M:%S')} | {line}")
                        
                        # Analyze the output
                        if "Energy:" in line:
                            try:
                                # Extract energy value
                                energy_str = line.split("Energy:")[1].split("|")[0].strip()
                                energy = float(energy_str)
                                
                                # Check if energy is varying (not constant)
                                if last_energy != 0.0:
                                    change = abs(energy - last_energy)
                                    if change > 0.1:
                                        energy_changes.append(change)
                                        if len(energy_changes) > 10:
                                            energy_changes.pop(0)
                                
                                last_energy = energy
                                
                                # Check for beat detection
                                if "BPM" in line and not beat_detected:
                                    bpm_str = line.split("(")[1].split("BPM")[0].strip()
                                    bpm = float(bpm_str)
                                    if bpm > 0:
                                        beat_detected = True
                                        print(f"\n🎵 BEAT DETECTION WORKING! BPM: {bpm}\n")
                                
                                # Periodic analysis
                                if len(energy_changes) >= 10:
                                    avg_change = sum(energy_changes) / len(energy_changes)
                                    if avg_change > 0.5:
                                        print(f"\n✓ DYNAMIC RANGE CONFIRMED - Avg energy change: {avg_change:.2f}\n")
                                        energy_changes.clear()
                                
                            except (ValueError, IndexError):
                                pass
                        
                        # Look for raw audio range debug output
                        if "Raw audio range:" in line:
                            print(f"\n📊 {line}\n")
                            
                except UnicodeDecodeError:
                    pass
            
            time.sleep(0.001)  # Small delay to prevent CPU spinning
            
    except serial.SerialException as e:
        print(f"Error: Could not open serial port {PORT}")
        print(f"Details: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nMonitoring stopped by user")
        ser.close()

if __name__ == "__main__":
    main()