// @file lightwave_types.qnt
// @module lightwave_types
// @description Shared types for LightwaveOS Hub/Node protocol

module lightwave_types {

  // ============================================================================
  // Processes
  // ============================================================================
  
  // Node identifiers (0 = Hub, 1+ = Nodes)
  // For now: single Node (Tab5.encoder)
  type Node = int
  pure val HUB: Node = 0
  pure val TAB5_NODE: Node = 1
  pure val NODES: Set[Node] = Set(TAB5_NODE)
  pure val ALL_PROCESSES: Set[Node] = Set(HUB, TAB5_NODE)
  
  // ============================================================================
  // Connection State
  // ============================================================================
  
  // Connection state is a string literal (Quint doesn't have union types)
  type ConnState = str
  
  // ============================================================================
  // Messages (Control Plane - Core Flow)
  // ============================================================================
  
  // Core messages for Phase 3 (connect → status → parameter change)
  type Message =
    | GetStatus
    | Status({
      effectId: int,
      brightness: int,
      speed: int,
      paletteId: int
    })
    | ParametersSet({
      brightness: int,
      speed: int,
      paletteId: int
    })
    | ParametersChanged({
      brightness: int,
      speed: int,
      paletteId: int
    })
    | EffectsSetCurrent({
      effectId: int
    })
    | EffectChanged({
      effectId: int
    })
  // Zones messages (Phase: zones slice)
    | ZonesGet
    | ZonesList({
      zones: List[ZoneInfo]
    })
    | ZonesUpdate({
      zoneId: int,
      effectId: int,
      brightness: int,
      speed: int,
      paletteId: int,
      blendMode: int
    })
    | ZonesChanged({
      zoneId: int,
      updatedFields: List[str]
    })
    | ZonesEffectChanged({
      zoneId: int,
      effectId: int
    })
    | ZonesLayoutChanged({
      zoneCount: int
    })
  
  // Zone info structure (for ZonesList)
  type ZoneInfo = {
    zoneId: int,
    enabled: bool,
    effectId: int,
    paletteId: int,
    brightness: int,
    speed: int,
    blendMode: int
  }
  
  // ============================================================================
  // Events (Timeouts and Connection Events)
  // ============================================================================
  
  type Event =
    | WsConnect(Node)
    | WsConnected(Node)
    | WsDisconnect(Node)
    | ReconnectTimer(Node)
  
  // ============================================================================
  // Message Envelopes (Phase 13: Soup Semantics)
  // ============================================================================
  
  // Message envelope with metadata for soup modeling (duplicates, stale epochs, etc.)
  type MsgEnvelope = {
    id: int,        // Unique message ID (allows duplicates via different IDs)
    epoch: int,     // Connection epoch when message was sent
    dir: str,       // Direction: "node2hub" or "hub2node"
    msg: Message    // The actual message payload
  }
  
  // ============================================================================
  // Debug State (Phase 13: Track last delivery metadata for invariants)
  // ============================================================================
  
  type DebugState = {
    lastMsgId: int,
    lastMsgEpoch: int,
    lastMsgType: str,            // Message type as string (e.g., "GetStatus", "ParametersSet")
    lastResult: str,             // "ok" or "rejected"
    lastReason: str,             // Rejection reason (e.g., "rate_limit", "parse_error", "")
    lastProtocolMutated: bool,   // Whether protocol state changed
    lastWasDuplicate: bool,      // Whether message was a duplicate
    lastWasStale: bool           // Whether message was from stale epoch
  }
  
  // ============================================================================
  // State Fields (Abstract State Model)
  // ============================================================================
  
  type NodeStateFields = {
    connState: ConnState,
    connEpoch: int,
    handshakeComplete: bool,
    lastAppliedParams: str -> int,
    lastStatusTs: int
  }
  
  type HubStateFields = {
    connState: Node -> ConnState,
    connEpoch: Node -> int,
    activeParams: str -> int,
    activeEffectId: int,
    lastBroadcastTs: int,
    // Zones state (Phase: zones slice)
    zoneCount: int,
    zones: int -> ZoneStateFields  // Map of zoneId to zone state
  }
  
  type ZoneStateFields = {
    enabled: bool,
    effectId: int,
    paletteId: int,
    brightness: int,
    speed: int,
    blendMode: int
  }
  
  // Combined state (for Choreo context)
  type LightwaveState = {
    node: NodeStateFields,
    hub: HubStateFields,
    debug: DebugState  // Phase 13: Debug metadata for invariants
  }

}
