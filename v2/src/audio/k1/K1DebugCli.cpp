/**
 * @file K1DebugCli.cpp
 * @brief K1 CLI command implementations
 *
 * Provides Serial CLI commands for debugging the K1-Lightwave beat tracker.
 * Output formats match Tab5.DSP for consistency.
 */

#include "K1DebugCli.h"

#if FEATURE_K1_DEBUG

namespace lightwaveos {
namespace audio {
namespace k1 {

// ============================================================================
// Compact Print (matches Tab5 beat_print_compact)
// ============================================================================

void k1_print_compact(Print& out, const K1Pipeline& pipeline) {
    out.print("BPM: ");
    out.print(pipeline.bpm(), 1);
    out.print(" | Conf: ");
    out.print(pipeline.confidence(), 2);
    out.print(" | Phase: ");
    out.print(pipeline.phase01(), 2);
    out.print(" | ");
    out.println(pipeline.locked() ? "LOCKED" : "UNLOCKED");
}

// ============================================================================
// Full Print with Top-3 Candidates
// ============================================================================

void k1_print_full(Print& out, const K1Pipeline& pipeline) {
    const K1ResonatorFrame& rf = pipeline.lastResonatorFrame();

    out.print("BPM: ");
    out.print(pipeline.bpm(), 1);
    out.print(" | Conf: ");
    out.print(pipeline.confidence(), 2);
    out.print(" | Phase: ");
    out.print(pipeline.phase01(), 2);
    out.print(" | ");
    out.print(pipeline.locked() ? "LOCKED" : "UNLOCKED");

    // Top-3 candidates
    out.print(" | Top3: ");
    int k = (rf.k > 3) ? 3 : rf.k;
    for (int i = 0; i < k; i++) {
        out.print(rf.candidates[i].bpm, 0);
        out.print("(");
        out.print(rf.candidates[i].magnitude, 2);
        out.print(") ");
    }
    out.println();
}

// ============================================================================
// Beat Tick Print (matches Tab5)
// ============================================================================

void k1_print_beat_tick(Print& out, const K1Pipeline& pipeline) {
    out.print(">>> BEAT <<< BPM: ");
    out.print(pipeline.bpm(), 1);
    out.print(" Conf: ");
    out.print(pipeline.confidence(), 2);
    out.print(" Phase: ");
    out.println(pipeline.phase01(), 2);
}

// ============================================================================
// ASCII Spectrum (matches Tab5 spectrum command)
// ============================================================================

void k1_print_spectrum(Print& out, const K1ResonatorFrame& rf) {
    out.println("\n=== K1 Resonator Spectrum ===");

    // Find max for normalization
    float max_val = 0.0001f;
    for (int i = 0; i < ST2_BPM_BINS; ++i) {
        if (rf.spectrum[i] > max_val) max_val = rf.spectrum[i];
    }

    // Print every 5 BPM for readability
    for (int i = 0; i < ST2_BPM_BINS; i += 5) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%3d BPM: ", ST2_BPM_MIN + i);
        out.print(buf);

        int bar = (int)(rf.spectrum[i] / max_val * 40);
        for (int j = 0; j < bar; ++j) out.print('#');
        out.println();
    }
    out.println();
}

// ============================================================================
// Novelty Z-Scores (matches Tab5 novelty command)
// ============================================================================

void k1_print_novelty(Print& out, const K1ResonatorBank& resonators, int count) {
    out.println("\n=== K1 Novelty Z-Scores ===");

    const auto& hist = resonators.getNoveltyHistory();
    int n = hist.size();
    if (n > count) n = count;
    if (n == 0) {
        out.println("(no novelty data yet)");
        return;
    }

    for (int i = 0; i < n; ++i) {
        float z = hist.get(i);
        char buf[16];
        snprintf(buf, sizeof(buf), "[%2d] z=%+5.2f ", i, z);
        out.print(buf);

        // Map [-6,+6] to [0,20] bars
        int bar = (int)((z + 6.0f) / 12.0f * 20);
        if (bar < 0) bar = 0;
        if (bar > 20) bar = 20;
        for (int j = 0; j < bar; ++j) out.print('|');
        out.println();
    }
    out.println();
}

// ============================================================================
// Stats Summary
// ============================================================================

void k1_print_stats(Print& out, const K1Pipeline& pipeline) {
    const K1ResonatorBank& resonators = pipeline.resonators();
    const K1TactusResolver& tactus = pipeline.tactus();
    const K1BeatClock& beatClock = pipeline.beatClock();
    const K1TactusFrame& tf = pipeline.lastTactusFrame();

    out.println("\n=== K1 Beat Tracker Stats ===");
    out.print("  BPM: ");
    out.println(pipeline.bpm(), 1);
    out.print("  Confidence: ");
    out.println(pipeline.confidence(), 3);
    out.print("  Phase: ");
    out.println(pipeline.phase01(), 3);
    out.print("  Lock state: ");
    out.println(pipeline.locked() ? "LOCKED" : "UNLOCKED");
    out.println();

    out.println("Stage 2 (Resonators):");
    out.print("  Updates: ");
    out.println(resonators.updates());
    out.print("  Novelty buffer: ");
    out.print(resonators.getNoveltyHistory().size());
    out.print(" / ");
    out.println(ST2_HISTORY_FRAMES);
    out.println();

    out.println("Stage 3 (Tactus):");
    out.print("  Locked BPM: ");
    out.println(tactus.lockedBpm(), 1);
    out.print("  Density conf: ");
    out.println(tf.density_conf, 3);
    out.print("  Family score: ");
    out.println(tf.family_score, 3);
    out.print("  Winning bin: ");
    out.println(tf.winning_bin);
    out.print("  Challenger frames: ");
    out.print(tf.challenger_frames);
    out.print(" / ");
    out.println(ST3_SWITCH_FRAMES);
    out.println();

    out.println("Stage 4 (Beat Clock):");
    out.print("  Phase error: ");
    out.print(beatClock.phaseError(), 3);
    out.println(" rad");
    out.print("  Freq error: ");
    out.print(beatClock.freqError(), 4);
    out.println(" rad/s");
    out.println();
}

// ============================================================================
// Command Handler
// ============================================================================

bool k1_handle_command(const String& cmd, K1Pipeline& pipeline) {
    if (cmd == "k1") {
        k1_print_full(Serial, pipeline);
        return true;
    }
    else if (cmd == "k1s") {
        k1_print_stats(Serial, pipeline);
        return true;
    }
    else if (cmd == "k1spec") {
        k1_print_spectrum(Serial, pipeline.lastResonatorFrame());
        return true;
    }
    else if (cmd == "k1nov") {
        k1_print_novelty(Serial, pipeline.resonators());
        return true;
    }
    else if (cmd == "k1reset") {
        pipeline.reset();
        Serial.println("K1 pipeline reset");
        return true;
    }
    else if (cmd == "k1c") {
        // Compact output for continuous monitoring
        k1_print_compact(Serial, pipeline);
        return true;
    }

    return false;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_K1_DEBUG
