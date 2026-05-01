import React, { useState, useMemo, useEffect } from 'react';
import { Search, Star, SearchX, X, Tag } from 'lucide-react';
import { useV2 } from '../../state/v2';
import { V2Client } from '../../services/v2/client';
import { v2Api } from '../../services/v2/api';
import type { V2EffectFamily, V2EffectMetadata } from '../../services/v2/types';
import { buildV2BaseUrl } from '../../services/v2/config';

// Effect type with firmware-aligned category
interface Effect {
  id: number;
  name: string;
  desc: string;
  category: string;
  color: string;
  starred: boolean;
  metadata?: V2EffectMetadata;
}

// Categories aligned with firmware EffectMetadata.h (fallback)
const EFFECT_CATEGORIES = ['All', 'Classic', 'Shockwave', 'Interference', 'Geometric', 'Organic', 'Quantum', 'Physics'] as const;

export const EffectsTab: React.FC = () => {
  const { state, actions } = useV2();
  const isConnected = state.connection.wsStatus === 'connected' || state.connection.httpOk;
  const [searchQuery, setSearchQuery] = useState('');
  const [activeFilter, setActiveFilter] = useState('All');
  const [selectedFamily, setSelectedFamily] = useState<number | null>(null);
  const [selectedTags, setSelectedTags] = useState<Set<string>>(new Set());
  const [families, setFamilies] = useState<V2EffectFamily[]>([]);
  const [loadingFamilies, setLoadingFamilies] = useState(false);
  const [selectedEffect, setSelectedEffect] = useState<Effect | null>(null);
  const [effectMetadata, setEffectMetadata] = useState<Map<number, V2EffectMetadata>>(new Map());

  const [effects, setEffects] = useState<Effect[]>([
    { id: 0, name: 'Fire', desc: 'Realistic fire simulation radiating from center', category: 'Classic', color: 'from-orange-500/20 to-red-500/10', starred: true },
    { id: 1, name: 'Ocean', desc: 'Deep ocean wave patterns from center point', category: 'Classic', color: 'from-blue-500/20 to-cyan-500/10', starred: false },
    { id: 2, name: 'Center Pulse', desc: 'Radiates from center outward', category: 'Classic', color: 'from-accent-cyan/20 to-accent-green/10', starred: true },
    { id: 5, name: 'Shockwave', desc: 'Energy pulse expanding from center', category: 'Shockwave', color: 'from-primary/20 to-accent-red/10', starred: false },
    { id: 6, name: 'Collision', desc: 'Dual shockwaves meeting at center', category: 'Shockwave', color: 'from-red-500/20 to-orange-500/10', starred: false },
    { id: 9, name: 'Holographic', desc: 'Holographic interference patterns', category: 'Interference', color: 'from-accent-cyan/20 to-accent-green/10', starred: false },
    { id: 10, name: 'Standing Wave', desc: 'Stable interference nodes at fixed positions', category: 'Interference', color: 'from-purple-500/20 to-blue-500/10', starred: false },
    { id: 13, name: 'Diamond Lattice', desc: 'Diamond lattice crystal pattern', category: 'Geometric', color: 'from-purple-500/20 to-pink-500/10', starred: false },
    { id: 14, name: 'Concentric Rings', desc: 'Expanding rings from center origin', category: 'Geometric', color: 'from-accent-green/20 to-accent-cyan/10', starred: false },
    { id: 22, name: 'Aurora', desc: 'Aurora borealis curtain effect', category: 'Organic', color: 'from-accent-green/20 to-accent-cyan/10', starred: false },
    { id: 23, name: 'Bioluminescent', desc: 'Glowing organic patterns', category: 'Organic', color: 'from-cyan-500/20 to-blue-500/10', starred: false },
    { id: 25, name: 'Quantum Tunneling', desc: 'Quantum tunneling probability waves', category: 'Quantum', color: 'from-indigo-500/20 to-teal-500/10', starred: false },
    { id: 26, name: 'Wave Collapse', desc: 'Quantum wave function collapse visualization', category: 'Quantum', color: 'from-violet-500/20 to-purple-500/10', starred: false },
    { id: 36, name: 'Liquid Crystal', desc: 'Liquid crystal birefringence patterns', category: 'Physics', color: 'from-pink-500/20 to-yellow-500/10', starred: false },
    { id: 37, name: 'Plasma Membrane', desc: 'Cellular membrane dynamics simulation', category: 'Physics', color: 'from-red-500/20 to-pink-500/10', starred: false },
  ]);

  // Pull effects list from device when connected
  useEffect(() => {
    if (!isConnected) return;
    actions.refreshEffects();
  }, [actions, isConnected]);

  // If the device provides an effects list, merge it into local list (keeps starring local for now)
  useEffect(() => {
    const fromDevice = state.effectsList?.effects;
    if (!fromDevice || fromDevice.length === 0) return;

    setEffects(prev => {
      const starred = new Map(prev.map(e => [e.id, e.starred]));
      return fromDevice.map(e => ({
        id: e.id,
        name: e.name,
        desc: e.description ?? '',
        category: e.category ?? 'Classic',
        color: 'from-primary/20 to-accent-cyan/10',
        starred: starred.get(e.id) ?? false,
      }));
    });
  }, [state.effectsList]);

  // Fetch families from API
  useEffect(() => {
    const fetchFamilies = async () => {
      if (!isConnected || !state.settings.deviceOrigin) return;
      setLoadingFamilies(true);
      try {
        const baseUrl = buildV2BaseUrl(state.settings.deviceOrigin);
        const client = new V2Client({ baseUrl, auth: { mode: state.settings.authMode, token: state.settings.authToken } });
        const api = v2Api(client);
        const result = await api.effectsFamilies();
        setFamilies(result.families);
      } catch (err) {
        console.error('Failed to fetch families:', err);
      } finally {
        setLoadingFamilies(false);
      }
    };
    fetchFamilies();
  }, [isConnected, state.settings]);

  // Fetch metadata for selected effect
  const fetchEffectMetadata = async (effectId: number) => {
    if (effectMetadata.has(effectId) || !state.settings.deviceOrigin) return;
    try {
      const baseUrl = buildV2BaseUrl(state.settings.deviceOrigin);
      const client = new V2Client({ baseUrl, auth: { mode: state.settings.authMode, token: state.settings.authToken } });
      const api = v2Api(client);
      const metadata = await api.effectsMetadata(effectId);
      setEffectMetadata(prev => new Map(prev).set(effectId, metadata));
      // Update effect with metadata
      setEffects(prev => prev.map(e => e.id === effectId ? { ...e, metadata } : e));
    } catch (err) {
      console.error(`Failed to fetch metadata for effect ${effectId}:`, err);
    }
  };

  const toggleStar = (id: number) => {
    setEffects(prev => prev.map(e => 
      e.id === id ? { ...e, starred: !e.starred } : e
    ));
  };

  const handleEffectClick = (effect: Effect) => {
    if (!isConnected) return;
    setSelectedEffect(effect);
    if (!effect.metadata) {
      fetchEffectMetadata(effect.id);
    }
  };

  const toggleTag = (tag: string) => {
    setSelectedTags(prev => {
      const next = new Set(prev);
      if (next.has(tag)) {
        next.delete(tag);
      } else {
        next.add(tag);
      }
      return next;
    });
  };

  // Get all unique tags from effects with metadata
  const availableTags = useMemo(() => {
    const tags = new Set<string>();
    effects.forEach(e => {
      e.metadata?.tags?.forEach(tag => tags.add(tag));
    });
    return Array.from(tags).sort();
  }, [effects]);

  // Filter effects by category, family, search query, and tags
  const filteredEffects = useMemo(() => {
    let result = effects;

    // Apply category filter (legacy)
    if (activeFilter !== 'All') {
      result = result.filter(e => e.category === activeFilter);
    }

    // Apply family filter
    if (selectedFamily !== null) {
      result = result.filter(e => {
        const meta = effectMetadata.get(e.id) || e.metadata;
        return meta?.familyId === selectedFamily;
      });
    }

    // Apply tag filter
    if (selectedTags.size > 0) {
      result = result.filter(e => {
        const meta = effectMetadata.get(e.id) || e.metadata;
        const effectTags = new Set(meta?.tags || []);
        return Array.from(selectedTags).some(tag => effectTags.has(tag));
      });
    }

    // Apply search filter (case-insensitive)
    if (searchQuery.trim()) {
      const q = searchQuery.toLowerCase().trim();
      result = result.filter(e => {
        const meta = effectMetadata.get(e.id) || e.metadata;
        return (
          e.name.toLowerCase().includes(q) ||
          e.desc.toLowerCase().includes(q) ||
          e.category.toLowerCase().includes(q) ||
          meta?.family?.toLowerCase().includes(q) ||
          meta?.story?.toLowerCase().includes(q) ||
          meta?.tags?.some(tag => tag.toLowerCase().includes(q))
        );
      });
    }

    return result;
  }, [effects, activeFilter, selectedFamily, selectedTags, searchQuery, effectMetadata]);

  const handleClearFilters = () => {
    setSearchQuery('');
    setActiveFilter('All');
    setSelectedFamily(null);
    setSelectedTags(new Set());
  };

  return (
    <div className="p-4 sm:p-5 lg:p-6 xl:p-8 animate-slide-down">
      {!isConnected && (
        <div className="mb-4 rounded-xl border border-accent-red/30 bg-accent-red/10 px-4 py-3 text-xs text-text-primary">
          <div className="font-semibold mb-1">Not connected</div>
          <div className="text-text-secondary">
            Effects are disabled. Connect in <span className="text-text-primary">System</span>.
          </div>
        </div>
      )}
      <div className="flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between mb-5">
        <div>
          <h1 className="font-display text-2xl uppercase tracking-tight leading-none">Effects Browser</h1>
          <p className="text-xs text-text-secondary mt-1">Browse, preview, and pin center-radiating effects</p>
        </div>
        <div className="flex items-center gap-2">
          <div className="relative">
            <label htmlFor="effects-search" className="sr-only">
              Search effects
            </label>
            <input 
              id="effects-search"
              type="text" 
              placeholder="Search effects..." 
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              aria-label="Search effects"
              disabled={!isConnected}
              className={`w-40 sm:w-48 h-9 rounded-lg bg-surface/50 border border-white/10 pl-8 pr-3 text-[11px] text-text-primary focus:outline-none focus:border-primary/50 transition-colors ${
                isConnected ? '' : 'opacity-50 cursor-not-allowed'
              }`}
            />
            <Search className="absolute left-2.5 top-1/2 -translate-y-1/2 w-3.5 h-3.5 text-text-secondary" aria-hidden="true" />
          </div>
        </div>
      </div>

      {/* Family Filter Dropdown */}
      {families.length > 0 && (
        <div className="mb-4">
          <label className="text-[10px] text-text-secondary mb-1 block">Filter by Family</label>
          <select
            value={selectedFamily ?? ''}
            onChange={(e) => setSelectedFamily(e.target.value ? Number(e.target.value) : null)}
            disabled={!isConnected}
            className={`w-full sm:w-64 h-9 rounded-lg bg-surface/50 border border-white/10 px-3 text-[11px] text-text-primary focus:outline-none focus:border-primary/50 transition-colors ${
              isConnected ? '' : 'opacity-50 cursor-not-allowed'
            }`}
          >
            <option value="">All Families</option>
            {families.map(family => (
              <option key={family.id} value={family.id}>
                {family.name} ({family.count})
              </option>
            ))}
          </select>
          {loadingFamilies && (
            <div className="mt-1 text-[10px] text-text-secondary">Loading families…</div>
          )}
        </div>
      )}

      {/* Tag Filters */}
      {availableTags.length > 0 && (
        <div className="mb-4">
          <label className="text-[10px] text-text-secondary mb-2 block">Filter by Tags</label>
          <div className="flex flex-wrap gap-2">
            {availableTags.map(tag => (
              <button
                key={tag}
                onClick={() => toggleTag(tag)}
                disabled={!isConnected}
                className={`rounded-full px-3 py-1 text-[10px] border transition-all flex items-center gap-1 ${
                  selectedTags.has(tag)
                    ? 'bg-primary/15 border-primary/40 text-primary'
                    : 'bg-transparent border-white/10 text-text-secondary hover:border-white/20'
                }`}
              >
                <Tag className="w-3 h-3" />
                {tag}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Filter Tags (Legacy Categories) */}
      <div className="flex flex-wrap items-center gap-2 mb-5">
        {EFFECT_CATEGORIES.map(filter => (
          <button
            key={filter}
            onClick={() => setActiveFilter(filter)}
            className={`rounded-full px-3 py-1 text-[10px] border transition-all ${
              activeFilter === filter
                ? 'bg-primary/15 border-primary/40 text-primary'
                : 'bg-transparent border-white/10 text-text-secondary hover:border-white/20'
            }`}
          >
            {filter}
          </button>
        ))}
        {(searchQuery || activeFilter !== 'All' || selectedFamily !== null || selectedTags.size > 0) && (
          <span className="text-[10px] text-text-secondary ml-2">
            {filteredEffects.length} of {effects.length} effects
          </span>
        )}
      </div>

      {/* Effects Grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-3">
        {filteredEffects.length === 0 ? (
          <div className="col-span-full flex flex-col items-center justify-center py-16 text-center">
            <SearchX className="w-12 h-12 text-text-secondary/30 mb-4" />
            <p className="text-sm text-text-secondary">No effects found</p>
            <p className="text-xs text-text-secondary/70 mt-1">
              {searchQuery
                ? `No results for "${searchQuery}"`
                : `No effects in ${activeFilter} category`}
            </p>
            <button
              onClick={handleClearFilters}
              className="mt-4 text-xs text-primary hover:underline"
            >
              Clear filters
            </button>
          </div>
        ) : filteredEffects.map(effect => {
          const meta = effectMetadata.get(effect.id) || effect.metadata;
          return (
            <div 
              key={effect.id} 
              onClick={() => handleEffectClick(effect)}
              className="group rounded-xl p-3 bg-surface/95 border border-white/10 hover:border-primary/40 hover:-translate-y-0.5 transition-all duration-200 cursor-pointer"
            >
              <div className={`relative rounded-lg overflow-hidden mb-3 h-20 bg-gradient-to-br ${effect.color}`}>
                <div className="absolute inset-0 animate-shimmer bg-gradient-to-r from-transparent via-white/5 to-transparent bg-[length:200%_100%]"></div>
                <button 
                  onClick={(e) => {
                    e.stopPropagation();
                    toggleStar(effect.id);
                  }}
                  className={`absolute top-2 right-2 p-1.5 rounded-full hover:bg-black/20 transition-colors ${effect.starred ? 'opacity-100' : 'opacity-0 group-hover:opacity-100'}`}
                >
                  <Star className={`w-3.5 h-3.5 ${effect.starred ? 'fill-primary text-primary' : 'text-text-secondary hover:text-primary'}`} />
                </button>
                <div className="absolute bottom-2 left-2 rounded px-1.5 py-0.5 bg-black/50 text-[8px] text-accent-cyan border border-accent-cyan/30">CENTER</div>
                {meta?.family && (
                  <div className="absolute bottom-2 right-2 rounded px-1.5 py-0.5 bg-black/50 text-[8px] text-text-primary border border-white/20">
                    {meta.family}
                  </div>
                )}
              </div>
              <div className="flex items-start justify-between">
                <div className="flex-1">
                  <div className="text-xs font-medium text-text-primary">{effect.name}</div>
                  <div className="text-[9px] text-text-secondary">{effect.desc}</div>
                  {meta?.tags && meta.tags.length > 0 && (
                    <div className="flex flex-wrap gap-1 mt-1">
                      {meta.tags.slice(0, 3).map(tag => (
                        <span key={tag} className="text-[8px] px-1.5 py-0.5 rounded bg-primary/10 text-primary border border-primary/20">
                          {tag}
                        </span>
                      ))}
                    </div>
                  )}
                </div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Effect Detail Modal */}
      {selectedEffect && (
        <div className="fixed inset-0 bg-black/50 flex items-center justify-center z-50 p-4" onClick={() => setSelectedEffect(null)}>
          <div className="bg-surface border border-white/20 rounded-xl p-6 max-w-2xl w-full max-h-[90vh] overflow-y-auto" onClick={(e) => e.stopPropagation()}>
            <div className="flex items-start justify-between mb-4">
              <div>
                <h2 className="text-xl font-display uppercase tracking-tight">{selectedEffect.name}</h2>
                <p className="text-sm text-text-secondary mt-1">{selectedEffect.desc}</p>
              </div>
              <div className="flex items-center gap-2">
                <button
                  onClick={async () => {
                    if (!isConnected) return;
                    await actions.setCurrentEffect(selectedEffect.id);
                    setSelectedEffect(null);
                  }}
                  disabled={!isConnected}
                  className={`h-9 px-3 rounded-lg bg-primary text-background text-xs font-medium transition-all touch-target ${
                    isConnected ? 'hover:brightness-110' : 'opacity-50 cursor-not-allowed'
                  }`}
                >
                  Apply
                </button>
                <button
                  onClick={() => setSelectedEffect(null)}
                  className="p-2 hover:bg-white/10 rounded-lg transition-colors"
                  aria-label="Close"
                >
                  <X className="w-5 h-5 text-text-secondary" aria-hidden="true" />
                </button>
              </div>
            </div>
            
            {(() => {
              const meta = effectMetadata.get(selectedEffect.id) || selectedEffect.metadata;
              if (!meta) {
                return <div className="text-sm text-text-secondary">Loading metadata...</div>;
              }
              
              return (
                <div className="space-y-4">
                  {meta.family && (
                    <div>
                      <div className="text-xs text-text-secondary mb-1">Family</div>
                      <div className="text-sm text-text-primary">{meta.family}</div>
                    </div>
                  )}
                  
                  {meta.story && (
                    <div>
                      <div className="text-xs text-text-secondary mb-1">Story</div>
                      <div className="text-sm text-text-primary">{meta.story}</div>
                    </div>
                  )}
                  
                  {meta.opticalIntent && (
                    <div>
                      <div className="text-xs text-text-secondary mb-1">Optical Intent</div>
                      <div className="text-sm text-text-primary">{meta.opticalIntent}</div>
                    </div>
                  )}
                  
                  {meta.tags && meta.tags.length > 0 && (
                    <div>
                      <div className="text-xs text-text-secondary mb-2">Tags</div>
                      <div className="flex flex-wrap gap-2">
                        {meta.tags.map(tag => (
                          <span key={tag} className="px-2 py-1 rounded bg-primary/10 text-primary text-xs border border-primary/20">
                            {tag}
                          </span>
                        ))}
                      </div>
                    </div>
                  )}
                  
                  {meta.recommended && (
                    <div>
                      <div className="text-xs text-text-secondary mb-2">Recommended Settings</div>
                      <div className="text-sm text-text-primary">
                        {meta.recommended.brightness && `Brightness: ${meta.recommended.brightness}`}
                        {meta.recommended.speed && ` • Speed: ${meta.recommended.speed}`}
                      </div>
                    </div>
                  )}
                </div>
              );
            })()}
          </div>
        </div>
      )}
    </div>
  );
};
