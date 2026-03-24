/**
 * Effects Library panel — browse and load effect presets.
 *
 * Displays presets grouped by category (Fixed, Original, Reference).
 * Each card shows the effect name, description, node count, and a
 * "Load" button that replaces the current graph with the preset.
 */

import { useMemo } from 'react';
import type { GraphPreset } from '../presets';
import { ALL_PRESETS } from '../presets';
import './EffectsLibrary.css';

// ---------------------------------------------------------------------------
// Category display configuration
// ---------------------------------------------------------------------------

interface CategoryConfig {
  key: string;
  label: string;
  cssModifier: string;
}

const CATEGORY_ORDER: CategoryConfig[] = [
  { key: '5L-AR (Fixed)',      label: '5L-AR (Fixed)',      cssModifier: 'fixed' },
  { key: '5L-AR (Original)',   label: '5L-AR (Original)',   cssModifier: 'original' },
  { key: 'Demo',               label: 'Demo',               cssModifier: 'reference' },
  { key: 'Fixed (Working)',    label: 'Fixed (Working)',     cssModifier: 'fixed' },
  { key: 'Original (Broken)',  label: 'Original (Broken)',  cssModifier: 'original' },
  { key: 'Reference',          label: 'Reference',          cssModifier: 'reference' },
];

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

interface EffectsLibraryProps {
  onLoadPreset: (preset: GraphPreset) => void;
  onClose: () => void;
}

export function EffectsLibrary({ onLoadPreset, onClose }: EffectsLibraryProps) {
  // Group presets by category
  const grouped = useMemo(() => {
    const groups = new Map<string, GraphPreset[]>();
    for (const cat of CATEGORY_ORDER) {
      groups.set(cat.key, []);
    }

    for (const preset of ALL_PRESETS) {
      const category = preset.category ?? 'Reference';
      const existing = groups.get(category);
      if (existing) {
        existing.push(preset);
      } else {
        // Unknown category — append to Reference
        const ref = groups.get('Reference');
        if (ref) ref.push(preset);
      }
    }

    return groups;
  }, []);

  const hasPresets = ALL_PRESETS.length > 0;

  return (
    <div className="effects-library">
      <div className="effects-library__header">
        <span className="effects-library__title">Effects</span>
        <button
          className="effects-library__close"
          onClick={onClose}
          title="Close effects library"
        >
          &times;
        </button>
      </div>

      <div className="effects-library__list">
        {!hasPresets && (
          <div className="effects-library__empty">
            No presets available yet.
            <br />
            Presets will appear here once they are added to the library.
          </div>
        )}

        {hasPresets &&
          CATEGORY_ORDER.map(({ key, label, cssModifier }) => {
            const presets = grouped.get(key);
            if (!presets || presets.length === 0) return null;

            return (
              <div key={key} className="effects-library__category">
                <div
                  className={`effects-library__category-label effects-library__category-label--${cssModifier}`}
                >
                  {label}
                </div>
                {presets.map((preset) => (
                  <PresetCard
                    key={preset.name}
                    preset={preset}
                    onLoad={onLoadPreset}
                  />
                ))}
              </div>
            );
          })}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Preset card
// ---------------------------------------------------------------------------

interface PresetCardProps {
  preset: GraphPreset;
  onLoad: (preset: GraphPreset) => void;
}

function PresetCard({ preset, onLoad }: PresetCardProps) {
  const nodeCount = preset.nodes.length;

  return (
    <div className="effects-library__card">
      <div className="effects-library__card-name">{preset.name}</div>
      <div className="effects-library__card-desc">{preset.description}</div>
      <div className="effects-library__card-footer">
        <span className="effects-library__card-meta">
          {nodeCount} node{nodeCount !== 1 ? 's' : ''}
        </span>
        <button
          className="effects-library__card-load"
          onClick={() => onLoad(preset)}
          title={`Load "${preset.name}" preset`}
        >
          Load
        </button>
      </div>
    </div>
  );
}
