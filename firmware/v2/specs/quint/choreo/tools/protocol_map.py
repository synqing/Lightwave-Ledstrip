#!/usr/bin/env python3
"""
Protocol Map: Event-to-Action Mapping

Maps hardware telemetry events (JSONL) to Quint model actions.
Used by conformance checker to verify traces match the model.
"""

from typing import Dict, Any, List, Optional
from dataclasses import dataclass
from enum import Enum
import json

class ActionType(Enum):
    """Types of model actions."""
    NODE_CONNECT = "node_connect"
    ENV_DELIVER_CONNECTED = "env_deliver_connected"
    NODE_CONNECTED = "node_connected"
    HUB_RESPOND_STATUS = "hub_respond_status"
    NODE_RECEIVE_STATUS = "node_receive_status"
    NODE_SEND_PARAMETERS = "node_send_parameters"
    HUB_APPLY_PARAMETERS = "hub_apply_parameters"
    NODE_RECEIVE_PARAMS_CHANGED = "node_receive_params_changed"
    NODE_SEND_EFFECT = "node_send_effect"
    HUB_SET_EFFECT = "hub_set_effect"
    NODE_DISCONNECT = "node_disconnect"
    REJECTED_MESSAGE = "rejected_message"  # For validation_negative traces
    # Zones actions
    NODE_SEND_ZONES_GET = "node_send_zones_get"
    HUB_RESPOND_ZONES_LIST = "hub_respond_zones_list"
    NODE_SEND_ZONES_UPDATE = "node_send_zones_update"
    HUB_APPLY_ZONES_UPDATE = "hub_apply_zones_update"
    NODE_RECEIVE_ZONES_CHANGED = "node_receive_zones_changed"
    # OTA actions
    NODE_SEND_OTA_CHECK = "node_send_ota_check"
    HUB_RESPOND_OTA_STATUS = "hub_respond_ota_status"
    NODE_SEND_OTA_BEGIN = "node_send_ota_begin"
    HUB_RESPOND_OTA_READY = "hub_respond_ota_ready"
    NODE_SEND_OTA_CHUNK = "node_send_ota_chunk"
    HUB_RESPOND_OTA_PROGRESS = "hub_respond_ota_progress"
    NODE_SEND_OTA_ABORT = "node_send_ota_abort"
    HUB_APPLY_OTA_ABORT = "hub_apply_ota_abort"
    NODE_SEND_OTA_VERIFY = "node_send_ota_verify"
    HUB_APPLY_OTA_VERIFY = "hub_apply_ota_verify"

@dataclass
class Action:
    """Represents a model action."""
    action_type: ActionType
    preconditions: Dict[str, Any]  # State conditions that must hold
    params: Dict[str, Any] = None  # Action parameters (if any)

def parse_payload_summary(payload_str: str) -> Optional[Dict[str, Any]]:
    """Parse payloadSummary field from JSONL event."""
    if not payload_str:
        return None
    try:
        return json.loads(payload_str)
    except (json.JSONDecodeError, TypeError):
        return None

def map_event_to_actions(event: Dict[str, Any]) -> List[Action]:
    """
    Map a JSONL event to one or more model actions.
    
    Returns list of actions that must execute in sequence to match the event.
    """
    event_type = event.get("event")
    if not event_type:
        return []
    
    result = event.get("result", "ok")
    
    # Connection events
    if event_type == "ws.connect":
        return [
            Action(
                action_type=ActionType.NODE_CONNECT,
                preconditions={"node.connState": "DISCONNECTED"}
            ),
            Action(
                action_type=ActionType.ENV_DELIVER_CONNECTED,
                preconditions={"node.connState": "CONNECTING"}
            ),
            Action(
                action_type=ActionType.NODE_CONNECTED,
                preconditions={"node.connState": "CONNECTING"}
            )
        ]
    
    elif event_type == "ws.disconnect":
        return [
            Action(
                action_type=ActionType.NODE_DISCONNECT,
                preconditions={"node.connState": "CONNECTED"}
            )
        ]
    
    # Message events
    elif event_type == "msg.recv":
        # For rejected messages (validation_negative traces), we don't model the action
        if result == "rejected":
            return [
                Action(
                    action_type=ActionType.REJECTED_MESSAGE,
                    preconditions={},  # Rejections can happen in any state
                    params={"reason": event.get("reason", "unknown")}
                )
            ]
        
        # Parse payload to determine message type
        # Firmware logs msgType in event, and payloadSummary contains the full JSON
        # Use msgType from event first (more reliable), fallback to parsing payloadSummary
        msg_type = event.get("msgType", "")
        
        # If msgType not in event, try parsing payloadSummary
        if not msg_type:
            payload_str = event.get("payloadSummary", "")
            payload = parse_payload_summary(payload_str)
            if payload:
                msg_type = payload.get("type", "")
        
        if not msg_type:
            return []  # Can't determine action without message type
        
        # Normalize: handle both legacy "getStatus" and "device.getStatus"
        if msg_type == "getStatus" or msg_type == "device.getStatus":
            # Client requests status -> Hub responds
            # This creates GetStatus in pendingMessages, then hub_respond_status
            return [
                Action(
                    action_type=ActionType.HUB_RESPOND_STATUS,
                    preconditions={"hub.connState[TAB5_NODE]": "CONNECTED"}
                ),
                Action(
                    action_type=ActionType.NODE_RECEIVE_STATUS,
                    preconditions={"node.connState": "CONNECTED"}
                )
            ]
        
        elif msg_type == "parameters.set":
            # Client sends parameters -> Hub applies and broadcasts
            # Extract params from payloadSummary if available
            payload_str = event.get("payloadSummary", "")
            payload = parse_payload_summary(payload_str) or {}
            params = payload.get("params", {})
            if not params:
                # If no params in payload, extract parameter fields directly from payload
                params = {k: v for k, v in payload.items() if k not in ("type", "requestId", "method")}
            return [
                Action(
                    action_type=ActionType.NODE_SEND_PARAMETERS,
                    preconditions={
                        "node.connState": "CONNECTED",
                        "node.handshakeComplete": True
                    },
                    params=params
                ),
                Action(
                    action_type=ActionType.HUB_APPLY_PARAMETERS,
                    preconditions={"hub.connState[TAB5_NODE]": "CONNECTED"}
                ),
                Action(
                    action_type=ActionType.NODE_RECEIVE_PARAMS_CHANGED,
                    preconditions={"node.connState": "CONNECTED"}
                )
            ]
        
        elif msg_type == "effects.setCurrent":
            # Client sets effect -> Hub updates and broadcasts
            # Extract params from payloadSummary if available
            payload_str = event.get("payloadSummary", "")
            payload = parse_payload_summary(payload_str) or {}
            params = payload.get("params", {})
            if not params:
                # If no params in payload, extract effect fields directly from payload
                params = {k: v for k, v in payload.items() if k not in ("type", "requestId", "method")}
            return [
                Action(
                    action_type=ActionType.NODE_SEND_EFFECT,
                    preconditions={
                        "node.connState": "CONNECTED",
                        "node.handshakeComplete": True
                    },
                    params=params
                ),
                Action(
                    action_type=ActionType.HUB_SET_EFFECT,
                    preconditions={"hub.connState[TAB5_NODE]": "CONNECTED"}
                )
            ]
        
        elif msg_type == "zones.get":
            # Client requests zones state -> Hub responds with zones.list
            return [
                Action(
                    action_type=ActionType.NODE_SEND_ZONES_GET,
                    preconditions={
                        "node.connState": "CONNECTED",
                        "node.handshakeComplete": True
                    }
                ),
                Action(
                    action_type=ActionType.HUB_RESPOND_ZONES_LIST,
                    preconditions={"hub.connState[TAB5_NODE]": "CONNECTED"}
                )
            ]
        
        elif msg_type == "zones.update":
            # Client updates zone -> Hub applies and broadcasts zones.changed
            # Extract params from payloadSummary if available
            payload_str = event.get("payloadSummary", "")
            payload = parse_payload_summary(payload_str) or {}
            params = payload.get("params", {})
            if not params:
                # If no params in payload, extract zone fields directly from payload
                params = {k: v for k, v in payload.items() if k not in ("type", "requestId", "method")}
            return [
                Action(
                    action_type=ActionType.NODE_SEND_ZONES_UPDATE,
                    preconditions={
                        "node.connState": "CONNECTED",
                        "node.handshakeComplete": True
                    },
                    params=params
                ),
                Action(
                    action_type=ActionType.HUB_APPLY_ZONES_UPDATE,
                    preconditions={"hub.connState[TAB5_NODE]": "CONNECTED"}
                ),
                Action(
                    action_type=ActionType.NODE_RECEIVE_ZONES_CHANGED,
                    preconditions={"node.connState": "CONNECTED"}
                )
            ]
        
        elif msg_type == "zones.setEffect":
            # Similar to zones.update but for effect changes
            # Extract params from payloadSummary if available
            payload_str = event.get("payloadSummary", "")
            payload = parse_payload_summary(payload_str) or {}
            params = payload.get("params", {})
            if not params:
                # If no params in payload, extract zone fields directly from payload
                params = {k: v for k, v in payload.items() if k not in ("type", "requestId", "method")}
            return [
                Action(
                    action_type=ActionType.NODE_SEND_ZONES_UPDATE,
                    preconditions={
                        "node.connState": "CONNECTED",
                        "node.handshakeComplete": True
                    },
                    params=params
                ),
                Action(
                    action_type=ActionType.HUB_APPLY_ZONES_UPDATE,
                    preconditions={"hub.connState[TAB5_NODE]": "CONNECTED"}
                ),
                Action(
                    action_type=ActionType.NODE_RECEIVE_ZONES_CHANGED,
                    preconditions={"node.connState": "CONNECTED"}
                )
            ]
    
    return []  # Unknown event type

def get_action_description(action: Action) -> str:
    """Get human-readable description of an action."""
    return action.action_type.value
