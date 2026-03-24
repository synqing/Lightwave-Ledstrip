/**
 * React hook wrapping the AudioManager for mic capture and simulated audio.
 *
 * Exposes start/stop lifecycle, active state, and getFrame() for the
 * engine tick loop to consume.
 */

import { useRef, useState, useCallback, useEffect } from 'react';
import { AudioManager } from '../../audio/audio-manager';
import type { ControlBusFrame } from '../../engine/types';

export interface AudioManagerHandle {
  start: () => Promise<void>;
  startSimulated: () => void;
  stop: () => void;
  isActive: boolean;
  isSimulated: boolean;
  getFrame: () => ControlBusFrame;
  tick: (dt: number) => void;
}

export function useAudioManager(): AudioManagerHandle {
  const managerRef = useRef<AudioManager | null>(null);
  const [isActive, setIsActive] = useState(false);
  const [isSimulated, setIsSimulated] = useState(false);

  // Lazily create AudioManager singleton
  if (!managerRef.current) {
    managerRef.current = new AudioManager();
  }

  const manager = managerRef.current;

  const start = useCallback(async () => {
    await manager.start();
    setIsActive(manager.isActive);
    setIsSimulated(manager.isSimulated);
  }, [manager]);

  const startSimulated = useCallback(() => {
    manager.startSimulated();
    setIsActive(manager.isActive);
    setIsSimulated(manager.isSimulated);
  }, [manager]);

  const stop = useCallback(() => {
    manager.stop();
    setIsActive(false);
    setIsSimulated(false);
  }, [manager]);

  const getFrame = useCallback((): ControlBusFrame => {
    return manager.getFrame();
  }, [manager]);

  const tick = useCallback((dt: number) => {
    manager.tick(dt);
  }, [manager]);

  // Clean up on unmount
  useEffect(() => {
    return () => {
      manager.stop();
    };
  }, [manager]);

  return {
    start,
    startSimulated,
    stop,
    isActive,
    isSimulated,
    getFrame,
    tick,
  };
}
