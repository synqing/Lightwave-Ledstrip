#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>

#include "network/RequestValidator.h"
#include "effects/transitions/TransitionTypes.h"
#include "config/effect_ids.h"

// Reproduce the ManualStaging contract for the native tests. Mirrors the
// definition in core/actors/ActorSystem.h — kept in sync by hand because
// the production header pulls in FreeRTOS / unique_ptr / Actor machinery
// the native test runner doesn't link against.
namespace lightwaveos { namespace actors {
struct ManualStagingForTest {
    static constexpr uint8_t kNoTransition = 0xFF;
    uint8_t  queuedTransitionType   = kNoTransition;
    uint16_t queuedDurationMs       = 0;
    uint8_t  queuedEasing           = 0xFF;
    EffectId stagedEffect           = lightwaveos::INVALID_EFFECT_ID;

    bool hasQueuedTransition() const { return queuedTransitionType != kNoTransition; }
    bool hasStagedEffect()    const { return stagedEffect != lightwaveos::INVALID_EFFECT_ID; }
    bool isArmedPair()        const { return hasStagedEffect() && hasQueuedTransition(); }
    bool isAnyArmed()         const { return hasStagedEffect() || hasQueuedTransition(); }

    void queue(uint8_t t, uint16_t d = 0, uint8_t e = 0xFF) {
        queuedTransitionType = t; queuedDurationMs = d; queuedEasing = e;
    }
    void clearQueue() {
        queuedTransitionType = kNoTransition; queuedDurationMs = 0; queuedEasing = 0xFF;
    }
    void stage(EffectId eid) { stagedEffect = eid; }
    void clearStage() { stagedEffect = lightwaveos::INVALID_EFFECT_ID; }
    void disarmAll() { clearQueue(); clearStage(); }
};
}} // namespace lightwaveos::actors

using lightwaveos::network::RequestSchemas::TransitionConfig;
using lightwaveos::network::RequestSchemas::TransitionConfigSize;
using lightwaveos::network::RequestSchemas::TriggerTransition;
using lightwaveos::network::RequestSchemas::TriggerTransitionSize;
using lightwaveos::network::RequestValidator;
using lightwaveos::transitions::TransitionTier;
using lightwaveos::transitions::TransitionType;
using lightwaveos::transitions::getDefaultDuration;
using lightwaveos::transitions::getTierDuration;
using lightwaveos::transitions::getTierName;
using lightwaveos::transitions::getTransitionTier;
using lightwaveos::transitions::getTransitionsInTier;
using lightwaveos::transitions::kCinematicDurationMs;
using lightwaveos::transitions::kMediumDurationMs;
using lightwaveos::transitions::kQuickDurationMs;

static lightwaveos::network::ValidationResult validateTrigger(const char* body,
                                                              JsonDocument& doc) {
    return RequestValidator::parseAndValidate(
        reinterpret_cast<const uint8_t*>(body),
        std::strlen(body),
        doc,
        TriggerTransition,
        TriggerTransitionSize);
}

static lightwaveos::network::ValidationResult validateConfig(const char* body,
                                                             JsonDocument& doc) {
    return RequestValidator::parseAndValidate(
        reinterpret_cast<const uint8_t*>(body),
        std::strlen(body),
        doc,
        TransitionConfig,
        TransitionConfigSize);
}

void test_transition_trigger_rejects_type_outside_runtime_range() {
    JsonDocument doc;
    auto result = validateTrigger("{\"toEffect\":4660,\"type\":12}", doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("type", result.fieldName);
}

void test_transition_trigger_accepts_max_runtime_type() {
    JsonDocument doc;
    auto result = validateTrigger("{\"toEffect\":4660,\"type\":11}", doc);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(11, doc["type"].as<uint8_t>());
}

void test_transition_trigger_rejects_easing_outside_runtime_range() {
    JsonDocument doc;
    auto result = validateTrigger("{\"toEffect\":4660,\"easing\":15}", doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("easing", result.fieldName);
}

void test_transition_trigger_accepts_max_runtime_easing() {
    JsonDocument doc;
    auto result = validateTrigger("{\"toEffect\":4660,\"easing\":14}", doc);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(14, doc["easing"].as<uint8_t>());
}

void test_transition_config_rejects_default_type_outside_runtime_range() {
    JsonDocument doc;
    auto result = validateConfig("{\"defaultType\":12}", doc);

    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_STRING("defaultType", result.fieldName);
}

void test_transition_config_accepts_max_runtime_default_type() {
    JsonDocument doc;
    auto result = validateConfig("{\"defaultType\":11}", doc);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(11, doc["defaultType"].as<uint8_t>());
}

void test_transition_config_accepts_enabled_kill_switch() {
    JsonDocument doc;
    auto result = validateConfig("{\"enabled\":false}", doc);

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_FALSE(doc["enabled"].as<bool>());
}

// ============================================================================
// Phase 2.2 — Tier system
// ============================================================================

void test_tier_duration_constants() {
    TEST_ASSERT_EQUAL_UINT16(500,  kQuickDurationMs);
    TEST_ASSERT_EQUAL_UINT16(1500, kMediumDurationMs);
    TEST_ASSERT_EQUAL_UINT16(2500, kCinematicDurationMs);
    TEST_ASSERT_EQUAL_UINT16(500,  getTierDuration(TransitionTier::QUICK));
    TEST_ASSERT_EQUAL_UINT16(1500, getTierDuration(TransitionTier::MEDIUM));
    TEST_ASSERT_EQUAL_UINT16(2500, getTierDuration(TransitionTier::CINEMATIC));
}

void test_tier_assignment_quick() {
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::FADE)     == TransitionTier::QUICK);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::WIPE_OUT) == TransitionTier::QUICK);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::WIPE_IN)  == TransitionTier::QUICK);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::DISSOLVE) == TransitionTier::QUICK);
}

void test_tier_assignment_medium() {
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::PHASE_SHIFT) == TransitionTier::MEDIUM);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::PULSEWAVE)   == TransitionTier::MEDIUM);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::IMPLOSION)   == TransitionTier::MEDIUM);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::IRIS)        == TransitionTier::MEDIUM);
}

void test_tier_assignment_cinematic() {
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::NUCLEAR)      == TransitionTier::CINEMATIC);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::STARGATE)     == TransitionTier::CINEMATIC);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::KALEIDOSCOPE) == TransitionTier::CINEMATIC);
    TEST_ASSERT_TRUE(getTransitionTier(TransitionType::MANDALA)      == TransitionTier::CINEMATIC);
}

void test_get_default_duration_returns_tier_value() {
    // Phase 2.2 contract: getDefaultDuration delegates via tier.
    TEST_ASSERT_EQUAL_UINT16(500,  getDefaultDuration(TransitionType::FADE));
    TEST_ASSERT_EQUAL_UINT16(500,  getDefaultDuration(TransitionType::WIPE_OUT));
    TEST_ASSERT_EQUAL_UINT16(1500, getDefaultDuration(TransitionType::PULSEWAVE));
    TEST_ASSERT_EQUAL_UINT16(1500, getDefaultDuration(TransitionType::IRIS));
    TEST_ASSERT_EQUAL_UINT16(2500, getDefaultDuration(TransitionType::STARGATE));
    TEST_ASSERT_EQUAL_UINT16(2500, getDefaultDuration(TransitionType::MANDALA));
}

void test_all_tier_members_share_duration() {
    // Every transition in a tier must return the canonical tier duration.
    for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); ++i) {
        const auto type = static_cast<TransitionType>(i);
        const TransitionTier tier = getTransitionTier(type);
        TEST_ASSERT_EQUAL_UINT16(getTierDuration(tier), getDefaultDuration(type));
    }
}

void test_get_transitions_in_tier_returns_four_members() {
    TransitionType quick[4];
    TransitionType medium[4];
    TransitionType cinematic[4];
    getTransitionsInTier(TransitionTier::QUICK, quick);
    getTransitionsInTier(TransitionTier::MEDIUM, medium);
    getTransitionsInTier(TransitionTier::CINEMATIC, cinematic);

    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_TRUE(getTransitionTier(quick[i])     == TransitionTier::QUICK);
        TEST_ASSERT_TRUE(getTransitionTier(medium[i])    == TransitionTier::MEDIUM);
        TEST_ASSERT_TRUE(getTransitionTier(cinematic[i]) == TransitionTier::CINEMATIC);
    }
}

void test_tier_name_returns_canonical_strings() {
    TEST_ASSERT_EQUAL_STRING("Quick",     getTierName(TransitionTier::QUICK));
    TEST_ASSERT_EQUAL_STRING("Medium",    getTierName(TransitionTier::MEDIUM));
    TEST_ASSERT_EQUAL_STRING("Cinematic", getTierName(TransitionTier::CINEMATIC));
}

// ============================================================================
// Phase 2.3 — ManualStaging state machine
// ============================================================================

using lightwaveos::actors::ManualStagingForTest;

void test_manual_staging_starts_idle() {
    ManualStagingForTest s;
    TEST_ASSERT_FALSE(s.hasQueuedTransition());
    TEST_ASSERT_FALSE(s.hasStagedEffect());
    TEST_ASSERT_FALSE(s.isArmedPair());
    TEST_ASSERT_FALSE(s.isAnyArmed());
    TEST_ASSERT_EQUAL_UINT8(0xFF, s.queuedTransitionType);
    TEST_ASSERT_EQUAL_UINT16(lightwaveos::INVALID_EFFECT_ID, s.stagedEffect);
}

void test_queue_transition_then_clear() {
    ManualStagingForTest s;
    s.queue(5, 1500, 3);
    TEST_ASSERT_TRUE(s.hasQueuedTransition());
    TEST_ASSERT_EQUAL_UINT8(5, s.queuedTransitionType);
    TEST_ASSERT_EQUAL_UINT16(1500, s.queuedDurationMs);
    TEST_ASSERT_EQUAL_UINT8(3, s.queuedEasing);
    TEST_ASSERT_FALSE(s.hasStagedEffect());
    TEST_ASSERT_FALSE(s.isArmedPair());
    TEST_ASSERT_TRUE(s.isAnyArmed());

    s.clearQueue();
    TEST_ASSERT_FALSE(s.hasQueuedTransition());
    TEST_ASSERT_FALSE(s.isAnyArmed());
}

void test_stage_effect_then_clear() {
    ManualStagingForTest s;
    s.stage(static_cast<lightwaveos::EffectId>(0x0A02));
    TEST_ASSERT_TRUE(s.hasStagedEffect());
    TEST_ASSERT_FALSE(s.hasQueuedTransition());
    TEST_ASSERT_FALSE(s.isArmedPair());
    TEST_ASSERT_TRUE(s.isAnyArmed());
    TEST_ASSERT_EQUAL_UINT16(0x0A02, s.stagedEffect);

    s.clearStage();
    TEST_ASSERT_FALSE(s.hasStagedEffect());
    TEST_ASSERT_FALSE(s.isAnyArmed());
}

void test_armed_pair_predicate() {
    ManualStagingForTest s;
    TEST_ASSERT_FALSE(s.isArmedPair());

    s.queue(0);  // FADE
    TEST_ASSERT_FALSE(s.isArmedPair());  // transition only

    s.stage(static_cast<lightwaveos::EffectId>(0x0A02));
    TEST_ASSERT_TRUE(s.isArmedPair());  // both populated

    s.clearQueue();
    TEST_ASSERT_FALSE(s.isArmedPair());  // effect only
}

void test_re_queue_replaces_prior_type() {
    ManualStagingForTest s;
    s.queue(0);
    TEST_ASSERT_EQUAL_UINT8(0, s.queuedTransitionType);

    s.queue(11);
    TEST_ASSERT_EQUAL_UINT8(11, s.queuedTransitionType);
    // Still queued (not stacked) — only ever ONE queued transition.
    TEST_ASSERT_TRUE(s.hasQueuedTransition());
}

void test_disarm_all_clears_both_slots() {
    ManualStagingForTest s;
    s.queue(7);
    s.stage(static_cast<lightwaveos::EffectId>(0x0501));
    TEST_ASSERT_TRUE(s.isArmedPair());

    s.disarmAll();
    TEST_ASSERT_FALSE(s.hasQueuedTransition());
    TEST_ASSERT_FALSE(s.hasStagedEffect());
    TEST_ASSERT_FALSE(s.isAnyArmed());
}

// ============================================================================
// Phase 2.4 — getRealLeadTime math contract
// ============================================================================
// ActorSystem::getRealLeadTime() = effectiveDuration + 50 (safety margin),
// where effectiveDuration = requestedMs != 0 ? requestedMs : getDefaultDuration(type).
// Tests below validate this contract WITHOUT instantiating ActorSystem
// (which would pull in FreeRTOS). The production impl in
// ActorSystem.cpp is a one-line delegate to getDefaultDuration; if these
// pass and the production impl matches, the REST endpoint will agree.

static constexpr uint16_t kSafetyMargin = 50;

static uint16_t leadTimeContract(uint8_t typeIdx, uint16_t requestedMs) {
    if (typeIdx >= static_cast<uint8_t>(TransitionType::TYPE_COUNT)) return 0;
    const auto tt = static_cast<TransitionType>(typeIdx);
    const uint16_t eff = (requestedMs != 0) ? requestedMs : getDefaultDuration(tt);
    return eff + kSafetyMargin;
}

void test_lead_time_fade_default() {
    // Fade is QUICK → 500ms + 50 = 550ms
    TEST_ASSERT_EQUAL_UINT16(550, leadTimeContract(0, 0));
}

void test_lead_time_stargate_default() {
    // Stargate is CINEMATIC → 2500ms + 50 = 2550ms
    TEST_ASSERT_EQUAL_UINT16(2550, leadTimeContract(9, 0));
}

void test_lead_time_explicit_duration_overrides_tier() {
    // Fade with explicit 1000ms duration → 1000 + 50 = 1050ms
    TEST_ASSERT_EQUAL_UINT16(1050, leadTimeContract(0, 1000));
}

void test_lead_time_invalid_type_returns_zero() {
    // type=12 is TYPE_COUNT (out of range) → 0 (no lead)
    TEST_ASSERT_EQUAL_UINT16(0, leadTimeContract(12, 0));
    TEST_ASSERT_EQUAL_UINT16(0, leadTimeContract(255, 0));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_transition_trigger_rejects_type_outside_runtime_range);
    RUN_TEST(test_transition_trigger_accepts_max_runtime_type);
    RUN_TEST(test_transition_trigger_rejects_easing_outside_runtime_range);
    RUN_TEST(test_transition_trigger_accepts_max_runtime_easing);
    RUN_TEST(test_transition_config_rejects_default_type_outside_runtime_range);
    RUN_TEST(test_transition_config_accepts_max_runtime_default_type);
    RUN_TEST(test_transition_config_accepts_enabled_kill_switch);
    RUN_TEST(test_tier_duration_constants);
    RUN_TEST(test_tier_assignment_quick);
    RUN_TEST(test_tier_assignment_medium);
    RUN_TEST(test_tier_assignment_cinematic);
    RUN_TEST(test_get_default_duration_returns_tier_value);
    RUN_TEST(test_all_tier_members_share_duration);
    RUN_TEST(test_get_transitions_in_tier_returns_four_members);
    RUN_TEST(test_tier_name_returns_canonical_strings);
    RUN_TEST(test_manual_staging_starts_idle);
    RUN_TEST(test_queue_transition_then_clear);
    RUN_TEST(test_stage_effect_then_clear);
    RUN_TEST(test_armed_pair_predicate);
    RUN_TEST(test_re_queue_replaces_prior_type);
    RUN_TEST(test_disarm_all_clears_both_slots);
    RUN_TEST(test_lead_time_fade_default);
    RUN_TEST(test_lead_time_stargate_default);
    RUN_TEST(test_lead_time_explicit_duration_overrides_tier);
    RUN_TEST(test_lead_time_invalid_type_returns_zero);
    return UNITY_END();
}

#endif
