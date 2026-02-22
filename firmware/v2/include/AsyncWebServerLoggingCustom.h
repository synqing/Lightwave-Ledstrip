/**
 * @file AsyncWebServerLoggingCustom.h
 * @brief Custom logging overrides for ESPAsyncWebServer library
 *
 * Suppresses ALL ESPAsyncWebServer internal logging.  Our own LW_LOG* macros
 * handle all necessary WebSocket event telemetry (connect, disconnect, route,
 * error) with structured JSON output.
 *
 * This eliminates the "Too many messages queued" serial flood that occurs when
 * slow SoftAP clients cause outbound queue overflow.  The overflow is expected
 * and harmless — setCloseClientOnQueueFull(false) silently drops frames so the
 * client stays connected and receives future updates.
 *
 * Activated by: -D ASYNCWEBSERVER_LOG_CUSTOM  (platformio.ini)
 */

#pragma once

#define async_ws_log_e(format, ...)
#define async_ws_log_w(format, ...)
#define async_ws_log_i(format, ...)
#define async_ws_log_d(format, ...)
#define async_ws_log_v(format, ...)
