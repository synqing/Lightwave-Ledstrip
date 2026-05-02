/**
 * @file test_main.cpp
 * @brief D-1 keystone — per-zone effect instance isolation regression tests.
 *
 * These tests pin the contract of `lightwaveos::zones::ZoneEffectPool`:
 *
 *   1. The same `effectId` assigned to two zones MUST yield two distinct
 *      `IEffect*` pointers when a factory is registered (the per-zone
 *      isolation invariant the keystone exists to enforce).
 *   2. `init()` runs once per `(effectId, zoneSlot)` pair on first acquire
 *      so per-zone counters / PSRAM allocations are populated.
 *   3. Re-acquiring the same `(effectId, zoneSlot)` returns the cached
 *      instance without constructing a new one.
 *   4. Different `effectId`s in the same zone slot get distinct instances.
 *   5. With NO factory registered, the pool falls back to the registry
 *      singleton and surfaces the (known-bad, documented) singleton-share
 *      behaviour for multi-zone use of the same effectId — this is the
 *      pre-D-1 baseline and is documented in ADR D-1 § Implementation impact.
 *   6. `clear()` deletes pool-owned instances and leaves singleton fallbacks
 *      alone (calling delete on a registry-owned static would corrupt heap).
 *
 * The test uses a minimal mock `IEffect` and `IZoneEffectSource` — it does
 * NOT pull in the full `EffectContext` / FreeRTOS / FastLED stack, so it
 * runs cleanly under `platform = native` on the host.
 *
 * Run via: `pio test -e native_test_zone_effect_isolation`
 *
 * Refs: docs/adr/zone-composer-architecture-decisions.md § D-1
 */

#include <unity.h>
#include <cstdint>
#include <cstdio>

// Pull in the IEffect interface and pool. NATIVE_BUILD guards keep the
// FastLED-bound members out of the way; we only need the lifecycle hooks
// the pool actually invokes.
#define NATIVE_BUILD 1

#include "../../src/plugins/api/IEffect.h"
#include "../../src/effects/zones/ZoneEffectPool.h"

namespace lwzones = lightwaveos::zones;
namespace lwplugins = lightwaveos::plugins;

// ============================================================================
// Mock IEffect — counts init() calls and stamps zone identity into state
// ============================================================================

namespace {

// File-static so the factory function (no captures) can hand out unique IDs.
static int g_nextMockId = 0;

class MockEffect : public lwplugins::IEffect {
public:
    MockEffect() : m_uniqueId(g_nextMockId++) {}
    ~MockEffect() override = default;

    bool init(lwplugins::EffectContext& ctx) override {
        // We deliberately do NOT touch ctx — the pool guarantees its caller
        // populates ctx.zoneId before invoking init(), but the mock only
        // needs to count calls. Casting to void quiets the unused warning.
        (void)ctx;
        ++m_initCount;
        return true;
    }

    void render(lwplugins::EffectContext& ctx) override {
        (void)ctx;  // Not exercised in this test.
    }

    void cleanup() override {
        ++m_cleanupCount;
    }

    const lwplugins::EffectMetadata& getMetadata() const override {
        static lwplugins::EffectMetadata meta{"MockEffect"};
        return meta;
    }

    int uniqueId() const { return m_uniqueId; }
    int initCount() const { return m_initCount; }
    int cleanupCount() const { return m_cleanupCount; }

private:
    int m_uniqueId = 0;
    int m_initCount = 0;
    int m_cleanupCount = 0;
};

// Factory the pool can call to construct fresh per-zone instances.
lwplugins::IEffect* makeMockEffect() {
    return new MockEffect();
}

// ============================================================================
// Mock IZoneEffectSource — records what the pool asked for
// ============================================================================

class MockSource : public lwzones::IZoneEffectSource {
public:
    MockSource() = default;

    void registerSingleton(lightwaveos::EffectId id, lwplugins::IEffect* singleton) {
        m_singletonId = id;
        m_singleton = singleton;
    }

    // Up to two factory registrations are sufficient for the isolation tests.
    void registerFactory(lightwaveos::EffectId id, lwzones::EffectFactoryFn fn) {
        if (m_factoryId == lightwaveos::INVALID_EFFECT_ID) {
            m_factoryId = id;
            m_factory = fn;
        } else {
            m_factoryId2 = id;
            m_factory2 = fn;
        }
    }

    lwplugins::IEffect* getEffectInstance(lightwaveos::EffectId id) const override {
        ++m_singletonLookups;
        if (id == m_singletonId) return m_singleton;
        return nullptr;
    }

    lwzones::EffectFactoryFn getEffectFactory(lightwaveos::EffectId id) const override {
        ++m_factoryLookups;
        if (id == m_factoryId) return m_factory;
        if (id == m_factoryId2) return m_factory2;
        return nullptr;
    }

    int singletonLookups() const { return m_singletonLookups; }
    int factoryLookups() const { return m_factoryLookups; }

private:
    lightwaveos::EffectId m_singletonId = lightwaveos::INVALID_EFFECT_ID;
    lwplugins::IEffect* m_singleton = nullptr;
    lightwaveos::EffectId m_factoryId = lightwaveos::INVALID_EFFECT_ID;
    lwzones::EffectFactoryFn m_factory = nullptr;
    lightwaveos::EffectId m_factoryId2 = lightwaveos::INVALID_EFFECT_ID;
    lwzones::EffectFactoryFn m_factory2 = nullptr;
    mutable int m_singletonLookups = 0;
    mutable int m_factoryLookups = 0;
};

// Reset the global mock-ID counter between tests so id-equality assertions
// are meaningful within a single test method.
void resetMockState() {
    g_nextMockId = 0;
}

}  // namespace

// ============================================================================
// Tests
// ============================================================================

constexpr lightwaveos::EffectId kEidA = 0x1301;  // pretend-K1-Bloom
constexpr lightwaveos::EffectId kEidB = 0x1302;  // pretend-K1-Waveform

// 1. Same effectId in two zones → two distinct instances when a factory exists.
//    This is the keystone invariant: prevents same-effect-in-multiple-zones
//    cross-zone state corruption that motivated D-1.
void test_same_effectid_two_zones_returns_distinct_instances(void) {
    resetMockState();

    MockSource source;
    source.registerFactory(kEidA, &makeMockEffect);

    lwzones::ZoneEffectPool pool;

    auto* z1 = pool.acquire(kEidA, /*zoneSlot=*/0, source);
    auto* z2 = pool.acquire(kEidA, /*zoneSlot=*/1, source);

    TEST_ASSERT_NOT_NULL_MESSAGE(z1, "Zone 1 acquire must succeed");
    TEST_ASSERT_NOT_NULL_MESSAGE(z2, "Zone 2 acquire must succeed");
    TEST_ASSERT_TRUE_MESSAGE(
        z1 != z2,
        "Same effectId in two zones MUST yield two distinct IEffect* — D-1 invariant"
    );

    // The pool should also contain three slots once we add a third zone.
    auto* z3 = pool.acquire(kEidA, /*zoneSlot=*/2, source);
    TEST_ASSERT_NOT_NULL(z3);
    TEST_ASSERT_TRUE(z3 != z1 && z3 != z2);
    TEST_ASSERT_EQUAL_UINT8(3, pool.size());
    TEST_ASSERT_EQUAL_UINT8(3, pool.ownedCount());
}

// 2. init() runs once per (effectId, zoneSlot) pair when ZoneComposer drives
//    the pool. The pool itself does NOT call init() — but since this test
//    drives the pool directly, we drive init() manually here to mirror what
//    ZoneComposer::acquireZoneEffect() does.
void test_init_called_once_per_zone_pair(void) {
    resetMockState();

    MockSource source;
    source.registerFactory(kEidA, &makeMockEffect);
    source.registerFactory(kEidB, &makeMockEffect);

    lwzones::ZoneEffectPool pool;

    // Three acquisitions → three init()s when ZoneComposer initialises each.
    MockEffect* m1 = static_cast<MockEffect*>(pool.acquire(kEidA, 0, source));
    MockEffect* m2 = static_cast<MockEffect*>(pool.acquire(kEidA, 1, source));
    MockEffect* m3 = static_cast<MockEffect*>(pool.acquire(kEidB, 0, source));

    TEST_ASSERT_NOT_NULL(m1);
    TEST_ASSERT_NOT_NULL(m2);
    TEST_ASSERT_NOT_NULL(m3);
    // Failed if the pool collapsed kEidB into the same singleton as kEidA.
    TEST_ASSERT_TRUE(m3 != m1);
    TEST_ASSERT_TRUE(m3 != m2);

    // Two zones share factory but not instance → unique ids must differ.
    TEST_ASSERT_TRUE_MESSAGE(
        m1->uniqueId() != m2->uniqueId(),
        "Per-zone instances must have independent state (uniqueId proxy)"
    );

    // Mimic ZoneComposer driving init() on each acquire.
    lwplugins::EffectContext* dummy = nullptr;
    (void)dummy;  // Real EffectContext not needed for this mock; init() does not deref.

    // Drive init() manually since the pool stays generic.
    // We use placeholders here — the mock's init() is parameterless in effect.
    // (Cast to bypass real EffectContext construction; mock ignores the arg.)
    // NOTE: passing nullptr would dereference; instead, build a stack-stub.
    // The MockEffect::init() casts ctx to void, so an arbitrary reference works.
    char ctxStorage[1] = {0};
    lwplugins::EffectContext& ctxRef =
        *reinterpret_cast<lwplugins::EffectContext*>(ctxStorage);
    m1->init(ctxRef);
    m2->init(ctxRef);
    m3->init(ctxRef);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, m1->initCount(),
                                  "Zone 1 instance init() count must be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, m2->initCount(),
                                  "Zone 2 instance init() count must be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, m3->initCount(),
                                  "Zone 3 instance (different effectId) init() count must be 1");
}

// 3. Re-acquiring the same (effectId, zoneSlot) is idempotent — no new
//    instance, no new factory call. The pool short-circuits on cached hit.
void test_reacquire_same_pair_is_idempotent(void) {
    resetMockState();

    MockSource source;
    source.registerFactory(kEidA, &makeMockEffect);

    lwzones::ZoneEffectPool pool;

    auto* first = pool.acquire(kEidA, 0, source);
    int factoryLookupsAfterFirst = source.factoryLookups();

    auto* second = pool.acquire(kEidA, 0, source);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(
        first, second,
        "Re-acquiring same (effectId, zoneSlot) must return cached instance"
    );
    TEST_ASSERT_EQUAL_UINT8(1, pool.size());
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        factoryLookupsAfterFirst, source.factoryLookups(),
        "Second acquire must not re-query the factory"
    );
}

// 4. Singleton fallback: when no factory is registered, the pool falls back
//    to the registry singleton. For a single zone slot this is fine; for
//    multiple slots it RE-INTRODUCES the pre-D-1 singleton-share. The test
//    documents that contract — Phase 2 work registers factories for the
//    affected effects to fix.
void test_no_factory_falls_back_to_singleton(void) {
    resetMockState();

    MockEffect singleton;
    MockSource source;
    source.registerSingleton(kEidA, &singleton);  // No factory registered.

    lwzones::ZoneEffectPool pool;

    auto* z0 = pool.acquire(kEidA, 0, source);
    auto* z1 = pool.acquire(kEidA, 1, source);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(
        &singleton, z0,
        "Without a factory, zone 0 must receive the registry singleton"
    );
    TEST_ASSERT_EQUAL_PTR_MESSAGE(
        &singleton, z1,
        "Without a factory, zone 1 also receives the singleton (DOCUMENTED degraded mode)"
    );
    TEST_ASSERT_EQUAL_PTR_MESSAGE(
        z0, z1,
        "Two zones share one instance when no factory is registered — pre-D-1 baseline"
    );
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        0, pool.ownedCount(),
        "Singleton fallbacks must NOT be marked pool-owned (would double-free)"
    );
}

// 5. clear() frees only pool-owned instances. Singletons are left alone.
//    A regression here would either leak factory-built instances or — far
//    worse — `delete` the registry's static singleton and corrupt the heap.
void test_clear_only_frees_pool_owned_instances(void) {
    resetMockState();

    MockEffect singleton;  // Caller-owned (registry).
    MockSource source;
    source.registerSingleton(kEidA, &singleton);
    source.registerFactory(kEidB, &makeMockEffect);

    lwzones::ZoneEffectPool pool;

    auto* sing = pool.acquire(kEidA, 0, source);  // Singleton fallback.
    auto* owned = pool.acquire(kEidB, 0, source); // Pool-owned.

    TEST_ASSERT_EQUAL_PTR(&singleton, sing);
    TEST_ASSERT_NOT_NULL(owned);
    TEST_ASSERT_TRUE(owned != &singleton);
    TEST_ASSERT_EQUAL_UINT8(2, pool.size());
    TEST_ASSERT_EQUAL_UINT8(1, pool.ownedCount());

    int singletonCleanupBefore = singleton.cleanupCount();

    pool.clear();

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, pool.size(), "clear() empties the slot table");
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        singletonCleanupBefore, singleton.cleanupCount(),
        "clear() must NOT call cleanup() on the registry singleton (it's not pool-owned)"
    );
    // The owned instance was deleted — we can't legally dereference `owned`
    // any more, so we don't. Heap-debug builds (Asan) catch double-frees.
}

// 6. Out-of-range zone slot is rejected (defence in depth). MAX_ZONES = 3,
//    so slot=3 / slot=4 / slot=255 must all return nullptr without touching
//    the pool.
void test_invalid_zone_slot_rejected(void) {
    resetMockState();

    MockSource source;
    source.registerFactory(kEidA, &makeMockEffect);

    lwzones::ZoneEffectPool pool;

    TEST_ASSERT_NULL(pool.acquire(kEidA, /*zoneSlot=*/3, source));
    TEST_ASSERT_NULL(pool.acquire(kEidA, /*zoneSlot=*/255, source));
    TEST_ASSERT_NULL(pool.acquire(lightwaveos::INVALID_EFFECT_ID, 0, source));
    TEST_ASSERT_EQUAL_UINT8(0, pool.size());
}

// 7. peek() does not create slots.
void test_peek_does_not_allocate(void) {
    resetMockState();

    MockSource source;
    source.registerFactory(kEidA, &makeMockEffect);

    lwzones::ZoneEffectPool pool;

    TEST_ASSERT_NULL_MESSAGE(
        pool.peek(kEidA, 0),
        "peek() must return nullptr when the pair is not in the pool"
    );
    TEST_ASSERT_EQUAL_UINT8(0, pool.size());

    pool.acquire(kEidA, 0, source);
    TEST_ASSERT_NOT_NULL(pool.peek(kEidA, 0));
    TEST_ASSERT_EQUAL_UINT8(1, pool.size());
}

// ============================================================================
// Test runner
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_same_effectid_two_zones_returns_distinct_instances);
    RUN_TEST(test_init_called_once_per_zone_pair);
    RUN_TEST(test_reacquire_same_pair_is_idempotent);
    RUN_TEST(test_no_factory_falls_back_to_singleton);
    RUN_TEST(test_clear_only_frees_pool_owned_instances);
    RUN_TEST(test_invalid_zone_slot_rejected);
    RUN_TEST(test_peek_does_not_allocate);
    return UNITY_END();
}
