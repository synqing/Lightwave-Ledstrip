import { describe, expect, it } from 'vitest';
import type { V2Parameters, V2WsEvent, V2ZonesState } from '../services/v2/types';
import { extractWsParameters, extractZonesList, mergeParameters, reduceZonesChanged } from './v2';

const fullParameters: V2Parameters = {
  brightness: 100,
  speed: 20,
  hue: 64,
  paletteId: 2,
  intensity: 80,
  saturation: 200,
  complexity: 50,
  variation: 10,
};

describe('v2 WebSocket reducers', () => {
  it('extracts parameters from passive status broadcasts', () => {
    const partial = extractWsParameters({
      type: 'status',
      brightness: 120,
      speed: 30,
      hue: 90,
      paletteId: 3,
      intensity: 70,
      saturation: 180,
      complexity: 45,
      variation: 12,
    });

    expect(mergeParameters(fullParameters, partial!)).toMatchObject({
      brightness: 120,
      paletteId: 3,
    });
  });

  it('extracts both parameters and parameters.changed response shapes', () => {
    const direct = extractWsParameters({
      type: 'parameters',
      ...fullParameters,
    } as V2WsEvent);
    const changed = extractWsParameters({
      type: 'parameters.changed',
      success: true,
      data: {
        updated: ['brightness'],
        current: { ...fullParameters, brightness: 150 },
      },
    } as V2WsEvent);

    expect(direct).toMatchObject({ hue: 64, paletteId: 2 });
    expect(changed).toMatchObject({ brightness: 150 });
  });

  it('treats zones.list as the passive zone refresh broadcast', () => {
    const zones = extractZonesList({
      type: 'zones.list',
      enabled: true,
      zoneCount: 1,
      zones: [{ id: 7, enabled: true, effectId: 1, brightness: 80, speed: 20, paletteId: 2 }],
    } as V2WsEvent);

    expect(zones?.zones[0].id).toBe(7);
    expect(zones?.zoneCount).toBe(1);
  });

  it('handles zones.changed command responses without relying on array index ids', () => {
    const initial: V2ZonesState = {
      enabled: true,
      zoneCount: 1,
      zones: [{ id: 7, enabled: true, effectId: 1, brightness: 80, speed: 20, paletteId: 2 }],
    };

    const updated = reduceZonesChanged(initial, {
      type: 'zones.changed',
      success: true,
      data: {
        zoneId: 7,
        updated: ['speed'],
        current: { speed: 44 },
      },
    } as V2WsEvent);

    expect(updated?.zones[0].speed).toBe(44);
  });
});
