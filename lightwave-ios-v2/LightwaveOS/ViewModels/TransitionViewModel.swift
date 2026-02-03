//
//  TransitionViewModel.swift
//  LightwaveOS
//
//  Transition controls for effect changes.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation

@MainActor
@Observable
class TransitionViewModel {
    // MARK: - State

    var currentType: TransitionType = .crossfade
    var duration: Int = 1200 // milliseconds

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - Available Types

    var availableTypes: [TransitionType] {
        TransitionType.allCases
    }

    // MARK: - API Methods

    func triggerTransition(toEffect effectId: Int? = nil) async {
        guard let client = restClient else { return }

        do {
            try await client.triggerTransition(
                toEffect: effectId ?? -1,
                type: currentType.rawValue,
                duration: duration
            )
        } catch {
            print("Error triggering transition: \(error)")
        }
    }

    func setType(_ type: TransitionType) {
        currentType = type
    }

    func setDuration(_ milliseconds: Int) {
        duration = max(200, min(5000, milliseconds))
    }
}
