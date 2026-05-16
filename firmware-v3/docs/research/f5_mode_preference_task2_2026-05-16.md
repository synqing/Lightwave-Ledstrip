# F-5 Task 2 — NVS WiFi Mode Preference Evidence

Date: 2026-05-16
Device: K1v2, MAC `b4:3a:45:a5:87:f8`
Build: `esp32dev_audio_esv11_k1v2_32khz_sta_validation`

## Scope

BACKLOG F-5 Task 2 requires a `mode` field in the `wifi_creds` NVS namespace so K1 can boot in the user's preferred WiFi mode: AP or pure STA, never concurrent AP+STA.

This pass does not add captive-portal provisioning UI and does not change the production `WIFI_AP_ONLY` default.

## Source Changes

- `WiFiCredentialsStorage` now owns `wifi_creds/mode` with stable values `ap` and `sta`.
- `WiFiManager::begin()` forces AP in `WIFI_AP_ONLY` builds and honours the stored preference only in non-`WIFI_AP_ONLY` validation builds.
- Successful pure STA connection persists `sta`; explicit AP request persists `ap`.
- `WebServer` treats a connecting `WIFI_MODE_STA` boot as STA, starts the HTTP server immediately, and defers mDNS until DHCP assigns a real STA IP.
- REST network status and serial `wifi` status expose `bootModePreference`.

## Build Evidence

Commands:

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
pio run -e esp32dev_audio_esv11_k1v2_32khz_sta_validation
```

Results:

- Production AP-only build: PASS. RAM `126812` bytes, flash `2513453` bytes.
- STA-validation build: PASS. RAM `126820` bytes, flash `2515897` bytes.

## Hardware Evidence

Upload:

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz_sta_validation -t upload --upload-port /dev/cu.usbmodem2101
```

Result: PASS. esptool identified MAC `b4:3a:45:a5:87:f8`.

AP default / unset mode:

- Serial `wifi` after first boot reported `Mode: AP`.
- `Boot Preference: ap`.
- AP IP `192.168.4.1`.
- Saved network list present; SSID redacted from this public artefact.

STA preference persistence:

- Serial `wifi connect` using saved NVS credentials entered pure STA.
- Device logged `Boot WiFi mode preference saved: sta`.
- Serial `wifi` reported `Mode: STA`, `Boot Preference: sta`, `Connected: YES`, IP `192.168.1.106`, AP clients `0`.

STA-preferred reboot:

- After hard reset, boot selected saved STA credentials without a serial connect command.
- Device logged smart network selection, pure STA connection, `Got IP - 192.168.1.106`, and mDNS service registration at the STA IP.
- Serial `wifi` reported `Mode: STA`, `Boot Preference: sta`, `Connected: YES`, AP clients `0`.
- Host checks:
  - `curl -4 --noproxy '*' http://192.168.1.106/api/v1/network/status` returned HTTP `200` with `apMode:false` and `bootModePreference:"sta"`.
  - `dscacheutil -q host -a name lightwaveos.local` resolved `192.168.1.106`.
  - `ping -c 2 lightwaveos.local` returned `2/2` replies.
  - `curl -4 --noproxy '*' http://lightwaveos.local/api/v1/network/status` returned HTTP `200` with `apMode:false` and `bootModePreference:"sta"`.

AP preference persistence:

- Serial `wifi ap` persisted `ap`, disconnected STA, and started SoftAP.
- Serial `wifi` reported `Mode: AP`, `Boot Preference: ap`, `Connected: NO`, AP IP `192.168.4.1`.
- After hard reset, serial `wifi` again reported `Mode: AP` and `Boot Preference: ap`.

## Residual Risk

- F-5 Task 4 remains partial: this proves one-router K1v2 mode-preference behaviour, not multiple-router pure-STA coverage.
- F-5 Task 3 captive-portal provisioning UI remains unimplemented.
- `firmware-v3/src/network/CLAUDE.md` still contains stale AP-only doctrine; root `BACKLOG.md` and root `AGENTS.md` are the current authority for F-5.
