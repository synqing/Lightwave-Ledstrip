//
//  WaveformSpectrumCard.swift
//  LightwaveOS
//
//  Dual mini canvas view displaying time-domain waveform and frequency spectrum.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct WaveformSpectrumCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("WAVEFORM & SPECTRUM")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                // Waveform canvas
                VStack(alignment: .leading, spacing: 4) {
                    Text("WAVEFORM (128 SAMPLES)")
                        .font(.pillLabel)
                        .foregroundStyle(Color.lwTextTertiary)
                        .tracking(1.0)

                    WaveformCanvas(samples: app.audio.waveform)
                        .frame(height: 48)
                }

                // Spectrum canvas
                VStack(alignment: .leading, spacing: 4) {
                    Text("SPECTRUM (64 BINS)")
                        .font(.pillLabel)
                        .foregroundStyle(Color.lwTextTertiary)
                        .tracking(1.0)

                    SpectrumCanvas(bins: app.audio.bands)
                        .frame(height: 48)
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

// MARK: - Waveform Canvas

struct WaveformCanvas: View {
    let samples: [Float]

    var body: some View {
        Canvas { context, size in
            guard samples.count > 1 else { return }

            let path = Path { path in
                let stepX = size.width / CGFloat(samples.count - 1)
                let centreY = size.height / 2

                for (index, sample) in samples.enumerated() {
                    let x = CGFloat(index) * stepX
                    let y = centreY - CGFloat(sample) * centreY

                    if index == 0 {
                        path.move(to: CGPoint(x: x, y: y))
                    } else {
                        path.addLine(to: CGPoint(x: x, y: y))
                    }
                }
            }

            context.stroke(
                path,
                with: .color(.lwCyan),
                lineWidth: 1.5
            )

            // Zero line
            let zeroLine = Path { path in
                path.move(to: CGPoint(x: 0, y: size.height / 2))
                path.addLine(to: CGPoint(x: size.width, y: size.height / 2))
            }

            context.stroke(
                zeroLine,
                with: .color(.lwTextTertiary.opacity(0.3)),
                lineWidth: 1
            )
        }
        .background(Color.lwElevated)
        .clipShape(RoundedRectangle(cornerRadius: 6))
    }
}

// MARK: - Spectrum Canvas

struct SpectrumCanvas: View {
    let bins: [Float]

    var body: some View {
        Canvas { context, size in
            guard bins.count > 0 else { return }

            let barWidth = size.width / CGFloat(bins.count)

            for (index, amplitude) in bins.enumerated() {
                let x = CGFloat(index) * barWidth
                let height = CGFloat(amplitude) * size.height
                let rect = CGRect(
                    x: x,
                    y: size.height - height,
                    width: barWidth * 0.9,
                    height: height
                )

                // Bass-heavy opacity falloff
                let normalizedIndex = Double(index) / Double(bins.count - 1)
                let opacity = 1.0 - (normalizedIndex * 0.5)

                context.fill(
                    Path(roundedRect: rect, cornerRadius: 1),
                    with: .color(.lwCyan.opacity(opacity))
                )
            }
        }
        .background(Color.lwElevated)
        .clipShape(RoundedRectangle(cornerRadius: 6))
    }
}

// MARK: - Preview

#Preview("Waveform & Spectrum Card") {
    WaveformSpectrumCard()
        .environment({
            let vm = AppViewModel()
            // Mock waveform data
            vm.audio.waveform = (0..<128).map { i in
                Float(sin(Double(i) * 0.1) * 0.5)
            }
            // Mock spectrum data
            vm.audio.bands = [0.8, 0.6, 0.5, 0.4, 0.3, 0.2, 0.15, 0.1]
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
