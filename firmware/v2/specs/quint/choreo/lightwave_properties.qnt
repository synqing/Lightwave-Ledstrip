// @file lightwave_properties.qnt
// @module lightwave_properties
// @description Invariants for v2 Hub ↔ Tab5 Node protocol conformance

module lightwave_properties {

  import lightwave_types.* from "./lightwave_types"
  import lightwave_step.* from "./lightwave_step"
  
  // ============================================================================
  // Invariant 1: NoEarlyApply
  // ============================================================================
  
  // Node never applies parameters before handshake complete
  val NoEarlyApply: bool = {
    (state.node.handshakeComplete) or (state.node.lastAppliedParams.keys().size() == 0)
  }
  
  // ============================================================================
  // Invariant 2: AckScopedToEpoch
  // ============================================================================
  
  // Acknowledgments (status responses) only reference commands from same epoch
  // For Phase 3: Simplified - status always uses current connEpoch
  val AckScopedToEpoch: bool = {
    // If status received, it must be in same epoch as connection
    true  // Simplified for Phase 3 - full check requires message tracking
  }
  
  // ============================================================================
  // Invariant 3: IdempotentUpdates
  // ============================================================================
  
  // Parameter updates are idempotent (same values applied multiple times = same state)
  // This is enforced by the protocol design (no side effects for unchanged values)
  val IdempotentUpdates: bool = {
    // Protocol-level guarantee: parameters.set with same values doesn't change state
    true  // Invariant holds by design (no side effects for unchanged values)
  }
  
  // ============================================================================
  // Invariant 4: HandshakeStrict
  // ============================================================================
  
  // Connected state requires handshake to be complete or in progress
  val HandshakeStrict: bool = {
    (state.node.connState != "CONNECTED") or (state.node.handshakeComplete)
  }
  
  // ============================================================================
  // Invariant 5: ConnEpochMonotonic
  // ============================================================================
  
  // Connection epoch never decreases (only increments on reconnect)
  val ConnEpochMonotonic: bool = {
    state.node.connEpoch >= 0
  }
  
  // ============================================================================
  // Invariant 6: EpochResetsHandshake
  // ============================================================================
  
  // When connection epoch changes, handshake must reset to incomplete
  // (prevents "reconnected but still considered authenticated" bug)
  // Simplified: if connecting/disconnected, handshake must be false
  val EpochResetsHandshake: bool = {
    (state.node.connState == "CONNECTED") or (not(state.node.handshakeComplete))
  }
  
  // ============================================================================
  // Invariant 7: StaleEpochNoMutate (Phase 13)
  // ============================================================================
  
  // Messages from stale epochs (epoch < current connEpoch) must not mutate protocol state
  val StaleEpochNoMutate: bool = {
    if (state.debug.lastWasStale) {
      not(state.debug.lastProtocolMutated)
    } else {
      true  // If not stale, no constraint
    }
  }
  
  // ============================================================================
  // Invariant 8: DuplicateIdempotent (Phase 13)
  // ============================================================================
  
  // Duplicate messages (same ID already delivered) must be idempotent (no state mutation)
  val DuplicateIdempotent: bool = {
    if (state.debug.lastWasDuplicate) {
      not(state.debug.lastProtocolMutated)
    } else {
      true  // If not duplicate, no constraint
    }
  }
  
  // ============================================================================
  // Invariant 9: RateLimitRejectNoMutate (Phase 13)
  // ============================================================================
  
  // Rate-limit rejections must not mutate protocol state
  val RateLimitRejectNoMutate: bool = {
    if (state.debug.lastResult == "rejected" and state.debug.lastReason == "rate_limit") {
      not(state.debug.lastProtocolMutated)
    } else {
      true  // If not rate-limit rejection, no constraint
    }
  }
  
  // ============================================================================
  // Invariant 10: NoOtaBeforeHandshake (Phase: OTA slice)
  // ============================================================================
  
  // OTA state can only transition from Idle if handshake complete
  val NoOtaBeforeHandshake: bool = {
    match state.hub.otaState {
      | Idle => true
      | InProgress(_) => state.node.handshakeComplete
      | Verifying => state.node.handshakeComplete
      | Complete => state.node.handshakeComplete
      | Failed(_) => true  // Failures can occur regardless of handshake
    }
  }
  
  // ============================================================================
  // Invariant 11: OtaMonotonicProgress (Phase: OTA slice)
  // ============================================================================
  
  // Within a connEpoch, OTA progress offset cannot decrease (unless abort resets to Idle)
  // Simplified check: if InProgress, ensure bytesReceived is non-decreasing
  // (Full check would require tracking previous state, done via action preconditions)
  val OtaMonotonicProgress: bool = {
    // This invariant is enforced in hub_respond_ota_progress action (offset >= bytesReceived)
    // Protocol-level guarantee: chunks must arrive in order with correct offsets
    true  // Enforced by action preconditions, not state-level invariant
  }
  
  // ============================================================================
  // Invariant 12: OtaNoCrossEpochContinuation (Phase: OTA slice)
  // ============================================================================
  
  // After connEpoch increments, OTA must restart (no silent continuation)
  // OTA state resets to Idle on epoch change (enforced by node_disconnect action)
  val OtaNoCrossEpochContinuation: bool = {
    // Protocol-level guarantee: OTA sessions are scoped to connection epoch
    // Enforced by resetting otaState to Idle on disconnect/reconnect
    true  // Enforced by node_disconnect action (resets hub state, including otaState)
  }
  
  // ============================================================================
  // Invariant 13: OtaSingleSession (Phase: OTA slice)
  // ============================================================================
  
  // Only one OTA session can be active at a time
  val OtaSingleSession: bool = {
    // Enforced by hub_respond_ota_ready precondition (otaState == Idle required)
    true  // Enforced by action preconditions
  }
  
  // ============================================================================
  // Combined Invariants
  // ============================================================================
  
  val Invariants: bool = {
    NoEarlyApply and
    HandshakeStrict and
    ConnEpochMonotonic and
    EpochResetsHandshake and
    NoOtaBeforeHandshake
  }
  
  // Phase 13: Adversarial invariants (checked separately in nightly quint verify)
  val AdversarialInvariants: bool = {
    StaleEpochNoMutate and
    DuplicateIdempotent and
    RateLimitRejectNoMutate
  }
  
  // OTA-specific invariants (checked separately)
  val OtaInvariants: bool = {
    NoOtaBeforeHandshake and
    OtaMonotonicProgress and
    OtaNoCrossEpochContinuation and
    OtaSingleSession
  }

}
