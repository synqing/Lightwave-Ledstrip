import React from 'react';

interface PresetButtonProps {
  label: string;
  color: string;
  onClick?: () => void;
  disabled?: boolean;
}

export const PresetButton: React.FC<PresetButtonProps> = ({ label, color, onClick, disabled = false }) => (
  <button 
    onClick={onClick}
    disabled={disabled}
    className={`relative overflow-hidden rounded-lg p-3 text-left border border-white/5 transition-all group active:scale-95 ${
      disabled ? 'opacity-50 cursor-not-allowed' : 'hover:border-white/20'
    }`}
  >
    <div className={`absolute inset-0 bg-gradient-to-br ${color} opacity-10 ${disabled ? '' : 'group-hover:opacity-20'} transition-opacity`}></div>
    <div className="relative z-10">
      <div className={`w-8 h-8 rounded-lg bg-gradient-to-br ${color} mb-2 shadow-lg ${disabled ? '' : 'group-hover:scale-110'} transition-transform duration-300`}></div>
      <span className={`text-xs font-medium text-text-secondary ${disabled ? '' : 'group-hover:text-text-primary'} block`}>{label}</span>
    </div>
  </button>
);
