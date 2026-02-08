# Snapshot Testing

Snapshot testing infrastructure for LightwaveOS iOS components using [swift-snapshot-testing](https://github.com/pointfreeco/swift-snapshot-testing).

## Overview

Snapshot tests capture images of UI components and compare them against reference images. This ensures visual consistency and catches unintended UI changes during development.

## Running Tests

### Normal Mode (Comparison)

Run tests normally to compare against existing snapshots:

```bash
# Via Xcode
⌘U (or Product > Test)

# Via command line
xcodebuild test -scheme LightwaveOS -destination 'platform=iOS Simulator,name=iPhone 15 Pro'
```

If a snapshot doesn't match, the test will fail and show the diff.

### Recording Mode (Update Snapshots)

To record new snapshots or update existing ones:

1. Edit the scheme in Xcode (`Product > Scheme > Edit Scheme...`)
2. Select `Test` action
3. Add environment variable: `SNAPSHOT_RECORD = 1`
4. Run tests (⌘U)
5. Remove the environment variable when done

Alternatively, set the environment variable in your test:

```swift
// Temporarily override in a specific test
override func setUp() {
    super.setUp()
    setenv("SNAPSHOT_RECORD", "1", 1)
}
```

## Test Structure

```
LightwaveOSTests/
├── Snapshots/
│   ├── SnapshotTestCase.swift          # Base test case with helpers
│   ├── Components/
│   │   ├── LWCardSnapshotTests.swift
│   │   ├── LWSliderSnapshotTests.swift
│   │   └── ConnectionDotSnapshotTests.swift
│   └── __Snapshots__/                   # Generated snapshot images
│       ├── LWCardSnapshotTests/
│       ├── LWSliderSnapshotTests/
│       └── ConnectionDotSnapshotTests/
```

## Writing Snapshot Tests

### Basic Example

```swift
import XCTest
import SwiftUI
@testable import LightwaveOS

final class MyComponentSnapshotTests: SnapshotTestCase {

    func testDefaultState() {
        let view = MyComponent()
            .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 375,
            height: 200,
            named: "default-state"
        )
    }
}
```

### Full-Screen Layout

```swift
func testFullScreen() {
    let view = MyView()

    // Uses iPhone 15 Pro device config
    assertSnapshot(of: view, named: "full-screen")
}
```

### Custom Sizing

```swift
func testComponentSize() {
    let view = MyComponent()

    assertSnapshot(
        of: view,
        width: 200,
        height: 100,
        named: "small-size"
    )
}
```

### Testing Components with @Binding

Use the `StatefulWrapper` helper:

```swift
func testSliderValue() {
    let view = StatefulWrapper(value: 128.0) { binding in
        LWSlider(
            title: "Brightness",
            value: binding,
            range: 0...255,
            step: 1
        )
    }
    .padding(Spacing.lg)

    assertSnapshot(of: view, width: 375, height: 120)
}
```

## Device Configuration

All snapshots use **iPhone 15 Pro** configuration for consistent sizing:
- Display size: 393 × 852 points
- Dark mode
- lwBase background color

## Best Practices

### 1. Disable Animations
Animations are automatically disabled in `SnapshotTestCase.setUp()` for deterministic snapshots.

### 2. Test Multiple States
Test components in various states to catch edge cases:

```swift
func testSliderAtMinimum() { /* ... */ }
func testSliderAtMedium() { /* ... */ }
func testSliderAtMaximum() { /* ... */ }
```

### 3. Use Semantic Naming
Name snapshots descriptively:
- ✅ `slider-brightness-high`
- ✅ `card-zone0-elevated`
- ❌ `test1`
- ❌ `snapshot`

### 4. Add Padding for Context
Include padding around components to show shadows and glows:

```swift
let view = MyComponent()
    .padding(Spacing.lg)  // Shows card shadows properly
```

### 5. Test in Context
Test components both in isolation and within containers:

```swift
func testCardAlone() { /* test card by itself */ }
func testCardInStack() { /* test card in VStack */ }
```

## Troubleshooting

### Test Fails with "Snapshot does not exist"
Run in recording mode to generate the initial snapshot.

### Test Fails with Image Diff
Visual change detected. Options:
1. If change is intended: run in recording mode to update
2. If change is unintended: fix the component
3. View the diff in the test failure details

### Snapshots Look Different on CI
Ensure CI uses the same simulator device (iPhone 15 Pro) and iOS version.

### Animation Artifacts in Snapshots
Verify `UIView.setAnimationsEnabled(false)` is called in test setup.

## Example Test Patterns

### Testing Card Variants

```swift
func testCardWithTitle() { /* ... */ }
func testCardWithoutTitle() { /* ... */ }
func testCardElevated() { /* ... */ }
func testCardWithZoneBorder() { /* ... */ }
```

### Testing Slider States

```swift
func testSliderAtMin() { /* value = 0 */ }
func testSliderAtMid() { /* value = 128 */ }
func testSliderAtMax() { /* value = 255 */ }
func testSliderWithGradient() { /* custom gradient */ }
func testSliderWithLabels() { /* endpoint labels */ }
```

### Testing Connection States

```swift
func testConnectionIdle() { /* state: .idle */ }
func testConnectionTargeting() { /* state: .targeting */ }
func testConnectionReady() { /* state: .ready */ }
func testConnectionFailed() { /* state: .failed */ }
```

## Resources

- [swift-snapshot-testing GitHub](https://github.com/pointfreeco/swift-snapshot-testing)
- [Point-Free Episode: Snapshot Testing](https://www.pointfree.co/episodes/ep41-a-tour-of-snapshot-testing)
- LightwaveOS Design System: `Theme/DesignTokens.swift`
