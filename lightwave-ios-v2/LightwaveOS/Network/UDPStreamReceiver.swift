//
//  UDPStreamReceiver.swift
//  LightwaveOS
//
//  UDP datagram receiver for LED and audio binary frames from LightwaveOS firmware.
//  Demultiplexes LED (966 bytes, magic 0xFE) and audio (464 bytes, magic 0x00445541) frames.
//  iOS 17+, Swift 6 with @Observable @MainActor. British English comments.
//

import Foundation
import Network
import Observation

@MainActor
@Observable
class UDPStreamReceiver {
    // MARK: - State

    var isListening = false
    private(set) var ledFrameCount: Int = 0
    private(set) var audioFrameCount: Int = 0

    // MARK: - Callbacks

    var onLedFrame: (([UInt8]) -> Void)?
    var onAudioFrame: ((Data) -> Void)?

    // MARK: - Private

    private var listener: NWListener?
    private let port: NWEndpoint.Port = 41234
    private let queue = DispatchQueue(label: "com.lightwaveos.udp-receiver", qos: .userInteractive)

    // MARK: - Lifecycle

    func start() {
        guard listener == nil else { return }

        do {
            let params = NWParameters.udp
            params.allowLocalEndpointReuse = true
            listener = try NWListener(using: params, on: port)
        } catch {
            print("[UDP] Failed to create listener: \(error)")
            return
        }

        listener?.stateUpdateHandler = { [weak self] state in
            Task { @MainActor [weak self] in
                switch state {
                case .ready:
                    self?.isListening = true
                    print("[UDP] Listening on port \(self?.port.rawValue ?? 0)")
                case .failed(let error):
                    print("[UDP] Listener failed: \(error)")
                    self?.isListening = false
                case .cancelled:
                    self?.isListening = false
                default:
                    break
                }
            }
        }

        listener?.newConnectionHandler = { [weak self] connection in
            self?.handleConnection(connection)
        }

        listener?.start(queue: queue)
    }

    func stop() {
        listener?.cancel()
        listener = nil
        isListening = false
        ledFrameCount = 0
        audioFrameCount = 0
    }

    func reset() {
        ledFrameCount = 0
        audioFrameCount = 0
    }

    /// Wait until the UDP listener is ready (or time out).
    func waitUntilReady(timeout: TimeInterval = 2.0, pollInterval: TimeInterval = 0.05) async -> Bool {
        if isListening { return true }

        let deadline = Date().addingTimeInterval(timeout)
        while !isListening && Date() < deadline {
            let sleepMs = UInt64(max(1.0, pollInterval * 1000.0))
            try? await Task.sleep(for: .milliseconds(Int(sleepMs)))
        }

        return isListening
    }

    // MARK: - Connection Handling (nonisolated — runs on background queue)

    nonisolated private func handleConnection(_ connection: NWConnection) {
        connection.start(queue: queue)
        receiveLoop(connection)
    }

    nonisolated private func receiveLoop(_ connection: NWConnection) {
        connection.receiveMessage { [weak self] content, _, isComplete, error in
            guard let self = self else { return }

            if let data = content, !data.isEmpty {
                self.processFrame(data)
            }

            if error == nil {
                self.receiveLoop(connection)
            }
        }
    }

    nonisolated private func processFrame(_ data: Data) {
        guard data.count >= 4 else { return }

        // Check magic bytes to identify frame type
        let firstByte = data[data.startIndex]

        if firstByte == 0xFE && data.count >= 966 {
            // LED frame (magic 0xFE, 966 bytes v1 format)
            let bytes = [UInt8](data)
            Task { @MainActor [weak self] in
                self?.ledFrameCount += 1
                self?.onLedFrame?(bytes)
            }
        } else if data.count >= 464 {
            // Check for audio magic: 0x00445541 at offset 0 (little-endian)
            // Manual byte reconstruction for ARM alignment safety
            let b0 = UInt32(data[data.startIndex])
            let b1 = UInt32(data[data.startIndex + 1])
            let b2 = UInt32(data[data.startIndex + 2])
            let b3 = UInt32(data[data.startIndex + 3])
            let magic = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)
            if magic == 0x00445541 {
                let capturedData = data
                Task { @MainActor [weak self] in
                    self?.audioFrameCount += 1
                    self?.onAudioFrame?(capturedData)
                }
            }
        }
    }
}
