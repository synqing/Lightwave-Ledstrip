//
//  WebSocketInspectorView.swift
//  LightwaveOS
//
//  Real-time WebSocket frame inspector. Lists captured frames newest-first
//  with chip-based filters for direction, message type and a deviceId
//  search field plus timestamp range pickers.
//
//  iOS 17+, Swift 6 with @Observable @MainActor. British English comments.
//

import SwiftUI

// MARK: - WSInspectorViewModel

/// Drives the inspector view. Holds the most-recent snapshot of frames and
/// the active filter spec. Subscribes to `WSInspector.shared` on `attach()`
/// and detaches on `detach()`. The view binds via `@Bindable` so chip /
/// search edits update the filter live.
@MainActor
@Observable
final class WSInspectorViewModel {

    // MARK: - State

    /// All captured frames, oldest → newest. The view inverts this for
    /// display so the user sees the newest at the top.
    private(set) var frames: [WSFrame] = []

    /// Active filter — chip selections, search field, date pickers all
    /// mutate fields on this struct.
    var filter = WSInspectorFilter()

    /// Toggles whether the live subscription feeds new frames into the
    /// view. Pausing makes scrolling through history readable.
    var isPaused: Bool = false

    /// Single-frame focus for the detail sheet.
    var focusedFrame: WSFrame?

    // MARK: - Dependencies

    /// Inspector instance backing this view. Defaults to the shared
    /// singleton; tests inject a fresh actor.
    private let inspector: WSInspector

    /// Active subscription task. Cancelled on `detach()` and on deinit.
    private var streamTask: Task<Void, Never>?

    // MARK: - Init

    init(inspector: WSInspector = .shared) {
        self.inspector = inspector
    }

    // MARK: - Lifecycle

    /// Attach to the inspector actor: pull the current snapshot, then
    /// subscribe to the live stream. Safe to call multiple times — the
    /// previous subscription is cancelled first.
    func attach() {
        streamTask?.cancel()
        streamTask = Task { [weak self] in
            guard let self = self else { return }
            // Initial snapshot.
            let initial = await self.inspector.snapshot()
            self.frames = initial

            // Live updates.
            for await frame in await self.inspector.subscribe() {
                if Task.isCancelled { break }
                if self.isPaused { continue }
                self.frames.append(frame)
                // Bound the in-memory copy to the inspector's buffer
                // limit so the view doesn't outgrow the source of truth
                // when the user pauses then resumes during heavy traffic.
                let limit = self.inspector.bufferLimit
                if self.frames.count > limit {
                    self.frames.removeFirst(self.frames.count - limit)
                }
            }
        }
    }

    /// Cancel the live subscription. Called from `onDisappear`.
    func detach() {
        streamTask?.cancel()
        streamTask = nil
    }

    /// Drop every captured frame from both the inspector and this view's
    /// snapshot. Bound to the toolbar "Clear" button.
    func clearAll() {
        let inspector = self.inspector
        Task { [weak self] in
            await inspector.clear()
            await MainActor.run { [weak self] in
                self?.frames.removeAll()
            }
        }
    }

    // MARK: - Filtered output

    /// Filtered + reversed view of `frames`, newest first.
    var filteredFrames: [WSFrame] {
        let filtered = filter.apply(to: frames)
        return Array(filtered.reversed())
    }

    /// Distinct typeLabels currently in the buffer. Used to populate filter
    /// chips so the user only sees labels that actually appear in the
    /// captured traffic.
    var availableTypeLabels: [String] {
        var seen: Set<String> = []
        var ordered: [String] = []
        for frame in frames {
            if seen.insert(frame.typeLabel).inserted {
                ordered.append(frame.typeLabel)
            }
        }
        return ordered.sorted()
    }
}

// MARK: - WebSocketInspectorView

struct WebSocketInspectorView: View {

    @State private var viewModel = WSInspectorViewModel()

    var body: some View {
        VStack(spacing: 0) {
            filterBar
                .padding(.horizontal, Spacing.md)
                .padding(.vertical, Spacing.sm)
                .background(Color.lwBase)

            Divider()
                .background(Color.lwTextTertiary.opacity(0.3))

            frameList
        }
        .background(Color.lwBase)
        .navigationTitle("WebSocket Inspector")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Menu {
                    Button(viewModel.isPaused ? "Resume capture" : "Pause capture") {
                        viewModel.isPaused.toggle()
                    }
                    Button("Clear", role: .destructive) {
                        viewModel.clearAll()
                    }
                } label: {
                    Image(systemName: "ellipsis.circle")
                        .foregroundStyle(Color.lwGold)
                }
            }
        }
        .sheet(item: Binding(
            get: { viewModel.focusedFrame },
            set: { viewModel.focusedFrame = $0 }
        )) { frame in
            FrameDetailSheet(frame: frame)
        }
        .onAppear { viewModel.attach() }
        .onDisappear { viewModel.detach() }
    }

    // MARK: - Filter bar

    @ViewBuilder
    private var filterBar: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            // Direction chips
            HStack(spacing: Spacing.sm) {
                Text("DIR")
                    .font(.microLabel)
                    .foregroundStyle(Color.lwTextTertiary)
                directionChip(label: "All", value: nil)
                directionChip(label: "In", value: .inbound)
                directionChip(label: "Out", value: .outbound)
                Spacer()
                if viewModel.isPaused {
                    Label("PAUSED", systemImage: "pause.fill")
                        .font(.microLabel)
                        .foregroundStyle(Color.lwError)
                }
            }

            // Type label chips. Limited to currently observed labels so the
            // chip strip does not balloon.
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: Spacing.xs) {
                    Text("TYPE")
                        .font(.microLabel)
                        .foregroundStyle(Color.lwTextTertiary)
                    typeChip(label: "Any", typeLabel: nil)
                    ForEach(viewModel.availableTypeLabels, id: \.self) { label in
                        typeChip(label: label, typeLabel: label)
                    }
                }
            }

            // Device ID search
            HStack(spacing: Spacing.sm) {
                Image(systemName: "magnifyingglass")
                    .font(.iconSmall)
                    .foregroundStyle(Color.lwTextTertiary)
                TextField("deviceId search", text: Binding(
                    get: { viewModel.filter.deviceIdSearch ?? "" },
                    set: { viewModel.filter.deviceIdSearch = $0.isEmpty ? nil : $0 }
                ))
                .font(.monospace)
                .foregroundStyle(Color.lwTextPrimary)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled(true)
            }
            .padding(.horizontal, Spacing.sm)
            .padding(.vertical, 6)
            .background(Color.lwCard, in: RoundedRectangle(cornerRadius: 8))

            // Timestamp range
            HStack(spacing: Spacing.sm) {
                DatePicker(
                    "From",
                    selection: Binding(
                        get: { viewModel.filter.startTime ?? Date.distantPast },
                        set: { viewModel.filter.startTime = ($0 == Date.distantPast) ? nil : $0 }
                    ),
                    displayedComponents: [.hourAndMinute]
                )
                .labelsHidden()
                .datePickerStyle(.compact)

                Text("→")
                    .foregroundStyle(Color.lwTextTertiary)

                DatePicker(
                    "To",
                    selection: Binding(
                        get: { viewModel.filter.endTime ?? Date.distantFuture },
                        set: { viewModel.filter.endTime = ($0 == Date.distantFuture) ? nil : $0 }
                    ),
                    displayedComponents: [.hourAndMinute]
                )
                .labelsHidden()
                .datePickerStyle(.compact)

                Spacer()

                if !viewModel.filter.isEmpty {
                    Button("Reset") {
                        viewModel.filter = WSInspectorFilter()
                    }
                    .font(.caption)
                    .foregroundStyle(Color.lwGold)
                }
            }
        }
    }

    // MARK: - Chip helpers

    @ViewBuilder
    private func directionChip(label: String, value: WSFrameDirection?) -> some View {
        let isSelected = (viewModel.filter.direction == value)
        Button {
            viewModel.filter.direction = value
        } label: {
            Text(label)
                .font(.pillLabel)
                .padding(.horizontal, Spacing.sm)
                .padding(.vertical, 4)
                .background(isSelected ? Color.lwGold : Color.lwCard, in: Capsule())
                .foregroundStyle(isSelected ? Color.lwBase : Color.lwTextPrimary)
        }
        .buttonStyle(.plain)
    }

    @ViewBuilder
    private func typeChip(label: String, typeLabel: String?) -> some View {
        let isSelected: Bool = {
            if let typeLabel = typeLabel {
                return viewModel.filter.typeLabels.contains(typeLabel)
            }
            return viewModel.filter.typeLabels.isEmpty
        }()

        Button {
            if let typeLabel = typeLabel {
                if viewModel.filter.typeLabels.contains(typeLabel) {
                    viewModel.filter.typeLabels.remove(typeLabel)
                } else {
                    viewModel.filter.typeLabels.insert(typeLabel)
                }
            } else {
                viewModel.filter.typeLabels.removeAll()
            }
        } label: {
            Text(label)
                .font(.caption)
                .padding(.horizontal, Spacing.sm)
                .padding(.vertical, 3)
                .background(isSelected ? Color.lwGold : Color.lwCard, in: Capsule())
                .foregroundStyle(isSelected ? Color.lwBase : Color.lwTextPrimary)
        }
        .buttonStyle(.plain)
    }

    // MARK: - Frame list

    @ViewBuilder
    private var frameList: some View {
        if viewModel.filteredFrames.isEmpty {
            VStack {
                Spacer()
                Image(systemName: "antenna.radiowaves.left.and.right.slash")
                    .font(.iconLarge)
                    .foregroundStyle(Color.lwTextTertiary)
                Text(viewModel.frames.isEmpty
                     ? "No frames captured yet"
                     : "No frames match the active filter")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextSecondary)
                    .multilineTextAlignment(.center)
                Spacer()
            }
        } else {
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 4) {
                    ForEach(viewModel.filteredFrames) { frame in
                        Button {
                            viewModel.focusedFrame = frame
                        } label: {
                            FrameRowView(frame: frame)
                        }
                        .buttonStyle(.plain)
                    }
                }
                .padding(.horizontal, Spacing.md)
                .padding(.vertical, Spacing.sm)
            }
        }
    }
}

// MARK: - Frame row view

struct FrameRowView: View {
    let frame: WSFrame

    var body: some View {
        HStack(alignment: .top, spacing: Spacing.sm) {
            directionGlyph
                .frame(width: 18)

            VStack(alignment: .leading, spacing: 2) {
                HStack {
                    Text(frame.typeLabel)
                        .font(.caption)
                        .foregroundStyle(typeColour)

                    Spacer()

                    Text(timestampString)
                        .font(.monospaceSmall)
                        .foregroundStyle(Color.lwTextTertiary)
                        .monospacedDigit()
                }

                Text(frame.summary)
                    .font(.monospaceSmall)
                    .foregroundStyle(Color.lwTextSecondary)
                    .lineLimit(2)

                if let deviceId = frame.deviceId {
                    Text("device: \(deviceId)")
                        .font(.microLabel)
                        .foregroundStyle(Color.lwTextTertiary)
                }
            }
        }
        .padding(Spacing.sm)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.lwCard, in: RoundedRectangle(cornerRadius: 8))
    }

    private var directionGlyph: some View {
        Group {
            switch frame.direction {
            case .inbound:
                Image(systemName: "arrow.down.left")
                    .foregroundStyle(Color.lwSuccess)
            case .outbound:
                Image(systemName: "arrow.up.right")
                    .foregroundStyle(Color.lwGold)
            }
        }
        .font(.iconSmall)
    }

    private var typeColour: Color {
        if frame.messageType == .unknown {
            return Color.lwCyan
        }
        return Color.lwTextPrimary
    }

    private var timestampString: String {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss.SSS"
        return formatter.string(from: frame.timestamp)
    }
}

// MARK: - Frame detail sheet

struct FrameDetailSheet: View {
    let frame: WSFrame
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: Spacing.md) {
                    metadataSection
                    payloadSection
                }
                .padding(Spacing.md)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .background(Color.lwBase)
            .navigationTitle(frame.typeLabel)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") { dismiss() }
                        .foregroundStyle(Color.lwGold)
                }
            }
        }
    }

    private var metadataSection: some View {
        VStack(alignment: .leading, spacing: Spacing.xs) {
            metaRow(label: "Direction", value: frame.direction.rawValue)
            metaRow(label: "Type", value: frame.typeLabel)
            metaRow(label: "Message kind", value: frame.messageType.rawValue)
            metaRow(label: "Timestamp", value: ISO8601DateFormatter().string(from: frame.timestamp))
            if let deviceId = frame.deviceId {
                metaRow(label: "deviceId", value: deviceId)
            }
            switch frame.payload {
            case .text(let text):
                metaRow(label: "Bytes", value: "\(text.utf8.count)")
            case .binary(let byteCount, let magic):
                metaRow(label: "Bytes", value: "\(byteCount)")
                if let magic = magic {
                    metaRow(label: "Magic", value: String(format: "0x%02X", magic))
                }
            }
        }
    }

    private func metaRow(label: String, value: String) -> some View {
        HStack(alignment: .top) {
            Text(label.uppercased())
                .font(.microLabel)
                .foregroundStyle(Color.lwTextTertiary)
                .frame(width: 96, alignment: .leading)
            Text(value)
                .font(.monospace)
                .foregroundStyle(Color.lwTextPrimary)
                .textSelection(.enabled)
            Spacer()
        }
    }

    private var payloadSection: some View {
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("PAYLOAD")
                .font(.microLabel)
                .foregroundStyle(Color.lwTextTertiary)
            Text(frame.fullPayloadText)
                .font(.monospace)
                .foregroundStyle(Color.lwTextPrimary)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(Spacing.sm)
                .background(Color.lwCard, in: RoundedRectangle(cornerRadius: 8))
        }
    }
}

// MARK: - Preview

#Preview("Inspector — empty") {
    NavigationStack {
        WebSocketInspectorView()
    }
}
