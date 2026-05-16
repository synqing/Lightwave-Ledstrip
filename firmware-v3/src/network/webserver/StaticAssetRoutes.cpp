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
#include <cstdlib>
#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif
#include "../../config/version.h"
#include "../../config/network_config.h"
#define LW_LOG_TAG "StaticRoutes"
#include "../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {

namespace {

constexpr size_t kLauncherBufferSize = 3072;
constexpr size_t kProvisioningBufferSize = 6144;

char* getLauncherBuffer() {
    static char* buffer = nullptr;
    if (buffer) {
        return buffer;
    }

#if !defined(NATIVE_BUILD) && defined(BOARD_HAS_PSRAM)
    buffer = static_cast<char*>(
        heap_caps_malloc(kLauncherBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#endif
#if !defined(NATIVE_BUILD)
    if (!buffer) {
        buffer = static_cast<char*>(
            heap_caps_malloc(kLauncherBufferSize, MALLOC_CAP_8BIT));
    }
#else
    if (!buffer) {
        buffer = static_cast<char*>(std::malloc(kLauncherBufferSize));
    }
#endif
    return buffer;
}

char* getProvisioningBuffer() {
    static char* buffer = nullptr;
    if (buffer) {
        return buffer;
    }

#if !defined(NATIVE_BUILD) && defined(BOARD_HAS_PSRAM)
    buffer = static_cast<char*>(
        heap_caps_malloc(kProvisioningBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#endif
#if !defined(NATIVE_BUILD)
    if (!buffer) {
        buffer = static_cast<char*>(
            heap_caps_malloc(kProvisioningBufferSize, MALLOC_CAP_8BIT));
    }
#else
    if (!buffer) {
        buffer = static_cast<char*>(std::malloc(kProvisioningBufferSize));
    }
#endif
    return buffer;
}

} // namespace

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

static const char PROVISIONING_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>LightwaveOS Setup</title>
<style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;background:#10141d;color:#edf1f7;
font-family:system-ui,-apple-system,sans-serif;display:flex;align-items:center;justify-content:center;padding:24px}
.wrap{width:100%%;max-width:420px}.brand{font-size:1.8rem;font-weight:700;color:#ffb84d;margin-bottom:8px}
p{color:#aab3c2;line-height:1.45;margin:0 0 22px}label{display:block;font-size:.86rem;color:#c8d0dc;margin:14px 0 6px}
input{width:100%%;height:46px;border:1px solid #30384a;background:#171d29;color:#fff;border-radius:8px;
padding:0 12px;font-size:1rem}button{width:100%%;height:48px;border:0;border-radius:8px;background:#ffb84d;
color:#10141d;font-size:1rem;font-weight:700;margin-top:18px}button:disabled{opacity:.6}
.msg{min-height:24px;margin-top:14px;color:#aab3c2}.ok{color:#6ee7a8}.err{color:#ff8f8f}
.foot{margin-top:28px;font-size:.76rem;color:#667085}
</style>
</head>
<body>
<main class="wrap">
<div class="brand">LightwaveOS</div>
<p>Connect this K1 to a WiFi network. The device will restart and join that network.</p>
<form id="form">
<label for="ssid">Network name</label>
<input id="ssid" name="ssid" autocomplete="off" maxlength="32" required/>
<label for="password">Password</label>
<input id="password" name="password" type="password" maxlength="64"/>
<button id="submit" type="submit">Save and restart</button>
</form>
<div id="msg" class="msg"></div>
<div class="foot">v%s &middot; %s.local</div>
</main>
<script>
const form=document.getElementById('form'),msg=document.getElementById('msg'),btn=document.getElementById('submit');
form.addEventListener('submit',async e=>{
 e.preventDefault();btn.disabled=true;msg.className='msg';msg.textContent='Saving...';
 const ssid=form.ssid.value.trim(),password=form.password.value;
 try{
  const r=await fetch('/api/v1/network/provision',{method:'POST',headers:{'Content-Type':'application/json'},
   body:JSON.stringify({ssid,password})});
  const j=await r.json();
  if(!r.ok||!j.success)throw new Error(j.error||'Provisioning failed');
  msg.className='msg ok';msg.textContent='Saved. Restarting now.';
 }catch(err){msg.className='msg err';msg.textContent=err.message;btn.disabled=false;}
});
</script>
</body>
</html>)rawhtml";

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------

void StaticAssetRoutes::registerRoutes(HttpRouteRegistry& registry) {

    // Root -- user-facing launcher page
    registry.onGet("/", [](AsyncWebServerRequest* request) {
#if defined(LW_STA_VALIDATION_BUILD) && !defined(WIFI_AP_ONLY)
        if (WiFi.getMode() == WIFI_MODE_AP) {
            char* buf = getProvisioningBuffer();
            if (!buf) {
                request->send(HttpStatus::SERVICE_UNAVAILABLE, "text/plain", "Provisioning unavailable");
                return;
            }
            snprintf(buf, kProvisioningBufferSize, PROVISIONING_HTML,
                     FIRMWARE_VERSION_STRING,
                     config::NetworkConfig::MDNS_HOSTNAME);
            request->send(200, "text/html", buf);
            return;
        }
#endif
        String ssid = WiFi.SSID();
        if (ssid.isEmpty()) {
            ssid = "WiFi";
        }

        // Persistent cold-path buffer; prefer PSRAM so the launcher page does
        // not reserve internal DRAM while idle.
        char* buf = getLauncherBuffer();
        if (!buf) {
            request->send(HttpStatus::SERVICE_UNAVAILABLE, "text/plain", "Launcher unavailable");
            return;
        }

        snprintf(buf, kLauncherBufferSize, LAUNCHER_HTML,
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

        // Captive portal / browser probe fallback for validation AP mode.
#if defined(LW_STA_VALIDATION_BUILD) && !defined(WIFI_AP_ONLY)
        if (WiFi.getMode() == WIFI_MODE_AP) {
            request->redirect("/");
            return;
        }
#endif
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Not found");
    });
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos
