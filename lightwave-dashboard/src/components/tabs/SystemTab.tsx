import React, { useMemo, useState } from 'react';
import { PlugZap, RefreshCw, Shield, Activity, Wifi } from 'lucide-react';
import { useV2 } from '../../state/v2';
import type { V2AuthMode } from '../../services/v2/config';
import { InfoRow } from '../ui/InfoRow';
import { NarrativeStatus } from '../NarrativeStatus';

const formatAge = (ts?: number) => {
  if (!ts) return 'Never';
  const secs = Math.floor((Date.now() - ts) / 1000);
  if (secs < 60) return `${secs}s ago`;
  const mins = Math.floor(secs / 60);
  return `${mins}m ago`;
};

export const SystemTab: React.FC = () => {
  const { state, actions } = useV2();
  const [deviceOrigin, setDeviceOrigin] = useState(state.settings.deviceOrigin);
  const [authMode, setAuthMode] = useState<V2AuthMode>(state.settings.authMode);
  const [authToken, setAuthToken] = useState(state.settings.authToken);

  const isConnected = state.connection.wsStatus === 'connected' || state.connection.httpOk;
  const fwVersion = state.deviceInfo?.firmwareVersion ?? 'unknown';
  const wsLabel = state.connection.wsStatus.toUpperCase();

  const metricsTop = useMemo(() => state.metrics.getSnapshot().slice(0, 8), [state.metrics, state.connection.lastOkAt, state.connection.lastError]);

  const handleApply = () => {
    actions.updateSettings({ deviceOrigin, authMode, authToken });
    actions.bootstrap();
    actions.reconnectWs();
  };

  return (
    <div className="p-4 sm:p-5 lg:p-6 xl:p-8 animate-slide-down">
      <div className="flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between mb-5">
        <div>
          <h1 className="font-display text-2xl uppercase tracking-tight leading-none">System</h1>
          <p className="text-xs text-text-secondary mt-1">API v2 connectivity, health, and diagnostics</p>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-[10px] text-text-secondary font-mono">FW: {fwVersion}</span>
          <div className={`w-1.5 h-1.5 rounded-full ${isConnected ? 'bg-accent-green' : 'bg-accent-red'}`}></div>
        </div>
      </div>

      {state.errors.bootstrap && (
        <div className="mb-4 rounded-xl border border-accent-red/30 bg-accent-red/10 px-4 py-3 text-xs text-text-primary">
          {state.errors.bootstrap}
        </div>
      )}

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
        {/* Narrative Status */}
        <NarrativeStatus />

        <div
          className="rounded-xl p-4 sm:p-5 card-hover lg:col-span-1"
          style={{
            background: 'rgba(37,45,63,0.95)',
            border: '1px solid rgba(255,255,255,0.10)',
            boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
          }}
        >
          <div className="flex items-center justify-between mb-4">
            <span className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>Connection</span>
            <PlugZap className="w-4 h-4 text-primary" />
          </div>

          <div className="space-y-3">
            <div className="space-y-1">
              <label htmlFor="device-origin-input" className="text-[10px] uppercase tracking-wide text-text-secondary font-medium">Device Origin</label>
              <input
                id="device-origin-input"
                type="text"
                value={deviceOrigin}
                onChange={(e) => setDeviceOrigin(e.target.value)}
                placeholder="http://lightwaveos.local"
                className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-3 py-2 text-xs text-text-primary focus:outline-none focus:border-primary/50 transition-colors"
              />
              <div className="text-[10px] text-text-secondary/80">
                Leave empty to use Vite proxy at <span className="font-mono">/api/v1</span>.
              </div>
            </div>

            <div className="space-y-1">
              <label className="text-[10px] uppercase tracking-wide text-text-secondary font-medium">Auth</label>
              <div className="grid grid-cols-2 gap-2">
                <select
                  value={authMode}
                  onChange={(e) => setAuthMode(e.target.value as V2AuthMode)}
                  className="h-9 rounded-lg bg-[#2f3849] border border-white/10 px-2 text-[11px] text-text-primary focus:outline-none focus:border-primary/50"
                >
                  <option value="none">None</option>
                  <option value="bearer">Bearer</option>
                  <option value="apiKey">API Key</option>
                </select>
                <div className="relative">
                  <input
                    type="password"
                    value={authToken}
                    onChange={(e) => setAuthToken(e.target.value)}
                    placeholder="token"
                    className="w-full h-9 rounded-lg bg-[#2f3849] border border-white/10 pl-3 pr-8 text-[11px] text-text-primary focus:outline-none focus:border-primary/50"
                  />
                  <Shield className="absolute right-2.5 top-1/2 -translate-y-1/2 w-3.5 h-3.5 text-text-secondary" />
                </div>
              </div>
            </div>

            <div className="grid grid-cols-2 gap-2 pt-1">
              <button
                onClick={handleApply}
                className="h-9 rounded-lg bg-primary/10 border border-primary/20 text-primary text-xs font-medium hover:bg-primary/20 transition-all touch-target"
              >
                Apply
              </button>
              <button
                onClick={() => actions.bootstrap()}
                className="h-9 rounded-lg border border-white/10 bg-white/5 text-text-secondary hover:bg-white/10 text-xs font-medium transition-all touch-target flex items-center justify-center gap-2"
              >
                <RefreshCw className={`w-3 h-3 ${state.loading.bootstrap ? 'animate-spin' : ''}`} />
                Refresh
              </button>
            </div>

            <div className="space-y-1 pt-2">
              <InfoRow label="HTTP" value={state.connection.httpOk ? 'OK' : 'ERROR'} />
              <InfoRow label="WebSocket" value={wsLabel} />
              <InfoRow label="Last OK" value={formatAge(state.connection.lastOkAt)} />
            </div>
          </div>
        </div>

        <div
          className="rounded-xl p-4 sm:p-5 card-hover lg:col-span-2"
          style={{
            background: 'rgba(37,45,63,0.95)',
            border: '1px solid rgba(255,255,255,0.10)',
            boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
          }}
        >
          <div className="flex items-center justify-between mb-4">
            <span className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>Device</span>
            <Wifi className="w-4 h-4 text-accent-cyan" />
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div className="space-y-1">
              <InfoRow label="Chip" value={state.deviceInfo?.chipModel ?? 'unknown'} />
              <InfoRow label="SDK" value={state.deviceInfo?.sdkVersion ?? 'unknown'} />
              <InfoRow label="Flash" value={state.deviceInfo?.flashSize != null ? `${Math.round(state.deviceInfo.flashSize / (1024 * 1024))} MB` : 'unknown'} />
              <InfoRow label="CPU" value={state.deviceStatus?.cpuFreqMHz != null ? `${state.deviceStatus.cpuFreqMHz} MHz` : 'unknown'} />
            </div>
            <div className="space-y-1">
              <InfoRow label="Uptime" value={state.deviceStatus?.uptime != null ? `${state.deviceStatus.uptime}s` : 'unknown'} />
              <InfoRow label="Free Heap" value={state.deviceStatus?.freeHeap != null ? `${Math.round(state.deviceStatus.freeHeap / 1024)} KB` : 'unknown'} />
              <InfoRow label="Min Heap" value={state.deviceStatus?.minFreeHeap != null ? `${Math.round(state.deviceStatus.minFreeHeap / 1024)} KB` : 'unknown'} />
              <InfoRow label="WS Clients" value={state.deviceStatus?.wsClients != null ? `${state.deviceStatus.wsClients}` : 'unknown'} />
            </div>
          </div>

          <div className="mt-6">
            <div className="flex items-center justify-between mb-3">
              <span className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>API Metrics</span>
              <Activity className="w-4 h-4 text-accent-green" />
            </div>

            {metricsTop.length === 0 ? (
              <div className="text-xs text-text-secondary">No API calls recorded yet.</div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-2">
                {metricsTop.map(m => (
                  <div key={m.key} className="rounded-lg border border-white/10 bg-surface/50 px-3 py-2">
                    <div className="flex items-center justify-between">
                      <span className="text-[10px] text-text-secondary font-mono">{m.key}</span>
                      <span className="text-[10px] text-text-secondary">
                        {m.lastLatencyMs != null ? `${Math.round(m.lastLatencyMs)}ms` : '—'}
                      </span>
                    </div>
                    <div className="flex items-center gap-3 mt-1 text-[10px] text-text-secondary">
                      <span>ok {m.okCount}/{m.count}</span>
                      <span>err {m.errorCount}</span>
                      <span>retry {m.retryCount}</span>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
