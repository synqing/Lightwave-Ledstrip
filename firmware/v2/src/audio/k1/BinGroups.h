/**
 * @file BinGroups.h
 * @brief Bin-to-group index maps for efficient group-by-N processing
 *
 * Groups bins by window length N to enable shared history/window operations.
 * Produced once at init for each bank.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "K1Types.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Group structure: bins sharing the same N
 */
struct BinGroup {
    uint16_t N;                          ///< Window length for this group
    std::vector<uint8_t> indices;         ///< Bin indices in this group
};

/**
 * @brief Bin group manager for a bank
 *
 * Organizes bins into groups by window length N.
 */
class BinGroups {
public:
    /**
     * @brief Constructor
     */
    BinGroups();

    /**
     * @brief Destructor
     */
    ~BinGroups();

    /**
     * @brief Build groups from bin specifications
     *
     * @param specs Array of GoertzelBinSpec
     * @param num_bins Number of bins
     * @return true if successful
     */
    bool buildGroups(const GoertzelBinSpec* specs, size_t num_bins);

    /**
     * @brief Get number of groups
     */
    size_t getGroupCount() const { return m_groups.size(); }

    /**
     * @brief Get group by index
     *
     * @param group_idx Group index (0 to getGroupCount()-1)
     * @return Pointer to BinGroup, or nullptr if invalid
     */
    const BinGroup* getGroup(size_t group_idx) const;

    /**
     * @brief Find group for specific N
     *
     * @param N Window length
     * @return Pointer to BinGroup, or nullptr if not found
     */
    const BinGroup* findGroupByN(uint16_t N) const;

private:
    std::vector<BinGroup> m_groups;      ///< Array of groups
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

