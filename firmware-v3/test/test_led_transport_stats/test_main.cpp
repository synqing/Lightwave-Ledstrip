#include <unity.h>

#include "hal/interface/LedTransportStats.h"

using lightwaveos::hal::LedDriverStats;
using lightwaveos::hal::LedTransportTimingSample;
using lightwaveos::hal::recordLedTransportSample;

void test_records_transport_boundaries_without_losing_legacy_show_time() {
    LedDriverStats stats{};

    LedTransportTimingSample sample{};
    sample.totalShowUs = 6100;
    sample.outputPrepUs = 120;
    sample.fastLedShowCallUs = 340;
    sample.rmtFenceUs = 5600;
    sample.latchWaitUs = 40;

    recordLedTransportSample(stats, sample);

    TEST_ASSERT_EQUAL_UINT32(1, stats.frameCount);
    TEST_ASSERT_EQUAL_UINT32(6100, stats.lastShowUs);
    TEST_ASSERT_EQUAL_UINT32(6100, stats.avgShowUs);
    TEST_ASSERT_EQUAL_UINT32(6100, stats.maxShowUs);
    TEST_ASSERT_EQUAL_UINT32(120, stats.lastOutputPrepUs);
    TEST_ASSERT_EQUAL_UINT32(120, stats.avgOutputPrepUs);
    TEST_ASSERT_EQUAL_UINT32(340, stats.lastFastLedShowCallUs);
    TEST_ASSERT_EQUAL_UINT32(340, stats.avgFastLedShowCallUs);
    TEST_ASSERT_EQUAL_UINT32(5600, stats.lastRmtFenceUs);
    TEST_ASSERT_EQUAL_UINT32(5600, stats.avgRmtFenceUs);
    TEST_ASSERT_EQUAL_UINT32(40, stats.lastLatchWaitUs);
    TEST_ASSERT_EQUAL_UINT32(40, stats.avgLatchWaitUs);
}

void test_transport_boundaries_use_existing_eighth_weighted_average() {
    LedDriverStats stats{};

    LedTransportTimingSample first{};
    first.totalShowUs = 8000;
    first.outputPrepUs = 800;
    first.fastLedShowCallUs = 400;
    first.rmtFenceUs = 5600;
    first.latchWaitUs = 200;
    recordLedTransportSample(stats, first);

    LedTransportTimingSample second{};
    second.totalShowUs = 4000;
    second.outputPrepUs = 400;
    second.fastLedShowCallUs = 200;
    second.rmtFenceUs = 2800;
    second.latchWaitUs = 100;
    recordLedTransportSample(stats, second);

    TEST_ASSERT_EQUAL_UINT32(2, stats.frameCount);
    TEST_ASSERT_EQUAL_UINT32(4000, stats.lastShowUs);
    TEST_ASSERT_EQUAL_UINT32(7500, stats.avgShowUs);
    TEST_ASSERT_EQUAL_UINT32(8000, stats.maxShowUs);
    TEST_ASSERT_EQUAL_UINT32(750, stats.avgOutputPrepUs);
    TEST_ASSERT_EQUAL_UINT32(375, stats.avgFastLedShowCallUs);
    TEST_ASSERT_EQUAL_UINT32(5250, stats.avgRmtFenceUs);
    TEST_ASSERT_EQUAL_UINT32(187, stats.avgLatchWaitUs);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_records_transport_boundaries_without_losing_legacy_show_time);
    RUN_TEST(test_transport_boundaries_use_existing_eighth_weighted_average);
    return UNITY_END();
}
