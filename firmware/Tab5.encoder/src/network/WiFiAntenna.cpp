// ============================================================================
// WiFiAntenna - Tab5 runtime antenna selection (MMCX vs internal 3D)
// ============================================================================

#include "network/WiFiAntenna.h"

#if ENABLE_WIFI

#include <M5Unified.h>

static bool s_useExternal = true;  // default matches boot selection

void setWiFiAntenna(bool useExternal) {
    auto& ioe = M5.getIOExpander(0);
    ioe.digitalWrite(0, useExternal ? 1 : 0);
    s_useExternal = useExternal;
}

bool isWiFiAntennaExternal() {
    return s_useExternal;
}

#endif
