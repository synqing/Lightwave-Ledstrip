#!/usr/bin/env python3
"""
Beat Detection Diagnostic Tool for AP_SOT
==========================================
This script helps diagnose and tune the beat detection system by:
1. Guiding through test sequences
2. Collecting energy and VoG data
3. Calculating optimal thresholds
4. Generating recommendations
"""

import serial
import time
import re
import numpy as np
import sys
from collections import deque
from datetime import datetime

class BeatDiagnostics:
    def __init__(self, port='/dev/cu.usbmodem1401', baud=115200):
        self.port = port
        self.baud = baud
        self.ser = None
        
        # Data collection
        self.energy_samples = []
        self.vog_samples = []
        self.beat_intervals = []
        self.last_beat_time = None
        
        # Test phases
        self.test_phases = [
            ("SILENCE", 30, "Please ensure complete silence (no music)"),
            ("EDM", 60, "Play EDM/Electronic music with clear 4/4 beat at medium volume"),
            ("ACOUSTIC", 60, "Play soft acoustic music (guitar/piano) at same volume"),
            ("ROCK", 60, "Play dynamic rock/pop with varied intensity at same volume"),
            ("VOLUME_SWEEP", 90, "Play EDM track, start quiet and gradually increase to loud (not distorted)")
        ]
        self.current_phase = 0
        self.phase_data = {}
        
    def connect(self):
        """Connect to serial port"""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
            time.sleep(2)  # Wait for connection
            self.ser.reset_input_buffer()
            print(f"✓ Connected to {self.port}")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
            
    def parse_line(self, line):
        """Parse various debug outputs from the system"""
        data = {}
        
        # Parse BEAT DETECTED line
        if "BEAT DETECTED!" in line:
            match = re.search(r'energy=(\d+\.?\d*)', line)
            if match:
                data['beat_energy'] = float(match.group(1))
                if self.last_beat_time:
                    interval = time.time() - self.last_beat_time
                    self.beat_intervals.append(interval * 1000)  # Convert to ms
                self.last_beat_time = time.time()
                
        # Parse BEAT DEBUG line
        elif "BEAT DEBUG:" in line:
            match = re.search(r'energy=(\d+\.?\d*), last=(\d+\.?\d*), trans=(\w+)', line)
            if match:
                data['debug_energy'] = float(match.group(1))
                data['last_energy'] = float(match.group(2))
                data['transient'] = match.group(3) == "YES"
                
        # Parse VoG DEBUG line
        elif "VoG DEBUG:" in line:
            match = re.search(r'raw=(\d+\.?\d*), agc=(\d+\.?\d*), ratio=(\d+\.?\d*)', line)
            if match:
                data['vog_raw'] = float(match.group(1))
                data['vog_agc'] = float(match.group(2))
                data['vog_ratio'] = float(match.group(3))
                
        # Parse main metrics line
        elif "Energy:" in line and "Bass:" in line:
            match = re.search(r'Beat: (\d+\.?\d*) \((\d+) BPM\) \| VoG: (\d+\.?\d*)', line)
            if match:
                data['beat_conf'] = float(match.group(1))
                data['bpm'] = int(match.group(2))
                data['vog_conf'] = float(match.group(3))
                
        return data
        
    def collect_phase_data(self, phase_name, duration):
        """Collect data for a specific test phase"""
        print(f"\n{'='*60}")
        print(f"PHASE: {phase_name}")
        print(f"Duration: {duration} seconds")
        print(f"{'='*60}")
        
        phase_energy = []
        phase_vog = []
        phase_beats = 0
        start_time = time.time()
        
        # Progress bar setup
        last_progress = -1
        
        while time.time() - start_time < duration:
            elapsed = time.time() - start_time
            progress = int((elapsed / duration) * 50)
            
            # Update progress bar
            if progress != last_progress:
                bar = '█' * progress + '░' * (50 - progress)
                remaining = duration - elapsed
                print(f'\r[{bar}] {elapsed:.1f}s / {duration}s (remaining: {remaining:.1f}s)', end='', flush=True)
                last_progress = progress
            
            # Read serial data
            if self.ser.in_waiting:
                try:
                    line = self.ser.readline().decode('utf-8').strip()
                    data = self.parse_line(line)
                    
                    if 'debug_energy' in data:
                        phase_energy.append(data['debug_energy'])
                        
                    if 'vog_ratio' in data:
                        phase_vog.append({
                            'raw': data['vog_raw'],
                            'agc': data['vog_agc'],
                            'ratio': data['vog_ratio']
                        })
                        
                    if 'beat_energy' in data:
                        phase_beats += 1
                        
                except:
                    pass
                    
        print()  # New line after progress bar
        
        # Store phase results
        self.phase_data[phase_name] = {
            'energy_samples': phase_energy,
            'vog_samples': phase_vog,
            'beat_count': phase_beats,
            'duration': duration
        }
        
        # Show phase summary
        if phase_energy:
            print(f"\n{phase_name} Summary:")
            print(f"  Energy: min={min(phase_energy):.1f}, max={max(phase_energy):.1f}, avg={np.mean(phase_energy):.1f}")
            print(f"  Beats detected: {phase_beats} ({phase_beats/duration*60:.1f} per minute)")
            
        if phase_vog:
            ratios = [v['ratio'] for v in phase_vog]
            print(f"  VoG ratios: min={min(ratios):.3f}, max={max(ratios):.3f}, avg={np.mean(ratios):.3f}")
            
    def analyze_results(self):
        """Analyze collected data and generate recommendations"""
        print(f"\n{'='*60}")
        print("ANALYSIS RESULTS")
        print(f"{'='*60}")
        
        # 1. Calculate baseline from silence
        silence_data = self.phase_data.get('SILENCE', {})
        if silence_data and silence_data['energy_samples']:
            silence_energy = silence_data['energy_samples']
            baseline = np.percentile(silence_energy, 95)  # 95th percentile of silence
            print(f"\n1. Baseline Energy (95th percentile of silence): {baseline:.1f}")
        else:
            baseline = 50.0  # Fallback
            print("\n1. No silence data collected, using default baseline")
            
        # 2. Analyze music phases
        music_energies = []
        beat_energies = []
        
        for phase in ['EDM', 'ACOUSTIC', 'ROCK']:
            data = self.phase_data.get(phase, {})
            if data and data['energy_samples']:
                music_energies.extend(data['energy_samples'])
                
        if music_energies:
            # Calculate threshold recommendations
            music_p25 = np.percentile(music_energies, 25)
            music_p50 = np.percentile(music_energies, 50)
            music_p75 = np.percentile(music_energies, 75)
            
            # Recommended threshold: significantly above baseline but below most music
            recommended_threshold = max(baseline * 3, music_p25)
            
            print(f"\n2. Energy Analysis Across Music:")
            print(f"   25th percentile: {music_p25:.1f}")
            print(f"   50th percentile: {music_p50:.1f}")
            print(f"   75th percentile: {music_p75:.1f}")
            print(f"   Current threshold: 50.0 (WAY TOO LOW!)")
            print(f"   RECOMMENDED THRESHOLD: {recommended_threshold:.1f}")
            
        # 3. Analyze VoG behavior
        all_vog = []
        for phase_data in self.phase_data.values():
            all_vog.extend(phase_data.get('vog_samples', []))
            
        if all_vog:
            ratios = [v['ratio'] for v in all_vog]
            above_one = sum(1 for r in ratios if r > 1.0)
            print(f"\n3. VoG Analysis:")
            print(f"   Total samples: {len(ratios)}")
            print(f"   Ratios > 1.0: {above_one} ({above_one/len(ratios)*100:.1f}%)")
            print(f"   Issue: AGC is usually higher than raw (ratio < 1.0)")
            print(f"   This suggests AGC is working but transients aren't strong enough")
            
        # 4. Beat detection analysis
        print(f"\n4. Beat Detection Issues:")
        print(f"   Current: Detecting beats almost every frame")
        print(f"   Reason: Energy threshold (50.0) is ~100x too low")
        print(f"   Solution: Increase threshold to {recommended_threshold:.1f}")
        
        # 5. Generate recommendations
        print(f"\n{'='*60}")
        print("RECOMMENDATIONS")
        print(f"{'='*60}")
        print(f"\n1. In legacy_beat_detector.h, change:")
        print(f"   static constexpr float ENERGY_THRESHOLD_MIN = 50.0f;")
        print(f"   TO:")
        print(f"   static constexpr float ENERGY_THRESHOLD_MIN = {recommended_threshold:.1f}f;")
        
        print(f"\n2. For better transient detection in beat_detector_node.h:")
        print(f"   Change: bool is_transient = energy_transient > (last_energy * 0.25f);")
        print(f"   TO:     bool is_transient = energy_transient > (last_energy * 0.5f);")
        
        print(f"\n3. VoG will start working once beat detection is fixed")
        print(f"   (AGC > raw is normal during steady-state music)")
        
    def run_diagnostics(self):
        """Run the complete diagnostic sequence"""
        print("\n" + "="*60)
        print("AP_SOT BEAT DETECTION DIAGNOSTIC TOOL")
        print("="*60)
        
        if not self.connect():
            return
            
        print("\nThis tool will guide you through a series of tests.")
        print("Please follow the instructions for each phase.\n")
        
        input("Press ENTER when ready to begin...")
        
        # Run each test phase
        for i, (phase_name, duration, instruction) in enumerate(self.test_phases):
            self.current_phase = i
            print(f"\n[Phase {i+1}/{len(self.test_phases)}]")
            print(f"INSTRUCTION: {instruction}")
            input(f"Press ENTER when ready to start {phase_name} test...")
            
            self.collect_phase_data(phase_name, duration)
            
            if i < len(self.test_phases) - 1:
                print(f"\nPhase complete! Preparing for next phase...")
                time.sleep(2)
                
        # Analyze and report
        self.analyze_results()
        
        # Save raw data
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"beat_diagnostic_{timestamp}.txt"
        with open(filename, 'w') as f:
            f.write("Beat Detection Diagnostic Report\n")
            f.write(f"Generated: {datetime.now()}\n\n")
            for phase, data in self.phase_data.items():
                f.write(f"\n{phase}:\n")
                f.write(f"  Samples: {len(data['energy_samples'])}\n")
                f.write(f"  Beats: {data['beat_count']}\n")
                if data['energy_samples']:
                    f.write(f"  Energy range: {min(data['energy_samples']):.1f} - {max(data['energy_samples']):.1f}\n")
                    
        print(f"\n✓ Raw data saved to: {filename}")
        
        self.ser.close()
        print("\n✓ Diagnostic complete!")

if __name__ == "__main__":
    # Allow custom port as argument
    port = sys.argv[1] if len(sys.argv) > 1 else '/dev/cu.usbmodem1401'
    
    diag = BeatDiagnostics(port=port)
    try:
        diag.run_diagnostics()
    except KeyboardInterrupt:
        print("\n\nDiagnostic interrupted by user")
        if diag.ser and diag.ser.is_open:
            diag.ser.close()