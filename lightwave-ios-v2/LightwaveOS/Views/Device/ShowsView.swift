//
//  ShowsView.swift
//  LightwaveOS
//
//  Phase 2 P2-4 — Show playback transport.
//
//  Lists available shows fetched via REST `GET /api/v1/shows`, exposes a
//  "Now Playing" section bound to `ShowViewModel.currentShow`, and provides
//  transport controls (play / pause / resume / stop) plus a seek slider.
//  Transport dispatch goes over WebSocket for low latency.
//
//  British English in all UI strings.
//

import SwiftUI

struct ShowsView: View {
    @Environment(AppViewModel.self) private var appVM
    @State private var showVM = ShowViewModel()
    @State private var seekValue: Double = 0

    var body: some View {
        List {
            nowPlayingSection
            showsSection
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle("Shows")
        .navigationBarTitleDisplayMode(.inline)
        .task {
            // Inject network services from AppViewModel and trigger initial load.
            showVM.restClient = appVM.rest
            showVM.ws = appVM.ws
            await showVM.loadShows()
            await showVM.requestStatus()
        }
        .onChange(of: showVM.elapsedMs) { _, newValue in
            // Keep the slider in sync with firmware-pushed elapsed time when
            // the user is not actively dragging.
            seekValue = Double(newValue)
        }
    }

    // MARK: - Now Playing Section

    @ViewBuilder
    private var nowPlayingSection: some View {
        Section("Now Playing") {
            if let show = showVM.currentShow {
                VStack(alignment: .leading, spacing: 8) {
                    Text(show.name)
                        .font(.headline)
                        .foregroundStyle(Color.lwTextPrimary)

                    Text(playbackStateLabel)
                        .font(.caption)
                        .foregroundStyle(Color.lwTextSecondary)

                    transportControls

                    if showVM.durationMs > 0 {
                        seekSlider
                    }
                }
                .padding(.vertical, 4)
            } else {
                Text("Nothing is playing")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
            }
        }
        .listRowBackground(Color.lwCard)
    }

    @ViewBuilder
    private var transportControls: some View {
        HStack(spacing: 24) {
            Button {
                Task { @MainActor [weak showVM] in
                    guard let vm = showVM else { return }
                    if vm.playbackState == .paused {
                        await vm.resume()
                    } else if let id = vm.currentShow?.id {
                        await vm.play(showId: id)
                    }
                }
            } label: {
                Image(systemName: "play.fill")
                    .font(.title2)
            }
            .disabled(showVM.currentShow == nil || showVM.playbackState == .playing)
            .accessibilityLabel("Play")

            Button {
                Task { @MainActor [weak showVM] in
                    await showVM?.pause()
                }
            } label: {
                Image(systemName: "pause.fill")
                    .font(.title2)
            }
            .disabled(showVM.playbackState != .playing)
            .accessibilityLabel("Pause")

            Button {
                Task { @MainActor [weak showVM] in
                    await showVM?.stop()
                }
            } label: {
                Image(systemName: "stop.fill")
                    .font(.title2)
            }
            .disabled(showVM.playbackState == .idle || showVM.playbackState == .stopped)
            .accessibilityLabel("Stop")
        }
        .frame(maxWidth: .infinity, alignment: .center)
        .foregroundStyle(Color.lwTextPrimary)
        .buttonStyle(.plain)
    }

    @ViewBuilder
    private var seekSlider: some View {
        VStack(alignment: .leading, spacing: 4) {
            Slider(
                value: $seekValue,
                in: 0...Double(max(1, showVM.durationMs)),
                onEditingChanged: { editing in
                    if !editing {
                        // Dispatch the seek when the user releases the thumb.
                        let target = Int(seekValue)
                        Task { @MainActor [weak showVM] in
                            await showVM?.seek(toMillis: target)
                        }
                    }
                }
            )
            .tint(Color.lwGold)

            HStack {
                Text(formatTime(ms: Int(seekValue)))
                Spacer()
                Text(formatTime(ms: showVM.durationMs))
            }
            .font(.caption2)
            .foregroundStyle(Color.lwTextSecondary)
        }
    }

    private var playbackStateLabel: String {
        switch showVM.playbackState {
        case .idle: return "Idle"
        case .loading: return "Loading…"
        case .playing: return "Playing"
        case .paused: return "Paused"
        case .stopped: return "Stopped"
        }
    }

    private func formatTime(ms: Int) -> String {
        let totalSec = max(0, ms) / 1000
        let m = totalSec / 60
        let s = totalSec % 60
        return String(format: "%d:%02d", m, s)
    }

    // MARK: - Shows List Section

    @ViewBuilder
    private var showsSection: some View {
        Section("All Shows") {
            if showVM.shows.isEmpty {
                Text("No shows uploaded")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
            } else {
                ForEach(showVM.shows) { show in
                    Button {
                        Task { @MainActor [weak showVM] in
                            await showVM?.play(showId: show.id)
                        }
                    } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(show.name)
                                    .font(.body)
                                    .foregroundStyle(Color.lwTextPrimary)
                                HStack(spacing: 6) {
                                    Text(show.builtin ? "Built-in" : "Uploaded")
                                    if show.looping {
                                        Text("• Loops")
                                    }
                                    Text("• \(formatTime(ms: show.durationMs))")
                                }
                                .font(.caption2)
                                .foregroundStyle(Color.lwTextSecondary)
                            }
                            Spacer()
                            if show.id == showVM.currentShow?.id {
                                Image(systemName: "speaker.wave.2.fill")
                                    .foregroundStyle(Color.lwGold)
                            }
                        }
                    }
                    .buttonStyle(.plain)
                }
            }
        }
        .listRowBackground(Color.lwCard)
    }
}

// MARK: - Preview

#Preview("Shows View") {
    NavigationStack {
        ShowsView()
            .environment(AppViewModel())
    }
}
