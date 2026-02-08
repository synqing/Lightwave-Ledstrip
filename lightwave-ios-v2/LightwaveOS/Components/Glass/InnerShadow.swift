//
//  InnerShadow.swift
//  LightwaveOS
//
//  Inner shadow primitive for depth/recess cue.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Inner Shadow Style

/// Depth levels for inner shadow
enum InnerShadowStyle {
    case subtle
    case standard
    case deep

    var colour: Color { .black }

    var opacity: Double {
        switch self {
        case .subtle:   return 0.30
        case .standard: return 0.45
        case .deep:     return 0.60
        }
    }

    var radius: CGFloat {
        switch self {
        case .subtle:   return 4
        case .standard: return 8
        case .deep:     return 12
        }
    }

    var offset: CGSize {
        switch self {
        case .subtle:   return CGSize(width: 0, height: 1)
        case .standard: return CGSize(width: 0, height: 2)
        case .deep:     return CGSize(width: 0, height: 4)
        }
    }
}

// MARK: - Inner Shadow Modifier

/// Applies an inner shadow (inset shading) for depth perception
struct InnerShadowModifier: ViewModifier {
    let style: InnerShadowStyle
    let cornerRadius: CGFloat

    func body(content: Content) -> some View {
        content
            .overlay {
                RoundedRectangle(cornerRadius: cornerRadius)
                    .stroke(style.colour.opacity(style.opacity), lineWidth: style.radius * 2)
                    .blur(radius: style.radius)
                    .offset(style.offset)
                    .mask {
                        RoundedRectangle(cornerRadius: cornerRadius)
                            .fill(
                                LinearGradient(
                                    colors: [Color.black, Color.clear],
                                    startPoint: .top,
                                    endPoint: UnitPoint(x: 0.5, y: 0.3)
                                )
                            )
                    }
                    .allowsHitTesting(false)
            }
    }
}

// MARK: - View Extension

extension View {
    /// Applies an inner shadow for depth perception
    /// - Parameters:
    ///   - style: Depth intensity level
    ///   - cornerRadius: Corner radius to match container
    func innerShadow(
        style: InnerShadowStyle = .standard,
        cornerRadius: CGFloat = CornerRadius.card
    ) -> some View {
        modifier(InnerShadowModifier(style: style, cornerRadius: cornerRadius))
    }
}

// MARK: - Preview

#Preview("Inner Shadows") {
    VStack(spacing: 24) {
        ForEach([InnerShadowStyle.subtle, .standard, .deep], id: \.self) { style in
            Text(String(describing: style))
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
                .padding()
                .frame(maxWidth: .infinity)
                .background(Color.lwCard)
                .innerShadow(style: style, cornerRadius: CornerRadius.card)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }
    }
    .padding()
    .background(Color.lwBase)
}
