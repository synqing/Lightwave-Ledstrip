/**
 * @file StaticAssetRoutes.cpp
 * @brief User-facing launcher page and fallback route registration
 *
 * Serves a consumer-friendly landing page at the root URL that confirms
 * the device is online and links the user to the desktop controller app.
 * No developer jargon, no API endpoints, no heap stats.
 */

#include "StaticAssetRoutes.h"
#include "../ApiResponse.h"
#include <WiFi.h>
#include <Arduino.h>
#include "../../config/version.h"
#include "../../config/network_config.h"
#define LW_LOG_TAG "StaticRoutes"
#include "../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {

// ---------------------------------------------------------------------------
// HTML template  (raw string literal with snprintf placeholders)
// ---------------------------------------------------------------------------

static const char LAUNCHER_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>LightwaveOS</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0f1219;color:#e0e0e0;font-family:system-ui,-apple-system,sans-serif;
     min-height:100vh;display:flex;align-items:center;justify-content:center;padding:1.5rem}
.wrap{max-width:440px;width:100%%;text-align:center}
h1{font-size:2rem;color:#FFB84D;margin-bottom:.5rem;letter-spacing:-.02em;font-weight:700}
.status{font-size:1.05rem;color:#e0e0e0;margin-bottom:.25rem}
.wifi{display:inline-flex;align-items:center;gap:.45rem;font-size:.9rem;color:#888;margin-bottom:2rem}
.dot{width:8px;height:8px;border-radius:50%%;background:#4ade80;display:inline-block;
     box-shadow:0 0 6px #4ade8088}
.cta{display:block;background:#FFB84D;color:#0f1219;font-size:1.1rem;font-weight:700;
     padding:1rem 2rem;border-radius:12px;text-decoration:none;margin:0 auto 2rem;
     max-width:320px;transition:background .15s,transform .1s;letter-spacing:.01em}
.cta:hover{background:#ffc96b;transform:translateY(-1px)}
.cta:active{transform:translateY(0)}
hr{border:none;border-top:1px solid #2a2f3e;margin:0 0 1.5rem}
.help{color:#888;font-size:.85rem;line-height:1.6}
.help strong{color:#b0b0b0;font-weight:500}
.footer{margin-top:2.5rem;font-size:.72rem;color:#555}
</style>
</head>
<body>
<div class="wrap">
  <h1>LightwaveOS</h1>
  <p class="status">Your device is online and ready.</p>
  <p class="wifi"><span class="dot"></span> Connected to %s</p>

  <a class="cta" href="http://localhost:8888">Open Controller App</a>

  <hr/>
  <p class="help">
    Don't have the app yet?<br/>
    Download and install the <strong>LightwaveOS Controller</strong><br/>
    on your computer first.
  </p>

  <p class="footer">v%s &middot; %s.local</p>
</div>
</body>
</html>)rawhtml";

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------

void StaticAssetRoutes::registerRoutes(HttpRouteRegistry& registry) {

    // Root -- user-facing launcher page
    registry.onGet("/", [](AsyncWebServerRequest* request) {
        String ssid = WiFi.SSID();
        if (ssid.isEmpty()) {
            ssid = "WiFi";
        }

        char buf[3072];
        snprintf(buf, sizeof(buf), LAUNCHER_HTML,
                 ssid.c_str(),                     // WiFi network name (%s)
                 FIRMWARE_VERSION_STRING,           // version in footer (%s)
                 config::NetworkConfig::MDNS_HOSTNAME); // hostname in footer (%s)

        request->send(200, "text/html", buf);
    });

    // Favicon -- no icon file, return 204
    registry.onGet("/favicon.ico", [](AsyncWebServerRequest* request) {
        request->send(HttpStatus::NO_CONTENT);
    });

    // 404 handler -- CORS OPTIONS passthrough + API error response
    registry.onNotFound([](AsyncWebServerRequest* request) {
        // CORS preflight
        if (request->method() == HTTP_OPTIONS) {
            request->send(HttpStatus::NO_CONTENT);
            return;
        }

        // API paths get a structured JSON error
        String url = request->url();
        if (url.startsWith("/api") || url.startsWith("/ws")) {
            sendErrorResponse(request, HttpStatus::NOT_FOUND,
                              ErrorCodes::NOT_FOUND, "Endpoint not found");
            return;
        }

        // Everything else
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Not found");
    });
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos
