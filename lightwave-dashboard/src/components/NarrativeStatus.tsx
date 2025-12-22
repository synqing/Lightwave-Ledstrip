import React, { useState, useEffect } from 'react';
import { Play, Pause, Settings, TrendingUp } from 'lucide-react';
import { useV2 } from '../state/v2';
import type { V2NarrativeStatus, V2NarrativeConfig } from '../services/v2/types';

const phaseColors: Record<string, string> = {
  BUILD: 'from-blue-500 to-cyan-500',
  HOLD: 'from-yellow-500 to-orange-500',
  RELEASE: 'from-orange-500 to-red-500',
  REST: 'from-purple-500 to-indigo-500',
  UNKNOWN: 'from-gray-500 to-gray-600',
};

const phaseLabels: Record<string, string> = {
  BUILD: 'Building Tension',
  HOLD: 'Peak Intensity',
  RELEASE: 'Releasing',
  REST: 'Resting',
  UNKNOWN: 'Unknown',
};

export const NarrativeStatus: React.FC = () => {
  const { state, actions } = useV2();
  const [status, setStatus] = useState<V2NarrativeStatus | null>(null);
  const [config, setConfig] = useState<V2NarrativeConfig | null>(null);
  const [loading, setLoading] = useState(false);
  const [showConfig, setShowConfig] = useState(false);
  const [localConfig, setLocalConfig] = useState<Partial<V2NarrativeConfig>>({});

  // Listen for WS responses
  useEffect(() => {
    const ws = actions.getWsClient();
    if (!ws) return;
    const cleanup = ws.onEvent((ev) => {
      // WS response format: { type, requestId?, success, data }
      const anyEv = ev as unknown as { type?: string; success?: boolean; data?: unknown };
      if (!anyEv?.type || anyEv.success !== true) return;

      if (anyEv.type === 'narrative.status') {
        setStatus(anyEv.data as V2NarrativeStatus);
      }
      if (anyEv.type === 'narrative.config') {
        // Only treat as config payload if it looks like it
        const d = anyEv.data as Partial<V2NarrativeConfig> | undefined;
        if (d && typeof d.enabled === 'boolean' && d.durations && d.curves) {
          setConfig(d as V2NarrativeConfig);
          setLocalConfig(d as V2NarrativeConfig);
        }
      }
    });
    return () => { cleanup(); };
  }, [actions, state.connection.wsStatus]);

  // Poll narrative status over WebSocket (real-time updates)
  useEffect(() => {
    if (state.connection.wsStatus !== 'connected') return;
    const ws = actions.getWsClient();
    if (!ws) return;

    // Kick immediately
    ws.send({ type: 'narrative.getStatus', requestId: 'narrative-status' });
    const interval = window.setInterval(() => {
      if (state.connection.wsStatus !== 'connected') return;
      ws.send({ type: 'narrative.getStatus', requestId: 'narrative-status' });
    }, 100);
    return () => window.clearInterval(interval);
  }, [actions, state.connection.wsStatus]);

  // Fetch config over WebSocket when panel opens
  useEffect(() => {
    if (!showConfig) return;
    if (state.connection.wsStatus !== 'connected') return;
    const ws = actions.getWsClient();
    if (!ws) return;
    ws.send({ type: 'narrative.config', requestId: 'narrative-config' });
  }, [actions, showConfig, state.connection.wsStatus]);

  const handleToggleEnabled = async () => {
    const ws = actions.getWsClient();
    if (!ws || !config) return;
    setLoading(true);
    try {
      ws.send({ type: 'narrative.config', requestId: 'narrative-config-set', enabled: !config.enabled });
      // Re-request config for canonical values
      ws.send({ type: 'narrative.config', requestId: 'narrative-config' });
    } catch (err) {
      console.error('Failed to toggle narrative:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleSaveConfig = async () => {
    const ws = actions.getWsClient();
    if (!ws) return;
    setLoading(true);
    try {
      ws.send({
        type: 'narrative.config',
        requestId: 'narrative-config-set',
        enabled: localConfig.enabled,
        durations: localConfig.durations,
        curves: localConfig.curves,
        holdBreathe: localConfig.holdBreathe,
        snapAmount: localConfig.snapAmount,
        durationVariance: localConfig.durationVariance,
      });
      // Refresh config and close
      ws.send({ type: 'narrative.config', requestId: 'narrative-config' });
      setShowConfig(false);
    } catch (err) {
      console.error('Failed to save narrative config:', err);
    } finally {
      setLoading(false);
    }
  };

  if (!status) {
    return (
      <div className="rounded-xl p-4 sm:p-5 card-hover" style={{
        background: 'rgba(37,45,63,0.95)',
        border: '1px solid rgba(255,255,255,0.10)',
        boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
      }}>
        <div className="text-xs text-text-secondary">Loading narrative status...</div>
      </div>
    );
  }

  const phaseColor = phaseColors[status.phase] || phaseColors.UNKNOWN;
  const phaseLabel = phaseLabels[status.phase] || phaseLabels.UNKNOWN;

  return (
    <div className="rounded-xl p-4 sm:p-5 card-hover" style={{
      background: 'rgba(37,45,63,0.95)',
      border: '1px solid rgba(255,255,255,0.10)',
      boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
    }}>
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-2">
          <TrendingUp className="w-4 h-4 text-primary" />
          <span className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>Narrative Engine</span>
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={() => setShowConfig(!showConfig)}
            className="p-1.5 rounded-lg hover:bg-white/5 transition-colors"
            title="Configure"
          >
            <Settings className="w-3.5 h-3.5 text-text-secondary" />
          </button>
          <button
            onClick={handleToggleEnabled}
            disabled={loading}
            className={`p-1.5 rounded-lg transition-colors ${status.enabled ? 'bg-primary/20 text-primary' : 'bg-white/5 text-text-secondary hover:bg-white/10'}`}
            title={status.enabled ? 'Disable' : 'Enable'}
          >
            {status.enabled ? (
              <Pause className="w-3.5 h-3.5" />
            ) : (
              <Play className="w-3.5 h-3.5" />
            )}
          </button>
        </div>
      </div>

      {!showConfig ? (
        <>
          {/* Phase Indicator */}
          <div className="mb-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] uppercase tracking-wide text-text-secondary font-medium">Current Phase</span>
              <span className={`text-xs font-medium px-2 py-0.5 rounded bg-gradient-to-r ${phaseColor} text-white`}>
                {status.phase}
              </span>
            </div>
            <div className="text-xs text-text-primary mb-1">{phaseLabel}</div>
            
            {/* Phase Progress Bar */}
            <div className="h-1.5 rounded-full bg-white/5 overflow-hidden">
              <div
                className={`h-full bg-gradient-to-r ${phaseColor} transition-all duration-100`}
                style={{ width: `${status.phaseT * 100}%` }}
              />
            </div>
            <div className="text-[9px] text-text-secondary mt-1">Phase: {Math.round(status.phaseT * 100)}%</div>
          </div>

          {/* Tension Progress Bar */}
          <div className="mb-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] uppercase tracking-wide text-text-secondary font-medium">Tension</span>
              <span className="text-xs font-medium text-primary">{Math.round(status.tension)}%</span>
            </div>
            <div className="h-2 rounded-full bg-white/5 overflow-hidden">
              <div
                className="h-full bg-gradient-to-r from-blue-500 via-cyan-500 to-green-500 transition-all duration-100"
                style={{ width: `${status.tension}%` }}
              />
            </div>
          </div>

          {/* Cycle Progress */}
          <div className="mb-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] uppercase tracking-wide text-text-secondary font-medium">Cycle Progress</span>
              <span className="text-xs text-text-secondary">{Math.round(status.cycleT * 100)}%</span>
            </div>
            <div className="h-1 rounded-full bg-white/5 overflow-hidden">
              <div
                className="h-full bg-primary transition-all duration-100"
                style={{ width: `${status.cycleT * 100}%` }}
              />
            </div>
          </div>

          {/* Derived Values */}
          <div className="grid grid-cols-2 gap-3 pt-2 border-t border-white/5">
            <div>
              <div className="text-[9px] text-text-secondary mb-0.5">Tempo Multiplier</div>
              <div className="text-sm font-medium text-text-primary">{status.tempoMultiplier.toFixed(2)}x</div>
            </div>
            <div>
              <div className="text-[9px] text-text-secondary mb-0.5">Complexity Scaling</div>
              <div className="text-sm font-medium text-text-primary">{status.complexityScaling.toFixed(2)}</div>
            </div>
          </div>

          {/* Phase Durations */}
          <div className="mt-4 pt-4 border-t border-white/5">
            <div className="text-[9px] text-text-secondary mb-2">Phase Durations</div>
            <div className="grid grid-cols-4 gap-2 text-[8px]">
              <div>
                <div className="text-text-secondary">Build</div>
                <div className="text-text-primary font-medium">{status.durations.build.toFixed(1)}s</div>
              </div>
              <div>
                <div className="text-text-secondary">Hold</div>
                <div className="text-text-primary font-medium">{status.durations.hold.toFixed(1)}s</div>
              </div>
              <div>
                <div className="text-text-secondary">Release</div>
                <div className="text-text-primary font-medium">{status.durations.release.toFixed(1)}s</div>
              </div>
              <div>
                <div className="text-text-secondary">Rest</div>
                <div className="text-text-primary font-medium">{status.durations.rest.toFixed(1)}s</div>
              </div>
            </div>
          </div>
        </>
      ) : (
        <div className="space-y-4">
          <div className="text-xs text-text-secondary mb-4">Narrative Configuration</div>
          
          {config && (
            <>
              {/* Phase Durations */}
              <div>
                <div className="text-[10px] uppercase tracking-wide text-text-secondary font-medium mb-2">Phase Durations (seconds)</div>
                <div className="grid grid-cols-2 gap-3">
                  <div>
                    <label className="text-[9px] text-text-secondary mb-1 block">Build</label>
                    <input
                      type="number"
                      step="0.1"
                      min="0.1"
                      value={localConfig.durations?.build ?? config.durations.build}
                      onChange={(e) => setLocalConfig(prev => ({
                        ...prev,
                        durations: { ...prev?.durations, ...config.durations, build: parseFloat(e.target.value) }
                      }))}
                      className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-2 py-1.5 text-xs text-text-primary focus:outline-none focus:border-primary/50"
                    />
                  </div>
                  <div>
                    <label className="text-[9px] text-text-secondary mb-1 block">Hold</label>
                    <input
                      type="number"
                      step="0.1"
                      min="0"
                      value={localConfig.durations?.hold ?? config.durations.hold}
                      onChange={(e) => setLocalConfig(prev => ({
                        ...prev,
                        durations: { ...prev?.durations, ...config.durations, hold: parseFloat(e.target.value) }
                      }))}
                      className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-2 py-1.5 text-xs text-text-primary focus:outline-none focus:border-primary/50"
                    />
                  </div>
                  <div>
                    <label className="text-[9px] text-text-secondary mb-1 block">Release</label>
                    <input
                      type="number"
                      step="0.1"
                      min="0.1"
                      value={localConfig.durations?.release ?? config.durations.release}
                      onChange={(e) => setLocalConfig(prev => ({
                        ...prev,
                        durations: { ...prev?.durations, ...config.durations, release: parseFloat(e.target.value) }
                      }))}
                      className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-2 py-1.5 text-xs text-text-primary focus:outline-none focus:border-primary/50"
                    />
                  </div>
                  <div>
                    <label className="text-[9px] text-text-secondary mb-1 block">Rest</label>
                    <input
                      type="number"
                      step="0.1"
                      min="0"
                      value={localConfig.durations?.rest ?? config.durations.rest}
                      onChange={(e) => setLocalConfig(prev => ({
                        ...prev,
                        durations: { ...prev?.durations, ...config.durations, rest: parseFloat(e.target.value) }
                      }))}
                      className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-2 py-1.5 text-xs text-text-primary focus:outline-none focus:border-primary/50"
                    />
                  </div>
                </div>
              </div>

              {/* Optional Behaviors */}
              <div>
                <div className="text-[10px] uppercase tracking-wide text-text-secondary font-medium mb-2">Optional Behaviors</div>
                <div className="space-y-2">
                  <div>
                    <label className="text-[9px] text-text-secondary mb-1 block">Hold Breathe (0-1)</label>
                    <input
                      type="number"
                      step="0.01"
                      min="0"
                      max="1"
                      value={localConfig.holdBreathe ?? config.holdBreathe}
                      onChange={(e) => setLocalConfig(prev => ({ ...prev, holdBreathe: parseFloat(e.target.value) }))}
                      className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-2 py-1.5 text-xs text-text-primary focus:outline-none focus:border-primary/50"
                    />
                  </div>
                  <div>
                    <label className="text-[9px] text-text-secondary mb-1 block">Snap Amount (0-1)</label>
                    <input
                      type="number"
                      step="0.01"
                      min="0"
                      max="1"
                      value={localConfig.snapAmount ?? config.snapAmount}
                      onChange={(e) => setLocalConfig(prev => ({ ...prev, snapAmount: parseFloat(e.target.value) }))}
                      className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-2 py-1.5 text-xs text-text-primary focus:outline-none focus:border-primary/50"
                    />
                  </div>
                  <div>
                    <label className="text-[9px] text-text-secondary mb-1 block">Duration Variance (0-1)</label>
                    <input
                      type="number"
                      step="0.01"
                      min="0"
                      max="1"
                      value={localConfig.durationVariance ?? config.durationVariance}
                      onChange={(e) => setLocalConfig(prev => ({ ...prev, durationVariance: parseFloat(e.target.value) }))}
                      className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-2 py-1.5 text-xs text-text-primary focus:outline-none focus:border-primary/50"
                    />
                  </div>
                </div>
              </div>

              <div className="flex gap-2 pt-2">
                <button
                  onClick={handleSaveConfig}
                  disabled={loading}
                  className="flex-1 h-9 rounded-lg bg-primary text-[#1c2130] text-xs font-medium hover:brightness-110 transition-all touch-target"
                >
                  {loading ? 'Saving...' : 'Save'}
                </button>
                <button
                  onClick={() => {
                    setShowConfig(false);
                    setLocalConfig(config);
                  }}
                  className="flex-1 h-9 rounded-lg border border-white/10 bg-white/5 text-text-secondary hover:bg-white/10 text-xs font-medium transition-all touch-target"
                >
                  Cancel
                </button>
              </div>
            </>
          )}
        </div>
      )}
    </div>
  );
};

