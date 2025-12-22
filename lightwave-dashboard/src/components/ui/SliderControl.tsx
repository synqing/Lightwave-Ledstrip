import React from 'react';

interface SliderControlProps {
  label: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  onChange: (val: number) => void;
  format?: (val: number) => string;
  icon?: React.ReactNode;
  fillColor?: string;
  disabled?: boolean;
}

export const SliderControl: React.FC<SliderControlProps> = ({ 
  label, 
  value, 
  min, 
  max, 
  step=1, 
  onChange, 
  format, 
  icon,
  fillColor = "from-primary/80 to-primary",
  disabled = false
}) => {
  const percentage = ((value - min) / (max - min)) * 100;
  
  return (
    <div className={`space-y-2 mb-4 group ${disabled ? 'opacity-50' : ''}`}>
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          {icon && <span className="text-text-secondary">{icon}</span>}
          <label className="text-[0.6875rem] font-medium text-text-primary">{label}</label>
        </div>
        <span className="text-[0.75rem] text-text-primary tabular-nums">
          {format ? format(value) : value}
        </span>
      </div>
      <div className="relative h-8 slider-container">
        <div className="absolute inset-x-0 top-1/2 -translate-y-1/2 h-1 rounded-full bg-[#2f3849]"></div>
        <div 
          className={`absolute left-0 top-1/2 -translate-y-1/2 h-1 rounded-full bg-gradient-to-90 ${fillColor}`}
          style={{ width: `${percentage}%` }}
        />
        <input 
          type="range" 
          min={min} 
          max={max} 
          step={step}
          value={value}
          onChange={(e) => onChange(Number(e.target.value))}
          disabled={disabled}
          className={`absolute inset-0 w-full opacity-0 z-10 ${disabled ? 'cursor-not-allowed' : 'cursor-pointer'}`}
          aria-label={label}
        />
        <div 
          className="absolute top-1/2 -translate-y-1/2 w-[1.125rem] h-[1.125rem] rounded-full bg-primary border-2 border-white/20 shadow-[0_2px_8px_rgba(0,0,0,0.4)] pointer-events-none slider-thumb"
          style={{ left: `calc(${percentage}% - 0.5625rem)` }}
        />
      </div>
    </div>
  );
};
