import type {
  V2ApiDiscovery,
  V2DeviceInfo,
  V2DeviceOverview,
  V2DeviceStatus,
  V2EffectCategories,
  V2EffectCurrent,
  V2EffectFamilies,
  V2EffectMetadata,
  V2EffectsList,
  V2NarrativeConfig,
  V2NarrativeStatus,
  V2PaletteCurrent,
  V2PalettesList,
  V2PaletteSetResponse,
  V2Parameters,
  V2ZoneSegment,
  V2ZonesState,
} from './types';
import { V2Client } from './client';

export const v2Api = (client: V2Client) => ({
  discovery: () => client.get<V2ApiDiscovery>('/'),
  openApi: () => client.get<Record<string, unknown>>('/openapi.json'),

  device: () => client.get<V2DeviceOverview>('/device'),
  deviceStatus: () => client.get<V2DeviceStatus>('/device/status'),
  deviceInfo: () => client.get<V2DeviceInfo>('/device/info'),

  parametersGet: () => client.get<V2Parameters>('/parameters'),
  parametersPatch: (partial: Partial<V2Parameters>) => client.patch<V2Parameters>('/parameters', partial),

  effectsList: (opts?: { offset?: number; limit?: number }) =>
    client.get<V2EffectsList>('/effects', { query: { offset: opts?.offset ?? 0, limit: opts?.limit ?? 50 } }),
  effectsCurrent: () => client.get<V2EffectCurrent>('/effects/current'),
  effectsSetCurrent: (effectId: number) => client.put<V2EffectCurrent>('/effects/current', { effectId }),
  effectsCategories: () => client.get<V2EffectCategories>('/effects/categories'),
  effectsFamilies: () => client.get<V2EffectFamilies>('/effects/families'),
  effectsMetadata: (effectId: number) => client.get<V2EffectMetadata>('/effects/metadata', { query: { id: effectId } }),
  narrativeStatus: () => client.get<V2NarrativeStatus>('/narrative/status'),
  narrativeConfigGet: () => client.get<V2NarrativeConfig>('/narrative/config'),
  narrativeConfigSet: (config: Partial<V2NarrativeConfig>) => client.post<V2NarrativeConfig>('/narrative/config', config),
  zonesList: () => client.get<V2ZonesState>('/zones'),
  zonesSetLayout: (zones: V2ZoneSegment[]) => client.post<{ zoneCount: number }>('/zones/layout', { zones }),
  zoneSetSpeed: (zoneId: number, speed: number) => client.post<{ zoneId: number; speed: number }>(`/zones/${zoneId}/speed`, { speed }),
  palettesList: (opts?: { offset?: number; limit?: number; category?: string; warm?: boolean; cool?: boolean; calm?: boolean; vivid?: boolean; cvd?: boolean }) =>
    client.get<V2PalettesList>('/palettes', {
      query: {
        offset: opts?.offset ?? 0,
        limit: opts?.limit ?? 100,
        category: opts?.category,
        warm: opts?.warm ? 'true' : undefined,
        cool: opts?.cool ? 'true' : undefined,
        calm: opts?.calm ? 'true' : undefined,
        vivid: opts?.vivid ? 'true' : undefined,
        cvd: opts?.cvd ? 'true' : undefined,
      },
    }),
  palettesCurrent: () => client.get<V2PaletteCurrent>('/palettes/current'),
  palettesSet: (paletteId: number) => client.post<V2PaletteSetResponse>('/palettes/set', { paletteId }),
});

