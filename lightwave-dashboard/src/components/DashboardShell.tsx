import React, { useState, useEffect, useRef } from 'react';
import {
  Settings,
  Sliders,
  Wand2,
  Cpu,
  Zap,
  Play
} from 'lucide-react';
import LGPVisualizer from './LGPVisualizer';
import { SliderControl } from './ui/SliderControl';
import { PresetButton } from './ui/PresetButton';
import { Listbox } from './ui/Listbox';
import { ShowsTab } from './tabs/ShowsTab';
import { EffectsTab } from './tabs/EffectsTab';
import { SystemTab } from './tabs/SystemTab';
import { useV2 } from '../state/v2';
import { useLedStream } from '../hooks/useLedStream';
import { SkipLinks } from './SkipLinks';

const TABS = [
  { id: 'control', label: 'Control', icon: Sliders },
  { id: 'shows', label: 'Shows', icon: Play },
  { id: 'effects', label: 'Effects', icon: Wand2 },
  { id: 'system', label: 'System', icon: Cpu },
];

const DashboardShell: React.FC = () => {
  const [activeTab, setActiveTab] = useState('control');
  const { state, actions } = useV2();
  const isWsConnected = state.connection.wsStatus === 'connected';
  const isHttpOk = state.connection.httpOk;
  const isConnected = isWsConnected || isHttpOk;
  // Controls require HTTP (for PATCH requests), not just WS (which is for streaming)
  const canControl = isHttpOk && !!state.parameters;
  const brightness = state.parameters?.brightness ?? 192;
  const speed = state.parameters?.speed ?? 18;
  const [blur, setBlur] = useState(0.45);
  const [gamma, setGamma] = useState(2.20);

  // Subscribe to real-time LED frame streaming from firmware
  const { ledData, isSubscribed: isStreaming } = useLedStream(isConnected);

  const tabs = TABS;

  // Tab Indicator Logic
  const [indicatorStyle, setIndicatorStyle] = useState({ left: 0, width: 0 });
  const tabsRef = useRef<(HTMLButtonElement | null)[]>([]);

  useEffect(() => {
    const activeIndex = tabs.findIndex(t => t.id === activeTab);
    const el = tabsRef.current[activeIndex];
    if (el) {
      setIndicatorStyle({
        left: el.offsetLeft,
        width: el.offsetWidth
      });
    }
  }, [activeTab, tabs]);

  // Tab keyboard navigation handler
  const handleTabKeyDown = (e: React.KeyboardEvent, tabId: string, index: number) => {
    let newIndex = index;
    switch (e.key) {
      case 'ArrowLeft':
        e.preventDefault();
        newIndex = index > 0 ? index - 1 : tabs.length - 1;
        break;
      case 'ArrowRight':
        e.preventDefault();
        newIndex = index < tabs.length - 1 ? index + 1 : 0;
        break;
      case 'Home':
        e.preventDefault();
        newIndex = 0;
        break;
      case 'End':
        e.preventDefault();
        newIndex = tabs.length - 1;
        break;
      case 'Enter':
      case ' ':
        e.preventDefault();
        setActiveTab(tabId);
        return;
      default:
        return; // Allow other keys to propagate
    }
    // Focus the new tab
    tabsRef.current[newIndex]?.focus();
  };

  const handleConnectionClick = () => {
    actions.bootstrap();
    actions.reconnectWs();
  };

  const handleAllOff = () => {
    if (!canControl) return;
    actions.setParameters({ brightness: 0 }, { debounceMs: 0 });
  };

  const handleSave = () => {
    if (!isConnected) return;
    actions.refreshParameters();
  };

  // Preset configurations: effect name mapping + parameter sets
  const PRESET_CONFIGS: Record<string, { effectName?: string; params: Partial<typeof state.parameters> }> = {
    'Movie Mode': {
      effectName: 'Movie Mode', // Will search for this in effectsList
      params: { brightness: 100, speed: 10 },
    },
    'Relax': {
      effectName: 'Relax',
      params: { brightness: 80, speed: 5 },
    },
    'Party': {
      effectName: 'Party',
      params: { brightness: 255, speed: 30 },
    },
    'Focus': {
      effectName: 'Focus',
      params: { brightness: 150, speed: 15 },
    },
  };

  const handlePresetClick = async (presetName: string) => {
    if (!canControl) return;

    const config = PRESET_CONFIGS[presetName];
    if (!config) return;

    try {
      // Find effect by name in effectsList
      if (config.effectName && state.effectsList?.effects) {
        const effect = state.effectsList.effects.find(
          (e) => e.name.toLowerCase() === config.effectName!.toLowerCase()
        );
        if (effect) {
          await actions.setCurrentEffect(effect.id);
        } else {
          console.warn(`Preset effect "${config.effectName}" not found in firmware effects list`);
        }
      }

      // Apply parameters
      if (config.params && Object.keys(config.params).length > 0) {
        actions.setParameters(config.params, { debounceMs: 0 });
      }
    } catch (err) {
      console.error(`Failed to apply preset "${presetName}":`, err);
    }
  };

  return (
    <div className="min-h-screen bg-background text-text-primary font-sans selection:bg-primary/20 selection:text-primary overflow-hidden flex flex-col p-3 sm:p-4 md:p-6 lg:p-8">
      <SkipLinks />
      
      {/* Main Container with V4 Glass Effect */}
      <section 
        className="mx-auto max-w-screen-2xl w-full rounded-2xl overflow-hidden flex flex-col relative"
        style={{
          background: 'rgba(37,45,63,0.55)',
          border: '1px solid rgba(255,255,255,0.10)',
          boxShadow: '0 0 0 1px rgba(255,255,255,0.04) inset, 0 24px 48px rgba(0,0,0,0.4)',
          backdropFilter: 'blur(24px)'
        }}
      >

        {/* Header */}
        <header 
          className="relative z-50 flex items-center justify-between px-4 sm:px-6 lg:px-8"
          style={{
            height: '4.5rem',
            background: 'rgba(37,45,63,0.95)',
            borderBottom: '1px solid rgba(255,255,255,0.10)',
            boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset',
            backgroundImage: 'radial-gradient(circle at top left, rgba(255,255,255,0.06), transparent 50%)'
          }}
        >
          
          {/* Logo */}
          <div className="flex items-center gap-3">
            <div 
              className="grid place-items-center rounded-xl"
              style={{
                width: '2.75rem',
                height: '2.75rem',
                background: 'linear-gradient(135deg, #ffb84d 0%, #f59e0b 100%)',
                boxShadow: '0 4px 12px rgba(255,184,77,0.3)'
              }}
            >
              <Zap className="w-[22px] h-[22px] text-[#1c2130]" strokeWidth={1.5} />
            </div>
            <div className="flex flex-col">
              <span className="uppercase tracking-tight font-display text-[1.375rem] leading-none" style={{ letterSpacing: '0.01em' }}>LightwaveOS</span>
              <span className="hidden md:block text-[0.6875rem] text-text-secondary" style={{ letterSpacing: '-0.01em' }}>Dual-strip • 320 LEDs • Center-radiating</span>
            </div>
          </div>
          
          <div className="flex items-center gap-2 sm:gap-3">
            <button 
              onClick={handleConnectionClick}
              className="flex items-center gap-2 rounded-xl px-2 sm:px-3 py-2 transition-colors hover:bg-white/5 cursor-pointer"
              style={{ background: 'rgba(47,56,73,0.65)', border: '1px solid rgba(255,255,255,0.08)' }}
            >
              <span className={`rounded-full w-2 h-2 ${isConnected ? 'bg-[#22dd88] status-pulse-connected' : 'bg-[#ef4444] status-pulse-disconnected'}`}></span>
              <span className={`hidden sm:inline text-[0.6875rem] font-medium ${isConnected ? 'text-[#22dd88]' : 'text-[#ef4444]'}`}>
                {isConnected ? 'CONNECTED' : 'DISCONNECTED'}
              </span>
            </button>
            <button 
              className="touch-target grid place-items-center rounded-xl btn-secondary"
              style={{ width: '2.75rem', height: '2.75rem', border: '1px solid rgba(255,255,255,0.10)' }}
              aria-label="Settings"
            >
              <Settings className="w-[18px] h-[18px] text-text-secondary" strokeWidth={1.5} />
            </button>
          </div>
        </header>

        {/* Tab Navigation */}
        <nav 
          className="relative flex items-center gap-1 px-4 sm:px-6 lg:px-8 overflow-x-auto tab-scroll flex-shrink-0"
          style={{
            height: '3.25rem',
            background: 'rgba(37,45,63,0.75)',
            borderBottom: '1px solid rgba(255,255,255,0.08)'
          }}
          role="tablist"
          aria-label="Main navigation"
        >
          {tabs.map((tab, index) => {
            const isActive = activeTab === tab.id;
            const tabPanelId = `tabpanel-${tab.id}`;
            return (
              <button
                key={tab.id}
                ref={el => { tabsRef.current[index] = el; }}
                onClick={() => setActiveTab(tab.id)}
                onKeyDown={(e) => handleTabKeyDown(e, tab.id, index)}
                className={`tab-btn touch-target relative h-full px-3 sm:px-4 whitespace-nowrap transition-colors focus:outline-none focus:ring-2 focus:ring-primary focus:ring-offset-2 focus:ring-offset-background ${
                  isActive ? 'text-primary' : 'text-text-secondary'
                }`}
                role="tab"
                aria-selected={isActive}
                aria-controls={tabPanelId}
                id={`tab-${tab.id}`}
                tabIndex={isActive ? 0 : -1}
              >
                <span className="uppercase tracking-wide font-display text-sm" style={{ letterSpacing: '0.04em' }}>
                  {tab.label}
                </span>
              </button>
            );
          })}
          
          {/* Sliding Underline */}
          <div 
            className="tab-underline absolute bottom-0 bg-primary h-[2px] rounded-[1px]"
            style={{
              left: `${indicatorStyle.left}px`,
              width: `${indicatorStyle.width}px`
            }}
          />
        </nav>

        {/* Content Area */}
        <div id="main-content" className="flex-1 overflow-y-auto min-h-[calc(100vh-12rem)]">
          {activeTab === 'control' && (
            <div 
              id="tabpanel-control"
              role="tabpanel"
              aria-labelledby="tab-control"
              className="p-4 sm:p-5 lg:p-6 xl:p-8 animate-slide-down"
            >
              {!isConnected && (
                <div className="mb-4 rounded-xl border border-accent-red/30 bg-accent-red/10 px-4 py-3 text-xs text-text-primary">
                  <div className="font-semibold mb-1">Not connected</div>
                  <div className="text-text-secondary">
                    Device controls are disabled. Go to <span className="text-text-primary">System</span> to set Device Origin and connect.
                  </div>
                </div>
              )}

              {isWsConnected && !isHttpOk && (
                <div className="mb-4 rounded-xl border border-accent-red/30 bg-accent-red/10 px-4 py-3 text-xs text-text-primary">
                  <div className="font-semibold mb-1">Streaming connected, but control unavailable</div>
                  <div className="text-text-secondary">
                    HTTP control plane is down. Sliders and presets are disabled. {state.connection.lastError && `Error: ${state.connection.lastError}`}
                  </div>
                </div>
              )}

              {state.errors.parameters && (
                <div className="mb-4 rounded-xl border border-accent-red/30 bg-accent-red/10 px-4 py-3 text-xs text-text-primary">
                  <div className="font-semibold mb-1">Parameter update failed</div>
                  <div className="text-text-secondary">{state.errors.parameters}</div>
                </div>
              )}
              
              {/* Page Header */}
              <div className="flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between mb-5">
                <div>
                  <h1 className="uppercase tracking-tight font-display text-2xl" style={{ letterSpacing: '0.01em' }}>Control + Zones</h1>
                  <p className="flex flex-wrap items-center gap-2 mt-1 text-xs text-text-secondary">
                    <span>Center-radiating effects</span>
                    <span className="w-[3px] h-[3px] rounded-full bg-text-secondary"></span>
                    <span>Stream: <span className={isStreaming ? 'text-[#22dd88]' : 'text-text-secondary'}>{isStreaming ? 'ON' : 'OFF'}</span></span>
                  </p>
                </div>
              </div>

              {/* LGP Visualizer */}
              <div 
                className="card-hover rounded-xl p-3 sm:p-4 mb-5"
                style={{ background: '#141820', border: '1px solid rgba(255,255,255,0.08)' }}
              >
                <div className="flex items-center justify-between mb-3">
                  <span className="uppercase tracking-wide font-display text-xs text-text-secondary" style={{ letterSpacing: '0.06em' }}>LGP Strip Visualizer</span>
                  <span className="hidden sm:inline text-[0.625rem] text-accent-cyan">160 LEDs × 2 • Dual edge-lit</span>
                </div>
                <LGPVisualizer
                  isConnected={isConnected}
                  ledData={ledData}
                  isStreaming={isStreaming}
                />
              </div>

              {/* Main Grid */}
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                
                {/* Global Controls */}
                <div 
                  className="card-hover rounded-xl p-4 sm:p-5"
                  style={{
                    background: 'rgba(37,45,63,0.95)',
                    border: '1px solid rgba(255,255,255,0.10)',
                    boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
                  }}
                >
                  <div className="flex items-center justify-between mb-4">
                    <span className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>Global Controls</span>
                    <span className="rounded-lg px-2 py-1 text-[0.5625rem] text-text-secondary" style={{ background: 'rgba(47,56,73,0.65)', border: '1px solid rgba(255,255,255,0.08)' }}>Strip A/B</span>
                  </div>

                  <SliderControl 
                    label="Brightness" 
                    value={brightness} 
                    min={0} 
                    max={255} 
                    onChange={(v) => actions.setParameters({ brightness: v })}
                    fillColor="from-primary to-primary-dark"
                    disabled={!canControl}
                  />
                  
                  <SliderControl 
                    label="Speed" 
                    value={speed} 
                    min={1} 
                    max={50} 
                    onChange={(v) => actions.setParameters({ speed: v })}
                    fillColor="from-primary to-primary-dark"
                    disabled={!canControl}
                  />

                  <SliderControl 
                    label="Blur" 
                    value={blur} 
                    min={0} 
                    max={1} 
                    step={0.01}
                    onChange={setBlur} 
                    format={(v) => v.toFixed(2)}
                    fillColor="from-accent-purple/80 to-accent-purple"
                    disabled={!isConnected}
                  />

                  <SliderControl 
                    label="Gamma" 
                    value={gamma} 
                    min={1} 
                    max={3} 
                    step={0.01}
                    onChange={setGamma} 
                    format={(v) => v.toFixed(2)}
                    fillColor="from-accent-cyan/80 to-accent-cyan"
                    disabled={!isConnected}
                  />

                  {/* Palette Dropdown */}
                  <div className="mb-4">
                    <Listbox
                      label="Palette"
                      options={[
                        { value: 'aurora', label: 'Aurora Glass' },
                        { value: 'ocean', label: 'Ocean Wave' },
                        { value: 'fire', label: 'Fire Glow' },
                        { value: 'ice', label: 'Ice Crystal' },
                        { value: 'sunset', label: 'Sunset Gradient' },
                      ]}
                      value={null}
                      onChange={(value) => {
                        // TODO: Integrate with palette selection API
                        console.log('Palette selected:', value);
                      }}
                      placeholder="Select palette"
                      disabled={!isConnected}
                    />
                  </div>

                  <div className="grid grid-cols-2 gap-2">
                    <button
                      onClick={handleAllOff}
                      disabled={!canControl}
                      className={`touch-target rounded-lg font-medium btn-secondary h-10 bg-transparent border border-primary text-primary text-xs transition-colors ${
                        canControl ? 'hover:bg-primary/10' : 'opacity-50 cursor-not-allowed'
                      }`}
                    >
                      All Off
                    </button>
                    <button
                      onClick={handleSave}
                      disabled={!isConnected}
                      className={`touch-target rounded-lg font-medium btn-primary h-10 bg-primary text-[#1c2130] text-xs transition-all ${
                        isConnected ? 'hover:brightness-110' : 'opacity-50 cursor-not-allowed'
                      }`}
                    >
                      Sync
                    </button>
                  </div>
                </div>

                {/* Presets - Keeping simplified for now but matching style */}
                <div 
                  className="card-hover rounded-xl p-4 sm:p-5 md:col-span-2 lg:col-span-1"
                  style={{
                    background: 'rgba(37,45,63,0.95)',
                    border: '1px solid rgba(255,255,255,0.10)',
                    boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
                  }}
                >
                  <div className="flex items-center justify-between mb-4">
                    <span className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>Quick Presets</span>
                  </div>
                  <div className="grid grid-cols-2 gap-3">
                    <PresetButton 
                      label="Movie Mode" 
                      color="from-purple-500 to-blue-600" 
                      disabled={!canControl}
                      onClick={() => handlePresetClick('Movie Mode')}
                    />
                    <PresetButton 
                      label="Relax" 
                      color="from-teal-400 to-emerald-500" 
                      disabled={!canControl}
                      onClick={() => handlePresetClick('Relax')}
                    />
                    <PresetButton 
                      label="Party" 
                      color="from-pink-500 to-rose-500" 
                      disabled={!canControl}
                      onClick={() => handlePresetClick('Party')}
                    />
                    <PresetButton 
                      label="Focus" 
                      color="from-amber-200 to-orange-400" 
                      disabled={!canControl}
                      onClick={() => handlePresetClick('Focus')}
                    />
                  </div>
                </div>

              </div>
            </div>
          )}
          
          {activeTab === 'shows' && (
            <div 
              id="tabpanel-shows"
              role="tabpanel"
              aria-labelledby="tab-shows"
            >
              <ShowsTab />
            </div>
          )}
          {activeTab === 'effects' && (
            <div 
              id="tabpanel-effects"
              role="tabpanel"
              aria-labelledby="tab-effects"
            >
              <EffectsTab />
            </div>
          )}
          {activeTab === 'system' && (
            <div 
              id="tabpanel-system"
              role="tabpanel"
              aria-labelledby="tab-system"
            >
              <SystemTab />
            </div>
          )}
        </div>
      </section>
    </div>
  );
};

export default DashboardShell;
