import { beforeEach, describe, expect, it, vi } from 'vitest';
import { V2Client } from './client';

describe('V2Client', () => {
  beforeEach(() => {
    vi.restoreAllMocks();
  });

  it('sends API key auth without bearer headers', async () => {
    const fetchMock = vi.fn(async () => new Response(JSON.stringify({
      success: true,
      data: { ok: true },
      timestamp: 1,
      version: '1.0.0',
    })));
    vi.stubGlobal('fetch', fetchMock);

    const client = new V2Client({
      baseUrl: 'http://192.168.4.1/api/v1',
      auth: { mode: 'apiKey', token: 'k1-secret' },
    });

    await client.get('/device/status');

    const [, init] = fetchMock.mock.calls[0] as unknown as [string, RequestInit];
    expect(init.headers).toMatchObject({ 'X-API-Key': 'k1-secret' });
    expect(init.headers).not.toHaveProperty('Authorization');
  });

  it('accepts empty success envelopes for acknowledgement-only mutations', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => new Response(JSON.stringify({
      success: true,
      timestamp: 1,
      version: '1.0.0',
    }))));

    const client = new V2Client({ baseUrl: 'http://192.168.4.1/api/v1' });
    await expect(client.patch<void>('/parameters', { brightness: 120 })).resolves.toBeUndefined();
  });
});
