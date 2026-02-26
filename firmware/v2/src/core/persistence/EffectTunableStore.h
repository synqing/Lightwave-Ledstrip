/**
 * @file EffectTunableStore.h
 * @brief Per-effect tunable persistence (NVS with volatile fallback)
 */

#pragma once

#include <cstdint>

#include "NVSManager.h"
#include "../../config/effect_ids.h"
#include "../../plugins/api/IEffect.h"

namespace lightwaveos {
namespace persistence {

class EffectTunableStore {
public:
    enum class Mode : uint8_t {
        NVS = 0,
        VOLATILE = 1
    };

    struct Status {
        const char* mode = "volatile";
        bool dirty = false;
        const char* lastError = nullptr;
    };

    static EffectTunableStore& instance();

    void init();
    void onEffectActivated(EffectId effectId, plugins::IEffect* effect);
    void onParameterApplied(EffectId effectId, plugins::IEffect* effect);
    void tick(uint32_t nowMs);
    Status getStatus(EffectId effectId) const;

private:
    static constexpr uint8_t kVersion = 1;
    static constexpr const char* kNamespace = "effect_tn";
    static constexpr uint8_t kMaxEffects = 24;
    static constexpr uint8_t kMaxEntriesPerEffect = 24;
    static constexpr uint32_t kDebounceMs = 1500;

    struct TunableEntry {
        char name[plugins::kEffectParameterNameMaxLen + 1];
        float value;
    };

    struct EffectBlob {
        uint8_t version;
        EffectId effectId;
        uint8_t count;
        TunableEntry entries[kMaxEntriesPerEffect];
        uint32_t checksum;
    };

    struct RuntimeRecord {
        bool used = false;
        bool dirty = false;
        uint32_t lastChangeMs = 0;
        EffectBlob blob{};
    };

    EffectTunableStore() = default;
    EffectTunableStore(const EffectTunableStore&) = delete;
    EffectTunableStore& operator=(const EffectTunableStore&) = delete;

    RuntimeRecord* findRecord(EffectId effectId);
    const RuntimeRecord* findRecord(EffectId effectId) const;
    RuntimeRecord* ensureRecord(EffectId effectId);

    static uint32_t checksumFor(const EffectBlob& blob);
    static bool isBlobValid(const EffectBlob& blob);
    static void makeKey(EffectId effectId, char* outKey, size_t outLen);

    void captureNonDefault(plugins::IEffect* effect, EffectBlob& outBlob);
    bool saveBlob(const EffectBlob& blob);
    bool loadBlob(EffectId effectId, EffectBlob& outBlob);
    void applyBlob(const EffectBlob& blob, plugins::IEffect* effect);

    const char* modeString() const;
    void enterVolatileFallback(NVSResult err);

    bool m_initialised = false;
    Mode m_mode = Mode::VOLATILE;
    NVSResult m_lastNvsError = NVSResult::OK;
    RuntimeRecord m_records[kMaxEffects] = {};
};

} // namespace persistence
} // namespace lightwaveos

