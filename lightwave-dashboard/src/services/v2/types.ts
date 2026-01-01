export type V2Envelope<T> =
  | { success: true; data: T; timestamp: number; version: string }
  | { success: false; error: V2ErrorBody; timestamp: number; version: string };

export type V2ErrorCode =
  | 'INVALID_JSON'
  | 'MISSING_FIELD'
  | 'OUT_OF_RANGE'
  | 'NOT_FOUND'
  | 'RATE_LIMITED'
  | 'METHOD_NOT_ALLOWED'
  | 'INTERNAL_ERROR'
  | string;

export interface V2ErrorBody {
  code: V2ErrorCode;
  message: string;
  field?: string;
}

export interface V2ApiDiscovery {
  name: string;
  apiVersion: string;
  description: string;
  hardware: {
    ledsTotal: number;
    strips: number;
    ledsPerStrip: number;
    centerPoint: number;
    maxZones: number;
    chipModel?: string;
    cpuFreqMHz?: number;
  };
  capabilities?: {
    centerOrigin?: boolean;
    zones?: boolean;
    transitions?: boolean;
    websocket?: boolean;
    effectCount?: number;
  };
  _links?: Record<string, string>;
}

export interface V2DeviceStatus {
  uptime: number;
  freeHeap: number;
  minFreeHeap?: number;
  cpuFreqMHz: number;
  wsClients: number;
}

export interface V2DeviceInfo {
  firmware: string;
  firmwareVersion: string;
  sdkVersion: string;
  chipModel: string;
  chipRevision: number;
  flashSize: number;
  flashSpeed?: number;
  sketchSize?: number;
  freeSketchSpace?: number;
  hardware?: {
    ledsTotal: number;
    strips: number;
    ledsPerStrip: number;
    maxZones: number;
  };
}

export interface V2DeviceOverview {
  status: V2DeviceStatus;
  info: V2DeviceInfo;
}

export interface V2Parameters {
  brightness: number;
  speed: number;
  mood: number;
  paletteId: number;
  intensity: number;
  saturation: number;
  complexity: number;
  variation: number;
  _meta?: Record<string, string>;
}

export interface V2EffectListItem {
  id: number;
  name: string;
  category?: string;
  categoryId?: number;
  description?: string;
  centerOrigin?: boolean;
}

export interface V2EffectsList {
  total: number;
  offset: number;
  limit: number;
  count: number;
  effects: V2EffectListItem[];
  _links?: Record<string, string>;
}

export interface V2EffectCurrent {
  effectId: number;
  name: string;
  category?: string;
  categoryId?: number;
  description?: string;
  parameters: V2Parameters;
}

export interface V2EffectCategory {
  id: number;
  name: string;
  count: number;
}

export interface V2EffectCategories {
  total: number;
  categories: V2EffectCategory[];
}

export interface V2EffectFamily {
  id: number;
  name: string;
  count: number;
}

export interface V2EffectFamilies {
  total: number;
  families: V2EffectFamily[];
}

export interface V2EffectMetadata {
  id: number;
  name: string;
  family?: string;
  familyId?: number;
  story?: string;
  opticalIntent?: string;
  tags?: string[];
  properties?: {
    centerOrigin?: boolean;
    symmetricStrips?: boolean;
    paletteAware?: boolean;
    speedResponsive?: boolean;
  };
  recommended?: {
    brightness?: number;
    speed?: number;
  };
}

export interface V2NarrativeStatus {
  enabled: boolean;
  tension: number;  // 0-100
  phase: string;    // BUILD, HOLD, RELEASE, REST
  phaseId: number;
  phaseT: number;  // 0-1
  cycleT: number;   // 0-1
  durations: {
    build: number;
    hold: number;
    release: number;
    rest: number;
    total: number;
  };
  tempoMultiplier: number;
  complexityScaling: number;
}

export interface V2NarrativeConfig {
  enabled: boolean;
  durations: {
    build: number;
    hold: number;
    release: number;
    rest: number;
    total: number;
  };
  curves: {
    build: number;
    release: number;
  };
  holdBreathe: number;
  snapAmount: number;
  durationVariance: number;
}

export interface V2Zone {
  id: number;
  enabled: boolean;
  effectId: number;
  effectName?: string;
  brightness: number;
  speed: number;
  paletteId: number;
  blendMode?: number;
  blendModeName?: string;
}

export interface V2ZoneSegment {
  zoneId: number;
  s1LeftStart: number;
  s1LeftEnd: number;
  s1RightStart: number;
  s1RightEnd: number;
  totalLeds: number;
}

export interface V2ZonesState {
  enabled: boolean;
  zoneCount: number;
  segments?: V2ZoneSegment[];
  zones: V2Zone[];
  presets?: Array<{ id: number; name: string }>;
}

export interface V2PaletteFlags {
  warm?: boolean;
  cool?: boolean;
  calm?: boolean;
  vivid?: boolean;
  cvdFriendly?: boolean;
  whiteHeavy?: boolean;
}

export interface V2PaletteListItem {
  id: number;
  name: string;
  category: string;
  flags?: V2PaletteFlags;
  avgBrightness?: number;
  maxBrightness?: number;
}

export interface V2PalettesList {
  total: number;
  offset: number;
  limit: number;
  count: number;
  palettes: V2PaletteListItem[];
  categories?: {
    artistic?: number;
    scientific?: number;
    lgpOptimized?: number;
  };
  pagination?: {
    page: number;
    limit: number;
    total: number;
    pages: number;
  };
  _links?: Record<string, string>;
}

export interface V2PaletteCurrent {
  paletteId: number;
  name: string;
  category: string;
  flags?: V2PaletteFlags;
  avgBrightness?: number;
  maxBrightness?: number;
}

export interface V2PaletteSetResponse {
  paletteId: number;
  name: string;
  category: string;
}

export type V2WsRequest = {
  requestId?: string;
} & (
  | { type: 'device.getStatus' }
  | { type: 'device.getInfo' }
  | { type: 'effects.list'; offset?: number; limit?: number; details?: boolean }
  | { type: 'effects.getCurrent' }
  | { type: 'effects.getMetadata'; effectId: number }
  | { type: 'effects.setCurrent'; effectId: number; transition?: { type: number; duration: number } }
  | { type: 'parameters.get' }
  | ({ type: 'parameters.set' } & Partial<V2Parameters>)
  | { type: 'narrative.getStatus' }
  | {
      type: 'narrative.config';
      enabled?: boolean;
      durations?: Partial<V2NarrativeConfig['durations']>;
      curves?: Partial<V2NarrativeConfig['curves']>;
      holdBreathe?: number;
      snapAmount?: number;
      durationVariance?: number;
    }
  | { type: 'zones.list' }
  | { type: 'zones.update'; zoneId: number; effectId?: number; brightness?: number; speed?: number; paletteId?: number }
  | { type: 'zones.setLayout'; zones: V2ZoneSegment[] }
  | { type: 'ledStream.subscribe' }
  | { type: 'ledStream.unsubscribe' }
);

export type V2WsEvent =
  | { type: 'effects.changed'; effectId: number; name: string; timestamp: number }
  | { type: 'parameters.changed'; updated: string[]; current: V2Parameters; timestamp: number }
  | { type: 'transition.started'; fromEffect: number; toEffect: number; transitionType: number; duration: number; timestamp: number }
  | { type: 'zones.list'; enabled: boolean; zoneCount: number; segments?: V2ZoneSegment[]; zones: V2Zone[]; timestamp: number }
  | { type: 'zones.changed'; zoneId: number; updated: string[]; current: Partial<V2Zone>; timestamp: number }
  | { type: 'zones.layoutChanged'; success: boolean; zoneCount: number; timestamp: number }
  | ({ type: string } & Record<string, unknown>);

export class V2ApiError extends Error {
  public readonly httpStatus?: number;
  public readonly body?: V2ErrorBody;
  public readonly url?: string;

  constructor(message: string, opts?: { httpStatus?: number; body?: V2ErrorBody; url?: string }) {
    super(message);
    this.name = 'V2ApiError';
    this.httpStatus = opts?.httpStatus;
    this.body = opts?.body;
    this.url = opts?.url;
  }
}

