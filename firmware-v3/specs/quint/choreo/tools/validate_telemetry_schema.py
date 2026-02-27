#!/usr/bin/env python3
"""
Telemetry Schema Validator

Validates JSONL telemetry traces against schema v1.0.0 (and future versions).
Checks required attributes per event type and enforces schema versioning.

Usage:
    python3 validate_telemetry_schema.py <path/to/trace.jsonl>
    python3 validate_telemetry_schema.py <path/to/trace.jsonl> --schema-version 1.0.0
"""

import json
import sys
import argparse
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple, Set


# Required attributes per event type (v1.0.0)
REQUIRED_ATTRS = {
    "ws.connect": {"event", "ts_mono_ms", "connEpoch", "eventSeq", "clientId", "schemaVersion"},
    "ws.connected": {"event", "ts_mono_ms", "connEpoch", "eventSeq", "clientId", "schemaVersion"},
    "ws.disconnect": {"event", "ts_mono_ms", "connEpoch", "eventSeq", "clientId", "schemaVersion"},
    "msg.recv": {"event", "ts_mono_ms", "connEpoch", "eventSeq", "clientId", "msgType", "result", "reason", "schemaVersion"},
    "msg.send": {"event", "ts_mono_ms", "connEpoch", "eventSeq", "clientId", "msgType", "schemaVersion"},
}

# Valid rejection reasons (v1.0.0)
VALID_REASONS = {"", "rate_limit", "size_limit", "parse_error", "auth_failed"}

# Valid event types (v1.0.0)
VALID_EVENTS = {"ws.connect", "ws.connected", "ws.disconnect", "msg.recv", "msg.send"}

# Supported schema versions
SUPPORTED_VERSIONS = {"1.0.0"}


def validate_event(event: Dict[str, Any], line_num: int, schema_version: Optional[str] = None) -> Tuple[bool, Optional[str]]:
    """Validate a single telemetry event against schema v1.0.0."""
    errors = []
    
    # Check event type
    event_type = event.get("event")
    if not event_type:
        return (False, f"Line {line_num}: Missing required field 'event'")
    
    if event_type not in VALID_EVENTS:
        return (False, f"Line {line_num}: Invalid event type '{event_type}' (valid: {', '.join(VALID_EVENTS)})")
    
    # Check schema version
    schema_ver = event.get("schemaVersion")
    if not schema_ver:
        return (False, f"Line {line_num}: Missing required field 'schemaVersion'")
    
    if schema_ver not in SUPPORTED_VERSIONS:
        return (False, f"Line {line_num}: Unsupported schema version '{schema_ver}' (supported: {', '.join(SUPPORTED_VERSIONS)})")
    
    if schema_version and schema_ver != schema_version:
        return (False, f"Line {line_num}: Schema version mismatch: expected '{schema_version}', got '{schema_ver}'")
    
    # Check required attributes for this event type
    required = REQUIRED_ATTRS.get(event_type, set())
    missing = required - set(event.keys())
    if missing:
        return (False, f"Line {line_num}: Missing required fields for '{event_type}': {', '.join(sorted(missing))}")
    
    # Validate msg.recv-specific fields
    if event_type == "msg.recv":
        result = event.get("result")
        reason = event.get("reason")
        
        if result not in {"ok", "rejected"}:
            return (False, f"Line {line_num}: Invalid 'result' value '{result}' (must be 'ok' or 'rejected')")
        
        if reason not in VALID_REASONS:
            return (False, f"Line {line_num}: Invalid 'reason' value '{reason}' (valid: {', '.join(sorted(VALID_REASONS))})")
        
        # If result is "ok", reason should be empty
        if result == "ok" and reason != "":
            return (False, f"Line {line_num}: 'reason' must be empty when 'result' is 'ok'")
        
        # If result is "rejected", reason should not be empty (unless explicitly allowed)
        if result == "rejected" and reason == "":
            return (False, f"Line {line_num}: 'reason' must not be empty when 'result' is 'rejected'")
    
    return (True, None)


def validate_telemetry_schema(jsonl_path: Path, schema_version: Optional[str] = None) -> Tuple[bool, List[str]]:
    """Validate JSONL telemetry trace against schema."""
    errors = []
    line_num = 0
    
    try:
        with open(jsonl_path) as f:
            for line in f:
                line_num += 1
                line = line.strip()
                if not line:
                    continue
                
                try:
                    event = json.loads(line)
                except json.JSONDecodeError as e:
                    errors.append(f"Line {line_num}: Invalid JSON: {e}")
                    continue
                
                valid, error = validate_event(event, line_num, schema_version)
                if not valid:
                    errors.append(error)
    
    except FileNotFoundError:
        return (False, [f"File not found: {jsonl_path}"])
    except Exception as e:
        return (False, [f"Error reading file: {e}"])
    
    return (len(errors) == 0, errors)


def main():
    parser = argparse.ArgumentParser(
        description="Validate JSONL telemetry traces against schema v1.0.0"
    )
    parser.add_argument(
        "jsonl_path",
        type=Path,
        help="Path to JSONL telemetry trace file"
    )
    parser.add_argument(
        "--schema-version",
        type=str,
        default=None,
        help="Required schema version (default: any supported version)"
    )
    
    args = parser.parse_args()
    
    if not args.jsonl_path.exists():
        print(f"ERROR: File not found: {args.jsonl_path}", file=sys.stderr)
        sys.exit(1)
    
    success, errors = validate_telemetry_schema(args.jsonl_path, args.schema_version)
    
    if success:
        print(f"PASS: Telemetry trace conforms to schema v1.0.0")
        sys.exit(0)
    else:
        print(f"FAIL: Schema validation errors:", file=sys.stderr)
        for error in errors[:50]:  # Limit output
            print(f"  {error}", file=sys.stderr)
        if len(errors) > 50:
            print(f"  ... and {len(errors) - 50} more errors", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
