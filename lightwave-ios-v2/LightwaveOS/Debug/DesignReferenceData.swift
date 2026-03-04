//
//  DesignReferenceData.swift
//  LightwaveOS
//
//  Minimal design-reference data model for debug utilities.
//

import Foundation

struct DesignReferenceItem: Identifiable, Sendable {
    let id = UUID()
    let title: String
}

enum DesignReferenceData {
    static let items: [DesignReferenceItem] = []
}

