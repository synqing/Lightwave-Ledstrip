/**
 * @file ZoneEffectPool.h
 * @brief D-1 keystone — per-zone effect instance pool for ZoneComposer
 *
 * LightwaveOS v2 - Zone System
 *
 * The pool guarantees that each `(effectId, zoneSlot)` pair owns a distinct
 * `IEffect` instance, so the SAME effectId assigned to two or three zones
 * simultaneously no longer corrupts cross-zone state via singleton sharing.
 *
 * Background
 * ----------
 * The pre-D-1 code path called `RendererActor::getEffectInstance(effectId)`
 * once per zone per frame. That returns a singleton: if Zone 1 and Zone 2
 * both ran `EID_LGP_HOLOGRAPHIC`, both rendered through the same instance
 * and rewrote each other's `m_phase`, accumulators, and PSRAM trail buffers.
 *
 * The pool is allowed to allocate (heap / `new`) — `acquire()` is invoked
 * from preset-load / setZoneEffect command paths on Core 0, never from
 * `render()`. ZoneComposer caches the resolved `IEffect*` per zone slot, so
 * the render-time lookup is O(1) (array dereference, no map walk).
 *
 * Capacity & lifetime
 * -------------------
 * Slot capacity: `MAX_ZONES * 3 = 9` instances total. This is a generous
 * upper bound — in practice, with 3 zones and at most 3 effect changes per
 * zone in a session, the pool stays at <= 3 in-use entries plus a few
 * lingering ones. LRU eviction is OUT OF SCOPE for this keystone (flagged
 * in the ADR follow-up note).
 *
 * Pool entries persist for the session — when a zone switches effect, the
 * old slot lingers (cheap; saves re-allocation cost on toggle). Only on
 * full pool teardown (system shutdown / explicit clear) is `cleanup()`
 * invoked and the heap-owned instances freed.
 *
 * Factory fallback (degraded mode)
 * --------------------------------
 * Effects with NO registered factory fall back to the registry singleton.
 * For zone slots beyond zone 0, this re-introduces the original singleton
 * sharing problem — but only for effects whose authors have not opted in.
 * Phase-2 work will register factories for the high-priority effects (K1
 * Bloom, K1 Waveform, the Enhanced LGP family). Until then, callers should
 * treat singleton fallback as KNOWN-BAD for multi-zone use of the same
 * effectId, and the pool surfaces this to logs.
 *
 * @see docs/adr/zone-composer-architecture-decisions.md § D-1
 * @see firmware-v3/src/effects/zones/ZoneComposer.cpp (consumer)
 */

#pragma once

#include <cstdint>
#include "../../config/effect_ids.h"
#include "ZoneDefinition.h"

namespace lightwaveos {
namespace plugins { class IEffect; class EffectContext; }

namespace zones {

/**
 * @brief Factory function type — constructs a fresh IEffect instance.
 *
 * The pool calls this when it needs a per-zone instance for an effectId
 * that is already in use by another zone. The returned pointer is owned
 * by the pool (`delete`d at pool teardown).
 *
 * Implementers should `return new MyEffect();` and ensure the type's
 * default constructor leaves the instance in a state where a subsequent
 * `init()` call fully initialises it.
 */
using EffectFactoryFn = plugins::IEffect* (*)();

/**
 * @brief Minimal source interface the pool needs from the effect registry.
 *
 * RendererActor implements this in the firmware build. Native tests provide
 * a mock that records calls for assertions. Keeping this small (two methods)
 * lets the pool be unit-tested without dragging in the full FreeRTOS-bound
 * RendererActor stack.
 */
class IZoneEffectSource {
public:
    virtual ~IZoneEffectSource() = default;

    /**
     * @brief Look up the registered singleton instance for an effectId.
     * @param id Effect ID
     * @return Singleton `IEffect*`, or `nullptr` if unregistered.
     *
     * Used as the degraded-mode fallback when no factory is registered.
     */
    virtual plugins::IEffect* getEffectInstance(EffectId id) const = 0;

    /**
     * @brief Look up the registered factory for an effectId.
     * @param id Effect ID
     * @return Factory function, or `nullptr` if no factory was registered.
     *
     * When `nullptr`, the pool falls back to `getEffectInstance(id)` and
     * accepts the singleton-sharing limitation for that effect.
     */
    virtual EffectFactoryFn getEffectFactory(EffectId id) const = 0;
};

// ==================== ZoneEffectPool ====================

/**
 * @brief Per-zone IEffect instance pool keyed by (effectId, zoneSlot).
 *
 * Memory: `kMaxSlots * sizeof(Slot)` ~= 9 * 16 = 144 bytes for the slot
 * table itself. Per-instance cost is paid by each effect's own `init()`
 * (typically 3-5 KB PSRAM via `heap_caps_malloc(MALLOC_CAP_SPIRAM)`).
 *
 * Thread safety: All methods are intended for Core 0 (command/preset
 * dispatch path). The pool MUST NOT be mutated from `render()` on Core 1.
 * ZoneComposer caches the resolved pointer in `m_zoneActiveEffects[]` so
 * the render path performs only an array dereference.
 */
class ZoneEffectPool {
public:
    /**
     * @brief Pool capacity — bounded at ~3 × kMaxZones per ADR D-1.
     *
     * 3 zones × 3 effect rotations per session = 9 max simultaneous
     * pool slots. Beyond this the pool refuses to allocate; the caller
     * sees the existing singleton fallback.
     */
    static constexpr uint8_t kMaxSlots = MAX_ZONES * 3;

    ZoneEffectPool() = default;
    ~ZoneEffectPool();

    // Non-copyable — instance pointers are owned, copy semantics are unsafe.
    ZoneEffectPool(const ZoneEffectPool&) = delete;
    ZoneEffectPool& operator=(const ZoneEffectPool&) = delete;

    /**
     * @brief Acquire (and lazily create) the IEffect instance for a (effectId, zoneSlot).
     *
     * - If the slot is already populated, returns the cached pointer.
     * - Else, if a factory is registered, constructs a fresh instance via
     *   `new`, marks it pool-owned, and stores it.
     * - Else, falls back to the registry singleton (NOT pool-owned). For
     *   zone slots > 0 this re-introduces singleton sharing — caller should
     *   surface a log warning.
     *
     * @param effectId Stable namespaced effect ID.
     * @param zoneSlot Zone slot index in [0, MAX_ZONES). Out-of-range
     *                 values are rejected (returns nullptr).
     * @param source Effect registry adapter (RendererActor in firmware,
     *               mock in tests).
     * @return Acquired instance, or `nullptr` if `effectId == INVALID_EFFECT_ID`,
     *         `zoneSlot >= MAX_ZONES`, the pool is full and no existing slot
     *         matches, OR the registry knows nothing about the effect.
     *
     * NOT safe to call from `render()` — may allocate.
     */
    plugins::IEffect* acquire(EffectId effectId, uint8_t zoneSlot,
                              const IZoneEffectSource& source);

    /**
     * @brief Look up an existing slot without creating a new one.
     *
     * Intended for telemetry / debug paths.
     *
     * @return Cached instance, or `nullptr` if the (effectId, zoneSlot)
     *         pair is not in the pool.
     */
    plugins::IEffect* peek(EffectId effectId, uint8_t zoneSlot) const;

    /**
     * @brief Free all heap-owned instances and clear the slot table.
     *
     * Calls `cleanup()` on each pool-owned instance, then `delete`s it.
     * Singleton fallbacks are NOT freed (they're owned by the registry).
     *
     * Intended for system shutdown or full pool reset. Not called on
     * per-zone effect changes (those just leave the old slot lingering).
     */
    void clear();

    /**
     * @brief Number of currently populated slots (test / telemetry helper).
     */
    uint8_t size() const { return m_slotCount; }

    /**
     * @brief Number of pool-owned (factory-constructed) instances —
     *        i.e. how many slots are NOT singleton fallbacks.
     */
    uint8_t ownedCount() const;

private:
    struct Slot {
        EffectId effectId = INVALID_EFFECT_ID;
        uint8_t zoneSlot = 0;
        plugins::IEffect* instance = nullptr;
        bool ownedByPool = false;  // true => `delete instance` on clear()
    };

    Slot m_slots[kMaxSlots];
    uint8_t m_slotCount = 0;
};

} // namespace zones
} // namespace lightwaveos
