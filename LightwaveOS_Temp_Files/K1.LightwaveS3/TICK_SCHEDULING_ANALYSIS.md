# Tick Scheduling Analysis: Should We Complete the Merge?

## Current State

**What's in the code:**
- `m_nextTick` member variable (declared and initialized) ✅
- Queue saturation prevention (draining logic) ✅
- **OLD** timeout-based tick scheduling (preserved intentionally) ⚠️

**Comment in code (line 333-334):**
```cpp
// CRITICAL: Tick happens on timeout, not by checking if due
// This preserves the original timing behavior that RendererNode depends on
```

## The Critical Question

**Why was the old behavior preserved?** The comment says "RendererNode depends on" timeout-based scheduling. But what does it actually depend on?

## Analysis: What RendererNode Actually Needs

### RendererNode Requirements:
1. **120 FPS timing** - `onTick()` called every ~8ms
2. **Deterministic cadence** - Consistent frame timing
3. **Message processing** - Can handle commands between frames

### Old Behavior (Timeout-Based):
```
Loop:
  1. Wait for message with timeout = 8ms
  2. If message arrives → process it
  3. If timeout → call onTick() (render frame)
```

**Timeline:**
```
Time:  0ms    8ms    16ms   24ms
      [Wait] [Wait] [Wait] [Wait]
       |      |      |      |
      Tick   Tick   Tick   Tick
      (on timeout)
```

**Characteristics:**
- Tick happens **exactly** 8ms after previous tick (if no messages)
- If `onTick()` takes 6ms, next tick starts at 14ms (6ms late)
- **Drift accumulates** if `onTick()` duration varies

### New Behavior (Deadline-Based):
```
Loop:
  1. Check if tick deadline passed → run onTick() immediately
  2. Calculate wait time until next deadline
  3. Wait for message with calculated timeout
  4. Process message if received
```

**Timeline:**
```
Time:  0ms    8ms    16ms   24ms
      [Tick] [Tick] [Tick] [Tick]
       |      |      |      |
      Deadline-based (runs immediately when due)
```

**Characteristics:**
- Tick happens **immediately** when deadline reached
- If `onTick()` takes 6ms, deadline advances to 16ms (no drift)
- **No drift accumulation** - deadline always advances by interval

## The Key Difference

**Old:** Tick happens on queue timeout (passive)
**New:** Tick happens when deadline reached (active check)

## Why the Comment Exists

The comment was added **after** the merge commit, suggesting someone intentionally reverted to old behavior. Possible reasons:

1. **Testing showed issues** with new behavior
2. **RendererNode timing assumptions** - might assume tick happens on timeout
3. **MusicalGrid timing** - line 807: "Tick MusicalGrid at 120 FPS" - might need exact timing
4. **AudioTime extrapolation** - might depend on consistent tick intervals

## Risk Assessment

### Completing the Merge (New Behavior):
**Risks:**
- ⚠️ Might break 120 FPS timing assumptions
- ⚠️ MusicalGrid might get ticked at wrong intervals
- ⚠️ AudioTime extrapolation might drift
- ⚠️ Unknown timing dependencies in effects

**Benefits:**
- ✅ Better handling of variable `onTick()` duration
- ✅ No drift accumulation
- ✅ More resilient to blocking operations

### Keeping Current Behavior (Old):
**Risks:**
- ⚠️ Timing drift if `onTick()` duration varies
- ⚠️ Less resilient to blocking operations

**Benefits:**
- ✅ Proven stable (current system works)
- ✅ Matches RendererNode's expectations
- ✅ No risk of breaking timing-sensitive code

## Recommendation

**DO NOT complete the merge without testing.** The comment suggests the old behavior was intentionally preserved for a reason. 

**Safer approach:**
1. **Keep current behavior** (old timeout-based) - it's working
2. **Remove `m_nextTick`** if not using it (cleanup)
3. **OR** document why `m_nextTick` exists but isn't used (future enhancement)

**If you want to test new behavior:**
1. Add feature flag to switch between old/new
2. Test extensively with zone mode, audio sync, MusicalGrid
3. Monitor FPS and timing accuracy
4. Only enable if tests pass

## Conclusion

**I am NOT certain** completing the merge is safe. The explicit comment preserving old behavior suggests it was tested and found necessary. The new behavior might work, but without runtime testing, it's risky to change timing-critical code.

**Recommendation:** Keep current behavior, remove unused `m_nextTick` OR document it as future enhancement.

