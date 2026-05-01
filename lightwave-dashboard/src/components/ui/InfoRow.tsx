import React from 'react';

interface InfoRowProps {
  label: string;
  value: string;
}

export const InfoRow: React.FC<InfoRowProps> = ({ label, value }) => (
  <div className="flex justify-between items-center py-2 border-b border-white/5 last:border-0">
    <span className="text-text-secondary">{label}</span>
    <span className="font-mono text-text-primary bg-surface/50 px-2 py-0.5 rounded border border-white/5">{value}</span>
  </div>
);
