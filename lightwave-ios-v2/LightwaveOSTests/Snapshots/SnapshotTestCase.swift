//
//  SnapshotTestCase.swift
//  LightwaveOSTests
//
//  Base class for snapshot testing with LightwaveOS design system integration.
//  Provides consistent dark mode background and recording mode support.
//

import XCTest
import SwiftUI
import SnapshotTesting
@testable import LightwaveOS

/// Base test case for snapshot testing with LightwaveOS theme.
///
/// Set `SNAPSHOT_RECORD=1` in scheme environment variables to record new snapshots.
/// Recorded snapshots are saved in `__Snapshots__/<TestName>/<testMethod>.png`
class SnapshotTestCase: XCTestCase {

    // MARK: - Configuration

    /// Recording mode - set via environment variable SNAPSHOT_RECORD=1
    var isRecording: Bool {
        ProcessInfo.processInfo.environment["SNAPSHOT_RECORD"] == "1"
    }

    /// Device configuration for consistent snapshot sizing
    let deviceConfig: ViewImageConfig = .iPhone15Pro

    // MARK: - Setup

    override func setUp() {
        super.setUp()

        // Disable animations for deterministic snapshots
        UIView.setAnimationsEnabled(false)
    }

    override func tearDown() {
        // Re-enable animations
        UIView.setAnimationsEnabled(true)

        super.tearDown()
    }

    // MARK: - Assertion Helpers

    /// Assert snapshot of a SwiftUI view with dark mode LightwaveOS background.
    ///
    /// - Parameters:
    ///   - view: The SwiftUI view to snapshot
    ///   - name: Optional test name suffix (default: function name)
    ///   - file: Source file (default: current file)
    ///   - testName: Test function name (default: current function)
    ///   - line: Source line (default: current line)
    func assertSnapshot<V: View>(
        of view: V,
        named name: String? = nil,
        file: StaticString = #file,
        testName: String = #function,
        line: UInt = #line
    ) {
        // Wrap view in dark mode with lwBase background
        let wrappedView = view
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color.lwBase)
            .preferredColorScheme(.dark)

        // Take snapshot with iPhone 15 Pro layout
        let hostingController = UIHostingController(rootView: wrappedView)

        SnapshotTesting.assertSnapshot(
            matching: hostingController,
            as: .image(on: deviceConfig),
            named: name,
            record: isRecording,
            file: file,
            testName: testName,
            line: line
        )
    }

    /// Assert snapshot of a SwiftUI view with custom frame size.
    ///
    /// Useful for testing individual components at specific dimensions.
    ///
    /// - Parameters:
    ///   - view: The SwiftUI view to snapshot
    ///   - width: Frame width
    ///   - height: Frame height
    ///   - name: Optional test name suffix
    ///   - file: Source file (default: current file)
    ///   - testName: Test function name (default: current function)
    ///   - line: Source line (default: current line)
    func assertSnapshot<V: View>(
        of view: V,
        width: CGFloat,
        height: CGFloat,
        named name: String? = nil,
        file: StaticString = #file,
        testName: String = #function,
        line: UInt = #line
    ) {
        // Wrap view in fixed frame with dark mode and lwBase background
        let wrappedView = view
            .frame(width: width, height: height)
            .background(Color.lwBase)
            .preferredColorScheme(.dark)

        let hostingController = UIHostingController(rootView: wrappedView)

        SnapshotTesting.assertSnapshot(
            matching: hostingController,
            as: .image(
                on: .init(
                    safeArea: .zero,
                    size: CGSize(width: width, height: height),
                    traits: .init(userInterfaceStyle: .dark)
                )
            ),
            named: name,
            record: isRecording,
            file: file,
            testName: testName,
            line: line
        )
    }
}
