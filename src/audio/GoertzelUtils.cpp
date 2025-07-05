#include "GoertzelUtils.h"
#include <math.h>
#include <string.h>
#include <memory>

namespace GoertzelUtils {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static const float* s_bins96 = nullptr;  // pointer provided by audio pipeline

void setBinsPointer(const float* bins) {
    s_bins96 = bins;
}

const float* getBins96() {
    return s_bins96;
}

float getBinMagnitude(int idx) {
    if (!s_bins96) return 0.0f;
    if (idx < 0 || idx >= BIN_COUNT) return 0.0f;
    return s_bins96[idx];
}

// Helper for log mapping: f(x) = log10(1 + 9x) / log10(10)
static inline float perceptualLog(float norm) {
    return log10f(1.0f + 9.0f * norm) * 0.434294482f;  // 1/log(10)
}

void mapBinsToZones(int zoneCount, float* out, bool logarithmic) {
    if (!out || zoneCount <= 0) return;

    // Clear output
    memset(out, 0, sizeof(float) * zoneCount);
    std::unique_ptr<int[]> counts(new int[zoneCount]());  // value-initialised to 0

    if (!s_bins96) {
        // Nothing to map yet – leave zeros
        return;
    }

    for (int bin = 0; bin < BIN_COUNT; ++bin) {
        float norm = static_cast<float>(bin) / (BIN_COUNT - 1);
        float pos = logarithmic ? perceptualLog(norm) : norm;  // 0–1
        int zone = static_cast<int>(pos * zoneCount);
        if (zone >= zoneCount) zone = zoneCount - 1;
        out[zone] += s_bins96[bin];
        ++counts[zone];
    }

    // Convert sums to averages (avoid div-by-zero)
    for (int z = 0; z < zoneCount; ++z) {
        if (counts[z] > 0) out[z] /= static_cast<float>(counts[z]);
    }
}

}  // namespace GoertzelUtils 