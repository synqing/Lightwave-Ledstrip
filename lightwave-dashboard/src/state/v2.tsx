import React, { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from 'react';
import { ApiMetrics } from '../services/v2/metrics';
import { V2Client } from '../services/v2/client';
import { v2Api } from '../services/v2/api';
import type { V2AuthMode } from '../services/v2/config';
import { buildV2BaseUrl, buildV2WsUrl, loadV2Settings, saveV2Settings } from '../services/v2/config';
import type { V2DeviceInfo, V2DeviceStatus, V2EffectCategories, V2EffectCurrent, V2EffectsList, V2Parameters } from '../services/v2/types';
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
  loading: { bootstrap: boolean; parameters: boolean; effects: boolean; system: boolean };
  errors: { bootstrap?: string; parameters?: string; effects?: string; system?: string };
  metrics: ApiMetrics;
}

export interface V2Actions {
  updateSettings: (partial: { deviceOrigin?: string; authMode?: V2AuthMode; authToken?: string }) => void;
  bootstrap: () => Promise<void>;
  refreshStatus: () => Promise<void>;
  refreshParameters: () => Promise<void>;
  refreshEffects: () => Promise<void>;
  setParameters: (partial: Partial<V2Parameters>, opts?: { debounceMs?: number }) => void;
  setCurrentEffect: (effectId: number) => Promise<void>;
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
  const [loading, setLoading] = useState({ bootstrap: false, parameters: false, effects: false, system: false });
  const [errors, setErrors] = useState<{ bootstrap?: string; parameters?: string; effects?: string; system?: string }>({});

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
          setParametersState(paramEv.current);
          setConnection(prev => ({ ...prev, lastOkAt: Date.now() }));
        }
        if (ev.type === 'effects.changed') {
          const effectEv = ev as { type: 'effects.changed'; effectId: number; name: string };
          setCurrentEffectState(prev => prev ? { ...prev, effectId: effectEv.effectId, name: effectEv.name } : prev);
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
      const [status, info, params, current, categories, list] = await Promise.all([
        api.deviceStatus(),
        api.deviceInfo(),
        api.parametersGet(),
        api.effectsCurrent(),
        api.effectsCategories(),
        api.effectsList({ offset: 0, limit: 50 }),
      ]);
      setDeviceStatus(status);
      setDeviceInfo(info);
      setParametersState(params);
      setCurrentEffectState(current);
      setEffectCategories(categories);
      setEffectsList(list);
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
        api.effectsList({ offset: 0, limit: 50 }),
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
      loading,
      errors,
      metrics,
    }),
    [connection, currentEffect, deviceInfo, deviceStatus, effectCategories, effectsList, errors, loading, metrics, parameters, settings]
  );

  const actions: V2Actions = useMemo(
    () => ({
      updateSettings,
      bootstrap,
      refreshStatus,
      refreshParameters,
      refreshEffects,
      setParameters,
      setCurrentEffect,
      reconnectWs,
      getWsClient,
    }),
    [bootstrap, getWsClient, reconnectWs, refreshEffects, refreshParameters, refreshStatus, setCurrentEffect, setParameters, updateSettings]
  );

  return <V2Context.Provider value={{ state, actions }}>{children}</V2Context.Provider>;
};

export const useV2 = () => {
  const ctx = useContext(V2Context);
  if (!ctx) throw new Error('useV2 must be used within V2Provider');
  return ctx;
};

