//
//  AudioParametersCard.swift
//  LightwaveOS
//
//  Audio parameter controls: mic type, gain, and threshold.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct AudioParametersCard: View {
    @Environment(AppViewModel.self) private var app

    private var audioVM: AudioViewModel {
        app.audio
    }

    var body: some View {
        @Bindable var audio = app.audio

        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("SPH0645 TUNING")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(alignment: .leading, spacing: Spacing.md) {
                // Gain slider
                VStack(alignment: .leading, spacing: Spacing.sm) {
                    HStack {
                        Text("GAIN")
                            .font(.cardLabel)
                            .foregroundStyle(Color.lwTextSecondary)

                        Spacer()

                        Text(String(format: "%.1f", audio.gain))
                            .font(.system(size: 13, weight: .semibold, design: .rounded))
                            .monospacedDigit()
                            .foregroundStyle(Color.lwGold)
                    }

                    AudioSlider(
                        value: $audio.gain,
                        range: 0.0...2.0,
                        step: 0.1,
                        onEnded: {
                            Task {
                                await audioVM.setAudioParams()
                            }
                        }
                    )
                }

                // Threshold slider
                VStack(alignment: .leading, spacing: Spacing.sm) {
                    HStack {
                        Text("THRESHOLD")
                            .font(.cardLabel)
                            .foregroundStyle(Color.lwTextSecondary)

                        Spacer()

                        Text(String(format: "%.2f", audio.threshold))
                            .font(.system(size: 13, weight: .semibold, design: .rounded))
                            .monospacedDigit()
                            .foregroundStyle(Color.lwGold)
                    }

                    AudioSlider(
                        value: $audio.threshold,
                        range: 0.0...1.0,
                        step: 0.05,
                        onEnded: {
                            Task {
                                await audioVM.setAudioParams()
                            }
                        }
                    )
                }
            }
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCard)
        )
        .shadow(color: .black.opacity(0.4), radius: 12, x: 0, y: 2)
    }
}

// MARK: - Audio Slider (simplified slider for floating-point values)

struct AudioSlider: View {
    @Binding var value: Double
    let range: ClosedRange<Double>
    let step: Double
    var onEnded: (() -> Void)?

    @State private var isDragging = false
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        GeometryReader { geometry in
            ZStack(alignment: .leading) {
                // Background track
                RoundedRectangle(cornerRadius: 5)
                    .fill(Color.lwCard)
                    .frame(height: 10)

                // Filled portion
                let fillWidth = CGFloat((value - range.lowerBound) / (range.upperBound - range.lowerBound)) * geometry.size.width
                RoundedRectangle(cornerRadius: 5)
                    .fill(
                        LinearGradient(
                            colors: [Color.lwTextTertiary, Color.lwGold],
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                    )
                    .frame(width: max(0, fillWidth), height: 10)

                // Thumb
                Circle()
                    .fill(Color.lwGold)
                    .frame(width: 24, height: 24)
                    .shadow(
                        color: isDragging && !reduceMotion ? Color.lwGold.opacity(0.6) : Color.clear,
                        radius: isDragging && !reduceMotion ? 8 : 0,
                        x: 0,
                        y: 0
                    )
                    .position(
                        x: fillWidth,
                        y: 5
                    )
                    .gesture(
                        DragGesture(minimumDistance: 0)
                            .onChanged { gesture in
                                if !isDragging {
                                    isDragging = true
                                }
                                updateValue(from: gesture.location.x, in: geometry.size.width)
                            }
                            .onEnded { _ in
                                withAnimation(.easeOut(duration: 0.3)) {
                                    isDragging = false
                                }
                                onEnded?()
                            }
                    )
            }
        }
        .frame(height: 24)
    }

    private func updateValue(from x: CGFloat, in width: CGFloat) {
        let percent = min(max(0, x / width), 1)
        let newValue = range.lowerBound + (range.upperBound - range.lowerBound) * Double(percent)
        let steppedValue = round(newValue / step) * step
        value = min(max(steppedValue, range.lowerBound), range.upperBound)
    }
}

#Preview("SPH0645 Tuning Card") {
    AudioParametersCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.gain = 0.8
            vm.audio.threshold = 0.3
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
