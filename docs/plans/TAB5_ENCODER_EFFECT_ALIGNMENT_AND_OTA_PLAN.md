# Tab5.encoder Effect Alignment Review and OTA Update Implementation Plan

## Overview

This plan covers two tasks:
1. **Effect Alignment Review**: Fix misleading comments and verify Tab5.encoder is aligned with v2 firmware's new effect count (92)
2. **OTA Update Support**: Add Over-The-Air firmware update capability to Tab5.encoder

---

## Part 1: Effect Alignment Review

### Current Status

✅ **Already Functional**: Tab5.encoder dynamically updates effect ranges from server response
- Location: `firmware/Tab5.encoder/src/main.cpp` lines 839-856
- When `effects.list` response is received, extracts `pagination.total` and updates `ParameterMap` metadata
- Effect max = total - 1 (0-indexed), so 92 effects → max = 91
- Device automatically works with new 92-effect count

✅ **Safe Default Ranges**: Hardcoded ranges are conservative
- `EFFECT_MAX = 95` in `Config.h` (allows 0-95 = 96 effects)
- `MAX_EFFECTS = 256` in `main.cpp` (buffer size for effect names)
- These accommodate current and future expansion

### Issues to Fix

#### Issue 1: Misleading Comment in ParameterMap.cpp
**File**: `firmware/Tab5.encoder/src/parameters/ParameterMap.cpp`
**Line**: 23
**Current**: `// Note: Effect max=95 (0-95 = 96 effects, matches v2 MAX_EFFECTS=96)`
**Problem**: v2 firmware doesn't have `MAX_EFFECTS=96`, it has `EXPECTED_EFFECT_COUNT=92`

**Fix**:
```cpp
// Note: Effect max=95 (0-95 = 96 effects, safe default for future expansion)
// Actual max is dynamically updated from v2 server (currently 92 effects, max=91)
// v2 firmware: EXPECTED_EFFECT_COUNT = 92 (see CoreEffects.cpp)
```

#### Issue 2: Verify Config.h Comments
**File**: `firmware/Tab5.encoder/src/config/Config.h`
**Line**: 131
**Action**: Check if comment exists and update to clarify it's a default that gets updated dynamically

### Implementation Steps

1. Update `ParameterMap.cpp` comment (line 23)
2. Review and update `Config.h` comment if present
3. Verify no hardcoded effect count assumptions in UI or preset code
4. Test Tab5.encoder with v2 firmware to confirm dynamic updates work

---

## Part 2: OTA Update Support Implementation

### Current Status

❌ **Not Implemented**: Tab5.encoder has no OTA update capability
- No HTTP server (only WebSocket)
- No OTA handlers
- No Update library integration
- Uploads currently require USB/serial connection

### Architecture Overview

Tab5.encoder will implement OTA updates similar to v2 firmware:
- HTTP server for OTA endpoints (in addition to existing WebSocket)
- ESP32 `Update` library for flash writing
- Token-based authentication
- Progress feedback on display
- Dual partition support (if partition table allows)

### Implementation Plan

#### Step 1: Add HTTP Server Library
**File**: `firmware/Tab5.encoder/platformio.ini`
**Action**: Add ESPAsyncWebServer library dependency
```ini
lib_deps = 
    ...
    https://github.com/me-no-dev/ESPAsyncWebServer.git
```

#### Step 2: Create OTA Handler Module
**New Files**:
- `firmware/Tab5.encoder/src/network/OtaHandler.h`
- `firmware/Tab5.encoder/src/network/OtaHandler.cpp`

**Functionality**:
- Handle `POST /api/v1/firmware/update` endpoint
- Handle `POST /update` legacy endpoint (for curl compatibility)
- Handle `GET /api/v1/firmware/version` endpoint
- Token authentication (`X-OTA-Token` header)
- Chunked upload processing
- Progress tracking
- Error handling

**Reference**: Use `firmware/v2/src/network/webserver/handlers/FirmwareHandlers.*` as template

#### Step 3: Add HTTP Server to Main Application
**File**: `firmware/Tab5.encoder/src/main.cpp`

**Changes**:
- Include `ESPAsyncWebServer.h`
- Create `AsyncWebServer` instance (port 80)
- Register OTA endpoints
- Start server after WiFi connection
- Handle OTA progress display on Tab5 screen

**Integration Points**:
- Initialize after `g_wifiManager.connect()` succeeds
- Display update progress on Tab5 screen (use `g_ui` if available)
- Reboot after successful update

#### Step 4: Add OTA Configuration
**File**: `firmware/Tab5.encoder/src/config/network_config.h`

**Add**:
```cpp
#ifndef OTA_UPDATE_TOKEN
#define OTA_UPDATE_TOKEN "LW-OTA-2024-SecureUpdate"  // Change in production!
#endif
```

**File**: `firmware/Tab5.encoder/wifi_credentials.ini.template`
**Add**: Optional OTA token configuration

#### Step 5: Verify Partition Table
**Action**: Check if `huge_app.csv` partition table supports OTA
- ESP32-P4 should support OTA partitions
- May need to adjust partition layout if current table doesn't support it
- Default Arduino partition tables usually support OTA

**Check**: Verify `ESP.getFreeSketchSpace()` returns sufficient space

#### Step 6: Add Progress Display
**File**: `firmware/Tab5.encoder/src/ui/DisplayUI.h` (or create new OtaProgressUI)

**Functionality**:
- Display upload progress percentage
- Show status messages (uploading, verifying, rebooting)
- Visual progress bar on Tab5 screen

#### Step 7: Add Build Configuration
**File**: `firmware/Tab5.encoder/platformio.ini`

**Add**:
```ini
build_flags = 
    ...
    -DFEATURE_OTA_UPDATE=1
    -DOTA_UPDATE_TOKEN=\"LW-OTA-2024-SecureUpdate\"
```

#### Step 8: Create OTA Update Documentation
**New File**: `firmware/Tab5.encoder/OTA_UPDATE_GUIDE.md`

**Content**:
- OTA endpoint documentation
- Upload commands (curl examples)
- Security considerations
- Troubleshooting guide

### File Structure

```
firmware/Tab5.encoder/
├── platformio.ini                    # Add ESPAsyncWebServer, OTA flags
├── src/
│   ├── main.cpp                      # Add HTTP server initialization
│   ├── config/
│   │   └── network_config.h          # Add OTA token config
│   └── network/
│       ├── OtaHandler.h              # NEW: OTA handler interface
│       ├── OtaHandler.cpp            # NEW: OTA handler implementation
│       └── ... (existing files)
└── OTA_UPDATE_GUIDE.md              # NEW: OTA documentation
```

### Security Considerations

1. **OTA Token**: Use strong, unique token per device in production
2. **Network Isolation**: Consider restricting OTA to local network only
3. **HTTPS**: Future enhancement - use HTTPS for OTA (requires certificate)
4. **Rollback**: Consider dual-OTA partition scheme for safe rollback

### Testing Checklist

- [ ] HTTP server starts after WiFi connection
- [ ] OTA endpoints respond correctly
- [ ] Token authentication works
- [ ] Firmware upload succeeds
- [ ] Progress display updates correctly
- [ ] Device reboots after successful update
- [ ] New firmware runs correctly after OTA
- [ ] Error handling works (invalid token, insufficient space, etc.)
- [ ] Version endpoint returns correct information

### Dependencies

- ESPAsyncWebServer library (async HTTP server)
- ESP32 Update library (built-in, for flash writing)
- Existing WiFiManager (for network connectivity)
- Existing DisplayUI (for progress feedback)

### Reference Implementation

- `firmware/v2/src/network/webserver/handlers/FirmwareHandlers.*` - OTA handler implementation
- `firmware/v2/docs/OTA_UPDATE_GUIDE.md` - OTA documentation
- `firmware/v2/src/config/network_config.h` - OTA token configuration

---

## Implementation Order

### Phase 1: Effect Alignment (Quick Fix)
1. Update `ParameterMap.cpp` comment
2. Review `Config.h` comments
3. Verify no hardcoded assumptions
4. Test with v2 firmware

**Estimated Time**: 15-30 minutes

### Phase 2: OTA Implementation (Major Feature)
1. Add HTTP server library dependency
2. Create OTA handler module
3. Integrate HTTP server into main application
4. Add progress display
5. Test OTA update flow
6. Create documentation

**Estimated Time**: 2-4 hours

---

## Success Criteria

### Effect Alignment
- ✅ Comments accurately reflect v2 firmware's effect count
- ✅ No misleading references to non-existent constants
- ✅ Device correctly handles 92 effects (max=91)

### OTA Update
- ✅ Tab5.encoder can receive firmware updates over WiFi
- ✅ Update process shows progress on display
- ✅ Authentication prevents unauthorized updates
- ✅ Device reboots and runs new firmware after update
- ✅ Documentation enables users to perform OTA updates

---

## Notes

- OTA implementation follows v2 firmware patterns for consistency
- HTTP server runs alongside existing WebSocket (no conflicts)
- Progress display enhances user experience during updates
- Security token should be changed from default in production deployments
- Consider adding OTA update notification/check on device startup

