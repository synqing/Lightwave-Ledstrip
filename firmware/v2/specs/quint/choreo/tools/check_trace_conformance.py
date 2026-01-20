#!/usr/bin/env python3
"""
ITF Trace Conformance Checker

Validates that all states in an ITF trace satisfy the Quint invariants
defined in lightwave_properties.qnt.

Invariants checked:
- NoEarlyApply: handshakeComplete or lastAppliedParams is empty
- HandshakeStrict: CONNECTED requires handshakeComplete
- ConnEpochMonotonic: connEpoch >= 0
- EpochResetsHandshake: If not CONNECTED, handshake must be false
- NoOtaBeforeHandshake: OTA state transitions require handshake complete

Usage:
    python3 check_trace_conformance.py <path/to/itf.json>
    python3 check_trace_conformance.py --expect-violation <path/to/itf.json>
"""

import json
import sys
import argparse
from pathlib import Path
from typing import Any, Dict, List, Tuple, Optional


def extract_bigint(value: Any) -> int:
    """Extract integer value from #bigint wrapper or raw value."""
    if isinstance(value, dict) and set(value.keys()) == {"#bigint"}:
        return int(value["#bigint"])
    if isinstance(value, int):
        return value
    raise ValueError(f"Cannot extract int from {value!r}")


def extract_node_state(state_obj: Dict[str, Any]) -> Dict[str, Any]:
    """Extract node state fields from ITF state object."""
    node_state = state_obj.get("state", {}).get("node", {})
    
    return {
        "connState": node_state.get("connState", "DISCONNECTED"),
        "connEpoch": extract_bigint(node_state.get("connEpoch", {"#bigint": "0"})),
        "handshakeComplete": node_state.get("handshakeComplete", False),
        "lastAppliedParams": node_state.get("lastAppliedParams", {}),
    }


def extract_hub_state(state_obj: Dict[str, Any]) -> Dict[str, Any]:
    """Extract hub state fields from ITF state object (including OTA state)."""
    hub_state = state_obj.get("state", {}).get("hub", {})
    
    ota_state = hub_state.get("otaState", "Idle")
    ota_total_size = extract_bigint(hub_state.get("otaTotalSize", {"#bigint": "0"}))
    ota_bytes_received = extract_bigint(hub_state.get("otaBytesReceived", {"#bigint": "0"}))
    
    return {
        "otaState": ota_state if isinstance(ota_state, str) else str(ota_state),
        "otaTotalSize": ota_total_size,
        "otaBytesReceived": ota_bytes_received,
    }


def check_no_early_apply(node_state: Dict[str, Any]) -> bool:
    """Invariant: Node never applies params before handshake complete."""
    return node_state["handshakeComplete"] or len(node_state["lastAppliedParams"]) == 0


def check_handshake_strict(node_state: Dict[str, Any]) -> bool:
    """Invariant: CONNECTED requires handshakeComplete."""
    return node_state["connState"] != "CONNECTED" or node_state["handshakeComplete"]


def check_conn_epoch_monotonic(node_state: Dict[str, Any]) -> bool:
    """Invariant: connEpoch >= 0."""
    return node_state["connEpoch"] >= 0


def check_epoch_resets_handshake(node_state: Dict[str, Any]) -> bool:
    """Invariant: If not CONNECTED, handshake must be false."""
    return node_state["connState"] == "CONNECTED" or not node_state["handshakeComplete"]


def check_no_ota_before_handshake(node_state: Dict[str, Any], hub_state: Dict[str, Any]) -> bool:
    """Invariant: OTA state can only transition from Idle if handshake complete (for WS OTA).
    
    REST OTA doesn't require WebSocket handshake - exempted when connState is DISCONNECTED.
    """
    ota_state = hub_state.get("otaState", "Idle")
    
    # Allow Idle state regardless of handshake
    if ota_state == "Idle":
        return True
    
    # Allow Failed state regardless of handshake (failures can occur anytime)
    if isinstance(ota_state, str) and ota_state.startswith("Failed"):
        return True
    
    # REST OTA: If no WebSocket connection (DISCONNECTED), allow OTA without handshake
    # REST OTA uses HTTP POST with X-OTA-Token auth, not WebSocket handshake
    conn_state = node_state.get("connState", "DISCONNECTED")
    if conn_state == "DISCONNECTED":
        # REST OTA path - no handshake required
        return True
    
    # WebSocket OTA path: For InProgress, Verifying, Complete - handshake must be complete
    if ota_state in ("InProgress", "Verifying", "Complete"):
        return node_state["handshakeComplete"]
    
    # Unknown state - allow (shouldn't happen, but be permissive)
    return True


# Map invariant names to checker functions (node-only)
NODE_INVARIANT_CHECKS = {
    "NoEarlyApply": check_no_early_apply,
    "HandshakeStrict": check_handshake_strict,
    "ConnEpochMonotonic": check_conn_epoch_monotonic,
    "EpochResetsHandshake": check_epoch_resets_handshake,
}

# Map invariant names to checker functions (require both node and hub state)
JOINT_INVARIANT_CHECKS = {
    "NoOtaBeforeHandshake": check_no_ota_before_handshake,
}


def check_trace_conformance(itf_path: Path, expect_violation: bool = False) -> Tuple[bool, Optional[str]]:
    """Check all states in ITF trace against invariants.
    
    Returns:
        (success, error_message): success is True if trace conforms (or violates as expected),
                                 error_message is None if successful, otherwise describes the violation.
    """
    try:
        with open(itf_path) as f:
            doc = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        return (False, f"Failed to load ITF file: {e}")
    
    states = doc.get("states", [])
    if not states:
        return (False, "ITF file contains no states")
    
    # Track previous hub state for monotonic progress check
    prev_hub_state = None
    
    # Check each state
    for state_idx, state_entry in enumerate(states):
        node_state = extract_node_state(state_entry)
        hub_state = extract_hub_state(state_entry)
        
        # Check node-only invariants
        for inv_name, check_func in NODE_INVARIANT_CHECKS.items():
            if not check_func(node_state):
                error_msg = (
                    f"Invariant violation: {inv_name} failed at state {state_idx}\n"
                    f"  State: connState={node_state['connState']!r}, "
                    f"connEpoch={node_state['connEpoch']}, "
                    f"handshakeComplete={node_state['handshakeComplete']}, "
                    f"lastAppliedParams.keys()={list(node_state['lastAppliedParams'].keys())}"
                )
                
                if expect_violation:
                    # Violation was expected, this is success
                    return (True, None)
                else:
                    return (False, error_msg)
        
        # Check joint invariants (require both node and hub state)
        for inv_name, check_func in JOINT_INVARIANT_CHECKS.items():
            if not check_func(node_state, hub_state):
                error_msg = (
                    f"Invariant violation: {inv_name} failed at state {state_idx}\n"
                    f"  Node state: connState={node_state['connState']!r}, "
                    f"handshakeComplete={node_state['handshakeComplete']}\n"
                    f"  Hub state: otaState={hub_state['otaState']!r}, "
                    f"otaBytesReceived={hub_state['otaBytesReceived']}"
                )
                
                if expect_violation:
                    # Violation was expected, this is success
                    return (True, None)
                else:
                    return (False, error_msg)
        
        # Optional: Check monotonic progress across state sequence
        if prev_hub_state is not None:
            prev_ota_state = prev_hub_state.get("otaState", "Idle")
            curr_ota_state = hub_state.get("otaState", "Idle")
            
            # If both are InProgress, bytesReceived should not decrease
            if (prev_ota_state == "InProgress" and curr_ota_state == "InProgress"):
                prev_bytes = prev_hub_state.get("otaBytesReceived", 0)
                curr_bytes = hub_state.get("otaBytesReceived", 0)
                if curr_bytes < prev_bytes:
                    error_msg = (
                        f"Invariant violation: OtaMonotonicProgress failed at state {state_idx}\n"
                        f"  Previous: otaBytesReceived={prev_bytes}, "
                        f"Current: otaBytesReceived={curr_bytes}"
                    )
                    
                    if expect_violation:
                        return (True, None)
                    else:
                        return (False, error_msg)
        
        prev_hub_state = hub_state
    
    # All states passed all invariants
    if expect_violation:
        return (False, "Expected violation but trace conforms to all invariants")
    else:
        return (True, None)


def main():
    parser = argparse.ArgumentParser(
        description="Check ITF trace conformance against Quint invariants"
    )
    parser.add_argument(
        "itf_path",
        type=Path,
        help="Path to ITF trace file"
    )
    parser.add_argument(
        "--expect-violation",
        action="store_true",
        help="Expect this trace to violate invariants (for known-bad traces)"
    )
    
    args = parser.parse_args()
    
    if not args.itf_path.exists():
        print(f"ERROR: ITF file not found: {args.itf_path}", file=sys.stderr)
        sys.exit(1)
    
    success, error_msg = check_trace_conformance(args.itf_path, args.expect_violation)
    
    if success:
        if args.expect_violation:
            print(f"PASS: Trace violates invariants as expected")
        else:
            print(f"PASS: Trace conforms to all invariants")
        sys.exit(0)
    else:
        print(f"FAIL: {error_msg}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
