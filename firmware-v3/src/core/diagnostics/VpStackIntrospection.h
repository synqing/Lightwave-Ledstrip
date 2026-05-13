#pragma once

#include <cstdint>

namespace lightwaveos {
namespace diagnostics {

enum class VpTopology : uint8_t {
    Unified = 0,
    ZoneUnified = 1,
    DirectStrip = 2,
};

enum class VpAuthoredSurface : uint8_t {
    UnifiedLeds = 0,
    PhysicalStrips = 1,
};

enum class VpCorrectionSurface : uint8_t {
    None = 0,
    UnifiedLeds = 1,
};

enum class VpOutputSurface : uint8_t {
    PhysicalStrips = 0,
};

struct VpSurfaceState {
    VpAuthoredSurface authored = VpAuthoredSurface::UnifiedLeds;
    VpCorrectionSurface correction = VpCorrectionSurface::None;
    VpOutputSurface output = VpOutputSurface::PhysicalStrips;
    bool surfaceMismatch = false;
};

inline const char* topologyName(VpTopology topology) {
    switch (topology) {
        case VpTopology::Unified:
            return "unified";
        case VpTopology::ZoneUnified:
            return "zone_unified";
        case VpTopology::DirectStrip:
            return "direct_strip";
    }
    return "unknown";
}

inline const char* authoredSurfaceName(VpAuthoredSurface surface) {
    switch (surface) {
        case VpAuthoredSurface::UnifiedLeds:
            return "m_leds";
        case VpAuthoredSurface::PhysicalStrips:
            return "physical_strips";
    }
    return "unknown";
}

inline const char* correctionSurfaceName(VpCorrectionSurface surface) {
    switch (surface) {
        case VpCorrectionSurface::None:
            return "none";
        case VpCorrectionSurface::UnifiedLeds:
            return "m_leds";
    }
    return "unknown";
}

inline const char* outputSurfaceName(VpOutputSurface surface) {
    switch (surface) {
        case VpOutputSurface::PhysicalStrips:
            return "physical_strips";
    }
    return "unknown";
}

inline VpSurfaceState deriveVpSurfaces(VpTopology topology,
                                       bool dualChannelMode,
                                       bool correctionApplied) {
    const bool stripAuthored = (topology == VpTopology::DirectStrip) || dualChannelMode;

    VpSurfaceState state;
    state.authored = stripAuthored ? VpAuthoredSurface::PhysicalStrips
                                   : VpAuthoredSurface::UnifiedLeds;
    state.correction = correctionApplied ? VpCorrectionSurface::UnifiedLeds
                                         : VpCorrectionSurface::None;
    state.output = VpOutputSurface::PhysicalStrips;
    state.surfaceMismatch =
        correctionApplied &&
        state.authored == VpAuthoredSurface::PhysicalStrips &&
        state.correction == VpCorrectionSurface::UnifiedLeds;
    return state;
}

} // namespace diagnostics
} // namespace lightwaveos
