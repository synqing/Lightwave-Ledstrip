#include <unity.h>

#include "effects/ReflectiveTwinPolicy.h"

using lightwaveos::effects::reflective_twin::allowDualChannel;
using lightwaveos::effects::reflective_twin::declaresDualChannel;
using lightwaveos::effects::reflective_twin::hasRoleFlag;
using lightwaveos::effects::reflective_twin::shouldMirrorUnified;
using lightwaveos::plugins::EffectMetadata;
using lightwaveos::plugins::EffectRoleFlags;

static void test_default_metadata_is_reflective_twin() {
    EffectMetadata meta{"Legacy", "legacy effect"};
    TEST_ASSERT_FALSE(declaresDualChannel(meta));
    TEST_ASSERT_FALSE(allowDualChannel(meta, true));
    TEST_ASSERT_TRUE(shouldMirrorUnified(meta, true));
}

static void test_dual_channel_flag_allows_direct_strip_mode() {
    EffectMetadata meta{"Interference", "dual strip effect",
                        lightwaveos::plugins::EffectCategory::QUANTUM,
                        1,
                        nullptr,
                        EffectRoleFlags::DUAL_CHANNEL};
    TEST_ASSERT_TRUE(declaresDualChannel(meta));
    TEST_ASSERT_TRUE(allowDualChannel(meta, true));
    TEST_ASSERT_FALSE(shouldMirrorUnified(meta, true));
}

static void test_dual_channel_flag_does_not_force_direct_mode() {
    EffectMetadata meta{"Declared", "does not request this frame",
                        lightwaveos::plugins::EffectCategory::AMBIENT,
                        1,
                        nullptr,
                        EffectRoleFlags::DUAL_CHANNEL};
    TEST_ASSERT_FALSE(allowDualChannel(meta, false));
    TEST_ASSERT_TRUE(shouldMirrorUnified(meta, false));
}

static void test_role_flag_helper_detects_composed_flags() {
    const auto flags = static_cast<EffectRoleFlags>(
        static_cast<uint8_t>(EffectRoleFlags::SELF_TRAILING) |
        static_cast<uint8_t>(EffectRoleFlags::DUAL_CHANNEL));

    TEST_ASSERT_TRUE(hasRoleFlag(flags, EffectRoleFlags::SELF_TRAILING));
    TEST_ASSERT_TRUE(hasRoleFlag(flags, EffectRoleFlags::DUAL_CHANNEL));
    TEST_ASSERT_FALSE(hasRoleFlag(flags, EffectRoleFlags::BACKGROUND));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_metadata_is_reflective_twin);
    RUN_TEST(test_dual_channel_flag_allows_direct_strip_mode);
    RUN_TEST(test_dual_channel_flag_does_not_force_direct_mode);
    RUN_TEST(test_role_flag_helper_detects_composed_flags);
    return UNITY_END();
}
