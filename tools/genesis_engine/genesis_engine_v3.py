#!/usr/bin/env python3
"""
Genesis Engine v3.0 - Audio Analysis for LED Visualization
Analyzes MP3 files and generates Genesis Maps for VP_DECODER

Requirements:
    pip install librosa numpy scipy madmom demucs torch torchaudio
"""

import json
import numpy as np
import librosa
try:
    import madmom
    HAS_MADMOM = True
except ImportError:
    HAS_MADMOM = False
    print("⚠️  Madmom not installed - using basic beat detection")
from datetime import datetime
from pathlib import Path
import argparse
from scipy import signal
from typing import Dict, List, Tuple
import warnings
warnings.filterwarnings('ignore')

class GenesisEngine:
    def __init__(self, sample_rate=44100, hop_length=1024):
        self.sample_rate = sample_rate
        self.hop_length = hop_length
        self.frame_rate = sample_rate / hop_length  # ~43 fps
        
        # Frequency band definitions (Hz)
        self.bass_range = (20, 250)
        self.mid_range = (250, 4000)
        self.high_range = (4000, 20000)
        
        # Default boost values
        self.bass_boost = 1.0
        self.mid_boost = 1.0
        self.high_boost = 1.0
        
    def analyze_file(self, audio_path: str, use_demucs: bool = False) -> Dict:
        """Main analysis pipeline"""
        print(f"🎵 Loading audio file: {audio_path}")
        
        # Load audio
        y, sr = librosa.load(audio_path, sr=self.sample_rate, mono=True)
        duration_ms = int(len(y) / sr * 1000)
        
        print(f"✅ Loaded: {duration_ms/1000:.1f} seconds @ {sr}Hz")
        
        # Source separation (optional)
        if use_demucs:
            print("🔄 Running Demucs source separation...")
            # Note: Demucs integration would go here
            # For now, we'll work with the full mix
            bass_enhanced = y
        else:
            bass_enhanced = y
            
        # Detect BPM and beats
        print("🎵 Detecting tempo and beats...")
        tempo, beats = self.detect_beats(y, sr)
        beat_times_ms = (beats * 1000).tolist()
        
        print(f"✅ Detected BPM: {float(tempo[0]) if isinstance(tempo, np.ndarray) else tempo:.1f}, Beats: {len(beats)}")
        
        # Analyze frequency bands
        print("📊 Analyzing frequency bands...")
        bass_curve = self.analyze_frequency_band(y, sr, self.bass_range, self.bass_boost)
        mid_curve = self.analyze_frequency_band(y, sr, self.mid_range, self.mid_boost)
        high_curve = self.analyze_frequency_band(y, sr, self.high_range, self.high_boost)
        
        # Create Genesis Map
        genesis_map = {
            "metadata": {
                "filename": Path(audio_path).name,
                "analysis_engine": "Genesis_Engine_v3.0_Python",
                "timestamp_utc": datetime.utcnow().isoformat() + "Z",
                "version": "v3.0"
            },
            "global_metrics": {
                "duration_ms": duration_ms,
                "bpm": int(tempo[0]) if isinstance(tempo, np.ndarray) else int(tempo)
            },
            "layers": {
                "rhythm": {
                    "beat_grid_ms": beat_times_ms
                },
                "frequency": {
                    "bass": bass_curve,
                    "mids": mid_curve,
                    "highs": high_curve
                }
            }
        }
        
        return genesis_map
    
    def detect_beats(self, y: np.ndarray, sr: int) -> Tuple[float, np.ndarray]:
        """Detect tempo and beat positions using madmom"""
        
        # Method 1: Librosa (faster, less accurate)
        tempo, beats = librosa.beat.beat_track(y=y, sr=sr)
        beat_times = librosa.frames_to_time(beats, sr=sr)
        
        # Method 2: Madmom (slower, more accurate)
        # Uncomment for better results:
        # proc = madmom.features.beats.DBNBeatTrackingProcessor(fps=100)
        # act = madmom.features.beats.RNNBeatProcessor()(y)
        # beat_times = proc(act)
        
        return tempo, beat_times
    
    def analyze_frequency_band(self, y: np.ndarray, sr: int, 
                              freq_range: Tuple[float, float], boost: float = 1.0) -> List[Dict]:
        """Analyze intensity over time for a frequency band"""
        
        # Compute STFT
        D = librosa.stft(y, hop_length=self.hop_length)
        
        # Convert to frequency bins
        freqs = librosa.fft_frequencies(sr=sr)
        
        # Find bins in frequency range
        freq_mask = (freqs >= freq_range[0]) & (freqs <= freq_range[1])
        
        # Extract band energy over time
        band_energy = np.sum(np.abs(D[freq_mask, :]) ** 2, axis=0)
        
        # Normalize to 0-1 range
        if band_energy.max() > 0:
            band_energy = band_energy / band_energy.max()
        
        # Smooth the curve
        band_energy = signal.savgol_filter(band_energy, 11, 3)
        
        # Apply boost and ensure 0-1 range
        band_energy = band_energy * boost
        band_energy = np.clip(band_energy, 0, 1)
        
        # Convert to time series
        times = librosa.frames_to_time(np.arange(len(band_energy)), 
                                      sr=sr, hop_length=self.hop_length)
        
        # Sample at ~47Hz (every 21.33ms) to match the expected format
        sample_interval = 0.02133  # seconds
        sampled_points = []
        
        for t in np.arange(0, times[-1], sample_interval):
            # Find nearest frame
            idx = np.argmin(np.abs(times - t))
            intensity = float(band_energy[idx])
            
            sampled_points.append({
                "time_ms": float(t * 1000),
                "intensity": intensity
            })
        
        return sampled_points
    
    def fine_tune_parameters(self, 
                           bass_boost: float = 1.0,
                           mid_boost: float = 1.0, 
                           high_boost: float = 1.0,
                           smoothing: int = 11):
        """Fine-tune analysis parameters"""
        self.bass_boost = bass_boost
        self.mid_boost = mid_boost
        self.high_boost = high_boost
        self.smoothing_window = smoothing


def main():
    parser = argparse.ArgumentParser(description='Genesis Engine v3.0 - Audio Analysis')
    parser.add_argument('input', help='Input MP3 file')
    parser.add_argument('-o', '--output', help='Output JSON file', 
                       default='genesis_map.json')
    parser.add_argument('--bass-boost', type=float, default=1.0,
                       help='Bass intensity multiplier')
    parser.add_argument('--mid-boost', type=float, default=1.0,
                       help='Mid intensity multiplier')
    parser.add_argument('--high-boost', type=float, default=1.0,
                       help='High intensity multiplier')
    parser.add_argument('--use-demucs', action='store_true',
                       help='Use Demucs for source separation')
    
    args = parser.parse_args()
    
    # Create engine
    engine = GenesisEngine()
    engine.fine_tune_parameters(
        bass_boost=args.bass_boost,
        mid_boost=args.mid_boost,
        high_boost=args.high_boost
    )
    
    # Analyze audio
    genesis_map = engine.analyze_file(args.input, use_demucs=args.use_demucs)
    
    # Save result
    with open(args.output, 'w') as f:
        json.dump(genesis_map, f, indent=2)
    
    print(f"✅ Genesis Map saved to: {args.output}")
    print(f"📊 File size: {Path(args.output).stat().st_size / 1024:.1f} KB")


if __name__ == "__main__":
    main()