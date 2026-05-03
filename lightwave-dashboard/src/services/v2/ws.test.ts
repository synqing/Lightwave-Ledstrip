import { afterEach, describe, expect, it, vi } from 'vitest';
import {
  LED_FRAME_SIZE_V0,
  LED_FRAME_SIZE_V1,
  LED_STREAM_MAGIC,
  V2WsClient,
  isValidLedStreamFrame,
} from './ws';

class MockWebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static instances: MockWebSocket[] = [];

  readyState = MockWebSocket.CONNECTING;
  binaryType: BinaryType = 'blob';
  sent: string[] = [];
  onopen: (() => void) | null = null;
  onmessage: ((ev: MessageEvent) => void) | null = null;
  onerror: (() => void) | null = null;
  onclose: (() => void) | null = null;
  readonly url: string;

  constructor(url: string) {
    this.url = url;
    MockWebSocket.instances.push(this);
  }

  send(data: string) {
    this.sent.push(data);
  }

  close() {
    this.readyState = MockWebSocket.CONNECTING;
    this.onclose?.();
  }
}

describe('V2WsClient', () => {
  afterEach(() => {
    vi.unstubAllGlobals();
    MockWebSocket.instances = [];
  });

  it('sends API key auth immediately after socket open when configured', () => {
    vi.stubGlobal('WebSocket', MockWebSocket);

    const client = new V2WsClient('ws://192.168.4.1/ws', { apiKey: 'k1-secret' });
    client.connect();

    const ws = MockWebSocket.instances[0];
    ws.readyState = MockWebSocket.OPEN;
    ws.onopen?.();

    expect(ws.sent).toEqual([JSON.stringify({ type: 'auth', apiKey: 'k1-secret' })]);
  });

  it('validates both LED stream frame sizes and rejects bad magic', () => {
    const v0 = new Uint8Array(LED_FRAME_SIZE_V0);
    v0[0] = LED_STREAM_MAGIC;
    const v1 = new Uint8Array(LED_FRAME_SIZE_V1);
    v1[0] = LED_STREAM_MAGIC;
    const bad = new Uint8Array(LED_FRAME_SIZE_V0);
    bad[0] = 0xff;

    expect(isValidLedStreamFrame(v0)).toBe(true);
    expect(isValidLedStreamFrame(v1)).toBe(true);
    expect(isValidLedStreamFrame(bad)).toBe(false);
  });
});
