#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>
#include "network/NetworkProvisionGate.h"
#include "network/RequestValidator.h"

using lightwaveos::network::NetworkProvisionGate;
using lightwaveos::network::RequestSchemas::NetworkProvision;
using lightwaveos::network::RequestSchemas::NetworkProvisionSize;
using lightwaveos::network::RequestValidator;
using lightwaveos::network::evaluateNetworkProvisionGate;
using lightwaveos::network::isNetworkProvisionPasswordLengthAllowed;
using lightwaveos::network::networkProvisionGateMessage;

static lightwaveos::network::ValidationResult validateProvisionBody(const char* body,
                                                                    JsonDocument& doc) {
    return RequestValidator::parseAndValidate(
        reinterpret_cast<const uint8_t*>(body),
        std::strlen(body),
        doc,
        NetworkProvision,
        NetworkProvisionSize);
}

void test_network_provision_gate_refuses_wifi_ap_only_build_first() {
    auto gate = evaluateNetworkProvisionGate(true, true, true);

    TEST_ASSERT_EQUAL(static_cast<int>(NetworkProvisionGate::WifiApOnlyBuild),
                      static_cast<int>(gate));
    TEST_ASSERT_EQUAL_STRING("Provisioning unavailable in WIFI_AP_ONLY build",
                             networkProvisionGateMessage(gate));
}

void test_network_provision_gate_refuses_non_validation_build() {
    auto gate = evaluateNetworkProvisionGate(false, false, true);

    TEST_ASSERT_EQUAL(static_cast<int>(NetworkProvisionGate::NonValidationBuild),
                      static_cast<int>(gate));
    TEST_ASSERT_EQUAL_STRING("Provisioning unavailable outside LW_STA_VALIDATION_BUILD",
                             networkProvisionGateMessage(gate));
}

void test_network_provision_gate_refuses_validation_request_outside_ap_mode() {
    auto gate = evaluateNetworkProvisionGate(false, true, false);

    TEST_ASSERT_EQUAL(static_cast<int>(NetworkProvisionGate::NotApMode),
                      static_cast<int>(gate));
    TEST_ASSERT_EQUAL_STRING("Provisioning is only available from AP mode",
                             networkProvisionGateMessage(gate));
}

void test_network_provision_gate_allows_validation_request_from_ap_mode() {
    auto gate = evaluateNetworkProvisionGate(false, true, true);

    TEST_ASSERT_EQUAL(static_cast<int>(NetworkProvisionGate::Allowed),
                      static_cast<int>(gate));
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

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_network_provision_gate_refuses_wifi_ap_only_build_first);
    RUN_TEST(test_network_provision_gate_refuses_non_validation_build);
    RUN_TEST(test_network_provision_gate_refuses_validation_request_outside_ap_mode);
    RUN_TEST(test_network_provision_gate_allows_validation_request_from_ap_mode);
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
    return UNITY_END();
}

#endif // NATIVE_BUILD
