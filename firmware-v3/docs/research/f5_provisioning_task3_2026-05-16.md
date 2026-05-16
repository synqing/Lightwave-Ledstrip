# F-5 Provisioning Task 3 Evidence — 2026-05-16

RBDO: DEGRADED-MODE

- Unresolved assumption: AP captive-portal HTTP behaviour could not be validated from this Mac because joining `LightwaveOS-AP` would terminate the active control session.
- Risk if wrong: the AP root page, captive DNS wildcard, browser-probe redirect, or AP-mode `/api/v1/network/provision` success path may still fail for real AP clients.
- Fallback: this pass is source/build/native/USB-serial/LAN-STA refusal evidence only, not full captive-portal end-to-end sign-off.
- Revisit trigger: Captain validates from a separate device, or a second safe network interface/machine can join `LightwaveOS-AP` without disrupting the Codex control path.
- Debt count / affected outputs: one output, F-5 Task 3 acceptance status.

## Scope

F-5 Task 3 only: first-boot provisioning UI for STA credential entry in `LW_STA_VALIDATION_BUILD`.

This pass did not change host WiFi association, host routes, DNS, service order, or internet path. This pass did not join `LightwaveOS-AP` from the Mac. This pass did not validate captive-portal browser behaviour through `192.168.4.1`.

## Source Evidence

Changed F-5 files:

- `docs/protocol/k1-rest-contract.yaml`
- `firmware-v3/src/network/RequestValidator.h`
- `firmware-v3/src/network/WebServer.cpp`
- `firmware-v3/src/network/WebServer.h`
- `firmware-v3/src/network/WiFiManager.cpp`
- `firmware-v3/src/network/WiFiManager.h`
- `firmware-v3/src/network/webserver/StaticAssetRoutes.cpp`
- `firmware-v3/src/network/webserver/V1ApiRoutes.cpp`
- `firmware-v3/src/network/webserver/handlers/NetworkHandlers.cpp`
- `firmware-v3/src/network/webserver/handlers/NetworkHandlers.h`
- `firmware-v3/test/test_network_provision_contract/test_network_provision_contract.cpp`

Implemented surfaces:

- `POST /api/v1/network/provision` is added to the REST contract as `auth: false`, HTTP `202` on success, and `LW_STA_VALIDATION_BUILD`-only.
- `RequestSchemas::NetworkProvision` validates `ssid` as required `1-32` chars and `password` as optional `0-64` chars.
- `V1ApiRoutes` registers `/api/v1/network/provision` without API-key auth and with route-level rate limiting.
- `NetworkHandlers::handleProvision()` refuses `WIFI_AP_ONLY` builds, refuses non-`LW_STA_VALIDATION_BUILD` builds, refuses non-AP WiFi mode, saves credentials, persists `sta`, and schedules restart.
- `StaticAssetRoutes` serves the provisioning page at `/` only when `LW_STA_VALIDATION_BUILD` is active, `WIFI_AP_ONLY` is absent, and the device is in `WIFI_MODE_AP`.
- Captive DNS is compiled only when `LW_STA_VALIDATION_BUILD` is active and `WIFI_AP_ONLY` is absent.
- No new concurrent AP+STA path was added.

Known source caveat:

- If restart task scheduling fails after credentials and boot preference are saved, the handler returns an error after partial persistence. This is not full transactionality; it is recorded here as residual risk.

## Validation

### Static Checks

```text
git diff --check -- docs/protocol/k1-rest-contract.yaml firmware-v3/src/network/RequestValidator.h firmware-v3/src/network/WebServer.cpp firmware-v3/src/network/WebServer.h firmware-v3/src/network/WiFiManager.cpp firmware-v3/src/network/WiFiManager.h firmware-v3/src/network/webserver/StaticAssetRoutes.cpp firmware-v3/src/network/webserver/V1ApiRoutes.cpp firmware-v3/src/network/webserver/handlers/NetworkHandlers.cpp firmware-v3/src/network/webserver/handlers/NetworkHandlers.h firmware-v3/test/test_network_provision_contract/test_network_provision_contract.cpp
PASS
```

### Native Test

```text
cd firmware-v3
pio test -e native_codec_test_ws --filter test_network_provision_contract
PASS: 3/3
```

Covered:

- valid SSID with empty password passes schema validation;
- missing SSID fails schema validation;
- SSID longer than 32 chars fails schema validation.

Non-gating note:

- `pio test -e native_test_ws_router` was attempted and is not a valid focused gate for this patch. It collected 54 unrelated suites and failed on pre-existing missing-symbol/include issues outside F-5. It is not used as Task 3 evidence.

### Builds

```text
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
PASS
RAM:   126812 / 327680 bytes
Flash: 2514117 / 7340032 bytes
```

```text
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz_sta_validation
PASS
RAM:   126820 / 327680 bytes
Flash: 2522749 / 7340032 bytes
```

### Upload And USB Serial

Target MAC was verified before upload:

```text
~/.platformio/penv/bin/python -m esptool --chip esp32s3 --port /dev/cu.usbmodem2101 read_mac
MAC: b4:3a:45:a5:87:f8
```

Upload:

```text
pio run -e esp32dev_audio_esv11_k1v2_32khz_sta_validation -t upload --upload-port /dev/cu.usbmodem2101
PASS
MAC: b4:3a:45:a5:87:f8
```

Passive serial `s` after upload:

```text
Heap: 8084591 / min 8078051 bytes
SPIRAM free: 8060743 bytes
Effect: 4866 (K1 Waveform)
FPS: 108 (target: 120)
Frames: 1295, Drops: 1250
Frame time: avg=9209, min=8265, max=33089 us
LED show: avg=6166, max=7482 us, skips=0
Stack watermark: 11356 words
Has show: NO
```

Passive serial `wifi` after upload:

```text
Mode: STA
Boot Preference: sta
Connected: YES
SSID: VX220-013F
IP: 192.168.1.106
RSSI: -79 dBm
Channel: 6
AP IP: 192.168.4.1
AP Clients: 0
Saved Networks (NVS): 1
  [0] VX220-013F
```

### Existing-LAN HTTP

No host network changes were made. The device was already reachable over the existing LAN route.

```text
dscacheutil -q host -a name lightwaveos.local
name: lightwaveos.local
ip_address: 192.168.1.106
```

```text
ping -c 2 -W 1000 192.168.1.106
2 packets transmitted, 2 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = 602.189/606.519/610.849/4.330 ms
```

```text
curl -4 --noproxy '*' --max-time 15 http://lightwaveos.local/api/v1/network/status
HTTP/1.1 200 OK
{"success":true,"data":{"connected":true,"state":"CONNECTED","apMode":false,"bootModePreference":"sta","ssid":"VX220-013F","ip":"192.168.1.106","rssi":-79,"channel":6,"stats":{"connectionAttempts":1,"successfulConnections":1,"uptimeSeconds":77}},"timestamp":85082,"version":"2.0"}
```

STA-mode provisioning refusal:

```text
curl -4 --noproxy '*' --max-time 15 -H 'Content-Type: application/json' -X POST http://lightwaveos.local/api/v1/network/provision --data '{"ssid":"ShouldNotSave","password":"valid-password"}'
HTTP/1.1 503 Service Unavailable
{"success":false,"error":{"code":"OPERATION_FAILED","message":"Provisioning is only available from AP mode"},"timestamp":98891,"version":"2.0"}
```

Follow-up passive serial `wifi` showed the attempted SSID was not saved:

```text
Saved Networks (NVS): 1
  [0] VX220-013F
```

## Unverified

The following are still unverified because validating them from this Mac requires host association with `LightwaveOS-AP`, which is forbidden for control-channel safety:

- AP captive portal root page over `http://192.168.4.1/`;
- captive DNS wildcard handling;
- browser probe redirect behaviour;
- AP-mode `/api/v1/network/provision` success path;
- browser form submission and post-save restart from an AP client.

Safe options for closing those items:

1. Captain validates from another device already allowed to join `LightwaveOS-AP`.
2. Use a second network interface or separate machine.
3. Keep this commit as source/build/USB/LAN-STA-refusal evidence and defer AP HTTP validation.
