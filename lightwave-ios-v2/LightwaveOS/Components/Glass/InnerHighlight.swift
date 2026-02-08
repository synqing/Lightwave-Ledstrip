//
//  InnerHighlight.swift
//  LightwaveOS
//
//  Inner highlight primitive for specular lip effect.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Inner Highlight Style

/// Intensity levels for inner highlight
enum InnerHighlightStyle {
    case subtle
    case standard
    case bright

    var opacity: Double {
        switch self {
        case .subtle:   return 0.15
        case .standard: return 0.25
        case .bright:   return 0.40
        }
    }

    var height: CGFloat {
        switch self {
        case .subtle:   return 2.0
        case .standard: return 4.0
        case .bright:   return 6.0
        }
    }
}

// MARK: - Inner Highlight Modifier

/// Applies a top-edge specular highlight (the "glass lip")
struct InnerHighlightModifier: ViewModifier {
    let style: InnerHighlightStyle
    let cornerRadius: CGFloat

    func body(content: Content) -> some View {
        content
            .overlay(alignment: .top) {
                LinearGradient(
                    colors: [
                        Color.lwGlassHighlight.opacity(style.opacity),
                        Color.clear
                    ],
                    startPoint: .top,
                    endPoint: .bottom
                )
                .frame(height: style.height + cornerRadius * 0.3)
                .clipShape(
                    UnevenRoundedRectangle(
                        topLeadingRadius: cornerRadius,
                        bottomLeadingRadius: 0,
                        bottomTrailingRadius: 0,
                        topTrailingRadius: cornerRadius
                    )
                )
                .allowsHitTesting(false)
            }
    }
}

// MARK: - View Extension

extension View {
    /// Applies an inner highlight (specular lip) at the top edge
    /// - Parameters:
    ///   - style: Intensity level
    ///   - cornerRadius: Corner radius to match container
    func innerHighlight(
        style: InnerHighlightStyle = .standard,
        cornerRadius: CGFloat = CornerRadius.card
    ) -> some View {
        modifier(InnerHighlightModifier(style: style, cornerRadius: cornerRadius))
    }
}

// MARK: - Preview

#Preview("Inner Highlights") {
    VStack(spacing: 24) {
        ForEach([InnerHighlightStyle.subtle, .standard, .bright], id: \.self) { style in
            Text(String(describing: style))
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
                .padding()
                .frame(maxWidth: .infinity)
                .background(Color.lwCard)
                .innerHighlight(style: style, cornerRadius: CornerRadius.card)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }
    }
    .padding()
    .background(Color.lwBase)
}
