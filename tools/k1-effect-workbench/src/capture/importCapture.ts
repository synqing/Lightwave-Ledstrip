import type { ActualCapture, FrameSource, LedFrame } from '../types';
import { parseLedCapture } from './frameParser';

export type ImportedCapture = {
  capture: ActualCapture;
  frames: LedFrame[];
};

export function importCaptureBytes(name: string, bytes: Uint8Array, source: FrameSource = 'imported-capture'): ImportedCapture {
  const parsed = parseLedCapture(bytes, source);
  return {
    capture: {
      name,
      source,
      frameCount: parsed.frames.length,
      frameSize: parsed.frameSize,
      importedAt: new Date().toISOString(),
    },
    frames: parsed.frames,
  };
}

