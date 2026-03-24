/**
 * Code Export side panel — displays generated C++ and provides
 * copy-to-clipboard and download functionality.
 *
 * Regenerates on every call to refresh() (driven by graph changes).
 * Passes zone-aware flag from the engine to the code generator.
 */

import { useState, useCallback, useRef, useEffect } from 'react';
import type { GraphEngine } from '../engine/engine';
import { generateCpp } from '../codegen/generator';
import './CodeExport.css';

// ---------------------------------------------------------------------------
// Props
// ---------------------------------------------------------------------------

interface CodeExportProps {
  engine: GraphEngine;
  /** External trigger: bump this value to regenerate code. */
  generation: number;
}

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

export function CodeExport({ engine, generation }: CodeExportProps) {
  const [effectName, setEffectName] = useState('MyComposedEffect');
  const [copied, setCopied] = useState(false);
  const [code, setCode] = useState({ header: '', source: '' });
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  // Regenerate code when graph changes or effect name changes
  useEffect(() => {
    try {
      const generated = generateCpp(engine, effectName, engine.zoneAware);
      setCode(generated);
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      setCode({
        header: `// Code generation failed:\n// ${message}`,
        source: '',
      });
    }
  }, [engine, effectName, generation]);

  // Copy to clipboard
  const handleCopy = useCallback(async () => {
    try {
      await navigator.clipboard.writeText(code.header);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    } catch {
      // Fallback: select textarea content
      textareaRef.current?.select();
      document.execCommand('copy');
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    }
  }, [code.header]);

  // Download as .h file
  const handleDownload = useCallback(() => {
    const className = effectName
      .replace(/[^a-zA-Z0-9\s]/g, '')
      .split(/\s+/)
      .map((w) => w.charAt(0).toUpperCase() + w.slice(1).toLowerCase())
      .join('');
    const filename = `${className}Effect.h`;

    const blob = new Blob([code.header], { type: 'text/x-c++hdr' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = filename;
    link.click();
    URL.revokeObjectURL(url);
  }, [code.header, effectName]);

  return (
    <div className="code-export-panel">
      <div className="code-export-header">
        <h3 className="code-export-title">C++ Export</h3>
        {engine.zoneAware && (
          <span className="code-export-zone-badge">Zone-Aware</span>
        )}
      </div>

      <div className="code-export-name-row">
        <label className="code-export-label" htmlFor="effect-name-input">
          Effect Name
        </label>
        <input
          id="effect-name-input"
          className="code-export-name-input"
          type="text"
          value={effectName}
          onChange={(e) => setEffectName(e.target.value)}
          placeholder="MyComposedEffect"
          spellCheck={false}
        />
      </div>

      <textarea
        ref={textareaRef}
        className="code-export-textarea"
        value={code.header}
        readOnly
        spellCheck={false}
        wrap="off"
      />

      <div className="code-export-actions">
        <button
          className="code-export-btn code-export-btn-copy"
          onClick={handleCopy}
          type="button"
        >
          {copied ? 'Copied' : 'Copy to Clipboard'}
        </button>
        <button
          className="code-export-btn code-export-btn-download"
          onClick={handleDownload}
          type="button"
        >
          Download .h
        </button>
      </div>
    </div>
  );
}
