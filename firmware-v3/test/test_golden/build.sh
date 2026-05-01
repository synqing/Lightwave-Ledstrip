#!/usr/bin/env bash
# build.sh — canonical build command for the golden-frame native runner.
#
# This script is the single source of truth for the native build flags.
# Do not duplicate the flag list in source comments — point at this file.
#
# Run from anywhere; cd's to its own directory first.

set -euo pipefail

cd "$(dirname "$0")"

CXX="${CXX:-g++}"

"${CXX}" -std=c++17 -O2 \
    -DNATIVE_BUILD=1 \
    -DLIGHTWAVEOS_V2=1 \
    -DFEATURE_AUDIO_SYNC=1 \
    -DFEATURE_AUDIO_BACKEND_ESV11=1 \
    -DFEATURE_AUDIO_HF_SEMANTICS=0 \
    -DFEATURE_AUDIO_BACKEND_PIPELINECORE=0 \
    -I../../src \
    -I../test_native/mocks \
    -I../test_native \
    -include native_stubs.h \
    golden_runner.cpp \
    ../../src/effects/ieffect/BeatPulseShockwaveEffect.cpp \
    ../../src/effects/ieffect/BeatPulseStackEffect.cpp \
    ../../src/effects/ieffect/BeatPulseShockwaveCascadeEffect.cpp \
    -o golden_runner

echo "Built: $(pwd)/golden_runner"
