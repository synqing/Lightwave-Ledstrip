//
//  ConnectionDot.swift
//  LightwaveOS
//
//  "Instrument jewel" status indicator for device connection state.
//  Premium treatment with elliptical specular, crescent rim, and controlled glow.
//
//  Part of the "Liquid Glass" visual system.
//

import SwiftUI

struct ConnectionDot: View {
    var connectionState: ConnectionState

    @SwiftUI.State private var pulseOpacity: Double = 1.0
    @SwiftUI.State private var rotationAngle: Double = 0.0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    // MARK: - Constants

    private let dotSize: CGFloat = 8
    private let specularWidth: CGFloat = 3
    private let specularHeight: CGFloat = 1.5
    private let glowRadius: CGFloat = 4  // 140-160% of dot size
    private let rimArcExtent: Angle = .degrees(120)
    private let rimArcOffset: Angle = .degrees(-60)

    // MARK: - Initialisers

    init(state: ConnectionState) {
        self.connectionState = state
    }

    /// Convenience initialiser for backwards compatibility
    init(isConnected: Bool, isConnecting: Bool = false) {
        if isConnecting {
            self.connectionState = .targeting
        } else if isConnected {
            self.connectionState = .ready
        } else {
            self.connectionState = .idle
        }
    }

    var body: some View {
        ZStack {
            // Base dot
            Circle()
                .fill(statusColor)
                .frame(width: dotSize, height: dotSize)

            // Crescent rim highlight (top-left arc)
            Circle()
                .trim(from: 0, to: rimArcExtent.degrees / 360)
                .stroke(
                    Color.white.opacity(0.2),
                    style: StrokeStyle(lineWidth: 0.5, lineCap: .round)
                )
                .frame(width: dotSize - 1, height: dotSize - 1)
                .rotationEffect(rimArcOffset)

            // Elliptical specular highlight (not circular)
            Ellipse()
                .fill(Color.white.opacity(GlassTokens.specularOpacity))
                .frame(width: specularWidth, height: specularHeight)
                .offset(y: -dotSize / 4)
                .blur(radius: 0.5)
        }
        .compositingGroup()
        // 3-layer instrument glow system (not single shadow)
        // Layer 1: Core specular energy (tight, bright)
        .shadow(
            color: glowColor.opacity(reduceMotion ? 0 : glowOpacity * 0.6),
            radius: 2,
            x: 0,
            y: 0
        )
        // Layer 2: Main glow halo (medium spread)
        .shadow(
            color: glowColor.opacity(reduceMotion ? 0 : glowOpacity * 0.4),
            radius: 4,
            x: 0,
            y: 0
        )
        // Layer 3: Outer bloom (restrained ambient)
        .shadow(
            color: glowColor.opacity(reduceMotion ? 0 : glowOpacity * 0.15),
            radius: 8,
            x: 0,
            y: 0
        )
        .opacity(reduceMotion ? 1.0 : pulseOpacity)
        .rotationEffect(.degrees(reduceMotion ? 0 : rotationAngle))
        .onAppear {
            startAnimationIfNeeded()
        }
        .onChange(of: connectionState) { _, _ in
            startAnimationIfNeeded()
        }
    }

    // MARK: - Display State

    /// Extended display states for instrument-quality indicator
    enum DisplayState {
        case searching      // Looking for device (subtle shimmer rotation)
        case connecting     // Handshake in progress (contained pulse)
        case connected      // Stable connection (static jewel)
        case degraded       // Poor signal (slow amber pulse)
        case offline        // No connection (static, minimal glow)
    }

    private var displayState: DisplayState {
        mapToDisplayState(connectionState)
    }

    private func mapToDisplayState(_ state: ConnectionState) -> DisplayState {
        switch state {
        case .ready:
            return .connected
        case .targeting:
            return .searching
        case .handshakeHTTP, .connectingWS:
            return .connecting
        case .backoff:
            return .degraded
        case .idle:
            return .offline
        case .failed:
            return .offline
        }
    }

    // MARK: - Colours

    private var statusColor: Color {
        switch displayState {
        case .connected:
            return Color.lwSuccess
        case .searching, .connecting:
            return Color.lwGold
        case .degraded:
            return Color(hex: "FFB347")  // Amber
        case .offline:
            return Color.lwError
        }
    }

    private var glowColor: Color {
        switch displayState {
        case .connected:
            return Color.lwSuccess
        case .searching, .connecting:
            return Color.lwGold
        case .degraded:
            return Color(hex: "FFB347")
        case .offline:
            return Color.lwError.opacity(0.5)  // Minimal red glow
        }
    }

    private var glowOpacity: Double {
        switch displayState {
        case .connected:
            return 0.3  // Subtle static glow
        case .searching:
            return 0.4
        case .connecting:
            return pulseOpacity * 0.5
        case .degraded:
            return pulseOpacity * 0.3
        case .offline:
            return 0.15  // Minimal
        }
    }

    // MARK: - Animation

    private func startAnimationIfNeeded() {
        // Reset animations
        pulseOpacity = 1.0
        rotationAngle = 0.0

        guard !reduceMotion else { return }

        switch displayState {
        case .searching:
            // Subtle shimmer rotation (not bouncy pulse)
            withAnimation(
                .linear(duration: AnimationTokens.rotationDuration)
                .repeatForever(autoreverses: false)
            ) {
                rotationAngle = 360
            }

        case .connecting:
            // Contained pulse with small ring bloom (subtle, not attention-grabbing)
            withAnimation(
                .easeInOut(duration: AnimationTokens.pulseRepeat)
                .repeatForever(autoreverses: true)
            ) {
                pulseOpacity = 0.75  // Reduced amplitude, maintain "instrument jewel" feel
            }

        case .degraded:
            // Slow, serious amber pulse (subtle, not toy LED)
            withAnimation(
                .easeInOut(duration: 1.2)
                .repeatForever(autoreverses: true)
            ) {
                pulseOpacity = 0.7  // Reduced amplitude, keep above 0.7 threshold
            }

        case .connected, .offline:
            // Static - no animation
            break
        }
    }
}

// MARK: - Preview

#Preview("Connection States") {
    VStack(spacing: Spacing.lg) {
        GlassCard(title: "5 Display States") {
            VStack(alignment: .leading, spacing: Spacing.md) {
                HStack {
                    ConnectionDot(state: .targeting)
                    Text("Searching...")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .connectingWS)
                    Text("Connecting...")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .ready)
                    Text("Connected")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .backoff(attempt: 2, nextRetry: Date().addingTimeInterval(5)))
                    Text("Degraded")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .idle)
                    Text("Offline")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
        }

        GlassCard(title: "All Connection States") {
            VStack(alignment: .leading, spacing: Spacing.md) {
                HStack {
                    ConnectionDot(state: .ready)
                    Text("Ready")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .idle)
                    Text("Idle")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .targeting)
                    Text("Targeting")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .handshakeHTTP)
                    Text("HTTP Handshake")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .connectingWS)
                    Text("WebSocket Connect")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .backoff(attempt: 2, nextRetry: Date()))
                    Text("Backoff")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .failed(.noTarget))
                    Text("Failed")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
        }

        GlassCard(title: "In Status Bar") {
            HStack(spacing: Spacing.sm) {
                ConnectionDot(state: .ready)
                Text("Connected")
                    .font(.cardLabel)
                    .foregroundStyle(Color.lwTextPrimary)

                Text("•")
                    .foregroundStyle(Color.lwTextTertiary)

                Text("LGP Holographic")
                    .font(.cardLabel)
                    .foregroundStyle(Color.lwTextSecondary)
            }
        }
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
}
