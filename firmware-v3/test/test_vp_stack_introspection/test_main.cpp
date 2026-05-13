#include <unity.h>

#include "core/diagnostics/VpStackIntrospection.h"

using lightwaveos::diagnostics::VpAuthoredSurface;
using lightwaveos::diagnostics::VpCorrectionSurface;
using lightwaveos::diagnostics::VpTopology;
using lightwaveos::diagnostics::deriveVpSurfaces;
using lightwaveos::diagnostics::topologyName;

void test_unified_surface_derivation() {
    const auto surfaces = deriveVpSurfaces(VpTopology::Unified, false, true);
    TEST_ASSERT_EQUAL(VpAuthoredSurface::UnifiedLeds, surfaces.authored);
    TEST_ASSERT_EQUAL(VpCorrectionSurface::UnifiedLeds, surfaces.correction);
    TEST_ASSERT_EQUAL_STRING("unified", topologyName(VpTopology::Unified));
    TEST_ASSERT_FALSE(surfaces.surfaceMismatch);
}

void test_direct_strip_surface_mismatch_is_explicit() {
    const auto surfaces = deriveVpSurfaces(VpTopology::DirectStrip, true, true);
    TEST_ASSERT_EQUAL(VpAuthoredSurface::PhysicalStrips, surfaces.authored);
    TEST_ASSERT_EQUAL(VpCorrectionSurface::UnifiedLeds, surfaces.correction);
    TEST_ASSERT_TRUE(surfaces.surfaceMismatch);
}

void test_correction_bypass_reports_no_correction_surface() {
    const auto surfaces = deriveVpSurfaces(VpTopology::ZoneUnified, false, false);
    TEST_ASSERT_EQUAL(VpAuthoredSurface::UnifiedLeds, surfaces.authored);
    TEST_ASSERT_EQUAL(VpCorrectionSurface::None, surfaces.correction);
    TEST_ASSERT_FALSE(surfaces.surfaceMismatch);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_unified_surface_derivation);
    RUN_TEST(test_direct_strip_surface_mismatch_is_explicit);
    RUN_TEST(test_correction_bypass_reports_no_correction_surface);
    return UNITY_END();
}
