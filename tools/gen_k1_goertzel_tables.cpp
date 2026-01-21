/**
 * @file gen_k1_goertzel_tables.cpp
 * @brief Generator for K1 Goertzel coefficient tables
 *
 * CRITICAL: Coeff must be computed from k/N, NOT from f directly.
 * Formula: k = round(N * f / FS_HZ), then ω = 2πk/N, coeff = 2*cos(ω)
 *
 * This ensures spectral stability across different window lengths.
 *
 * Usage:
 *   ./gen_k1_goertzel_tables > firmware/v2/src/audio/k1/K1GoertzelTables_16k.h
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <set>

constexpr uint32_t FS_HZ = 16000;
constexpr uint16_t N_MIN = 256;
constexpr uint16_t N_MAX = 1536;

/**
 * @brief Compute window length N for a given frequency
 *
 * Policy: N_raw = round(FS_HZ / (2 * max_neighbor_distance_hz))
 * Then clamp to [N_MIN, N_MAX]
 */
uint16_t computeWindowLength(float freq_hz, float prev_freq_hz, float next_freq_hz) {
    float dist_prev = (prev_freq_hz > 0.0f) ? (freq_hz - prev_freq_hz) : freq_hz;
    float dist_next = (next_freq_hz > 0.0f) ? (next_freq_hz - freq_hz) : freq_hz;
    float max_dist = std::max(dist_prev, dist_next);
    
    uint16_t N_raw = static_cast<uint16_t>(std::round(FS_HZ / (2.0f * max_dist)));
    
    if (N_raw < N_MIN) return N_MIN;
    if (N_raw > N_MAX) return N_MAX;
    return N_raw;
}

/**
 * @brief Generate HarmonyBank bins (64 semitone bins from A2=110Hz)
 */
void generateHarmonyBins() {
    std::cout << "// -----------------------------------------------------------------------------\n";
    std::cout << "// HarmonyBank: 64 semitone bins from A2 (110 Hz)\n";
    std::cout << "// Each line includes note label for sanity checking.\n";
    std::cout << "// -----------------------------------------------------------------------------\n";
    std::cout << "static const GoertzelBinSpec kHarmonyBins_16k_64[64] = {\n";
    
    const char* note_names[] = {
        "A2", "A#2", "B2", "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3",
        "A3", "A#3", "B3", "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4",
        "A4", "A#4", "B4", "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5",
        "A5", "A#5", "B5", "C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6",
        "A6", "A#6", "B6", "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7",
        "A7", "A#7", "B7", "C8"
    };
    
    std::vector<float> freqs;
    for (int n = 0; n < 64; n++) {
        float freq = 110.0f * std::pow(2.0f, n / 12.0f);
        freqs.push_back(freq);
    }
    
    for (size_t n = 0; n < 64; n++) {
        float freq = freqs[n];
        float prev_freq = (n > 0) ? freqs[n-1] : 0.0f;
        float next_freq = (n < 63) ? freqs[n+1] : 0.0f;
        
        uint16_t N = computeWindowLength(freq, prev_freq, next_freq);
        
        // CRITICAL: Compute k from N and f, then ω from k/N
        uint16_t k = static_cast<uint16_t>(std::round(N * freq / FS_HZ));
        float omega = 2.0f * M_PI * k / N;  // NOT 2πf/FS!
        float coeff = 2.0f * std::cos(omega);
        int16_t coeff_q14 = static_cast<int16_t>(std::round(coeff * 16384.0f));
        
        std::cout << "  { " << std::fixed << std::setprecision(6) << freq << "f, "
                  << N << ", " << k << ", " << coeff_q14 << " }, // " << note_names[n]
                  << "  f=" << freq << "Hz\n";
    }
    
    std::cout << "};\n\n";
    std::cout << "static_assert(sizeof(kHarmonyBins_16k_64) / sizeof(kHarmonyBins_16k_64[0]) == 64,\n";
    std::cout << "              \"Harmony bin table must have 64 entries\");\n\n";
}

/**
 * @brief Generate RhythmBank bins (24 evidence bins)
 */
void generateRhythmBins() {
    std::cout << "// -----------------------------------------------------------------------------\n";
    std::cout << "// RhythmBank: 24 evidence bins\n";
    std::cout << "// -----------------------------------------------------------------------------\n";
    std::cout << "static const GoertzelBinSpec kRhythmBins_16k_24[24] = {\n";
    
    // Fixed frequencies and N policy from z3.md
    struct RhythmBin {
        float freq;
        uint16_t N;
    };
    
    RhythmBin bins[] = {
        {35.0f, 1536}, {45.0f, 1536}, {55.0f, 1536}, {70.0f, 1536}, {85.0f, 1536},
        {100.0f, 1536}, {120.0f, 1536},
        {160.0f, 1024}, {200.0f, 1024}, {250.0f, 1024}, {315.0f, 1024}, {400.0f, 1024},
        {500.0f, 512}, {630.0f, 512}, {800.0f, 512}, {1000.0f, 512}, {1250.0f, 512},
        {2000.0f, 256}, {2500.0f, 256}, {3150.0f, 256}, {4000.0f, 256}, {5000.0f, 256},
        {6300.0f, 256}, {7500.0f, 256}
    };
    
    for (const auto& bin : bins) {
        uint16_t N = bin.N;
        float freq = bin.freq;
        
        // CRITICAL: Compute k from N and f, then ω from k/N
        uint16_t k = static_cast<uint16_t>(std::round(N * freq / FS_HZ));
        float omega = 2.0f * M_PI * k / N;  // NOT 2πf/FS!
        float coeff = 2.0f * std::cos(omega);
        int16_t coeff_q14 = static_cast<int16_t>(std::round(coeff * 16384.0f));
        
        std::cout << "  { " << std::fixed << std::setprecision(1) << freq << "f, "
                  << N << ", " << k << ", " << coeff_q14 << " }, // f=" << freq << "Hz\n";
    }
    
    std::cout << "};\n\n";
    std::cout << "static_assert(sizeof(kRhythmBins_16k_24) / sizeof(kRhythmBins_16k_24[0]) == 24,\n";
    std::cout << "              \"Rhythm bin table must have 24 entries\");\n";
}

int main() {
    std::cout << "/**\n";
    std::cout << " * @file K1GoertzelTables_16k.h\n";
    std::cout << " * @brief Auto-generated Goertzel coefficient tables for K1 Dual-Bank Front-End\n";
    std::cout << " *\n";
    std::cout << " * Generated by: tools/gen_k1_goertzel_tables.cpp\n";
    std::cout << " * Fs = 16000 Hz\n";
    std::cout << " *\n";
    std::cout << " * CRITICAL: Coeff computed from k/N (NOT from f directly)\n";
    std::cout << " *   k = round(N * f / FS_HZ)\n";
    std::cout << " *   ω = 2πk/N\n";
    std::cout << " *   coeff = 2*cos(ω)\n";
    std::cout << " *   coeff_q14 = round(coeff * 16384)\n";
    std::cout << " *\n";
    std::cout << " * DO NOT EDIT THIS FILE - Regenerate from source if changes needed.\n";
    std::cout << " */\n\n";
    std::cout << "#pragma once\n\n";
    std::cout << "#include <stdint.h>\n";
    std::cout << "#include \"K1Types.h\"\n\n";
    
    generateHarmonyBins();
    generateRhythmBins();
    
    return 0;
}

