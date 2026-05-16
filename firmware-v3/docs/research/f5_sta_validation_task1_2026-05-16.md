# F-5 Task 1 Pure-STA Validation Evidence - 2026-05-16

## Scope

BACKLOG F-5 Task 1: replace concurrent AP+STA paths with pure STA before STA-only validation.

This evidence covers the K1v2 validation-only build path. It does not claim F-5 Tasks 2-3:
NVS mode preference and first-boot provisioning UI remain separate backlog work.

## Source Changes Validated

- Production K1v2 env remains `esp32dev_audio_esv11_k1v2_32khz` with `WIFI_AP_ONLY=1`.
- Validation env added: `esp32dev_audio_esv11_k1v2_32khz_sta_validation`.
- `WiFiManager` now enters `WIFI_MODE_STA` for explicit STA validation connects and `WIFI_MODE_AP`
  for AP fallback/request paths; no active F-5 connection path sets `WIFI_MODE_APSTA`.
- REST connect honours the request `save` flag; serial `wifi connect SSID PASS` keeps its save-by-default behaviour.

## Verification Commands

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
pio run -e esp32dev_audio_esv11_k1v2_32khz_sta_validation
pio run -e esp32dev_audio_esv11_k1v2_32khz_sta_validation -t upload --upload-port /dev/cu.usbmodem2101
```

## Build Results

- Production K1v2 AP-only build: PASS, `RAM 126812 / 327680`, `Flash 2512037 / 7340032`.
- K1v2 STA validation build: PASS, `RAM 126820 / 327680`, `Flash 2514065 / 7340032`.

## Upload Target

- Port: `/dev/cu.usbmodem2101`
- MAC: `b4:3a:45:a5:87:f8`
- Upload result: PASS, hash verified, hard reset via RTS.

## Hardware STA Evidence

Serial validation used saved NVS credentials. SSID and password are intentionally not recorded here.

Observed serial sequence:

- Initial mode before connect: AP.
- Command: `wifi connect <saved-ssid>`.
- Result: `Connection initiated (pure STA)`.
- `WiFi` log: connected to upstream AP, got IP `192.168.1.106`.
- `WebServer` log: mDNS re-registered `lightwaveos.local` at `192.168.1.106`.
- Post-soak serial `wifi` status: Mode `STA`, Connected `YES`, AP clients `0`.

Host checks:

```bash
curl -4 --noproxy '*' --max-time 5 http://lightwaveos.local/api/v1/network/status
dscacheutil -q host -a name lightwaveos.local
ping -c 2 -W 1000 lightwaveos.local
```

Results:

- `curl -4` to `lightwaveos.local`: HTTP 200.
- `dscacheutil`: `lightwaveos.local` resolved to `192.168.1.106`.
- `ping`: 2/2 packets received, 0.0% loss.

## Soak Result

Command sampled `/api/v1/network/status` once per minute for 30 samples using IPv4 mDNS:

```bash
curl -4 --noproxy '*' --max-time 5 http://lightwaveos.local/api/v1/network/status
```

Summary:

- Samples: 30/30 HTTP 200.
- State: `CONNECTED` for every sample.
- AP mode: `false` for every sample.
- Connection counters: `connectionAttempts=1`, `successfulConnections=1` for every sample.
- Uptime increased from `81` seconds at sample 1 to `1831` seconds at sample 30.
- RSSI range during sampled soak: `-89` dBm to `-77` dBm.

## Residual Risk

- The validation env still boots into AP and then enters pure STA on explicit validation connect.
  Boot-time mode preference is F-5 Task 2 and was not implemented here.
- Plain `curl http://lightwaveos.local/...` without `-4` timed out on this Mac during one check,
  while `dscacheutil`, `ping`, and `curl -4` all resolved/used `lightwaveos.local` successfully.
