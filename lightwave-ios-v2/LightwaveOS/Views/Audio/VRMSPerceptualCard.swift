//
//  VRMSPerceptualCard.swift
//  LightwaveOS
//
//  Phase 2 — Task P2-2: VRMS (Visual RMS) perceptual-metrics card.
//
//  Reflects the latest `vrms.frame` JSON broadcast from the firmware
//  (10 Hz cadence). Eight perceptual metrics, surfaced as compact bar
//  readouts so a glance reveals dominant hue, symmetry and audio-visual
//  correlation health.
//
//  British English in all UI strings.
//

import SwiftUI

// MARK: - VRMSPerceptualCard

struct VRMSPerceptualCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        LWCard(title: "VRMS PERCEPTUAL") {
            VStack(alignment: .leading, spacing: Spacing.sm) {
                if let frame = app.audio.vrmsLatest {
                    contentRows(for: frame)
                } else {
                    waitingPlaceholder
                }

                Text(footerLabel)
                    .font(.monospaceSmall)
                    .foregroundStyle(Color.lwTextTertiary)
            }
        }
    }

    // MARK: Content

    private func contentRows(for frame: VRMSFrame) -> some View {
        VStack(spacing: Spacing.sm) {
            MetricBar(
                label: "DOMINANT HUE",
                value: String(format: "%.0f°", hueDegrees(from: frame.dominantHue)),
                fill: Double(frame.dominantHue / 256.0),
                fillColor: hueColour(from: frame.dominantHue)
            )

            MetricBar(
                label: "COLOUR VARIANCE",
                value: String(format: "%.3f", frame.colourVariance),
                fill: Double(frame.colourVariance),
                fillColor: .lwCyan
            )

            MetricBar(
                label: "SYMMETRY",
                value: String(format: "%+.3f", frame.symmetryScore),
                fill: Double((frame.symmetryScore + 1) / 2),
                fillColor: .lwSuccess
            )

            MetricBar(
                label: "BRIGHTNESS",
                value: String(format: "%.0f", frame.brightnessMean),
                fill: Double(frame.brightnessMean / 255.0),
                fillColor: .lwGold
            )

            MetricBar(
                label: "TEMPORAL FREQ",
                value: String(format: "%.3f", frame.temporalFreq),
                fill: Double(frame.temporalFreq),
                fillColor: .lwBeatAccent
            )

            MetricBar(
                label: "AV CORRELATION",
                value: String(format: "%+.3f", frame.audioVisualCorr),
                fill: Double((frame.audioVisualCorr + 1) / 2),
                fillColor: .lwCyan
            )
        }
    }

    private var waitingPlaceholder: some View {
        ZStack {
            RoundedRectangle(cornerRadius: CornerRadius.nested)
                .stroke(Color.lwTextTertiary.opacity(0.3), lineWidth: 1)
            Text("Awaiting VRMS stream…")
                .font(.cardLabel)
                .foregroundStyle(Color.lwTextTertiary)
        }
        .frame(height: 96)
        .frame(maxWidth: .infinity)
    }

    // MARK: Footer

    private var footerLabel: String {
        let count = app.audio.vrmsFrameCount
        if count == 0 { return "Subscribe via WS to receive VRMS frames" }
        return "Frames: \(count) · ts \(app.audio.lastVRMSTimestampMs) ms"
    }

    // MARK: Helpers

    /// Convert the firmware's HSV hue (range 0..<256) into degrees (0..<360).
    private func hueDegrees(from hue: Float) -> Float {
        (hue / 256.0) * 360.0
    }

    /// Map the dominant hue back to an indicative SwiftUI Color so the bar
    /// fill reflects the actual hue value rather than a fixed accent.
    private func hueColour(from hue: Float) -> Color {
        Color(hue: Double(hue / 256.0), saturation: 0.85, brightness: 0.9)
    }
}

// MARK: - Preview

#Preview("VRMS Perceptual Card") {
    let vm = AppViewModel()
    vm.audio.handleVRMSFrame(VRMSFrame.mock, timestamp: 4242)
    return ScrollView {
        VRMSPerceptualCard()
            .padding(Spacing.lg)
    }
    .background(Color.lwBase)
    .environment(vm)
    .preferredColorScheme(.dark)
}
