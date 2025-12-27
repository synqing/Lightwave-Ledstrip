#!/usr/bin/env python3
"""
LightwaveOS Web Server Benchmark Tool

Purpose:
- Run repeatable REST + WebSocket workloads to measure performance
- Collect latency distribution (p50/p95), error rates, payload sizes
- Emit JSON report for before/after comparison

Usage:
  python3 tools/web_benchmark.py --host lightwaveos.local --duration 30
  python3 tools/web_benchmark.py --host 192.168.1.123 --bench --output report.json
"""

import argparse
import json
import statistics
import sys
import time
import urllib.request
import urllib.error
import websocket
import threading
from collections import defaultdict
from typing import Dict, List, Optional

class BenchmarkStats:
    def __init__(self):
        self.http_latencies: List[float] = []
        self.ws_latencies: List[float] = []
        self.http_errors: int = 0
        self.ws_errors: int = 0
        self.http_requests: int = 0
        self.ws_messages: int = 0
        self.payload_sizes: List[int] = []
        self.start_time: Optional[float] = None
        self.end_time: Optional[float] = None
        
    def add_http_request(self, latency: float, success: bool, payload_size: int = 0):
        self.http_requests += 1
        if success:
            self.http_latencies.append(latency)
            if payload_size > 0:
                self.payload_sizes.append(payload_size)
        else:
            self.http_errors += 1
    
    def add_ws_message(self, latency: float, success: bool):
        self.ws_messages += 1
        if success:
            self.ws_latencies.append(latency)
        else:
            self.ws_errors += 1
    
    def get_summary(self) -> Dict:
        """Get summary statistics"""
        summary = {
            'http': {
                'total': self.http_requests,
                'errors': self.http_errors,
                'success_rate': (self.http_requests - self.http_errors) / max(self.http_requests, 1),
            },
            'ws': {
                'total': self.ws_messages,
                'errors': self.ws_errors,
                'success_rate': (self.ws_messages - self.ws_errors) / max(self.ws_messages, 1),
            },
            'duration': (self.end_time - self.start_time) if self.start_time and self.end_time else 0,
        }
        
        if self.http_latencies:
            summary['http']['p50'] = statistics.median(self.http_latencies)
            summary['http']['p95'] = self._percentile(self.http_latencies, 95)
            summary['http']['p99'] = self._percentile(self.http_latencies, 99)
            summary['http']['mean'] = statistics.mean(self.http_latencies)
            summary['http']['min'] = min(self.http_latencies)
            summary['http']['max'] = max(self.http_latencies)
        
        if self.ws_latencies:
            summary['ws']['p50'] = statistics.median(self.ws_latencies)
            summary['ws']['p95'] = self._percentile(self.ws_latencies, 95)
            summary['ws']['p99'] = self._percentile(self.ws_latencies, 99)
            summary['ws']['mean'] = statistics.mean(self.ws_latencies)
            summary['ws']['min'] = min(self.ws_latencies)
            summary['ws']['max'] = max(self.ws_latencies)
        
        if self.payload_sizes:
            summary['payload'] = {
                'mean': statistics.mean(self.payload_sizes),
                'min': min(self.payload_sizes),
                'max': max(self.payload_sizes),
            }
        
        return summary
    
    @staticmethod
    def _percentile(data: List[float], percentile: int) -> float:
        """Calculate percentile"""
        sorted_data = sorted(data)
        index = int(len(sorted_data) * percentile / 100)
        return sorted_data[min(index, len(sorted_data) - 1)]


def http_get(url: str, timeout: float = 5.0) -> tuple[dict, float, bool, int]:
    """Perform HTTP GET request and measure latency"""
    start = time.time()
    try:
        req = urllib.request.Request(url, method="GET", headers={"Accept": "application/json"})
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            data = resp.read()
            latency = time.time() - start
            payload_size = len(data)
            obj = json.loads(data.decode("utf-8"))
            return obj, latency, True, payload_size
    except Exception as e:
        latency = time.time() - start
        return {}, latency, False, 0


def ws_send_command(ws, command: dict, timeout: float = 2.0) -> tuple[dict, float, bool]:
    """Send WebSocket command and wait for response"""
    start = time.time()
    response_received = threading.Event()
    response_data = {}
    
    def on_message(ws, message):
        nonlocal response_data
        try:
            response_data = json.loads(message)
        except:
            pass
        response_received.set()
    
    ws.on_message = on_message
    
    try:
        ws.send(json.dumps(command))
        if response_received.wait(timeout):
            latency = time.time() - start
            return response_data, latency, True
        else:
            latency = time.time() - start
            return {}, latency, False
    except Exception as e:
        latency = time.time() - start
        return {}, latency, False


def run_http_workload(base_url: str, duration: float, stats: BenchmarkStats):
    """Run HTTP workload for specified duration"""
    endpoints = [
        "/api/v1/",
        "/api/v1/device/status",
        "/api/v1/device/info",
        "/api/v1/effects",
        "/api/v1/effects/current",
        "/api/v1/parameters",
        "/api/v1/palettes",
        "/api/v1/palettes/current",
    ]
    
    end_time = time.time() + duration
    request_count = 0
    
    while time.time() < end_time:
        for endpoint in endpoints:
            url = base_url + endpoint
            obj, latency, success, payload_size = http_get(url)
            stats.add_http_request(latency, success, payload_size)
            request_count += 1
            
            if time.time() >= end_time:
                break
            
            # Small delay to avoid overwhelming the server
            time.sleep(0.1)
    
    return request_count


def run_ws_workload(ws_url: str, duration: float, stats: BenchmarkStats):
    """Run WebSocket workload for specified duration"""
    commands = [
        {"type": "device.getStatus", "requestId": "req1"},
        {"type": "effects.getCurrent", "requestId": "req2"},
        {"type": "parameters.get", "requestId": "req3"},
        {"type": "effects.list", "requestId": "req4", "page": 1, "limit": 20},
        {"type": "palettes.list", "requestId": "req5", "page": 1, "limit": 20},
    ]
    
    try:
        ws = websocket.WebSocket()
        ws.connect(ws_url)
        
        end_time = time.time() + duration
        message_count = 0
        
        while time.time() < end_time:
            for cmd in commands:
                cmd_copy = cmd.copy()
                cmd_copy["requestId"] = f"req_{message_count}_{int(time.time() * 1000)}"
                
                resp, latency, success = ws_send_command(ws, cmd_copy)
                stats.add_ws_message(latency, success)
                message_count += 1
                
                if time.time() >= end_time:
                    break
                
                time.sleep(0.2)  # Delay between commands
        
        ws.close()
        return message_count
    except Exception as e:
        print(f"WebSocket error: {e}", file=sys.stderr)
        return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="LightwaveOS Web Server Benchmark")
    ap.add_argument("--host", required=True, help="Device host or IP")
    ap.add_argument("--port", type=int, default=80, help="HTTP port (default: 80)")
    ap.add_argument("--duration", type=float, default=30.0, help="Benchmark duration in seconds (default: 30)")
    ap.add_argument("--bench", action="store_true", help="Run full benchmark (HTTP + WS)")
    ap.add_argument("--http-only", action="store_true", help="Run HTTP workload only")
    ap.add_argument("--ws-only", action="store_true", help="Run WebSocket workload only")
    ap.add_argument("--output", help="Output JSON report file")
    args = ap.parse_args()
    
    base_url = f"http://{args.host}:{args.port}"
    ws_url = f"ws://{args.host}:{args.port}/ws"
    
    stats = BenchmarkStats()
    stats.start_time = time.time()
    
    print("=" * 70)
    print("LightwaveOS Web Server Benchmark")
    print("=" * 70)
    print(f"Target: {args.host}:{args.port}")
    print(f"Duration: {args.duration}s")
    print()
    
    # Run workloads
    if args.bench or not (args.http_only or args.ws_only):
        # Run both by default
        print("Running HTTP workload...")
        http_count = run_http_workload(base_url, args.duration / 2, stats)
        print(f"  Completed {http_count} HTTP requests")
        
        print("Running WebSocket workload...")
        ws_count = run_ws_workload(ws_url, args.duration / 2, stats)
        print(f"  Completed {ws_count} WebSocket messages")
    elif args.http_only:
        print("Running HTTP workload...")
        http_count = run_http_workload(base_url, args.duration, stats)
        print(f"  Completed {http_count} HTTP requests")
    elif args.ws_only:
        print("Running WebSocket workload...")
        ws_count = run_ws_workload(ws_url, args.duration, stats)
        print(f"  Completed {ws_count} WebSocket messages")
    
    stats.end_time = time.time()
    
    # Print summary
    summary = stats.get_summary()
    print()
    print("=" * 70)
    print("Benchmark Results")
    print("=" * 70)
    
    if 'http' in summary and summary['http']['total'] > 0:
        http = summary['http']
        print(f"HTTP Requests:")
        print(f"  Total: {http['total']}")
        print(f"  Errors: {http['errors']}")
        print(f"  Success Rate: {http['success_rate']:.1%}")
        if 'p50' in http:
            print(f"  Latency (ms): p50={http['p50']*1000:.1f}, p95={http['p95']*1000:.1f}, p99={http['p99']*1000:.1f}")
            print(f"  Mean: {http['mean']*1000:.1f}ms, Min: {http['min']*1000:.1f}ms, Max: {http['max']*1000:.1f}ms")
    
    if 'ws' in summary and summary['ws']['total'] > 0:
        ws = summary['ws']
        print(f"WebSocket Messages:")
        print(f"  Total: {ws['total']}")
        print(f"  Errors: {ws['errors']}")
        print(f"  Success Rate: {ws['success_rate']:.1%}")
        if 'p50' in ws:
            print(f"  Latency (ms): p50={ws['p50']*1000:.1f}, p95={ws['p95']*1000:.1f}, p99={ws['p99']*1000:.1f}")
            print(f"  Mean: {ws['mean']*1000:.1f}ms, Min: {ws['min']*1000:.1f}ms, Max: {ws['max']*1000:.1f}ms")
    
    if 'payload' in summary:
        payload = summary['payload']
        print(f"Payload Sizes:")
        print(f"  Mean: {payload['mean']:.0f} bytes")
        print(f"  Min: {payload['min']} bytes, Max: {payload['max']} bytes")
    
    print(f"Total Duration: {summary['duration']:.2f}s")
    print()
    
    # Save JSON report
    report = {
        'timestamp': time.time(),
        'host': args.host,
        'port': args.port,
        'summary': summary,
        'raw': {
            'http_latencies': stats.http_latencies,
            'ws_latencies': stats.ws_latencies,
            'payload_sizes': stats.payload_sizes,
        }
    }
    
    if args.output:
        with open(args.output, 'w') as f:
            json.dump(report, f, indent=2)
        print(f"Report saved to: {args.output}")
    else:
        print("(Use --output to save JSON report)")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())

