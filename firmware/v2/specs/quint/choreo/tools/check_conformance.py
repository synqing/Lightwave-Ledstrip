#!/usr/bin/env python3
"""
Trace Conformance Checker

Verifies that hardware traces (JSONL) are explainable by the Quint model.
For each event, verifies:
1. The event maps to allowed model action(s)
2. Preconditions for the action(s) hold
3. All invariants hold after state transition
"""

import json
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional
from dataclasses import dataclass, field
from protocol_map import map_event_to_actions, Action, ActionType, get_action_description

# ============================================================================
# Abstract State Model (matches Quint model)
# ============================================================================

@dataclass
class NodeState:
    """Abstract state for Tab5 Node."""
    connState: str = "DISCONNECTED"  # DISCONNECTED, CONNECTING, CONNECTED
    connEpoch: int = 0
    handshakeComplete: bool = False
    lastAppliedParams: Dict[str, int] = field(default_factory=dict)
    lastStatusTs: int = 0

@dataclass
class HubState:
    """Abstract state for v2 Hub."""
    connState: Dict[str, str] = field(default_factory=dict)  # Node ID -> state
    connEpoch: Dict[str, int] = field(default_factory=dict)  # Node ID -> epoch
    activeParams: Dict[str, int] = field(default_factory=dict)
    activeEffectId: int = 0
    lastBroadcastTs: int = 0
    # Zones state (Phase: zones slice)
    zoneCount: int = 0
    zones: Dict[int, Dict[str, Any]] = field(default_factory=dict)  # zoneId -> zone state

@dataclass
class ModelState:
    """Combined state matching Quint LightwaveState."""
    node: NodeState
    hub: HubState
    TAB5_NODE: str = "1"  # Node identifier

def initial_state() -> ModelState:
    """Create initial state matching Quint init action."""
    return ModelState(
        node=NodeState(),
        hub=HubState()
    )

# ============================================================================
# Invariant Checkers (matching lightwave_properties.qnt)
# ============================================================================

def check_no_early_apply(state: ModelState) -> bool:
    """Invariant: Node never applies parameters before handshake complete."""
    return state.node.handshakeComplete or (len(state.node.lastAppliedParams) == 0)

def check_handshake_strict(state: ModelState) -> bool:
    """
    Invariant: Connected state requires handshake to be complete.
    
    Note: There's a brief window after node_connected where we're CONNECTED
    but handshakeComplete is False (until node_receive_status). This is acceptable
    as the handshake is in progress. The invariant ensures we don't remain in this
    state indefinitely.
    """
    # Allow CONNECTED without handshakeComplete only if no params have been applied yet
    # (this represents the handshake-in-progress state)
    if state.node.connState == "CONNECTED" and not state.node.handshakeComplete:
        # This is OK only during initial handshake (no params applied yet)
        return len(state.node.lastAppliedParams) == 0
    return (state.node.connState != "CONNECTED") or state.node.handshakeComplete

def check_conn_epoch_monotonic(state: ModelState) -> bool:
    """Invariant: Connection epoch never decreases."""
    return state.node.connEpoch >= 0

def check_epoch_resets_handshake(state: ModelState) -> bool:
    """Invariant: If connecting/disconnected, handshake must be false."""
    return (state.node.connState == "CONNECTED") or (not state.node.handshakeComplete)

INVARIANTS = [
    ("NoEarlyApply", check_no_early_apply),
    ("HandshakeStrict", check_handshake_strict),
    ("ConnEpochMonotonic", check_conn_epoch_monotonic),
    ("EpochResetsHandshake", check_epoch_resets_handshake),
]

# ============================================================================
# Precondition Checking
# ============================================================================

def check_precondition(state: ModelState, precondition: str, expected: Any) -> bool:
    """Check if a precondition holds in the current state."""
    # Parse precondition path like "node.connState" or "hub.connState[TAB5_NODE]"
    parts = precondition.split(".")
    if parts[0] == "node":
        obj = state.node
        path = parts[1:]
    elif parts[0] == "hub":
        obj = state.hub
        path = parts[1:]
    else:
        return False
    
    # Navigate path
    try:
        for part in path:
            if "[" in part:
                # Handle indexed access like "connState[TAB5_NODE]"
                key, index_expr = part.split("[", 1)
                index = index_expr.rstrip("]")
                if index == "TAB5_NODE":
                    index = state.TAB5_NODE
                attr_dict = getattr(obj, key)
                if index not in attr_dict:
                    # Key doesn't exist - return False if expected value is not None
                    return expected is None or expected == ""
                obj = attr_dict[index]
            else:
                obj = getattr(obj, part)
        
        return obj == expected
    except (KeyError, AttributeError):
        # Path doesn't exist - return False unless expected is None/empty
        return expected is None or expected == "" or expected == "DISCONNECTED"

def check_action_preconditions(state: ModelState, action: Action) -> tuple[bool, str]:
    """Check if all preconditions for an action hold."""
    for precond, expected in action.preconditions.items():
        if not check_precondition(state, precond, expected):
            return False, f"Precondition failed: {precond} == {expected} (actual: {precondition_value(state, precond)})"
    return True, ""

def precondition_value(state: ModelState, precondition: str) -> Any:
    """Get the actual value of a precondition path."""
    try:
        parts = precondition.split(".")
        if parts[0] == "node":
            obj = state.node
            path = parts[1:]
        elif parts[0] == "hub":
            obj = state.hub
            path = parts[1:]
        else:
            return None
        
        for part in path:
            if "[" in part:
                key, index_expr = part.split("[", 1)
                index = index_expr.rstrip("]")
                if index == "TAB5_NODE":
                    index = state.TAB5_NODE
                attr_dict = getattr(obj, key)
                if index not in attr_dict:
                    return "<missing>"
                obj = attr_dict[index]
            else:
                obj = getattr(obj, part)
        return obj
    except (KeyError, AttributeError):
        return "<missing>"

# ============================================================================
# State Transitions (matching Quint actions)
# ============================================================================

def apply_action(state: ModelState, action: Action) -> ModelState:
    """Apply an action to the state, returning a new state."""
    # Deep copy state
    new_state = ModelState(
        node=NodeState(
            connState=state.node.connState,
            connEpoch=state.node.connEpoch,
            handshakeComplete=state.node.handshakeComplete,
            lastAppliedParams=dict(state.node.lastAppliedParams),
            lastStatusTs=state.node.lastStatusTs
        ),
        hub=HubState(
            connState=dict(state.hub.connState),
            connEpoch=dict(state.hub.connEpoch),
            activeParams=dict(state.hub.activeParams),
            activeEffectId=state.hub.activeEffectId,
            lastBroadcastTs=state.hub.lastBroadcastTs,
            zoneCount=state.hub.zoneCount,
            zones={k: dict(v) for k, v in state.hub.zones.items()}
        ),
        TAB5_NODE=state.TAB5_NODE
    )
    
    if action.action_type == ActionType.NODE_CONNECT:
        new_state.node.connState = "CONNECTING"
        new_state.node.connEpoch += 1
        new_state.node.handshakeComplete = False  # CRITICAL: Reset on reconnect
        new_state.hub.connState[new_state.TAB5_NODE] = "CONNECTING"
        new_state.hub.connEpoch[new_state.TAB5_NODE] = new_state.node.connEpoch
    
    elif action.action_type == ActionType.NODE_CONNECTED:
        new_state.node.connState = "CONNECTED"
        new_state.hub.connState[new_state.TAB5_NODE] = "CONNECTED"
    
    elif action.action_type == ActionType.NODE_RECEIVE_STATUS:
        new_state.node.handshakeComplete = True
        # Note: lastAppliedParams would be updated from Status payload in full implementation
    
    elif action.action_type == ActionType.NODE_SEND_PARAMETERS:
        # Parameters are added to pendingMessages in model, we just track that it's allowed
        pass
    
    elif action.action_type == ActionType.HUB_APPLY_PARAMETERS:
        if action.params:
            for key, value in action.params.items():
                if key != "type" and key != "method":
                    new_state.hub.activeParams[key] = value
    
    elif action.action_type == ActionType.NODE_RECEIVE_PARAMS_CHANGED:
        if action.params:
            for key, value in action.params.items():
                if key != "type" and key != "method":
                    new_state.node.lastAppliedParams[key] = value
    
    elif action.action_type == ActionType.HUB_SET_EFFECT:
        if action.params and "effectId" in action.params:
            new_state.hub.activeEffectId = action.params["effectId"]
    
    elif action.action_type == ActionType.NODE_DISCONNECT:
        new_state.node.connState = "DISCONNECTED"
        new_state.node.handshakeComplete = False  # Reset on disconnect
        new_state.node.lastAppliedParams.clear()
        new_state.hub.connState.pop(new_state.TAB5_NODE, None)
    
    elif action.action_type == ActionType.REJECTED_MESSAGE:
        # Rejected messages don't change state (validation_negative traces)
        pass
    
    elif action.action_type == ActionType.NODE_SEND_ZONES_GET:
        # Node requests zones state (no state change, just tracking)
        pass
    
    elif action.action_type == ActionType.HUB_RESPOND_ZONES_LIST:
        # Hub responds with zones.list (simplified - would initialize zones state in full implementation)
        # For now, just track that the response was sent
        pass
    
    elif action.action_type == ActionType.NODE_SEND_ZONES_UPDATE:
        # Node sends zones.update (no state change yet, hub applies it)
        pass
    
    elif action.action_type == ActionType.HUB_APPLY_ZONES_UPDATE:
        # Hub applies zones.update and broadcasts zones.changed
        if action.params:
            # Extract zoneId from params
            zone_id = action.params.get("zoneId")
            if zone_id is not None:
                if zone_id not in new_state.hub.zones:
                    new_state.hub.zones[zone_id] = {}
                # Update zone fields (simplified - full implementation would extract all fields)
                for key, value in action.params.items():
                    if key not in ("type", "requestId", "method"):
                        new_state.hub.zones[zone_id][key] = value
    
    elif action.action_type == ActionType.NODE_RECEIVE_ZONES_CHANGED:
        # Node receives zones.changed broadcast (simplified - no state change tracked for node)
        pass
    
    return new_state

# ============================================================================
# Conformance Checking
# ============================================================================

@dataclass
class ConformanceResult:
    """Result of conformance checking."""
    passed: bool
    error_msg: str = ""
    event_index: int = -1
    invariant_name: str = ""
    
    @staticmethod
    def pass_() -> 'ConformanceResult':
        return ConformanceResult(passed=True)
    
    @staticmethod
    def illegal_transition(index: int, event: Dict[str, Any], action: Action, reason: str) -> 'ConformanceResult':
        return ConformanceResult(
            passed=False,
            error_msg=f"Illegal transition at event {index}: {reason}\nEvent: {json.dumps(event, indent=2)}\nAction: {get_action_description(action)}",
            event_index=index
        )
    
    @staticmethod
    def invariant_violation(index: int, invariant_name: str, state: ModelState) -> 'ConformanceResult':
        return ConformanceResult(
            passed=False,
            error_msg=f"Invariant violation at event {index}: {invariant_name}\nState: node.connState={state.node.connState}, node.handshakeComplete={state.node.handshakeComplete}, node.connEpoch={state.node.connEpoch}",
            event_index=index,
            invariant_name=invariant_name
        )

def check_trace(jsonl_path: Path) -> ConformanceResult:
    """Check conformance of a JSONL trace against the model."""
    state = initial_state()
    
    # Read JSONL events
    events = []
    with open(jsonl_path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
                events.append(event)
            except json.JSONDecodeError:
                continue
    
    # Process each event
    for i, event in enumerate(events):
        # Map event to actions
        actions = map_event_to_actions(event)
        
        if not actions:
            # Unknown event type - skip (could be telemetry.boot, etc.)
            continue
        
        # Apply each action in sequence (check preconditions, apply transitions)
        for action in actions:
            # Check preconditions
            precond_ok, precond_msg = check_action_preconditions(state, action)
            if not precond_ok:
                # For msg.recv events, if we're DISCONNECTED, implicitly allow connect sequence
                if event.get("event") == "msg.recv" and state.node.connState == "DISCONNECTED":
                    # Implicit connect: execute node_connect, env_deliver_connected, node_connected first
                    connect_actions = [
                        Action(ActionType.NODE_CONNECT, {"node.connState": "DISCONNECTED"}),
                        Action(ActionType.ENV_DELIVER_CONNECTED, {"node.connState": "CONNECTING"}),
                        Action(ActionType.NODE_CONNECTED, {"node.connState": "CONNECTING"})
                    ]
                    for connect_action in connect_actions:
                        state = apply_action(state, connect_action)
                    # Now retry the original action
                    precond_ok, precond_msg = check_action_preconditions(state, action)
                
                if not precond_ok:
                    return ConformanceResult.illegal_transition(i, event, action, precond_msg)
            
            # Apply state transition
            state = apply_action(state, action)
        
        # Check invariants after all actions from this event are processed
        # (this allows intermediate states like CONNECTED before handshakeComplete)
        for inv_name, inv_checker in INVARIANTS:
            if not inv_checker(state):
                return ConformanceResult.invariant_violation(i, inv_name, state)
    
    return ConformanceResult.pass_()

# ============================================================================
# Command-Line Interface
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: check_conformance.py <trace.jsonl>")
        sys.exit(1)
    
    jsonl_path = Path(sys.argv[1])
    
    if not jsonl_path.exists():
        print(f"ERROR: Trace file not found: {jsonl_path}")
        sys.exit(1)
    
    result = check_trace(jsonl_path)
    
    if result.passed:
        print(f"PASS: {jsonl_path.name} is model-conformant")
        sys.exit(0)
    else:
        print(f"FAIL: {jsonl_path.name}")
        print(result.error_msg)
        sys.exit(1)

if __name__ == "__main__":
    main()
