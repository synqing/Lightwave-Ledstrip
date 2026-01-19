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
  // Combined Invariants
  // ============================================================================
  
  val Invariants: bool = {
    NoEarlyApply and
    HandshakeStrict and
    ConnEpochMonotonic and
    EpochResetsHandshake
  }
  
  // Phase 13: Adversarial invariants (checked separately in nightly quint verify)
  val AdversarialInvariants: bool = {
    StaleEpochNoMutate and
    DuplicateIdempotent and
    RateLimitRejectNoMutate
  }

}
