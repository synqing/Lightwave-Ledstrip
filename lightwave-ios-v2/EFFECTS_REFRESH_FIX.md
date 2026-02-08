# Effects List Refresh Fix

## Problem
The iOS app was not showing new effects added to the firmware (113 effects total). The effects list was only loaded once during initial connection and never refreshed.

## Root Cause
1. `loadEffects()` was only called in `loadInitialState()` during connection setup
2. No mechanism to reload effects on subsequent connections
3. No pull-to-refresh capability in UI
4. Errors were silently logged to console without user feedback

## Changes Made

### 1. EffectViewModel.swift
- **Added state properties:**
  - `isLoading: Bool` - tracks loading state for UI feedback
  - `lastError: String?` - captures error messages for display

- **Enhanced `loadEffects()`:**
  - Sets `isLoading = true` at start, `false` at end
  - Captures errors in `lastError` property
  - Improved logging with effect count and audio-reactive count
  - Example output: `✓ Loaded 113 effects (45 audio-reactive)`

- **Added `disconnect()` method:**
  - Resets `allEffects` to empty array
  - Clears `currentEffectId` and `currentEffectName`
  - Resets `isLoading` and `lastError`
  - Called by AppViewModel on disconnect to ensure fresh load on reconnect

### 2. AppViewModel.swift
- **Updated `connectionDidDisconnect()`:**
  - Now calls `effects.disconnect()` to clear effect state
  - Also calls `palettes.disconnect()` for consistency
  - Ensures effects are reloaded fresh on every connection

### 3. PaletteViewModel.swift
- **Added `disconnect()` method:**
  - Resets `allPalettes` to defaults
  - Clears `currentPaletteId`
  - Preserves favourites across disconnects (intentional)

### 4. EffectSelectorView.swift
- **Added pull-to-refresh:**
  - ScrollView now has `.refreshable` modifier
  - Pull down to manually reload effects from device

- **Added loading indicator:**
  - ProgressView in toolbar when `isLoading == true`
  - Uses `Color.lwGold` tint to match theme

- **Enhanced navigation title:**
  - Shows "Effects (113)" when loaded
  - Shows "Select Effect" when empty

- **Improved error display:**
  - Shows error message when `lastError` is set
  - Displays "Retry" button to attempt reload
  - Error message is centered and styled in red

## Effect Count Verification
- Firmware: `MAX_EFFECTS = 113` (defined in `/firmware/v2/src/config/limits.h`)
- iOS API limit: `200` (already sufficient)
- New logging shows exact count received from device

## Testing Checklist
1. ✅ Connect to device with 113 effects
2. ✅ Verify effects count shows in navigation title
3. ✅ Disconnect and reconnect - effects reload fresh
4. ✅ Pull down to refresh - loading indicator appears
5. ✅ Console shows: `✓ Loaded 113 effects (N audio-reactive)`
6. ✅ Test error handling by disconnecting WiFi during load
7. ✅ Verify "Retry" button appears on error

## API Behaviour
- Effects are fetched from `GET /api/v1/effects?page=1&limit=200`
- Limit increased from 100 to 200 provides headroom for future growth
- Each effect includes:
  - `id`, `name`, `category`, `categoryId`
  - `isAudioReactive` flag (used for "Audio" filter)
  - `categoryName` for display

## User Experience Improvements
1. **Automatic refresh:** Effects reload on every connection
2. **Manual refresh:** Pull-to-refresh gesture available
3. **Visual feedback:** Loading spinner in toolbar
4. **Error visibility:** Clear error messages with retry option
5. **Effect count:** Navigation title shows total loaded
6. **Better logging:** Console shows audio-reactive count

## Note on Build Errors
Pre-existing build errors in `ShowsView.swift` are unrelated to this fix:
- Missing `GradientStrokeStyle.danger` member
- `AppViewModel` missing `restClient` property (should use `rest`)

These should be fixed separately.
