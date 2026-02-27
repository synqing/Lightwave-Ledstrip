#!/usr/bin/env python3
"""
JSONL to ITF Converter

Converts JSONL event logs (from firmware traces) into ITF (Informal Trace Format)
state traces for Choreo conformance checking.

Input: JSONL events (one JSON object per line)
Output: ITF JSON file (state sequence format)

Event → State abstraction:
- Events update abstract state (connState, connEpoch, lastAppliedParams, etc.)
- State snapshots are created at each event (or at configurable intervals)
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Any, Optional
from dataclasses import dataclass, field
from datetime import datetime

# ============================================================================
# Abstract State Model
# ============================================================================

@dataclass
class NodeState:
    """Abstract state for Tab5 Node."""
    connState: str = "DISCONNECTED"  # DISCONNECTED, CONNECTING, CONNECTED, ERROR
    connEpoch: int = 0
    handshakeComplete: bool = False
    lastAppliedParams: Dict[str, int] = field(default_factory=dict)
    pendingCommands: List[Dict[str, Any]] = field(default_factory=list)
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
    # OTA state (Phase: OTA slice)
    otaState: str = "Idle"
    otaTotalSize: int = 0
    otaBytesReceived: int = 0

# ============================================================================
# ITF Integer Encoding Helper (ADR-015 #bigint format)
# ============================================================================

def wrap_bigint(value: int) -> Dict[str, str]:
    """Wrap integer value in ADR-015 #bigint format."""
    return {"#bigint": str(value)}

# ============================================================================
# Event Parser
# ============================================================================

def parse_jsonl_event(line: str) -> Optional[Dict[str, Any]]:
    """Parse a single JSONL event line."""
    try:
        return json.loads(line)
    except json.JSONDecodeError:
        return None

# ============================================================================
# State Builder (Event → State Transitions)
# ============================================================================

class StateBuilder:
    """Builds abstract state from event stream."""
    
    def __init__(self, node_id: str = "Tab5"):
        self.node_id = node_id
        self.node_state = NodeState()
        self.hub_state = HubState()
        self.states: List[Dict[str, Any]] = []
    
    def process_event(self, event: Dict[str, Any]):
        """Process a single event and update abstract state."""
        # Support both old schema (type field) and new schema (event field)
        event_type = event.get("event") or event.get("type")
        conn_epoch = event.get("connEpoch", 0)
        ts_ms = event.get("ts_mono_ms") or event.get("ts_ms", 0)
        direction = event.get("direction", "server")  # Default to server (Hub perspective)
        
        # Update connection epoch if provided
        if "connEpoch" in event:
            if direction == "client":
                self.node_state.connEpoch = conn_epoch
                self.hub_state.connEpoch[self.node_id] = conn_epoch
            elif direction == "server":
                # Server-side event (Hub perspective)
                node_id = event.get("node", "Tab5")
                self.hub_state.connEpoch[node_id] = conn_epoch
        
        # Handle connection events
        if event_type == "ws.connect":
            if direction == "client":
                self.node_state.connState = "CONNECTING"
                self.hub_state.connState[self.node_id] = "CONNECTING"
            self._snapshot_state(ts_ms)
        
        elif event_type == "ws.connected":
            if direction == "client":
                self.node_state.connState = "CONNECTED"
                self.hub_state.connState[self.node_id] = "CONNECTED"
            self._snapshot_state(ts_ms)
        
        elif event_type == "ws.disconnect":
            if direction == "client":
                self.node_state.connState = "DISCONNECTED"
                self.node_state.pendingCommands.clear()
                self.hub_state.connState.pop(self.node_id, None)
            self._snapshot_state(ts_ms)
        
        # Handle messages (new schema: event field instead of type field)
        elif event_type == "msg.send" or event.get("event") == "msg.send":
            # Extract msgType from event (new schema) or msg (old schema)
            msg_type = event.get("msgType") or (event.get("msg", {}).get("type"))
            result = event.get("result", "ok")
            
            if direction == "client":
                # Node sending to Hub
                if msg_type == "getStatus":
                    self.node_state.pendingCommands.append({"type": "getStatus", "sentAt": ts_ms})
                elif msg_type == "parameters.set":
                    self.node_state.pendingCommands.append({"type": "parameters.set", "sentAt": ts_ms, "payload": event.get("payloadSummary", {})})
            
            self._snapshot_state(ts_ms)
        
        elif event_type == "msg.recv" or event.get("event") == "msg.recv":
            # Extract msgType from event (firmware logs this directly)
            msg_type = event.get("msgType", "")
            
            # Try to parse payloadSummary if available (new schema)
            msg_payload = {}
            if "payloadSummary" in event:
                try:
                    import json
                    payload_str = event["payloadSummary"]
                    if payload_str:
                        msg_payload = json.loads(payload_str)
                        # If msgType not in event, extract from payload
                        if not msg_type and "type" in msg_payload:
                            msg_type = msg_payload["type"]
                except (json.JSONDecodeError, TypeError):
                    pass
            
            # Fallback to old schema with nested msg
            if not msg_payload and "msg" in event:
                msg_payload = event.get("msg", {})
                if not msg_type and "type" in msg_payload:
                    msg_type = msg_payload["type"]
            
            if direction == "client":
                # Node receiving from Hub
                if msg_type == "status":
                    # Handshake complete on first status
                    if not self.node_state.handshakeComplete:
                        self.node_state.handshakeComplete = True
                    
                    # Extract parameters from status payload
                    if "brightness" in msg_payload:
                        self.node_state.lastAppliedParams["brightness"] = msg_payload["brightness"]
                    if "speed" in msg_payload:
                        self.node_state.lastAppliedParams["speed"] = msg_payload["speed"]
                    if "effectId" in msg_payload:
                        self.hub_state.activeEffectId = msg_payload["effectId"]
                    
                    self.node_state.lastStatusTs = ts_ms
                    self.hub_state.lastBroadcastTs = ts_ms
                    
                    # Clear pending getStatus
                    self.node_state.pendingCommands = [c for c in self.node_state.pendingCommands if c.get("type") != "getStatus"]
                
                elif msg_type == "parameters.changed":
                    # Update last applied params
                    for key, value in msg_payload.items():
                        if key != "type":
                            self.node_state.lastAppliedParams[key] = value
                
                elif msg_type == "effect.changed":
                    if "effectId" in msg_payload:
                        self.hub_state.activeEffectId = msg_payload["effectId"]
                
                elif msg_type == "zones.changed":
                    # Zone update broadcast from Hub
                    if "zoneId" in msg_payload:
                        zone_id = msg_payload["zoneId"]
                        # Update zone state (simplified - full implementation would track all fields)
                        if zone_id not in self.hub_state.zones:
                            self.hub_state.zones[zone_id] = {}
                        if "updatedFields" in msg_payload:
                            # Track that this zone was updated
                            pass
                
                elif msg_type == "zones.list":
                    # Zones list response (initialize zones state if needed)
                    if "zones" in msg_payload and isinstance(msg_payload["zones"], list):
                        self.hub_state.zoneCount = len(msg_payload["zones"])
                        for zone_info in msg_payload["zones"]:
                            if "zoneId" in zone_info:
                                zone_id = zone_info["zoneId"]
                                self.hub_state.zones[zone_id] = {
                                    "enabled": zone_info.get("enabled", False),
                                    "effectId": zone_info.get("effectId", 0),
                                    "paletteId": zone_info.get("paletteId", 0),
                                    "brightness": zone_info.get("brightness", 128),
                                    "speed": zone_info.get("speed", 50),
                                    "blendMode": zone_info.get("blendMode", 0)
                                }
            
            elif direction == "server":
                # Hub receiving from Node
                if msg_type == "parameters.set":
                    # Update Hub active params
                    for key, value in msg_payload.items():
                        if key != "type":
                            self.hub_state.activeParams[key] = value
                
                # Handle zones commands via msgType (from event or payload)
                if msg_type == "zones.get":
                    # Client requesting zones state (no state change, response will update)
                    pass
                
                elif msg_type == "zones.update":
                    # Client updating zone -> Hub will apply and broadcast
                    if "zoneId" in msg_payload:
                        zone_id = msg_payload["zoneId"]
                        if zone_id not in self.hub_state.zones:
                            self.hub_state.zones[zone_id] = {}
                        # Update zone fields (simplified - full implementation would extract all fields)
                        if "effectId" in msg_payload:
                            self.hub_state.zones[zone_id]["effectId"] = msg_payload["effectId"]
                        if "brightness" in msg_payload:
                            self.hub_state.zones[zone_id]["brightness"] = msg_payload["brightness"]
                        if "speed" in msg_payload:
                            self.hub_state.zones[zone_id]["speed"] = msg_payload["speed"]
                        if "paletteId" in msg_payload:
                            self.hub_state.zones[zone_id]["paletteId"] = msg_payload["paletteId"]
                        if "blendMode" in msg_payload:
                            self.hub_state.zones[zone_id]["blendMode"] = msg_payload["blendMode"]
                
                # OTA messages (WebSocket protocol)
                elif msg_type in ("ota.check", "otaCheck"):
                    # OTA check request (no state change)
                    pass
                
                elif msg_type in ("ota.status", "otaStatus"):
                    # OTA status response (no state change, just informational)
                    pass
                
                elif msg_type in ("ota.begin", "otaBegin"):
                    # OTA begin request - Hub will respond with ota.ready
                    if "size" in msg_payload:
                        # Transition to InProgress will happen on ota.ready response
                        pass
                
                elif msg_type in ("ota.ready", "otaReady"):
                    # Hub responds to ota.begin, starting OTA session
                    if "totalSize" in msg_payload:
                        self.hub_state.otaState = "InProgress"
                        self.hub_state.otaTotalSize = msg_payload["totalSize"]
                        self.hub_state.otaBytesReceived = 0
                
                elif msg_type in ("ota.chunk", "otaChunk"):
                    # OTA chunk received - progress tracked via ota.progress response
                    pass
                
                elif msg_type in ("ota.progress", "otaProgress"):
                    # OTA progress update
                    if "offset" in msg_payload:
                        self.hub_state.otaBytesReceived = msg_payload["offset"]
                        # Keep state as InProgress (Complete only on verify)
                        if self.hub_state.otaState != "InProgress":
                            self.hub_state.otaState = "InProgress"
                
                elif msg_type in ("ota.abort", "otaAbort"):
                    # OTA abort request - Hub will reset state
                    pass
                
                elif msg_type in ("ota.verify", "otaVerify"):
                    # OTA verify request - Hub will complete
                    if self.hub_state.otaBytesReceived >= self.hub_state.otaTotalSize:
                        self.hub_state.otaState = "Complete"
            
            self._snapshot_state(ts_ms)
        
        # OTA telemetry events (REST and WebSocket)
        elif event_type in ("ota.ws.begin", "ota.rest.begin"):
            # OTA session started
            if "totalBytes" in event:
                self.hub_state.otaState = "InProgress"
                self.hub_state.otaTotalSize = event["totalBytes"]
                self.hub_state.otaBytesReceived = 0
            self._snapshot_state(ts_ms)
        
        elif event_type in ("ota.ws.chunk", "ota.rest.progress"):
            # OTA progress update
            if "offset" in event:
                self.hub_state.otaBytesReceived = event["offset"]
                if self.hub_state.otaState != "InProgress":
                    self.hub_state.otaState = "InProgress"
            self._snapshot_state(ts_ms)
        
        elif event_type in ("ota.ws.complete", "ota.rest.complete"):
            # OTA completed successfully
            self.hub_state.otaState = "Complete"
            if "finalSize" in event:
                self.hub_state.otaBytesReceived = event["finalSize"]
            self._snapshot_state(ts_ms)
        
        elif event_type in ("ota.ws.abort", "ota.ws.failed", "ota.rest.failed"):
            # OTA aborted or failed - reset to Idle
            self.hub_state.otaState = "Idle"
            self.hub_state.otaTotalSize = 0
            self.hub_state.otaBytesReceived = 0
            self._snapshot_state(ts_ms)
        
        # Timer events (timeouts, reconnects)
        elif event_type == "timer.fire":
            timer_type = event.get("timerType", "")
            if timer_type == "reconnect":
                # Increment epoch on reconnect
                self.node_state.connEpoch += 1
                self.hub_state.connEpoch[self.node_id] = self.node_state.connEpoch
            
            self._snapshot_state(ts_ms)
    
    def _snapshot_state(self, ts_ms: int):
        """Create a state snapshot at current point in time."""
        # Convert integer dicts to #bigint format
        last_applied_params_bigint = {k: wrap_bigint(v) for k, v in self.node_state.lastAppliedParams.items()}
        active_params_bigint = {k: wrap_bigint(v) for k, v in self.hub_state.activeParams.items()}
        
        # Convert pendingCommands with integer fields
        pending_cmds_bigint = []
        for cmd in self.node_state.pendingCommands:
            cmd_copy = dict(cmd)
            if "sentAt" in cmd_copy:
                cmd_copy["sentAt"] = wrap_bigint(cmd_copy["sentAt"])
            pending_cmds_bigint.append(cmd_copy)
        
        # Convert connEpoch maps (dict of node_id -> epoch)
        hub_conn_epoch_bigint = {k: wrap_bigint(v) for k, v in self.hub_state.connEpoch.items()}
        
        # Convert zones map (zoneId -> zone state) with bigint wrapping
        zones_bigint = {}
        for zone_id, zone_state in self.hub_state.zones.items():
            zones_bigint[zone_id] = {
                k: wrap_bigint(v) if isinstance(v, int) else v
                for k, v in zone_state.items()
            }
        
        state_snapshot = {
            "node": {
                "connState": self.node_state.connState,
                "connEpoch": wrap_bigint(self.node_state.connEpoch),
                "handshakeComplete": self.node_state.handshakeComplete,
                "lastAppliedParams": last_applied_params_bigint,
                "pendingCommands": pending_cmds_bigint,
                "lastStatusTs": wrap_bigint(self.node_state.lastStatusTs)
            },
            "hub": {
                "connState": dict(self.hub_state.connState),
                "connEpoch": hub_conn_epoch_bigint,
                "activeParams": active_params_bigint,
                "activeEffectId": wrap_bigint(self.hub_state.activeEffectId),
                "lastBroadcastTs": wrap_bigint(self.hub_state.lastBroadcastTs),
                "zoneCount": wrap_bigint(self.hub_state.zoneCount),
                "zones": zones_bigint,
                "otaState": self.hub_state.otaState,  # String enum, no bigint wrapping
                "otaTotalSize": wrap_bigint(self.hub_state.otaTotalSize),
                "otaBytesReceived": wrap_bigint(self.hub_state.otaBytesReceived)
            }
        }
        
        self.states.append({
            "state": state_snapshot,
            "metadata": {
                "ts_ms": wrap_bigint(ts_ms)
            }
        })
    
    def to_itf(self) -> Dict[str, Any]:
        """Convert accumulated states to ITF format."""
        return {
            "format": "ITF",
            "formatVersion": "1.0",
            "generatedAt": datetime.now().isoformat(),
            "states": self.states
        }

# ============================================================================
# Main Conversion Logic
# ============================================================================

def convert_jsonl_to_itf(jsonl_path: Path, itf_path: Path, node_id: str = "Tab5"):
    """Convert JSONL event log to ITF state trace."""
    builder = StateBuilder(node_id=node_id)
    
    # Read JSONL events
    with open(jsonl_path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            
            event = parse_jsonl_event(line)
            if event:
                builder.process_event(event)
    
    # Generate ITF output
    itf_data = builder.to_itf()
    
    # Write ITF file
    with open(itf_path, "w") as f:
        json.dump(itf_data, f, indent=2)
    
    print(f"Converted {len(builder.states)} state snapshots from {jsonl_path} to {itf_path}")
    return itf_path

# ============================================================================
# Command-Line Interface
# ============================================================================

def main():
    if len(sys.argv) < 3:
        print("Usage: jsonl_to_itf.py <input.jsonl> <output.itf.json> [node_id]")
        sys.exit(1)
    
    jsonl_path = Path(sys.argv[1])
    itf_path = Path(sys.argv[2])
    node_id = sys.argv[3] if len(sys.argv) > 3 else "Tab5"
    
    if not jsonl_path.exists():
        print(f"ERROR: Input file not found: {jsonl_path}")
        sys.exit(1)
    
    convert_jsonl_to_itf(jsonl_path, itf_path, node_id)
    print(f"ITF trace written to: {itf_path}")

if __name__ == "__main__":
    main()
