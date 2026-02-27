// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// Bebas Neue 48px - converted from LVGL to simple bitmap format for M5GFX
// This is a TEMPORARY solution - just uses Font8 at larger sizes

#pragma once

// For now, we just define a macro to use Font8 scaled up
// Font8 is FreeSans which is close enough to Bebas Neue
#define BEBAS_LABEL  &fonts::Font8
#define BEBAS_VALUE  &fonts::Font8

// Scale factors for different uses
#define BEBAS_LABEL_SCALE  2.5f
#define BEBAS_VALUE_SCALE  5.0f
#define BEBAS_HERO_SCALE   6.5f
