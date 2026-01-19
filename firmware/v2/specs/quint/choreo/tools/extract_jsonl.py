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
    "telemetry.boot"
}

def is_jsonl_event(line):
    """Check if line is a JSONL telemetry event (handles ESP_LOG prefixes)"""
    # Find first '{' in line (handles ESP_LOG prefixes like "[12345][INFO]...")
    json_start = line.find('{')
    if json_start == -1:
        return False
    
    # Extract JSON portion
    json_str = line[json_start:]
    
    # Quick check for event field
    if '"event":"' not in json_str:
        return False
    
    # Try parsing as JSON to verify
    try:
        obj = json.loads(json_str.strip())
        event = obj.get("event")
        return event in TELEMETRY_EVENTS
    except (json.JSONDecodeError, AttributeError):
        return False

def extract_events(input_stream, output_stream):
    """Extract JSONL events from input stream, write to output stream"""
    count = 0
    
    for line in input_stream:
        # Remove ANSI escape codes if present
        line_clean = re.sub(r'\x1b\[[0-9;]*m', '', line)
        
        if is_jsonl_event(line_clean):
            # Find JSON portion (may have ESP_LOG prefix)
            json_start = line_clean.find('{')
            if json_start == -1:
                continue
            
            json_str = line_clean[json_start:]
            
            # Validate and output clean JSON
            try:
                event = json.loads(json_str.strip())
                json.dump(event, output_stream)
                output_stream.write('\n')
                count += 1
            except json.JSONDecodeError:
                # Invalid JSON - skip
                continue
    
    return count

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
        count = extract_events(input_file, sys.stdout)
        if input_file != sys.stdin:
            input_file.close()
        
        # Print stats to stderr (so stdout remains clean JSONL)
        print(f"Extracted {count} telemetry events", file=sys.stderr)
    except KeyboardInterrupt:
        print("\nInterrupted", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
