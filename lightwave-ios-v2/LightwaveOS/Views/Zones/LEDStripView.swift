//
//  LEDStripView.swift
//  LightwaveOS
//
//  60pt tall LED strip visualization with zone-coloured segments.
//  Features 6-layer "Liquid Glass" render stack for premium depth and texture.
//

import SwiftUI

struct LEDStripView: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        VStack(spacing: Spacing.sm) {
            // Header
            Text("LED STRIP VISUALISATION")
                .font(.pillLabel)
                .foregroundStyle(Color.lwTextTertiary)
                .tracking(0.8)
                .frame(maxWidth: .infinity)

            // Metadata row: Left/Right labels
            HStack {
                Text("Left (0-79)")
                    .font(.monospaceSmall)
                    .foregroundStyle(Color.lwTextTertiary)

                Spacer()

                Text("Right (80-159)")
                    .font(.monospaceSmall)
                    .foregroundStyle(Color.lwTextTertiary)
            }

            // LED strip with 6-layer "Liquid Glass" render stack
            ledStripWithGlassLayers
                .frame(height: 28)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
                .overlay(
                    GeometryReader { geometry in
                        Color.clear
                            .contentShape(Rectangle())
                            .frame(width: 44, height: geometry.size.height)
                            .position(x: geometry.size.width / 2, y: geometry.size.height / 2)
                            .onTapGesture {
                                appVM.zones.resetSplitsToEven()
                            }
                    }
                )

            // Centre label
            Text("Centre pair: LEDs 79 (left) / 80 (right) • Tap centre to reset splits")
                .font(.monospaceSmall)
                .foregroundStyle(Color.lwTextTertiary)
                .frame(maxWidth: .infinity)
                .multilineTextAlignment(.center)
        }
    }

    // MARK: - 6-Layer Liquid Glass Stack

    /// 6-layer "Liquid Glass" render stack for premium LED strip visualization
    /// Layers (bottom to top):
    /// 1. GlassCard wrapper (compact variant for nested use)
    /// 2. LED core (existing Canvas rendering)
    /// 3. Inter-LED bleed (2px horizontal blur for smoother look)
    /// 4. Ambient glow (radial gradient from centre, lwGold at 0.1 opacity)
    /// 5. Reflection (top gradient white→clear at 0.08 opacity, 20% height)
    /// 6. Beat pulse overlay (scale 1.02 on beat)
    private var ledStripWithGlassLayers: some View {
        ZStack {
            // Layer 2: LED core (existing Canvas rendering)
            Canvas { context, size in
                drawLEDStrip(context: context, size: size)
            }
            // Layer 3: Inter-LED bleed effect (2px horizontal blur)
            .blur(radius: 1.0)

            // Layer 4: Ambient glow (radial gradient from centre)
            RadialGradient(
                colors: [
                    Color.lwGold.opacity(0.1),
                    Color.clear
                ],
                center: .center,
                startRadius: 0,
                endRadius: 100
            )
            .allowsHitTesting(false)

            // Layer 5: Reflection (top gradient white→clear, 20% height)
            LinearGradient(
                colors: [
                    Color.white.opacity(0.08),
                    Color.clear
                ],
                startPoint: .top,
                endPoint: UnitPoint(x: 0.5, y: 0.2)
            )
            .allowsHitTesting(false)
        }
        // Layer 1: GlassCard wrapper (compact variant, minimal layers for nested use)
        .glassSurface(style: .card, cornerRadius: CornerRadius.nested)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.nested)
                .fill(Color.lwCardGradient)
                .opacity(0.6)
        )
        .innerShadow(style: .subtle, cornerRadius: CornerRadius.nested)
        .gradientStroke(style: .subtle, cornerRadius: CornerRadius.nested)
        // Layer 6: Beat pulse overlay (scale 1.02 on beat)
        .energyPulse(
            isActive: appVM.audio.isBeating,
            isDownbeat: false,
            colour: .lwGold
        )
    }

    // MARK: - Canvas Drawing

    /// Draw the LED strip with centre-origin mirrored layout and gold centre divider
    private func drawLEDStrip(context: GraphicsContext, size: CGSize) {
        let stripRect = CGRect(origin: .zero, size: size)

        // Draw background strip
        context.fill(
            Path(roundedRect: stripRect, cornerRadius: CornerRadius.nested),
            with: .color(.lwElevated)
        )

        // Draw zone segments if zones are enabled (mirrored layout)
        if appVM.zones.zonesEnabled && !appVM.zones.segments.isEmpty {
            // Sort segments by zone ID to ensure inner zones are drawn first
            let sortedSegments = appVM.zones.segments.sorted { $0.zoneId < $1.zoneId }

            for segment in sortedSegments {
                drawZoneSegmentMirrored(context: context, segment: segment, canvasWidth: size.width, canvasHeight: size.height)
            }
        }

        // Draw centre divider (gold vertical line at centre, 2px, 0.5 opacity)
        let centreX = size.width / 2
        var centrePath = Path()
        centrePath.move(to: CGPoint(x: centreX, y: 0))
        centrePath.addLine(to: CGPoint(x: centreX, y: size.height))
        context.stroke(centrePath, with: .color(.lwGold.opacity(0.5)), lineWidth: 2)
    }

    /// Draw zone segment with centre-origin mirrored layout
    /// LEFT half: Z2 outer (edge) → Z1 inner (centre)
    /// RIGHT half: Z1 inner (centre) → Z2 outer (edge)
    private func drawZoneSegmentMirrored(context: GraphicsContext, segment: ZoneSegment, canvasWidth: CGFloat, canvasHeight: CGFloat) {
        let zoneColour = Color.zoneColor(segment.zoneId).opacity(0.25)
        let centreX = canvasWidth / 2

        // LEFT half (0-79): segments are mirrored so inner zones appear at centre
        if segment.s1LeftStart <= segment.s1LeftEnd {
            // Map LED indices to visual position (0-79 maps to 0-50% of width)
            let leftStartLED = Double(segment.s1LeftStart)
            let leftEndLED = Double(segment.s1LeftEnd)

            // Convert LED index to X position (0-79 → 0 to centreX)
            let leftStartX = (leftStartLED / 79.0) * centreX
            let leftEndX = ((leftEndLED + 1) / 79.0) * centreX

            let leftRect = CGRect(x: leftStartX, y: 0, width: leftEndX - leftStartX, height: canvasHeight)
            context.fill(Path(leftRect), with: .color(zoneColour))
        }

        // RIGHT half (80-159): normal layout, inner zones at centre
        if segment.s1RightStart <= segment.s1RightEnd {
            // Map LED indices to visual position (80-159 maps to 50%-100% of width)
            let rightStartLED = Double(segment.s1RightStart - 80)
            let rightEndLED = Double(segment.s1RightEnd - 80)

            // Convert LED index to X position (80-159 → centreX to width)
            let rightStartX = centreX + (rightStartLED / 79.0) * centreX
            let rightEndX = centreX + ((rightEndLED + 1) / 79.0) * centreX

            let rightRect = CGRect(x: rightStartX, y: 0, width: rightEndX - rightStartX, height: canvasHeight)
            context.fill(Path(rightRect), with: .color(zoneColour))
        }
    }

}

// MARK: - Preview

#Preview("LED Strip View - Dual Zones") {
    let appVM = {
        let vm = AppViewModel()
        vm.zones.zonesEnabled = true
        vm.zones.zoneCount = 2
        vm.zones.segments = [
            ZoneSegment(zoneId: 0, s1LeftStart: 0, s1LeftEnd: 39, s1RightStart: 80, s1RightEnd: 119),
            ZoneSegment(zoneId: 1, s1LeftStart: 40, s1LeftEnd: 79, s1RightStart: 120, s1RightEnd: 159)
        ]
        return vm
    }()

    LEDStripView()
        .environment(appVM)
        .padding(Spacing.lg)
        .background(Color.lwBase)
}

#Preview("LED Strip View - Centre Zone") {
    let appVM = {
        let vm = AppViewModel()
        vm.zones.zonesEnabled = true
        vm.zones.zoneCount = 1
        vm.zones.segments = [
            ZoneSegment(zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94)
        ]
        return vm
    }()

    LEDStripView()
        .environment(appVM)
        .padding(Spacing.lg)
        .background(Color.lwBase)
}

#Preview("LED Strip View - Disabled") {
    let appVM = {
        let vm = AppViewModel()
        vm.zones.zonesEnabled = false
        return vm
    }()

    LEDStripView()
        .environment(appVM)
        .padding(Spacing.lg)
        .background(Color.lwBase)
}

#Preview("LED Strip View - Beat Pulse Demo") {
    struct BeatPulseDemo: View {
        @State private var isBeating = false

        var body: some View {
            let appVM = {
                let vm = AppViewModel()
                vm.zones.zonesEnabled = true
                vm.zones.zoneCount = 2
                vm.zones.segments = [
                    ZoneSegment(zoneId: 0, s1LeftStart: 0, s1LeftEnd: 39, s1RightStart: 80, s1RightEnd: 119),
                    ZoneSegment(zoneId: 1, s1LeftStart: 40, s1LeftEnd: 79, s1RightStart: 120, s1RightEnd: 159)
                ]
                vm.audio.isBeating = isBeating
                return vm
            }()

            VStack(spacing: Spacing.lg) {
                LEDStripView()
                    .environment(appVM)

                Button("Trigger Beat Pulse") {
                    isBeating = true
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                        isBeating = false
                    }
                }
                .buttonStyle(.borderedProminent)
                .tint(Color.lwGold)
            }
            .padding(Spacing.lg)
            .background(Color.lwBase)
        }
    }

    return BeatPulseDemo()
}
