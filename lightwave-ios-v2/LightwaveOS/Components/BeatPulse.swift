//
//  BeatPulse.swift
//  LightwaveOS
//
//  Animated pulsing circle that reacts to beat events.
//  Enlarged to 12pt diameter for v2.
//

import SwiftUI

struct BeatPulse: View {
    var isBeating: Bool
    var isDownbeat: Bool
    var confidence: Double

    @State private var pulseScale: CGFloat = 1.0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    private var opacity: Double {
        0.4 + (confidence * 0.6)
    }

    private var targetScale: CGFloat {
        if isBeating {
            return isDownbeat ? 1.8 : 1.4
        }
        return 1.0
    }

    private var shadowRadius: CGFloat {
        isDownbeat ? 8 : 0
    }

    var body: some View {
        RiveViewContainer(
            asset: RiveAssetRegistry.beatPulse,
            inputs: [
                .bool("isBeating", isBeating),
                .bool("isDownbeat", isDownbeat),
                .number("confidence", confidence)
            ],
            fallback: AnyView(nativePulse)
        )
        .frame(width: 16, height: 16)
    }

    private var nativePulse: some View {
        Circle()
            .fill(Color.lwBeatAccent)
            .frame(width: 12, height: 12)
            .scaleEffect(reduceMotion ? 1.0 : pulseScale)
            .opacity(opacity)
            .shadow(
                color: isDownbeat && !reduceMotion ? Color.lwBeatAccent.opacity(0.6) : Color.clear,
                radius: shadowRadius,
                x: 0,
                y: 0
            )
            .onChange(of: isBeating) { _, newValue in
                guard !reduceMotion else { return }

                if newValue {
                    // Pulse up
                    withAnimation(.spring(duration: 0.1)) {
                        pulseScale = targetScale
                    }
                    // Decay back down
                    Task { @MainActor in
                        try? await Task.sleep(for: .milliseconds(100))
                        withAnimation(.spring(duration: 0.2)) {
                            pulseScale = 1.0
                        }
                    }
                }
            }
    }
}

// MARK: - Preview

#Preview("Beat Pulse States") {
    struct PreviewWrapper: View {
        @State private var isBeating = false
        @State private var isDownbeat = false
        @State private var confidence: Double = 0.8

        var body: some View {
            ScrollView {
                VStack(spacing: Spacing.lg) {
                    LWCard(title: "Regular Beat") {
                        VStack(spacing: Spacing.md) {
                            BeatPulse(
                                isBeating: isBeating,
                                isDownbeat: false,
                                confidence: confidence
                            )

                            LWButton(title: "Trigger Beat", style: .secondary) {
                                isBeating = true
                                Task { @MainActor in
                                    try? await Task.sleep(for: .milliseconds(300))
                                    isBeating = false
                                }
                            }
                        }
                    }

                    LWCard(title: "Downbeat (stronger)") {
                        VStack(spacing: Spacing.md) {
                            BeatPulse(
                                isBeating: isDownbeat,
                                isDownbeat: true,
                                confidence: confidence
                            )

                            LWButton(title: "Trigger Downbeat", style: .secondary) {
                                isDownbeat = true
                                Task { @MainActor in
                                    try? await Task.sleep(for: .milliseconds(300))
                                    isDownbeat = false
                                }
                            }
                        }
                    }

                    LWCard(title: "Confidence Adjustment") {
                        VStack(spacing: Spacing.md) {
                            HStack {
                                Text("Confidence")
                                    .font(.cardLabel)
                                    .foregroundStyle(Color.lwTextSecondary)

                                Spacer()

                                Text("\(Int(confidence * 100))%")
                                    .font(.cardLabel)
                                    .foregroundStyle(Color.lwGold)
                            }

                            Slider(value: $confidence, in: 0...1)
                                .tint(Color.lwGold)
                        }
                    }

                    LWCard(title: "Simulated Beat Pattern") {
                        VStack(spacing: Spacing.md) {
                            HStack(spacing: Spacing.md) {
                                BeatPulse(isBeating: true, isDownbeat: false, confidence: 0.9)
                                BeatPulse(isBeating: false, isDownbeat: false, confidence: 0.5)
                                BeatPulse(isBeating: false, isDownbeat: false, confidence: 0.3)
                                BeatPulse(isBeating: false, isDownbeat: false, confidence: 0.8)
                            }

                            Text("Low confidence (30%) vs high (90%)")
                                .font(.caption)
                                .foregroundStyle(Color.lwTextTertiary)
                        }
                    }

                    LWButton(
                        title: "Start Auto Beat (120 BPM)",
                        icon: "metronome",
                        style: .primary
                    ) {
                        startBeatSimulation()
                    }
                }
                .padding(Spacing.lg)
            }
            .background(Color.lwBase)
        }

        private func startBeatSimulation() {
            Task { @MainActor in
                for _ in 0..<4 {
                    isBeating = true
                    try? await Task.sleep(nanoseconds: 300_000_000)
                    isBeating = false
                    try? await Task.sleep(nanoseconds: 200_000_000)
                }
            }
        }
    }

    return PreviewWrapper()
}
