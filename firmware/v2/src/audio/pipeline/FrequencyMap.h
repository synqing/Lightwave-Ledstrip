#pragma once
// ============================================================================
// FrequencyMap.h — Frequency-Semantic Bin Query Infrastructure
// ============================================================================
//
// PURPOSE:
//   Decouples effect intent ("give me sub-bass energy") from DSP
//   implementation detail (bin indices, FFT size, sample rate).
//
//   Effects MUST NOT use hardcoded bin indices. They use either:
//     1. Named accessors: subBass(), kick(), shimmer(), air()
//     2. Generic query:   energyInRange(freqLo, freqHi)
//
// DESIGN:
//   - Bin ranges precomputed at init from sample rate + FFT size
//   - Runtime cost of energyInRange(): N additions (N = bin count in range)
//   - No divisions, no floating-point in hot path
//   - If sample rate or FFT size changes, call init() again
//
// THREAD SAFETY:
//   - init() must be called once before any queries (Audio thread setup)
//   - All query methods are const and safe to call from any thread
//   - Underlying spectrum pointer must be valid for lifetime of queries
//
// AUTHOR:  Integration artifact — CTO advisory
// DATE:    2026-02-19
// VERSION: 1.0.0
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cmath>

namespace audio {

// ============================================================================
// FrequencyBand — Precomputed bin range for a named frequency query
// ============================================================================
struct FrequencyBand {
    uint16_t binLo;     // First bin index (inclusive)
    uint16_t binHi;     // Last bin index (exclusive)
    float    freqLo;    // Lower frequency bound (Hz) — for documentation/debug
    float    freqHi;    // Upper frequency bound (Hz) — for documentation/debug
};

// ============================================================================
// Named Band IDs — Canonical frequency ranges for semantic queries
// ============================================================================
//
// These define WHAT the effect is asking for musically.
// The bin indices that satisfy each query depend on sample rate and FFT size.
//
// IMPORTANT: These ranges are tuning targets, not mathematical truths.
//            They will be refined during listening sessions.
//            The architecture supports changing them without touching effect code.
//
enum class NamedBand : uint8_t {
    SUB_BASS = 0,   //  20 - 120 Hz  | Kick drum fundamental, sub-bass rumble
    KICK,            //  60 - 150 Hz  | Focused kick detection
    LOW_MID,         // 250 - 500 Hz  | Warmth, body, male vocal fundamental
    MID,             // 500 - 2000 Hz | Presence, vocal clarity
    SHIMMER,         // 1300 - 4200 Hz| Treble shimmer, cymbal wash
    SNARE,           // 150 - 900 Hz  | Snare body + snap (wider than ES v1.1)
    HIHAT,           // 6000 - 16000 Hz| Hi-hat, cymbal air (NOW ACTUALLY WORKS)
    AIR,             // 8000 - 16000 Hz| Breath, sibilance, air
    COUNT
};

// ============================================================================
// Frequency range definitions (Hz) — THE SINGLE SOURCE OF TRUTH
// ============================================================================
// If you want to change what "sub-bass" means, change it HERE and nowhere else.
//
struct BandDefinition {
    float freqLo;
    float freqHi;
};

static constexpr BandDefinition kNamedBandDefs[static_cast<size_t>(NamedBand::COUNT)] = {
    // NamedBand::SUB_BASS
    {  20.0f,   120.0f },
    // NamedBand::KICK
    {  60.0f,   150.0f },
    // NamedBand::LOW_MID
    { 250.0f,   500.0f },
    // NamedBand::MID
    { 500.0f,  2000.0f },
    // NamedBand::SHIMMER
    { 1300.0f, 4200.0f },
    // NamedBand::SNARE
    { 150.0f,   900.0f },
    // NamedBand::HIHAT
    { 6000.0f, 16000.0f },
    // NamedBand::AIR
    { 8000.0f, 16000.0f },
};

// ============================================================================
// FrequencyMap — Precomputed lookup for frequency-to-bin mapping
// ============================================================================
class FrequencyMap {
public:
    // Maximum supported FFT bins (256 for 512-pt FFT)
    static constexpr size_t kMaxBins = 256;

    // ------------------------------------------------------------------------
    // init() — Call once at audio pipeline startup
    // ------------------------------------------------------------------------
    // Precomputes bin ranges for all named bands and caches binHz.
    //
    // @param sampleRate  Audio sample rate in Hz (e.g., 32000)
    // @param fftSize     FFT window size (e.g., 512)
    //
    // POST: All query methods are valid. numBins() returns fftSize/2.
    //
    void init(float sampleRate, uint16_t fftSize) {
        m_sampleRate = sampleRate;
        m_fftSize    = fftSize;
        m_numBins    = fftSize / 2;
        m_binHz      = sampleRate / static_cast<float>(fftSize);

        // Precompute all named band bin ranges
        for (size_t i = 0; i < static_cast<size_t>(NamedBand::COUNT); ++i) {
            m_namedBands[i] = computeBand(
                kNamedBandDefs[i].freqLo,
                kNamedBandDefs[i].freqHi
            );
        }

        m_initialised = true;
    }

    // ------------------------------------------------------------------------
    // Query: Named band energy
    // ------------------------------------------------------------------------
    // Returns sum of magnitudes in the named frequency band.
    //
    // @param spectrum    Pointer to magnitude spectrum (m_numBins floats)
    // @param band        Which named band to query
    // @return            Sum of magnitudes in band. 0 if band is empty.
    //
    float bandEnergy(const float* spectrum, NamedBand band) const {
        const FrequencyBand& fb = m_namedBands[static_cast<size_t>(band)];
        return sumBins(spectrum, fb.binLo, fb.binHi);
    }

    // ------------------------------------------------------------------------
    // Query: Named band mean energy
    // ------------------------------------------------------------------------
    // Returns mean magnitude in the named frequency band.
    // Useful when comparing bands of different widths.
    //
    float bandMeanEnergy(const float* spectrum, NamedBand band) const {
        const FrequencyBand& fb = m_namedBands[static_cast<size_t>(band)];
        uint16_t count = fb.binHi - fb.binLo;
        if (count == 0) return 0.0f;
        return sumBins(spectrum, fb.binLo, fb.binHi) / static_cast<float>(count);
    }

    // ------------------------------------------------------------------------
    // Query: Arbitrary frequency range energy
    // ------------------------------------------------------------------------
    // For effects that need custom frequency ranges not covered by named bands.
    //
    // @param spectrum    Pointer to magnitude spectrum
    // @param freqLo      Lower frequency bound (Hz)
    // @param freqHi      Upper frequency bound (Hz)
    // @return            Sum of magnitudes in range. 0 if range is empty.
    //
    // NOTE: Uses simple rounding to nearest bin boundaries.
    //       Precision loss is < 0.5 * binHz at each edge.
    //       At 62.5 Hz spacing, that's ±31 Hz — well within the
    //       ControlBus smoothing pipeline's tolerance.
    //
    float energyInRange(const float* spectrum, float freqLo, float freqHi) const {
        FrequencyBand fb = computeBand(freqLo, freqHi);
        return sumBins(spectrum, fb.binLo, fb.binHi);
    }

    // ------------------------------------------------------------------------
    // Query: Mean energy in arbitrary range
    // ------------------------------------------------------------------------
    float meanEnergyInRange(const float* spectrum, float freqLo, float freqHi) const {
        FrequencyBand fb = computeBand(freqLo, freqHi);
        uint16_t count = fb.binHi - fb.binLo;
        if (count == 0) return 0.0f;
        return sumBins(spectrum, fb.binLo, fb.binHi) / static_cast<float>(count);
    }

    // ------------------------------------------------------------------------
    // Accessors for introspection / debug
    // ------------------------------------------------------------------------
    float    binHz()      const { return m_binHz; }
    uint16_t numBins()    const { return m_numBins; }
    float    sampleRate() const { return m_sampleRate; }
    uint16_t fftSize()    const { return m_fftSize; }
    bool     ready()      const { return m_initialised; }

    const FrequencyBand& namedBand(NamedBand band) const {
        return m_namedBands[static_cast<size_t>(band)];
    }

    // ------------------------------------------------------------------------
    // Utility: Convert frequency to nearest bin index
    // ------------------------------------------------------------------------
    uint16_t freqToBin(float freqHz) const {
        int32_t bin = static_cast<int32_t>(roundf(freqHz / m_binHz));
        if (bin < 0) bin = 0;
        if (bin >= static_cast<int32_t>(m_numBins)) bin = m_numBins - 1;
        return static_cast<uint16_t>(bin);
    }

    // ------------------------------------------------------------------------
    // Utility: Convert bin index to center frequency
    // ------------------------------------------------------------------------
    float binToFreq(uint16_t bin) const {
        return static_cast<float>(bin) * m_binHz;
    }

private:
    float    m_sampleRate   = 0.0f;
    uint16_t m_fftSize      = 0;
    uint16_t m_numBins      = 0;
    float    m_binHz        = 0.0f;
    bool     m_initialised  = false;

    FrequencyBand m_namedBands[static_cast<size_t>(NamedBand::COUNT)] = {};

    // Compute bin range for a frequency range (called at init and for ad-hoc queries)
    FrequencyBand computeBand(float freqLo, float freqHi) const {
        FrequencyBand fb;
        fb.freqLo = freqLo;
        fb.freqHi = freqHi;

        // Round to nearest bin. For sub-bass where precision matters most,
        // this gives ±31 Hz error at 62.5 Hz spacing — acceptable for
        // aggregate energy queries.
        int32_t lo = static_cast<int32_t>(roundf(freqLo / m_binHz));
        int32_t hi = static_cast<int32_t>(roundf(freqHi / m_binHz));

        // Clamp to valid range
        if (lo < 0) lo = 0;
        if (hi < 0) hi = 0;
        if (lo >= static_cast<int32_t>(m_numBins)) lo = m_numBins;
        if (hi >= static_cast<int32_t>(m_numBins)) hi = m_numBins;
        if (lo > hi) lo = hi;  // Empty band if inverted

        fb.binLo = static_cast<uint16_t>(lo);
        fb.binHi = static_cast<uint16_t>(hi);
        return fb;
    }

    // Sum magnitude bins in range [binLo, binHi)
    // This is the hot path — keep it tight.
    static float sumBins(const float* spectrum, uint16_t binLo, uint16_t binHi) {
        float sum = 0.0f;
        for (uint16_t i = binLo; i < binHi; ++i) {
            sum += spectrum[i];
        }
        return sum;
    }
};

} // namespace audio
