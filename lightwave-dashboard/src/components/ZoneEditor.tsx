import React, { useState, useMemo, useCallback } from 'react';
import { useV2 } from '../state/v2';
import type { V2ZoneSegment } from '../services/v2/types';
import { Listbox } from './ui/Listbox';

interface ValidationError {
  field?: string;
  message: string;
}

const ZONE_COLORS = [
  'from-accent-cyan/80 to-accent-cyan/40',
  'from-accent-green/80 to-accent-green/40',
  'from-accent-purple/80 to-accent-purple/40',
  'from-primary/80 to-primary/40',
];

const ZONE_BORDER_COLORS = [
  'border-accent-cyan/60',
  'border-accent-green/60',
  'border-accent-purple/60',
  'border-primary/60',
];

const CENTER_LEFT = 79;
const CENTER_RIGHT = 80;
const MAX_LED = 159;

export const ZoneEditor: React.FC = () => {
  const { state, actions } = useV2();
  const zones = state.zones;
  const canControl = state.connection.httpOk;

  const [editingSegments, setEditingSegments] = useState<V2ZoneSegment[]>([]);
  const [selectedPreset, setSelectedPreset] = useState<number | null>(null);
  const [validationErrors, setValidationErrors] = useState<ValidationError[]>([]);
  const [isApplying, setIsApplying] = useState(false);

  // Initialize editing segments from current state
  React.useEffect(() => {
    if (zones?.segments && zones.segments.length > 0) {
      setEditingSegments([...zones.segments]);
      setSelectedPreset(null);
    }
  }, [zones?.segments]);

  // Validation functions
  const validateLayout = useCallback((segments: V2ZoneSegment[]): ValidationError[] => {
    const errors: ValidationError[] = [];

    if (segments.length === 0) {
      errors.push({ message: 'At least one zone is required' });
      return errors;
    }

    if (segments.length > 4) {
      errors.push({ message: 'Maximum 4 zones allowed' });
      return errors;
    }

    // Check each segment
    for (let i = 0; i < segments.length; i++) {
      const seg = segments[i];
      
      // Check zoneId matches index
      if (seg.zoneId !== i) {
        errors.push({ field: `zones[${i}].zoneId`, message: `Zone ID must be ${i}` });
      }

      // Check left segment bounds
      if (seg.s1LeftStart < 0 || seg.s1LeftStart > CENTER_LEFT) {
        errors.push({ field: `zones[${i}].s1LeftStart`, message: `Left start must be 0-${CENTER_LEFT}` });
      }
      if (seg.s1LeftEnd < seg.s1LeftStart || seg.s1LeftEnd > CENTER_LEFT) {
        errors.push({ field: `zones[${i}].s1LeftEnd`, message: `Left end must be >= start and <= ${CENTER_LEFT}` });
      }

      // Check right segment bounds
      if (seg.s1RightStart < CENTER_RIGHT || seg.s1RightStart > MAX_LED) {
        errors.push({ field: `zones[${i}].s1RightStart`, message: `Right start must be ${CENTER_RIGHT}-${MAX_LED}` });
      }
      if (seg.s1RightEnd < seg.s1RightStart || seg.s1RightEnd > MAX_LED) {
        errors.push({ field: `zones[${i}].s1RightEnd`, message: `Right end must be >= start and <= ${MAX_LED}` });
      }

      // Check centre-origin symmetry
      const leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
      const rightSize = seg.s1RightEnd - seg.s1RightStart + 1;
      if (leftSize !== rightSize) {
        errors.push({ field: `zones[${i}]`, message: `Zone ${i}: Left and right segments must have equal size` });
      }
      const leftDistance = CENTER_LEFT - seg.s1LeftEnd;
      const rightDistance = seg.s1RightStart - CENTER_RIGHT;
      if (leftDistance !== rightDistance) {
        errors.push({ field: `zones[${i}]`, message: `Zone ${i}: Segments must be symmetric around centre pair (79/80)` });
      }

      // Check minimum size
      if (leftSize < 1 || rightSize < 1) {
        errors.push({ field: `zones[${i}]`, message: `Zone ${i}: Each segment must have at least 1 LED` });
      }
    }

    // Check for overlaps and coverage
    const leftCoverage = new Set<number>();
    const rightCoverage = new Set<number>();

    for (let i = 0; i < segments.length; i++) {
      const seg = segments[i];
      
      // Check left segment
      for (let led = seg.s1LeftStart; led <= seg.s1LeftEnd; led++) {
        if (leftCoverage.has(led)) {
          errors.push({ field: `zones[${i}]`, message: `LED ${led} on left strip is assigned to multiple zones` });
        }
        leftCoverage.add(led);
      }

      // Check right segment
      for (let led = seg.s1RightStart; led <= seg.s1RightEnd; led++) {
        if (rightCoverage.has(led)) {
          errors.push({ field: `zones[${i}]`, message: `LED ${led} on right strip is assigned to multiple zones` });
        }
        rightCoverage.add(led);
      }
    }

    // Check complete coverage
    for (let led = 0; led <= CENTER_LEFT; led++) {
      if (!leftCoverage.has(led)) {
        errors.push({ message: `LED ${led} on left strip is not assigned to any zone` });
      }
    }
    for (let led = CENTER_RIGHT; led <= MAX_LED; led++) {
      if (!rightCoverage.has(led)) {
        errors.push({ message: `LED ${led} on right strip is not assigned to any zone` });
      }
    }

    // Check centre-outward ordering
    for (let i = 0; i < segments.length - 1; i++) {
      const current = segments[i];
      const next = segments[i + 1];
      
      // Inner zone should be closer to centre
      const currentMinDist = Math.min(CENTER_LEFT - current.s1LeftEnd, current.s1RightStart - CENTER_RIGHT);
      const nextMinDist = Math.min(CENTER_LEFT - next.s1LeftEnd, next.s1RightStart - CENTER_RIGHT);
      
      if (currentMinDist > nextMinDist) {
        errors.push({ message: `Zones must be ordered from centre outward (Zone ${i} should be closer to centre than Zone ${i + 1})` });
      }
    }

    // Check centre pair inclusion
    let includesCentre = false;
    for (const seg of segments) {
      if (seg.s1LeftEnd >= CENTER_LEFT || seg.s1RightStart <= CENTER_RIGHT) {
        includesCentre = true;
        break;
      }
    }
    if (!includesCentre) {
      errors.push({ message: 'At least one zone must include the centre pair (LEDs 79/80)' });
    }

    return errors;
  }, []);

  // Update a segment field
  const updateSegment = useCallback((index: number, field: keyof V2ZoneSegment, value: number) => {
    setEditingSegments(prev => {
      const updated = [...prev];
      updated[index] = { ...updated[index], [field]: value };
      
      // Recalculate totalLeds
      const seg = updated[index];
      const leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
      const rightSize = seg.s1RightEnd - seg.s1RightStart + 1;
      updated[index].totalLeds = (leftSize + rightSize) * 2; // Both strips
      
      // Run validation on update
      const errors = validateLayout(updated);
      setValidationErrors(errors);
      
      return updated;
    });
    setSelectedPreset(null);
  }, [validateLayout]);

  // Validate and apply changes
  const handleApply = useCallback(async () => {
    const errors = validateLayout(editingSegments);
    setValidationErrors(errors);
    
    if (errors.length > 0) {
      return;
    }

    setIsApplying(true);
    try {
      await actions.setZoneLayout(editingSegments);
      setValidationErrors([]);
    } catch (err) {
      setValidationErrors([{ message: err instanceof Error ? err.message : 'Failed to apply zone layout' }]);
    } finally {
      setIsApplying(false);
    }
  }, [editingSegments, validateLayout, actions]);

  // Preset segment definitions (matching firmware ZoneDefinition.h)
  const PRESET_SEGMENTS: Record<number, V2ZoneSegment[]> = {
    0: [ // Unified - uses 3-zone config
      { zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94, totalLeds: 30 },
      { zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139, totalLeds: 90 },
      { zoneId: 2, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 },
    ],
    1: [ // Dual Split - uses 3-zone config
      { zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94, totalLeds: 30 },
      { zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139, totalLeds: 90 },
      { zoneId: 2, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 },
    ],
    2: [ // Triple Rings - uses 3-zone config
      { zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94, totalLeds: 30 },
      { zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139, totalLeds: 90 },
      { zoneId: 2, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 },
    ],
    3: [ // Quad Active - uses 4-zone config
      { zoneId: 0, s1LeftStart: 60, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 99, totalLeds: 40 },
      { zoneId: 1, s1LeftStart: 40, s1LeftEnd: 59, s1RightStart: 100, s1RightEnd: 119, totalLeds: 40 },
      { zoneId: 2, s1LeftStart: 20, s1LeftEnd: 39, s1RightStart: 120, s1RightEnd: 139, totalLeds: 40 },
      { zoneId: 3, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 },
    ],
    4: [ // Heartbeat Focus - uses 3-zone config
      { zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94, totalLeds: 30 },
      { zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139, totalLeds: 90 },
      { zoneId: 2, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 },
    ],
  };

  // Handle preset selection
  const handlePresetSelect = useCallback((presetId: number) => {
    setSelectedPreset(presetId);
    const presetSegments = PRESET_SEGMENTS[presetId];
    if (presetSegments) {
      const updated = [...presetSegments];
      setEditingSegments(updated);
      // Validate preset segments (should be valid, but check anyway)
      const errors = validateLayout(updated);
      setValidationErrors(errors);
    }
  }, [validateLayout]);

  // Get LED position in visualization (0-159 for left, 160-319 for right)
  const getLedPosition = useCallback((ledIndex: number, isRight: boolean) => {
    if (isRight) {
      return ledIndex; // Right strip: 80-159 maps to 80-159 in visualization
    } else {
      return CENTER_LEFT - ledIndex; // Left strip: 0-79 maps to 79-0 (reversed, centre at right)
    }
  }, []);

  // Check if LED is in a zone
  const getZoneForLed = useCallback((ledIndex: number, isRight: boolean) => {
    for (const seg of editingSegments) {
      if (isRight) {
        if (ledIndex >= seg.s1RightStart && ledIndex <= seg.s1RightEnd) {
          return seg.zoneId;
        }
      } else {
        if (ledIndex >= seg.s1LeftStart && ledIndex <= seg.s1LeftEnd) {
          return seg.zoneId;
        }
      }
    }
    return -1;
  }, [editingSegments]);

  const presetOptions = useMemo(() => {
    if (!zones?.presets) return [];
    return zones.presets.map(p => ({ value: p.id, label: p.name }));
  }, [zones?.presets]);

  if (!zones) {
    return (
      <div className="card-hover rounded-xl p-4 sm:p-5" style={{
        background: 'rgba(37,45,63,0.95)',
        border: '1px solid rgba(255,255,255,0.10)',
        boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
      }}>
        <p className="text-text-secondary">Zone system not available</p>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      {/* Header */}
      <div className="card-hover rounded-xl p-4 sm:p-5" style={{
        background: 'rgba(37,45,63,0.95)',
        border: '1px solid rgba(255,255,255,0.10)',
        boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
      }}>
        <div className="flex items-center justify-between mb-4">
          <h3 className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>
            Zone Layout Editor
          </h3>
          <span className="rounded-lg px-2 py-1 text-[0.5625rem] text-text-secondary" style={{ background: 'rgba(47,56,73,0.65)', border: '1px solid rgba(255,255,255,0.08)' }}>
            {editingSegments.length} Zones
          </span>
        </div>

        {/* Preset Selector */}
        {presetOptions.length > 0 && (
          <div className="mb-4">
            <label className="block text-[0.6875rem] font-medium text-text-primary mb-2">
              Preset
            </label>
            <Listbox
              options={[{ value: -1, label: 'Custom' }, ...presetOptions]}
              value={selectedPreset ?? -1}
              onChange={(val) => {
                if (val === -1) {
                  setSelectedPreset(null);
                } else {
                  handlePresetSelect(val);
                }
              }}
              disabled={!canControl}
            />
          </div>
        )}

        {/* Zone Visualization */}
        <div className="mb-4">
          <div className="text-[0.6875rem] font-medium text-text-primary mb-2">
            LED Strip Visualization
          </div>
          <div className="relative" style={{ height: '120px' }}>
            {/* Left Strip (0-79, displayed reversed) */}
            <div className="absolute left-0 top-0 w-1/2 h-full">
              <div className="text-[0.5625rem] text-text-secondary mb-1">Left (0-79)</div>
              <div className="flex flex-row-reverse gap-0.5 h-16">
                {Array.from({ length: 80 }, (_, i) => {
                  const ledIndex = i;
                  const zoneId = getZoneForLed(ledIndex, false);
                  const isCenter = ledIndex === CENTER_LEFT;
                  return (
                    <div
                      key={`left-${ledIndex}`}
                      className={`flex-1 rounded-sm ${
                        zoneId >= 0
                          ? `bg-gradient-to-b ${ZONE_COLORS[zoneId % ZONE_COLORS.length]} ${ZONE_BORDER_COLORS[zoneId % ZONE_BORDER_COLORS.length]} border`
                          : 'bg-[#2f3849]'
                      } ${isCenter ? 'ring-2 ring-primary ring-offset-1 ring-offset-[#252d3f]' : ''}`}
                      title={`LED ${ledIndex}${isCenter ? ' (Centre)' : ''}`}
                    />
                  );
                })}
              </div>
            </div>

            {/* Centre Divider */}
            <div className="absolute left-1/2 top-0 w-px h-full bg-primary/40" />

            {/* Right Strip (80-159) */}
            <div className="absolute right-0 top-0 w-1/2 h-full">
              <div className="text-[0.5625rem] text-text-secondary mb-1 text-right">Right (80-159)</div>
              <div className="flex gap-0.5 h-16">
                {Array.from({ length: 80 }, (_, i) => {
                  const ledIndex = CENTER_RIGHT + i;
                  const zoneId = getZoneForLed(ledIndex, true);
                  const isCenter = ledIndex === CENTER_RIGHT;
                  return (
                    <div
                      key={`right-${ledIndex}`}
                      className={`flex-1 rounded-sm ${
                        zoneId >= 0
                          ? `bg-gradient-to-b ${ZONE_COLORS[zoneId % ZONE_COLORS.length]} ${ZONE_BORDER_COLORS[zoneId % ZONE_BORDER_COLORS.length]} border`
                          : 'bg-[#2f3849]'
                      } ${isCenter ? 'ring-2 ring-primary ring-offset-1 ring-offset-[#252d3f]' : ''}`}
                      title={`LED ${ledIndex}${isCenter ? ' (Centre)' : ''}`}
                    />
                  );
                })}
              </div>
            </div>
          </div>
          <div className="text-[0.5625rem] text-text-secondary mt-2 text-center">
            Centre pair: LEDs 79 (left) / 80 (right)
          </div>
        </div>

        {/* Zone Segment Editors */}
        <div className="space-y-4">
          {editingSegments.map((seg, index) => {
            const borderColorClass = index < ZONE_BORDER_COLORS.length 
              ? ZONE_BORDER_COLORS[index % ZONE_BORDER_COLORS.length]
              : 'border-white/8';
            return (
            <div
              key={index}
              className={`rounded-lg p-3 border ${borderColorClass}`}
              style={{
                background: 'rgba(47,56,73,0.5)',
              }}
            >
              <div className="flex items-center justify-between mb-3">
                <h4 className="text-[0.75rem] font-medium text-text-primary">
                  Zone {seg.zoneId}
                </h4>
                <span className="text-[0.5625rem] text-text-secondary">
                  {seg.totalLeds} LEDs total
                </span>
              </div>

              <div className="grid grid-cols-2 gap-3">
                {/* Left Segment */}
                <div>
                  <label className="block text-[0.5625rem] text-text-secondary mb-1">Left Segment</label>
                  <div className="flex gap-2">
                    <div className="flex-1">
                      <label className="block text-[0.5rem] text-text-secondary mb-0.5">Start</label>
                      <input
                        type="number"
                        min={0}
                        max={CENTER_LEFT}
                        value={seg.s1LeftStart}
                        onChange={(e) => updateSegment(index, 's1LeftStart', parseInt(e.target.value) || 0)}
                        className="w-full px-2 py-1 text-[0.75rem] rounded bg-[#2f3849] border border-white/5 text-text-primary"
                        disabled={!canControl}
                      />
                    </div>
                    <div className="flex-1">
                      <label className="block text-[0.5rem] text-text-secondary mb-0.5">End</label>
                      <input
                        type="number"
                        min={seg.s1LeftStart}
                        max={CENTER_LEFT}
                        value={seg.s1LeftEnd}
                        onChange={(e) => updateSegment(index, 's1LeftEnd', parseInt(e.target.value) || seg.s1LeftStart)}
                        className="w-full px-2 py-1 text-[0.75rem] rounded bg-[#2f3849] border border-white/5 text-text-primary"
                        disabled={!canControl}
                      />
                    </div>
                  </div>
                </div>

                {/* Right Segment */}
                <div>
                  <label className="block text-[0.5625rem] text-text-secondary mb-1">Right Segment</label>
                  <div className="flex gap-2">
                    <div className="flex-1">
                      <label className="block text-[0.5rem] text-text-secondary mb-0.5">Start</label>
                      <input
                        type="number"
                        min={CENTER_RIGHT}
                        max={MAX_LED}
                        value={seg.s1RightStart}
                        onChange={(e) => updateSegment(index, 's1RightStart', parseInt(e.target.value) || CENTER_RIGHT)}
                        className="w-full px-2 py-1 text-[0.75rem] rounded bg-[#2f3849] border border-white/5 text-text-primary"
                        disabled={!canControl}
                      />
                    </div>
                    <div className="flex-1">
                      <label className="block text-[0.5rem] text-text-secondary mb-0.5">End</label>
                      <input
                        type="number"
                        min={seg.s1RightStart}
                        max={MAX_LED}
                        value={seg.s1RightEnd}
                        onChange={(e) => updateSegment(index, 's1RightEnd', parseInt(e.target.value) || seg.s1RightStart)}
                        className="w-full px-2 py-1 text-[0.75rem] rounded bg-[#2f3849] border border-white/5 text-text-primary"
                        disabled={!canControl}
                      />
                    </div>
                  </div>
                </div>
              </div>
            </div>
            );
          })}
        </div>

        {/* Validation Errors */}
        {validationErrors.length > 0 && (
          <div className="mt-4 p-3 rounded-lg bg-red-500/10 border border-red-500/30">
            <div className="text-[0.6875rem] font-medium text-red-400 mb-2">Validation Errors</div>
            <ul className="space-y-1">
              {validationErrors.map((err, i) => (
                <li key={i} className="text-[0.5625rem] text-red-300">
                  {err.message}
                </li>
              ))}
            </ul>
          </div>
        )}

        {/* Apply Button */}
        <button
          onClick={handleApply}
          disabled={!canControl || isApplying || validationErrors.length > 0}
          className="mt-4 w-full px-4 py-2 rounded-lg bg-primary/20 hover:bg-primary/30 border border-primary/40 text-primary text-[0.75rem] font-medium disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
        >
          {isApplying ? 'Applying...' : 'Apply Zone Layout'}
        </button>
      </div>
    </div>
  );
};

