#!/usr/bin/env node
/**
 * Trinity → Lightwave bridge (host-side)
 *
 * Reads Trinity WebSocket messages as JSON Lines from stdin and forwards them
 * to a LightwaveOS device WebSocket (`/ws`). Optionally applies segment-based
 * semantic mappings (e.g., section labels → effect/palette/parameter actions).
 *
 * No external dependencies: uses Node's built-in `WebSocket` + `readline`.
 */

import fs from 'node:fs';
import readline from 'node:readline';

function usage(exitCode = 1) {
  // Keep this short; README covers details.
  console.error(
    [
      'Usage:',
      '  trinity-bridge.mjs --device <host|wsUrl> [--mapping <file.json>] [--dry-run] [--no-forward]',
      '',
      'Examples:',
      '  cat messages.jsonl | node tools/trinity-bridge/trinity-bridge.mjs --device lightwaveos.local',
      '  cat messages.jsonl | node tools/trinity-bridge/trinity-bridge.mjs --device ws://192.168.0.10/ws --mapping tools/trinity-bridge/example-mapping.json',
    ].join('\n')
  );
  process.exit(exitCode);
}

function parseArgs(argv) {
  const args = { device: null, mapping: null, dryRun: false, forward: true, verbose: false };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--device') args.device = argv[++i] ?? null;
    else if (a === '--mapping') args.mapping = argv[++i] ?? null;
    else if (a === '--dry-run') args.dryRun = true;
    else if (a === '--no-forward') args.forward = false;
    else if (a === '--verbose') args.verbose = true;
    else if (a === '--help' || a === '-h') usage(0);
    else usage(1);
  }
  if (!args.device) usage(1);
  return args;
}

function normaliseDeviceWsUrl(deviceArg) {
  const s = String(deviceArg).trim();
  if (s.startsWith('ws://') || s.startsWith('wss://')) return s;
  if (s.startsWith('http://')) return `ws://${s.slice('http://'.length).replace(/\/+$/, '')}/ws`;
  if (s.startsWith('https://')) return `wss://${s.slice('https://'.length).replace(/\/+$/, '')}/ws`;
  return `ws://${s.replace(/\/+$/, '')}/ws`;
}

function compileSegmentRules(mapping) {
  const rules = Array.isArray(mapping?.segmentRules) ? mapping.segmentRules : [];
  return rules
    .map((r) => {
      const match = r?.match;
      let matcher = null;
      if (typeof match === 'string') {
        matcher = (label) => label === match;
      } else if (match && typeof match === 'object' && typeof match.pattern === 'string') {
        const flags = typeof match.flags === 'string' ? match.flags : '';
        const re = new RegExp(match.pattern, flags);
        matcher = (label) => re.test(label);
      }
      const actions = Array.isArray(r?.actions) ? r.actions : [];
      if (!matcher || actions.length === 0) return null;
      return { matcher, actions, name: r?.name ?? 'rule' };
    })
    .filter(Boolean);
}

function canonicalSegmentLabel(label) {
  if (!label) return '';
  const s = String(label).trim().toLowerCase();
  if (!s) return '';

  // Mirror PRISM_Dashboard_V4 canonicalisation so host-side rules stay stable.
  if (s.startsWith('verse')) return 'verse';
  if (s.startsWith('chorus')) return 'chorus';
  if (s.startsWith('intro')) return 'intro';
  if (s.startsWith('outro')) return 'end';
  if (s === 'end' || s.startsWith('end')) return 'end';
  if (s.startsWith('solo')) return 'solo';
  if (s.startsWith('inst')) return 'inst';
  if (s.startsWith('bridge')) return 'bridge';
  if (s.startsWith('breakdown')) return 'breakdown';
  if (s.startsWith('drop')) return 'drop';
  if (s.startsWith('start')) return 'start';

  return s.replace(/\s+/g, '_').slice(0, 64);
}

function isFiniteNumber(x) {
  return Number.isFinite(x);
}

function toUInt8(n) {
  if (!Number.isFinite(n)) return null;
  const i = Math.trunc(n);
  if (i < 0 || i > 255) return null;
  return i;
}

function resolveId(raw, aliases) {
  // Accept number, numeric string, or alias lookup (string).
  if (raw === null || raw === undefined) return null;
  if (typeof raw === 'number') return toUInt8(raw);
  const s = String(raw).trim();
  if (!s) return null;
  const asNum = Number(s);
  if (Number.isFinite(asNum) && String(asNum) === s) return toUInt8(asNum);
  if (aliases && typeof aliases === 'object' && Object.prototype.hasOwnProperty.call(aliases, s)) {
    return toUInt8(Number(aliases[s]));
  }
  return null;
}

function compileK1MappingV1(mapping) {
  if (!mapping || mapping.schema !== 'k1_mapping_v1') return null;

  const macros = mapping.macros && typeof mapping.macros === 'object' ? mapping.macros : {};
  const segFadeMs = Number(macros.segment_fade_ms);
  const transitionDuration = Number.isFinite(segFadeMs) && segFadeMs >= 0 ? Math.trunc(segFadeMs) : 900;

  const lightwave = mapping.lightwave && typeof mapping.lightwave === 'object' ? mapping.lightwave : {};
  const paletteAliases = lightwave.paletteAliases && typeof lightwave.paletteAliases === 'object' ? lightwave.paletteAliases : null;
  const effectAliases = lightwave.effectAliases && typeof lightwave.effectAliases === 'object' ? lightwave.effectAliases : null;

  const segments = Array.isArray(mapping.segments) ? mapping.segments : [];
  const byIndex = new Map();
  const byId = new Map();

  for (let i = 0; i < segments.length; i++) {
    const s = segments[i] || {};
    const idx = toUInt8(i);
    const id = typeof s.id === 'string' ? s.id : null;
    const label = typeof s.label === 'string' ? s.label : '';
    const labelCanon = canonicalSegmentLabel(label);

    const paletteId = resolveId(s.palette_id, paletteAliases);
    const effectId = resolveId(s.motion_id, effectAliases);

    const actions = [];
    if (effectId !== null) {
      actions.push({
        type: 'effects.setCurrent',
        effectId,
        transition: { type: 1, duration: transitionDuration },
      });
    }
    if (paletteId !== null) {
      actions.push({
        type: 'palettes.set',
        paletteId,
      });
    }

    const entry = {
      index: idx,
      id,
      label,
      labelCanon,
      actions,
    };
    byIndex.set(idx, entry);
    if (id) byId.set(id, entry);
  }

  return { transitionDuration, byIndex, byId };
}

async function main() {
  const args = parseArgs(process.argv);
  const deviceWsUrl = normaliseDeviceWsUrl(args.device);

  let mapping = null;
  let segmentRules = [];
  let k1 = null;
  if (args.mapping) {
    const raw = fs.readFileSync(args.mapping, 'utf8');
    mapping = JSON.parse(raw);
    k1 = compileK1MappingV1(mapping);
    segmentRules = k1 ? [] : compileSegmentRules(mapping);
  }

  const ws = new WebSocket(deviceWsUrl);

  await new Promise((resolve, reject) => {
    ws.onopen = resolve;
    ws.onerror = reject;
  });

  console.error(`[trinity-bridge] connected: ${deviceWsUrl}`);

  ws.onmessage = (evt) => {
    if (!args.verbose) return;
    const s = typeof evt.data === 'string' ? evt.data : '';
    if (s) console.error(`[trinity-bridge] ← ${s}`);
  };

  let sendSeq = 0;
  const sendJson = (obj) => {
    if (args.dryRun) {
      console.log(JSON.stringify(obj));
      return;
    }
    ws.send(JSON.stringify(obj));
  };

  let lastSegmentKey = '';

  const rl = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
  for await (const line of rl) {
    const trimmed = String(line).trim();
    if (!trimmed) continue;

    let msg;
    try {
      msg = JSON.parse(trimmed);
    } catch (e) {
      console.error(`[trinity-bridge] invalid JSON: ${e?.message ?? e}`);
      continue;
    }

    const type = typeof msg?.type === 'string' ? msg.type : '';
    if (!type) continue;

    // 1) Pass through Trinity messages to firmware (beat/macro/sync/segment, etc.)
    if (args.forward && type.startsWith('trinity.')) {
      sendJson(msg);
    }

    // 2) Segment-driven semantic mapping (host-side)
    if (type === 'trinity.segment' && (segmentRules.length > 0 || k1)) {
      const index = isFiniteNumber(msg.index) ? Number(msg.index) : -1;
      const labelRaw = typeof msg.label === 'string' ? msg.label : '';
      const label = canonicalSegmentLabel(labelRaw);
      const id = typeof msg.id === 'string' ? msg.id : '';
      const segKey = `${index}:${id}:${label}`;
      if (segKey !== lastSegmentKey) {
        lastSegmentKey = segKey;

        if (k1) {
          const idx8 = toUInt8(index);
          const seg = (id && k1.byId.get(id)) || (idx8 !== null ? k1.byIndex.get(idx8) : null);
          const actions = seg?.actions || [];
          for (const action of actions) {
            const out = { ...action, requestId: action.requestId ?? `bridge-${++sendSeq}` };
            sendJson(out);
          }
        } else {
          for (const rule of segmentRules) {
            if (!rule.matcher(label)) continue;
            for (const action of rule.actions) {
              if (!action || typeof action.type !== 'string') continue;
              const out = { ...action, requestId: action.requestId ?? `bridge-${++sendSeq}` };
              sendJson(out);
            }
          }
        }
      }
    }
  }

  ws.close();
}

main().catch((err) => {
  console.error(`[trinity-bridge] fatal: ${err?.stack ?? err}`);
  process.exit(1);
});
