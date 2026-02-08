//
//  EnergyPulse.swift
//  LightwaveOS
//
//  Beat-reactive energy pulse animation primitive.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Energy Pulse Modifier

/// Applies beat-reactive scale and glow animations
struct EnergyPulseModifier: ViewModifier {
    let isActive: Bool
    let isDownbeat: Bool
    let colour: Color

    @State private var pulseScale: CGFloat = 1.0
    @State private var glowOpacity: Double = 0.0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    func body(content: Content) -> some View {
        content
            .scaleEffect(reduceMotion ? 1.0 : pulseScale)
            .compositingGroup()
            .shadow(
                color: colour.opacity(reduceMotion ? 0 : glowOpacity),
                radius: isDownbeat ? 24 : 16,
                x: 0,
                y: 0
            )
            .onChange(of: isActive) { _, active in
                guard !reduceMotion, active else { return }

                // Constrained scale: max 1.35 for downbeat, 1.15 for regular
                let targetScale: CGFloat = isDownbeat
                    ? GlassTokens.beatBurstMax
                    : GlassTokens.beatBurstRegular
                let targetGlow: Double = isDownbeat ? 0.6 : 0.4

                // Quick attack
                withAnimation(.spring(
                    response: AnimationTokens.beatSpringResponse,
                    dampingFraction: AnimationTokens.beatSpringDamping
                )) {
                    pulseScale = targetScale
                    glowOpacity = targetGlow
                }

                // Decay to rest
                withAnimation(.spring(
                    response: AnimationTokens.springResponse,
                    dampingFraction: AnimationTokens.springDamping
                ).delay(AnimationTokens.energyPulse)) {
                    pulseScale = 1.0
                    glowOpacity = 0.0
                }
            }
    }
}

// MARK: - View Extension

extension View {
    /// Applies beat-reactive energy pulse animation
    /// - Parameters:
    ///   - isActive: Triggers pulse when becoming true
    ///   - isDownbeat: If true, uses stronger pulse (max 1.35x scale)
    ///   - colour: Glow colour (default: lwGold)
    func energyPulse(
        isActive: Bool,
        isDownbeat: Bool = false,
        colour: Color = .lwGold
    ) -> some View {
        modifier(EnergyPulseModifier(
            isActive: isActive,
            isDownbeat: isDownbeat,
            colour: colour
        ))
    }
}

// MARK: - Preview

#Preview("Energy Pulse") {
    struct PulseDemo: View {
        @State private var isBeating = false
        @State private var isDownbeat = false

        var body: some View {
            VStack(spacing: 48) {
                // Demo circle
                Circle()
                    .fill(Color.lwGold)
                    .frame(width: 80, height: 80)
                    .energyPulse(isActive: isBeating, isDownbeat: isDownbeat)

                // Controls
                VStack(spacing: 16) {
                    Button("Regular Beat") {
                        isDownbeat = false
                        isBeating = true
                        // Reset after animation
                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                            isBeating = false
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(Color.lwGold)

                    Button("Downbeat") {
                        isDownbeat = true
                        isBeating = true
                        // Reset after animation
                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                            isBeating = false
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(Color.lwBeatAccent)
                }
            }
            .padding(48)
            .background(Color.lwBase)
        }
    }

    return PulseDemo()
}
