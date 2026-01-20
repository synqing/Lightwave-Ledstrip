#!/usr/bin/env python3
"""
JSONL Event Extractor

Filters telemetry events from serial output (stdin or file).
Outputs clean JSONL format for Choreo trace processing.

Accepts all telemetry events: msg.recv, msg.send, ws.connect, ws.connected, ws.disconnect, telemetry.boot

Usage:
    pio device monitor --raw | python3 extract_jsonl.py > trace.jsonl
    python3 extract_jsonl.py < serial_log.txt > trace.jsonl
"""

import json
import sys
import re
from pathlib import Path

# All supported telemetry event types
TELEMETRY_EVENTS = {
    "msg.recv", "msg.send",
    "ws.connect", "ws.connected", "ws.disconnect",
    "telemetry.boot",
    # OTA WebSocket events
    "ota.ws.begin", "ota.ws.chunk", "ota.ws.complete", "ota.ws.failed", "ota.ws.abort",
    # OTA REST events
    "ota.rest.begin", "ota.rest.progress", "ota.rest.complete", "ota.rest.failed"
}


def extract_events(input_stream, output_stream):
    """Extract JSONL events from input stream, write to output stream
    
    Returns:
        tuple: (count, parse_errors) where count is events extracted and parse_errors is count of JSON parse failures
    """
    count = 0
    parse_errors = 0
    
    for line in input_stream:
        # Remove ANSI escape codes if present
        line_clean = re.sub(r'\x1b\[[0-9;]*m', '', line)
        
        # Find JSON portion (may have ESP_LOG prefix)
        json_start = line_clean.find('{')
        if json_start == -1:
            continue
        
        json_str = line_clean[json_start:]
        
        # Quick check for event field (fast path)
        if '"event":"' not in json_str:
            continue
        
        # Try to parse as JSON (strict - no repair fallback)
        event = None
        try:
            event = json.loads(json_str.strip())
        except json.JSONDecodeError:
            # Invalid JSON - count as parse error and skip this line
            # Firmware must emit valid JSONL; we do not repair malformed telemetry
            parse_errors += 1
            continue
        
        # Check if it's a telemetry event
        if event and event.get('event') in TELEMETRY_EVENTS:
            json.dump(event, output_stream)
            output_stream.write('\n')
            count += 1
    
    return count, parse_errors

def main():
    input_file = sys.stdin
    
    # If file path provided as argument, read from file instead
    if len(sys.argv) > 1:
        input_path = Path(sys.argv[1])
        if not input_path.exists():
            print(f"Error: File not found: {input_path}", file=sys.stderr)
            sys.exit(1)
        input_file = open(input_path, 'r')
    
    try:
        count, parse_errors = extract_events(input_file, sys.stdout)
        if input_file != sys.stdin:
            input_file.close()
        
        # Print stats to stderr (so stdout remains clean JSONL)
        print(f"Extracted {count} telemetry events", file=sys.stderr)
        if parse_errors > 0:
            print(f"WARNING: {parse_errors} lines failed JSON parsing (skipped)", file=sys.stderr)
            # Fail if any parse errors - this is the "no extractor heroics" guardrail
            sys.exit(1)
    except KeyboardInterrupt:
        print("\nInterrupted", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
