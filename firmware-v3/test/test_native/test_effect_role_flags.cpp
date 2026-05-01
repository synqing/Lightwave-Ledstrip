// INF-06 EffectRoleFlags — substrate test
//
// Per Topology_Reconciliation §6 item 9 / Phase 1 Move 1.2: a bitmask of role
// hints lives on EffectMetadata so downstream consumers (INF-02 mandatory LPF,
// COM-12 invert-input, COM-16 colour/geometry orthogonal split, COM-04
// background/foreground tagging, PER-09/11/13/14 opt-out hooks) can decide
// per-effect whether their pass applies without scanning render bodies.
//
// The flag set is intentionally small (6 named bits + 2 reserved) so the whole
// metadata field stays in one byte and existing effects keep their default
// behaviour (roleFlags = 0 → "legacy effect, behave as before").

#include <unity.h>
#include <cstdint>
#include <type_traits>

#include "plugins/api/IEffect.h"

using lightwaveos::plugins::EffectMetadata;
using lightwaveos::plugins::EffectRoleFlags;

namespace {

// 1 — Enum width: must fit in a single byte to keep EffectMetadata compact.
void test_role_flags_underlying_type_is_uint8() {
    TEST_ASSERT_EQUAL(1u, sizeof(EffectRoleFlags));
    static_assert(std::is_same<std::underlying_type<EffectRoleFlags>::type, uint8_t>::value,
                  "EffectRoleFlags must be backed by uint8_t");
}

// 2 — Each named flag occupies a distinct power-of-two bit position. No
// overlaps; downstream consumers AND the bit they care about against the
// metadata field, so collisions silently break role detection.
void test_role_flags_are_distinct_bits() {
    const uint8_t bits[] = {
        static_cast<uint8_t>(EffectRoleFlags::SELF_TRAILING),
        static_cast<uint8_t>(EffectRoleFlags::RENDERS_COLOUR_ONLY),
        static_cast<uint8_t>(EffectRoleFlags::RENDERS_GEOMETRY_ONLY),
        static_cast<uint8_t>(EffectRoleFlags::INVERT_INPUT_OK),
        static_cast<uint8_t>(EffectRoleFlags::BACKGROUND),
        static_cast<uint8_t>(EffectRoleFlags::OPTS_OUT_OF_PERSISTENCE),
    };
    constexpr size_t kCount = sizeof(bits) / sizeof(bits[0]);

    // Each bit must be a single power of two.
    for (size_t i = 0; i < kCount; ++i) {
        TEST_ASSERT_NOT_EQUAL_UINT8(0, bits[i]);
        TEST_ASSERT_EQUAL_UINT8(0, bits[i] & static_cast<uint8_t>(bits[i] - 1));
    }
    // No two flags share a bit.
    for (size_t i = 0; i < kCount; ++i) {
        for (size_t j = i + 1; j < kCount; ++j) {
            TEST_ASSERT_EQUAL_UINT8(0, bits[i] & bits[j]);
        }
    }
}

// 3 — Default-constructed metadata has zero flags, so all ~350 existing
// effects retain pre-INF-06 behaviour without source edits.
void test_metadata_role_flags_default_zero() {
    EffectMetadata meta{"Test", "desc"};
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(meta.roleFlags));
}

// 4 — Bitwise OR composes flags. INF-02 needs (SELF_TRAILING |
// OPTS_OUT_OF_PERSISTENCE) on the same effect when an effect bakes its own
// trail AND wants to skip the persistence pass.
void test_role_flags_compose_via_bitwise_or() {
    const auto combined = static_cast<EffectRoleFlags>(
        static_cast<uint8_t>(EffectRoleFlags::SELF_TRAILING) |
        static_cast<uint8_t>(EffectRoleFlags::OPTS_OUT_OF_PERSISTENCE));

    const uint8_t mask = static_cast<uint8_t>(combined);
    TEST_ASSERT_TRUE(mask & static_cast<uint8_t>(EffectRoleFlags::SELF_TRAILING));
    TEST_ASSERT_TRUE(mask & static_cast<uint8_t>(EffectRoleFlags::OPTS_OUT_OF_PERSISTENCE));
    TEST_ASSERT_FALSE(mask & static_cast<uint8_t>(EffectRoleFlags::RENDERS_COLOUR_ONLY));
}

// 5 — Construction with explicit flags sticks; metadata is plain data.
void test_metadata_constructed_with_role_flags_preserves_them() {
    using lightwaveos::plugins::EffectCategory;
    EffectMetadata meta{"TrailingFx",
                        "self-trailing",
                        EffectCategory::AMBIENT,
                        1,
                        nullptr,
                        EffectRoleFlags::SELF_TRAILING};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EffectRoleFlags::SELF_TRAILING),
                            static_cast<uint8_t>(meta.roleFlags));
}

}  // namespace

void run_effect_role_flags_tests() {
    RUN_TEST(test_role_flags_underlying_type_is_uint8);
    RUN_TEST(test_role_flags_are_distinct_bits);
    RUN_TEST(test_metadata_role_flags_default_zero);
    RUN_TEST(test_role_flags_compose_via_bitwise_or);
    RUN_TEST(test_metadata_constructed_with_role_flags_preserves_them);
}
