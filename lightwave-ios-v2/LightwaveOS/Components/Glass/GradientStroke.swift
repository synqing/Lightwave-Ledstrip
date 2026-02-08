//
//  GradientStroke.swift
//  LightwaveOS
//
//  Gradient stroke primitive for glass card edges.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Gradient Stroke Style

/// Styles for gradient strokes with multi-stop gradients
enum GradientStrokeStyle {
    case subtle         // Low contrast, ambient
    case standard       // Default card stroke
    case accent         // Gold-tinted for selected states
    case danger         // Red-tinted for error states
    case zone(Color)    // Zone-specific colour

    var gradient: LinearGradient {
        switch self {
        case .subtle:
            return LinearGradient(
                stops: [
                    .init(color: Color.white.opacity(0.20), location: 0.0),
                    .init(color: Color.white.opacity(0.10), location: 0.5),
                    .init(color: Color.white.opacity(0.05), location: 1.0)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
        case .standard:
            return LinearGradient(
                stops: [
                    .init(color: Color.white.opacity(0.30), location: 0.0),
                    .init(color: Color.white.opacity(0.15), location: 0.3),
                    .init(color: Color.white.opacity(0.05), location: 0.7),
                    .init(color: Color.clear, location: 1.0)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
        case .accent:
            return LinearGradient(
                stops: [
                    .init(color: Color.lwGold.opacity(0.6), location: 0.0),
                    .init(color: Color.lwGold.opacity(0.25), location: 0.4),
                    .init(color: Color.white.opacity(0.10), location: 1.0)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
        case .danger:
            return LinearGradient(
                stops: [
                    .init(color: Color.lwError.opacity(0.6), location: 0.0),
                    .init(color: Color.lwError.opacity(0.25), location: 0.4),
                    .init(color: Color.white.opacity(0.10), location: 1.0)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
        case .zone(let colour):
            return LinearGradient(
                stops: [
                    .init(color: colour.opacity(0.5), location: 0.0),
                    .init(color: colour.opacity(0.2), location: 0.4),
                    .init(color: Color.white.opacity(0.05), location: 1.0)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
        }
    }

    var lineWidth: CGFloat { 1.0 }
}

// MARK: - Gradient Stroke Modifier

/// Applies a 1px gradient stroke overlay
struct GradientStrokeModifier: ViewModifier {
    let style: GradientStrokeStyle
    let cornerRadius: CGFloat

    func body(content: Content) -> some View {
        content
            .overlay {
                RoundedRectangle(cornerRadius: cornerRadius)
                    .strokeBorder(style.gradient, lineWidth: style.lineWidth)
            }
    }
}

// MARK: - View Extension

extension View {
    /// Applies a gradient stroke border
    /// - Parameters:
    ///   - style: Stroke style (subtle, standard, accent, zone)
    ///   - cornerRadius: Corner radius for the stroke
    func gradientStroke(
        style: GradientStrokeStyle = .standard,
        cornerRadius: CGFloat = CornerRadius.card
    ) -> some View {
        modifier(GradientStrokeModifier(style: style, cornerRadius: cornerRadius))
    }
}

// MARK: - Preview

#Preview("Gradient Strokes") {
    VStack(spacing: 24) {
        Text("Subtle")
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.lwCard)
            .gradientStroke(style: .subtle)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))

        Text("Standard")
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.lwCard)
            .gradientStroke(style: .standard)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))

        Text("Accent")
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.lwCard)
            .gradientStroke(style: .accent)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))

        Text("Zone 0")
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.lwCard)
            .gradientStroke(style: .zone(.lwZone0))
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
    }
    .font(.bodyValue)
    .foregroundStyle(Color.lwTextPrimary)
    .padding()
    .background(Color.lwBase)
}
