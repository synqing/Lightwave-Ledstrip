//
//  EdgeMixerCard.swift
//  LightwaveOS
//
//  EdgeMixer post-processor controls for the Play tab.
//  7 colour modes, spread/strength sliders, spatial/temporal pickers.
//

import SwiftUI

struct EdgeMixerCard: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        let em = appVM.edgeMixer

        LWCard(title: "Edge Mixer") {
            VStack(alignment: .leading, spacing: Spacing.md) {
                // Mode chip row (horizontal scroll)
                modeChipRow(em: em)

                divider

                // Spread slider (conditional on mode)
                if em.config.mode.usesSpread {
                    LWSlider(
                        title: "Spread",
                        value: Binding(
                            get: { Double(em.config.spread) },
                            set: { em.sliderChanged("spread", value: Int($0)) }
                        ),
                        range: 0...60,
                        step: 1,
                        leftLabel: "0\u{00B0}",
                        rightLabel: "60\u{00B0}",
                        onChanged: { value in
                            em.sliderChanged("spread", value: Int(value))
                        },
                        onEnded: {
                            em.sliderReleased("spread")
                        }
                    )
                    .transition(.opacity.combined(with: .move(edge: .top)))

                    divider
                }

                // Strength slider
                LWSlider(
                    title: "Strength",
                    value: Binding(
                        get: { Double(em.config.strength) },
                        set: { em.sliderChanged("strength", value: Int($0)) }
                    ),
                    range: 0...255,
                    step: 1,
                    trackGradient: LinearGradient(
                        colors: [Color.lwGold.opacity(0.3), Color.lwGold],
                        startPoint: .leading,
                        endPoint: .trailing
                    ),
                    onChanged: { value in
                        em.sliderChanged("strength", value: Int(value))
                    },
                    onEnded: {
                        em.sliderReleased("strength")
                    }
                )

                divider

                // Spatial picker
                HStack {
                    Text("Spatial")
                        .font(.cardLabel)
                        .foregroundStyle(Color.lwTextSecondary)
                        .textCase(.uppercase)

                    Spacer()

                    Picker("", selection: Binding(
                        get: { em.config.spatial },
                        set: { em.setSpatial($0) }
                    )) {
                        ForEach(EdgeMixerSpatial.allCases) { s in
                            Text(s.name).tag(s)
                        }
                    }
                    .pickerStyle(.segmented)
                    .frame(maxWidth: 200)
                }

                divider

                // Temporal picker
                HStack {
                    Text("Temporal")
                        .font(.cardLabel)
                        .foregroundStyle(Color.lwTextSecondary)
                        .textCase(.uppercase)

                    Spacer()

                    Picker("", selection: Binding(
                        get: { em.config.temporal },
                        set: { em.setTemporal($0) }
                    )) {
                        ForEach(EdgeMixerTemporal.allCases) { t in
                            Text(t.name).tag(t)
                        }
                    }
                    .pickerStyle(.segmented)
                    .frame(maxWidth: 200)
                }
            }
            .animation(.easeInOut(duration: 0.25), value: em.config.mode)
        }
    }

    // MARK: - Mode Chip Row

    @ViewBuilder
    private func modeChipRow(em: EdgeMixerViewModel) -> some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: Spacing.sm) {
                ForEach(EdgeMixerMode.allCases) { mode in
                    Button {
                        em.setMode(mode)
                    } label: {
                        Text(mode.name)
                            .font(.cardLabel)
                            .foregroundStyle(
                                mode == em.config.mode
                                    ? Color.lwBase
                                    : Color.lwTextSecondary
                            )
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                            .background(
                                RoundedRectangle(cornerRadius: CornerRadius.nested)
                                    .fill(
                                        mode == em.config.mode
                                            ? Color.lwGold
                                            : Color.lwElevated
                                    )
                            )
                    }
                    .buttonStyle(.plain)
                }
            }
        }
    }

    // MARK: - Divider

    private var divider: some View {
        Rectangle()
            .fill(Color.white.opacity(0.04))
            .frame(height: 1)
    }
}

// MARK: - Preview

#Preview("Edge Mixer - Mirror") {
    ScrollView {
        EdgeMixerCard()
            .padding(Spacing.lg)
    }
    .background(Color.lwBase)
    .environment(AppViewModel())
}

#Preview("Edge Mixer - Analogous") {
    ScrollView {
        EdgeMixerCard()
            .padding(Spacing.lg)
    }
    .background(Color.lwBase)
    .environment({
        let vm = AppViewModel()
        vm.edgeMixer.config.mode = .analogous
        vm.edgeMixer.config.spread = 25
        vm.edgeMixer.config.strength = 200
        return vm
    }())
}
