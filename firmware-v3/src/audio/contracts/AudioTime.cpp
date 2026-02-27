#include "AudioTime.h"

namespace lightwaveos::audio {

int64_t AudioTime_SamplesBetween(const AudioTime& a, const AudioTime& b) {
    // Cast to signed to allow negative.
    const int64_t ai = (int64_t)a.sample_index;
    const int64_t bi = (int64_t)b.sample_index;
    return (bi - ai);
}

float AudioTime_SecondsBetween(const AudioTime& a, const AudioTime& b) {
    if (a.sample_rate_hz == 0) return 0.0f;

    // We assume both times refer to the same sample_rate_hz domain (your system contract).
    const int64_t ds = AudioTime_SamplesBetween(a, b);
    return (float)ds / (float)a.sample_rate_hz;
}

} // namespace lightwaveos::audio
