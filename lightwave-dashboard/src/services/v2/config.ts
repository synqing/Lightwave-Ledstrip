export type V2AuthMode = 'none' | 'bearer' | 'apiKey';

export interface V2ClientConfig {
  baseUrl: string;
  wsUrl: string;
  authMode: V2AuthMode;
  authToken?: string;
}

export const DEFAULT_V2_SETTINGS = {
  deviceOrigin: '',
  authMode: 'none' as V2AuthMode,
  authToken: '',
};

export const buildV2BaseUrl = (deviceOrigin?: string) => {
  const origin = (deviceOrigin || '').trim();
  if (!origin) return '/api/v1';
  return `${origin.replace(/\/+$/, '')}/api/v1`;
};

export const buildV2WsUrl = (deviceOrigin?: string) => {
  const origin = (deviceOrigin || '').trim();
  if (!origin) {
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${proto}//${window.location.host}/ws`;
  }
  const url = new URL(origin);
  const wsProto = url.protocol === 'https:' ? 'wss:' : 'ws:';
  return `${wsProto}//${url.host}/ws`;
};

export const loadV2Settings = () => {
  const deviceOrigin = localStorage.getItem('lw.v2.deviceOrigin') ?? DEFAULT_V2_SETTINGS.deviceOrigin;
  const authMode = (localStorage.getItem('lw.v2.authMode') as V2AuthMode) ?? DEFAULT_V2_SETTINGS.authMode;
  const authToken = localStorage.getItem('lw.v2.authToken') ?? DEFAULT_V2_SETTINGS.authToken;

  const safeAuthMode: V2AuthMode = authMode === 'bearer' || authMode === 'apiKey' ? authMode : 'none';

  return { deviceOrigin, authMode: safeAuthMode, authToken };
};

export const saveV2Settings = (settings: { deviceOrigin: string; authMode: V2AuthMode; authToken: string }) => {
  localStorage.setItem('lw.v2.deviceOrigin', settings.deviceOrigin);
  localStorage.setItem('lw.v2.authMode', settings.authMode);
  localStorage.setItem('lw.v2.authToken', settings.authToken);
};

