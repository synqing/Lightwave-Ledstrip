//
//  ShowsView.swift
//  LightwaveOS
//
//  Show playback and management.
//  Displays available shows with playback controls.
//

import SwiftUI

// MARK: - Shows View

struct ShowsView: View {
    // MARK: Environment

    @Environment(AppViewModel.self) private var appVM

    // MARK: State

    @State private var shows: [ShowsResponse.ShowsData.ShowMetadata] = []
    @State private var currentShow: CurrentShowResponse.CurrentShowData?
    @State private var isLoading = true
    @State private var errorMessage: String?

    // MARK: Body

    var body: some View {
        ScrollView {
            VStack(spacing: Spacing.lg) {
                // Current show status (if playing)
                if let current = currentShow, current.playing {
                    nowPlayingCard(current)
                }

                // Show list
                if isLoading {
                    loadingState
                } else if let error = errorMessage {
                    errorState(error)
                } else if shows.isEmpty {
                    emptyState
                } else {
                    showsList
                }
            }
            .padding(Spacing.md)
        }
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle("Shows")
        .navigationBarTitleDisplayMode(.inline)
        .task {
            await loadShows()
        }
        .refreshable {
            await loadShows()
        }
    }

    // MARK: - Now Playing Card

    @ViewBuilder
    private func nowPlayingCard(_ current: CurrentShowResponse.CurrentShowData) -> some View {
        GlassCard(
            title: "Now Playing",
            variant: .standard,
            strokeStyle: .accent,
            glowLevel: .subtle,
            glowColour: .lwGold
        ) {
            VStack(alignment: .leading, spacing: Spacing.md) {
                // Show name and chapter
                VStack(alignment: .leading, spacing: Spacing.xs) {
                    Text(current.showName ?? "Unknown Show")
                        .font(.headline)
                        .foregroundStyle(Color.lwTextPrimary)

                    if let chapterName = current.chapterName {
                        HStack(spacing: 4) {
                            Text("Chapter:")
                                .font(.caption)
                                .foregroundStyle(Color.lwTextTertiary)
                            Text(chapterName)
                                .font(.caption)
                                .foregroundStyle(Color.lwTextSecondary)
                        }
                    }
                }

                // Progress bar
                if let progress = current.progress {
                    VStack(alignment: .leading, spacing: Spacing.xs) {
                        GeometryReader { geometry in
                            ZStack(alignment: .leading) {
                                // Background track
                                RoundedRectangle(cornerRadius: 2)
                                    .fill(Color.lwElevated)
                                    .frame(height: 4)

                                // Progress fill
                                RoundedRectangle(cornerRadius: 2)
                                    .fill(
                                        LinearGradient(
                                            colors: [Color.lwGold, Color.lwGoldSecondary],
                                            startPoint: .leading,
                                            endPoint: .trailing
                                        )
                                    )
                                    .frame(width: geometry.size.width * progress, height: 4)
                            }
                        }
                        .frame(height: 4)

                        // Time labels
                        HStack {
                            if let elapsed = current.elapsed {
                                Text(formatTime(elapsed))
                                    .font(.caption2)
                                    .foregroundStyle(Color.lwTextSecondary)
                            }
                            Spacer()
                            if let remaining = current.remaining {
                                Text("-\(formatTime(remaining))")
                                    .font(.caption2)
                                    .foregroundStyle(Color.lwTextSecondary)
                            }
                        }
                    }
                }

                // Playback controls
                HStack(spacing: Spacing.md) {
                    // Pause/Resume button
                    Button(action: { Task { await togglePause() } }) {
                        HStack(spacing: Spacing.xs) {
                            Image(systemName: current.paused == true ? "play.fill" : "pause.fill")
                                .font(.system(size: 14, weight: .semibold))
                            Text(current.paused == true ? "Resume" : "Pause")
                                .font(.subheadline)
                                .fontWeight(.semibold)
                        }
                        .padding(.horizontal, 16)
                        .padding(.vertical, 10)
                        .background(Color.lwElevated)
                        .foregroundStyle(Color.lwGold)
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                    }

                    // Stop button
                    Button(action: { Task { await stopShow() } }) {
                        HStack(spacing: Spacing.xs) {
                            Image(systemName: "stop.fill")
                                .font(.system(size: 14, weight: .semibold))
                            Text("Stop")
                                .font(.subheadline)
                                .fontWeight(.semibold)
                        }
                        .padding(.horizontal, 16)
                        .padding(.vertical, 10)
                        .background(Color.lwElevated)
                        .foregroundStyle(Color.lwTextSecondary)
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                    }

                    Spacer()
                }
            }
        }
    }

    // MARK: - Shows List

    @ViewBuilder
    private var showsList: some View {
        VStack(spacing: Spacing.md) {
            ForEach(shows) { show in
                showCard(show)
            }
        }
    }

    @ViewBuilder
    private func showCard(_ show: ShowsResponse.ShowsData.ShowMetadata) -> some View {
        Button(action: { Task { await playShow(show) } }) {
            GlassCard(variant: .standard) {
                HStack(spacing: Spacing.md) {
                    // Play icon
                    Image(systemName: "play.circle.fill")
                        .font(.system(size: 32))
                        .foregroundStyle(Color.lwGold)

                    // Show info
                    VStack(alignment: .leading, spacing: Spacing.xs) {
                        Text(show.name)
                            .font(.headline)
                            .foregroundStyle(Color.lwTextPrimary)

                        HStack(spacing: Spacing.sm) {
                            // Duration
                            HStack(spacing: 4) {
                                Image(systemName: "clock")
                                    .font(.caption)
                                Text(show.formattedDuration)
                                    .font(.caption)
                            }
                            .foregroundStyle(Color.lwTextSecondary)

                            // Looping indicator
                            if show.looping {
                                HStack(spacing: 4) {
                                    Image(systemName: "repeat")
                                        .font(.caption)
                                    Text("Loop")
                                        .font(.caption)
                                }
                                .foregroundStyle(Color.lwCyan)
                            }

                            // Step count
                            if let stepCount = show.stepCount {
                                HStack(spacing: 4) {
                                    Image(systemName: "list.bullet")
                                        .font(.caption)
                                    Text("\(stepCount) steps")
                                        .font(.caption)
                                }
                                .foregroundStyle(Color.lwTextTertiary)
                            }
                        }
                    }

                    Spacer()

                    // Chevron
                    Image(systemName: "chevron.right")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(Color.lwTextTertiary)
                }
            }
        }
        .buttonStyle(.plain)
    }

    // MARK: - Loading/Error/Empty States

    @ViewBuilder
    private var loadingState: some View {
        GlassCard(isLoading: true) {
            VStack(spacing: Spacing.md) {
                ProgressView()
                    .tint(Color.lwGold)
                Text("Loading shows...")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextSecondary)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, Spacing.lg)
        }
    }

    @ViewBuilder
    private func errorState(_ message: String) -> some View {
        GlassCard(strokeStyle: .zone(Color.lwError)) {
            VStack(spacing: Spacing.sm) {
                Image(systemName: "exclamationmark.triangle.fill")
                    .font(.system(size: 32))
                    .foregroundStyle(Color.lwError)
                Text("Error Loading Shows")
                    .font(.headline)
                    .foregroundStyle(Color.lwTextPrimary)
                Text(message)
                    .font(.caption)
                    .foregroundStyle(Color.lwTextSecondary)
                    .multilineTextAlignment(.center)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, Spacing.lg)
        }
    }

    @ViewBuilder
    private var emptyState: some View {
        GlassCard {
            VStack(spacing: Spacing.sm) {
                Image(systemName: "sparkles")
                    .font(.system(size: 32))
                    .foregroundStyle(Color.lwTextTertiary)
                Text("No Shows Available")
                    .font(.headline)
                    .foregroundStyle(Color.lwTextPrimary)
                Text("Shows are choreographed multi-minute light sequences.")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextSecondary)
                    .multilineTextAlignment(.center)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, Spacing.lg)
        }
    }

    // MARK: - API Methods

    private func loadShows() async {
        isLoading = true
        errorMessage = nil

        do {
            guard let restClient = appVM.rest else {
                throw APIClientError.connectionFailed(NSError(domain: "ShowsView", code: -1, userInfo: [NSLocalizedDescriptionKey: "No REST client available"]))
            }

            // Load shows and current status in parallel
            async let showsResult = restClient.getShows()
            async let currentResult = restClient.getCurrentShow()

            let (showsResponse, currentResponse) = try await (showsResult, currentResult)

            shows = showsResponse.data.shows
            currentShow = currentResponse.data

            isLoading = false
        } catch {
            errorMessage = error.localizedDescription
            isLoading = false
        }
    }

    private func playShow(_ show: ShowsResponse.ShowsData.ShowMetadata) async {
        do {
            guard let restClient = appVM.rest else { return }
            try await restClient.playShow(id: show.id)
            await refreshCurrentShow()
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func togglePause() async {
        do {
            guard let restClient = appVM.rest else { return }
            try await restClient.pauseShow()
            await refreshCurrentShow()
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func stopShow() async {
        do {
            guard let restClient = appVM.rest else { return }
            try await restClient.stopShow()
            await refreshCurrentShow()
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func refreshCurrentShow() async {
        do {
            guard let restClient = appVM.rest else { return }
            let response = try await restClient.getCurrentShow()
            currentShow = response.data
        } catch {
            // Silent failure for status refresh
            currentShow = nil
        }
    }

    // MARK: - Helpers

    private func formatTime(_ milliseconds: Int) -> String {
        let seconds = milliseconds / 1000
        let minutes = seconds / 60
        let remainingSeconds = seconds % 60
        return String(format: "%d:%02d", minutes, remainingSeconds)
    }
}

// MARK: - Preview

#Preview("Shows View - With Shows") {
    NavigationStack {
        ShowsView()
            .environment(AppViewModel())
    }
}

#Preview("Shows View - Playing") {
    NavigationStack {
        ShowsView()
            .environment(AppViewModel())
    }
}
