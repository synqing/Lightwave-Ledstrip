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
    private let heroHeight: CGFloat = 140

    var body: some View {
        ZStack {
            // LED Canvas — Two stacked 160-LED rows (total 320), centre-origin per strip
            Canvas { context, size in
                guard appVM.ledData.count >= 960 else { return }

                let ledWidth = size.width / 160.0
                let rowHeight = size.height / 2.0
                for row in 0..<2 {
                    let baseIndex = row * 160
                    let yOffset = CGFloat(row) * rowHeight

                    // Render row LEDs
                    for i in 0..<160 {
                        let idx = (baseIndex + i) * 3
                        let r = appVM.ledData[idx]
                        let g = appVM.ledData[idx + 1]
                        let b = appVM.ledData[idx + 2]
                        let color = Color(
                            red: Double(r) / 255.0,
                            green: Double(g) / 255.0,
                            blue: Double(b) / 255.0
                        )

                        let rect = CGRect(
                            x: CGFloat(i) * ledWidth,
                            y: yOffset,
                            width: ledWidth + 0.5, // Slight overlap to prevent gaps
                            height: rowHeight
                        )
                        context.fill(Path(rect), with: .color(color))
                    }

                }
            }
            .frame(height: heroHeight)
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
            .frame(height: heroHeight)

            // BPM ring overlay (centred in card)
            heroOverlay
        }
        .frame(height: heroHeight)
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
        .gesture(
            DragGesture(minimumDistance: 24)
                .onEnded { value in
                    let horizontal = value.translation.width
                    let vertical = value.translation.height
                    guard abs(horizontal) > abs(vertical) else { return }
                    if horizontal < -30 {
                        triggerEffectChange(next: true)
                    } else if horizontal > 30 {
                        triggerEffectChange(next: false)
                    }
                }
        )
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(Text("LED preview"))
        .accessibilityValue(Text("\(appVM.effects.currentEffectName), BPM \(Int(appVM.audio.currentBpm))"))
        .accessibilityHint(Text("Swipe left or right to change effect."))
    }

    private func triggerEffectChange(next: Bool) {
        let effects = appVM.effects.allEffects
        guard !effects.isEmpty else { return }
        let currentIndex = effects.firstIndex(where: { $0.id == appVM.effects.currentEffectId }) ?? -1
        let targetIndex = next ? (currentIndex + 1) % effects.count : (currentIndex > 0 ? currentIndex - 1 : effects.count - 1)
        let target = effects[targetIndex]

        Task {
            await appVM.transition.triggerTransition(toEffect: target.id)
        }
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
                        .font(.bpmNumeral)
                        .kerning(2)
                        .foregroundStyle(Color.lwGold)
                        .shadow(color: Color.black.opacity(0.8), radius: 8, x: 0, y: 2)
                } else {
                    Text("—")
                        .font(.bpmNumeral)
                        .foregroundStyle(Color.lwGold)
                        .shadow(color: Color.black.opacity(0.8), radius: 8, x: 0, y: 2)
                }

                Text("BPM")
                    .font(.microLabel)
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

// MARK: - Rive Overlay

private extension HeroLEDPreview {
    @ViewBuilder
    var heroOverlay: some View {
        RiveViewContainer(
            asset: RiveAssetRegistry.heroOverlay,
            inputs: [
                .number("bpm", appVM.audio.currentBpm),
                .number("confidence", appVM.audio.bpmConfidence),
                .bool("isBeating", appVM.audio.isBeating)
            ],
            fallback: AnyView(
                BPMRingOverlay(
                    bpm: Int(appVM.audio.currentBpm),
                    confidence: appVM.audio.bpmConfidence,
                    isBeating: appVM.audio.isBeating
                )
            )
        )
        .frame(width: 96, height: 96)
    }
}

// MARK: - Preview

#Preview("Hero LED Preview") {
    VStack(spacing: Spacing.lg) {
        // Preview with 320 LEDs (two rows)
        HeroLEDPreview()
            .environment({
                let vm = AppViewModel()
                vm.effects.currentEffectName = "Dual Strip Test"
                vm.audio.currentBpm = 124
                vm.audio.bpmConfidence = 0.82
                vm.audio.isBeating = false
                vm.ledData = (0..<320).flatMap { i -> [UInt8] in
                    let t = Double(i) / 319.0
                    let r = UInt8(255 * t)
                    let g = UInt8(255 * (1.0 - t))
                    let b = UInt8(128 + 127 * sin(t * .pi * 2))
                    return [r, g, b]
                }
                return vm
            }())

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
