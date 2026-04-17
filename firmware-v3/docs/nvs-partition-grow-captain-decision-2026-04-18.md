---
abstract: "Captain-decision document for P1-06: growing K1 NVS partition from 20KB to 64KB. Presents three deploy options (erase+reflash / staged migration / dual-partition), partition layout impact, field-data loss matrix, and recommendation. Read before shipping any firmware release that includes the new partitions_custom.csv."
---

# K1 NVS Partition Grow — Captain Decision (2026-04-18)

**Finding:** P1-06 in `docs/forensic-audit-2026-04-17.md`. K1 NVS partition is 20 KB (502 free entries after 113 in use). Tab5-encoder was grown to 64 KB in a previous release; K1 was missed. One feature expansion away from full.

**Deliverable status:** Sandbox build PASSES with the proposed layout. RAM 41.0%, Flash 32.6%. New partition table is NOT compatible with pre-existing K1 flash images — this is the captain-gated decision.

---

## 1. Partition layout — before and after

| Partition | Before (offset, size)  | After (offset, size)   | Delta                 |
|-----------|------------------------|------------------------|-----------------------|
| nvs       | 0x9000, 20 KB          | 0x9000, **64 KB**      | +44 KB                |
| otadata   | 0xE000, 8 KB           | 0x19000, 8 KB          | offset +44 KB         |
| app0      | 0x10000, 7 MB          | **0x20000, 7 MB**      | offset +64 KB (align) |
| app1      | 0x710000, 7 MB         | 0x720000, 7 MB         | offset +64 KB         |
| spiffs    | 0xE10000, 512 KB       | 0xE20000, 512 KB       | offset +64 KB         |
| userdata  | 0xE90000, 1024 KB      | 0xEA0000, **896 KB**   | offset +64 KB, -128KB |
| coredump  | 0xF90000, 64 KB        | 0xF80000, 64 KB        | offset -64 KB         |

Total flash used: 0xF90000 → 0xF90000 (unchanged, 15.5 MB of 16 MB). Slack: 448 KB unchanged.

**Why app0 must shift to 0x20000:** OTA app partitions must be 64 KB-aligned. Growing nvs by 44 KB pushes otadata end from 0x10000 to 0x1B000. The next 64 KB boundary is 0x20000 → app0 must move there. This is the same discipline tab5 used (its app0 is also at 0x20000 for the same reason).

**Why `userdata` shrinks rather than `spiffs`:** No source file in `firmware-v3/src/` references the `userdata` partition (grep: zero hits for `"userdata"`, `0xE90000`, `esp_partition_find.*SUBTYPE_ANY.*0x40`). `spiffs` is actively mounted via `LittleFS.begin()` in `WebServer.cpp:287, 503` and `PluginManagerActor.cpp:153, 195` — shrinking it risks breaking plugin storage. `userdata` is a historical reserve; reclaiming 128 KB from it has no code impact.

**Compile-verified.** Produced `partitions.bin` decodes to the table above.

---

## 2. NVS usage inventory (what K1 actually stores)

Namespaces in use, enumerated by grepping `NVS_NAMESPACE` constants:

| Namespace        | Owner                        | Keys / payload                       | Typical size    |
|------------------|------------------------------|--------------------------------------|-----------------|
| `wifi_creds`     | `WiFiCredentialsStorage`     | SSID + password per network          | ~64 B per entry |
| `wificred`       | `WiFiCredentialManager`      | `netlist` blob (all known networks)  | up to 2 KB      |
| `auth`           | `ApiKeyManager`              | `api_key` string                     | ~64 B           |
| `ota`            | `OtaTokenManager`            | `token` string                       | ~64 B           |
| `audio_tune`     | `AudioTuningManager`         | tuning presets (blobs)               | ~512 B each     |
| `effects`        | `EffectPresetManager`        | effect presets (blobs)               | ~256 B each     |
| `zones`          | `ZonePresetManager`          | zone presets (blobs)                 | ~512 B each     |
| `zone_config`    | `ZoneConfigManager`          | current zone layout                  | ~128 B          |
| `zone_presets`   | `ZonePresetHandlers` (WS)    | `count` + `preset_N` blobs           | ~256 B each     |
| `system` (state) | `ZoneConfigManager`          | `state` blob (restore on boot)       | ~128 B          |
| `ColourCorrection` | `ColorCorrectionEngine`    | Preferences kv                       | ~64 B           |
| `EdgeMixer`      | `EdgeMixer`                  | Preferences kv                       | ~64 B           |

**Current usage (from field boot log):** 113 / 630 entries, ~18 %. Each NVS "entry" is 32 bytes; a blob occupies `ceil(size/32)` entries plus one for the header.

**Why 64 KB is the right size (not 32 KB or 128 KB):**

- 32 KB would reach ~40 % utilisation under the next feature cycle (predicted: per-zone audio tuning, per-preset colour correction, additional effect slots). Still one feature away from full.
- 64 KB matches tab5, gives ~1 600 entries of headroom, survives 3–5 feature cycles.
- 128 KB would require shrinking `spiffs` below 384 KB — risks LittleFS corruption on plugin storage.

---

## 3. Deploy options and trade-offs

### Option (a) — One-shot `esptool erase_flash` + reflash per device  **[RECOMMENDED]**

**Procedure:**
1. Release firmware build with new `partitions_custom.csv`.
2. Ship release notes: "Requires clean reflash — existing presets and WiFi config will be lost." Include the standard erase+reflash command block.
3. Over serial, per device:
   ```bash
   esptool.py --port <PORT> erase_flash
   pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload
   ```
4. User reconfigures: WiFi creds via Tab5 onboarding flow, presets via iOS app "restore defaults" or manual recreate.

**Pros:** Dead simple. No migration code to maintain. Matches tab5 precedent. Zero risk of partial migration leaving a device in mixed-partition state. Ensures no stale NVS blobs carry over if a key's layout ever changes.

**Cons:** Every field device loses its saved state. For K1-at-launch (small fleet, mostly internal + captain), acceptable. For post-launch fleet, painful.

**Timing fit:** If we do this BEFORE public launch, the "fleet" is ≤10 devices — all owned by captain or in testbed. Zero end-user impact.

### Option (b) — Staged migration firmware

**Procedure:**
1. Release _migration firmware_ v1 built against the OLD partition table. On boot, it:
   - Reads all NVS namespaces, serialises them to a single blob,
   - Writes that blob to the `userdata` partition (which is unused and survives partition-table changes because it is outside the shifted range if we DON'T shift app partitions… but we must shift app partitions, so this doesn't work without rewriting to a filesystem on `spiffs` instead).
2. Release _target firmware_ v2 built against the NEW partition table. On first boot after erase+reflash, it:
   - Reads the migration blob,
   - Restores each NVS namespace.
3. User runs erase+reflash manually between v1 and v2.

**Pros:** Preserves user data.

**Cons:**
- Still requires a manual erase+reflash step — no OTA path across the partition-table change. The only saving is user-data preservation.
- Migration code is write-once-then-delete — roughly 200 LOC of serialise/deserialise for ~10 namespaces, plus versioning, plus fallback if a blob is corrupted.
- The staging partition must survive the reflash. Since we're shifting app0/app1 and therefore the whole flash layout, we'd need to stage to a partition whose offset is unchanged between old and new tables. No such partition exists in both tables — the old `userdata` at 0xE90000 is not at 0xEA0000 in the new table.
- **This option is structurally impossible without either (a) keeping app0 at 0x10000 (which means wasting the 20 KB alignment gap as before — defeats the whole point) or (b) staging to a physical flash address outside the partition table (dangerous, driver-level hack).**

**Verdict:** REJECT. Engineering cost too high for data that can be re-entered in 90 seconds.

### Option (c) — Dual-partition NVS transition window

**Procedure:**
1. Keep existing `nvs` at 0x9000 / 20 KB unchanged.
2. Add a second NVS partition `nvs_v2` at a fresh offset (e.g. reclaim from `userdata`) with 64 KB.
3. Firmware reads from `nvs_v2` first, falls back to legacy `nvs` for missing keys.
4. Every write goes to `nvs_v2`. Over time legacy `nvs` is exhausted-read-only.
5. Eventually (6+ months) release a cleanup firmware that drops the legacy partition.

**Pros:** Zero user-visible data loss. No erase+reflash required — can ship as OTA (app0/app1 unchanged).

**Cons:**
- Requires refactoring every `NVS_MANAGER.loadBlob()` / `saveBlob()` call site to call a new dual-partition wrapper (20+ call sites across 6 files).
- Dual partition lookup doubles NVS open latency for cache-miss reads.
- Legacy partition never fully decommissions without a later partition-table change — same captain-gate problem deferred, not solved.
- Complicates P1-05 fix (silent-wipe): which partition do we wipe on `NEW_VERSION_FOUND`?

**Verdict:** REJECT for launch-stage K1. Reconsider only if a post-launch fleet ever needs an NVS grow without erase+reflash.

---

## 4. Recommended option: (a), shipped with the next firmware release

**Rationale:**
1. Launch-stage fleet is small — captain + testbed. Data-loss cost is minimal.
2. Matches tab5 precedent exactly. No divergence in partition discipline across devices.
3. Build is verified PASS on the new table. Zero engineering follow-up.
4. Resolves P1-06 now rather than carrying it through launch. Any feature cycle that fills the 20 KB mid-launch forces this decision under pressure; doing it now does it under control.
5. Pairs naturally with P1-05 fix (silent-wipe telemetry) — once deployed, the louder `[NVS] ERROR:` log lines will make any future silent-wipe-adjacent events obvious in field telemetry.

**Ship window:** Next firmware release. Do NOT ship between launch-critical releases (e.g. do not include in a same-day hot-fix); this is a release-note-worthy change.

---

## 5. Exact deploy procedure

### Pre-flight (before merging to main)
1. Confirm `partitions_custom.csv` diff matches the layout table above.
2. Confirm build passes for `esp32dev_audio_esv11_k1v2_32khz` AND `esp32dev_audio_esv11_32khz` (V1 hardware).
3. Tag the pre-change commit as `stage-prenvs-grow-YYYY-MM-DD` for rollback reference.
4. Update `CHANGELOG.md` under `## [Unreleased]`:
   ```
   ### Changed
   - firmware: K1 NVS enlarged 20KB -> 64KB (matches tab5). `userdata` reduced 1024KB -> 896KB. Field devices require `esptool erase_flash` + full reflash.
   ```
5. Draft release notes (user-facing) with the exact erase+reflash command.

### Per-device deploy
1. Verify device MAC before flashing (K1 MAC: `b4:3a:45:a5:87:f8`).
   ```bash
   esptool.py --port /dev/cu.usbmodem101 chip_id
   ```
2. Erase:
   ```bash
   esptool.py --port /dev/cu.usbmodem101 erase_flash
   ```
3. Flash:
   ```bash
   pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem101
   ```
4. Verify first boot:
   ```bash
   pio device monitor -b 115200 --port /dev/cu.usbmodem101
   # expect: "[NVS] Partition stats: 0 entries used, ~1600 entries free"
   ```
5. Reconfigure WiFi, presets, OTA token via normal onboarding.

### Rollback plan
If the new firmware misbehaves post-deploy:
1. Revert `partitions_custom.csv` to the pre-change commit (`git checkout <tag> -- partitions_custom.csv`).
2. Rebuild + `erase_flash` + reflash. Same 90-second procedure. No flash state preserved in either direction, which is the whole reason this is captain-gated.

### Post-deploy (fleet hygiene)
1. Append a "Deployed" entry with the date to the Document Changelog below.
2. Move this document to `firmware-v3/docs/` as an archived captain decision (rename `nvs-partition-grow-captain-decision-2026-04-18.md` → `nvs-partition-grow-decision-2026-04-18.md` once accepted).

---

## 6. Field-device impact matrix (what gets lost per path)

| User data             | Option (a) erase+reflash | Option (b) staged | Option (c) dual  |
|-----------------------|--------------------------|-------------------|------------------|
| WiFi credentials      | LOST                     | preserved (if mig works) | preserved |
| API auth key          | LOST                     | preserved         | preserved        |
| OTA token             | LOST                     | preserved         | preserved        |
| Effect presets        | LOST                     | preserved         | preserved        |
| Zone config + presets | LOST                     | preserved         | preserved        |
| Audio tuning          | LOST                     | preserved         | preserved        |
| ColourCorrection      | LOST                     | preserved         | preserved        |
| EdgeMixer             | LOST                     | preserved         | preserved        |
| Plugin blobs (LittleFS)| preserved (spiffs untouched\*) | preserved | preserved |

\* spiffs partition physical offset shifts (0xE10000 → 0xE20000) but an erase_flash clears the whole device anyway, so plugin state on disk is also lost under option (a). However, LittleFS is usually redeployable from the iOS/Tab5 side; it is not authoritative user data.

---

## 7. Open questions for captain

1. **Ship with release N or release N+1?** If N is a same-day hot-fix, defer. If N is a planned release with test window, include.
2. **Release notes audience:** do we send an email to testbed owners before they power up the next firmware? Suggested: yes, one-line note "this release erases your presets — recreate from defaults."
3. **Tab5 and iOS sides:** no changes needed. Both connect fresh to the K1 AP and re-upload presets. But confirm iOS app handles "K1 responded with empty preset list" gracefully on first connection after reflash.
4. **Do we want to land P1-05 (silent-wipe telemetry) simultaneously?** Recommended yes — the two are topically adjacent and both live in the NVS subsystem. Bundled release reduces fleet churn.

---

## 8. What this document does NOT do

- Does NOT flash any device.
- Does NOT modify the real (non-sandbox) tree.
- Does NOT commit the sandbox change.
- Does NOT advocate a ship decision — it presents options with trade-offs. The ship/hold call is the captain's.

---

**Document Changelog**

| Date       | Author                    | Change                                                                 |
|------------|---------------------------|------------------------------------------------------------------------|
| 2026-04-18 | agent:embedded-firmware   | Created. Sandbox build PASS. Layout table + NVS inventory + 3 options. |
