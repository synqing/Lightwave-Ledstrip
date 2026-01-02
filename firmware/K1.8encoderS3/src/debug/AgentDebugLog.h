#pragma once

#include <Arduino.h>

#ifndef AGENT_DEBUG_SESSION_ID
#define AGENT_DEBUG_SESSION_ID "debug-session"
#endif

#ifndef AGENT_DEBUG_RUN_ID
#define AGENT_DEBUG_RUN_ID "pre-fix"
#endif

// Minimal structured logging over Serial, meant to be captured host-side into NDJSON.
// Prefix: "DBGNDJSON:" so a host script can filter reliably.
static inline void agent_dbg_log(const char* hypothesisId,
                                 const char* location,
                                 const char* message,
                                 const char* dataJson) {
    // #region agent log
    Serial.print("DBGNDJSON:{\"sessionId\":\"" AGENT_DEBUG_SESSION_ID "\",\"runId\":\"" AGENT_DEBUG_RUN_ID "\",\"hypothesisId\":\"");
    Serial.print(hypothesisId ? hypothesisId : "");
    Serial.print("\",\"location\":\"");
    Serial.print(location ? location : "");
    Serial.print("\",\"message\":\"");
    Serial.print(message ? message : "");
    Serial.print("\",\"data\":");
    Serial.print(dataJson ? dataJson : "{}");
    Serial.print(",\"timestamp\":");
    Serial.print((uint64_t)millis());
    Serial.println("}");
    // #endregion
}


