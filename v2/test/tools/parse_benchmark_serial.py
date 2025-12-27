#!/usr/bin/env python3
"""
Parse benchmark results from serial output or test logs.
Extracts performance metrics for regression detection.
"""

import re
import json
import sys
from typing import Dict, List, Optional
from dataclasses import dataclass, asdict


@dataclass
class BenchmarkMetric:
    """Single benchmark metric result"""
    name: str
    value: float
    unit: str
    timestamp: Optional[str] = None


@dataclass
class BenchmarkResult:
    """Complete benchmark run results"""
    snr_db: float
    false_trigger_rate: float
    cpu_load_percent: float
    latency_ms: float
    memory_usage_kb: Optional[float] = None
    sample_rate_hz: Optional[int] = None
    buffer_size: Optional[int] = None

    def to_dict(self) -> dict:
        """Convert to dictionary, excluding None values"""
        return {k: v for k, v in asdict(self).items() if v is not None}


class BenchmarkParser:
    """Parse benchmark output from serial logs or test output"""

    # Regex patterns for benchmark output
    PATTERNS = {
        'snr': r'SNR:\s*([\d.]+)\s*dB',
        'false_triggers': r'False\s+Trigger\s+Rate:\s*([\d.]+)%',
        'cpu_load': r'CPU\s+Load:\s*([\d.]+)%',
        'latency': r'Latency:\s*([\d.]+)\s*ms',
        'memory': r'Memory\s+Usage:\s*([\d.]+)\s*KB',
        'sample_rate': r'Sample\s+Rate:\s*(\d+)\s*Hz',
        'buffer_size': r'Buffer\s+Size:\s*(\d+)',
    }

    # Alternative patterns for different output formats
    ALT_PATTERNS = {
        'snr': r'Signal-to-Noise\s+Ratio:\s*([\d.]+)',
        'false_triggers': r'False\s+Positives:\s*([\d.]+)%?',
        'cpu_load': r'CPU\s+Usage:\s*([\d.]+)%?',
        'latency': r'Processing\s+Latency:\s*([\d.]+)',
    }

    def __init__(self):
        self.metrics: List[BenchmarkMetric] = []

    def parse_line(self, line: str) -> Optional[BenchmarkMetric]:
        """Parse a single line for benchmark metrics"""
        line = line.strip()

        # Try primary patterns
        for metric_name, pattern in self.PATTERNS.items():
            match = re.search(pattern, line, re.IGNORECASE)
            if match:
                value = float(match.group(1))
                unit = self._get_unit(metric_name)
                return BenchmarkMetric(metric_name, value, unit)

        # Try alternative patterns
        for metric_name, pattern in self.ALT_PATTERNS.items():
            match = re.search(pattern, line, re.IGNORECASE)
            if match:
                value = float(match.group(1))
                unit = self._get_unit(metric_name)
                return BenchmarkMetric(metric_name, value, unit)

        return None

    def parse_text(self, text: str) -> Dict[str, float]:
        """Parse entire text block for all metrics"""
        results = {}

        for line in text.split('\n'):
            metric = self.parse_line(line)
            if metric:
                results[metric.name] = metric.value
                self.metrics.append(metric)

        return results

    def parse_file(self, filepath: str) -> Dict[str, float]:
        """Parse benchmark results from file"""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                text = f.read()
            return self.parse_text(text)
        except FileNotFoundError:
            print(f"Error: File not found: {filepath}", file=sys.stderr)
            return {}
        except Exception as e:
            print(f"Error parsing file {filepath}: {e}", file=sys.stderr)
            return {}

    def to_benchmark_result(self, metrics: Dict[str, float]) -> Optional[BenchmarkResult]:
        """Convert parsed metrics to BenchmarkResult object"""
        try:
            result = BenchmarkResult(
                snr_db=metrics.get('snr', 0.0),
                false_trigger_rate=metrics.get('false_triggers', 0.0),
                cpu_load_percent=metrics.get('cpu_load', 0.0),
                latency_ms=metrics.get('latency', 0.0),
                memory_usage_kb=metrics.get('memory'),
                sample_rate_hz=int(metrics['sample_rate']) if 'sample_rate' in metrics else None,
                buffer_size=int(metrics['buffer_size']) if 'buffer_size' in metrics else None,
            )
            return result
        except KeyError as e:
            print(f"Error: Missing required metric: {e}", file=sys.stderr)
            return None

    @staticmethod
    def _get_unit(metric_name: str) -> str:
        """Get unit for metric name"""
        units = {
            'snr': 'dB',
            'false_triggers': '%',
            'cpu_load': '%',
            'latency': 'ms',
            'memory': 'KB',
            'sample_rate': 'Hz',
            'buffer_size': 'samples',
        }
        return units.get(metric_name, '')


def parse_platformio_test_output(text: str) -> Dict[str, float]:
    """
    Parse PlatformIO native test output format.
    Handles Unity test framework output with embedded benchmark data.
    """
    parser = BenchmarkParser()

    # Extract test output section
    test_section = ""
    in_test = False

    for line in text.split('\n'):
        # Detect test start
        if 'test_pipeline_benchmark' in line or 'AudioPipelineBenchmark' in line:
            in_test = True

        if in_test:
            test_section += line + '\n'

        # Detect test end
        if in_test and ('OK' in line or 'FAIL' in line or '---' in line):
            if len(line.strip()) < 5:  # End marker
                in_test = False

    # Parse the test section
    return parser.parse_text(test_section if test_section else text)


def main():
    """CLI interface for parsing benchmark results"""
    import argparse

    parser = argparse.ArgumentParser(
        description='Parse audio benchmark results from serial/test output'
    )
    parser.add_argument(
        'input_file',
        help='Path to serial log or test output file'
    )
    parser.add_argument(
        '-o', '--output',
        help='Output JSON file path (default: stdout)'
    )
    parser.add_argument(
        '-f', '--format',
        choices=['json', 'summary'],
        default='json',
        help='Output format (default: json)'
    )
    parser.add_argument(
        '--platformio',
        action='store_true',
        help='Parse PlatformIO test output format'
    )

    args = parser.parse_args()

    # Read input
    try:
        with open(args.input_file, 'r', encoding='utf-8') as f:
            text = f.read()
    except Exception as e:
        print(f"Error reading input file: {e}", file=sys.stderr)
        sys.exit(1)

    # Parse based on format
    if args.platformio:
        metrics = parse_platformio_test_output(text)
    else:
        bench_parser = BenchmarkParser()
        metrics = bench_parser.parse_text(text)

    if not metrics:
        print("Warning: No benchmark metrics found in input", file=sys.stderr)
        sys.exit(1)

    # Convert to result object
    bench_parser = BenchmarkParser()
    result = bench_parser.to_benchmark_result(metrics)

    if not result:
        print("Error: Could not create benchmark result", file=sys.stderr)
        sys.exit(1)

    # Format output
    if args.format == 'json':
        output_data = result.to_dict()
        output_json = json.dumps(output_data, indent=2)

        if args.output:
            with open(args.output, 'w') as f:
                f.write(output_json)
            print(f"Results written to {args.output}")
        else:
            print(output_json)

    elif args.format == 'summary':
        print("\n=== Audio Benchmark Results ===")
        print(f"SNR:                  {result.snr_db:.2f} dB")
        print(f"False Trigger Rate:   {result.false_trigger_rate:.2f}%")
        print(f"CPU Load:             {result.cpu_load_percent:.2f}%")
        print(f"Latency:              {result.latency_ms:.2f} ms")
        if result.memory_usage_kb:
            print(f"Memory Usage:         {result.memory_usage_kb:.2f} KB")
        if result.sample_rate_hz:
            print(f"Sample Rate:          {result.sample_rate_hz} Hz")
        if result.buffer_size:
            print(f"Buffer Size:          {result.buffer_size} samples")
        print()

    sys.exit(0)


if __name__ == '__main__':
    main()
