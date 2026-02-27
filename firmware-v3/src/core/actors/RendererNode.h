// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file RendererNode.h
 * @brief Compatibility header - forwards to RendererActor.h
 *
 * This header provides backward compatibility for code that references
 * RendererNode (the old name) instead of RendererActor (the new name).
 */

#pragma once

#include "RendererActor.h"

namespace lightwaveos {
namespace nodes {
    // Type alias for backward compatibility
    using RendererNode = actors::RendererActor;
} // namespace nodes
} // namespace lightwaveos
