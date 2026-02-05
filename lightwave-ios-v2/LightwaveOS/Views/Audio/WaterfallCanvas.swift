//
//  WaterfallCanvas.swift
//  LightwaveOS
//
//  SwiftUI Canvas-based spectrogram waterfall visualisation.
//  Renders an 8-band × 60-history grid using PaletteLUT colour mapping.
//

import SwiftUI

// MARK: - WaterfallCanvas

/// Waterfall spectrogram visualisation for 8-band audio spectrum data
///
/// Displays a scrolling waterfall where:
/// - 8 rows represent frequency bands (Sub to Air, bottom to top)
/// - 60 columns represent history frames (newest right, oldest left)
/// - Colours are mapped via PaletteLUT based on band intensity
/// - Slight age fade is applied to older frames
struct WaterfallCanvas: View {
    @Environment(AudioViewModel.self) private var audio

    let palette: PaletteLUT

    // MARK: - Constants

    private let bands = 8
    private let history = 60
    private let ageFadeAmount: Float = 0.15

    // MARK: - Body

    var body: some View {
        // 15 Hz matches firmware audio frame rate — saves battery vs 60 Hz .animation
        TimelineView(.periodic(from: .now, by: 1.0 / 15.0)) { timeline in
            Canvas { ctx, size in
                // Fill background with lwBase
                ctx.fill(
                    Path(CGRect(origin: .zero, size: size)),
                    with: .color(Color.lwBase)
                )

                // Calculate cell dimensions
                let colWidth = size.width / CGFloat(history)
                let rowHeight = size.height / CGFloat(bands)

                // Draw waterfall grid
                for col in 0..<history {
                    // Calculate ring buffer index (newest = writeRow - 1, oldest = writeRow - 60)
                    let idx = (audio.writeRow - 1 - col + history) % history

                    // Age fade: newer frames are brighter
                    let age = Float(col) / Float(history)
                    let ageFactor = 1.0 - (age * ageFadeAmount)

                    // Access ring buffer row (guard against empty data)
                    guard idx >= 0 && idx < audio.ringBuffer.count else { continue }
                    let row = audio.ringBuffer[idx]

                    // Render each band in this column
                    for band in 0..<bands {
                        guard band < row.count else { continue }

                        // Get band value and apply age fade
                        let val = row[band] * ageFactor

                        // Map to palette colour
                        let colour = palette.color(at: val)

                        // Calculate cell position
                        // Band 0 (Sub) at bottom, Band 7 (Air) at top
                        let y = CGFloat(bands - 1 - band) * rowHeight

                        // Newest frame on right, oldest on left
                        let x = size.width - CGFloat(col + 1) * colWidth

                        // Draw cell with slight overlap to prevent gaps
                        let rect = CGRect(
                            x: x,
                            y: y,
                            width: colWidth + 0.5,
                            height: rowHeight + 0.5
                        )

                        ctx.fill(Path(rect), with: .color(colour))
                    }
                }
            }
        }
    }
}

// MARK: - Preview

#Preview("Waterfall Canvas") {
    let audio = AudioViewModel()

    // Pre-populate ringBuffer with mock sine wave data
    // Create 60 frames of synthetic spectrum data
    for i in 0..<60 {
        var frame: [Float] = []
        for b in 0..<8 {
            // Generate varied patterns using sine waves
            // Different frequencies per band for visual variety
            let value = sin(Float(i) * 0.15 + Float(b) * 0.7) * 0.5 + 0.5
            frame.append(value)
        }
        audio.ringBuffer[i] = frame
    }
    audio.writeRow = 0

    return VStack(spacing: Spacing.md) {
        // Viridis palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Viridis")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            WaterfallCanvas(palette: .viridis)
                .frame(height: 200)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Plasma palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Plasma")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            WaterfallCanvas(palette: .plasma)
                .frame(height: 200)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Sunset Real palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Sunset Real")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            WaterfallCanvas(palette: .sunsetReal)
                .frame(height: 200)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Berlin palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Berlin")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            WaterfallCanvas(palette: .berlin)
                .frame(height: 200)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .environment(audio)
}
