import { describe, expect, it, beforeEach } from 'vitest';
import { buildV2BaseUrl, buildV2WsUrl, DEFAULT_V2_SETTINGS, loadV2Settings } from './config';

describe('v2 config', () => {
  beforeEach(() => {
    localStorage.clear();
  });

  it('defaults to the K1 AP origin', () => {
    expect(DEFAULT_V2_SETTINGS.deviceOrigin).toBe('http://192.168.4.1');
    expect(buildV2BaseUrl(DEFAULT_V2_SETTINGS.deviceOrigin)).toBe('http://192.168.4.1/api/v1');
    expect(buildV2WsUrl(DEFAULT_V2_SETTINGS.deviceOrigin)).toBe('ws://192.168.4.1/ws');
  });

  it('keeps same-origin mode available when origin is empty', () => {
    expect(buildV2BaseUrl('')).toBe('/api/v1');
    expect(buildV2WsUrl('')).toBe('ws://localhost:3000/ws');
  });

  it('migrates bearer or unknown auth modes to none', () => {
    localStorage.setItem('lw.v2.authMode', 'bearer');
    expect(loadV2Settings().authMode).toBe('none');

    localStorage.setItem('lw.v2.authMode', 'unknown');
    expect(loadV2Settings().authMode).toBe('none');
  });
});
