//
//  AudioViewModel.swift
//  LightwaveOS
//
//  Audio beat tracking, BPM monitoring, and audio parameter controls.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import UIKit
import Observation

@MainActor
@Observable
class AudioViewModel {
    // MARK: - State

    var currentBpm: Double = 0
    var bpmConfidence: Double = 0
    var beatPosition: Int = 0
    var lastBeatTime: Date?
    var isBeating: Bool = false
    var isDownbeat: Bool = false

    // MARK: - Audio Parameters

    var micType: MicType = .i2s
    var gain: Double = 1.0
    var threshold: Double = 0.3
    var hapticsEnabled: Bool = true

    // MARK: - DSP Parameters

    // Control Bus Smoothing
    var alphaFast: Double = 0.35
    var alphaSlow: Double = 0.12

    // Band Envelope
    var bandAttack: Double = 0.15
    var bandRelease: Double = 0.03
    var heavyAttack: Double = 0.08
    var heavyRelease: Double = 0.02

    // Silence Detection
    var silenceHysteresis: Double = 5000
    var silenceThreshold: Double = 0.01

    // Novelty / Onset
    var spectralFluxEnabled: Bool = true
    var fluxScale: Double = 1.0

    // MARK: - Spectrum Data

    /// Current 8-band frequency values (Sub, Bass, Low, LMid, Mid, HMid, High, Air)
    var bands: [Float] = Array(repeating: 0, count: 8)

    /// Ring buffer: 60 frames of 8-band history
    var ringBuffer: [[Float]] = Array(repeating: Array(repeating: Float(0), count: 8), count: 60)

    /// Current write position in ringBuffer (0..<60)
    var writeRow: Int = 0

    /// Current RMS level
    var rms: Float = 0

    /// Current waveform (128 samples, normalised -1...1)
    var waveform: [Float] = Array(repeating: 0, count: 128)

    // MARK: - BPM Properties

    var bpm: Float = 0

    // MARK: - DSP Metrics (Read-Only)

    /// Noise floor measurement
    var noiseFloor: Float = 0.0004

    /// Clip count
    var clipCount: Int = 0

    /// Tempo locked indicator
    var tempoLocked: Bool = false

    /// Silence detection state
    var isSilent: Bool = false

    /// Silent scale factor
    var silentScale: Float = 1.0

    /// Raw RMS before mapping
    var rmsRaw: Float = 0

    /// Mapped RMS after transformation
    var rmsMapped: Float = 0

    /// Spectral flux value
    var flux: Float = 0

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - Haptic Feedback

    private let hapticMedium = UIImpactFeedbackGenerator(style: .medium)
    private let hapticHeavy = UIImpactFeedbackGenerator(style: .heavy)

    // MARK: - Beat Reset Tasks

    private var beatResetTask: Task<Void, Never>?
    private var downbeatResetTask: Task<Void, Never>?

    // MARK: - Debounce Tasks

    private var audioParamsDebounceTask: Task<Void, Never>?

    // MARK: - Initialization

    init() {
        // Prepare haptic generators
        hapticMedium.prepare()
        hapticHeavy.prepare()
    }

    // MARK: - Disconnect Cleanup

    func reset() {
        beatResetTask?.cancel()
        downbeatResetTask?.cancel()
        isBeating = false
        isDownbeat = false
    }

    // MARK: - Beat Event Handling

    func handleBeatEvent(_ data: [String: Any]) {
        guard let type = data["type"] as? String else { return }

        lastBeatTime = Date()

        switch type {
        case "tick":
            handleRegularBeat()

        case "downbeat":
            handleDownbeat()

        default:
            break
        }

        // Update BPM if provided
        if let bpm = data["bpm"] as? Double {
            currentBpm = bpm
        }

        if let confidence = data["confidence"] as? Double {
            bpmConfidence = confidence
        }

        if let position = data["position"] as? Int {
            beatPosition = position
        }
    }

    /// Overload for binary audio metrics frame
    func handleBeatEvent(type: String, bpm: Float, confidence: Float, position: Int?) {
        lastBeatTime = Date()

        switch type {
        case "tick":
            handleRegularBeat()

        case "downbeat":
            handleDownbeat()

        default:
            break
        }

        currentBpm = Double(bpm)
        self.bpmConfidence = Double(confidence)
        if let pos = position {
            beatPosition = pos
        }
    }

    // MARK: - Audio Metrics Frame Handling

    /// Process a binary audio metrics frame from the WebSocket
    func handleMetricsFrame(_ frame: AudioMetricsFrame) {
        // Update current values
        bands = frame.bands
        rms = frame.rms
        waveform = frame.waveform
        bpm = frame.bpmSmoothed
        self.bpmConfidence = Double(frame.tempoConfidence)

        // Push bands into ring buffer
        ringBuffer[writeRow] = frame.bands
        writeRow = (writeRow + 1) % 60

        // Handle beat tick (trigger haptic on beat)
        if frame.beatTick > 0 {
            handleBeatEvent(type: "tick", bpm: frame.bpmSmoothed, confidence: frame.tempoConfidence, position: nil)
        }
        if frame.downbeatTick > 0 {
            handleBeatEvent(type: "downbeat", bpm: frame.bpmSmoothed, confidence: frame.tempoConfidence, position: nil)
        }
    }

    func updateFromStatus(bpm: Double, confidence: Double) {
        currentBpm = bpm
        bpmConfidence = confidence
    }

    // MARK: - Audio Parameters

    func setAudioParams() {
        guard let client = restClient else { return }

        // Cancel previous debounce task
        audioParamsDebounceTask?.cancel()

        // Debounce the network update (150ms)
        audioParamsDebounceTask = Task {
            do {
                try await Task.sleep(nanoseconds: 150_000_000) // 150ms
                guard !Task.isCancelled else { return }

                try await client.setAudioParameters(
                    gain: gain,
                    threshold: threshold,
                    micType: micType.firmwareValue
                )

            } catch is CancellationError {
                // Cancelled by newer update
            } catch {
                print("Error updating audio params: \(error)")
            }
        }
    }

    // MARK: - Private Beat Handlers

    private func handleRegularBeat() {
        // Trigger visual feedback
        isBeating = true

        // Fire medium haptic (gated by preference and confidence)
        if hapticsEnabled && bpmConfidence > 0.4 {
            hapticMedium.impactOccurred()
        }

        // Reset visual after 150ms
        beatResetTask?.cancel()
        beatResetTask = Task { @MainActor in
            do {
                try await Task.sleep(nanoseconds: 150_000_000) // 150ms
                guard !Task.isCancelled else { return }

                self.isBeating = false
                if self.hapticsEnabled { hapticMedium.prepare() }
            } catch {
                // Cancellation is expected
            }
        }
    }

    private func handleDownbeat() {
        // Trigger stronger visual feedback
        isBeating = true
        isDownbeat = true

        // Fire heavy haptic (gated by preference and confidence)
        if hapticsEnabled && bpmConfidence > 0.4 {
            hapticHeavy.impactOccurred()
        }

        // Reset downbeat visual after 200ms
        downbeatResetTask?.cancel()
        downbeatResetTask = Task { @MainActor in
            do {
                try await Task.sleep(nanoseconds: 200_000_000) // 200ms
                guard !Task.isCancelled else { return }

                self.isDownbeat = false

                // Regular beat resets after downbeat ends
                try await Task.sleep(nanoseconds: 100_000_000) // Additional 100ms
                guard !Task.isCancelled else { return }

                self.isBeating = false
                if self.hapticsEnabled { hapticHeavy.prepare() }
            } catch {
                // Cancellation is expected
            }
        }
    }
}

// MARK: - Mic Type

enum MicType: String, Sendable, CaseIterable, Identifiable {
    case analog = "Analog"
    case i2s = "I2S"
    case pdm = "PDM"

    var id: String { rawValue }

    var displayName: String {
        rawValue
    }

    /// Firmware integer value for REST API
    var firmwareValue: Int {
        switch self {
        case .analog: return 0
        case .i2s: return 1
        case .pdm: return 2
        }
    }
}
