import React, { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from 'react';
import { ApiMetrics } from '../services/v2/metrics';
import { V2Client } from '../services/v2/client';
import { v2Api } from '../services/v2/api';
import type { V2AuthMode } from '../services/v2/config';
import { buildV2BaseUrl, buildV2WsUrl, loadV2Settings, saveV2Settings } from '../services/v2/config';
import type { V2DeviceInfo, V2DeviceStatus, V2EffectCategories, V2EffectCurrent, V2EffectsList, V2PaletteCurrent, V2PalettesList, V2Parameters, V2ZonesState } from '../services/v2/types';
import { V2ApiError } from '../services/v2/types';
import { V2WsClient, type WsStatus } from '../services/v2/ws';

export interface V2ConnectionState {
  httpOk: boolean;
  wsStatus: WsStatus;
  lastOkAt?: number;
  lastError?: string;
}

export interface V2State {
  settings: { deviceOrigin: string; authMode: V2AuthMode; authToken: string };
  connection: V2ConnectionState;
  deviceStatus?: V2DeviceStatus;
  deviceInfo?: V2DeviceInfo;
  parameters?: V2Parameters;
  currentEffect?: V2EffectCurrent;
  effectsList?: V2EffectsList;
  effectCategories?: V2EffectCategories;
  palettesList?: V2PalettesList;
  currentPalette?: V2PaletteCurrent;
  zones?: V2ZonesState;
  loading: { bootstrap: boolean; parameters: boolean; effects: boolean; system: boolean; zones: boolean; palettes: boolean };
  errors: { bootstrap?: string; parameters?: string; effects?: string; system?: string; zones?: string; palettes?: string };
  metrics: ApiMetrics;
}

export interface V2Actions {
  updateSettings: (partial: { deviceOrigin?: string; authMode?: V2AuthMode; authToken?: string }) => void;
  bootstrap: () => Promise<void>;
  refreshStatus: () => Promise<void>;
  refreshParameters: () => Promise<void>;
  refreshEffects: () => Promise<void>;
  refreshPalettes: () => Promise<void>;
  refreshZones: () => Promise<void>;
  setParameters: (partial: Partial<V2Parameters>, opts?: { debounceMs?: number }) => void;
  setCurrentEffect: (effectId: number) => Promise<void>;
  setPalette: (paletteId: number) => Promise<void>;
  setZoneSpeed: (zoneId: number, speed: number) => Promise<void>;
  setZoneLayout: (zones: import('../services/v2/types').V2ZoneSegment[]) => Promise<void>;
  reconnectWs: () => void;
  getWsClient: () => V2WsClient | null;
}

const V2Context = createContext<{ state: V2State; actions: V2Actions } | null>(null);

const formatErr = (err: unknown) => {
  if (err instanceof V2ApiError) return err.message;
  if (err instanceof Error) return err.message;
  return 'Unknown error';
};

export const V2Provider: React.FC<React.PropsWithChildren<{ autoConnect?: boolean }>> = ({ children, autoConnect = true }) => {
  const [settings, setSettings] = useState(() => loadV2Settings());
  const metrics = useMemo(() => new ApiMetrics(), []);
  const clientRef = useRef<V2Client | null>(null);
  const wsRef = useRef<V2WsClient | null>(null);

  const [connection, setConnection] = useState<V2ConnectionState>({ httpOk: false, wsStatus: 'disconnected' });
  const [deviceStatus, setDeviceStatus] = useState<V2DeviceStatus | undefined>(undefined);
  const [deviceInfo, setDeviceInfo] = useState<V2DeviceInfo | undefined>(undefined);
  const [parameters, setParametersState] = useState<V2Parameters | undefined>(undefined);
  const [currentEffect, setCurrentEffectState] = useState<V2EffectCurrent | undefined>(undefined);
  const [effectsList, setEffectsList] = useState<V2EffectsList | undefined>(undefined);
  const [effectCategories, setEffectCategories] = useState<V2EffectCategories | undefined>(undefined);
  const [palettesList, setPalettesList] = useState<V2PalettesList | undefined>(undefined);
  const [currentPalette, setCurrentPalette] = useState<V2PaletteCurrent | undefined>(undefined);
  const [zones, setZones] = useState<V2ZonesState | undefined>(undefined);
  const [loading, setLoading] = useState({ bootstrap: false, parameters: false, effects: false, system: false, zones: false, palettes: false });
  const [errors, setErrors] = useState<{ bootstrap?: string; parameters?: string; effects?: string; system?: string; zones?: string; palettes?: string }>({});

  const patchTimer = useRef<number | null>(null);
  const pendingPatch = useRef<Partial<V2Parameters>>({});

  const getClient = useCallback(() => {
    if (!clientRef.current) {
      clientRef.current = new V2Client({
        baseUrl: buildV2BaseUrl(settings.deviceOrigin),
        auth: { mode: settings.authMode, token: settings.authToken || undefined },
        metrics,
        requestTimeoutMs: 5000,
      });
    }
    return clientRef.current;
  }, [metrics, settings.authMode, settings.authToken, settings.deviceOrigin]);

  const getWs = useCallback(() => {
    if (!wsRef.current) {
      wsRef.current = new V2WsClient(buildV2WsUrl(settings.deviceOrigin));
      wsRef.current.onStatus((wsStatus) => {
        setConnection(prev => ({ ...prev, wsStatus }));
      });
      wsRef.current.onEvent((ev) => {
        if (ev.type === 'parameters.changed') {
          const paramEv = ev as { type: 'parameters.changed'; current: V2Parameters };
          setParametersState(prev => {
            // If paletteId changed, trigger a refresh to get full palette details
            if (prev && paramEv.current.paletteId !== undefined && paramEv.current.paletteId !== prev.paletteId) {
              // Use setTimeout to avoid calling refreshPalettes during render
              setTimeout(() => {
                const api = v2Api(getClient());
                api.palettesCurrent().then(pal => setCurrentPalette(pal)).catch(() => {});
              }, 0);
            }
            return paramEv.current;
          });
          setConnection(prev => ({ ...prev, lastOkAt: Date.now() }));
        }
        if (ev.type === 'effects.changed') {
          const effectEv = ev as { type: 'effects.changed'; effectId: number; name: string };
          setCurrentEffectState(prev => prev ? { ...prev, effectId: effectEv.effectId, name: effectEv.name } : prev);
          setConnection(prev => ({ ...prev, lastOkAt: Date.now() }));
        }
        if (ev.type === 'zones.changed') {
          const zoneEv = ev as { type: 'zones.changed'; zoneId: number; current: Partial<import('../services/v2/types').V2Zone> };
          setZones(prev => {
            if (!prev) return prev;
            const updatedZones = [...prev.zones];
            if (updatedZones[zoneEv.zoneId]) {
              updatedZones[zoneEv.zoneId] = { ...updatedZones[zoneEv.zoneId], ...zoneEv.current };
            }
            return { ...prev, zones: updatedZones };
          });
          setConnection(prev => ({ ...prev, lastOkAt: Date.now() }));
        }
        if (ev.type === 'zones.list') {
          const zoneListEv = ev as { type: 'zones.list'; enabled: boolean; zoneCount: number; segments?: import('../services/v2/types').V2ZoneSegment[]; zones: import('../services/v2/types').V2Zone[] };
          setZones({
            enabled: zoneListEv.enabled,
            zoneCount: zoneListEv.zoneCount,
            segments: zoneListEv.segments,
            zones: zoneListEv.zones,
          });
          setConnection(prev => ({ ...prev, lastOkAt: Date.now() }));
        }
      });
    }
    return wsRef.current;
  }, [settings.deviceOrigin]);

  const updateSettings = useCallback((partial: { deviceOrigin?: string; authMode?: V2AuthMode; authToken?: string }) => {
    setSettings(prev => {
      const next = { ...prev, ...partial };
      saveV2Settings(next);
      clientRef.current?.setBaseUrl(buildV2BaseUrl(next.deviceOrigin));
      clientRef.current?.setAuth({ mode: next.authMode, token: next.authToken || undefined });
      wsRef.current?.setUrl(buildV2WsUrl(next.deviceOrigin));
      return next;
    });
  }, []);

  const bootstrap = useCallback(async () => {
    setLoading(prev => ({ ...prev, bootstrap: true }));
    setErrors(prev => ({ ...prev, bootstrap: undefined }));

    const api = v2Api(getClient());
    try {
      const [status, info, params, current, categories, list, palettes, currentPal, zonesData] = await Promise.all([
        api.deviceStatus(),
        api.deviceInfo(),
        api.parametersGet(),
        api.effectsCurrent(),
        api.effectsCategories(),
        api.effectsList({ offset: 0, limit: 100 }),
        api.palettesList({ offset: 0, limit: 100 }).catch(() => null), // Palettes may not be available, don't fail bootstrap
        api.palettesCurrent().catch(() => null), // Current palette may not be available, don't fail bootstrap
        api.zonesList().catch(() => null), // Zones may not be available, don't fail bootstrap
      ]);
      setDeviceStatus(status);
      setDeviceInfo(info);
      setParametersState(params);
      setCurrentEffectState(current);
      setEffectCategories(categories);
      setEffectsList(list);
      if (palettes) setPalettesList(palettes);
      if (currentPal) setCurrentPalette(currentPal);
      if (zonesData) {
        setZones(zonesData);
      }
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, bootstrap: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    } finally {
      setLoading(prev => ({ ...prev, bootstrap: false }));
    }
  }, [getClient]);

  const refreshStatus = useCallback(async () => {
    setLoading(prev => ({ ...prev, system: true }));
    setErrors(prev => ({ ...prev, system: undefined }));
    const api = v2Api(getClient());
    try {
      const status = await api.deviceStatus();
      setDeviceStatus(status);
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, system: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    } finally {
      setLoading(prev => ({ ...prev, system: false }));
    }
  }, [getClient]);

  const refreshParameters = useCallback(async () => {
    setLoading(prev => ({ ...prev, parameters: true }));
    setErrors(prev => ({ ...prev, parameters: undefined }));
    const api = v2Api(getClient());
    try {
      const params = await api.parametersGet();
      setParametersState(params);
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, parameters: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    } finally {
      setLoading(prev => ({ ...prev, parameters: false }));
    }
  }, [getClient]);

  const refreshEffects = useCallback(async () => {
    setLoading(prev => ({ ...prev, effects: true }));
    setErrors(prev => ({ ...prev, effects: undefined }));
    const api = v2Api(getClient());
    try {
      const [list, categories, current] = await Promise.all([
        api.effectsList({ offset: 0, limit: 100 }),
        api.effectsCategories(),
        api.effectsCurrent(),
      ]);
      setEffectsList(list);
      setEffectCategories(categories);
      setCurrentEffectState(current);
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, effects: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    } finally {
      setLoading(prev => ({ ...prev, effects: false }));
    }
  }, [getClient]);

  const flushPatch = useCallback(async () => {
    const toSend = { ...pendingPatch.current };
    pendingPatch.current = {};
    patchTimer.current = null;
    if (Object.keys(toSend).length === 0) return;

    const api = v2Api(getClient());
    try {
      const updated = await api.parametersPatch(toSend);
      setParametersState(updated);
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
      getWs().send({ type: 'parameters.get' });
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, parameters: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    }
  }, [getClient, getWs]);

  const setParameters = useCallback((partial: Partial<V2Parameters>, opts?: { debounceMs?: number }) => {
    setParametersState(prev => prev ? ({ ...prev, ...partial }) : (prev));
    pendingPatch.current = { ...pendingPatch.current, ...partial };

    const debounceMs = opts?.debounceMs ?? 80;
    if (patchTimer.current) window.clearTimeout(patchTimer.current);
    patchTimer.current = window.setTimeout(() => {
      flushPatch();
    }, debounceMs);
  }, [flushPatch]);

  const setCurrentEffect = useCallback(async (effectId: number) => {
    setErrors(prev => ({ ...prev, effects: undefined }));
    const api = v2Api(getClient());
    try {
      const current = await api.effectsSetCurrent(effectId);
      setCurrentEffectState(current);
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
      getWs().send({ type: 'effects.getCurrent' });
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, effects: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    }
  }, [getClient, getWs]);

  const setPalette = useCallback(async (paletteId: number) => {
    setErrors(prev => ({ ...prev, palettes: undefined }));
    const api = v2Api(getClient());
    try {
      const response = await api.palettesSet(paletteId);
      // Update current palette from the set response
      setCurrentPalette({
        paletteId: response.paletteId,
        name: response.name,
        category: response.category,
      });
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, palettes: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    }
  }, [getClient]);

  const refreshPalettes = useCallback(async () => {
    setLoading(prev => ({ ...prev, palettes: true }));
    setErrors(prev => ({ ...prev, palettes: undefined }));
    const api = v2Api(getClient());
    try {
      const [palettes, currentPal] = await Promise.all([
        api.palettesList({ offset: 0, limit: 100 }),
        api.palettesCurrent(),
      ]);
      setPalettesList(palettes);
      setCurrentPalette(currentPal);
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, palettes: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    } finally {
      setLoading(prev => ({ ...prev, palettes: false }));
    }
  }, [getClient]);

  const refreshZones = useCallback(async () => {
    setLoading(prev => ({ ...prev, zones: true }));
    setErrors(prev => ({ ...prev, zones: undefined }));
    const api = v2Api(getClient());
    try {
      const zonesData = await api.zonesList();
      setZones(zonesData);
      setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
    } catch (err) {
      const msg = formatErr(err);
      setErrors(prev => ({ ...prev, zones: msg }));
      setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
    } finally {
      setLoading(prev => ({ ...prev, zones: false }));
    }
  }, [getClient]);

  const setZoneSpeed = useCallback(async (zoneId: number, speed: number) => {
    setErrors(prev => ({ ...prev, zones: undefined }));
    const ws = getWs();
    // Prefer WebSocket for low latency, fallback to REST
    if (ws.getStatus() === 'connected') {
      ws.send({ type: 'zones.update', zoneId, speed });
    } else {
      const api = v2Api(getClient());
      try {
        await api.zoneSetSpeed(zoneId, speed);
        // Refresh zones to get updated state
        await refreshZones();
        setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
      } catch (err) {
        const msg = formatErr(err);
        setErrors(prev => ({ ...prev, zones: msg }));
        setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
      }
    }
  }, [getClient, getWs, refreshZones]);

  const setZoneLayout = useCallback(async (zones: import('../services/v2/types').V2ZoneSegment[]) => {
    setErrors(prev => ({ ...prev, zones: undefined }));
    const ws = getWs();
    // Prefer WebSocket for low latency, fallback to REST
    if (ws.getStatus() === 'connected') {
      ws.send({ type: 'zones.setLayout', zones });
    } else {
      const api = v2Api(getClient());
      try {
        await api.zonesSetLayout(zones);
        // Refresh zones to get updated state
        await refreshZones();
        setConnection(prev => ({ ...prev, httpOk: true, lastOkAt: Date.now(), lastError: undefined }));
      } catch (err) {
        const msg = formatErr(err);
        setErrors(prev => ({ ...prev, zones: msg }));
        setConnection(prev => ({ ...prev, httpOk: false, lastError: msg }));
      }
    }
  }, [getClient, getWs, refreshZones]);

  const reconnectWs = useCallback(() => {
    const ws = getWs();
    ws.disconnect();
    ws.connect();
  }, [getWs]);

  const getWsClient = useCallback(() => {
    return wsRef.current;
  }, []);

  useEffect(() => {
    if (!autoConnect) return;
    bootstrap();
  }, [autoConnect, bootstrap]);

  useEffect(() => {
    if (!autoConnect) return;
    const ws = getWs();
    ws.connect();
    const pollId = window.setInterval(() => {
      if (ws.getStatus() !== 'connected') refreshStatus();
    }, 5000);
    return () => {
      window.clearInterval(pollId);
      ws.disconnect();
    };
  }, [autoConnect, getWs, refreshStatus]);

  const state: V2State = useMemo(
    () => ({
      settings,
      connection,
      deviceStatus,
      deviceInfo,
      parameters,
      currentEffect,
      effectsList,
      effectCategories,
      palettesList,
      currentPalette,
      zones,
      loading,
      errors,
      metrics,
    }),
    [connection, currentEffect, currentPalette, deviceInfo, deviceStatus, effectCategories, effectsList, errors, loading, metrics, parameters, palettesList, settings, zones]
  );

  const actions: V2Actions = useMemo(
    () => ({
      updateSettings,
      bootstrap,
      refreshStatus,
      refreshParameters,
      refreshEffects,
      refreshPalettes,
      refreshZones,
      setParameters,
      setCurrentEffect,
      setPalette,
      setZoneSpeed,
      setZoneLayout,
      reconnectWs,
      getWsClient,
    }),
    [bootstrap, getWsClient, reconnectWs, refreshEffects, refreshPalettes, refreshParameters, refreshStatus, refreshZones, setCurrentEffect, setPalette, setParameters, setZoneSpeed, setZoneLayout, updateSettings]
  );

  return <V2Context.Provider value={{ state, actions }}>{children}</V2Context.Provider>;
};

export const useV2 = () => {
  const ctx = useContext(V2Context);
  if (!ctx) throw new Error('useV2 must be used within V2Provider');
  return ctx;
};

