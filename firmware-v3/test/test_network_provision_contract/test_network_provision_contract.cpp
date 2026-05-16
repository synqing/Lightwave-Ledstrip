#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>
#include "network/RequestValidator.h"

using lightwaveos::network::RequestSchemas::NetworkProvision;
using lightwaveos::network::RequestSchemas::NetworkProvisionSize;
using lightwaveos::network::RequestValidator;

void test_network_provision_schema_accepts_ssid_and_empty_password() {
    const char* body = "{\"ssid\":\"StudioRouter\",\"password\":\"\"}";
    JsonDocument doc;

    auto result = RequestValidator::parseAndValidate(
        reinterpret_cast<const uint8_t*>(body),
        std::strlen(body),
        doc,
        NetworkProvision,
        NetworkProvisionSize);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("StudioRouter", doc["ssid"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("", doc["password"].as<const char*>());
}

void test_network_provision_schema_rejects_missing_ssid() {
    const char* body = "{\"password\":\"valid-password\"}";
    JsonDocument doc;

    auto result = RequestValidator::parseAndValidate(
        reinterpret_cast<const uint8_t*>(body),
        std::strlen(body),
        doc,
        NetworkProvision,
        NetworkProvisionSize);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("ssid", result.fieldName);
}

void test_network_provision_schema_rejects_overlong_ssid() {
    const char* body = "{\"ssid\":\"123456789012345678901234567890123\",\"password\":\"valid-password\"}";
    JsonDocument doc;

    auto result = RequestValidator::parseAndValidate(
        reinterpret_cast<const uint8_t*>(body),
        std::strlen(body),
        doc,
        NetworkProvision,
        NetworkProvisionSize);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("ssid", result.fieldName);
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_network_provision_schema_accepts_ssid_and_empty_password);
    RUN_TEST(test_network_provision_schema_rejects_missing_ssid);
    RUN_TEST(test_network_provision_schema_rejects_overlong_ssid);
    return UNITY_END();
}

#endif // NATIVE_BUILD
