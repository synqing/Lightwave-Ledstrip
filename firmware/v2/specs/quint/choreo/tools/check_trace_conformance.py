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


# Map invariant names to checker functions
INVARIANT_CHECKS = {
    "NoEarlyApply": check_no_early_apply,
    "HandshakeStrict": check_handshake_strict,
    "ConnEpochMonotonic": check_conn_epoch_monotonic,
    "EpochResetsHandshake": check_epoch_resets_handshake,
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
    
    # Check each state
    for state_idx, state_entry in enumerate(states):
        node_state = extract_node_state(state_entry)
        
        # Check each invariant
        for inv_name, check_func in INVARIANT_CHECKS.items():
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
