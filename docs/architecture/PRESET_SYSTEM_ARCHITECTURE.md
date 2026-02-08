# LightwaveOS Preset System Architecture

**Version:** 1.0.0
**Status:** Design Document
**Target:** Complete preset system spanning firmware (ESP32-S3) and iOS app

---

## Executive Summary

This document defines the complete architecture for the LightwaveOS preset system, enabling users to save, load, and manage device configurations across three preset types:

1. **Effect Presets** - Single effect configurations (effect ID, palette, speed, brightness, parameters)
2. **Zone Presets** - Multi-zone layout snapshots (zone counts, segments, per-zone effects/palettes)
3. **Shows** - Temporal sequences of presets with transitions and timing

The architecture uses a **three-tier design**: persistence layer (NVS storage), REST/WebSocket API layer (firmware handlers), and client layer (iOS ViewModels/Views).

---

## Architecture Impact Assessment

**Impact Level:** HIGH

### Scope
- **Firmware components:** 6 new/modified classes (2 managers, 2 handler sets, 2 WebSocket command handlers)
- **iOS components:** 4 new/modified classes (3 ViewModels, 3 Views, 1 model)
- **API endpoints:** 15 new REST endpoints + 6 WebSocket commands
- **Storage:** NVS persistence with blob storage for preset data

### Risks
- **Flash wear:** Frequent preset saves could degrade NVS flash (mitigated by CRC validation and smart write)
- **Memory allocation:** Loading preset JSON requires heap allocation (use PSRAM for large presets)
- **API version compatibility:** Preset format must be versioned for future-proofing

### Benefits
- **User experience:** Instant recall of complex configurations
- **Workflow efficiency:** Save experimentation results without manual note-taking
- **Cross-device sync:** REST API enables preset backup/restore across devices
- **Show automation:** Foundation for temporal sequencing and choreography

---

## System Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         iOS App Layer                            │
├─────────────────────────────────────────────────────────────────┤
│  PresetsViewModel          ZonePresetsViewModel   ShowsViewModel│
│  PresetsView               ZonePresetsView        ShowsView     │
│                                                                  │
│  Models: EffectPreset, ZonePreset, Show                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ REST + WebSocket
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                       Firmware API Layer                         │
├─────────────────────────────────────────────────────────────────┤
│  REST Handlers                  WebSocket Commands              │
│  ────────────────────────────────────────────────────────       │
│  EffectPresetHandlers           WsEffectPresetCommands          │
│  ZonePresetHandlers             WsZonePresetCommands            │
│  ShowHandlers                   WsShowCommands                  │
│                                                                  │
│  Routes: /api/v1/presets/*, /api/v1/zones/presets/*, /shows/*  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ C++ API calls
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Firmware Persistence Layer                    │
├─────────────────────────────────────────────────────────────────┤
│  EffectPresetManager        ZonePresetManager    ShowManager    │
│  (NVS namespace: effects)   (NVS: zones)        (NVS: shows)    │
│                                                                  │
│  Storage: 16 presets each, blob format with CRC32 validation    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ NVS API
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      NVSManager (Singleton)                      │
│  Blob storage | CRC32 validation | Thread-safe operations       │
└─────────────────────────────────────────────────────────────────┘
```

---

## Firmware Components

### 1. EffectPresetManager

**File:** `firmware/v2/src/core/persistence/EffectPresetManager.{h,cpp}`

**Responsibility:** Persist and load single-effect configurations.

**Storage Format:**
```cpp
struct EffectPreset {
    uint8_t version;           // Format version (current: 1)
    uint8_t effectId;          // Effect ID (0-255)
    uint8_t paletteId;         // Palette ID (0-74)
    uint8_t brightness;        // Brightness (0-255)
    uint8_t speed;             // Speed (1-100)
    char name[32];             // User-friendly name (null-terminated)

    // Expression parameters (semantic mapping)
    uint8_t mood;              // 0-255
    uint8_t trails;            // 0-255
    uint8_t hue;               // 0-255
    uint8_t saturation;        // 0-255
    uint8_t intensity;         // 0-255
    uint8_t complexity;        // 0-255
    uint8_t variation;         // 0-255

    uint32_t timestamp;        // Creation timestamp (Unix epoch)
    uint32_t crc32;            // CRC32 checksum (excludes this field)
};
```

**Interface:**
```cpp
class EffectPresetManager {
public:
    static EffectPresetManager& instance();

    // Initialization
    bool init();

    // CRUD operations
    NVSResult save(uint8_t slot, const EffectPreset& preset);
    NVSResult load(uint8_t slot, EffectPreset& preset);
    NVSResult list(EffectPresetMetadata* metadata, uint8_t& count);
    NVSResult remove(uint8_t slot);

    // Convenience methods
    NVSResult saveCurrentEffect(uint8_t slot, const char* name,
                                RendererActor* renderer);
    bool isSlotOccupied(uint8_t slot);

    // Constants
    static constexpr uint8_t MAX_PRESETS = 16;
    static constexpr const char* NVS_NAMESPACE = "effects";

private:
    EffectPresetManager();
    bool m_initialized;
};
```

**Key Design Decisions:**
- **Fixed slots (0-15):** Simplifies NVS key naming (`preset_00`, `preset_01`, etc.)
- **Semantic parameters embedded:** No need for effect-specific parameter serialization
- **CRC32 validation:** Detect corrupted presets on load
- **Timestamp tracking:** Enables "recently used" sorting in UI

---

### 2. ZonePresetManager

**File:** `firmware/v2/src/core/persistence/ZonePresetManager.{h,cpp}`

**Responsibility:** Persist and load multi-zone layout configurations.

**Storage Format:**
```cpp
struct ZonePresetEntry {
    uint8_t effectId;
    uint8_t paletteId;
    uint8_t brightness;
    uint8_t speed;
    uint8_t blendMode;         // BlendMode enum (0-7)

    // Zone segment definitions (LED ranges)
    uint16_t s1LeftStart;
    uint16_t s1LeftEnd;
    uint16_t s1RightStart;
    uint16_t s1RightEnd;
};

struct ZonePreset {
    uint8_t version;           // Format version (current: 1)
    uint8_t zoneCount;         // Number of active zones (1-4)
    char name[32];             // User-friendly name
    ZonePresetEntry zones[4];  // Fixed array for all zones
    uint32_t timestamp;        // Creation timestamp
    uint32_t crc32;            // CRC32 checksum
};
```

**Interface:**
```cpp
class ZonePresetManager {
public:
    static ZonePresetManager& instance();

    bool init();

    // CRUD operations
    NVSResult save(uint8_t slot, const ZonePreset& preset);
    NVSResult load(uint8_t slot, ZonePreset& preset);
    NVSResult list(ZonePresetMetadata* metadata, uint8_t& count);
    NVSResult remove(uint8_t slot);

    // Integration with ZoneComposer
    NVSResult saveCurrentZones(uint8_t slot, const char* name,
                               ZoneComposer* composer);
    NVSResult applyToZones(uint8_t slot, ZoneComposer* composer);

    static constexpr uint8_t MAX_PRESETS = 16;
    static constexpr const char* NVS_NAMESPACE = "zones";

private:
    ZonePresetManager();
    bool m_initialized;
};
```

**Key Design Decisions:**
- **Stores complete zone layout:** Includes segment definitions, not just effect/palette
- **Fixed zone array:** Always stores 4 zones, `zoneCount` determines active zones
- **Direct ZoneComposer integration:** Simplifies save-current and load workflows

---

### 3. ShowManager

**File:** `firmware/v2/src/core/persistence/ShowManager.{h,cpp}`

**Responsibility:** Persist and load temporal preset sequences (future feature).

**Storage Format:**
```cpp
struct ShowStep {
    uint8_t presetType;        // 0=Effect, 1=Zone
    uint8_t presetSlot;        // Slot ID in respective manager
    uint16_t durationMs;       // Step duration in milliseconds
    uint8_t transitionType;    // Transition ID (0-11)
    uint16_t transitionMs;     // Transition duration
};

struct Show {
    uint8_t version;           // Format version (current: 1)
    char name[32];             // Show name
    uint8_t stepCount;         // Number of steps (1-32)
    bool loop;                 // Loop playback
    ShowStep steps[32];        // Fixed array of steps
    uint32_t timestamp;        // Creation timestamp
    uint32_t crc32;            // CRC32 checksum
};
```

**Interface:**
```cpp
class ShowManager {
public:
    static ShowManager& instance();

    bool init();

    // CRUD operations
    NVSResult save(uint8_t slot, const Show& show);
    NVSResult load(uint8_t slot, Show& show);
    NVSResult list(ShowMetadata* metadata, uint8_t& count);
    NVSResult remove(uint8_t slot);

    static constexpr uint8_t MAX_SHOWS = 8;
    static constexpr const char* NVS_NAMESPACE = "shows";

private:
    ShowManager();
    bool m_initialized;
};
```

**Key Design Decisions:**
- **References presets by slot:** Shows don't embed full preset data
- **Temporal sequencing:** Each step has duration + transition
- **Future feature:** Stub implementation for now, full playback engine later

---

### 4. REST API Handlers

#### EffectPresetHandlers

**File:** `firmware/v2/src/network/webserver/handlers/EffectPresetHandlers.{h,cpp}`

**Endpoints:**
- `GET /api/v1/presets/effects` - List all effect presets
- `GET /api/v1/presets/effects/{id}` - Get specific preset details
- `POST /api/v1/presets/effects/save-current` - Save current effect as preset
- `POST /api/v1/presets/effects/{id}/load` - Load effect preset
- `DELETE /api/v1/presets/effects/{id}` - Delete effect preset

**Implementation:**
```cpp
class EffectPresetHandlers {
public:
    static void handleList(AsyncWebServerRequest* request);
    static void handleGet(AsyncWebServerRequest* request, uint8_t id);
    static void handleSaveCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                   actors::RendererActor* renderer);
    static void handleLoad(AsyncWebServerRequest* request, uint8_t id,
                           actors::ActorSystem& orchestrator,
                           actors::RendererActor* renderer);
    static void handleDelete(AsyncWebServerRequest* request, uint8_t id);
};
```

**Response Format (List):**
```json
{
  "success": true,
  "data": {
    "presets": [
      {
        "id": 0,
        "name": "Sunset Glow",
        "effectId": 5,
        "paletteId": 12,
        "timestamp": 1738886400
      }
    ]
  },
  "timestamp": 1738886401,
  "version": "1.0.0"
}
```

**Response Format (Get):**
```json
{
  "success": true,
  "data": {
    "id": 0,
    "name": "Sunset Glow",
    "effectId": 5,
    "paletteId": 12,
    "brightness": 200,
    "speed": 35,
    "parameters": {
      "mood": 180,
      "trails": 120,
      "hue": 25,
      "saturation": 230,
      "intensity": 190,
      "complexity": 80,
      "variation": 60
    },
    "timestamp": 1738886400
  }
}
```

---

#### ZonePresetHandlers

**File:** `firmware/v2/src/network/webserver/handlers/ZonePresetHandlers.{h,cpp}`

**Endpoints:**
- `GET /api/v1/presets/zones` - List all zone presets
- `GET /api/v1/presets/zones/{id}` - Get specific zone preset
- `POST /api/v1/presets/zones/save-current` - Save current zone config
- `POST /api/v1/presets/zones/{id}/load` - Load zone preset
- `DELETE /api/v1/presets/zones/{id}` - Delete zone preset

**Implementation:**
```cpp
class ZonePresetHandlers {
public:
    static void handleList(AsyncWebServerRequest* request);
    static void handleGet(AsyncWebServerRequest* request, uint8_t id);
    static void handleSaveCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                   zones::ZoneComposer* composer);
    static void handleLoad(AsyncWebServerRequest* request, uint8_t id,
                           actors::ActorSystem& orchestrator,
                           zones::ZoneComposer* composer,
                           std::function<void()> broadcastFn);
    static void handleDelete(AsyncWebServerRequest* request, uint8_t id);
};
```

**Response Format (List):**
```json
{
  "success": true,
  "data": {
    "presets": [
      {
        "id": 0,
        "name": "Dual Split",
        "zoneCount": 2,
        "timestamp": 1738886400
      }
    ]
  }
}
```

**Response Format (Get):**
```json
{
  "success": true,
  "data": {
    "id": 0,
    "name": "Dual Split",
    "zoneCount": 2,
    "zones": [
      {
        "effectId": 5,
        "paletteId": 12,
        "brightness": 200,
        "speed": 35,
        "blendMode": 1,
        "segments": {
          "s1LeftStart": 0,
          "s1LeftEnd": 79,
          "s1RightStart": 80,
          "s1RightEnd": 159
        }
      },
      {
        "effectId": 7,
        "paletteId": 15,
        "brightness": 180,
        "speed": 50,
        "blendMode": 2,
        "segments": {
          "s1LeftStart": 160,
          "s1LeftEnd": 239,
          "s1RightStart": 240,
          "s1RightEnd": 319
        }
      }
    ],
    "timestamp": 1738886400
  }
}
```

---

#### ShowHandlers

**File:** `firmware/v2/src/network/webserver/handlers/ShowHandlers.{h,cpp}`

**Endpoints:**
- `GET /api/v1/shows` - List all shows
- `GET /api/v1/shows/{id}` - Get show details
- `POST /api/v1/shows` - Create/upload show
- `PUT /api/v1/shows/{id}` - Update show
- `DELETE /api/v1/shows/{id}` - Delete show
- `POST /api/v1/shows/{id}/play` - Start show playback
- `POST /api/v1/shows/{id}/stop` - Stop show playback
- `GET /api/v1/shows/current` - Get current playback state

**Implementation:**
```cpp
class ShowHandlers {
public:
    static void handleList(AsyncWebServerRequest* request);
    static void handleGet(AsyncWebServerRequest* request, uint8_t id);
    static void handleCreate(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleUpdate(AsyncWebServerRequest* request, uint8_t id,
                             uint8_t* data, size_t len);
    static void handleDelete(AsyncWebServerRequest* request, uint8_t id);
    static void handlePlay(AsyncWebServerRequest* request, uint8_t id,
                           actors::ActorSystem& orchestrator);
    static void handleStop(AsyncWebServerRequest* request,
                           actors::ActorSystem& orchestrator);
    static void handleCurrent(AsyncWebServerRequest* request,
                              actors::ActorSystem& orchestrator);
};
```

**Response Format (Get):**
```json
{
  "success": true,
  "data": {
    "id": 0,
    "name": "Evening Sequence",
    "loop": true,
    "stepCount": 3,
    "steps": [
      {
        "presetType": 0,
        "presetSlot": 2,
        "durationMs": 30000,
        "transitionType": 5,
        "transitionMs": 2000
      }
    ],
    "timestamp": 1738886400
  }
}
```

---

### 5. WebSocket Command Handlers

#### WsEffectPresetCommands

**File:** `firmware/v2/src/network/webserver/ws/WsEffectPresetCommands.{h,cpp}`

**Commands:**
- `effectPresets.list` - List all effect presets
- `effectPresets.get` - Get specific preset
- `effectPresets.saveCurrent` - Save current effect
- `effectPresets.load` - Load preset
- `effectPresets.delete` - Delete preset

**Implementation:**
```cpp
void registerWsEffectPresetCommands(const WebServerContext& ctx) {
    WsCommandRouter& router = WsCommandRouter::instance();

    router.onCommand("effectPresets.list", [ctx](AsyncWebSocketClient* client, JsonDocument& doc) {
        // List presets and send to client
    });

    router.onCommand("effectPresets.saveCurrent", [ctx](AsyncWebSocketClient* client, JsonDocument& doc) {
        // Save current effect, broadcast to all clients
    });

    router.onCommand("effectPresets.load", [ctx](AsyncWebSocketClient* client, JsonDocument& doc) {
        // Load preset, broadcast state change
    });

    // ... other commands
}
```

**Message Format (Save Current):**
```json
{
  "type": "effectPresets.saveCurrent",
  "slot": 0,
  "name": "My Effect"
}
```

**Broadcast Format (After Save):**
```json
{
  "type": "effectPresets.saved",
  "slot": 0,
  "preset": {
    "id": 0,
    "name": "My Effect",
    "effectId": 5,
    "paletteId": 12
  },
  "timestamp": 1738886401
}
```

---

#### WsZonePresetCommands

**File:** `firmware/v2/src/network/webserver/ws/WsZonePresetCommands.{h,cpp}`

**Commands:**
- `zonePresets.list` - List all zone presets
- `zonePresets.get` - Get specific zone preset
- `zonePresets.saveCurrent` - Save current zone config
- `zonePresets.load` - Load zone preset
- `zonePresets.delete` - Delete zone preset

**Implementation:** (Similar structure to effect presets)

---

#### WsShowCommands

**File:** `firmware/v2/src/network/webserver/ws/WsShowCommands.{h,cpp}`

**Commands:**
- `shows.list` - List all shows
- `shows.get` - Get show details
- `shows.play` - Start playback
- `shows.stop` - Stop playback
- `shows.status` - Get playback state

---

## iOS Components

### 1. Models

#### EffectPreset

**File:** `lightwave-ios-v2/LightwaveOS/Models/EffectPreset.swift`

```swift
struct EffectPreset: Identifiable, Codable, Sendable, Hashable {
    let id: Int
    let name: String
    let effectId: Int
    let paletteId: Int
    let brightness: Int
    let speed: Int
    let parameters: EffectParameters?
    let timestamp: TimeInterval?

    struct EffectParameters: Codable, Sendable, Hashable {
        let mood: Int?
        let trails: Int?
        let hue: Int?
        let saturation: Int?
        let intensity: Int?
        let complexity: Int?
        let variation: Int?
    }

    var displayName: String { name }

    var formattedDate: String? {
        guard let timestamp = timestamp else { return nil }
        let date = Date(timeIntervalSince1970: timestamp)
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}
```

---

#### ZonePreset

**File:** `lightwave-ios-v2/LightwaveOS/Models/ZonePreset.swift`

```swift
struct ZonePreset: Identifiable, Codable, Sendable, Hashable {
    let id: Int
    let name: String
    let zoneCount: Int
    let zones: [ZonePresetEntry]
    let timestamp: TimeInterval?

    struct ZonePresetEntry: Codable, Sendable, Hashable {
        let effectId: Int
        let paletteId: Int
        let brightness: Int
        let speed: Int
        let blendMode: Int
        let segments: ZoneSegments

        struct ZoneSegments: Codable, Sendable, Hashable {
            let s1LeftStart: Int
            let s1LeftEnd: Int
            let s1RightStart: Int
            let s1RightEnd: Int
        }
    }

    var displayName: String { name }
}
```

---

#### Show

**File:** `lightwave-ios-v2/LightwaveOS/Models/Show.swift`

```swift
struct Show: Identifiable, Codable, Sendable, Hashable {
    let id: Int
    let name: String
    let loop: Bool
    let stepCount: Int
    let steps: [ShowStep]
    let timestamp: TimeInterval?

    struct ShowStep: Codable, Sendable, Hashable {
        let presetType: Int  // 0=Effect, 1=Zone
        let presetSlot: Int
        let durationMs: Int
        let transitionType: Int
        let transitionMs: Int
    }

    var displayName: String { name }
    var totalDurationSeconds: Int {
        steps.reduce(0) { $0 + ($1.durationMs / 1000) }
    }
}
```

---

### 2. ViewModels

#### EffectPresetsViewModel

**File:** `lightwave-ios-v2/LightwaveOS/ViewModels/EffectPresetsViewModel.swift`

```swift
@MainActor
@Observable
class EffectPresetsViewModel {
    // MARK: - State

    var presets: [EffectPreset] = []
    var currentPresetId: Int?
    var isLoading = false
    var saveSheetPresented = false
    var presetToDelete: EffectPreset?

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - API Methods

    func loadPresets() async {
        guard let client = restClient else { return }
        isLoading = true
        defer { isLoading = false }

        do {
            let response = try await client.getEffectPresets()
            presets = response.data.presets
        } catch {
            print("Failed to load effect presets: \(error)")
        }
    }

    func loadPreset(_ preset: EffectPreset) async {
        guard let client = restClient else { return }

        do {
            try await client.loadEffectPreset(id: preset.id)
            currentPresetId = preset.id
        } catch {
            print("Failed to load preset: \(error)")
        }
    }

    func saveCurrentAsPreset(name: String, slot: Int) async {
        guard let client = restClient else { return }

        do {
            try await client.saveEffectPreset(slot: slot, name: name)
            await loadPresets()
        } catch {
            print("Failed to save preset: \(error)")
        }
    }

    func deletePreset(_ preset: EffectPreset) async {
        guard let client = restClient else { return }

        do {
            try await client.deleteEffectPreset(id: preset.id)
            await loadPresets()
        } catch {
            print("Failed to delete preset: \(error)")
        }
    }
}
```

---

#### ZonePresetsViewModel

**File:** `lightwave-ios-v2/LightwaveOS/ViewModels/ZonePresetsViewModel.swift`

```swift
@MainActor
@Observable
class ZonePresetsViewModel {
    var presets: [ZonePreset] = []
    var currentPresetId: Int?
    var isLoading = false
    var saveSheetPresented = false
    var presetToDelete: ZonePreset?

    var restClient: RESTClient?

    func loadPresets() async {
        guard let client = restClient else { return }
        isLoading = true
        defer { isLoading = false }

        do {
            let response = try await client.getZonePresets()
            presets = response.data.presets
        } catch {
            print("Failed to load zone presets: \(error)")
        }
    }

    func loadPreset(_ preset: ZonePreset) async {
        guard let client = restClient else { return }

        do {
            try await client.loadZonePreset(id: preset.id)
            currentPresetId = preset.id
        } catch {
            print("Failed to load zone preset: \(error)")
        }
    }

    func saveCurrentAsPreset(name: String, slot: Int) async {
        guard let client = restClient else { return }

        do {
            try await client.saveZonePreset(slot: slot, name: name)
            await loadPresets()
        } catch {
            print("Failed to save zone preset: \(error)")
        }
    }

    func deletePreset(_ preset: ZonePreset) async {
        guard let client = restClient else { return }

        do {
            try await client.deleteZonePreset(id: preset.id)
            await loadPresets()
        } catch {
            print("Failed to delete zone preset: \(error)")
        }
    }
}
```

---

#### ShowsViewModel

**File:** `lightwave-ios-v2/LightwaveOS/ViewModels/ShowsViewModel.swift`

```swift
@MainActor
@Observable
class ShowsViewModel {
    var shows: [Show] = []
    var currentShow: Show?
    var isPlaying = false
    var currentStep = 0
    var isLoading = false

    var restClient: RESTClient?

    func loadShows() async {
        guard let client = restClient else { return }
        isLoading = true
        defer { isLoading = false }

        do {
            let response = try await client.getShows()
            shows = response.data.shows
        } catch {
            print("Failed to load shows: \(error)")
        }
    }

    func playShow(_ show: Show) async {
        guard let client = restClient else { return }

        do {
            try await client.playShow(id: show.id)
            currentShow = show
            isPlaying = true
        } catch {
            print("Failed to play show: \(error)")
        }
    }

    func stopShow() async {
        guard let client = restClient else { return }

        do {
            try await client.stopShow()
            isPlaying = false
            currentStep = 0
        } catch {
            print("Failed to stop show: \(error)")
        }
    }

    func deleteShow(_ show: Show) async {
        guard let client = restClient else { return }

        do {
            try await client.deleteShow(id: show.id)
            await loadShows()
        } catch {
            print("Failed to delete show: \(error)")
        }
    }
}
```

---

### 3. Views

#### EffectPresetsView

**File:** `lightwave-ios-v2/LightwaveOS/Views/Device/EffectPresetsView.swift`

```swift
struct EffectPresetsView: View {
    @Environment(EffectPresetsViewModel.self) private var viewModel

    var body: some View {
        List {
            if viewModel.isLoading {
                ProgressView()
                    .frame(maxWidth: .infinity, alignment: .center)
            } else if viewModel.presets.isEmpty {
                Section {
                    Text("No effect presets saved")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }
                .listRowBackground(Color.lwCard)
            } else {
                ForEach(viewModel.presets) { preset in
                    EffectPresetRow(preset: preset)
                        .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                            Button(role: .destructive) {
                                viewModel.presetToDelete = preset
                            } label: {
                                Label("Delete", systemImage: "trash")
                            }
                        }
                        .listRowBackground(Color.lwCard)
                }
            }
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle("Effect Presets")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button {
                    viewModel.saveSheetPresented = true
                } label: {
                    Label("Save Current", systemImage: "plus.circle")
                }
            }
        }
        .sheet(isPresented: $viewModel.saveSheetPresented) {
            SavePresetSheet(onSave: { name, slot in
                Task {
                    await viewModel.saveCurrentAsPreset(name: name, slot: slot)
                }
            })
        }
        .confirmationDialog(
            "Delete Preset",
            isPresented: .init(
                get: { viewModel.presetToDelete != nil },
                set: { if !$0 { viewModel.presetToDelete = nil } }
            ),
            titleVisibility: .visible
        ) {
            Button("Delete", role: .destructive) {
                if let preset = viewModel.presetToDelete {
                    Task {
                        await viewModel.deletePreset(preset)
                    }
                }
            }
        } message: {
            if let preset = viewModel.presetToDelete {
                Text("Delete '\(preset.name)'? This cannot be undone.")
            }
        }
        .task {
            await viewModel.loadPresets()
        }
    }
}

struct EffectPresetRow: View {
    @Environment(EffectPresetsViewModel.self) private var viewModel
    let preset: EffectPreset

    var body: some View {
        Button {
            Task {
                await viewModel.loadPreset(preset)
            }
        } label: {
            HStack(spacing: 12) {
                // Icon placeholder (future: effect thumbnail)
                Image(systemName: "sparkles")
                    .font(.title2)
                    .foregroundStyle(Color.lwAccent)
                    .frame(width: 40, height: 40)
                    .background(Color.lwAccent.opacity(0.1))
                    .clipShape(RoundedRectangle(cornerRadius: 8))

                VStack(alignment: .leading, spacing: 4) {
                    Text(preset.name)
                        .font(.headline)
                        .foregroundStyle(Color.lwTextPrimary)

                    if let date = preset.formattedDate {
                        Text(date)
                            .font(.caption)
                            .foregroundStyle(Color.lwTextSecondary)
                    }
                }

                Spacer()

                if viewModel.currentPresetId == preset.id {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundStyle(Color.lwAccent)
                }
            }
        }
        .buttonStyle(.plain)
    }
}

struct SavePresetSheet: View {
    @Environment(\.dismiss) private var dismiss
    @State private var name = ""
    @State private var selectedSlot = 0

    let onSave: (String, Int) -> Void

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Preset Name", text: $name)
                }

                Section {
                    Picker("Slot", selection: $selectedSlot) {
                        ForEach(0..<16) { slot in
                            Text("Slot \(slot + 1)").tag(slot)
                        }
                    }
                } header: {
                    Text("Save Location")
                } footer: {
                    Text("Choose a slot to save this preset. Existing presets will be overwritten.")
                }
            }
            .navigationTitle("Save Effect Preset")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        dismiss()
                    }
                }

                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        onSave(name, selectedSlot)
                        dismiss()
                    }
                    .disabled(name.isEmpty)
                }
            }
        }
    }
}
```

---

#### ZonePresetsView

**File:** `lightwave-ios-v2/LightwaveOS/Views/Device/ZonePresetsView.swift`

(Similar structure to EffectPresetsView, with zone-specific UI)

---

#### ShowsView

**File:** `lightwave-ios-v2/LightwaveOS/Views/Device/ShowsView.swift`

```swift
struct ShowsView: View {
    @Environment(ShowsViewModel.self) private var viewModel

    var body: some View {
        List {
            // Current playback section
            if let currentShow = viewModel.currentShow, viewModel.isPlaying {
                Section("Now Playing") {
                    ShowPlaybackCard(show: currentShow, currentStep: viewModel.currentStep)
                }
                .listRowBackground(Color.lwCard)
            }

            // Shows list
            if viewModel.shows.isEmpty {
                Section {
                    Text("No shows available")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }
                .listRowBackground(Color.lwCard)
            } else {
                ForEach(viewModel.shows) { show in
                    ShowRow(show: show)
                        .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                            Button(role: .destructive) {
                                Task {
                                    await viewModel.deleteShow(show)
                                }
                            } label: {
                                Label("Delete", systemImage: "trash")
                            }
                        }
                        .listRowBackground(Color.lwCard)
                }
            }
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle("Shows")
        .navigationBarTitleDisplayMode(.inline)
        .task {
            await viewModel.loadShows()
        }
    }
}

struct ShowRow: View {
    @Environment(ShowsViewModel.self) private var viewModel
    let show: Show

    var body: some View {
        Button {
            Task {
                await viewModel.playShow(show)
            }
        } label: {
            HStack(spacing: 12) {
                Image(systemName: "play.circle.fill")
                    .font(.title2)
                    .foregroundStyle(Color.lwAccent)
                    .frame(width: 40, height: 40)

                VStack(alignment: .leading, spacing: 4) {
                    Text(show.name)
                        .font(.headline)
                        .foregroundStyle(Color.lwTextPrimary)

                    Text("\(show.stepCount) steps • \(show.totalDurationSeconds)s")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextSecondary)
                }

                Spacer()

                if viewModel.currentShow?.id == show.id && viewModel.isPlaying {
                    Image(systemName: "speaker.wave.2.fill")
                        .foregroundStyle(Color.lwAccent)
                }
            }
        }
        .buttonStyle(.plain)
    }
}
```

---

## Data Flow

### Save Current Effect Preset

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. User Action (iOS)                                             │
│    User taps "Save Current" button in EffectPresetsView         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. ViewModel Call                                                │
│    effectPresetsViewModel.saveCurrentAsPreset(name, slot)        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. RESTClient Request                                            │
│    POST /api/v1/presets/effects/save-current                    │
│    Body: {"slot": 0, "name": "My Effect"}                       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4. Firmware Handler                                              │
│    EffectPresetHandlers::handleSaveCurrent()                    │
│    - Extract slot and name from JSON body                       │
│    - Call RendererActor to get current effect state             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. Persistence Layer                                             │
│    EffectPresetManager::saveCurrentEffect(slot, name, renderer) │
│    - Read current effect ID, palette, brightness, speed         │
│    - Read current semantic parameters (mood, trails, etc.)      │
│    - Populate EffectPreset struct                               │
│    - Calculate CRC32                                             │
│    - Save to NVS: key="preset_00", namespace="effects"          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. Response                                                      │
│    200 OK: {"success": true, "data": {...}}                     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 7. WebSocket Broadcast (Optional)                               │
│    {"type": "effectPresets.saved", "slot": 0, ...}              │
│    Sent to all connected clients                                │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 8. iOS Refresh                                                   │
│    effectPresetsViewModel.loadPresets()                         │
│    Updates UI with new preset in list                           │
└─────────────────────────────────────────────────────────────────┘
```

---

### Load Effect Preset

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. User Action (iOS)                                             │
│    User taps preset row in EffectPresetsView                    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. ViewModel Call                                                │
│    effectPresetsViewModel.loadPreset(preset)                    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. RESTClient Request                                            │
│    POST /api/v1/presets/effects/{id}/load                       │
│    Body: {} (empty)                                              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4. Firmware Handler                                              │
│    EffectPresetHandlers::handleLoad(id)                         │
│    - Extract preset ID from URL path parameter                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. Persistence Layer                                             │
│    EffectPresetManager::load(slot, preset)                      │
│    - Read blob from NVS: key="preset_00"                        │
│    - Validate CRC32 checksum                                     │
│    - Deserialize into EffectPreset struct                       │
│    - Return NVSResult::OK or error code                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. Actor System Integration                                      │
│    RendererActor receives SetEffect message:                    │
│    - effectId = preset.effectId                                 │
│    - paletteId = preset.paletteId                               │
│    - brightness = preset.brightness                             │
│    - speed = preset.speed                                        │
│    ParameterActor receives SetParameters message:               │
│    - mood, trails, hue, saturation, intensity, etc.             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 7. Effect Rendering                                              │
│    RendererActor::update() applies new effect on next frame     │
│    LED strip displays loaded preset effect immediately          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 8. WebSocket Broadcast                                           │
│    WebServer broadcasts state change to all clients:            │
│    {"type": "effect.changed", "effectId": 5, ...}               │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 9. iOS UI Update                                                 │
│    WebSocket listener updates EffectViewModel                   │
│    Play tab reflects new effect, palette, brightness, speed     │
└─────────────────────────────────────────────────────────────────┘
```

---

## Implementation Phases

### Phase 1: Effect Presets (Firmware + iOS)

**Duration:** 2-3 days

**Firmware Tasks:**
1. Implement `EffectPresetManager` (persistence layer)
2. Implement `EffectPresetHandlers` (REST API)
3. Implement `WsEffectPresetCommands` (WebSocket)
4. Register routes in `V1ApiRoutes.cpp`
5. Test save/load/delete operations via cURL

**iOS Tasks:**
1. Create `EffectPreset` model
2. Create `EffectPresetsViewModel`
3. Create `EffectPresetsView` with save/load UI
4. Add REST methods to `RESTClient`
5. Add WebSocket handlers in `WebSocketService`
6. Add "Effect Presets" navigation link in Device tab

**Testing:**
- Save current effect via iOS
- Load preset via iOS
- Delete preset via iOS
- Verify WebSocket broadcasts update all clients
- Test NVS persistence across device reboots

---

### Phase 2: Zone Presets (Firmware + iOS)

**Duration:** 2-3 days

**Firmware Tasks:**
1. Implement `ZonePresetManager` (persistence layer)
2. Implement `ZonePresetHandlers` (REST API)
3. Implement `WsZonePresetCommands` (WebSocket)
4. Register routes in `V1ApiRoutes.cpp`
5. Test with multi-zone configurations

**iOS Tasks:**
1. Create `ZonePreset` model
2. Create `ZonePresetsViewModel`
3. Create `ZonePresetsView`
4. Add REST methods to `RESTClient`
5. Add WebSocket handlers
6. Add "Zone Presets" navigation link

**Testing:**
- Save 2-zone configuration
- Load zone preset and verify ZoneComposer state
- Test preset compatibility with 1-4 zone counts
- Verify segment ranges are preserved correctly

---

### Phase 3: Shows (Firmware + iOS)

**Duration:** 3-4 days

**Firmware Tasks:**
1. Implement `ShowManager` (persistence layer)
2. Implement `ShowHandlers` (REST API)
3. Implement `WsShowCommands` (WebSocket)
4. Create `ShowDirectorActor` for playback orchestration
5. Implement temporal sequencing logic
6. Add transition support between show steps

**iOS Tasks:**
1. Create `Show` model
2. Create `ShowsViewModel`
3. Create `ShowsView` with playback controls
4. Create `ShowEditorView` for step-by-step show building
5. Add REST methods to `RESTClient`
6. Add "Shows" navigation link

**Testing:**
- Create 3-step show with transitions
- Play show and verify step progression
- Test loop mode
- Verify transition timing accuracy
- Test show stop/resume functionality

---

## API Endpoints Summary

### Effect Presets

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/presets/effects` | List all effect presets |
| GET | `/api/v1/presets/effects/{id}` | Get preset details |
| POST | `/api/v1/presets/effects/save-current` | Save current effect |
| POST | `/api/v1/presets/effects/{id}/load` | Load preset |
| DELETE | `/api/v1/presets/effects/{id}` | Delete preset |

### Zone Presets

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/presets/zones` | List all zone presets |
| GET | `/api/v1/presets/zones/{id}` | Get preset details |
| POST | `/api/v1/presets/zones/save-current` | Save current zone config |
| POST | `/api/v1/presets/zones/{id}/load` | Load zone preset |
| DELETE | `/api/v1/presets/zones/{id}` | Delete zone preset |

### Shows

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/shows` | List all shows |
| GET | `/api/v1/shows/{id}` | Get show details |
| POST | `/api/v1/shows` | Create show |
| PUT | `/api/v1/shows/{id}` | Update show |
| DELETE | `/api/v1/shows/{id}` | Delete show |
| POST | `/api/v1/shows/{id}/play` | Start playback |
| POST | `/api/v1/shows/stop` | Stop playback |
| GET | `/api/v1/shows/current` | Get playback state |

---

## WebSocket Commands Summary

### Effect Presets

- `effectPresets.list`
- `effectPresets.get`
- `effectPresets.saveCurrent`
- `effectPresets.load`
- `effectPresets.delete`

### Zone Presets

- `zonePresets.list`
- `zonePresets.get`
- `zonePresets.saveCurrent`
- `zonePresets.load`
- `zonePresets.delete`

### Shows

- `shows.list`
- `shows.get`
- `shows.play`
- `shows.stop`
- `shows.status`

---

## Storage Budget

### NVS Partition Usage

**Effect Presets:**
- Size per preset: ~112 bytes (struct + CRC)
- Max presets: 16
- Total: ~1.8 KB

**Zone Presets:**
- Size per preset: ~240 bytes (4 zones + metadata)
- Max presets: 16
- Total: ~3.8 KB

**Shows:**
- Size per show: ~1.1 KB (32 steps + metadata)
- Max shows: 8
- Total: ~8.8 KB

**Total NVS Usage:** ~14.4 KB (well within NVS partition limits)

---

## Security Considerations

1. **API Authentication:** Preset endpoints require API key (enforced by `checkAPIKey` middleware)
2. **Rate Limiting:** Save operations limited to 20 req/sec per client
3. **Input Validation:** Preset names sanitized to prevent injection attacks
4. **CRC Validation:** Prevents loading corrupted presets that could crash firmware
5. **Slot Boundaries:** Firmware enforces slot range (0-15) to prevent buffer overflows

---

## Future Enhancements

1. **Preset Export/Import:** Download presets as JSON files for backup/sharing
2. **Cloud Sync:** Sync presets across multiple devices via cloud storage
3. **Preset Tags:** Categorize presets (e.g., "Party", "Chill", "Seasonal")
4. **Preset Thumbnails:** Generate LED strip preview images for visual browsing
5. **Show Recording:** Record user interactions as show steps automatically
6. **Show Scheduler:** Schedule shows to play at specific times (cron-like)
7. **Community Sharing:** Share presets with other LightwaveOS users via REST API

---

## References

- **API Documentation:** `/docs/api/api-v1.md`
- **NVS Manager:** `/firmware/v2/src/core/persistence/NVSManager.{h,cpp}`
- **Zone Composer:** `/firmware/v2/src/effects/zones/ZoneComposer.{h,cpp}`
- **Renderer Actor:** `/firmware/v2/src/core/actors/RendererActor.{h,cpp}`
- **WebSocket Gateway:** `/firmware/v2/src/network/webserver/WsGateway.{h,cpp}`
- **iOS RESTClient:** `/lightwave-ios-v2/LightwaveOS/Network/RESTClient.swift`

---

## Appendix A: Preset Format Versioning

To support future schema changes, all preset structs include a `version` field:

```cpp
struct EffectPreset {
    uint8_t version;  // Current: 1
    // ... rest of struct
};
```

**Version 1 (Current):**
- 7 semantic parameters (mood, trails, hue, saturation, intensity, complexity, variation)
- Fixed 32-byte name field
- CRC32 validation

**Version 2 (Future):**
- Add `effectCategory` field (uint8_t)
- Add `tags` bitmap (uint32_t) for filtering
- Extend name field to 64 bytes

**Backwards Compatibility Strategy:**
- Firmware checks `version` field before deserializing
- Version 1 presets auto-migrate to Version 2 on first load
- iOS client requests version info via API discovery endpoint

---

## Appendix B: Error Handling

### NVS Errors

| Error | HTTP Status | Action |
|-------|-------------|--------|
| `NOT_INITIALIZED` | 500 | Return "NVS not ready, try again" |
| `NOT_FOUND` | 404 | Return "Preset not found" |
| `CHECKSUM_ERROR` | 500 | Return "Preset corrupted, delete and re-save" |
| `WRITE_ERROR` | 500 | Return "Flash write failed, check NVS stats" |

### iOS Error Handling

```swift
do {
    try await viewModel.loadPreset(preset)
} catch let error as RESTClientError {
    switch error {
    case .httpError(let statusCode, let message):
        if statusCode == 404 {
            // Show "Preset not found" alert
        } else {
            // Show generic error alert
        }
    case .networkError:
        // Show "No connection" alert
    default:
        // Show "Unknown error" alert
    }
}
```

---

**End of Document**
