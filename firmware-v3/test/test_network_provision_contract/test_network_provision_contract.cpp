#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>
#include "network/NetworkProvisionGate.h"
#include "network/RequestValidator.h"
#include "network/webserver/CaptivePortalProbes.h"

using lightwaveos::network::NetworkProvisionGate;
using lightwaveos::network::RequestSchemas::NetworkProvision;
using lightwaveos::network::RequestSchemas::NetworkProvisionSize;
using lightwaveos::network::RequestValidator;
using lightwaveos::network::evaluateNetworkProvisionGate;
using lightwaveos::network::isNetworkProvisionPasswordLengthAllowed;
using lightwaveos::network::networkProvisionGateMessage;
using lightwaveos::network::webserver::captive_portal::PORTAL_BOOTSTRAP_HTML;
using lightwaveos::network::webserver::captive_portal::ProbeAction;
using lightwaveos::network::webserver::captive_portal::responseForProbePath;

static lightwaveos::network::ValidationResult validateProvisionBody(const char* body,
                                                                    JsonDocument& doc) {
    return RequestValidator::parseAndValidate(
        reinterpret_cast<const uint8_t*>(body),
        std::strlen(body),
        doc,
        NetworkProvision,
        NetworkProvisionSize);
}

void test_network_provision_gate_refuses_outside_ap_mode() {
    auto gate = evaluateNetworkProvisionGate(false);

    TEST_ASSERT_EQUAL(static_cast<int>(NetworkProvisionGate::NotApMode),
                      static_cast<int>(gate));
    TEST_ASSERT_EQUAL_STRING("Provisioning is only available from AP mode",
                             networkProvisionGateMessage(gate));
}

void test_network_provision_gate_allows_request_from_ap_mode() {
    auto gate = evaluateNetworkProvisionGate(true);

    TEST_ASSERT_EQUAL(static_cast<int>(NetworkProvisionGate::Allowed),
                      static_cast<int>(gate));
    TEST_ASSERT_EQUAL_STRING("", networkProvisionGateMessage(gate));
}

void test_network_provision_schema_accepts_ssid_and_empty_password() {
    const char* body = "{\"ssid\":\"StudioRouter\",\"password\":\"\"}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("StudioRouter", doc["ssid"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("", doc["password"].as<const char*>());
}

void test_network_provision_schema_accepts_max_length_ssid_and_password() {
    const char* body = "{\"ssid\":\"12345678901234567890123456789012\",\"password\":\"1234567890123456789012345678901234567890123456789012345678901234\"}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("12345678901234567890123456789012", doc["ssid"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("1234567890123456789012345678901234567890123456789012345678901234",
                             doc["password"].as<const char*>());
}

void test_network_provision_schema_rejects_empty_ssid() {
    const char* body = "{\"ssid\":\"\",\"password\":\"valid-password\"}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("ssid", result.fieldName);
}

void test_network_provision_schema_rejects_missing_ssid() {
    const char* body = "{\"password\":\"valid-password\"}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("ssid", result.fieldName);
}

void test_network_provision_schema_rejects_wrong_ssid_type() {
    const char* body = "{\"ssid\":1234,\"password\":\"valid-password\"}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("ssid", result.fieldName);
}

void test_network_provision_schema_rejects_overlong_ssid() {
    const char* body = "{\"ssid\":\"123456789012345678901234567890123\",\"password\":\"valid-password\"}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("ssid", result.fieldName);
}

void test_network_provision_schema_accepts_omitted_password() {
    const char* body = "{\"ssid\":\"StudioRouter\"}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("StudioRouter", doc["ssid"].as<const char*>());
    TEST_ASSERT_TRUE(doc["password"].isNull());
}

void test_network_provision_schema_rejects_wrong_password_type() {
    const char* body = "{\"ssid\":\"StudioRouter\",\"password\":12345678}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("password", result.fieldName);
}

void test_network_provision_schema_rejects_overlong_password() {
    const char* body = "{\"ssid\":\"StudioRouter\",\"password\":\"12345678901234567890123456789012345678901234567890123456789012345\"}";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("password", result.fieldName);
}

void test_network_provision_schema_rejects_malformed_json() {
    const char* body = "{\"ssid\":\"StudioRouter\",\"password\":\"valid-password\"";
    JsonDocument doc;

    auto result = validateProvisionBody(body, doc);

    TEST_ASSERT_FALSE(result.valid);
}

void test_network_provision_password_rule_allows_open_or_wpa_length() {
    TEST_ASSERT_TRUE(isNetworkProvisionPasswordLengthAllowed(0));
    TEST_ASSERT_FALSE(isNetworkProvisionPasswordLengthAllowed(1));
    TEST_ASSERT_FALSE(isNetworkProvisionPasswordLengthAllowed(7));
    TEST_ASSERT_TRUE(isNetworkProvisionPasswordLengthAllowed(8));
    TEST_ASSERT_TRUE(isNetworkProvisionPasswordLengthAllowed(64));
    TEST_ASSERT_FALSE(isNetworkProvisionPasswordLengthAllowed(65));
}

// ---------------------------------------------------------------------------
// Captive-portal probe response shape (response semantics, not route wiring)
// ---------------------------------------------------------------------------

void test_captive_portal_generate_204_redirects_not_204() {
    auto resp = responseForProbePath("/generate_204");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::RedirectToRoot),
                      static_cast<int>(resp.action));
    TEST_ASSERT_NOT_NULL(resp.location);
    TEST_ASSERT_EQUAL_STRING("/", resp.location);
}

void test_captive_portal_apple_hotspot_returns_non_success_html() {
    auto resp = responseForProbePath("/hotspot-detect.html");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::BootstrapHtml),
                      static_cast<int>(resp.action));
    TEST_ASSERT_EQUAL_STRING("text/html", resp.contentType);
    TEST_ASSERT_NOT_NULL(resp.body);
    // Body MUST NOT match Apple's expected literal success body or the
    // captive-portal sheet will not pop.
    TEST_ASSERT_NULL(std::strstr(resp.body, "<TITLE>Success</TITLE>"));
    TEST_ASSERT_NULL(std::strstr(resp.body, "<BODY>Success</BODY>"));
}

void test_captive_portal_apple_library_test_returns_non_success_html() {
    auto resp = responseForProbePath("/library/test/success.html");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::BootstrapHtml),
                      static_cast<int>(resp.action));
    TEST_ASSERT_EQUAL_STRING("text/html", resp.contentType);
    TEST_ASSERT_NULL(std::strstr(resp.body, "<TITLE>Success</TITLE>"));
}

void test_captive_portal_firefox_canonical_returns_bootstrap_html() {
    auto resp = responseForProbePath("/canonical.html");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::BootstrapHtml),
                      static_cast<int>(resp.action));
    TEST_ASSERT_EQUAL_STRING("text/html", resp.contentType);
    // Firefox canonical is short plain-text "success"; our body is much larger
    // HTML, so detector will classify as portal.
    TEST_ASSERT_TRUE(std::strlen(resp.body) > 64);
}

void test_captive_portal_connectivity_check_returns_bootstrap_html() {
    auto resp = responseForProbePath("/connectivity-check.html");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::BootstrapHtml),
                      static_cast<int>(resp.action));
    TEST_ASSERT_EQUAL_STRING("text/html", resp.contentType);
}

void test_captive_portal_windows_connecttest_redirects_not_microsoft() {
    auto resp = responseForProbePath("/connecttest.txt");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::RedirectToRoot),
                      static_cast<int>(resp.action));
    TEST_ASSERT_EQUAL_STRING("/", resp.location);
}

void test_captive_portal_windows_ncsi_redirects_not_ncsi() {
    auto resp = responseForProbePath("/ncsi.txt");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::RedirectToRoot),
                      static_cast<int>(resp.action));
    TEST_ASSERT_EQUAL_STRING("/", resp.location);
}

void test_captive_portal_windows_redirect_redirects_to_root() {
    auto resp = responseForProbePath("/redirect");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::RedirectToRoot),
                      static_cast<int>(resp.action));
    TEST_ASSERT_EQUAL_STRING("/", resp.location);
}

void test_captive_portal_unknown_path_returns_not_applicable() {
    auto resp = responseForProbePath("/foo");
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::NotApplicable),
                      static_cast<int>(resp.action));
}

void test_captive_portal_null_path_returns_not_applicable() {
    auto resp = responseForProbePath(nullptr);
    TEST_ASSERT_EQUAL(static_cast<int>(ProbeAction::NotApplicable),
                      static_cast<int>(resp.action));
}

void test_captive_portal_bootstrap_html_contains_meta_refresh_to_root() {
    TEST_ASSERT_NOT_NULL(std::strstr(PORTAL_BOOTSTRAP_HTML,
                                      "meta http-equiv=\"refresh\""));
    TEST_ASSERT_NOT_NULL(std::strstr(PORTAL_BOOTSTRAP_HTML, "url=/"));
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_network_provision_gate_refuses_outside_ap_mode);
    RUN_TEST(test_network_provision_gate_allows_request_from_ap_mode);
    RUN_TEST(test_network_provision_schema_accepts_ssid_and_empty_password);
    RUN_TEST(test_network_provision_schema_accepts_max_length_ssid_and_password);
    RUN_TEST(test_network_provision_schema_rejects_empty_ssid);
    RUN_TEST(test_network_provision_schema_rejects_missing_ssid);
    RUN_TEST(test_network_provision_schema_rejects_wrong_ssid_type);
    RUN_TEST(test_network_provision_schema_rejects_overlong_ssid);
    RUN_TEST(test_network_provision_schema_accepts_omitted_password);
    RUN_TEST(test_network_provision_schema_rejects_wrong_password_type);
    RUN_TEST(test_network_provision_schema_rejects_overlong_password);
    RUN_TEST(test_network_provision_schema_rejects_malformed_json);
    RUN_TEST(test_network_provision_password_rule_allows_open_or_wpa_length);
    RUN_TEST(test_captive_portal_generate_204_redirects_not_204);
    RUN_TEST(test_captive_portal_apple_hotspot_returns_non_success_html);
    RUN_TEST(test_captive_portal_apple_library_test_returns_non_success_html);
    RUN_TEST(test_captive_portal_firefox_canonical_returns_bootstrap_html);
    RUN_TEST(test_captive_portal_connectivity_check_returns_bootstrap_html);
    RUN_TEST(test_captive_portal_windows_connecttest_redirects_not_microsoft);
    RUN_TEST(test_captive_portal_windows_ncsi_redirects_not_ncsi);
    RUN_TEST(test_captive_portal_windows_redirect_redirects_to_root);
    RUN_TEST(test_captive_portal_unknown_path_returns_not_applicable);
    RUN_TEST(test_captive_portal_null_path_returns_not_applicable);
    RUN_TEST(test_captive_portal_bootstrap_html_contains_meta_refresh_to_root);
    return UNITY_END();
}

#endif // NATIVE_BUILD
