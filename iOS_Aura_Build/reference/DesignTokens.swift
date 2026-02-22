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

    // MARK: Glass Surface Colours

    /// Glass tint colour - subtle blue-black for glass surfaces
    static let lwGlassTint = Color(hex: "0A0D14")

    /// Glass highlight - warm white for specular highlights
    static let lwGlassHighlight = Color(hex: "FFFAF5")

    /// Glass secondary gold - for BPM ring gradient endpoint
    static let lwGoldSecondary = Color(hex: "FFD700")

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
    static let heroNumeral = Font.custom("BebasNeue-Bold", size: 48, relativeTo: .largeTitle)

    /// Effect title: 22pt Bebas Neue Bold (current effect name on hero)
    static let effectTitle = Font.custom("BebasNeue-Bold", size: 22, relativeTo: .title3)

    /// Section header: 14pt Bebas Neue Bold (uppercase, +1.5px tracking)
    static let sectionHeader = Font.custom("BebasNeue-Bold", size: 14, relativeTo: .headline)

    /// Sub-group header: 12pt Bebas Neue Bold (uppercase, +1.5px tracking)
    static let subGroupHeader = Font.custom("BebasNeue-Bold", size: 12, relativeTo: .subheadline)

    /// Status brand: 16pt Bebas Neue Bold ("LIGHTWAVEOS")
    static let statusBrand = Font.custom("BebasNeue-Bold", size: 16, relativeTo: .headline)

    /// Sheet title: 17pt Rajdhani Medium (modal titles)
    static let sheetTitle = Font.custom("Rajdhani-Medium", size: 17, relativeTo: .headline)

    /// Zone pill label: 13pt Bebas Neue Bold (zone chip)
    static let zonePillLabel = Font.custom("BebasNeue-Bold", size: 13, relativeTo: .subheadline)

    /// BPM numeral: 32pt Bebas Neue Bold
    static let bpmNumeral = Font.custom("BebasNeue-Bold", size: 32, relativeTo: .title)

    // MARK: Rajdhani — Body / Labels

    /// Body value: 15pt Rajdhani Medium (selected effect/palette name)
    static let bodyValue = Font.custom("Rajdhani-Medium", size: 15, relativeTo: .body)

    /// Card label: 12pt Rajdhani Medium (slider labels, metadata)
    static let cardLabel = Font.custom("Rajdhani-Medium", size: 12, relativeTo: .subheadline)

    /// Caption: 11pt Rajdhani Medium (IDs, counts, timestamps)
    static let caption = Font.custom("Rajdhani-Medium", size: 11, relativeTo: .caption)

    /// Micro label: 10pt Rajdhani Medium (tight UI labels)
    static let microLabel = Font.custom("Rajdhani-Medium", size: 10, relativeTo: .caption2)

    /// Haptic label: 13pt Rajdhani Medium (toggle labels)
    static let hapticLabel = Font.custom("Rajdhani-Medium", size: 13, relativeTo: .callout)

    /// Status label: 12pt Rajdhani Medium (connection status)
    static let statusLabel = Font.custom("Rajdhani-Medium", size: 12, relativeTo: .footnote)

    /// Pill label: 11pt Bebas Neue Bold (PATTERN, PALETTE)
    static let pillLabel = Font.custom("BebasNeue-Bold", size: 11, relativeTo: .caption)

    // MARK: JetBrains Mono — Values / Monospace

    /// Slider value: 13pt JetBrains Mono Medium (numeric readouts)
    static let sliderValue = Font.custom("JetBrainsMono-Medium", size: 13, relativeTo: .body)

    /// Monospace: 11pt JetBrains Mono Medium (debug log, IPs, LED indices)
    static let monospace = Font.custom("JetBrainsMono-Medium", size: 11, relativeTo: .caption)

    /// Monospace small: 10pt JetBrains Mono Medium (compact readouts)
    static let monospaceSmall = Font.custom("JetBrainsMono-Medium", size: 10, relativeTo: .caption2)

    /// Metric value: 11pt JetBrains Mono Medium (real-time metric readouts)
    static let metricValue = Font.custom("JetBrainsMono-Medium", size: 11, relativeTo: .caption)

    /// Stepper value: 13pt JetBrains Mono Medium (stepper readouts)
    static let stepperValue = Font.custom("JetBrainsMono-Medium", size: 13, relativeTo: .body)

    // MARK: System Icons

    /// Icon tiny: 10pt system
    static let iconTiny = Font.system(size: 10, weight: .semibold)

    /// Icon small: 13pt system
    static let iconSmall = Font.system(size: 13, weight: .semibold)

    /// Icon nav: 14pt system
    static let iconNav = Font.system(size: 14, weight: .regular)

    /// Icon medium: 24pt system
    static let iconMedium = Font.system(size: 24, weight: .regular)

    /// Icon large: 48pt system
    static let iconLarge = Font.system(size: 48, weight: .regular)
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

// MARK: - Phase Palette (Beat Phases)

/// Semantic colours for the 4-beat phase cycle.
/// Maps musical energy flow to warm gold/orange spectrum.
enum PhaseTokens {
    /// BUILD phase (rising energy) - warm amber
    static let build = Color(hex: "FFB347")

    /// HOLD phase (peak energy) - bright gold
    static let hold = Color(hex: "FFD700")

    /// RELEASE phase (falling energy) - warm orange
    static let release = Color(hex: "FF8C42")

    /// REST phase (low energy) - muted gold
    static let rest = Color(hex: "CC9933")

    /// Inactive dot colour
    static let inactive = Color.lwElevated.opacity(0.3)

    /// Returns the semantic colour for a phase index (0-3)
    static func color(forPhase phase: Int) -> Color {
        switch phase % 4 {
        case 0: return build
        case 1: return hold
        case 2: return release
        case 3: return rest
        default: return inactive
        }
    }
}

// MARK: - Glass Tokens

/// Tokens for the "Liquid Glass" optical rendering system.
/// Used by GlassSurface, GlowStack, and other glass primitives.
enum GlassTokens {
    // MARK: Surface Tint

    /// Background tint opacity for glass surfaces
    static let surfaceTint: Double = 0.75

    // MARK: Blur Radii

    /// Surface blur radius for glass foundation (pt)
    static let surfaceBlur: CGFloat = 16

    /// Bloom blur radius for outer glows (pt)
    static let bloomBlur: CGFloat = 28

    /// Specular blur radius for highlights (pt)
    static let specularBlur: CGFloat = 6

    // MARK: Opacity Values

    /// Inner glow opacity (0-1)
    static let innerGlowOpacity: Double = 0.25

    /// Specular highlight opacity (0-1)
    static let specularOpacity: Double = 0.25

    /// Bloom/outer glow opacity (0-1)
    static let bloomOpacity: Double = 0.15

    /// Gradient stroke opacity (0-1)
    static let strokeOpacity: Double = 0.12

    /// Grain overlay intensity (0-1)
    static let grainIntensity: Double = 0.04

    // MARK: Scale Factors

    /// Bloom layer scale relative to container (1.0 = 100%)
    static let bloomScale: CGFloat = 1.25

    /// Maximum beat burst scale - CONSTRAINED (NOT 2.5+)
    static let beatBurstMax: CGFloat = 1.35

    /// Regular beat scale
    static let beatBurstRegular: CGFloat = 1.15

    // MARK: Shadow Tokens

    // Card shadows
    static let shadowCardContactOpacity: Double = 0.35
    static let shadowCardContactRadius: CGFloat = 4
    static let shadowCardContactX: CGFloat = 0
    static let shadowCardContactY: CGFloat = 2

    static let shadowCardSpreadOpacity: Double = 0.50
    static let shadowCardSpreadRadius: CGFloat = 16
    static let shadowCardSpreadX: CGFloat = 0
    static let shadowCardSpreadY: CGFloat = 6

    // Elevated shadows
    static let shadowElevatedContactOpacity: Double = 0.40
    static let shadowElevatedContactRadius: CGFloat = 6
    static let shadowElevatedContactX: CGFloat = 0
    static let shadowElevatedContactY: CGFloat = 3

    static let shadowElevatedSpreadOpacity: Double = 0.55
    static let shadowElevatedSpreadRadius: CGFloat = 20
    static let shadowElevatedSpreadX: CGFloat = 0
    static let shadowElevatedSpreadY: CGFloat = 8

    static let shadowElevatedDiffuseOpacity: Double = 0.25
    static let shadowElevatedDiffuseRadius: CGFloat = 40
    static let shadowElevatedDiffuseX: CGFloat = 0
    static let shadowElevatedDiffuseY: CGFloat = 16

    // Hero shadows
    static let shadowHeroContactOpacity: Double = 0.45
    static let shadowHeroContactRadius: CGFloat = 10
    static let shadowHeroContactX: CGFloat = 0
    static let shadowHeroContactY: CGFloat = 5

    static let shadowHeroSpreadOpacity: Double = 0.60
    static let shadowHeroSpreadRadius: CGFloat = 30
    static let shadowHeroSpreadX: CGFloat = 0
    static let shadowHeroSpreadY: CGFloat = 12

    static let shadowHeroDiffuseOpacity: Double = 0.30
    static let shadowHeroDiffuseRadius: CGFloat = 60
    static let shadowHeroDiffuseX: CGFloat = 0
    static let shadowHeroDiffuseY: CGFloat = 20
}

// MARK: - Animation Tokens

/// Timing tokens for the "Liquid Glass" animation system.
/// All durations in seconds.
enum AnimationTokens {
    // MARK: Beat-Reactive

    /// Energy pulse expand duration (quick attack)
    static let energyPulse: Double = 0.15

    /// Energy pulse decay duration (return to rest)
    static let energyDecay: Double = 0.25

    // MARK: Loading/Scanning

    /// Full shimmer sweep duration
    static let shimmerDuration: Double = 1.2

    // MARK: State Transitions

    /// Glow level transition duration
    static let glowTransition: Double = 0.3

    /// Surface state change duration
    static let surfaceTransition: Double = 0.2

    // MARK: Connection States

    /// Pulsing indicator repeat interval
    static let pulseRepeat: Double = 0.8

    /// Rotation animation duration (searching)
    static let rotationDuration: Double = 1.5

    // MARK: Spring Parameters

    /// Default spring response
    static let springResponse: Double = 0.3

    /// Default spring damping fraction
    static let springDamping: Double = 0.7

    /// Beat spring response (quick)
    static let beatSpringResponse: Double = 0.15

    /// Beat spring damping (bouncy)
    static let beatSpringDamping: Double = 0.6
}
