/**
 * @file test_lattice_origin.cpp
 * @brief Verify the canonical ESV11 Goertzel detector lattice is C-origin.
 *
 * Public contract (ControlBus.h:42, EffectContext.h:248,276):
 *   chroma[0] = C, chroma[1] = C#, ..., chroma[11] = B.
 *
 * For this contract to hold, the detector lattice must place a C at bin 0
 * (and at every (i % 12) == 0 octave-aliasing position). The vendor notes[]
 * table is quarter-tone spaced from A1 = 55 Hz, so:
 *   - notes[6]  ≈ 65.40639 Hz = C2  (correct anchor)
 *   - notes[12] ≈ 77.78175 Hz = D#2 (legacy ES anchor — pre-fix)
 *
 * The fix is BOTTOM_NOTE = 6, NOTE_STEP = 2, NUM_FREQS = 64. This produces
 * 5 complete octaves C2..B6 in bins 0..59, with bins 60..63 covering
 * C7..D#7 as spectrum-only extras (excluded from the i%12 chroma fold).
 *
 * This test asserts the corrected lattice. It FAILS with BOTTOM_NOTE = 12
 * (D#-origin, the pre-fix state) and PASSES with BOTTOM_NOTE = 6.
 */

#include <unity.h>

#include <cmath>
#include <cstdint>

// Vendored ES pipeline (header-only globals live in this TU)
#include "audio/backends/esv11/vendor/EsV11Shim.h"
#include "audio/backends/esv11/vendor/global_defines.h"
#include "audio/backends/esv11/vendor/microphone.h"
#include "audio/backends/esv11/vendor/goertzel.h"
#include "audio/backends/esv11/vendor/utilities_min.h"

static bool g_initialised = false;

static void ensure_lattice_initialised()
{
    if (g_initialised) return;
    esv11_init_buffers();
    init_window_lookup();
    init_goertzel_constants();
    g_initialised = true;
}

// ---------------------------------------------------------------------------
// Frequency-map assertions: bin index → expected musical note (C-origin).
// Tolerances are tight (≤0.5 Hz at the audio-band ceiling) — quarter-tone table
// values are exact 12-TET pitches to 4 decimal places.
// ---------------------------------------------------------------------------

static void test_lattice_bin_0_is_C2()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.40639f, frequencies_musical[0].target_freq);
}

static void test_lattice_bin_12_is_C3()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 130.8128f, frequencies_musical[12].target_freq);
}

static void test_lattice_bin_24_is_C4()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 261.6256f, frequencies_musical[24].target_freq);
}

static void test_lattice_bin_36_is_C5()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 523.2511f, frequencies_musical[36].target_freq);
}

static void test_lattice_bin_48_is_C6()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1046.502f, frequencies_musical[48].target_freq);
}

static void test_lattice_bin_59_is_B6()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 1975.533f, frequencies_musical[59].target_freq);
}

static void test_lattice_bin_60_is_C7()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 2093.005f, frequencies_musical[60].target_freq);
}

static void test_lattice_bin_63_is_Dsharp7()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 2489.016f, frequencies_musical[63].target_freq);
}

// ---------------------------------------------------------------------------
// Chromagram fold contract: with a C-origin lattice and the unchanged
// vendor `i % 12` fold (60 bins / 5 octaves), each bin's pitch class equals
// (bin index % 12). Verify the indexing identity holds for the 5 anchor bins
// that should land in chroma[0] (= C).
// ---------------------------------------------------------------------------

static void test_chroma_zero_aggregates_only_C_bins()
{
    ensure_lattice_initialised();
    // Bins 0, 12, 24, 36, 48 must all be C-pitch-class detectors after the fix.
    // (They are the 5 octaves of C from C2 to C6 that fold into chroma[0].)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.40639f, frequencies_musical[0].target_freq);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 130.8128f, frequencies_musical[12].target_freq);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 261.6256f, frequencies_musical[24].target_freq);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 523.2511f, frequencies_musical[36].target_freq);
    TEST_ASSERT_FLOAT_WITHIN(0.1f,  1046.502f, frequencies_musical[48].target_freq);
}

// chroma[1] should aggregate C# pitch class (bins 1, 13, 25, 37, 49).
static void test_chroma_one_aggregates_only_Csharp_bins()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 69.29566f, frequencies_musical[1].target_freq);   // C#2
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 138.5913f, frequencies_musical[13].target_freq);  // C#3
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 277.1826f, frequencies_musical[25].target_freq);  // C#4
}

// chroma[9] should aggregate A pitch class (bins 9, 21, 33, 45, 57).
// A4 = 440 Hz lands at bin 21 under the corrected lattice.
static void test_chroma_nine_aggregates_only_A_bins()
{
    ensure_lattice_initialised();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 110.0f, frequencies_musical[9].target_freq);   // A2
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 220.0f, frequencies_musical[21].target_freq);  // A3
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 440.0f, frequencies_musical[33].target_freq);  // A4
}

void setUp() {}
void tearDown() {}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_lattice_bin_0_is_C2);
    RUN_TEST(test_lattice_bin_12_is_C3);
    RUN_TEST(test_lattice_bin_24_is_C4);
    RUN_TEST(test_lattice_bin_36_is_C5);
    RUN_TEST(test_lattice_bin_48_is_C6);
    RUN_TEST(test_lattice_bin_59_is_B6);
    RUN_TEST(test_lattice_bin_60_is_C7);
    RUN_TEST(test_lattice_bin_63_is_Dsharp7);

    RUN_TEST(test_chroma_zero_aggregates_only_C_bins);
    RUN_TEST(test_chroma_one_aggregates_only_Csharp_bins);
    RUN_TEST(test_chroma_nine_aggregates_only_A_bins);

    return UNITY_END();
}
