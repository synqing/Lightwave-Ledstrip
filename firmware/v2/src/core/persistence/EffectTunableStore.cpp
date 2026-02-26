/**
 * @file EffectTunableStore.cpp
 * @brief Per-effect tunable persistence implementation
 */

#include "EffectTunableStore.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace lightwaveos {
namespace persistence {

EffectTunableStore& EffectTunableStore::instance() {
    static EffectTunableStore s_instance;
    return s_instance;
}

void EffectTunableStore::init() {
    if (m_initialised) {
        return;
    }
    m_initialised = true;
    m_lastNvsError = NVSResult::OK;
    memset(m_records, 0, sizeof(m_records));

    if (NVS_MANAGER.isInitialized() || NVS_MANAGER.init()) {
        m_mode = Mode::NVS;
    } else {
        m_mode = Mode::VOLATILE;
        m_lastNvsError = NVSResult::NOT_INITIALIZED;
    }
}

void EffectTunableStore::onEffectActivated(EffectId effectId, plugins::IEffect* effect) {
    if (!effect) {
        return;
    }
    if (!m_initialised) {
        init();
    }

    EffectBlob blob{};
    bool loaded = false;

    if (m_mode == Mode::NVS) {
        loaded = loadBlob(effectId, blob);
    }

    if (!loaded) {
        const RuntimeRecord* rec = findRecord(effectId);
        if (rec && rec->used && rec->blob.count > 0) {
            blob = rec->blob;
            loaded = true;
        }
    }

    if (loaded) {
        applyBlob(blob, effect);
    }
}

void EffectTunableStore::onParameterApplied(EffectId effectId, plugins::IEffect* effect) {
    if (!effect) {
        return;
    }
    if (!m_initialised) {
        init();
    }

    RuntimeRecord* rec = ensureRecord(effectId);
    if (!rec) {
        if (m_mode == Mode::NVS) {
            enterVolatileFallback(NVSResult::WRITE_ERROR);
        }
        return;
    }

    captureNonDefault(effect, rec->blob);
    rec->used = true;
    rec->dirty = true;
    rec->lastChangeMs = millis();
}

void EffectTunableStore::tick(uint32_t nowMs) {
    if (!m_initialised) {
        init();
    }
    if (m_mode != Mode::NVS) {
        return;
    }

    for (uint8_t i = 0; i < kMaxEffects; ++i) {
        RuntimeRecord& rec = m_records[i];
        if (!rec.used || !rec.dirty) {
            continue;
        }
        if ((nowMs - rec.lastChangeMs) < kDebounceMs) {
            continue;
        }
        if (saveBlob(rec.blob)) {
            rec.dirty = false;
        } else {
            rec.dirty = false;
            break;
        }
    }
}

EffectTunableStore::Status EffectTunableStore::getStatus(EffectId effectId) const {
    Status status;
    status.mode = modeString();
    status.lastError = (m_lastNvsError == NVSResult::OK) ? nullptr : NVSManager::resultToString(m_lastNvsError);

    const RuntimeRecord* rec = findRecord(effectId);
    status.dirty = (rec && rec->used && rec->dirty);
    return status;
}

EffectTunableStore::RuntimeRecord* EffectTunableStore::findRecord(EffectId effectId) {
    for (uint8_t i = 0; i < kMaxEffects; ++i) {
        if (m_records[i].used && m_records[i].blob.effectId == effectId) {
            return &m_records[i];
        }
    }
    return nullptr;
}

const EffectTunableStore::RuntimeRecord* EffectTunableStore::findRecord(EffectId effectId) const {
    for (uint8_t i = 0; i < kMaxEffects; ++i) {
        if (m_records[i].used && m_records[i].blob.effectId == effectId) {
            return &m_records[i];
        }
    }
    return nullptr;
}

EffectTunableStore::RuntimeRecord* EffectTunableStore::ensureRecord(EffectId effectId) {
    RuntimeRecord* rec = findRecord(effectId);
    if (rec) {
        return rec;
    }
    for (uint8_t i = 0; i < kMaxEffects; ++i) {
        if (!m_records[i].used) {
            m_records[i].used = true;
            m_records[i].dirty = false;
            m_records[i].lastChangeMs = 0;
            memset(&m_records[i].blob, 0, sizeof(EffectBlob));
            m_records[i].blob.version = kVersion;
            m_records[i].blob.effectId = effectId;
            return &m_records[i];
        }
    }
    return nullptr;
}

uint32_t EffectTunableStore::checksumFor(const EffectBlob& blob) {
    const size_t dataSize = offsetof(EffectBlob, checksum);
    return NVSManager::calculateCRC32(&blob, dataSize);
}

bool EffectTunableStore::isBlobValid(const EffectBlob& blob) {
    if (blob.version != kVersion) {
        return false;
    }
    if (blob.count > kMaxEntriesPerEffect) {
        return false;
    }
    return checksumFor(blob) == blob.checksum;
}

void EffectTunableStore::makeKey(EffectId effectId, char* outKey, size_t outLen) {
    if (!outKey || outLen == 0) {
        return;
    }
    snprintf(outKey, outLen, "fx_%04X", static_cast<unsigned>(effectId & 0xFFFF));
}

void EffectTunableStore::captureNonDefault(plugins::IEffect* effect, EffectBlob& outBlob) {
    outBlob.version = kVersion;
    outBlob.count = 0;

    if (!effect) {
        outBlob.checksum = checksumFor(outBlob);
        return;
    }

    const uint8_t paramCount = effect->getParameterCount();
    for (uint8_t i = 0; i < paramCount; ++i) {
        const plugins::EffectParameter* param = effect->getParameter(i);
        if (!param || !param->name || param->name[0] == '\0') {
            continue;
        }

        const float value = effect->getParameter(param->name);
        const float delta = fabsf(value - param->defaultValue);
        if (delta <= 0.00001f) {
            continue;
        }

        if (outBlob.count >= kMaxEntriesPerEffect) {
            break;
        }

        TunableEntry& dst = outBlob.entries[outBlob.count++];
        strncpy(dst.name, param->name, sizeof(dst.name) - 1);
        dst.name[sizeof(dst.name) - 1] = '\0';
        dst.value = value;
    }

    outBlob.checksum = checksumFor(outBlob);
}

bool EffectTunableStore::saveBlob(const EffectBlob& blob) {
    if (m_mode != Mode::NVS) {
        return false;
    }

    char key[12] = {};
    makeKey(blob.effectId, key, sizeof(key));

    EffectBlob saveCopy = blob;
    saveCopy.checksum = checksumFor(saveCopy);
    const NVSResult res = NVS_MANAGER.saveBlob(kNamespace, key, &saveCopy, sizeof(saveCopy));
    if (res != NVSResult::OK) {
        enterVolatileFallback(res);
        return false;
    }

    m_lastNvsError = NVSResult::OK;
    return true;
}

bool EffectTunableStore::loadBlob(EffectId effectId, EffectBlob& outBlob) {
    if (m_mode != Mode::NVS) {
        return false;
    }

    char key[12] = {};
    makeKey(effectId, key, sizeof(key));
    const NVSResult res = NVS_MANAGER.loadBlob(kNamespace, key, &outBlob, sizeof(outBlob));
    if (res == NVSResult::NOT_FOUND) {
        return false;
    }
    if (res != NVSResult::OK) {
        enterVolatileFallback(res);
        return false;
    }
    if (!isBlobValid(outBlob) || outBlob.effectId != effectId) {
        enterVolatileFallback(NVSResult::CHECKSUM_ERROR);
        return false;
    }

    RuntimeRecord* rec = ensureRecord(effectId);
    if (rec) {
        rec->blob = outBlob;
        rec->dirty = false;
        rec->lastChangeMs = 0;
    }
    return true;
}

void EffectTunableStore::applyBlob(const EffectBlob& blob, plugins::IEffect* effect) {
    if (!effect) {
        return;
    }
    for (uint8_t i = 0; i < blob.count; ++i) {
        const TunableEntry& entry = blob.entries[i];
        if (entry.name[0] == '\0') {
            continue;
        }
        effect->setParameter(entry.name, entry.value);
    }
}

const char* EffectTunableStore::modeString() const {
    return (m_mode == Mode::NVS) ? "nvs" : "volatile";
}

void EffectTunableStore::enterVolatileFallback(NVSResult err) {
    m_mode = Mode::VOLATILE;
    m_lastNvsError = err;
}

} // namespace persistence
} // namespace lightwaveos
