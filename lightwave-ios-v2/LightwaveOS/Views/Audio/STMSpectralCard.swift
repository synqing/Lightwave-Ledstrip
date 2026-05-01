//
//  STMSpectralCard.swift
//  LightwaveOS
//
//  Phase 2 — Task P2-2: STM (Spectral-Temporal Modulation) visualisation card.
//
//  Renders the latest STM frame (42 spectral bins + 16 temporal bands) as a
//  Canvas-based line chart. Refreshes whenever `AudioViewModel.stmLatest`
//  changes. The firmware broadcasts the underlying frames at ~30 FPS once
//  the iOS app has called `WebSocketService.subscribeSTM()`.
//
//  British English in all UI strings.
//

import SwiftUI

// MARK: - STMSpectralCard

struct STMSpectralCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        LWCard(title: "STM SPECTRAL") {
            VStack(alignment: .leading, spacing: Spacing.sm) {
                if let frame = app.audio.stmLatest, frame.ready {
                    spectralChart(for: frame)
                        .frame(height: 96)
                        .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))

                    metricsRow(for: frame)
                } else {
                    waitingPlaceholder
                        .frame(height: 96)
                        .frame(maxWidth: .infinity)
                }

                Text(footerLabel)
                    .font(.monospaceSmall)
                    .foregroundStyle(Color.lwTextTertiary)
            }
        }
    }

    // MARK: Spectral chart

    private func spectralChart(for frame: STMFrame) -> some View {
        Canvas { context, size in
            let bins = frame.spectral
            guard !bins.isEmpty else { return }

            // Background panel.
            context.fill(
                Path(CGRect(origin: .zero, size: size)),
                with: .color(Color.lwBase.opacity(0.6))
            )

            let maxValue = max(bins.max() ?? 1, 0.0001)
            let stepX = size.width / CGFloat(bins.count - 1)

            // Build polyline path.
            var path = Path()
            for (i, value) in bins.enumerated() {
                let xCoord = CGFloat(i) * stepX
                let normalised = CGFloat(value / maxValue)
                let yCoord = size.height - (normalised * size.height)
                if i == 0 {
                    path.move(to: CGPoint(x: xCoord, y: yCoord))
                } else {
                    path.addLine(to: CGPoint(x: xCoord, y: yCoord))
                }
            }

            // Stroke the line.
            context.stroke(path, with: .color(Color.lwGold), lineWidth: 1.5)

            // Highlight the dominant bin.
            let dominantIndex = max(0, min(bins.count - 1, Int(frame.dominantBin.rounded())))
            let dotX = CGFloat(dominantIndex) * stepX
            let dotY = size.height - CGFloat(bins[dominantIndex] / maxValue) * size.height
            let dotRect = CGRect(x: dotX - 3, y: dotY - 3, width: 6, height: 6)
            context.fill(Path(ellipseIn: dotRect), with: .color(Color.lwBeatAccent))
        }
    }

    // MARK: Metrics row

    private func metricsRow(for frame: STMFrame) -> some View {
        HStack(spacing: Spacing.md) {
            metricCell("SPECTRAL ENERGY", value: String(format: "%.2f", frame.spectralEnergy))
            metricCell("TEMPORAL ENERGY", value: String(format: "%.2f", frame.temporalEnergy))
            metricCell("CENTROID", value: String(format: "%.1f", frame.spectralCentroid))
            metricCell("DOMINANT", value: String(format: "%.0f", frame.dominantBin))
        }
    }

    private func metricCell(_ title: String, value: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title)
                .font(.microLabel)
                .foregroundStyle(Color.lwTextTertiary)
                .tracking(0.6)
            Text(value)
                .font(.metricValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    // MARK: Placeholder + footer

    private var waitingPlaceholder: some View {
        ZStack {
            RoundedRectangle(cornerRadius: CornerRadius.nested)
                .stroke(Color.lwTextTertiary.opacity(0.3), lineWidth: 1)
            Text("Awaiting STM stream…")
                .font(.cardLabel)
                .foregroundStyle(Color.lwTextTertiary)
        }
    }

    private var footerLabel: String {
        let count = app.audio.stmFrameCount
        if count == 0 { return "Subscribe via WS to receive STM frames" }
        return "Frames: \(count)"
    }
}

// MARK: - Preview

#Preview("STM Spectral Card") {
    let vm = AppViewModel()
    vm.audio.handleSTMFrame(STMFrame.mock)
    return ScrollView {
        STMSpectralCard()
            .padding(Spacing.lg)
    }
    .background(Color.lwBase)
    .environment(vm)
    .preferredColorScheme(.dark)
}
