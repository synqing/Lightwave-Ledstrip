// @file lightwave_system.qnt
// @module lightwave_system
// @description Unified system module for Quint verification
// Phase 13: Re-exports step + properties so CI can reference invariants correctly

module lightwave_system {

  import lightwave_step.* from "./lightwave_step"
  import lightwave_properties.* from "./lightwave_properties"
  
  // With wildcard imports, all names (init, step, Invariants, AdversarialInvariants, etc.)
  // are already available in this module's namespace and can be referenced directly.
  // No re-exports needed - quint verify can access them via this module.

}
