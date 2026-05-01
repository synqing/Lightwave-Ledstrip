import { useEffect, useRef, useState } from 'react';
import { LED_STREAM_MAGIC, LED_STREAM_VERSION_V1, LEDS_PER_STRIP, LED_FRAME_SIZE_V0, LED_FRAME_SIZE_V1, FRAME_HEADER_SIZE } from '../services/v2/ws';
import { useV2 } from '../state/v2';

export interface DualStripLedData {
  top: Uint8Array;    // Top edge (GPIO4), 160 LEDs × RGB = 480 bytes
  bottom: Uint8Array; // Bottom edge (GPIO5), 160 LEDs × RGB = 480 bytes
}

export interface LedStreamState {
  ledData: DualStripLedData | null;
  isSubscribed: boolean;
  fps: number;
}

/**
 * Hook to subscribe to real-time LED frame streaming from the device.
 * Connects to the firmware's binary WebSocket LED streaming.
 *
 * @param enabled - Whether to subscribe to LED streaming (default: true)
 * @returns { ledData, isSubscribed, fps }
 */
/**
 * Parse LED frame data into dual-strip format
 * Supports both v0 (legacy flat 320) and v1 (dual-strip) formats
 * @internal Exported for testing
 */
export function parseLedFrame(bytes: Uint8Array): DualStripLedData | null {
  if (bytes.length === LED_FRAME_SIZE_V0) {
    // Legacy v0 format: [MAGIC][RGB×320]
    // Split into two strips: [0..159] = top, [160..319] = bottom
    const rgbData = bytes.slice(1); // Skip magic byte
    const top = rgbData.slice(0, LEDS_PER_STRIP * 3);
    const bottom = rgbData.slice(LEDS_PER_STRIP * 3, LEDS_PER_STRIP * 3 * 2);
    return { top, bottom };
  } else if (bytes.length === LED_FRAME_SIZE_V1) {
    // v1 format: [MAGIC][VERSION][NUM_STRIPS][LEDS_PER_STRIP][STRIP0_ID][RGB×160][STRIP1_ID][RGB×160]
    if (bytes[0] !== LED_STREAM_MAGIC || bytes[1] !== LED_STREAM_VERSION_V1) {
      return null;
    }
    
    let offset = FRAME_HEADER_SIZE;
    
    // Strip 0 (TOP edge, GPIO4)
    const strip0Id = bytes[offset++];
    if (strip0Id !== 0) {
      console.warn('[LED Stream] Unexpected strip 0 ID:', strip0Id);
    }
    const top = bytes.slice(offset, offset + LEDS_PER_STRIP * 3);
    offset += LEDS_PER_STRIP * 3;
    
    // Strip 1 (BOTTOM edge, GPIO5)
    const strip1Id = bytes[offset++];
    if (strip1Id !== 1) {
      console.warn('[LED Stream] Unexpected strip 1 ID:', strip1Id);
    }
    const bottom = bytes.slice(offset, offset + LEDS_PER_STRIP * 3);
    
    return { top, bottom };
  }
  
  return null;
}

export function useLedStream(enabled = true): LedStreamState {
  const { state, actions } = useV2();

  const [ledData, setLedData] = useState<DualStripLedData | null>(null);
  const [isSubscribed, setIsSubscribed] = useState(false);
  const [fps, setFps] = useState(0);

  // FPS tracking
  const frameCountRef = useRef(0);
  const lastFpsUpdateRef = useRef(0);

  useEffect(() => {
    if (!enabled || state.connection.wsStatus !== 'connected') {
      setIsSubscribed(false);
      setLedData(null);
      return;
    }

    const wsClient = actions.getWsClient();
    if (!wsClient) {
      setIsSubscribed(false);
      setLedData(null);
      return;
    }

    // Subscribe to LED stream
    wsClient.send({ type: 'ledStream.subscribe' });
    setIsSubscribed(true);
    frameCountRef.current = 0;
    lastFpsUpdateRef.current = Date.now();

    // Handle binary LED frames
    const unsubscribeBinary = wsClient.onBinary((bytes) => {
      const parsed = parseLedFrame(bytes);
      if (parsed) {
        setLedData(parsed);
      }

      // Update FPS counter
      frameCountRef.current += 1;
      const now = Date.now();
      if (now - lastFpsUpdateRef.current >= 1000) {
        setFps(frameCountRef.current);
        frameCountRef.current = 0;
        lastFpsUpdateRef.current = now;
      }
    });

    return () => {
      // Unsubscribe when unmounting or disabled
      wsClient.send({ type: 'ledStream.unsubscribe' });
      unsubscribeBinary();
      setIsSubscribed(false);
      setLedData(null);
    };
  }, [enabled, state.connection.wsStatus, actions]);

  return { ledData, isSubscribed, fps };
}

export { LEDS_PER_STRIP };
