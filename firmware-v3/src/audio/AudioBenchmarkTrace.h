// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioBenchmarkTrace.h
 * @brief Legacy include path -- redirects to config/Trace.h
 *
 * All TRACE_* macros now live in config/Trace.h so they can be used
 * system-wide (audio, render, network, etc.). This header exists
 * purely so existing #include "AudioBenchmarkTrace.h" directives
 * continue to compile without changes.
 */

#pragma once

#include "../config/Trace.h"
