#pragma once
// ============================================================================
// WiFiAntenna - Tab5 runtime antenna selection (MMCX vs internal 3D)
// ============================================================================
// PI4IOE5V6408 IO expander 0, pin P0: HIGH = external MMCX, LOW = internal 3D.
// MUST call initWiFiAntennaPin() before WiFi.begin() — the IO expander powers
// up with all pins as INPUT/high-impedance. Without init, the antenna select
// line is floating and WiFi association will fail intermittently.
// ============================================================================

#include "config/Config.h"

#if ENABLE_WIFI

/** Configure IO expander pin P0 as push-pull OUTPUT and set to external MMCX.
 *  MUST be called once during setup(), before WiFi.begin(). */
void initWiFiAntennaPin();

/** Set WiFi antenna: true = external MMCX, false = internal 3D. */
void setWiFiAntenna(bool useExternal);

/** Last-set antenna: true = external MMCX, false = internal 3D. */
bool isWiFiAntennaExternal();

#else

inline void initWiFiAntennaPin() {}
inline void setWiFiAntenna(bool /* useExternal */) {}
inline bool isWiFiAntennaExternal() { return false; }

#endif
