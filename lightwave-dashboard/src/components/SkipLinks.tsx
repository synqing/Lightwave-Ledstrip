import React from 'react';

/**
 * SkipLinks Component
 * 
 * Provides skip navigation links for keyboard users to jump to
 * main content areas, improving accessibility and navigation efficiency.
 * 
 * @component
 * 
 * @example
 * ```tsx
 * <SkipLinks />
 * ```
 */

export const SkipLinks: React.FC = () => {
  return (
    <div className="sr-only focus-within:not-sr-only focus-within:absolute focus-within:top-0 focus-within:left-0 focus-within:z-50 focus-within:p-2">
      <a
        href="#main-content"
        className="block px-4 py-2 bg-primary text-background rounded-lg font-medium text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:ring-offset-2 focus-within:not-sr-only"
      >
        Skip to main content
      </a>
    </div>
  );
};

