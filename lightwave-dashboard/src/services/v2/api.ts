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
  V2Parameters,
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
});

