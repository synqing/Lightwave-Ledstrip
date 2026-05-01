import { describe, expect, it } from 'vitest';
import {
  LED_COUNT,
  RGB_BYTES,
  SERIAL_HEADER_SIZE,
  SERIAL_SYNC,
  SERIAL_V2_FRAME_SIZE,
  WEB_FRAME_SIZE,
  WEB_LEGACY_FRAME_SIZE,
  WEB_MAGIC,
  parseLedCapture,
} from '../src/capture/frameParser';

function payload(): Uint8Array {
  const bytes = new Uint8Array(RGB_BYTES);
  bytes[79 * 3] = 255;
  bytes[80 * 3 + 1] = 255;
  return bytes;
}

describe('frame parser', () => {
  it('parses raw 960-byte RGB frames', () => {
    const parsed = parseLedCapture(payload());
    expect(parsed.frames).toHaveLength(1);
    expect(parsed.frames[0].leds).toHaveLength(LED_COUNT * 3);
  });

  it('parses 961-byte legacy web frames', () => {
    const frame = new Uint8Array(WEB_LEGACY_FRAME_SIZE);
    frame[0] = WEB_MAGIC;
    frame.set(payload(), 1);
    const parsed = parseLedCapture(frame);
    expect(parsed.frameSize).toBe(WEB_LEGACY_FRAME_SIZE);
    expect(parsed.frames[0].leds[79 * 3]).toBe(255);
  });

  it('parses 966-byte web frames', () => {
    const frame = new Uint8Array(WEB_FRAME_SIZE);
    frame[0] = WEB_MAGIC;
    frame.set(payload(), 6);
    const parsed = parseLedCapture(frame);
    expect(parsed.frameSize).toBe(WEB_FRAME_SIZE);
    expect(parsed.frames[0].leds[80 * 3 + 1]).toBe(255);
  });

  it('parses serial v2 frames with a 17-byte header', () => {
    const frame = new Uint8Array(SERIAL_V2_FRAME_SIZE);
    frame[0] = SERIAL_SYNC;
    frame.set(payload(), SERIAL_HEADER_SIZE);
    const parsed = parseLedCapture(frame);
    expect(parsed.frameSize).toBe(SERIAL_V2_FRAME_SIZE);
    expect(parsed.frames[0].leds[79 * 3]).toBe(255);
  });
});

