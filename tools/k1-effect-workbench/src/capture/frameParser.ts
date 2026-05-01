import type { FrameSource, LedFrame } from '../types';

export const LED_COUNT = 320;
export const RGB_BYTES = LED_COUNT * 3;
export const WEB_LEGACY_FRAME_SIZE = 961;
export const WEB_FRAME_SIZE = 966;
export const SERIAL_V2_FRAME_SIZE = 1009;
export const SERIAL_V4_FRAME_SIZE = 529;
export const SERIAL_HEADER_SIZE = 17;
export const SERIAL_METRICS_SIZE = 32;
export const WEB_MAGIC = 0xfe;
export const SERIAL_SYNC = 0xfd;

export type ParsedCapture = {
  frames: LedFrame[];
  frameSize: number;
};

function payloadOffsetForFrame(frame: Uint8Array): number | null {
  if (frame.length === RGB_BYTES) return 0;
  if (frame.length === WEB_LEGACY_FRAME_SIZE && frame[0] === WEB_MAGIC) return 1;
  if (frame.length === WEB_FRAME_SIZE && frame[0] === WEB_MAGIC) return 6;
  if (frame.length === SERIAL_V2_FRAME_SIZE && frame[0] === SERIAL_SYNC) return SERIAL_HEADER_SIZE;
  return null;
}

function payloadLengthForFrame(frame: Uint8Array): number {
  if (frame.length === SERIAL_V4_FRAME_SIZE) return 160 * 3;
  return RGB_BYTES;
}

function expandV4Payload(payload: Uint8Array): Uint8Array {
  const expanded = new Uint8Array(RGB_BYTES);
  for (let i = 0; i < 160; i += 1) {
    const src = i * 3;
    const dstA = i * 6;
    const dstB = dstA + 3;
    expanded[dstA] = payload[src];
    expanded[dstA + 1] = payload[src + 1];
    expanded[dstA + 2] = payload[src + 2];
    expanded[dstB] = payload[src];
    expanded[dstB + 1] = payload[src + 1];
    expanded[dstB + 2] = payload[src + 2];
  }
  return expanded;
}

export function parseLedCapture(bytes: Uint8Array, source: FrameSource = 'imported-capture'): ParsedCapture {
  const candidates = [SERIAL_V2_FRAME_SIZE, WEB_FRAME_SIZE, WEB_LEGACY_FRAME_SIZE, RGB_BYTES, SERIAL_V4_FRAME_SIZE];
  const frameSize = candidates.find((size) => bytes.length >= size && bytes.length % size === 0);
  if (!frameSize) {
    throw new Error(`Unsupported capture length ${bytes.length}; expected 960, 961, 966, 529, or 1009 byte frames`);
  }

  const frames: LedFrame[] = [];
  for (let offset = 0; offset < bytes.length; offset += frameSize) {
    const frame = bytes.slice(offset, offset + frameSize);
    const payloadOffset = payloadOffsetForFrame(frame);
    if (payloadOffset === null) {
      throw new Error(`Unsupported frame header at offset ${offset}`);
    }
    const payloadLength = payloadLengthForFrame(frame);
    const payload = frame.slice(payloadOffset, payloadOffset + payloadLength);
    const leds = frame.length === SERIAL_V4_FRAME_SIZE ? expandV4Payload(payload) : payload;
    if (leds.length !== RGB_BYTES) {
      throw new Error(`Frame at offset ${offset} produced ${leds.length} RGB bytes, expected ${RGB_BYTES}`);
    }
    frames.push({
      source,
      timestampMs: frames.length * (1000 / 30),
      leds,
      metadata: { frameSize, byteOffset: offset },
    });
  }

  return { frames, frameSize };
}

export function makeEmptyFrame(source: FrameSource = 'schematic-workbench'): LedFrame {
  return {
    source,
    timestampMs: 0,
    leds: new Uint8Array(RGB_BYTES),
    metadata: {},
  };
}

export function makeCentrePulseFrame(rgb: [number, number, number] = [255, 180, 64]): LedFrame {
  const frame = makeEmptyFrame();
  for (const index of [79, 80, 239, 240]) {
    const base = index * 3;
    frame.leds[base] = rgb[0];
    frame.leds[base + 1] = rgb[1];
    frame.leds[base + 2] = rgb[2];
  }
  return frame;
}
