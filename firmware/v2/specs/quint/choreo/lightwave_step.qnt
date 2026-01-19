// @file lightwave_step.qnt
// @module lightwave_step
// @description Fast Choreo step model for v2 Hub ↔ Tab5 Node protocol
// Phase 3: Core flow (connect → status → parameter change)
// Simplified version for Quint 0.29 compatibility

module lightwave_step {

  import lightwave_types.* from "./lightwave_types"
  
  // ============================================================================
  // State Variables
  // ============================================================================
  
  var state: LightwaveState
  
  // Message soup (in-flight messages, nondeterministic order)
  // Phase 13: Changed from Set[Message] to Set[MsgEnvelope] to support duplicates/stale epochs
  var pendingMessages: Set[MsgEnvelope]
  
  // Track delivered message IDs to detect duplicates
  var deliveredMsgIds: Set[int]
  
  // Event queue (timeouts, reconnects)
  var pendingEvents: Set[Event]
  
  // ============================================================================
  // Helper predicates for message type checking (Phase 13: work with envelopes)
  // ============================================================================
  
  // Check if soup contains a GetStatus message
  pure def hasGetStatusMessage(envs: Set[MsgEnvelope]): bool = {
    envs.exists(e => match e.msg {
      | GetStatus => true
      | _ => false
    })
  }
  
  // Check if soup contains a Status message
  pure def hasStatusMessage(envs: Set[MsgEnvelope]): bool = {
    envs.exists(e => match e.msg {
      | Status(_) => true
      | _ => false
    })
  }
  
  // Check if soup contains a ParametersSet message
  pure def hasParametersSetMessage(envs: Set[MsgEnvelope]): bool = {
    envs.exists(e => match e.msg {
      | ParametersSet(_) => true
      | _ => false
    })
  }
  
  // Check if soup contains a ParametersChanged message
  pure def hasParametersChangedMessage(envs: Set[MsgEnvelope]): bool = {
    envs.exists(e => match e.msg {
      | ParametersChanged(_) => true
      | _ => false
    })
  }
  
  // Check if soup contains a ZonesGet message
  pure def hasZonesGetMessage(envs: Set[MsgEnvelope]): bool = {
    envs.exists(e => match e.msg {
      | ZonesGet => true
      | _ => false
    })
  }
  
  // Check if soup contains a ZonesUpdate message
  pure def hasZonesUpdateMessage(envs: Set[MsgEnvelope]): bool = {
    envs.exists(e => match e.msg {
      | ZonesUpdate(_) => true
      | _ => false
    })
  }
  
  // Check if soup contains a ZonesChanged message
  pure def hasZonesChangedMessage(envs: Set[MsgEnvelope]): bool = {
    envs.exists(e => match e.msg {
      | ZonesChanged(_) => true
      | _ => false
    })
  }
  
  // Extract message type as string (for debug tracking)
  pure def msgTypeToString(m: Message): str = {
    match m {
      | GetStatus => "GetStatus"
      | Status(_) => "Status"
      | ParametersSet(_) => "ParametersSet"
      | ParametersChanged(_) => "ParametersChanged"
      | EffectsSetCurrent(_) => "EffectsSetCurrent"
      | EffectChanged(_) => "EffectChanged"
      | ZonesGet => "ZonesGet"
      | ZonesList(_) => "ZonesList"
      | ZonesUpdate(_) => "ZonesUpdate"
      | ZonesChanged(_) => "ZonesChanged"
      | ZonesEffectChanged(_) => "ZonesEffectChanged"
      | ZonesLayoutChanged(_) => "ZonesLayoutChanged"
    }
  }
  
  // Check if message ID was already delivered (duplicate detection)
  pure def isDuplicate(msgId: int, delivered: Set[int]): bool = {
    delivered.contains(msgId)
  }
  
  // Check if message epoch is stale (less than current epoch)
  pure def isStale(msgEpoch: int, currentEpoch: int): bool = {
    msgEpoch < currentEpoch
  }
  
  // Helper to create envelope with auto-incrementing ID (simplified - uses epoch as part of ID)
  // In practice, message IDs would be tracked separately, but for modeling we use epoch*1000 + sequence
  pure def mkEnvelope(id: int, epoch: int, dir: str, msg: Message): MsgEnvelope = {
    {id: id, epoch: epoch, dir: dir, msg: msg}
  }
  
  // ============================================================================
  // Initialization
  // ============================================================================
  
  action init = {
    all {
      state' = {
        node: {
          connState: "DISCONNECTED",
          connEpoch: 0,
          handshakeComplete: false,
          lastAppliedParams: Map(),
          lastStatusTs: 0
        },
        hub: {
          connState: Map(),
          connEpoch: Map(),
          activeParams: Map(),
          activeEffectId: 0,
          lastBroadcastTs: 0,
          zoneCount: 0,  // Initial zones state
          zones: Map()   // Empty zones map
        },
        debug: {
          lastMsgId: 0,
          lastMsgEpoch: 0,
          lastMsgType: "",
          lastResult: "",
          lastReason: "",
          lastProtocolMutated: false,
          lastWasDuplicate: false,
          lastWasStale: false
        }
      },
      pendingMessages' = Set(),
      deliveredMsgIds' = Set(),
      // Start with WsConnect event to drive the protocol
      pendingEvents' = Set(WsConnect(TAB5_NODE))
    }
  }
  
  // ============================================================================
  // Node Actions (Tab5.encoder behavior)
  // ============================================================================
  
  // Node connects to Hub
  action node_connect = {
    all {
      state.node.connState == "DISCONNECTED",
      pendingEvents.contains(WsConnect(TAB5_NODE)),
      state' = {
        node: {
          connState: "CONNECTING",
          connEpoch: state.node.connEpoch + 1,
          handshakeComplete: false,  // CRITICAL: Reset on reconnect (EpochResetsHandshake invariant)
          lastAppliedParams: state.node.lastAppliedParams,
          lastStatusTs: state.node.lastStatusTs
        },
        hub: {
          connState: state.hub.connState.put(TAB5_NODE, "CONNECTING"),
          connEpoch: state.hub.connEpoch.put(TAB5_NODE, state.node.connEpoch + 1),
          activeParams: state.hub.activeParams,
          activeEffectId: state.hub.activeEffectId,
          lastBroadcastTs: state.hub.lastBroadcastTs,
          zoneCount: state.hub.zoneCount,
          zones: state.hub.zones
        },
        debug: state.debug  // Preserve debug state
      },
      pendingEvents' = pendingEvents.exclude(Set(WsConnect(TAB5_NODE))),
      pendingMessages' = pendingMessages,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Connection established
  action node_connected = {
    nondet msgId = 1.to(10000).oneOf()  // Nondeterministic message ID
    all {
      state.node.connState == "CONNECTING",
      pendingEvents.contains(WsConnected(TAB5_NODE)),
      not(deliveredMsgIds.contains(msgId)),  // ID must be unique (for now)
      state' = {
        node: {
          connState: "CONNECTED",
          connEpoch: state.node.connEpoch,
          handshakeComplete: state.node.handshakeComplete,
          lastAppliedParams: state.node.lastAppliedParams,
          lastStatusTs: state.node.lastStatusTs
        },
        hub: {
          connState: state.hub.connState.put(TAB5_NODE, "CONNECTED"),
          connEpoch: state.hub.connEpoch,
          activeParams: state.hub.activeParams,
          activeEffectId: state.hub.activeEffectId,
          lastBroadcastTs: state.hub.lastBroadcastTs,
          zoneCount: state.hub.zoneCount,
          zones: state.hub.zones
        },
        debug: state.debug  // Preserve debug state (no protocol mutation yet)
      },
      pendingEvents' = pendingEvents.exclude(Set(WsConnected(TAB5_NODE))),
      // Phase 13: Create GetStatus as envelope with current epoch
      pendingMessages' = pendingMessages.union(Set(mkEnvelope(msgId, state.node.connEpoch, "node2hub", GetStatus))),
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Node receives status (handshake complete) - simplified without pattern match
  action node_receive_status = {
    all {
      state.node.connState == "CONNECTED",
      hasStatusMessage(pendingMessages),
      state' = {
        node: {
          connState: state.node.connState,
          connEpoch: state.node.connEpoch,
          handshakeComplete: true,
          lastAppliedParams: state.node.lastAppliedParams,
          lastStatusTs: state.node.lastStatusTs
        },
        hub: state.hub,
        debug: state.debug
      },
      pendingMessages' = pendingMessages.filter(e => match e.msg {
        | Status(_) => false
        | _ => true
      }),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Node sends parameter update
  action node_send_parameters = {
    nondet brightness = 0.to(255).oneOf()
    nondet speed = 1.to(100).oneOf()
    nondet msgId = 1.to(10000).oneOf()
    all {
      state.node.handshakeComplete,
      state.node.connState == "CONNECTED",
      not(deliveredMsgIds.contains(msgId)),
      state' = state,
      pendingMessages' = pendingMessages.union(Set(mkEnvelope(msgId, state.node.connEpoch, "node2hub", ParametersSet({brightness: brightness, speed: speed, paletteId: 0})))),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // ============================================================================
  // Hub Actions (v2 device behavior)
  // ============================================================================
  
  // Hub receives getStatus, responds with status
  action hub_respond_status = {
    nondet respMsgId = 1.to(10000).oneOf()
    all {
      hasGetStatusMessage(pendingMessages),
      state.hub.connState.get(TAB5_NODE) == "CONNECTED",
      not(deliveredMsgIds.contains(respMsgId)),
      state' = state,
      pendingMessages' = pendingMessages
        .filter(e => match e.msg {
          | GetStatus => false
          | _ => true
        })
        .union(Set(mkEnvelope(respMsgId, state.hub.connEpoch.get(TAB5_NODE), "hub2node", Status({
          effectId: state.hub.activeEffectId,
          brightness: if (state.hub.activeParams.keys().contains("brightness")) state.hub.activeParams.get("brightness") else 128,
          speed: if (state.hub.activeParams.keys().contains("speed")) state.hub.activeParams.get("speed") else 50,
          paletteId: if (state.hub.activeParams.keys().contains("paletteId")) state.hub.activeParams.get("paletteId") else 0
        })))),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Hub receives parameters.set, updates state and broadcasts - simplified
  action hub_apply_parameters = {
    all {
      hasParametersSetMessage(pendingMessages),
      state' = {
        node: state.node,
        hub: {
          connState: state.hub.connState,
          connEpoch: state.hub.connEpoch,
          activeParams: state.hub.activeParams
            .put("brightness", 128)
            .put("speed", 50)
            .put("paletteId", 0),
          activeEffectId: state.hub.activeEffectId,
          lastBroadcastTs: state.hub.lastBroadcastTs,
          zoneCount: state.hub.zoneCount,
          zones: state.hub.zones
        },
        debug: state.debug
      },
      // Note: ParametersChanged response would be added as envelope in full Phase 13 implementation
      pendingMessages' = pendingMessages
        .filter(e => match e.msg {
          | ParametersSet(_) => false
          | _ => true
        }),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Node receives parameters.changed, updates local state - simplified
  action node_receive_params_changed = {
    all {
      hasParametersChangedMessage(pendingMessages),
      state.node.connState == "CONNECTED",
      state' = {
        node: {
          connState: state.node.connState,
          connEpoch: state.node.connEpoch,
          handshakeComplete: state.node.handshakeComplete,
          lastAppliedParams: state.node.lastAppliedParams
            .put("brightness", 128)
            .put("speed", 50)
            .put("paletteId", 0),
          lastStatusTs: state.node.lastStatusTs
        },
        hub: state.hub,
        debug: state.debug
      },
      pendingMessages' = pendingMessages.filter(e => match e.msg {
        | ParametersChanged(_) => false
        | _ => true
      }),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Node sends effect update
  action node_send_effect = {
    nondet effectId = 0.to(255).oneOf()
    nondet msgId = 1.to(10000).oneOf()
    all {
      state.node.handshakeComplete,
      state.node.connState == "CONNECTED",
      not(deliveredMsgIds.contains(msgId)),
      state' = state,
      pendingMessages' = pendingMessages.union(Set(mkEnvelope(msgId, state.node.connEpoch, "node2hub", EffectsSetCurrent({effectId: effectId})))),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Hub receives effects.setCurrent, updates state and broadcasts
  action hub_set_effect = {
    all {
      pendingMessages.exists(e => match e.msg {
        | EffectsSetCurrent(_) => true
        | _ => false
      }),
      state.hub.connState.get(TAB5_NODE) == "CONNECTED",
      state' = {
        node: state.node,
        hub: {
          connState: state.hub.connState,
          connEpoch: state.hub.connEpoch,
          activeParams: state.hub.activeParams,
          activeEffectId: 1,  // Simplified - extract from message in full implementation
          lastBroadcastTs: state.hub.lastBroadcastTs,
          zoneCount: state.hub.zoneCount,
          zones: state.hub.zones
        },
        debug: state.debug
      },
      pendingMessages' = pendingMessages.filter(e => match e.msg {
        | EffectsSetCurrent(_) => false
        | _ => true
      }),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Node disconnects
  action node_disconnect = {
    all {
      state.node.connState == "CONNECTED",
      pendingEvents.contains(WsDisconnect(TAB5_NODE)),
      state' = {
        node: {
          connState: "DISCONNECTED",
          connEpoch: state.node.connEpoch,
          handshakeComplete: false,  // Reset on disconnect
          lastAppliedParams: Map(),  // Clear applied params on disconnect
          lastStatusTs: state.node.lastStatusTs
        },
        hub: {
          // Remove TAB5_NODE from hub connection maps (simplified - clear since we only track one node)
          connState: Map(),
          connEpoch: Map(),
          activeParams: state.hub.activeParams,
          activeEffectId: state.hub.activeEffectId,
          lastBroadcastTs: state.hub.lastBroadcastTs,
          zoneCount: state.hub.zoneCount,  // Preserve zones on disconnect
          zones: state.hub.zones
        },
        debug: state.debug
      },
      pendingEvents' = pendingEvents.exclude(Set(WsDisconnect(TAB5_NODE))),
      pendingMessages' = pendingMessages,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // ============================================================================
  // Environment Actions (network delivers events)
  // ============================================================================
  
  // Network delivers WsConnected after connect
  action env_deliver_connected = {
    all {
      state.node.connState == "CONNECTING",
      not(pendingEvents.contains(WsConnected(TAB5_NODE))),
      state' = state,
      pendingMessages' = pendingMessages,
      pendingEvents' = pendingEvents.union(Set(WsConnected(TAB5_NODE))),
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // ============================================================================
  // Fault Injection Actions (Phase 13: Soup semantics - duplicates, drops, stale epochs)
  // ============================================================================
  
  // Fault: Drop message (remove envelope from soup without delivering)
  // Note: Simplified - deferred to Phase 13 full implementation
  // action env_drop_message = { ... }
  
  // Fault: Duplicate message (insert clone envelope with new ID but same epoch/msg)
  // Note: Simplified - deferred to Phase 13 full implementation  
  // action env_duplicate_message = { ... }
  
  // Note: Stale epoch delivery is naturally possible via normal delivery actions
  // if an envelope has epoch < current connEpoch. The staleness check happens
  // in receive actions via isStale() helper, and debug.lastWasStale tracks it.
  
  // ============================================================================
  // Zones Actions (Phase: zones slice)
  // ============================================================================
  
  // Node sends zones.get request
  action node_send_zones_get = {
    nondet msgId = 1.to(10000).oneOf()
    all {
      state.node.handshakeComplete,
      state.node.connState == "CONNECTED",
      not(deliveredMsgIds.contains(msgId)),
      state' = state,
      pendingMessages' = pendingMessages.union(Set(mkEnvelope(msgId, state.node.connEpoch, "node2hub", ZonesGet))),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Hub responds to zones.get with zones.list
  action hub_respond_zones_list = {
    all {
      hasZonesGetMessage(pendingMessages),
      state.hub.connState.get(TAB5_NODE) == "CONNECTED",
      state' = state,
      // Simplified: just remove ZonesGet from soup (actual list encoding would be more complex)
      pendingMessages' = pendingMessages.filter(e => match e.msg {
        | ZonesGet => false
        | _ => true
      }),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Node sends zones.update request
  action node_send_zones_update = {
    nondet msgId = 1.to(10000).oneOf()
    nondet zoneId = 0.to(3).oneOf()
    nondet effectId = 0.to(255).oneOf()
    nondet brightness = 0.to(255).oneOf()
    nondet speed = 1.to(100).oneOf()
    nondet paletteId = 0.to(255).oneOf()
    nondet blendMode = 0.to(7).oneOf()
    all {
      state.node.handshakeComplete,
      state.node.connState == "CONNECTED",
      not(deliveredMsgIds.contains(msgId)),
      state' = state,
      pendingMessages' = pendingMessages.union(Set(mkEnvelope(msgId, state.node.connEpoch, "node2hub", ZonesUpdate({
        zoneId: zoneId,
        effectId: effectId,
        brightness: brightness,
        speed: speed,
        paletteId: paletteId,
        blendMode: blendMode
      })))),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Hub applies zones.update and broadcasts zones.changed
  action hub_apply_zones_update = {
    all {
      hasZonesUpdateMessage(pendingMessages),
      state.hub.connState.get(TAB5_NODE) == "CONNECTED",
      // Simplified: zones update applied (full implementation would extract zone fields from message)
      state' = {
        node: state.node,
        hub: {
          connState: state.hub.connState,
          connEpoch: state.hub.connEpoch,
          activeParams: state.hub.activeParams,
          activeEffectId: state.hub.activeEffectId,
          lastBroadcastTs: state.hub.lastBroadcastTs,
          zoneCount: state.hub.zoneCount,
          // Simplified: update zones map (full implementation would extract from message)
          zones: state.hub.zones
        },
        debug: state.debug
      },
      pendingMessages' = pendingMessages.filter(e => match e.msg {
        | ZonesUpdate(_) => false
        | _ => true
      }),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // Node receives zones.changed broadcast
  action node_receive_zones_changed = {
    all {
      hasZonesChangedMessage(pendingMessages),
      state.node.connState == "CONNECTED",
      state' = state,
      pendingMessages' = pendingMessages.filter(e => match e.msg {
        | ZonesChanged(_) => false
        | _ => true
      }),
      pendingEvents' = pendingEvents,
      deliveredMsgIds' = deliveredMsgIds
    }
  }
  
  // ============================================================================
  // Main Step (Nondeterministic scheduler)
  // ============================================================================
  
  action step = {
    any {
      node_connect,
      env_deliver_connected,
      node_connected,
      hub_respond_status,
      node_receive_status,
      node_send_parameters,
      hub_apply_parameters,
      node_receive_params_changed,
      node_send_effect,
      hub_set_effect,
      node_disconnect,
      node_send_zones_get,
      hub_respond_zones_list,
      node_send_zones_update,
      hub_apply_zones_update,
      node_receive_zones_changed
    }
  }

}
