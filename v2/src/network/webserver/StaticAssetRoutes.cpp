/**
 * @file StaticAssetRoutes.cpp
 * @brief Static asset route registration implementation
 */

#include "StaticAssetRoutes.h"
#include "../ApiResponse.h"
#include <LittleFS.h>

namespace lightwaveos {
namespace network {
namespace webserver {

void StaticAssetRoutes::registerRoutes(HttpRouteRegistry& registry) {
    // Root - serve index.html or fallback
    registry.onGet("/", [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
            return;
        }

        const char* html =
            "<!doctype html><html><head><meta charset=\"utf-8\"/>"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
            "<title>LightwaveOS</title></head><body>"
            "<h1>LightwaveOS Web UI not installed</h1>"
            "<p>The device web server is running, but no UI files were found on LittleFS.</p>"
            "<p>API is available at <a href=\"/api/v1/\">/api/v1/</a>.</p>"
            "</body></html>";
        request->send(200, "text/html", html);
    });

    // Favicon
    registry.onGet("/favicon.ico", [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/favicon.ico")) {
            request->send(LittleFS, "/favicon.ico", "image/x-icon");
            return;
        }
        request->send(HttpStatus::NO_CONTENT);
    });

    // Simple webapp files (root level)
    registry.onGet("/app.js", [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/app.js")) {
            request->send(LittleFS, "/app.js", "application/javascript");
            return;
        }
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /app.js");
    });

    registry.onGet("/styles.css", [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/styles.css")) {
            request->send(LittleFS, "/styles.css", "text/css");
            return;
        }
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /styles.css");
    });

    // Dashboard assets
    registry.onGet("/assets/app.js", [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/assets/app.js")) {
            request->send(LittleFS, "/assets/app.js", "application/javascript");
            return;
        }
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /assets/app.js");
    });

    registry.onGet("/assets/styles.css", [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/assets/styles.css")) {
            request->send(LittleFS, "/assets/styles.css", "text/css");
            return;
        }
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /assets/styles.css");
    });

    registry.onGet("/vite.svg", [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/vite.svg")) {
            request->send(LittleFS, "/vite.svg", "image/svg+xml");
            return;
        }
        request->send(HttpStatus::NOT_FOUND, "text/plain", "Missing /vite.svg");
    });

    // 404 handler with SPA fallback
    registry.onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(HttpStatus::NO_CONTENT);
            return;
        }

        String url = request->url();
        if (!url.startsWith("/api") && !url.startsWith("/ws")) {
            if (LittleFS.exists("/index.html")) {
                int slash = url.lastIndexOf('/');
                int dot = url.lastIndexOf('.');
                bool hasExtension = (dot > slash);
                if (!hasExtension) {
                    request->send(LittleFS, "/index.html", "text/html");
                    return;
                }
            }
            request->send(HttpStatus::NOT_FOUND, "text/plain", "Not found");
            return;
        }

        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Endpoint not found");
    });
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

