// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file NodeOrchestrator.h
 * @brief Compatibility header - forwards to ActorSystem.h
 *
 * This header provides backward compatibility for code that references
 * NodeOrchestrator (the old name) instead of ActorSystem (the new name).
 */

#pragma once

#include "ActorSystem.h"

namespace lightwaveos {
namespace nodes {
    // Type alias for backward compatibility
    using NodeOrchestrator = actors::ActorSystem;
} // namespace nodes
} // namespace lightwaveos
