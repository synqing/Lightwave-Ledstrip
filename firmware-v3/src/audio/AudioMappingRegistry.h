// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioMappingRegistry.h
 * @brief Compatibility header for AudioMappingRegistry
 *
 * Some network handlers historically included `audio/AudioMappingRegistry.h`.
 * The implementation now lives in `audio/contracts/AudioEffectMapping.h`.
 *
 * Keep this shim to preserve include stability across refactors.
 */

#pragma once

#include "contracts/AudioEffectMapping.h"


