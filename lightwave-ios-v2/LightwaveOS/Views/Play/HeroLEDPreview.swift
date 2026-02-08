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

    @State private var beatIntensity: Double = 0.0
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

            // Beat pulse overlay — Centre-origin bloom (inside card, no geometry bounce)
            if beatIntensity > 0 {
                RadialGradient(
                    colors: [
                        Color.lwGold.opacity(beatIntensity * GlassTokens.bloomOpacity * 2.0),
                        Color.lwGold.opacity(beatIntensity * GlassTokens.bloomOpacity * 0.5),
                        Color.clear
                    ],
                    center: .center,
                    startRadius: 0,
                    endRadius: heroHeight * 0.7
                )
                .blendMode(.plusLighter)
                .allowsHitTesting(false)
                .frame(height: heroHeight)
            }
        }
        .frame(height: heroHeight)
        // Liquid Glass treatment (applied to container, not scaled)
        .glassSurface(style: .hero, cornerRadius: CornerRadius.hero)
        .innerShadow(cornerRadius: CornerRadius.hero)
        .innerHighlight(style: .standard, cornerRadius: CornerRadius.hero)
        .gradientStroke(style: .accent, cornerRadius: CornerRadius.hero)
        .grainOverlay(.subtle)
        .clipShape(RoundedRectangle(cornerRadius: CornerRadius.hero))
        .shadowStack(style: .hero)
        .overlay {
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
            .allowsHitTesting(false)

            // BPM ring overlay (centred in card)
            heroOverlay
        }
        .onChange(of: appVM.audio.isBeating) { _, isBeating in
            guard isBeating, !reduceMotion else {
                beatIntensity = 0.0
                return
            }

            // Beat pulse animation — Intensity modulation, not scale
            withAnimation(.spring(
                response: AnimationTokens.beatSpringResponse,
                dampingFraction: AnimationTokens.beatSpringDamping
            )) {
                beatIntensity = 1.0
            }
            withAnimation(.spring(
                response: AnimationTokens.beatSpringResponse * 1.33,
                dampingFraction: AnimationTokens.beatSpringDamping
            ).delay(AnimationTokens.beatSpringResponse)) {
                beatIntensity = 0.0
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
//
// Part of the "Liquid Glass" visual system.
// Constraints:
// - Palette: ONLY [lwGold → lwGoldSecondary → lwGold] — NO RAINBOWS
// - Burst scale: max 1.35 (downbeat), 1.15 (regular) — NOT 2.5+
// - Bloom: 120-130% of ring, 28-40px blur max
// - Sharp specular highlight for instrument-quality look

struct BPMRingOverlay: View {
    let bpm: Int
    let confidence: Double
    let isBeating: Bool
    let beatPosition: Int
    var isDownbeat: Bool = false

    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    // Constants — constrained per Liquid Glass spec
    private let ringSize: CGFloat = 80
    private let strokeWidth: CGFloat = 4
    private let bloomScale: CGFloat = 1.25  // 120-130% of ring
    private let bloomBlur: CGFloat = 32     // 28-40px range
    private let specularSize = CGSize(width: 16, height: 6)

    // Constrained burst scales — NOT 2.5+
    private var burstScale: CGFloat {
        guard isBeating, !reduceMotion else { return 1.0 }
        return isDownbeat ? GlassTokens.beatBurstMax : GlassTokens.beatBurstRegular
    }

    // Constrained palette — NO RAINBOWS
    // Only: [lwGold → lwGoldSecondary → lwGold]
    private var ringGradient: AngularGradient {
        AngularGradient(
            colors: [.lwGold, .lwGoldSecondary, .lwGold],
            center: .center
        )
    }

    var body: some View {
        ZStack {
            // Bloom layer (120-130% of ring)
            Circle()
                .fill(Color.lwGold.opacity(GlassTokens.bloomOpacity))
                .frame(width: ringSize * bloomScale, height: ringSize * bloomScale)
                .blur(radius: bloomBlur)

            // Background ring (always visible)
            Circle()
                .stroke(
                    Color.lwElevated.opacity(0.5),
                    lineWidth: strokeWidth
                )
                .frame(width: ringSize, height: ringSize)

            // Foreground confidence arc with constrained gradient
            Circle()
                .trim(from: 0, to: confidence)
                .stroke(
                    ringGradient,
                    style: StrokeStyle(lineWidth: strokeWidth, lineCap: .round)
                )
                .frame(width: ringSize, height: ringSize)
                .rotationEffect(.degrees(-90))

            // Sharp specular highlight (instrument glass, not soft flare)
            Ellipse()
                .fill(Color.lwGlassHighlight.opacity(0.2))
                .frame(width: specularSize.width, height: specularSize.height)
                .offset(x: -ringSize / 4, y: -ringSize / 3)
                .blur(radius: 2)

            // BPM value, label, and phase indicator
            VStack(spacing: 2) {
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

                // Beat phase indicator (4/4 time)
                PhaseIndicator(
                    currentPhase: beatPosition,
                    totalPhases: 4,
                    isActive: isBeating && bpm > 0
                )
                .padding(.top, 4)
            }
        }
        .compositingGroup()
        .scaleEffect(burstScale)
        .animation(
            .spring(
                response: AnimationTokens.beatSpringResponse,
                dampingFraction: AnimationTokens.beatSpringDamping
            ),
            value: isBeating
        )
    }
}

// MARK: - BPM Overlay (SwiftUI fallback; Rive overlay when scaffold available)

private extension HeroLEDPreview {
    @ViewBuilder
    var heroOverlay: some View {
        BPMRingOverlay(
            bpm: Int(appVM.audio.currentBpm),
            confidence: appVM.audio.bpmConfidence,
            isBeating: appVM.audio.isBeating,
            beatPosition: appVM.audio.beatPosition
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
