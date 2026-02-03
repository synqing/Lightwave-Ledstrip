//
//  HeroLEDPreview.swift
//  LightwaveOS
//
//  Canvas-based 320-LED strip preview with centre marker, effect name overlay,
//  BPM ring, and beat pulse animation.
//

import SwiftUI

struct HeroLEDPreview: View {
    @Environment(AppViewModel.self) private var appVM
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    @State private var beatScale: CGFloat = 1.0

    var body: some View {
        ZStack {
            // LED Canvas — Strip 1 only (160 LEDs), centre-origin
            Canvas { context, size in
                guard appVM.ledData.count >= 480 else { return }

                let ledWidth = size.width / 160.0

                // Render Strip 1: LEDs 0-159
                for i in 0..<160 {
                    let r = appVM.ledData[i * 3]
                    let g = appVM.ledData[i * 3 + 1]
                    let b = appVM.ledData[i * 3 + 2]
                    let color = Color(
                        red: Double(r) / 255.0,
                        green: Double(g) / 255.0,
                        blue: Double(b) / 255.0
                    )

                    let rect = CGRect(
                        x: CGFloat(i) * ledWidth,
                        y: 0,
                        width: ledWidth + 0.5, // Slight overlap to prevent gaps
                        height: size.height
                    )
                    context.fill(Path(rect), with: .color(color))
                }

                // Centre marker removed per design review
            }
            .frame(height: 120)
            .background(Color.lwCardGradient)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.hero))
            .heroGlow(color: .lwGold)
            .scaleEffect(beatScale)

            // Effect name overlay (bottom-left)
            VStack {
                Spacer()
                HStack {
                    Text(appVM.effects.currentEffectName)
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                        .shadow(color: Color.black.opacity(0.8), radius: 4, x: 0, y: 2)
                        .padding([.leading, .bottom], 16)
                    Spacer()
                }
            }
            .frame(height: 120)

            // BPM ring overlay (centred in card)
            BPMRingOverlay(
                bpm: Int(appVM.audio.currentBpm),
                confidence: appVM.audio.bpmConfidence,
                isBeating: appVM.audio.isBeating
            )
        }
        .frame(height: 120)
        .onChange(of: appVM.audio.isBeating) { _, isBeating in
            guard isBeating, !reduceMotion else { return }

            // Beat pulse animation
            withAnimation(.spring(duration: 0.15, bounce: 0.3)) {
                beatScale = 1.02
            }
            withAnimation(.spring(duration: 0.2, bounce: 0.2).delay(0.15)) {
                beatScale = 1.0
            }
        }
        // Swipe gestures for effect navigation will be added when effect VM is complete
    }
}

// MARK: - BPM Ring Overlay

struct BPMRingOverlay: View {
    let bpm: Int
    let confidence: Double
    let isBeating: Bool

    var body: some View {
        ZStack {
            // Background ring (always visible)
            Circle()
                .stroke(
                    Color.lwElevated.opacity(0.5),
                    lineWidth: 4
                )
                .frame(width: 80, height: 80)

            // Foreground confidence arc
            Circle()
                .trim(from: 0, to: confidence)
                .stroke(
                    Color.lwGold,
                    style: StrokeStyle(lineWidth: 4, lineCap: .round)
                )
                .frame(width: 80, height: 80)
                .rotationEffect(.degrees(-90))

            // BPM value and label
            VStack(spacing: 0) {
                if bpm > 0 {
                    Text("\(bpm)")
                        .font(.custom("BebasNeue-Bold", size: 32))
                        .kerning(2)
                        .foregroundStyle(Color.lwGold)
                        .shadow(color: Color.black.opacity(0.8), radius: 8, x: 0, y: 2)
                } else {
                    Text("—")
                        .font(.custom("BebasNeue-Bold", size: 32))
                        .foregroundStyle(Color.lwGold)
                        .shadow(color: Color.black.opacity(0.8), radius: 8, x: 0, y: 2)
                }

                Text("BPM")
                    .font(.custom("Rajdhani-Medium", size: 10))
                    .kerning(1.5)
                    .textCase(.uppercase)
                    .foregroundStyle(Color.lwTextSecondary)
                    .shadow(color: Color.black.opacity(0.8), radius: 8, x: 0, y: 2)
            }
        }
        .scaleEffect(isBeating ? 1.03 : 1.0)
        .animation(.spring(duration: 0.15, bounce: 0.3), value: isBeating)
    }
}

// MARK: - Preview

#Preview("Hero LED Preview") {
    VStack(spacing: Spacing.lg) {
        // Preview with BPM
        HeroLEDPreview()
            .environment({
                let vm = AppViewModel()
                vm.effects.currentEffectName = "Pulse Waves"
                vm.audio.currentBpm = 128
                vm.audio.bpmConfidence = 0.75
                vm.audio.isBeating = false
                return vm
            }())

        // Preview with no BPM
        HeroLEDPreview()
            .environment({
                let vm = AppViewModel()
                vm.effects.currentEffectName = "Neural Mesh"
                vm.audio.currentBpm = 0
                vm.audio.bpmConfidence = 0.0
                return vm
            }())

        // Preview during beat pulse
        HeroLEDPreview()
            .environment({
                let vm = AppViewModel()
                vm.effects.currentEffectName = "Beat Reactive"
                vm.audio.currentBpm = 140
                vm.audio.bpmConfidence = 0.95
                vm.audio.isBeating = true
                return vm
            }())
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
}
