#!/usr/bin/env node
/**
 * K1 Wired Proxy — USB serial bridge for K1 Composer (wired-first discovery)
 *
 * Probes for a K1 Lightwave device on USB serial (115200 baud, JSON line protocol),
 * then exposes a local HTTP + WebSocket server so the K1 Composer dashboard can
 * connect to localhost:8765 without WiFi.
 *
 * Repurposes: firmware-v3 main.cpp serial JSON gateway, PRISM serial bridge
 * normalisation, trinity-bridge host-side proxy pattern.
 *
 * Usage:
 *   node k1-wired-proxy.mjs [--port 8765] [--serial-path /dev/cu.usbmodem*]
 *   node k1-wired-proxy.mjs --probe-only   # List ports and probe, no server
 */

import { createServer } from "node:http";
import { SerialPort } from "serialport";
import { ReadlineParser } from "@serialport/parser-readline";
import WebSocket from "ws";

const DEFAULT_PORT = 8765;
const SERIAL_BAUD = 115200;
const PROBE_TIMEOUT_MS = 2500;
const SERIAL_REQUEST_TIMEOUT_MS = 3000;

function usage(exitCode = 1) {
  console.error(
    [
      "Usage:",
      "  k1-wired-proxy.mjs [--port 8765] [--serial-path PATH] [--probe-only]",
      "",
      "  --port N          Local HTTP/WS server port (default: 8765)",
      "  --serial-path P   Use this serial port (default: auto-detect K1)",
      "  --probe-only      Only list ports and probe; do not start server",
      "",
      "With no --serial-path, the first port that responds to device.getStatus is used.",
    ].join("\n")
  );
  process.exit(exitCode);
}

function parseArgs(argv) {
  const args = { port: DEFAULT_PORT, serialPath: null, probeOnly: false };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--port") args.port = parseInt(argv[++i], 10) || DEFAULT_PORT;
    else if (a === "--serial-path") args.serialPath = argv[++i] ?? null;
    else if (a === "--probe-only") args.probeOnly = true;
    else if (a === "--help" || a === "-h") usage(0);
  }
  return args;
}

/**
 * Probe a serial port: send device.getStatus, expect JSON line with "type" (and ideally "data").
 */
async function probePort(path) {
  return new Promise((resolve) => {
    const port = new SerialPort({ path, baudRate: SERIAL_BAUD }, (err) => {
      if (err) {
        resolve(null);
        return;
      }
    });

    const parser = port.pipe(new ReadlineParser({ delimiter: "\n" }));
    parser.on("data", () => {});

    const timeout = setTimeout(() => {
      port.close(() => resolve(null));
    }, PROBE_TIMEOUT_MS);

    port.write(JSON.stringify({ type: "device.getStatus" }) + "\n", (err) => {
      if (err) {
        clearTimeout(timeout);
        port.close(() => resolve(null));
        return;
      }
    });

    parser.once("data", (buf) => {
      const str = buf.toString("utf8").trim();
      try {
        const msg = JSON.parse(str);
        if (msg.type === "device.getStatus" || msg.type === "error") {
          clearTimeout(timeout);
          port.close(() => resolve({ path, msg }));
        }
      } catch (_) {}
    });
  });
}

/**
 * Auto-detect K1: list ports, probe each until one responds to device.getStatus.
 */
async function findK1Port(explicitPath) {
  if (explicitPath) {
    const result = await probePort(explicitPath);
    return result ? result.path : null;
  }

  const ports = await SerialPort.list();
  for (const p of ports) {
    const path = p.path;
    if (!path) continue;
    const result = await probePort(path);
    if (result) return path;
  }
  return null;
}

async function main() {
  const args = parseArgs(process.argv);

  console.error("[k1-wired-proxy] Probing for K1 Lightwave on USB serial...");
  const portPath = await findK1Port(args.serialPath);

  if (!portPath) {
    console.error("[k1-wired-proxy] No K1 device found. Plug in the device via USB and try again.");
    process.exit(1);
  }

  console.error(`[k1-wired-proxy] Found K1 at ${portPath}`);

  if (args.probeOnly) {
    console.log(portPath);
    process.exit(0);
  }

  const serial = new SerialPort({ path: portPath, baudRate: SERIAL_BAUD }, (err) => {
    if (err) {
      console.error("[k1-wired-proxy] Failed to open serial:", err.message);
      process.exit(1);
    }
  });

  const readlineParser = serial.pipe(new ReadlineParser({ delimiter: "\n" }));
  const pendingRequests = new Map();
  let requestIdCounter = 0;

  readlineParser.on("data", (buf) => {
    const str = buf.toString("utf8").trim();
    if (!str) return;
    try {
      const msg = JSON.parse(str);
      const rid = msg.requestId;
      if (rid != null && pendingRequests.has(rid)) {
        const { resolve } = pendingRequests.get(rid);
        pendingRequests.delete(rid);
        resolve(msg);
        return;
      }
      for (const client of wss.clients) {
        if (client.readyState === WebSocket.OPEN) client.send(str);
      }
    } catch (_) {}
  });

  function serialRequestWithId(type, data = {}) {
    const requestId = `proxy-${++requestIdCounter}-${Date.now()}`;
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        if (pendingRequests.delete(requestId)) reject(new Error(`Timeout: ${type}`));
      }, SERIAL_REQUEST_TIMEOUT_MS);
      pendingRequests.set(requestId, {
        resolve: (msg) => {
          clearTimeout(timeout);
          resolve(msg);
        },
      });
      const line = JSON.stringify({ type, requestId, ...data }) + "\n";
      serial.write(line, (err) => {
        if (err) {
          pendingRequests.delete(requestId);
          clearTimeout(timeout);
          reject(err);
        }
      });
    });
  }

  const corsHeaders = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type",
  };

  const httpServer = createServer((req, res) => {
    if (req.method === "OPTIONS") {
      res.writeHead(204, corsHeaders);
      res.end();
      return;
    }

    const setCors = () => {
      Object.entries(corsHeaders).forEach(([k, v]) => res.setHeader(k, v));
    };

    const url = req.url ?? "";
    if (url === "/api/v1/device/status" && req.method === "GET") {
      serialRequestWithId("device.getStatus")
        .then((msg) => {
          const body = msg.data != null ? JSON.stringify(msg.data) : JSON.stringify(msg);
          setCors();
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(body);
        })
        .catch((err) => {
          setCors();
          res.writeHead(502, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: err.message }));
        });
      return;
    }

    if (url === "/api/v1/control/status" && req.method === "GET") {
      serialRequestWithId("control.status")
        .then((msg) => {
          // Firmware does not implement control.status over serial (WebSocket only); device returns "unknown command type"
          const isUnknownCommand =
            msg.type === "error" &&
            (msg.error || "").toLowerCase().includes("unknown command");
          const body = isUnknownCommand
            ? JSON.stringify({
                active: false,
                remainingMs: 0,
                _serialUnsupported: true,
              })
            : msg.data != null
              ? JSON.stringify(msg.data)
              : JSON.stringify(msg);
          setCors();
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(body);
        })
        .catch((err) => {
          setCors();
          res.writeHead(502, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: err.message }));
        });
      return;
    }

    setCors();
    res.writeHead(404, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ error: "Not found" }));
  });

  const wss = new WebSocket.Server({ noServer: true });
  wss.on("connection", (ws) => {
    ws.on("message", (data) => {
      const str = typeof data === "string" ? data : data.toString("utf8");
      if (str.trim()) serial.write(str.trimEnd() + "\n");
    });
  });

  httpServer.on("upgrade", (request, socket, head) => {
    if (request.url === "/ws") {
      wss.handleUpgrade(request, socket, head, (ws) => wss.emit("connection", ws, request));
    } else {
      socket.destroy();
    }
  });

  httpServer.listen(args.port, "127.0.0.1", () => {
    console.error(`[k1-wired-proxy] Listening on http://127.0.0.1:${args.port}`);
    console.error(`[k1-wired-proxy] K1 Composer can connect to http://localhost:${args.port}`);
  });
}

main().catch((err) => {
  console.error("[k1-wired-proxy]", err);
  process.exit(1);
});
