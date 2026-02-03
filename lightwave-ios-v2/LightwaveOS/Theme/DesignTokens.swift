//
//  DesignTokens.swift
//  LightwaveOS
//
//  Design tokens for dark theme with Lightwave brand colours.
//  Expanded for v2 with typography scale, gradients, and shadows.
//

import SwiftUI

// MARK: - Colour Extensions

extension Color {
    // MARK: Brand Colours

    /// Primary accent colour - Lightwave Gold
    static let lwGold = Color(hex: "FFB84D")

    /// App background - deepest black-blue
    static let lwBase = Color(hex: "0F1219")

    /// Card background - elevated surface
    static let lwCard = Color(hex: "1E2535")

    /// Elevated surfaces - highest elevation
    static let lwElevated = Color(hex: "252D3F")

    // MARK: Text Colours

    /// Primary text colour - high contrast
    static let lwTextPrimary = Color(hex: "E6E9EF")

    /// Secondary text colour - medium contrast
    static let lwTextSecondary = Color(hex: "9CA3B0")

    /// Tertiary text colour - low contrast
    static let lwTextTertiary = Color(hex: "6B7280")

    // MARK: Status Colours

    /// Success state - bright green
    static let lwSuccess = Color(hex: "4DFFB8")

    /// Error state - bright red
    static let lwError = Color(hex: "FF4D4D")

    /// Zone accent - cyan
    static let lwCyan = Color(hex: "00D4FF")

    // MARK: Beat Accent

    /// Beat accent (brighter than lwGold)
    static let lwBeatAccent = Color(hex: "FFCC33")

    // MARK: Zone Colours

    static let lwZone0 = Color(hex: "00FFFF")  // Cyan
    static let lwZone1 = Color(hex: "00FF99")  // Green
    static let lwZone2 = Color(hex: "9900FF")  // Purple
    static let lwZone3 = Color(hex: "FF6600")  // Orange

    static func zoneColor(_ index: Int) -> Color {
        switch index {
        case 0: return .lwZone0
        case 1: return .lwZone1
        case 2: return .lwZone2
        case 3: return .lwZone3
        default: return .lwGold
        }
    }

    // MARK: Tab Bar Colours

    static let lwTabBarBackground = Color(hex: "0F1219").opacity(0.95)
    static let lwTabBarActive = Color.lwGold
    static let lwTabBarInactive = Color(hex: "6B7280")

    // MARK: Gradients

    /// Card background gradient (top to bottom)
    static let lwCardGradient = LinearGradient(
        colors: [Color(hex: "1E2535"), Color(hex: "1A2030")],
        startPoint: .top,
        endPoint: .bottom
    )

    /// Hero background glow (radial)
    static let lwGoldRadial = RadialGradient(
        colors: [Color.lwGold.opacity(0.08), Color.clear],
        center: .center,
        startRadius: 0,
        endRadius: 200
    )

    // MARK: Hex Initialiser

    /// Initialise Color from hex string
    /// - Parameter hex: Hex string (with or without # prefix)
    init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var int: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&int)
        let a, r, g, b: UInt64
        switch hex.count {
        case 3: // RGB (12-bit)
            (a, r, g, b) = (255, (int >> 8) * 17, (int >> 4 & 0xF) * 17, (int & 0xF) * 17)
        case 6: // RGB (24-bit)
            (a, r, g, b) = (255, int >> 16, int >> 8 & 0xFF, int & 0xFF)
        case 8: // ARGB (32-bit)
            (a, r, g, b) = (int >> 24, int >> 16 & 0xFF, int >> 8 & 0xFF, int & 0xFF)
        default:
            (a, r, g, b) = (255, 0, 0, 0)
        }

        self.init(
            .sRGB,
            red: Double(r) / 255,
            green: Double(g) / 255,
            blue: Double(b) / 255,
            opacity: Double(a) / 255
        )
    }
}

// MARK: - Typography (Tab5.encoder font stack)

extension Font {
    // MARK: Bebas Neue — Brand / Headings

    /// Hero numeral: 48pt Bebas Neue Bold (BPM display)
    static let heroNumeral = Font.custom("BebasNeue-Bold", size: 48)

    /// Effect title: 22pt Bebas Neue Bold (current effect name on hero)
    static let effectTitle = Font.custom("BebasNeue-Bold", size: 22)

    /// Section header: 14pt Bebas Neue Bold (uppercase, +1.5px tracking)
    static let sectionHeader = Font.custom("BebasNeue-Bold", size: 14)

    /// Sub-group header: 12pt Bebas Neue Bold (uppercase, +1.5px tracking)
    static let subGroupHeader = Font.custom("BebasNeue-Bold", size: 12)

    /// Status brand: 16pt Bebas Neue Bold ("LIGHTWAVEOS")
    static let statusBrand = Font.custom("BebasNeue-Bold", size: 16)

    // MARK: Rajdhani — Body / Labels

    /// Body value: 15pt Rajdhani Medium (selected effect/palette name)
    static let bodyValue = Font.custom("Rajdhani-Medium", size: 15)

    /// Card label: 12pt Rajdhani Medium (slider labels, metadata)
    static let cardLabel = Font.custom("Rajdhani-Medium", size: 12)

    /// Caption: 11pt Rajdhani Regular (IDs, counts, timestamps)
    static let caption = Font.custom("Rajdhani-Medium", size: 11)

    /// Haptic label: 13pt Rajdhani Medium (toggle labels)
    static let hapticLabel = Font.custom("Rajdhani-Medium", size: 13)

    /// Status label: 12pt Rajdhani Medium (connection status)
    static let statusLabel = Font.custom("Rajdhani-Medium", size: 12)

    /// Pill label: 11pt Bebas Neue Bold (PATTERN, PALETTE)
    static let pillLabel = Font.custom("BebasNeue-Bold", size: 11)

    // MARK: JetBrains Mono — Values / Monospace

    /// Slider value: 13pt JetBrains Mono Medium (numeric readouts)
    static let sliderValue = Font.custom("JetBrainsMono-Medium", size: 13)

    /// Monospace: 11pt JetBrains Mono Medium (debug log, IPs, LED indices)
    static let monospace = Font.custom("JetBrainsMono-Medium", size: 11)

    /// Metric value: 11pt JetBrains Mono Medium (real-time metric readouts)
    static let metricValue = Font.custom("JetBrainsMono-Medium", size: 11)

    /// Stepper value: 13pt JetBrains Mono Medium (stepper readouts)
    static let stepperValue = Font.custom("JetBrainsMono-Medium", size: 13)
}

// MARK: - Spacing

enum Spacing {
    /// Inline (icon → text) - 4pt
    static let xs: CGFloat = 4

    /// Between related elements (label → slider) - 8pt
    static let sm: CGFloat = 8

    /// Between cards within section - 12pt
    static let md: CGFloat = 12

    /// Between major sections - 24pt
    static let lg: CGFloat = 24
}

// MARK: - Corner Radius

enum CornerRadius {
    /// Hero container (LED preview, BPM hero) - 24pt
    static let hero: CGFloat = 24

    /// Primary card (control cards, zone cards) - 16pt
    static let card: CGFloat = 16

    /// Nested element (sliders, dropdowns, badges) - 12pt
    static let nested: CGFloat = 12
}

// MARK: - Shadows

extension View {
    /// Ambient card shadow: 0 2px 12px rgba(0,0,0,0.4)
    func ambientShadow() -> some View {
        self.shadow(color: Color.black.opacity(0.4), radius: 12, x: 0, y: 2)
    }

    /// Elevated shadow: 0 4px 20px rgba(0,0,0,0.5)
    func elevatedShadow() -> some View {
        self.shadow(color: Color.black.opacity(0.5), radius: 20, x: 0, y: 4)
    }

    /// Hero glow: 0 0 40px accent.opacity(0.15)
    func heroGlow(color: Color = .lwGold) -> some View {
        self.shadow(color: color.opacity(0.15), radius: 40, x: 0, y: 0)
    }

    /// Beat pulse glow: 0 0 24px lwGold.opacity(0.6)
    func beatPulseGlow() -> some View {
        self.shadow(color: Color.lwGold.opacity(0.6), radius: 24, x: 0, y: 0)
    }
}
