/**
 * @file ZoneEffectPool.cpp
 * @brief D-1 keystone — implementation of ZoneEffectPool
 *
 * @see ZoneEffectPool.h for the design rationale and contract.
 */

#include "ZoneEffectPool.h"

#include "../../plugins/api/IEffect.h"

namespace lightwaveos {
namespace zones {

ZoneEffectPool::~ZoneEffectPool() {
    clear();
}

plugins::IEffect* ZoneEffectPool::acquire(EffectId effectId,
                                          uint8_t zoneSlot,
                                          const IZoneEffectSource& source) {
    // Reject obviously invalid inputs early. ZoneComposer should already
    // have validated zoneSlot via validateZoneId(); belt and braces here.
    if (effectId == INVALID_EFFECT_ID || zoneSlot >= MAX_ZONES) {
        return nullptr;
    }

    // 1) Cached hit — the (effectId, zoneSlot) pair already has an instance.
    //    This is the common case after the first acquire on a stable preset.
    for (uint8_t i = 0; i < m_slotCount; ++i) {
        if (m_slots[i].effectId == effectId && m_slots[i].zoneSlot == zoneSlot) {
            return m_slots[i].instance;
        }
    }

    // 2) Pool full — refuse to allocate. Caller can fall back to whatever
    //    the registry provides (typically the singleton). 9-slot capacity
    //    is generous enough that this should not fire in practice; if it
    //    does, the system has cycled effects 9+ times and should clear()
    //    on a fresh preset load. Surface to the caller as nullptr; the
    //    caller is responsible for any logging.
    if (m_slotCount >= kMaxSlots) {
        return nullptr;
    }

    // 3) Try the registered factory first — this is the path that gives
    //    true per-zone state isolation.
    plugins::IEffect* instance = nullptr;
    bool ownedByPool = false;

    EffectFactoryFn factory = source.getEffectFactory(effectId);
    if (factory != nullptr) {
        instance = factory();
        ownedByPool = (instance != nullptr);
    }

    // 4) Factory missing or returned nullptr — fall back to the registry
    //    singleton. The pool still caches it so we don't repeat the lookup,
    //    but `ownedByPool` stays false so we don't `delete` something the
    //    registry owns.
    //
    //    Limitation: for zoneSlot > 0 with a singleton fallback, the same
    //    instance can be cached in two slots, so calling render() twice per
    //    frame still corrupts state. That's the pre-D-1 behaviour and is
    //    documented in the ADR — phase-2 work registers factories for the
    //    affected effects to fix it.
    if (instance == nullptr) {
        instance = source.getEffectInstance(effectId);
    }

    if (instance == nullptr) {
        // Registry knows nothing — propagate the failure.
        return nullptr;
    }

    Slot& slot = m_slots[m_slotCount];
    slot.effectId = effectId;
    slot.zoneSlot = zoneSlot;
    slot.instance = instance;
    slot.ownedByPool = ownedByPool;
    ++m_slotCount;

    return instance;
}

plugins::IEffect* ZoneEffectPool::peek(EffectId effectId,
                                       uint8_t zoneSlot) const {
    if (effectId == INVALID_EFFECT_ID || zoneSlot >= MAX_ZONES) {
        return nullptr;
    }
    for (uint8_t i = 0; i < m_slotCount; ++i) {
        if (m_slots[i].effectId == effectId && m_slots[i].zoneSlot == zoneSlot) {
            return m_slots[i].instance;
        }
    }
    return nullptr;
}

void ZoneEffectPool::clear() {
    for (uint8_t i = 0; i < m_slotCount; ++i) {
        Slot& slot = m_slots[i];
        if (slot.ownedByPool && slot.instance != nullptr) {
            // Symmetry with init() — give the effect a chance to free its
            // PSRAM/scratch before the pool drops the heap allocation.
            slot.instance->cleanup();
            delete slot.instance;
        }
        slot.instance = nullptr;
        slot.ownedByPool = false;
        slot.effectId = INVALID_EFFECT_ID;
        slot.zoneSlot = 0;
    }
    m_slotCount = 0;
}

uint8_t ZoneEffectPool::ownedCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < m_slotCount; ++i) {
        if (m_slots[i].ownedByPool) {
            ++n;
        }
    }
    return n;
}

} // namespace zones
} // namespace lightwaveos
