# LightwaveOS Stress Harness

`tools/web/stress_harness.py` is the repeatable load test runner for:
- REST load
- WebSocket load
- mixed control-plane load
- serial low-heap/shedding telemetry

It writes both Markdown + JSON reports under `docs/planning/perf_runs/`.

## Setup

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip
python3 -m venv .venv-perf
. .venv-perf/bin/activate
pip install websocket-client pyserial
```

## Run

One-shot via Make (recommended):

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tools
make perf-quick
make perf-typical
make perf-stress
make perf-serial
```

Override target host/WS/serial when needed:

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tools
make perf-stress \
  PERF_HOST=http://192.168.1.123 \
  PERF_WS_URL=ws://192.168.1.123/ws \
  PERF_SERIAL_PORT=/dev/cu.usbmodem212401
```

Serial-only burn-in (no REST/WS access required):

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tools
make perf-serial PERF_SERIAL_PORT=/dev/cu.usbmodem212401 PERF_SERIAL_DURATION=600
```

Direct CLI (equivalent):

Quick sanity:

```bash
. .venv-perf/bin/activate
python3 tools/web/stress_harness.py \
  --host http://192.168.1.101 \
  --ws-url ws://192.168.1.101/ws \
  --serial-port /dev/cu.usbmodem212401 \
  --profile quick
```

Typical baseline:

```bash
. .venv-perf/bin/activate
python3 tools/web/stress_harness.py \
  --host http://192.168.1.101 \
  --ws-url ws://192.168.1.101/ws \
  --serial-port /dev/cu.usbmodem212401 \
  --profile typical
```

Stress profile:

```bash
. .venv-perf/bin/activate
python3 tools/web/stress_harness.py \
  --host http://192.168.1.101 \
  --ws-url ws://192.168.1.101/ws \
  --serial-port /dev/cu.usbmodem212401 \
  --profile stress
```

Serial-only CLI:

```bash
. .venv-perf/bin/activate
python3 tools/web/stress_harness.py \
  --serial-only \
  --serial-port /dev/cu.usbmodem212401 \
  --serial-duration 600
```

## Built-in Gates

`FAIL` if any scenario has:
- uptime reset detected
- REST error rate > 30%
- WS error rate > 30%
- fatal serial indicators (`Guru Meditation`, `Backtrace`, `abort()`, `rst:0x`, `Rebooting...`)

`PASS` otherwise.

Non-fatal note:
- internal heap dip below 12KB is highlighted in report notes.

## Output

Reports are emitted to:
- `docs/planning/perf_runs/stress_harness_<profile>_<timestamp>.md`
- `docs/planning/perf_runs/stress_harness_<profile>_<timestamp>.json`
