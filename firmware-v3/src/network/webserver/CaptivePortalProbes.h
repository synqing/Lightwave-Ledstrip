/**
 * @file CaptivePortalProbes.h
 * @brief Captive-portal probe response shapes served while in AP mode.
 *
 * Mobile and desktop OS captive-portal detectors probe well-known URLs after
 * joining a Wi-Fi network. When the response body matches the detector's
 * expected canonical "success" content (Android 204, Apple "Success" literal,
 * Firefox canonical, Microsoft Connect Test, NCSI), the OS classifies the
 * network as the open internet and never auto-opens the captive-portal UI.
 *
 * For the K1 validation-build access point, the desired behaviour is the
 * opposite: every probe URL must return a captive-positive response so the OS
 * detector classifies the response as a portal and pops the provisioning
 * sheet. Detector-specific notes are inlined at each path in
 * responseForProbePath().
 *
 * This header is intentionally header-only and Arduino-independent so the
 * native test harness (`pio test -e native_test_network_provision_contract`)
 * can include and assert response shapes without mocking AsyncWebServer.
 *
 * Caller responsibility: invoke responseForProbePath() only while the device
 * is currently in WIFI_MODE_AP (runtime check). STA-mode operation should
 * fall through to 404.
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace captive_portal {

/**
 * Captive-positive HTML body served when an OS detector probes an HTML
 * captive-portal endpoint. Deliberately not the OS-expected canonical
 * success body, so detectors pop the captive-portal sheet. The meta refresh
 * provides a browser-side fallback if the user dismisses the sheet.
 *
 * ASCII-only, sized to fit in a single TCP packet (well under 1460 bytes).
 */
constexpr const char PORTAL_BOOTSTRAP_HTML[] =
    "<!DOCTYPE html><html lang=\"en\"><head>"
    "<meta charset=\"utf-8\"/>"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
    "<meta http-equiv=\"refresh\" content=\"0; url=/\"/>"
    "<title>K1 Wi-Fi Setup</title>"
    "</head><body>"
    "<p>Redirecting to K1 Wi-Fi setup&hellip;</p>"
    "<p><a href=\"/\">Tap here if your browser does not redirect automatically.</a></p>"
    "</body></html>";

/**
 * The captive-positive action a probe handler should take when the device
 * is in AP mode and the build flag is set. NotApplicable means the path is
 * not a recognised probe; the handler should fall through to 404.
 */
enum class ProbeAction : uint8_t {
    NotApplicable = 0,
    RedirectToRoot = 1,
    BootstrapHtml = 2,
};

struct ProbeResponse {
    ProbeAction action = ProbeAction::NotApplicable;
    const char* contentType = nullptr;  // "text/html" for BootstrapHtml, nullptr otherwise
    const char* body = nullptr;          // PORTAL_BOOTSTRAP_HTML for BootstrapHtml, nullptr otherwise
    const char* location = nullptr;      // "/" for RedirectToRoot, nullptr otherwise
};

/**
 * Pure helper. Returns the captive-positive response shape for a known
 * captive-portal probe path. Returns NotApplicable for any other path
 * (or for nullptr) so the handler should fall through to 404.
 *
 * Path map:
 * - /generate_204         -> RedirectToRoot (Android NetworkMonitor reads 204 as success;
 *                                            302 keeps the response in the portal class.)
 * - /connecttest.txt      -> RedirectToRoot (Windows 10+ NCSI expects literal
 *                                            "Microsoft Connect Test" body.)
 * - /ncsi.txt             -> RedirectToRoot (legacy Windows NCSI expects literal
 *                                            "Microsoft NCSI" body.)
 * - /redirect             -> RedirectToRoot (Windows NCSI follow-up redirect path.)
 * - /hotspot-detect.html  -> BootstrapHtml  (Apple iOS / iPadOS / macOS detector expects
 *                                            exact "<HTML>...Success...</HTML>" body.)
 * - /library/test/success.html -> BootstrapHtml (macOS alternate Apple-detector path.)
 * - /connectivity-check.html   -> BootstrapHtml (generic detector path.)
 * - /canonical.html       -> BootstrapHtml (Firefox detectportal.firefox.com canonical;
 *                                            expects literal "success" plain-text body.)
 *
 * This function does not consult runtime WiFi state. Callers must apply the
 * WiFi.getMode() == WIFI_MODE_AP runtime guard before invoking.
 */
inline ProbeResponse responseForProbePath(const char* path) {
    if (path == nullptr) {
        return {};
    }

    // 302 redirect probes
    if (std::strcmp(path, "/generate_204") == 0 ||
        std::strcmp(path, "/connecttest.txt") == 0 ||
        std::strcmp(path, "/ncsi.txt") == 0 ||
        std::strcmp(path, "/redirect") == 0) {
        ProbeResponse resp;
        resp.action = ProbeAction::RedirectToRoot;
        resp.location = "/";
        return resp;
    }

    // 200 text/html non-OS-canonical body probes
    if (std::strcmp(path, "/hotspot-detect.html") == 0 ||
        std::strcmp(path, "/library/test/success.html") == 0 ||
        std::strcmp(path, "/connectivity-check.html") == 0 ||
        std::strcmp(path, "/canonical.html") == 0) {
        ProbeResponse resp;
        resp.action = ProbeAction::BootstrapHtml;
        resp.contentType = "text/html";
        resp.body = PORTAL_BOOTSTRAP_HTML;
        return resp;
    }

    return {};
}

}  // namespace captive_portal
}  // namespace webserver
}  // namespace network
}  // namespace lightwaveos
