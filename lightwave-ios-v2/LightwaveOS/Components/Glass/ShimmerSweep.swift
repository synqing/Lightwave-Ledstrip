//
//  ShimmerSweep.swift
//  LightwaveOS
//
//  Shimmer sweep animation for loading/scanning states.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Shimmer Sweep Modifier

/// Applies an animated shimmer sweep effect for loading states
struct ShimmerSweepModifier: ViewModifier {
    let isActive: Bool
    let duration: Double

    @State private var phase: CGFloat = -1.0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    func body(content: Content) -> some View {
        if isActive && !reduceMotion {
            content
                .overlay {
                    GeometryReader { geo in
                        let shimmerGradient = LinearGradient(
                            stops: [
                                .init(color: .clear, location: 0.0),
                                .init(color: Color.white.opacity(0.10), location: 0.35),
                                .init(color: Color.white.opacity(0.20), location: 0.5),
                                .init(color: Color.white.opacity(0.10), location: 0.65),
                                .init(color: .clear, location: 1.0)
                            ],
                            startPoint: UnitPoint(x: phase - 0.5, y: 0),
                            endPoint: UnitPoint(x: phase + 0.5, y: 1)
                        )

                        Rectangle()
                            .fill(shimmerGradient)
                            .blendMode(.overlay)
                    }
                }
                .onAppear {
                    withAnimation(
                        .linear(duration: duration)
                        .repeatForever(autoreverses: false)
                    ) {
                        phase = 2.0
                    }
                }
                .onDisappear {
                    phase = -1.0
                }
        } else {
            content
        }
    }
}

// MARK: - View Extension

extension View {
    /// Applies an animated shimmer sweep for loading states
    /// - Parameters:
    ///   - isActive: Whether shimmer is currently active
    ///   - duration: Full sweep duration in seconds
    func shimmerSweep(
        isActive: Bool,
        duration: Double = AnimationTokens.shimmerDuration
    ) -> some View {
        modifier(ShimmerSweepModifier(isActive: isActive, duration: duration))
    }
}

// MARK: - Preview

#Preview("Shimmer Sweep") {
    struct ShimmerDemo: View {
        @State private var isLoading = true

        var body: some View {
            VStack(spacing: 32) {
                // Loading card
                RoundedRectangle(cornerRadius: CornerRadius.card)
                    .fill(Color.lwCard)
                    .frame(height: 120)
                    .shimmerSweep(isActive: isLoading)
                    .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))

                // Skeleton text lines
                VStack(alignment: .leading, spacing: 12) {
                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.lwElevated)
                        .frame(width: 200, height: 16)
                        .shimmerSweep(isActive: isLoading)
                        .clipShape(RoundedRectangle(cornerRadius: 4))

                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.lwElevated)
                        .frame(width: 280, height: 16)
                        .shimmerSweep(isActive: isLoading)
                        .clipShape(RoundedRectangle(cornerRadius: 4))

                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.lwElevated)
                        .frame(width: 160, height: 16)
                        .shimmerSweep(isActive: isLoading)
                        .clipShape(RoundedRectangle(cornerRadius: 4))
                }

                // Toggle
                Toggle("Loading", isOn: $isLoading)
                    .toggleStyle(.switch)
                    .foregroundStyle(Color.lwTextPrimary)
                    .padding()
            }
            .padding()
            .background(Color.lwBase)
        }
    }

    return ShimmerDemo()
}
