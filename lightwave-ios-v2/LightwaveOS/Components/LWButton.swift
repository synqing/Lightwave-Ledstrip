//
//  LWButton.swift
//  LightwaveOS
//
//  Primary and secondary button styles with haptic feedback.
//

import SwiftUI

struct LWButton: View {
    let title: String
    var icon: String?
    var style: ButtonStyle = .primary
    let action: () -> Void

    enum ButtonStyle {
        case primary
        case secondary
        case compact
    }

    private let impactFeedback = UIImpactFeedbackGenerator(style: .light)

    var body: some View {
        Button(action: {
            impactFeedback.impactOccurred()
            action()
        }) {
            HStack(spacing: Spacing.xs) {
                if let icon = icon {
                    Image(systemName: icon)
                        .font(style == .compact ? .iconTiny : .iconSmall)
                }

                if !title.isEmpty {
                    Text(title)
                        .font(style == .compact ? .microLabel : .hapticLabel)
                        .fontWeight(.semibold)
                }
            }
            .padding(.horizontal, horizontalPadding)
            .padding(.vertical, verticalPadding)
            .frame(minHeight: minHeight)
            .background(backgroundColor)
            .foregroundStyle(foregroundColor)
            .overlay(
                RoundedRectangle(cornerRadius: CornerRadius.nested)
                    .strokeBorder(borderColor, lineWidth: borderWidth)
            )
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
        }
        .buttonStyle(.plain)
    }

    // MARK: - Style Properties

    private var horizontalPadding: CGFloat {
        switch style {
        case .primary, .secondary:
            return 8
        case .compact:
            return 4
        }
    }

    private var verticalPadding: CGFloat {
        switch style {
        case .primary, .secondary:
            return 4
        case .compact:
            return 2
        }
    }

    private var minHeight: CGFloat {
        switch style {
        case .primary, .secondary:
            return 30
        case .compact:
            return 22
        }
    }

    private var backgroundColor: Color {
        switch style {
        case .primary:
            return Color.lwGold
        case .secondary, .compact:
            return Color.clear
        }
    }

    private var foregroundColor: Color {
        switch style {
        case .primary:
            return Color.lwBase
        case .secondary, .compact:
            return Color.lwGold
        }
    }

    private var borderColor: Color {
        switch style {
        case .primary:
            return Color.clear
        case .secondary, .compact:
            return Color.lwGold
        }
    }

    private var borderWidth: CGFloat {
        switch style {
        case .primary:
            return 0
        case .secondary:
            return 2
        case .compact:
            return 1
        }
    }
}

// MARK: - Preview

#Preview("Button Styles") {
    VStack(spacing: Spacing.lg) {
        LWButton(
            title: "Apply Effect",
            icon: "wand.and.stars",
            style: .primary
        ) {
            print("Primary tapped")
        }

        LWButton(
            title: "Cancel",
            style: .secondary
        ) {
            print("Secondary tapped")
        }

        HStack(spacing: Spacing.md) {
            LWButton(
                title: "",
                icon: "chevron.left",
                style: .compact
            ) {
                print("Prev")
            }

            LWButton(
                title: "",
                icon: "chevron.right",
                style: .compact
            ) {
                print("Next")
            }
        }

        LWButton(
            title: "Icon Only",
            icon: "play.fill",
            style: .primary
        ) {
            print("Play")
        }

        LWButton(
            title: "Trigger Transition",
            icon: "arrow.triangle.turn.up.right.diamond",
            style: .secondary
        ) {
            print("Transition")
        }
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
}
