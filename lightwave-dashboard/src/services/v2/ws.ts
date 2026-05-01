import type { V2WsEvent, V2WsRequest } from './types';

export type WsStatus = 'disconnected' | 'connecting' | 'connected' | 'error';

// LED stream frame format constants
export const LED_STREAM_MAGIC = 0xfe;
export const LED_STREAM_VERSION_V1 = 1;
export const LEDS_PER_STRIP = 160;
export const NUM_STRIPS = 2;
export const TOTAL_LED_COUNT = LEDS_PER_STRIP * NUM_STRIPS; // 320

// Frame format v1 (dual-strip): [MAGIC][VERSION][NUM_STRIPS][LEDS_PER_STRIP][STRIP0_ID][RGB×160][STRIP1_ID][RGB×160]
export const FRAME_HEADER_SIZE = 4; // Magic + Version + NumStrips + LEDsPerStrip
export const FRAME_SIZE_PER_STRIP = 1 + (LEDS_PER_STRIP * 3); // StripID + RGB (481 bytes)
export const FRAME_PAYLOAD_SIZE = NUM_STRIPS * FRAME_SIZE_PER_STRIP; // 962 bytes
export const LED_FRAME_SIZE_V1 = FRAME_HEADER_SIZE + FRAME_PAYLOAD_SIZE; // 966 bytes

// Legacy frame format v0: [MAGIC][RGB×320] = 961 bytes
export const LED_FRAME_SIZE_V0 = 1 + (TOTAL_LED_COUNT * 3); // 961 bytes

export class V2WsClient {
  private ws: WebSocket | null = null;
  private status: WsStatus = 'disconnected';
  private url: string;
  private reconnectAttempt = 0;
  private reconnectTimer: number | null = null;
  private listeners = new Set<(event: V2WsEvent) => void>();
  private statusListeners = new Set<(status: WsStatus) => void>();
  private binaryListeners = new Set<(data: Uint8Array) => void>();

  constructor(url: string) {
    this.url = url;
  }

  setUrl(url: string) {
    this.url = url;
  }

  connect() {
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) return;
    this.setStatus('connecting');
    try {
      this.ws = new WebSocket(this.url);
    } catch {
      this.setStatus('error');
      this.scheduleReconnect();
      return;
    }

    this.ws.binaryType = 'arraybuffer';
    this.ws.onopen = () => {
      this.reconnectAttempt = 0;
      this.setStatus('connected');
    };
    this.ws.onmessage = (ev) => {
      // Handle binary LED frame data
      if (ev.data instanceof ArrayBuffer) {
        const bytes = new Uint8Array(ev.data);
        // Accept both v0 (legacy) and v1 (dual-strip) formats
        if (bytes[0] === LED_STREAM_MAGIC && 
            (bytes.length === LED_FRAME_SIZE_V0 || bytes.length === LED_FRAME_SIZE_V1)) {
          this.binaryListeners.forEach(l => l(bytes));
        }
        return;
      }
      // Handle JSON messages
      const parsed = this.safeParse(ev.data);
      if (parsed) this.listeners.forEach(l => l(parsed));
    };
    this.ws.onerror = () => {
      this.setStatus('error');
    };
    this.ws.onclose = () => {
      this.ws = null;
      this.setStatus('disconnected');
      this.scheduleReconnect();
    };
  }

  disconnect() {
    if (this.reconnectTimer) {
      window.clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    this.reconnectAttempt = 0;
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
    this.setStatus('disconnected');
  }

  send(msg: V2WsRequest) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return false;
    this.ws.send(JSON.stringify(msg));
    return true;
  }

  onEvent(cb: (event: V2WsEvent) => void) {
    this.listeners.add(cb);
    return () => this.listeners.delete(cb);
  }

  onStatus(cb: (status: WsStatus) => void) {
    this.statusListeners.add(cb);
    return () => this.statusListeners.delete(cb);
  }

  onBinary(cb: (data: Uint8Array) => void) {
    this.binaryListeners.add(cb);
    return () => this.binaryListeners.delete(cb);
  }

  getStatus(): WsStatus {
    return this.status;
  }

  private scheduleReconnect() {
    if (this.reconnectTimer) return;
    this.reconnectAttempt += 1;
    const delay = Math.min(30000, 500 * Math.pow(2, Math.min(6, this.reconnectAttempt - 1)));
    const jitter = Math.random() * 200;
    this.reconnectTimer = window.setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, delay + jitter);
  }

  private setStatus(status: WsStatus) {
    if (this.status === status) return;
    this.status = status;
    this.statusListeners.forEach(l => l(status));
  }

  private safeParse(data: unknown): V2WsEvent | null {
    if (typeof data !== 'string') return null;
    try {
      return JSON.parse(data) as V2WsEvent;
    } catch {
      return null;
    }
  }
}

