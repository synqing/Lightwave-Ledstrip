/**
 * Debug diagnostic for BeatPulseTransportCore in native mock environment.
 * Traces injection + advection step-by-step to find zero-energy root cause.
 *
 * Build:
 *   g++ -std=c++17 -O0 -g -DNATIVE_BUILD=1 -DLIGHTWAVEOS_V2=1 -DNUM_LEDS=320 \
 *       -DCENTER_POINT=80 -I../../src -I. -Imocks \
 *       debug_transport.cpp mocks/fastled_mock.cpp mocks/arduino_mock.cpp mocks/freertos_mock.cpp \
 *       -o debug_transport -lm
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "mocks/fastled_mock.h"

#define private public
#include "effects/ieffect/BeatPulseTransportCore.h"
#undef private

using namespace lightwaveos::effects::ieffect;

static void printBins(const char* label, const BeatPulseTransportCore::RGB16* hist, int count) {
    printf("  %s: ", label);
    uint64_t total = 0;
    for (int i = 0; i < count; i++) {
        total += hist[i].r + hist[i].g + hist[i].b;
        if (i < 15 || hist[i].r > 0) {
            printf("[%d]=(%u,%u,%u) ", i, hist[i].r, hist[i].g, hist[i].b);
        }
    }
    printf("\n  total_energy=%lu\n", (unsigned long)total);
}

int main() {
    printf("=== BeatPulseTransportCore Debug Diagnostic ===\n\n");

    BeatPulseTransportCore core;

    // Verify allocation
    printf("1. After construction:\n");
    printf("   m_ps = %p\n", (void*)core.m_ps);
    if (!core.m_ps) {
        printf("   FATAL: m_ps is NULL — malloc failed!\n");
        return 1;
    }
    printf("   m_ps->hist size = %lu bytes\n", (unsigned long)sizeof(core.m_ps->hist));
    printf("   m_nowMs = %u\n", core.m_nowMs);

    // Reset
    core.resetAll();
    core.setNowMs(0);
    printf("\n2. After resetAll + setNowMs(0):\n");
    printBins("hist[0]", core.m_ps->hist[0], 10);

    // Inject: white, amount=1.0, spread=0.5
    CRGB white(255, 255, 255);
    printf("\n3. Injecting: white(255,255,255), amount=1.0, spread=0.5, radialLen=80\n");
    core.injectAtCentre(0, 80, white, 1.0f, 0.5f);
    printBins("hist[0] after inject", core.m_ps->hist[0], 10);

    // Single advection frame: offset=0.5, persist=0.95, diff=0.0, dt=0.016
    printf("\n4. Single advect: offset=0.5, persist=0.95, diff=0.0, dt=0.016\n");
    core.advectOutward(0, 80, 0.5f, 0.95f, 0.0f, 0.016f);
    core.setNowMs(16);
    printBins("hist[0] after 1 advect", core.m_ps->hist[0], 15);

    // 9 more advections (total 10)
    for (int f = 1; f < 10; f++) {
        core.advectOutward(0, 80, 0.5f, 0.95f, 0.0f, 0.016f);
        core.setNowMs(core.m_nowMs + 16);
    }
    printf("\n5. After 10 total advections:\n");
    printBins("hist[0]", core.m_ps->hist[0], 20);

    // 90 more advections (total 100)
    for (int f = 10; f < 100; f++) {
        core.advectOutward(0, 80, 0.5f, 0.95f, 0.0f, 0.016f);
        core.setNowMs(core.m_nowMs + 16);
    }
    printf("\n6. After 100 total advections:\n");
    printBins("hist[0]", core.m_ps->hist[0], 80);

    // Now test with the exact params that the generator uses (first combo)
    printf("\n\n=== Generator first-pair test: offset=0.5, persist=0.90, diff=0.0, dt=0.008, amount=0.5, spread=0.2 ===\n");
    core.resetAll();
    core.setNowMs(0);
    core.injectAtCentre(0, 80, white, 0.5f, 0.2f);
    printf("After inject:\n");
    printBins("hist[0]", core.m_ps->hist[0], 10);

    for (int f = 0; f < 100; f++) {
        core.advectOutward(0, 80, 0.5f, 0.90f, 0.0f, 0.008f);
        core.setNowMs(static_cast<uint32_t>(core.m_nowMs + (0.008f * 1000.0f)));
        if (f == 0 || f == 9 || f == 49 || f == 99) {
            printf("After frame %d (m_nowMs=%u):\n", f, core.m_nowMs);
            printBins("hist[0]", core.m_ps->hist[0], 30);
        }
    }

    printf("\n=== Done ===\n");
    return 0;
}
