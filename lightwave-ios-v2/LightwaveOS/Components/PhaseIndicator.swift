//
//  PhaseIndicator.swift
//  LightwaveOS
//
//  Beat phase position indicator with crisp, saturated dots.
//  Part of the "Liquid Glass" visual system.
//

import SwiftUI

struct PhaseIndicator: View {
    /// Current phase position (0-based index)
    let currentPhase: Int

    /// Total number of phases (typically 4 or 8)
    let totalPhases: Int

    /// Whether beat is currently active
    let isActive: Bool

    // MARK: - Constants

    private let dotSize: CGFloat = 8
    private let dotSpacing: CGFloat = 6
    private let glowScale: CGFloat = 1.4

    // MARK: - Body

    var body: some View {
        HStack(spacing: dotSpacing) {
            ForEach(0..<totalPhases, id: \.self) { phase in
                phaseDot(for: phase)
            }
        }
    }

    // MARK: - Phase Dot

    @ViewBuilder
    private func phaseDot(for phase: Int) -> some View {
        let isCurrent = phase == currentPhase && isActive
        let isPast = phase < currentPhase && isActive
        let phaseColor = PhaseTokens.color(forPhase: phase)

        ZStack {
            // Glow ring (only for current phase)
            if isCurrent {
                Circle()
                    .fill(phaseColor.opacity(0.3))
                    .frame(width: dotSize * glowScale, height: dotSize * glowScale)
                    .blur(radius: 4)
            }

            // Base dot
            Circle()
                .fill(dotColor(isCurrent: isCurrent, isPast: isPast, phase: phase))
                .frame(width: dotSize, height: dotSize)

            // Specular highlight (current phase only)
            if isCurrent {
                Ellipse()
                    .fill(Color.white.opacity(0.3))
                    .frame(width: 3, height: 1.5)
                    .offset(y: -dotSize / 4)
            }
        }
        .animation(.spring(response: 0.2, dampingFraction: 0.7), value: currentPhase)
    }

    // MARK: - Colours

    private func dotColor(isCurrent: Bool, isPast: Bool, phase: Int) -> Color {
        let phaseColor = PhaseTokens.color(forPhase: phase)

        if isCurrent {
            // Current phase gets full semantic colour
            return phaseColor
        } else if isPast {
            // Past phases get dimmed version of their semantic colour
            return phaseColor.opacity(0.6)
        } else {
            // Future phases get inactive colour
            return PhaseTokens.inactive
        }
    }
}

// MARK: - Preview

#Preview("Phase Indicator") {
    struct PhaseDemo: View {
        @State private var currentPhase = 0
        @State private var isActive = true

        var body: some View {
            VStack(spacing: 32) {
                // 4-phase indicator
                GlassCard(title: "4 Phases") {
                    VStack(spacing: Spacing.md) {
                        PhaseIndicator(
                            currentPhase: currentPhase % 4,
                            totalPhases: 4,
                            isActive: isActive
                        )

                        Text("Phase: \((currentPhase % 4) + 1) / 4")
                            .font(.cardLabel)
                            .foregroundStyle(Color.lwTextSecondary)
                    }
                }

                // 8-phase indicator
                GlassCard(title: "8 Phases") {
                    VStack(spacing: Spacing.md) {
                        PhaseIndicator(
                            currentPhase: currentPhase % 8,
                            totalPhases: 8,
                            isActive: isActive
                        )

                        Text("Phase: \((currentPhase % 8) + 1) / 8")
                            .font(.cardLabel)
                            .foregroundStyle(Color.lwTextSecondary)
                    }
                }

                // Controls
                VStack(spacing: Spacing.sm) {
                    Button("Next Phase") {
                        currentPhase += 1
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(Color.lwGold)

                    Toggle("Active", isOn: $isActive)
                        .foregroundStyle(Color.lwTextPrimary)
                        .padding(.horizontal)
                }
            }
            .padding()
            .background(Color.lwBase)
        }
    }

    return PhaseDemo()
}

#Preview("Phase States") {
    VStack(spacing: 24) {
        ForEach(0..<4, id: \.self) { phase in
            GlassCard {
                HStack {
                    PhaseIndicator(
                        currentPhase: phase,
                        totalPhases: 4,
                        isActive: true
                    )

                    Spacer()

                    Text("Phase \(phase + 1)")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
        }

        // Inactive state
        GlassCard(title: "Inactive") {
            PhaseIndicator(
                currentPhase: 0,
                totalPhases: 4,
                isActive: false
            )
        }
    }
    .padding()
    .background(Color.lwBase)
}
