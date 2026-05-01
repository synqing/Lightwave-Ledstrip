import type { CatalogueEntry } from '../types';

export const transportCatalogue: CatalogueEntry[] = [
  {
    key: 'audio.parameters.get',
    label: 'Audio parameters query',
    status: 'yamlDocumented',
    source: 'docs/protocol/k1-ws-contract.yaml:700-728',
    rationale: 'Transport command, not an effect-facing audio signal.',
  },
  {
    key: 'stimulus.patch',
    label: 'Synthetic ControlBus patch',
    status: 'yamlDocumented',
    source: 'docs/protocol/k1-ws-contract.yaml:1454-1469',
    rationale: 'Useful for future capture/probe workflows, not an AP choice.',
  },
  {
    key: 'beat.subscribe',
    label: 'Beat stream subscription',
    status: 'yamlDocumented',
    source: 'docs/protocol/k1-ws-contract.yaml:1598-1616',
    rationale: 'Client stream command; runtime authority must be reconciled against firmware handlers.',
  },
  {
    key: 'vrms.subscribe',
    label: 'VRMS stream subscription',
    status: 'firmwareOnly',
    source: 'firmware-v3/src/network/webserver/ws/WsStreamCommands.cpp:566-572',
    rationale: 'Active firmware command missing from YAML; label as docs drift before exposing.',
  },
  {
    key: 'wifi.sta.enable',
    label: 'Station mode enable',
    status: 'invariantBlocked',
    source: 'firmware-v3/src/network/webserver/V1ApiRoutes.cpp:1616-1629',
    rationale: 'K1 is AP-only; do not expose STA enable even if firmware route exists.',
  },
];

export const transportAuthorityNote =
  'BACKLOG.md:43-49 makes firmware runtime behaviour the source of truth; YAML enriches commands but cannot be the only catalogue source.';
