"""Benchmark runner module for executing dithering tests."""

import numpy as np
import time
import json
from pathlib import Path
from typing import Dict, List, Any
from dataclasses import dataclass, asdict

from dither_bench.pipelines import LWOSPipeline, SensoryBridgePipeline, EmotiscopePipeline
from dither_bench.stimuli import generators
from dither_bench.metrics import banding, temporal, accuracy


@dataclass
class TestResult:
    """Result from a single test run."""
    pipeline_name: str
    stimulus_name: str
    config: Dict[str, Any]
    metrics: Dict[str, Any]
    execution_time_ms: float
    

class BenchmarkRunner:
    """Orchestrates benchmark execution across all pipelines and stimuli."""
    
    def __init__(self, config):
        """
        Initialize benchmark runner.
        
        Args:
            config: BenchConfig instance
        """
        self.config = config
        self.results: List[TestResult] = []
    
    def run_all(self):
        """Execute complete benchmark suite."""
        print("=" * 80)
        print("DitherBench: Running Benchmark Suite")
        print("=" * 80)
        print()
        
        # Create pipelines
        pipelines = self._create_pipelines()
        
        # Generate stimuli
        stimuli = self._generate_stimuli()
        
        # Run test matrix
        total_tests = len(pipelines) * len(stimuli)
        test_count = 0
        
        for pipeline_name, pipeline in pipelines.items():
            for stimulus_name, stimulus_data in stimuli.items():
                test_count += 1
                print(f"[{test_count}/{total_tests}] {pipeline_name} × {stimulus_name}")
                
                result = self._run_single_test(
                    pipeline_name,
                    pipeline,
                    stimulus_name,
                    stimulus_data
                )
                
                self.results.append(result)
                
                # Brief result preview
                if 'banding_score' in result.metrics:
                    print(f"  → Banding: {result.metrics['banding_score']:.3f}, "
                          f"Stability: {result.metrics.get('stability_score', 0):.3f}")
        
        print()
        print(f"✓ Completed {total_tests} tests")
        print()
    
    def _create_pipelines(self) -> Dict[str, Any]:
        """Create pipeline instances for testing."""
        pipelines = {}
        
        # LWOS variants
        if self.config.enable_lwos_bayer:
            pipelines['LWOS (Bayer+Temporal)'] = LWOSPipeline(
                self.config.num_leds,
                {'gamma': 2.2, 'bayer_enabled': True, 'temporal_enabled': True}
            )
            
            pipelines['LWOS (Bayer only)'] = LWOSPipeline(
                self.config.num_leds,
                {'gamma': 2.2, 'bayer_enabled': True, 'temporal_enabled': False}
            )
            
            pipelines['LWOS (No dither)'] = LWOSPipeline(
                self.config.num_leds,
                {'gamma': 2.2, 'bayer_enabled': False, 'temporal_enabled': False}
            )
        
        # SensoryBridge
        if self.config.enable_sb_quantiser:
            pipelines['SensoryBridge'] = SensoryBridgePipeline(
                self.config.num_leds,
                {'brightness': 1.0, 'temporal_enabled': True}
            )
        
        # Emotiscope
        if self.config.enable_emo_quantiser:
            pipelines['Emotiscope'] = EmotiscopePipeline(
                self.config.num_leds,
                {'gamma': 1.5, 'temporal_enabled': True, 'seed': self.config.seed}
            )
        
        return pipelines
    
    def _generate_stimuli(self) -> Dict[str, np.ndarray]:
        """Generate test stimuli."""
        stimuli = {}
        
        for stimulus_name in self.config.stimuli:
            if stimulus_name == 'ramp':
                stimuli['Ramp (near-black)'] = generators.generate_near_black(
                    self.config.num_leds, max_value=0.2
                )
                stimuli['Ramp (full)'] = generators.generate_ramp(
                    self.config.num_leds
                )
            
            elif stimulus_name == 'constant':
                stimuli['Constant (mid)'] = generators.generate_constant_field(
                    self.config.num_leds, value=0.5
                )
            
            elif stimulus_name == 'gradient':
                stimuli['LGP Gradient'] = generators.generate_lgp_gradient(
                    self.config.num_leds
                )
            
            elif stimulus_name == 'palette_blend':
                stimuli['Palette Blend'] = generators.generate_palette_blend(
                    self.config.num_leds
                )
        
        return stimuli
    
    def _run_single_test(self, pipeline_name: str, pipeline: Any,
                        stimulus_name: str, stimulus: np.ndarray) -> TestResult:
        """Run single test configuration."""
        # Reset pipeline state
        pipeline.reset()
        
        # Generate frames
        frames = []
        start_time = time.time()
        
        for frame_idx in range(self.config.num_frames):
            # Process through pipeline
            output = pipeline.process_frame(stimulus)
            frames.append(output)
        
        execution_time_ms = (time.time() - start_time) * 1000.0
        
        # Convert to numpy array
        frames_array = np.array(frames, dtype=np.uint8)
        
        # Compute metrics
        metrics = {}
        
        # Banding (spatial)
        banding_metrics = banding.measure_banding(frames_array)
        metrics.update(banding_metrics)
        
        # Temporal stability (only if multiple frames)
        if self.config.num_frames > 1:
            temporal_metrics = temporal.measure_temporal_stability(frames_array)
            metrics.update(temporal_metrics)
        
        # Accuracy (vs reference)
        accuracy_metrics = accuracy.measure_accuracy(frames_array, stimulus)
        metrics.update(accuracy_metrics)
        
        return TestResult(
            pipeline_name=pipeline_name,
            stimulus_name=stimulus_name,
            config=pipeline.get_config(),
            metrics=metrics,
            execution_time_ms=execution_time_ms
        )
    
    def save_results(self, output_dir: Path):
        """Save results to CSV and JSON."""
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Save as JSON
        results_dict = [asdict(r) for r in self.results]
        
        json_path = output_dir / 'results.json'
        with open(json_path, 'w') as f:
            json.dump(results_dict, f, indent=2)
        
        print(f"Results saved to: {json_path}")
        
        # Save as CSV
        import csv
        csv_path = output_dir / 'results.csv'
        
        if self.results:
            with open(csv_path, 'w', newline='') as f:
                # Flatten metrics into CSV columns
                fieldnames = ['pipeline', 'stimulus', 'execution_time_ms']
                
                # Add metric columns
                first_result = self.results[0]
                metric_names = list(first_result.metrics.keys())
                fieldnames.extend(metric_names)
                
                writer = csv.DictWriter(f, fieldnames=fieldnames)
                writer.writeheader()
                
                for result in self.results:
                    row = {
                        'pipeline': result.pipeline_name,
                        'stimulus': result.stimulus_name,
                        'execution_time_ms': result.execution_time_ms,
                    }
                    row.update(result.metrics)
                    writer.writerow(row)
            
            print(f"CSV saved to: {csv_path}")
