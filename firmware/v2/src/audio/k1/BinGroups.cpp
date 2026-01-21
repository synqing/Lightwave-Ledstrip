/**
 * @file BinGroups.cpp
 * @brief Bin group manager implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "BinGroups.h"
#include <algorithm>
#include <map>

namespace lightwaveos {
namespace audio {
namespace k1 {

BinGroups::BinGroups() {
}

BinGroups::~BinGroups() {
    m_groups.clear();
}

bool BinGroups::buildGroups(const GoertzelBinSpec* specs, size_t num_bins) {
    if (specs == nullptr || num_bins == 0) {
        return false;
    }

    m_groups.clear();

    // Group bins by N using a map
    std::map<uint16_t, std::vector<uint8_t>> n_to_indices;

    for (size_t i = 0; i < num_bins; i++) {
        uint16_t N = specs[i].N;
        n_to_indices[N].push_back(static_cast<uint8_t>(i));
    }

    // Convert map to BinGroup vector
    for (const auto& pair : n_to_indices) {
        BinGroup group;
        group.N = pair.first;
        group.indices = pair.second;
        m_groups.push_back(group);
    }

    return true;
}

const BinGroup* BinGroups::getGroup(size_t group_idx) const {
    if (group_idx >= m_groups.size()) {
        return nullptr;
    }
    return &m_groups[group_idx];
}

const BinGroup* BinGroups::findGroupByN(uint16_t N) const {
    for (const auto& group : m_groups) {
        if (group.N == N) {
            return &group;
        }
    }
    return nullptr;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

