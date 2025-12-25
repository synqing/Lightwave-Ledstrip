/**
 * @file AudioTuning.h
 * @brief Runtime-tunable audio pipeline and contract parameters
 *
 * This keeps DSP tuning adjustable via API without recompiling.
 * Values are clamped to safe ranges to avoid unstable behaviour.
 */

#pragma once

#include <algorithm>
#include <cstdint>

namespace lightwaveos::audio {

struct AudioPipelineTuning {
    float dcAlpha = 0.001f;

    float agcTargetRms = 0.25f;
    float agcMinGain = 1.0f;
    float agcMaxGain = 100.0f;
    float agcAttack = 0.08f;
    float agcRelease = 0.02f;
    float agcClipReduce = 0.90f;
    float agcIdleReturnRate = 0.01f;

    float noiseFloorMin = 0.0004f;
    float noiseFloorRise = 0.0005f;
    float noiseFloorFall = 0.01f;

    float gateStartFactor = 1.5f;
    float gateRangeFactor = 1.5f;
    float gateRangeMin = 0.0005f;

    float rmsDbFloor = -65.0f;
    float rmsDbCeil = -12.0f;
    float bandDbFloor = -65.0f;
    float bandDbCeil = -12.0f;
    float chromaDbFloor = -65.0f;
    float chromaDbCeil = -12.0f;

    float fluxScale = 1.0f;

    float controlBusAlphaFast = 0.35f;
    float controlBusAlphaSlow = 0.12f;
};

struct AudioContractTuning {
    float audioStalenessMs = 100.0f;

    float bpmMin = 30.0f;
    float bpmMax = 300.0f;
    float bpmTau = 0.50f;
    float confidenceTau = 1.00f;
    float phaseCorrectionGain = 0.35f;
    float barCorrectionGain = 0.20f;

    uint8_t beatsPerBar = 4;
    uint8_t beatUnit = 4;
};

inline float clampf(float v, float lo, float hi) {
    return std::min(std::max(v, lo), hi);
}

inline AudioPipelineTuning clampAudioPipelineTuning(const AudioPipelineTuning& in) {
    AudioPipelineTuning out = in;

    out.dcAlpha = clampf(out.dcAlpha, 0.000001f, 0.1f);

    out.agcTargetRms = clampf(out.agcTargetRms, 0.01f, 1.0f);
    out.agcMinGain = clampf(out.agcMinGain, 0.1f, 50.0f);
    out.agcMaxGain = clampf(out.agcMaxGain, 1.0f, 500.0f);
    if (out.agcMaxGain < out.agcMinGain) {
        out.agcMaxGain = out.agcMinGain;
    }
    out.agcAttack = clampf(out.agcAttack, 0.0f, 1.0f);
    out.agcRelease = clampf(out.agcRelease, 0.0f, 1.0f);
    out.agcClipReduce = clampf(out.agcClipReduce, 0.1f, 1.0f);
    out.agcIdleReturnRate = clampf(out.agcIdleReturnRate, 0.0f, 1.0f);

    out.noiseFloorMin = clampf(out.noiseFloorMin, 0.0f, 0.1f);
    out.noiseFloorRise = clampf(out.noiseFloorRise, 0.0f, 1.0f);
    out.noiseFloorFall = clampf(out.noiseFloorFall, 0.0f, 1.0f);

    out.gateStartFactor = clampf(out.gateStartFactor, 0.0f, 10.0f);
    out.gateRangeFactor = clampf(out.gateRangeFactor, 0.0f, 10.0f);
    out.gateRangeMin = clampf(out.gateRangeMin, 0.0f, 0.1f);

    out.rmsDbFloor = clampf(out.rmsDbFloor, -120.0f, 0.0f);
    out.rmsDbCeil = clampf(out.rmsDbCeil, -120.0f, 0.0f);
    if (out.rmsDbCeil <= out.rmsDbFloor + 1.0f) {
        out.rmsDbCeil = out.rmsDbFloor + 1.0f;
    }

    out.bandDbFloor = clampf(out.bandDbFloor, -120.0f, 0.0f);
    out.bandDbCeil = clampf(out.bandDbCeil, -120.0f, 0.0f);
    if (out.bandDbCeil <= out.bandDbFloor + 1.0f) {
        out.bandDbCeil = out.bandDbFloor + 1.0f;
    }

    out.chromaDbFloor = clampf(out.chromaDbFloor, -120.0f, 0.0f);
    out.chromaDbCeil = clampf(out.chromaDbCeil, -120.0f, 0.0f);
    if (out.chromaDbCeil <= out.chromaDbFloor + 1.0f) {
        out.chromaDbCeil = out.chromaDbFloor + 1.0f;
    }

    out.fluxScale = clampf(out.fluxScale, 0.0f, 10.0f);

    out.controlBusAlphaFast = clampf(out.controlBusAlphaFast, 0.0f, 1.0f);
    out.controlBusAlphaSlow = clampf(out.controlBusAlphaSlow, 0.0f, 1.0f);

    return out;
}

inline AudioContractTuning clampAudioContractTuning(const AudioContractTuning& in) {
    AudioContractTuning out = in;

    out.audioStalenessMs = clampf(out.audioStalenessMs, 10.0f, 1000.0f);

    out.bpmMin = clampf(out.bpmMin, 20.0f, 200.0f);
    out.bpmMax = clampf(out.bpmMax, 60.0f, 400.0f);
    if (out.bpmMax < out.bpmMin + 1.0f) {
        out.bpmMax = out.bpmMin + 1.0f;
    }

    out.bpmTau = clampf(out.bpmTau, 0.01f, 10.0f);
    out.confidenceTau = clampf(out.confidenceTau, 0.01f, 10.0f);
    out.phaseCorrectionGain = clampf(out.phaseCorrectionGain, 0.0f, 1.0f);
    out.barCorrectionGain = clampf(out.barCorrectionGain, 0.0f, 1.0f);

    if (out.beatsPerBar == 0) out.beatsPerBar = 4;
    if (out.beatUnit == 0) out.beatUnit = 4;
    if (out.beatsPerBar > 12) out.beatsPerBar = 12;
    if (out.beatUnit > 16) out.beatUnit = 16;

    return out;
}

} // namespace lightwaveos::audio
