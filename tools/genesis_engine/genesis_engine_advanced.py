#!/usr/bin/env python3
"""
Genesis Engine v3.0 Advanced - Professional Audio Analysis
Enhanced version with spectral flux, onset detection, and dynamic range analysis
"""

import json
import numpy as np
import librosa
import librosa.display
from datetime import datetime
from pathlib import Path
import argparse
from scipy import signal, stats
from typing import Dict, List, Tuple, Optional
import matplotlib.pyplot as plt
from dataclasses import dataclass
import warnings
warnings.filterwarnings('ignore')

@dataclass
class AnalysisConfig:
    """Configuration for analysis parameters"""
    # Frequency bands (Hz)
    bass_range: Tuple[float, float] = (20, 250)
    mid_range: Tuple[float, float] = (250, 4000) 
    high_range: Tuple[float, float] = (4000, 20000)
    
    # Analysis parameters
    sample_rate: int = 44100
    hop_length: int = 512  # Higher resolution
    n_fft: int = 2048
    
    # Smoothing and filtering
    smoothing_window: int = 11
    energy_threshold: float = 0.01
    
    # Intensity scaling
    bass_boost: float = 1.2
    mid_boost: float = 1.0
    high_boost: float = 0.8
    
    # Onset detection
    onset_threshold: float = 0.3
    onset_backtrack: bool = True


class AdvancedGenesisEngine:
    def __init__(self, config: Optional[AnalysisConfig] = None):
        self.config = config or AnalysisConfig()
        self.frame_rate = self.config.sample_rate / self.config.hop_length
        
    def analyze_file(self, audio_path: str, 
                    visualize: bool = False,
                    use_harmonic_separation: bool = True) -> Dict:
        """Enhanced analysis pipeline with multiple techniques"""
        
        print(f"🎵 Genesis Engine v3.0 Advanced")
        print(f"📁 Loading: {audio_path}")
        
        # Load audio with original sample rate first for tempo detection
        y_orig, sr_orig = librosa.load(audio_path, sr=None, mono=True)
        
        # Resample for analysis
        y = librosa.resample(y_orig, orig_sr=sr_orig, target_sr=self.config.sample_rate)
        sr = self.config.sample_rate
        duration_ms = int(len(y) / sr * 1000)
        
        print(f"✅ Loaded: {duration_ms/1000:.1f}s @ {sr}Hz")
        
        # Harmonic-percussive separation
        if use_harmonic_separation:
            print("🎼 Separating harmonic and percussive components...")
            y_harmonic, y_percussive = librosa.effects.hpss(y)
        else:
            y_harmonic = y
            y_percussive = y
        
        # Advanced beat tracking
        print("🥁 Advanced beat and tempo analysis...")
        tempo, beats, beat_strengths = self.detect_beats_advanced(y_percussive, sr)
        beat_times_ms = (beats * 1000).tolist()
        
        # Onset detection for transients
        print("⚡ Detecting onsets and transients...")
        onsets = self.detect_onsets(y_percussive, sr)
        
        # Spectral analysis
        print("🌈 Performing spectral analysis...")
        bass_curve = self.analyze_band_advanced(y, sr, self.config.bass_range, 
                                               boost=self.config.bass_boost)
        mid_curve = self.analyze_band_advanced(y_harmonic, sr, self.config.mid_range,
                                              boost=self.config.mid_boost)
        high_curve = self.analyze_band_advanced(y_harmonic, sr, self.config.high_range,
                                               boost=self.config.high_boost)
        
        # Dynamic range analysis
        print("📊 Analyzing dynamics...")
        dynamics = self.analyze_dynamics(y, sr)
        
        # Spectral features
        spectral_features = self.extract_spectral_features(y, sr)
        
        # Create enhanced Genesis Map
        genesis_map = {
            "metadata": {
                "filename": Path(audio_path).name,
                "analysis_engine": "Genesis_Engine_v3.0_Advanced",
                "timestamp_utc": datetime.utcnow().isoformat() + "Z",
                "version": "v3.0",
                "analysis_config": {
                    "sample_rate": self.config.sample_rate,
                    "hop_length": self.config.hop_length,
                    "harmonic_separation": use_harmonic_separation
                }
            },
            "global_metrics": {
                "duration_ms": duration_ms,
                "bpm": float(tempo),
                "average_dynamics": float(dynamics['average']),
                "dynamic_range": float(dynamics['range']),
                "spectral_centroid_avg": float(spectral_features['centroid_avg'])
            },
            "layers": {
                "rhythm": {
                    "beat_grid_ms": beat_times_ms,
                    "beat_strengths": beat_strengths.tolist() if isinstance(beat_strengths, np.ndarray) else beat_strengths,
                    "onset_times_ms": (onsets * 1000).tolist()
                },
                "frequency": {
                    "bass": bass_curve,
                    "mids": mid_curve, 
                    "highs": high_curve
                },
                "dynamics": dynamics,
                "spectral": spectral_features
            }
        }
        
        # Visualization
        if visualize:
            self.visualize_analysis(y, sr, genesis_map)
        
        return genesis_map
    
    def detect_beats_advanced(self, y: np.ndarray, sr: int) -> Tuple[float, np.ndarray, np.ndarray]:
        """Advanced beat tracking with confidence scores"""
        
        # Use multiple tempo estimation methods
        # Method 1: Standard beat tracker
        tempo1, beats1 = librosa.beat.beat_track(y=y, sr=sr, trim=False)
        
        # Method 2: Onset-based tempo
        onset_env = librosa.onset.onset_strength(y=y, sr=sr)
        tempo2 = librosa.beat.tempo(onset_envelope=onset_env, sr=sr)[0]
        
        # Method 3: Tempogram-based
        tempogram = librosa.feature.tempogram(onset_envelope=onset_env, sr=sr)
        tempo3 = librosa.beat.tempo(onset_envelope=onset_env, sr=sr, 
                                   prior=stats.uniform(60, 240))[0]
        
        # Weighted average tempo (handle numpy array returns)
        t1 = float(tempo1[0]) if isinstance(tempo1, np.ndarray) else float(tempo1)
        t2 = float(tempo2[0]) if isinstance(tempo2, np.ndarray) else float(tempo2)
        t3 = float(tempo3[0]) if isinstance(tempo3, np.ndarray) else float(tempo3)
        tempo = np.average([t1, t2, t3], weights=[0.5, 0.3, 0.2])
        
        # Get beat positions with dynamic programming
        beats = librosa.beat.beat_track(y=y, sr=sr, bpm=tempo, trim=False)[1]
        beat_times = librosa.frames_to_time(beats, sr=sr)
        
        # Calculate beat strengths
        beat_strengths = onset_env[beats]
        beat_strengths = beat_strengths / np.max(beat_strengths) if np.max(beat_strengths) > 0 else beat_strengths
        
        return tempo, beat_times, beat_strengths
    
    def detect_onsets(self, y: np.ndarray, sr: int) -> np.ndarray:
        """Detect note onsets and transients"""
        
        # Spectral flux onset detection
        onset_frames = librosa.onset.onset_detect(
            y=y, sr=sr,
            hop_length=self.config.hop_length,
            backtrack=self.config.onset_backtrack,
            pre_max=3,
            post_max=3,
            pre_avg=3,
            post_avg=5,
            delta=0.2,
            wait=10
        )
        
        onset_times = librosa.frames_to_time(onset_frames, sr=sr, 
                                            hop_length=self.config.hop_length)
        return onset_times
    
    def analyze_band_advanced(self, y: np.ndarray, sr: int, 
                            freq_range: Tuple[float, float],
                            boost: float = 1.0) -> List[Dict]:
        """Advanced frequency band analysis with multiple features"""
        
        # Compute STFT with window
        D = librosa.stft(y, n_fft=self.config.n_fft, 
                        hop_length=self.config.hop_length,
                        window='hann')
        
        # Get magnitude spectrogram
        S = np.abs(D)
        
        # Frequency bins
        freqs = librosa.fft_frequencies(sr=sr, n_fft=self.config.n_fft)
        freq_mask = (freqs >= freq_range[0]) & (freqs <= freq_range[1])
        
        # Extract band energy with perceptual weighting
        band_energy = np.sum(S[freq_mask, :] ** 2, axis=0)
        
        # Apply A-weighting for perceptual loudness
        a_weight = self.a_weighting(freqs[freq_mask])
        weighted_energy = np.sum((S[freq_mask, :] * a_weight[:, np.newaxis]) ** 2, axis=0)
        
        # Normalize and boost
        if weighted_energy.max() > 0:
            weighted_energy = weighted_energy / weighted_energy.max()
        weighted_energy *= boost
        
        # Adaptive smoothing based on energy variance
        energy_variance = np.var(weighted_energy)
        adaptive_window = int(self.config.smoothing_window * (1 + energy_variance))
        smoothed_energy = signal.savgol_filter(weighted_energy, 
                                             min(adaptive_window, len(weighted_energy)-1), 
                                             3)
        
        # Peak detection for accents
        peaks, properties = signal.find_peaks(smoothed_energy, 
                                            height=0.3,
                                            distance=sr//self.config.hop_length//10)
        
        # Convert to time series with interpolation
        times = librosa.frames_to_time(np.arange(len(smoothed_energy)), 
                                      sr=sr, hop_length=self.config.hop_length)
        
        # High-resolution sampling (every ~10ms for smoother visualization)
        sample_interval = 0.01  # 10ms
        sampled_points = []
        
        for t in np.arange(0, times[-1], sample_interval):
            # Cubic interpolation for smoother curves
            intensity = np.interp(t, times, smoothed_energy)
            
            # Clip to valid range
            intensity = np.clip(intensity, 0, 1)
            
            sampled_points.append({
                "time_ms": float(t * 1000),
                "intensity": float(intensity)
            })
        
        return sampled_points
    
    def analyze_dynamics(self, y: np.ndarray, sr: int) -> Dict:
        """Analyze dynamic range and loudness variations"""
        
        # RMS energy
        rms = librosa.feature.rms(y=y, hop_length=self.config.hop_length)[0]
        
        # Dynamic range metrics
        dynamics = {
            "average": float(np.mean(rms)),
            "range": float(np.ptp(rms)),
            "variance": float(np.var(rms)),
            "rms_curve": []
        }
        
        # Sample RMS curve
        times = librosa.frames_to_time(np.arange(len(rms)), 
                                      sr=sr, hop_length=self.config.hop_length)
        
        for i in range(0, len(times), 10):  # Sample every 10 frames
            dynamics["rms_curve"].append({
                "time_ms": float(times[i] * 1000),
                "level": float(rms[i])
            })
        
        return dynamics
    
    def extract_spectral_features(self, y: np.ndarray, sr: int) -> Dict:
        """Extract additional spectral features for advanced effects"""
        
        # Spectral centroid (brightness)
        cent = librosa.feature.spectral_centroid(y=y, sr=sr, 
                                                hop_length=self.config.hop_length)[0]
        
        # Spectral rolloff
        rolloff = librosa.feature.spectral_rolloff(y=y, sr=sr,
                                                  hop_length=self.config.hop_length)[0]
        
        # Zero crossing rate (noisiness)
        zcr = librosa.feature.zero_crossing_rate(y, 
                                                hop_length=self.config.hop_length)[0]
        
        return {
            "centroid_avg": float(np.mean(cent)),
            "centroid_std": float(np.std(cent)),
            "rolloff_avg": float(np.mean(rolloff)),
            "zcr_avg": float(np.mean(zcr))
        }
    
    def a_weighting(self, frequencies: np.ndarray) -> np.ndarray:
        """Apply A-weighting to frequencies for perceptual loudness"""
        f = frequencies
        f2 = f ** 2
        
        num = 12194 ** 2 * f2 ** 2
        den = (f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194 ** 2)
        
        A = 2.0 + 20 * np.log10(num / den)
        A = 10 ** (A / 20)
        
        return A
    
    def visualize_analysis(self, y: np.ndarray, sr: int, genesis_map: Dict):
        """Create visualization of the analysis"""
        
        plt.figure(figsize=(14, 10))
        
        # Waveform
        plt.subplot(4, 1, 1)
        times = np.arange(len(y)) / sr
        plt.plot(times, y, alpha=0.6)
        plt.title('Waveform')
        plt.ylabel('Amplitude')
        
        # Frequency bands
        plt.subplot(4, 1, 2)
        for band_name, band_data in genesis_map['layers']['frequency'].items():
            if isinstance(band_data, list):
                times = [p['time_ms'] / 1000 for p in band_data]
                values = [p['intensity'] for p in band_data]
                plt.plot(times, values, label=band_name.capitalize(), alpha=0.8)
        plt.title('Frequency Band Intensities')
        plt.ylabel('Intensity')
        plt.legend()
        
        # Beat grid
        plt.subplot(4, 1, 3)
        beat_times = np.array(genesis_map['layers']['rhythm']['beat_grid_ms']) / 1000
        plt.vlines(beat_times, 0, 1, color='r', alpha=0.5, label='Beats')
        plt.title('Beat Grid')
        plt.ylabel('Beat')
        
        # Spectrogram
        plt.subplot(4, 1, 4)
        D = librosa.amplitude_to_db(np.abs(librosa.stft(y)), ref=np.max)
        librosa.display.specshow(D, y_axis='log', x_axis='time', sr=sr)
        plt.title('Spectrogram')
        plt.colorbar(format='%+2.0f dB')
        
        plt.tight_layout()
        plt.show()


def main():
    parser = argparse.ArgumentParser(description='Genesis Engine v3.0 Advanced')
    parser.add_argument('input', help='Input audio file')
    parser.add_argument('-o', '--output', help='Output JSON file', 
                       default='genesis_map_advanced.json')
    parser.add_argument('--visualize', action='store_true',
                       help='Show analysis visualization')
    parser.add_argument('--no-harmonic', action='store_true',
                       help='Disable harmonic separation')
    parser.add_argument('--config', help='JSON config file for parameters')
    
    args = parser.parse_args()
    
    # Load custom config if provided
    config = AnalysisConfig()
    if args.config:
        with open(args.config) as f:
            custom = json.load(f)
            for key, value in custom.items():
                if hasattr(config, key):
                    setattr(config, key, value)
    
    # Create engine
    engine = AdvancedGenesisEngine(config)
    
    # Analyze
    genesis_map = engine.analyze_file(
        args.input, 
        visualize=args.visualize,
        use_harmonic_separation=not args.no_harmonic
    )
    
    # Save
    with open(args.output, 'w') as f:
        json.dump(genesis_map, f, indent=2)
    
    print(f"✅ Advanced Genesis Map saved to: {args.output}")
    print(f"📊 File size: {Path(args.output).stat().st_size / 1024:.1f} KB")
    
    # Summary
    print(f"\n📈 Analysis Summary:")
    print(f"  BPM: {genesis_map['global_metrics']['bpm']:.1f}")
    print(f"  Beats: {len(genesis_map['layers']['rhythm']['beat_grid_ms'])}")
    print(f"  Duration: {genesis_map['global_metrics']['duration_ms']/1000:.1f}s")


if __name__ == "__main__":
    main()