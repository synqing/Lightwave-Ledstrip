import { describe, it, expect } from 'vitest';
import { parseLedFrame } from './useLedStream';
import { LED_STREAM_MAGIC, LED_STREAM_VERSION_V1, LEDS_PER_STRIP, LED_FRAME_SIZE_V0, LED_FRAME_SIZE_V1, FRAME_HEADER_SIZE } from '../services/v2/ws';

describe('parseLedFrame', () => {
  it('parses v0 legacy format (flat 320 LEDs)', () => {
    // Build legacy frame: [MAGIC][RGB×320]
    const frame = new Uint8Array(LED_FRAME_SIZE_V0);
    frame[0] = LED_STREAM_MAGIC;
    
    // Fill with test data: top strip (0-159) = red, bottom strip (160-319) = blue
    for (let i = 0; i < LEDS_PER_STRIP; i++) {
      const idx = 1 + i * 3;
      frame[idx] = 255;     // R
      frame[idx + 1] = 0;   // G
      frame[idx + 2] = 0;   // B
    }
    for (let i = 0; i < LEDS_PER_STRIP; i++) {
      const idx = 1 + (LEDS_PER_STRIP + i) * 3;
      frame[idx] = 0;       // R
      frame[idx + 1] = 0;   // G
      frame[idx + 2] = 255; // B
    }
    
    const result = parseLedFrame(frame);
    expect(result).not.toBeNull();
    expect(result!.top.length).toBe(LEDS_PER_STRIP * 3);
    expect(result!.bottom.length).toBe(LEDS_PER_STRIP * 3);
    
    // Check first LED of top strip is red
    expect(result!.top[0]).toBe(255);
    expect(result!.top[1]).toBe(0);
    expect(result!.top[2]).toBe(0);
    
    // Check first LED of bottom strip is blue
    expect(result!.bottom[0]).toBe(0);
    expect(result!.bottom[1]).toBe(0);
    expect(result!.bottom[2]).toBe(255);
  });
  
  it('parses v1 dual-strip format', () => {
    // Build v1 frame: [MAGIC][VERSION][NUM_STRIPS][LEDS_PER_STRIP][STRIP0_ID][RGB×160][STRIP1_ID][RGB×160]
    const frame = new Uint8Array(LED_FRAME_SIZE_V1);
    frame[0] = LED_STREAM_MAGIC;
    frame[1] = LED_STREAM_VERSION_V1;
    frame[2] = 2; // NUM_STRIPS
    frame[3] = LEDS_PER_STRIP;
    
    let offset = FRAME_HEADER_SIZE;
    
    // Strip 0 (TOP): green
    frame[offset++] = 0; // Strip ID
    for (let i = 0; i < LEDS_PER_STRIP; i++) {
      frame[offset++] = 0;   // R
      frame[offset++] = 255;  // G
      frame[offset++] = 0;    // B
    }
    
    // Strip 1 (BOTTOM): cyan
    frame[offset++] = 1; // Strip ID
    for (let i = 0; i < LEDS_PER_STRIP; i++) {
      frame[offset++] = 0;   // R
      frame[offset++] = 255;  // G
      frame[offset++] = 255;  // B
    }
    
    const result = parseLedFrame(frame);
    expect(result).not.toBeNull();
    expect(result!.top.length).toBe(LEDS_PER_STRIP * 3);
    expect(result!.bottom.length).toBe(LEDS_PER_STRIP * 3);
    
    // Check first LED of top strip is green
    expect(result!.top[0]).toBe(0);
    expect(result!.top[1]).toBe(255);
    expect(result!.top[2]).toBe(0);
    
    // Check first LED of bottom strip is cyan
    expect(result!.bottom[0]).toBe(0);
    expect(result!.bottom[1]).toBe(255);
    expect(result!.bottom[2]).toBe(255);
  });
  
  it('returns null for invalid frame size', () => {
    const invalidFrame = new Uint8Array(100);
    invalidFrame[0] = LED_STREAM_MAGIC;
    expect(parseLedFrame(invalidFrame)).toBeNull();
  });
  
  it('returns null for invalid magic byte in v1 format', () => {
    const frame = new Uint8Array(LED_FRAME_SIZE_V1);
    frame[0] = 0xFF; // Wrong magic
    frame[1] = LED_STREAM_VERSION_V1;
    expect(parseLedFrame(frame)).toBeNull();
  });
});

