//
//  SpectrumVisualisationCard.swift
//  LightwaveOS
//
//  Reusable card wrapper for audio spectrum visualisations (Waterfall, Dot Ridge, Dot Matrix).
//  Provides title, palette indicator strip, and tap-to-cycle palette behaviour.
//

import SwiftUI

// MARK: - SpectrumVisualisationCard

struct SpectrumVisualisationCard<Content: View>: View {
    let title: String
    @ViewBuilder let content: (PaletteLUT) -> Content

    @State private var paletteIndex: Int = 0

    private var currentPalette: PaletteLUT {
        PaletteLUT.all[paletteIndex % PaletteLUT.all.count]
    }

    var body: some View {
        LWCard(title: title) {
            VStack(spacing: 0) {
                // Canvas content
                content(currentPalette)
                    .frame(height: 120)
                    .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))

                // Palette strip (gradient + name label)
                paletteStrip
                    .padding(.top, 8)
            }
            .contentShape(Rectangle())
            .onTapGesture {
                // Haptic feedback on palette cycle
                let impact = UIImpactFeedbackGenerator(style: .light)
                impact.impactOccurred()

                withAnimation(.easeInOut(duration: 0.2)) {
                    paletteIndex = (paletteIndex + 1) % PaletteLUT.all.count
                }
            }
        }
    }

    // MARK: - Palette Strip

    private var paletteStrip: some View {
        VStack(spacing: 4) {
            // Gradient strip showing palette colours
            HStack(spacing: 0) {
                ForEach(0..<32, id: \.self) { i in
                    let t = Float(i) / 31.0
                    Rectangle()
                        .fill(currentPalette.color(at: t))
                }
            }
            .frame(height: 3)
            .clipShape(Capsule())

            // Palette name label
            Text(currentPalette.name)
                .font(.system(size: 10, weight: .medium, design: .monospaced))
                .foregroundStyle(Color.lwTextSecondary)
        }
    }
}

// MARK: - Preview

#Preview("Spectrum Visualisation Card") {
    ScrollView {
        VStack(spacing: Spacing.md) {
            // Mock waterfall visualisation
            SpectrumVisualisationCard(title: "WATERFALL") { palette in
                LinearGradient(
                    colors: (0..<8).map { palette.color(at: Float($0) / 7.0) },
                    startPoint: .leading,
                    endPoint: .trailing
                )
            }

            // Mock dot ridge visualisation
            SpectrumVisualisationCard(title: "DOT RIDGE") { palette in
                ZStack {
                    Color.lwBase

                    // Simulated frequency bars
                    HStack(spacing: 2) {
                        ForEach(0..<20, id: \.self) { i in
                            let height = CGFloat.random(in: 20...160)
                            let t = Float(height / 160.0)
                            RoundedRectangle(cornerRadius: 2)
                                .fill(palette.color(at: t))
                                .frame(height: height)
                        }
                    }
                }
            }

            // Mock dot matrix visualisation
            SpectrumVisualisationCard(title: "DOT MATRIX") { palette in
                ZStack {
                    Color.lwBase

                    // Simulated dot grid
                    LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 4), count: 16), spacing: 4) {
                        ForEach(0..<64, id: \.self) { i in
                            let t = Float.random(in: 0...1)
                            Circle()
                                .fill(palette.color(at: t))
                                .frame(width: 8, height: 8)
                        }
                    }
                    .padding(8)
                }
            }
        }
        .padding(Spacing.lg)
    }
    .background(Color.lwBase)
}
