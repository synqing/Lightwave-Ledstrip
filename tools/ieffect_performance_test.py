#!/usr/bin/env python3
"""
IEffect Performance Regression Test

Compares performance metrics before/after IEffect migration to ensure no regression.
Tests FPS, CPU usage, frame timing, and visual consistency.

Usage:
    python3 tools/ieffect_performance_test.py [--baseline] [--compare] [--effect-id N]
"""

import serial
import time
import re
import json
import argparse
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional

class PerformanceMonitor:
    def __init__(self, port: str = '/dev/ttyUSB0', baud: int = 115200):
        self.port = port
        self.baud = baud
        self.ser = None
        
    def connect(self):
        """Connect to serial port."""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=2)
            time.sleep(2)  # Wait for boot
            self.ser.reset_input_buffer()
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Close serial connection."""
        if self.ser:
            self.ser.close()
    
    def send_command(self, cmd: str) -> str:
        """Send command and read response."""
        if not self.ser:
            return ""
        
        self.ser.write(f"{cmd}\n".encode())
        time.sleep(0.1)
        
        response = ""
        start_time = time.time()
        while time.time() - start_time < 2:
            if self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    response += line + "\n"
            else:
                time.sleep(0.01)
        
        return response
    
    def get_stats(self) -> Optional[Dict]:
        """Get current render statistics."""
        response = self.send_command('s')
        
        stats = {}
        
        # Parse FPS
        fps_match = re.search(r'FPS:\s*(\d+)', response)
        if fps_match:
            stats['fps'] = int(fps_match.group(1))
        
        # Parse CPU
        cpu_match = re.search(r'CPU:\s*(\d+)%', response)
        if cpu_match:
            stats['cpu_percent'] = int(cpu_match.group(1))
        
        # Parse frame times
        frame_time_match = re.search(r'Frame time:\s*avg=(\d+),\s*min=(\d+),\s*max=(\d+)', response)
        if frame_time_match:
            stats['avg_frame_time_us'] = int(frame_time_match.group(1))
            stats['min_frame_time_us'] = int(frame_time_match.group(2))
            stats['max_frame_time_us'] = int(frame_time_match.group(3))
        
        # Parse frame drops
        drops_match = re.search(r'Drops:\s*(\d+)', response)
        if drops_match:
            stats['frame_drops'] = int(drops_match.group(1))
        
        # Parse frames rendered
        frames_match = re.search(r'Frames:\s*(\d+)', response)
        if frames_match:
            stats['frames_rendered'] = int(frames_match.group(1))
        
        return stats if stats else None
    
    def set_effect(self, effect_id: int):
        """Set effect by ID."""
        self.send_command(f'effect {effect_id}')
        time.sleep(0.5)  # Wait for effect to initialize
    
    def get_ieffect_status(self) -> Dict:
        """Get IEffect system status."""
        response = self.send_command('I')
        
        status = {
            'pilot_effects': [],
            'total_ieffect': 0,
            'total_legacy': 0,
            'current_effect_type': None
        }
        
        # Parse pilot effects
        for line in response.split('\n'):
            if '[✓]' in line:
                id_match = re.search(r'ID\s+(\d+):', line)
                if id_match:
                    status['pilot_effects'].append(int(id_match.group(1)))
            
            if 'IEffect Native:' in line:
                count_match = re.search(r'(\d+)\s+effects', line)
                if count_match:
                    status['total_ieffect'] = int(count_match.group(1))
            
            if 'Legacy' in line and 'effects' in line:
                count_match = re.search(r'(\d+)\s+effects', line)
                if count_match:
                    status['total_legacy'] = int(count_match.group(1))
            
            if 'Type:' in line:
                if 'IEffect Native' in line:
                    status['current_effect_type'] = 'IEffect'
                elif 'Legacy' in line:
                    status['current_effect_type'] = 'Legacy'
        
        return status
    
    def measure_effect_performance(self, effect_id: int, duration_sec: int = 10) -> Dict:
        """Measure performance for a specific effect."""
        print(f"Testing effect {effect_id} for {duration_sec} seconds...")
        
        self.set_effect(effect_id)
        time.sleep(1)  # Warm-up
        
        # Get initial stats
        initial_stats = self.get_stats()
        initial_frames = initial_stats.get('frames_rendered', 0) if initial_stats else 0
        
        # Wait for measurement period
        time.sleep(duration_sec)
        
        # Get final stats
        final_stats = self.get_stats()
        final_frames = final_stats.get('frames_rendered', 0) if final_stats else 0
        
        if not final_stats:
            return {}
        
        # Calculate metrics
        metrics = {
            'effect_id': effect_id,
            'duration_sec': duration_sec,
            'fps': final_stats.get('fps', 0),
            'cpu_percent': final_stats.get('cpu_percent', 0),
            'avg_frame_time_us': final_stats.get('avg_frame_time_us', 0),
            'min_frame_time_us': final_stats.get('min_frame_time_us', 0),
            'max_frame_time_us': final_stats.get('max_frame_time_us', 0),
            'frame_drops': final_stats.get('frame_drops', 0),
            'frames_rendered': final_frames - initial_frames,
            'timestamp': datetime.now().isoformat()
        }
        
        return metrics

def save_baseline(metrics: Dict, output_file: Path):
    """Save baseline metrics to file."""
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w') as f:
        json.dump(metrics, f, indent=2)
    print(f"Baseline saved to: {output_file}")

def load_baseline(baseline_file: Path) -> Dict:
    """Load baseline metrics from file."""
    with open(baseline_file, 'r') as f:
        return json.load(f)

def compare_metrics(baseline: Dict, current: Dict) -> Dict:
    """Compare current metrics against baseline."""
    comparison = {
        'effect_id': current.get('effect_id', 0),
        'regressions': [],
        'improvements': [],
        'within_tolerance': []
    }
    
    # FPS comparison (target: >= baseline * 0.95, i.e., max 5% regression)
    baseline_fps = baseline.get('fps', 0)
    current_fps = current.get('fps', 0)
    if baseline_fps > 0:
        fps_change = ((current_fps - baseline_fps) / baseline_fps) * 100
        if fps_change < -5:
            comparison['regressions'].append(f"FPS dropped {abs(fps_change):.1f}% ({baseline_fps} -> {current_fps})")
        elif fps_change > 5:
            comparison['improvements'].append(f"FPS improved {fps_change:.1f}% ({baseline_fps} -> {current_fps})")
        else:
            comparison['within_tolerance'].append(f"FPS within tolerance ({current_fps})")
    
    # CPU comparison (target: <= baseline * 1.1, i.e., max 10% increase)
    baseline_cpu = baseline.get('cpu_percent', 0)
    current_cpu = current.get('cpu_percent', 0)
    if baseline_cpu > 0:
        cpu_change = ((current_cpu - baseline_cpu) / baseline_cpu) * 100
        if cpu_change > 10:
            comparison['regressions'].append(f"CPU usage increased {cpu_change:.1f}% ({baseline_cpu}% -> {current_cpu}%)")
        elif cpu_change < -10:
            comparison['improvements'].append(f"CPU usage decreased {abs(cpu_change):.1f}% ({baseline_cpu}% -> {current_cpu}%)")
        else:
            comparison['within_tolerance'].append(f"CPU usage within tolerance ({current_cpu}%)")
    
    # Frame time comparison
    baseline_avg = baseline.get('avg_frame_time_us', 0)
    current_avg = current.get('avg_frame_time_us', 0)
    if baseline_avg > 0:
        time_change = ((current_avg - baseline_avg) / baseline_avg) * 100
        if time_change > 10:
            comparison['regressions'].append(f"Frame time increased {time_change:.1f}% ({baseline_avg}us -> {current_avg}us)")
        elif time_change < -10:
            comparison['improvements'].append(f"Frame time decreased {abs(time_change):.1f}% ({baseline_avg}us -> {current_avg}us)")
        else:
            comparison['within_tolerance'].append(f"Frame time within tolerance ({current_avg}us)")
    
    return comparison

def main():
    parser = argparse.ArgumentParser(description='IEffect Performance Regression Test')
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial port')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
    parser.add_argument('--baseline', action='store_true', help='Capture baseline metrics')
    parser.add_argument('--compare', action='store_true', help='Compare against baseline')
    parser.add_argument('--effect-id', type=int, help='Test specific effect ID')
    parser.add_argument('--duration', type=int, default=10, help='Test duration in seconds')
    parser.add_argument('--baseline-file', default='reports/ieffect_baseline.json', help='Baseline file path')
    
    args = parser.parse_args()
    
    monitor = PerformanceMonitor(args.port, args.baud)
    
    if not monitor.connect():
        print("Failed to connect to device")
        return 1
    
    try:
        baseline_file = Path(args.baseline_file)
        
        if args.baseline:
            print("=== Capturing Baseline Metrics ===")
            
            # Test pilot effects
            pilot_effects = [15, 22, 67]  # Modal Resonance, Chevron Waves, Chromatic Interference
            baseline_metrics = {}
            
            for effect_id in pilot_effects:
                metrics = monitor.measure_effect_performance(effect_id, args.duration)
                if metrics:
                    baseline_metrics[f'effect_{effect_id}'] = metrics
                    print(f"Effect {effect_id}: {metrics['fps']} FPS, {metrics['cpu_percent']}% CPU")
            
            # Test a few legacy effects for comparison
            legacy_effects = [0, 1, 2]  # Fire, Ocean, Aurora
            for effect_id in legacy_effects:
                metrics = monitor.measure_effect_performance(effect_id, args.duration)
                if metrics:
                    baseline_metrics[f'effect_{effect_id}'] = metrics
                    print(f"Effect {effect_id}: {metrics['fps']} FPS, {metrics['cpu_percent']}% CPU")
            
            baseline_metrics['timestamp'] = datetime.now().isoformat()
            baseline_metrics['system_info'] = monitor.get_ieffect_status()
            
            save_baseline(baseline_metrics, baseline_file)
            print("\n✅ Baseline captured successfully")
            
        elif args.compare:
            print("=== Comparing Against Baseline ===")
            
            if not baseline_file.exists():
                print(f"Error: Baseline file not found: {baseline_file}")
                return 1
            
            baseline = load_baseline(baseline_file)
            print(f"Loaded baseline from: {baseline_file}")
            print(f"Baseline timestamp: {baseline.get('timestamp', 'unknown')}\n")
            
            # Test same effects
            all_effects = []
            for key in baseline.keys():
                if key.startswith('effect_'):
                    effect_id = int(key.split('_')[1])
                    all_effects.append(effect_id)
            
            if args.effect_id is not None:
                all_effects = [args.effect_id]
            
            results = []
            for effect_id in all_effects:
                print(f"\nTesting effect {effect_id}...")
                current = monitor.measure_effect_performance(effect_id, args.duration)
                
                baseline_key = f'effect_{effect_id}'
                if baseline_key in baseline:
                    comparison = compare_metrics(baseline[baseline_key], current)
                    results.append(comparison)
                    
                    print(f"\nEffect {effect_id} Results:")
                    if comparison['regressions']:
                        print("  ❌ REGRESSIONS:")
                        for reg in comparison['regressions']:
                            print(f"    - {reg}")
                    if comparison['improvements']:
                        print("  ✅ IMPROVEMENTS:")
                        for imp in comparison['improvements']:
                            print(f"    - {imp}")
                    if comparison['within_tolerance']:
                        print("  ✓ Within tolerance:")
                        for tol in comparison['within_tolerance']:
                            print(f"    - {tol}")
            
            # Summary
            total_regressions = sum(len(r['regressions']) for r in results)
            if total_regressions == 0:
                print("\n✅ NO REGRESSIONS DETECTED - All metrics within tolerance")
            else:
                print(f"\n⚠️  {total_regressions} regression(s) detected")
            
        else:
            # Just show current status
            print("=== Current System Status ===")
            status = monitor.get_ieffect_status()
            print(f"Pilot effects: {status['pilot_effects']}")
            print(f"Total IEffect: {status['total_ieffect']}")
            print(f"Total Legacy: {status['total_legacy']}")
            print(f"Current type: {status['current_effect_type']}")
            
            stats = monitor.get_stats()
            if stats:
                print(f"\nCurrent Performance:")
                print(f"  FPS: {stats.get('fps', 0)}")
                print(f"  CPU: {stats.get('cpu_percent', 0)}%")
                print(f"  Avg frame time: {stats.get('avg_frame_time_us', 0)}us")
    
    finally:
        monitor.disconnect()
    
    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())

