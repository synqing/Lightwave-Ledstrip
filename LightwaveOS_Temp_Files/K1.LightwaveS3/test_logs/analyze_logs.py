#!/usr/bin/env python3
"""
Analyze device logs for Phase 0-5 validation
"""

import re
import sys
from collections import defaultdict

def analyze_logs(log_file):
    """Parse and analyze device logs"""
    
    # Metrics
    heartbeats = []
    audio_samples = []
    fps_readings = []
    bpm_readings = []
    tempo_classic = []
    vu_levels = []
    errors = []
    
    # Timing patterns
    timing_pattern = re.compile(r'avg_ms render/quantize/wait/tx: ([\d.]+) / ([\d.]+) / ([\d.]+) / ([\d.]+)')
    heartbeat_pattern = re.compile(r'\[HEARTBEAT\] frames=\+(\d+) audio=\+(\d+) snap=(\d+)\(\+(\d+)\) pat=(\d+) vu=([\d.]+) tempo=([\d.]+)')
    fps_pattern = re.compile(r'FPS: ([\d.]+)')
    bpm_pattern = re.compile(r'BPM: .*?(\d+\.?\d*)')
    tempo_pattern = re.compile(r'tempo classic bpm=([\d.]+) conf=([\d.]+)')
    vu_pattern = re.compile(r'VU: ([\d.]+)')
    error_pattern = re.compile(r'\[E\]|\[ERROR\]|error:|Error:|failed|Failed')
    
    with open(log_file, 'r') as f:
        for line in f:
            # Extract heartbeat data
            hb_match = heartbeat_pattern.search(line)
            if hb_match:
                heartbeats.append({
                    'frames': int(hb_match.group(1)),
                    'audio': int(hb_match.group(2)),
                    'snap': int(hb_match.group(3)),
                    'audio_increment': int(hb_match.group(4)),
                    'pattern': int(hb_match.group(5)),
                    'vu': float(hb_match.group(6)),
                    'tempo': float(hb_match.group(7))
                })
            
            # Extract timing data
            timing_match = timing_pattern.search(line)
            if timing_match:
                render_ms = float(timing_match.group(1))
                quantize_ms = float(timing_match.group(2))
                wait_ms = float(timing_match.group(3))
                tx_ms = float(timing_match.group(4))
                total_ms = render_ms + quantize_ms + wait_ms + tx_ms
                fps_readings.append({
                    'render': render_ms,
                    'quantize': quantize_ms,
                    'wait': wait_ms,
                    'tx': tx_ms,
                    'total': total_ms
                })
            
            # Extract FPS
            fps_match = fps_pattern.search(line)
            if fps_match:
                fps_readings.append(float(fps_match.group(1)))
            
            # Extract BPM
            bpm_match = bpm_pattern.search(line)
            if bpm_match:
                bpm_readings.append(float(bpm_match.group(1)))
            
            # Extract tempo classic
            tempo_match = tempo_pattern.search(line)
            if tempo_match:
                tempo_classic.append({
                    'bpm': float(tempo_match.group(1)),
                    'conf': float(tempo_match.group(2))
                })
            
            # Extract VU levels
            vu_match = vu_pattern.search(line)
            if vu_match:
                vu_levels.append(float(vu_match.group(1)))
            
            # Check for errors
            if error_pattern.search(line):
                errors.append(line.strip())
    
    return {
        'heartbeats': heartbeats,
        'fps_readings': fps_readings,
        'bpm_readings': bpm_readings,
        'tempo_classic': tempo_classic,
        'vu_levels': vu_levels,
        'errors': errors
    }

def print_report(metrics):
    """Generate validation report"""
    
    print("=" * 80)
    print("PHASE 0-5 VALIDATION REPORT")
    print("=" * 80)
    print()
    
    # Heartbeat analysis
    if metrics['heartbeats']:
        print("HEARTBEAT ANALYSIS:")
        print(f"  Total heartbeats captured: {len(metrics['heartbeats'])}")
        
        avg_frames = sum(h['frames'] for h in metrics['heartbeats']) / len(metrics['heartbeats'])
        avg_audio = sum(h['audio'] for h in metrics['heartbeats']) / len(metrics['heartbeats'])
        
        print(f"  Avg frames per second: {avg_frames:.1f}")
        print(f"  Avg audio samples per second: {avg_audio:.1f}")
        print()
    
    # FPS analysis
    if isinstance(metrics['fps_readings'], list) and metrics['fps_readings']:
        if isinstance(metrics['fps_readings'][0], dict):
            print("TIMING ANALYSIS:")
            for timing in metrics['fps_readings']:
                if isinstance(timing, dict):
                    print(f"  Render: {timing['render']:.2f}ms")
                    print(f"  Quantize: {timing['quantize']:.2f}ms")
                    print(f"  Total: {timing['total']:.2f}ms")
                    print()
    
    # VU levels
    if metrics['vu_levels']:
        avg_vu = sum(metrics['vu_levels']) / len(metrics['vu_levels'])
        min_vu = min(metrics['vu_levels'])
        max_vu = max(metrics['vu_levels'])
        print(f"VU METER ANALYSIS:")
        print(f"  Average VU: {avg_vu:.3f}")
        print(f"  Min VU: {min_vu:.3f}")
        print(f"  Max VU: {max_vu:.3f}")
        print()
    
    # Tempo analysis
    if metrics['tempo_classic']:
        print("TEMPO TRACKER ANALYSIS:")
        print(f"  Tempo updates captured: {len(metrics['tempo_classic'])}")
        for t in metrics['tempo_classic'][:5]:  # First 5
            print(f"    BPM: {t['bpm']:.1f}, Confidence: {t['conf']:.2f}")
        print()
    
    # BPM analysis
    if metrics['bpm_readings']:
        avg_bpm = sum(metrics['bpm_readings']) / len(metrics['bpm_readings'])
        print(f"BPM READINGS:")
        print(f"  Average BPM: {avg_bpm:.1f}")
        print(f"  BPM range: {min(metrics['bpm_readings']):.1f} - {max(metrics['bpm_readings']):.1f}")
        print()
    
    # Error analysis
    if metrics['errors']:
        print("ERRORS/WARNINGS:")
        for err in metrics['errors']:
            print(f"  {err}")
        print()
    
    # Success criteria checklist
    print("=" * 80)
    print("SUCCESS CRITERIA VALIDATION:")
    print("=" * 80)
    
    checks = []
    checks.append(("Device boot completed", len(metrics['heartbeats']) > 0))
    checks.append(("Audio processing active", len(metrics['heartbeats']) > 0 and 
                   any(h['audio'] > 0 for h in metrics['heartbeats'])))
    checks.append(("VU meter functional", len(metrics['vu_levels']) > 0))
    checks.append(("Tempo tracker active", len(metrics['tempo_classic']) > 0))
    checks.append(("BPM detection active", len(metrics['bpm_readings']) > 0))
    checks.append(("No critical errors", len([e for e in metrics['errors'] 
                   if 'Guru Meditation' in e or 'segfault' in e]) == 0))
    
    for check, passed in checks:
        status = "✅ PASS" if passed else "❌ FAIL"
        print(f"  {status}: {check}")
    
    print()
    print("=" * 80)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: analyze_logs.py <log_file>")
        sys.exit(1)
    
    log_file = sys.argv[1]
    metrics = analyze_logs(log_file)
    print_report(metrics)
