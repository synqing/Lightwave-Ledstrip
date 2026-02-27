# WebSocket Hardware Test Scripts

Hardware test scripts for validating WS connection stability and detecting regressions between v2 and Tab5 devices.

## Overview

These scripts perform hardware-level testing of WebSocket connections to ensure:
- No origin validation rejections (regression from recent changes)
- No handler table overflow (regression from recent changes)
- Stable WS connections between v2 and Tab5 devices
- Successful command routing and execution

## Scripts

### v2 Device: `ws_hardware_check.py`

Tests the v2 device's WebSocket server:
- Connects via mDNS (`lightwaveos.local`) and/or direct IP
- Sends key WS commands: `getStatus`, `ledStream.subscribe/unsubscribe`, `plugins.list`, `ota.check`
- Captures serial logs and validates no regression indicators
- Verifies handler registration success

**Usage:**
```bash
cd firmware/v2/tools
python3 ws_hardware_check.py --host lightwaveos.local
python3 ws_hardware_check.py --host 192.168.0.27
python3 ws_hardware_check.py --host lightwaveos.local --ip 192.168.0.27
```

**Options:**
- `--host`: mDNS hostname or IP (default: `lightwaveos.local`)
- `--ip`: Direct IP address to test (in addition to --host)
- `--port`: WebSocket port (default: 80)
- `--serial-port`: Serial port for v2 device (default: `/dev/cu.usbmodem1101`)
- `--duration`: Serial capture duration in seconds (default: 30.0)
- `--output`: Output file for serial logs

### Tab5 Device: `ws_hardware_check.py`

Monitors Tab5 encoder's WebSocket client:
- Monitors serial output for WS connection events
- Detects rapid disconnect/reconnect cycles (connection instability)
- Validates successful connection and hello message exchange
- Tracks connection statistics over test duration

**Usage:**
```bash
cd firmware/Tab5.encoder/scripts
python3 ws_hardware_check.py
python3 ws_hardware_check.py --duration 120
python3 ws_hardware_check.py --output tab5_ws_log.txt
```

**Options:**
- `--serial-port`: Serial port for Tab5 device (default: `/dev/cu.usbmodem101`)
- `--duration`: Monitoring duration in seconds (default: 60.0)
- `--output`: Output file for serial logs
- `--reconnect-threshold`: Threshold in seconds for rapid reconnect detection (default: 5.0)

### OTA WS Testing: `capture_ota_ws_trace_direct.py`

Reuses existing OTA WS tool for comprehensive OTA flow testing:
- Located at: `firmware/v2/specs/quint/choreo/tools/capture_ota_ws_trace_direct.py`
- Tests OTA WS scenarios: happy path, reconnect mid-transfer, abort and retry
- Captures serial logs and extracts JSONL telemetry

**Usage:**
```bash
cd firmware/v2/specs/quint/choreo/tools
python3 capture_ota_ws_trace_direct.py ota_ws_happy_path ./output_dir
```

## Shared Failure Signatures

These patterns in serial logs indicate **regressions** and cause test failure:

### v2 Device Failures
- `origin_not_allowed` - Origin validation rejection (should be disabled)
- `Handler table full` - Command handler registration overflow
- `ERROR: Handler table full` - Handler registration error

### Tab5 Device Failures
- `origin_not_allowed` - Connection rejected due to origin
- `Connection lost` - Unexpected disconnection
- `Reconnecting` - Frequent reconnection attempts
- `ERROR` - General error indicators
- `Failed to connect` - Connection failure

### Connection Instability
- **Rapid reconnects**: Multiple disconnects within 5 seconds
- **Connection churn**: Connect → disconnect → connect → disconnect cycles
- **No connections**: Zero successful connections during monitoring period

## Shared Success Signatures

These patterns indicate **expected behavior** and contribute to test pass:

### v2 Device Success
- `WS: Client ... connected` - Successful client connection
- `WebSocket commands registered: X/Y handlers` - Handler registration success

### Tab5 Device Success
- `[WS] Connected to server` - Successful connection to v2
- `[WS] Sending hello` - Hello message exchange
- `[WS] Server IP: ...` - Connection details logged

## Test Coverage

### Required Test Scenarios

1. **v2 + Tab5 same WiFi**
   - Both devices on primary SSID
   - Tab5 connects to v2 via mDNS or direct IP
   - Validate stable WS connection, no origin rejection, no handler overflow

2. **mDNS + Direct IP**
   - Test both `ws://lightwaveos.local/ws` and explicit IP connection
   - Verify both paths work correctly

3. **OTA WS Flows**
   - Use existing `capture_ota_ws_trace_direct.py` tool
   - Test `ota.check/begin/chunk/verify` scenarios
   - Validate OTA commands are registered (not dropped due to handler overflow)

## Dependencies

Both scripts require:
- Python 3.6+
- `pyserial` - For serial port communication
- `websocket-client` - For WebSocket connections (v2 script only)

**Install:**
```bash
pip3 install pyserial websocket-client
```

## Exit Codes

- `0` - All checks passed, no regressions detected
- `1` - Failures detected (regression indicators, connection issues, etc.)

## Integration with CI/CD

These scripts can be integrated into hardware test workflows:
1. Flash latest firmware to both devices
2. Run v2 script to validate server-side WS health
3. Run Tab5 script to validate client-side WS stability
4. Run OTA WS scenarios if OTA features are enabled
5. Fail build if any script exits non-zero

## Troubleshooting

**Serial port not found:**
- Check device connections: `ls -la /dev/cu.usbmodem*`
- Verify ports match AGENTS.md: v2=`/dev/cu.usbmodem1101`, Tab5=`/dev/cu.usbmodem101`
- Ensure no other process is using the serial port

**No WS connections:**
- Verify WiFi connectivity on both devices
- Check v2 device IP: `pio device monitor -p /dev/cu.usbmodem1101 -b 115200` and look for "STA IP"
- Verify mDNS resolution: `ping lightwaveos.local`

**Handler table full errors:**
- Check v2 serial logs for handler registration count
- Verify `MAX_HANDLERS` in `WsCommandRouter.h` is sufficient (currently 128)
- Review handler registration in `WebServer::setupWebSocket()`

**Origin rejection errors:**
- Verify Origin validation is disabled in `WebServer::begin()`
- Check that `m_ws->handleHandshake()` is not registered
- Review `WsGateway::validateOrigin()` implementation

## See Also

- [AGENTS.md](../../../AGENTS.md) - Device port mappings and build commands
- [WebServer v2 Refactor](../../../docs/architecture/WEB_SERVER_V2_REFACTOR.md) - WebSocket architecture
- [OTA WS Trace Tool](../../specs/quint/choreo/tools/capture_ota_ws_trace_direct.py) - OTA testing tool
