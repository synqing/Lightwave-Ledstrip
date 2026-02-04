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

    // MARK: - Stream Diagnostics

    /// Total audio frames received since launch
    var audioFrameCount: Int = 0

    /// Timestamp of last audio frame
    var lastAudioFrameAt: Date?

    // MARK: - Telemetry Polling (HTTP Fallback)

    private var fftPollTask: Task<Void, Never>?
    private var tempoPollTask: Task<Void, Never>?
    private var statePollTask: Task<Void, Never>?
    private var parametersPollTask: Task<Void, Never>?

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
    private var audioTuningDebounceTask: Task<Void, Never>?

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
        audioFrameCount = 0
        lastAudioFrameAt = nil
        stopTelemetryPolling()
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
        audioFrameCount += 1
        lastAudioFrameAt = Date()

        // Update current values
        bands = frame.bands
        rms = frame.rms
        flux = frame.flux
        rmsRaw = frame.fastRMS
        rmsMapped = frame.rms
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

    // MARK: - Telemetry Polling

    /// Starts periodic REST polling to keep Audio tab visualisations functional even when WS streaming is unavailable.
    ///
    /// Rate budget: keep under firmware HTTP limit (~20 req/sec). Current plan ~9.5 req/sec:
    /// - FFT: 6 Hz
    /// - Tempo: 2 Hz
    /// - State: 1 Hz
    /// - Parameters: 0.5 Hz
    func startTelemetryPolling() {
        stopTelemetryPolling()

        fftPollTask = Task { @MainActor [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                guard let client = self.restClient else {
                    try? await Task.sleep(for: .seconds(1))
                    continue
                }
                do {
                    let resp = try await client.getAudioFFT()
                    self.applyFFT(resp.data)
                } catch {
                    // Keep last known values; avoid spamming UI.
                }
                try? await Task.sleep(for: .milliseconds(166)) // ~6 Hz
            }
        }

        tempoPollTask = Task { @MainActor [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                guard let client = self.restClient else {
                    try? await Task.sleep(for: .seconds(1))
                    continue
                }
                do {
                    let resp = try await client.getAudioTempo()
                    self.currentBpm = resp.data.bpm
                    // Prefer control-bus confidence if available; tempo endpoint is a good fallback.
                    self.bpmConfidence = resp.data.confidence
                } catch {
                    // Ignore transient failures.
                }
                try? await Task.sleep(for: .milliseconds(500)) // 2 Hz
            }
        }

        statePollTask = Task { @MainActor [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                guard let client = self.restClient else {
                    try? await Task.sleep(for: .seconds(1))
                    continue
                }
                do {
                    let resp = try await client.getAudioState()
                    if let cb = resp.data.controlBus {
                        self.isSilent = cb.isSilent
                        self.silentScale = Float(cb.silentScale)
                        self.tempoLocked = cb.tempoLocked
                        self.bpmConfidence = cb.tempoConfidence
                    }
                } catch {
                    // Ignore transient failures.
                }
                try? await Task.sleep(for: .seconds(1)) // 1 Hz
            }
        }

        parametersPollTask = Task { @MainActor [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                guard let client = self.restClient else {
                    try? await Task.sleep(for: .seconds(1))
                    continue
                }
                do {
                    let resp = try await client.getAudioTuning()
                    if let state = resp.data.state {
                        if let v = state.noiseFloor { self.noiseFloor = Float(v) }
                        if let v = state.clipCount { self.clipCount = v }
                        if let v = state.fluxMapped { self.flux = Float(v) }
                        if let v = state.rmsRaw { self.rmsRaw = Float(v) }
                        if let v = state.rmsMapped { self.rmsMapped = Float(v) }
                        // Use mapped RMS as the main RMS readout on the Energy card.
                        self.rms = self.rmsMapped
                    }
                } catch {
                    // Ignore transient failures.
                }
                try? await Task.sleep(for: .seconds(2)) // 0.5 Hz
            }
        }
    }

    func stopTelemetryPolling() {
        fftPollTask?.cancel()
        fftPollTask = nil
        tempoPollTask?.cancel()
        tempoPollTask = nil
        statePollTask?.cancel()
        statePollTask = nil
        parametersPollTask?.cancel()
        parametersPollTask = nil
    }

    private func applyFFT(_ data: AudioFFTResponse.AudioFFTData) {
        audioFrameCount += 1
        lastAudioFrameAt = Date()

        rmsRaw = Float(data.rmsRaw)
        rmsMapped = Float(data.rmsMapped)
        rms = rmsMapped

        // Bands are already smoothed by the firmware's control bus.
        let nextBands = data.bands.prefix(8).map { Float($0) }
        if nextBands.count == 8 {
            bands = nextBands
            ringBuffer[writeRow] = nextBands
            writeRow = (writeRow + 1) % 60
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

    // MARK: - Audio DSP Tuning

    func loadAudioTuning() async {
        guard let client = restClient else { return }

        do {
            let response = try await client.getAudioTuning()
            let pipeline = response.data.pipeline
            let controlBus = response.data.controlBus
            let novelty = pipeline.novelty

            if let alphaFast = controlBus.alphaFast { self.alphaFast = alphaFast }
            if let alphaSlow = controlBus.alphaSlow { self.alphaSlow = alphaSlow }

            if let bandAttack = pipeline.bandAttack { self.bandAttack = bandAttack }
            if let bandRelease = pipeline.bandRelease { self.bandRelease = bandRelease }
            if let heavyAttack = pipeline.heavyBandAttack { self.heavyAttack = heavyAttack }
            if let heavyRelease = pipeline.heavyBandRelease { self.heavyRelease = heavyRelease }

            if let silenceHysteresisMs = pipeline.silenceHysteresisMs { self.silenceHysteresis = silenceHysteresisMs }
            if let silenceThreshold = pipeline.silenceThreshold { self.silenceThreshold = silenceThreshold }

            if let useSpectralFlux = novelty?.useSpectralFlux { self.spectralFluxEnabled = useSpectralFlux }
            if let fluxScale = novelty?.spectralFluxScale { self.fluxScale = fluxScale }

        } catch {
            print("Error loading audio tuning: \(error)")
        }
    }

    func queueAudioTuningUpdate() {
        guard let client = restClient else { return }

        audioTuningDebounceTask?.cancel()

        let payload: [String: Any] = [
            "controlBus": [
                "alphaFast": alphaFast,
                "alphaSlow": alphaSlow
            ],
            "pipeline": [
                "bandAttack": bandAttack,
                "bandRelease": bandRelease,
                "heavyBandAttack": heavyAttack,
                "heavyBandRelease": heavyRelease,
                "silenceHysteresisMs": silenceHysteresis,
                "silenceThreshold": silenceThreshold
            ],
            "novelty": [
                "useSpectralFlux": spectralFluxEnabled,
                "spectralFluxScale": fluxScale
            ]
        ]

        audioTuningDebounceTask = Task {
            do {
                try await Task.sleep(nanoseconds: 150_000_000) // 150ms
                guard !Task.isCancelled else { return }

                try await client.patchAudioTuning(payload)

            } catch is CancellationError {
                // Cancelled by newer update
            } catch {
                print("Error updating audio tuning: \(error)")
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
