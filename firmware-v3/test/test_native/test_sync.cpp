// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * LightwaveOS v2 - Multi-Device Sync Unit Tests
 *
 * Tests for the sync system including:
 * - DeviceUUID parsing and comparison
 * - LeaderElection (Bully algorithm)
 * - CommandSerializer (serialize/parse round-trip)
 * - StateSerializer (serialize/parse round-trip)
 * - ConflictResolver (version comparison)
 */

#include <unity.h>
#include <cstring>
#include <cstdlib>

// Define NATIVE_BUILD before including sync headers
#ifndef NATIVE_BUILD
#define NATIVE_BUILD
#endif

#include "../../src/sync/DeviceUUID.h"
#include "../../src/sync/SyncProtocol.h"
#include "../../src/sync/LeaderElection.h"
#include "../../src/sync/CommandSerializer.h"
#include "../../src/sync/StateSerializer.h"
#include "../../src/sync/ConflictResolver.h"
#include "../../src/sync/CommandType.h"

using namespace lightwaveos::sync;
using namespace lightwaveos::state;

//==============================================================================
// Test Fixtures
//==============================================================================

void resetSyncTestState() {
    // Reset any global state if needed
}

//==============================================================================
// DeviceUUID Tests
//==============================================================================

void test_device_uuid_format() {
    // DeviceUUID format should be "LW-XXXXXXXXXXXX"
    const char* uuid = DEVICE_UUID.toString();

    TEST_ASSERT_NOT_NULL(uuid);
    TEST_ASSERT_EQUAL_INT(15, strlen(uuid));  // "LW-" + 12 hex chars
    TEST_ASSERT_EQUAL_STRING_LEN("LW-", uuid, 3);
}

void test_device_uuid_parse_valid() {
    uint8_t mac[6];
    bool result = DeviceUUID::parseUUID("LW-AABBCCDDEEFF", mac);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_HEX8(0xAA, mac[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, mac[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, mac[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, mac[3]);
    TEST_ASSERT_EQUAL_HEX8(0xEE, mac[4]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, mac[5]);
}

void test_device_uuid_parse_invalid_prefix() {
    uint8_t mac[6];
    bool result = DeviceUUID::parseUUID("XX-AABBCCDDEEFF", mac);
    TEST_ASSERT_FALSE(result);
}

void test_device_uuid_parse_invalid_length() {
    uint8_t mac[6];
    bool result = DeviceUUID::parseUUID("LW-AABBCCDD", mac);  // Too short
    TEST_ASSERT_FALSE(result);
}

void test_device_uuid_comparison_higher() {
    // Test that higher MAC compares correctly
    // Default test MAC is DE:AD:BE:EF:00:01
    TEST_ASSERT_TRUE(DEVICE_UUID.isHigherThan("LW-000000000000"));
    TEST_ASSERT_TRUE(DEVICE_UUID.isHigherThan("LW-DEADBEEF0000"));
}

void test_device_uuid_comparison_lower() {
    // Higher MACs should return false from isHigherThan
    TEST_ASSERT_FALSE(DEVICE_UUID.isHigherThan("LW-FFFFFFFFFFFF"));
}

void test_device_uuid_comparison_equal() {
    // Equal MACs should return false (not strictly higher)
    const char* ownUuid = DEVICE_UUID.toString();
    TEST_ASSERT_FALSE(DEVICE_UUID.isHigherThan(ownUuid));
}

void test_device_uuid_matches() {
    const char* ownUuid = DEVICE_UUID.toString();
    TEST_ASSERT_TRUE(DEVICE_UUID.matches(ownUuid));
    TEST_ASSERT_FALSE(DEVICE_UUID.matches("LW-000000000000"));
}

//==============================================================================
// LeaderElection Tests
//==============================================================================

void test_leader_election_no_peers() {
    LeaderElection election;

    // With no peers, we should be leader
    const char* const* noPeers = nullptr;
    SyncRole role = election.evaluate(noPeers, 0);
    TEST_ASSERT_EQUAL(SyncRole::LEADER, role);
    TEST_ASSERT_TRUE(election.isLeader());
}

void test_leader_election_lower_peer() {
    LeaderElection election;

    // Peer with lower UUID - we should be leader
    const char* peers[] = { "LW-000000000000" };
    SyncRole role = election.evaluate(peers, 1);

    TEST_ASSERT_EQUAL(SyncRole::LEADER, role);
    TEST_ASSERT_TRUE(election.isLeader());
}

void test_leader_election_higher_peer() {
    LeaderElection election;

    // Peer with higher UUID - we should be follower
    const char* peers[] = { "LW-FFFFFFFFFFFF" };
    SyncRole role = election.evaluate(peers, 1);

    TEST_ASSERT_EQUAL(SyncRole::FOLLOWER, role);
    TEST_ASSERT_FALSE(election.isLeader());
}

void test_leader_election_multiple_peers() {
    LeaderElection election;

    // Mixed peers - one higher, one lower
    const char* peers[] = { "LW-000000000000", "LW-FFFFFFFFFFFF" };
    SyncRole role = election.evaluate(peers, 2);

    // Should be follower because at least one peer is higher
    TEST_ASSERT_EQUAL(SyncRole::FOLLOWER, role);
}

void test_leader_election_leader_uuid() {
    LeaderElection election;

    // When we're leader, getLeaderUuid should return our UUID
    const char* const* noPeers = nullptr;
    election.evaluate(noPeers, 0);
    const char* leaderUuid = election.getLeaderUuid();

    TEST_ASSERT_NOT_NULL(leaderUuid);
    TEST_ASSERT_TRUE(DEVICE_UUID.matches(leaderUuid));
}

//==============================================================================
// CommandSerializer Tests
//==============================================================================

void test_command_serializer_set_effect() {
    char buffer[256];

    size_t len = CommandSerializer::serializeSetEffect(
        5,              // effectId
        1234,           // version
        "LW-AABBCCDDEEFF",
        buffer,
        sizeof(buffer)
    );

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"c\":\"eff\""));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"e\":5"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"v\":1234"));
}

void test_command_serializer_set_brightness() {
    char buffer[256];

    size_t len = CommandSerializer::serializeSetBrightness(
        200,            // brightness
        5678,           // version
        "LW-112233445566",
        buffer,
        sizeof(buffer)
    );

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"c\":\"bri\""));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"b\":200"));
}

void test_command_serializer_zone_set_effect() {
    char buffer[256];

    size_t len = CommandSerializer::serializeZoneSetEffect(
        2,              // zoneId
        7,              // effectId
        9999,           // version
        "LW-AABBCCDDEEFF",
        buffer,
        sizeof(buffer)
    );

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"c\":\"zef\""));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"z\":2"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"e\":7"));
}

void test_command_serializer_parse_set_effect() {
    const char* json = "{\"t\":\"sync.cmd\",\"c\":\"eff\",\"v\":1234,\"ts\":5678,\"u\":\"LW-AABBCCDDEEFF\",\"p\":{\"e\":42}}";

    ParsedCommand cmd = CommandSerializer::parse(json, strlen(json));

    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_EQUAL(CommandType::SET_EFFECT, cmd.type);
    TEST_ASSERT_EQUAL_UINT32(1234, cmd.version);
    TEST_ASSERT_EQUAL_UINT8(42, cmd.params.effect.effectId);
    // UUID may be truncated in 15-char buffer
    TEST_ASSERT_EQUAL_STRING_LEN("LW-AABBCCDDEEFF", cmd.senderUuid, 15);
}

void test_command_serializer_parse_zone_effect() {
    const char* json = "{\"t\":\"sync.cmd\",\"c\":\"zef\",\"v\":100,\"ts\":200,\"u\":\"LW-112233445566\",\"p\":{\"z\":1,\"e\":15}}";

    ParsedCommand cmd = CommandSerializer::parse(json, strlen(json));

    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_EQUAL(CommandType::ZONE_SET_EFFECT, cmd.type);
    TEST_ASSERT_EQUAL_UINT8(1, cmd.params.zoneEffect.zoneId);
    TEST_ASSERT_EQUAL_UINT8(15, cmd.params.zoneEffect.effectId);
}

void test_command_serializer_roundtrip() {
    char buffer[256];

    // Serialize
    size_t len = CommandSerializer::serializeSetEffect(
        33, 12345, "LW-AABBCCDDEEFF", buffer, sizeof(buffer)
    );
    TEST_ASSERT_GREATER_THAN(0, len);

    // Parse
    ParsedCommand cmd = CommandSerializer::parse(buffer, len);
    TEST_ASSERT_TRUE(cmd.valid);
    TEST_ASSERT_EQUAL(CommandType::SET_EFFECT, cmd.type);
    TEST_ASSERT_EQUAL_UINT8(33, cmd.params.effect.effectId);
    TEST_ASSERT_EQUAL_UINT32(12345, cmd.version);
}

void test_command_code_to_type() {
    TEST_ASSERT_EQUAL(CommandType::SET_EFFECT, codeToCommandType("eff"));
    TEST_ASSERT_EQUAL(CommandType::SET_BRIGHTNESS, codeToCommandType("bri"));
    TEST_ASSERT_EQUAL(CommandType::SET_PALETTE, codeToCommandType("pal"));
    TEST_ASSERT_EQUAL(CommandType::SET_SPEED, codeToCommandType("spd"));
    TEST_ASSERT_EQUAL(CommandType::ZONE_SET_EFFECT, codeToCommandType("zef"));
    TEST_ASSERT_EQUAL(CommandType::SET_ZONE_MODE, codeToCommandType("zmm"));
    TEST_ASSERT_EQUAL(CommandType::UNKNOWN, codeToCommandType("xyz"));
}

//==============================================================================
// StateSerializer Tests
//==============================================================================

void test_state_serializer_basic() {
    SystemState state;
    state.version = 42;
    state.currentEffectId = 5;
    state.brightness = 200;
    state.speed = 20;

    char buffer[512];
    size_t len = StateSerializer::serialize(state, "LW-AABBCCDDEEFF", buffer, sizeof(buffer));

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"t\":\"sync.state\""));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"e\":5"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"b\":200"));
}

void test_state_serializer_is_state_message() {
    const char* stateJson = "{\"t\":\"sync.state\",\"v\":100}";
    const char* cmdJson = "{\"t\":\"sync.cmd\",\"c\":\"eff\"}";

    TEST_ASSERT_TRUE(StateSerializer::isStateMessage(stateJson, strlen(stateJson)));
    TEST_ASSERT_FALSE(StateSerializer::isStateMessage(cmdJson, strlen(cmdJson)));
}

void test_state_serializer_extract_version() {
    const char* json = "{\"t\":\"sync.state\",\"v\":98765,\"ts\":12345}";
    uint32_t version = StateSerializer::extractVersion(json, strlen(json));
    TEST_ASSERT_EQUAL_UINT32(98765, version);
}

void test_state_serializer_roundtrip() {
    SystemState original;
    original.version = 12345;
    original.currentEffectId = 7;
    original.brightness = 180;
    original.speed = 25;
    original.currentPaletteId = 3;
    original.intensity = 200;
    original.saturation = 240;
    original.complexity = 150;
    original.variation = 100;
    original.zoneModeEnabled = true;
    original.activeZoneCount = 2;

    char buffer[512];
    size_t len = StateSerializer::serialize(original, "LW-AABBCCDDEEFF", buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, len);

    SystemState parsed;
    bool result = StateSerializer::parse(buffer, len, parsed);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL_UINT32(original.version, parsed.version);
    TEST_ASSERT_EQUAL_UINT8(original.currentEffectId, parsed.currentEffectId);
    TEST_ASSERT_EQUAL_UINT8(original.brightness, parsed.brightness);
    TEST_ASSERT_EQUAL_UINT8(original.speed, parsed.speed);
    TEST_ASSERT_EQUAL_UINT8(original.currentPaletteId, parsed.currentPaletteId);
    TEST_ASSERT_EQUAL(original.zoneModeEnabled, parsed.zoneModeEnabled);
    TEST_ASSERT_EQUAL_UINT8(original.activeZoneCount, parsed.activeZoneCount);
}

//==============================================================================
// ConflictResolver Tests
//==============================================================================

void test_conflict_resolver_remote_higher_version() {
    ConflictResolver resolver;

    ConflictDecision decision = resolver.resolveCommand(
        100,    // local version
        200,    // remote version (higher)
        false   // not from leader
    );

    TEST_ASSERT_EQUAL(ConflictResult::ACCEPT_REMOTE, decision.result);
}

void test_conflict_resolver_local_higher_version() {
    ConflictResolver resolver;

    ConflictDecision decision = resolver.resolveCommand(
        200,    // local version (higher)
        100,    // remote version
        false   // not from leader
    );

    TEST_ASSERT_EQUAL(ConflictResult::ACCEPT_LOCAL, decision.result);
}

void test_conflict_resolver_same_version_from_leader() {
    ConflictResolver resolver;

    ConflictDecision decision = resolver.resolveCommand(
        100,    // local version
        100,    // remote version (same)
        true    // from leader
    );

    TEST_ASSERT_EQUAL(ConflictResult::ACCEPT_REMOTE, decision.result);
}

void test_conflict_resolver_same_version_not_leader() {
    ConflictResolver resolver;

    ConflictDecision decision = resolver.resolveCommand(
        100,    // local version
        100,    // remote version (same)
        false   // not from leader
    );

    TEST_ASSERT_EQUAL(ConflictResult::ACCEPT_LOCAL, decision.result);
}

void test_conflict_resolver_version_divergence() {
    ConflictResolver resolver;

    // Versions too far apart
    bool divergent = resolver.isVersionDivergent(100, 500);
    TEST_ASSERT_TRUE(divergent);

    divergent = resolver.isVersionDivergent(100, 150);
    TEST_ASSERT_FALSE(divergent);
}

void test_conflict_resolver_version_comparison() {
    // Normal comparison
    TEST_ASSERT_TRUE(ConflictResolver::compareVersions(100, 200) < 0);
    TEST_ASSERT_TRUE(ConflictResolver::compareVersions(200, 100) > 0);
    TEST_ASSERT_EQUAL(0, ConflictResolver::compareVersions(100, 100));
}

void test_conflict_resolver_version_distance() {
    TEST_ASSERT_EQUAL_UINT32(50, ConflictResolver::versionDistance(100, 150));
    TEST_ASSERT_EQUAL_UINT32(50, ConflictResolver::versionDistance(150, 100));
    TEST_ASSERT_EQUAL_UINT32(0, ConflictResolver::versionDistance(100, 100));
}

//==============================================================================
// SyncProtocol Constants Tests
//==============================================================================

void test_sync_protocol_constants() {
    TEST_ASSERT_EQUAL(SYNC_PROTOCOL_VERSION, 1);
    TEST_ASSERT_EQUAL(MAX_DISCOVERED_PEERS, 8);
    TEST_ASSERT_EQUAL(MAX_PEER_CONNECTIONS, 4);
    TEST_ASSERT_EQUAL(PEER_SCAN_INTERVAL_MS, 30000);
    TEST_ASSERT_EQUAL(HEARTBEAT_INTERVAL_MS, 10000);
    TEST_ASSERT_EQUAL(HEARTBEAT_MISS_LIMIT, 3);
}

void test_sync_role_to_string() {
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", syncRoleToString(SyncRole::UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("LEADER", syncRoleToString(SyncRole::LEADER));
    TEST_ASSERT_EQUAL_STRING("FOLLOWER", syncRoleToString(SyncRole::FOLLOWER));
}

void test_sync_state_to_string() {
    TEST_ASSERT_EQUAL_STRING("INITIALIZING", syncStateToString(SyncState::INITIALIZING));
    TEST_ASSERT_EQUAL_STRING("DISCOVERING", syncStateToString(SyncState::DISCOVERING));
    TEST_ASSERT_EQUAL_STRING("ELECTING", syncStateToString(SyncState::ELECTING));
    TEST_ASSERT_EQUAL_STRING("LEADING", syncStateToString(SyncState::LEADING));
    TEST_ASSERT_EQUAL_STRING("FOLLOWING", syncStateToString(SyncState::FOLLOWING));
    TEST_ASSERT_EQUAL_STRING("SYNCHRONIZED", syncStateToString(SyncState::SYNCHRONIZED));
}

//==============================================================================
// Test Suite Runner
//==============================================================================

void run_sync_tests() {
    // DeviceUUID tests
    RUN_TEST(test_device_uuid_format);
    RUN_TEST(test_device_uuid_parse_valid);
    RUN_TEST(test_device_uuid_parse_invalid_prefix);
    RUN_TEST(test_device_uuid_parse_invalid_length);
    RUN_TEST(test_device_uuid_comparison_higher);
    RUN_TEST(test_device_uuid_comparison_lower);
    RUN_TEST(test_device_uuid_comparison_equal);
    RUN_TEST(test_device_uuid_matches);

    // LeaderElection tests
    RUN_TEST(test_leader_election_no_peers);
    RUN_TEST(test_leader_election_lower_peer);
    RUN_TEST(test_leader_election_higher_peer);
    RUN_TEST(test_leader_election_multiple_peers);
    RUN_TEST(test_leader_election_leader_uuid);

    // CommandSerializer tests
    RUN_TEST(test_command_serializer_set_effect);
    RUN_TEST(test_command_serializer_set_brightness);
    RUN_TEST(test_command_serializer_zone_set_effect);
    RUN_TEST(test_command_serializer_parse_set_effect);
    RUN_TEST(test_command_serializer_parse_zone_effect);
    RUN_TEST(test_command_serializer_roundtrip);
    RUN_TEST(test_command_code_to_type);

    // StateSerializer tests
    RUN_TEST(test_state_serializer_basic);
    RUN_TEST(test_state_serializer_is_state_message);
    RUN_TEST(test_state_serializer_extract_version);
    RUN_TEST(test_state_serializer_roundtrip);

    // ConflictResolver tests
    RUN_TEST(test_conflict_resolver_remote_higher_version);
    RUN_TEST(test_conflict_resolver_local_higher_version);
    RUN_TEST(test_conflict_resolver_same_version_from_leader);
    RUN_TEST(test_conflict_resolver_same_version_not_leader);
    RUN_TEST(test_conflict_resolver_version_divergence);
    RUN_TEST(test_conflict_resolver_version_comparison);
    RUN_TEST(test_conflict_resolver_version_distance);

    // SyncProtocol tests
    RUN_TEST(test_sync_protocol_constants);
    RUN_TEST(test_sync_role_to_string);
    RUN_TEST(test_sync_state_to_string);
}
