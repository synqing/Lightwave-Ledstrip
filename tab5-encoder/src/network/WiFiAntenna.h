#pragma once
// ============================================================================
// WiFiAntenna - Tab5 runtime antenna selection (MMCX vs internal 3D)
// ============================================================================
// Tab5 uses PI4IOE5V6408 IO expander (E1) pin P0: HIGH = external MMCX,
// LOW = internal 3D antenna. This module allows switching at runtime.
//
// Caveat: ESP32-C6 may only sample the antenna pin at WiFi init. If runtime
// switch does not change RSSI, reconnect WiFi or reboot to apply.
// ============================================================================

#include "config/Config.h"

#if ENABLE_WIFI

/** Set WiFi antenna: true = external MMCX, false = internal 3D. */
void setWiFiAntenna(bool useExternal);

/** Last-set antenna: true = external MMCX, false = internal 3D. */
bool isWiFiAntennaExternal();

#else

inline void setWiFiAntenna(bool /* useExternal */) {}
inline bool isWiFiAntennaExternal() { return false; }

#endif
